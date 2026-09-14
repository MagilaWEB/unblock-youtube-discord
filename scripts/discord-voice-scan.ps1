# Discord voice region scanner.
# Watches the Windows DNS resolver cache while the user joins Discord voice
# channels, collects live voice server names (c-<region>-<hash>.discord.media),
# groups them by region and writes a live region list.
#
# Usage:
#   pwsh -File scripts/discord-voice-scan.ps1                     # watch 600 s
#   pwsh -File scripts/discord-voice-scan.ps1 -DurationSec 120
#   pwsh -File scripts/discord-voice-scan.ps1 -AppendHosts        # also pin IPs
#
# Background: the old namespace (regionNNNNN.discord.gg / .media) is dead -
# no A records at the authoritative NS. The live voice names handed out by
# the gateway are c-<region>-<hash>.discord.media where the hash is random
# per region+server pair (not enumerable). The only source of truth is a
# real client session: the client resolves those names through the OS
# resolver, so the DNS cache contains every name the client actually used.

param(
	[int]$DurationSec = 600,
	[int]$PollMs = 400,
	[string]$OutFile = "docs\discord-voice-regions.txt",
	[switch]$AppendHosts,
	[string]$HostsFile = "configs\dns_hosts\discord.list"
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$OutPath	= if ([IO.Path]::IsPathRooted($OutFile)) { $OutFile } else { Join-Path $repoRoot $OutFile }
$HostsPath = if ([IO.Path]::IsPathRooted($HostsFile)) { $HostsFile } else { Join-Path $repoRoot $HostsFile }

# c-<region>-<hash>.discord.media and any future c-* sibling zone
$voiceNameRegex = '^c-([a-z]+[0-9]+)-([0-9a-f]+)\.discord\.(media|gg)$'

$seen = @{}   # name -> List of ips
$deadline = (Get-Date).AddSeconds($DurationSec)

Write-Host "[scan] watching DNS cache for $DurationSec s (poll ${PollMs} ms)"
Write-Host "[scan] JOIN A DISCORD VOICE CHANNEL NOW, hop a few channels/servers"

while ((Get-Date) -lt $deadline)
{
	$entries = Get-DnsClientCache -ErrorAction SilentlyContinue |
		Where-Object { $_.Entry -match $voiceNameRegex }

	foreach ($e in $entries)
	{
		$name = [string]$e.Entry
		if (-not $seen.ContainsKey($name))
		{
			$seen[$name] = [System.Collections.Generic.List[string]]::new()
			Write-Host "[scan] + $name"
		}

		$data = [string]$e.Data
		if ($data -and -not $seen[$name].Contains($data))
		{
			$seen[$name].Add($data)
			Write-Host "[scan]   ip $data"
		}
	}

	Start-Sleep -Milliseconds $PollMs
}

if ($seen.Count -eq 0)
{
	Write-Warning "[scan] nothing captured - was the client in a voice channel? (DNS cache TTL may have flushed old entries)"
	exit 1
}

# region summary
$regions = @{}
foreach ($name in $seen.Keys)
{
	if ($name -match $voiceNameRegex)
	{
		$region = $Matches[1]
		if (-not $regions.ContainsKey($region)) { $regions[$region] = [System.Collections.Generic.List[string]]::new() }
		$regions[$region].Add($name)
	}
}

# report
$stamp = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Discord voice live regions (auto-collected)")
$lines.Add("# updated: $stamp")
$lines.Add("# source: Windows DNS cache capture of real client voice sessions")
$lines.Add("# old namespace regionNNNNN.discord.gg/.media is dead (NODATA at authoritative NS)")
$lines.Add("# live names are c-<region>-<hash>.discord.media, hash is random per region+server")
$lines.Add("")
$lines.Add("[regions]")
foreach ($region in ($regions.Keys | Sort-Object))
{
	$lines.Add("$region=$($regions[$region].Count)")
}
$lines.Add("")
foreach ($name in ($seen.Keys | Sort-Object))
{
	$lines.Add("$name => $($seen[$name] -join ', ')")
}

$lines | Set-Content -Path $OutPath -Encoding utf8NoBOM
Write-Host "[scan] report written: $OutPath ($($seen.Count) names, $($regions.Count) regions)"

# optional: pin harvested names to their ips in the unblock hosts list
if ($AppendHosts)
{
	$existing = Get-Content $HostsPath -ErrorAction SilentlyContinue
	$added = 0
	foreach ($name in ($seen.Keys | Sort-Object))
	{
		foreach ($ip in $seen[$name])
		{
			# hosts file needs ipv4 only
			if ($ip -notmatch '^\d{1,3}(\.\d{1,3}){3}$') { continue }
			if ($existing -contains "$ip->$name") { continue }

			$existing = $existing + "$ip->$name"
			$added++
		}
	}

	$existing | Set-Content -Path $HostsPath -Encoding utf8NoBOM
	Write-Host "[scan] hosts list updated: $HostsPath (+$added forced mappings)"
}

param(
    # Preset built before packaging: release-min builds no tests.
    [string]$Preset = 'release-min',
    # Where unblock.7z and unblock.exe land (relative to the repository root).
    [string]$OutDir = 'release',
    # Package the current bin/ as is, without rebuilding.
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
Set-Location $root

if (-not $SkipBuild) {
    & (Join-Path $root 'build-ai.ps1') -Preset $Preset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$sevenZip = Join-Path $root '_build_ai\7za\x64\7za.exe'
if (-not (Test-Path $sevenZip)) {
    Write-Host "7za.exe not found: $sevenZip (configure the project first)"
    exit 1
}

# bin/ is shared with the IDE's debug preset, so a stray -SkipBuild after a debug
# build would package debug binaries into a release (and their symbols are just
# dropped by the filter below, hiding the mistake). A release build ships no PDB.
$debugSymbols = Get-ChildItem -Path (Join-Path $root 'bin') -Filter '*.pdb' -File -ErrorAction SilentlyContinue
if ($debugSymbols) {
    Write-Host 'bin/ contains debug symbols, so it holds a debug build:'
    $debugSymbols | ForEach-Object { Write-Host "  $($_.Name)" }
    Write-Host 'Refusing to package a debug build; run release-ai.ps1 without -SkipBuild.'
    exit 3
}

$stage   = Join-Path $env:TEMP 'unblock-release'
$payload = Join-Path $stage 'unblock'
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $payload | Out-Null

# What a release ships: the app (bin/), the engines (binaries/), the shipped
# configs and the UI, plus the two root documents. The updater downloads
# unblock.7z and expects unblock/bin/engine.exe inside, so this layout is fixed.
foreach ($dir in 'bin', 'binaries', 'configs', 'ui') {
    $from = Join-Path $root $dir
    if (-not (Test-Path $from)) { Write-Host "Missing source directory: $from"; exit 1 }
    Copy-Item -Recurse -Force $from (Join-Path $payload $dir)
}
foreach ($file in 'LICENSE.txt', 'README.md') {
    Copy-Item -Force (Join-Path $root $file) (Join-Path $payload $file)
}

# Nothing from a dev build may leak into a release: test binaries, debug symbols
# and wrapper logs. User data (user/, logs/) is never part of the payload.
$dropped = 0
foreach ($file in Get-ChildItem -LiteralPath $payload -Recurse -File) {
    if ($file.Name -like 'test_*' -or $file.Extension -eq '.pdb' -or $file.Extension -eq '.log') {
        Write-Host "  dropping $($file.FullName.Substring($payload.Length + 1))"
        Remove-Item -LiteralPath $file.FullName -Force
        $dropped++
    }
}
Write-Host "Payload ready: $dropped file(s) dropped as not-for-release"

New-Item -ItemType Directory -Force (Join-Path $root $OutDir) | Out-Null
$archive = Join-Path $root (Join-Path $OutDir 'unblock.7z')
Remove-Item -Force $archive -ErrorAction SilentlyContinue

# 7za stores the paths exactly as given, so run from the staging directory:
# the archive then holds unblock\... and nothing else.
Push-Location $stage
& $sevenZip a -t7z -mx=9 -mf=off $archive 'unblock' | Out-Null
$code = $LASTEXITCODE
Pop-Location
if ($code -ne 0) { exit $code }

$version = (Get-Item (Join-Path $root 'bin\engine.exe')).VersionInfo.ProductVersion

# Online installer: a small Inno Setup wizard that downloads unblock.7z from the
# latest GitHub release at install time and unpacks it into the chosen folder
# (default C:\unblock). The payload itself stays a separate release asset.
$inno = $null
foreach ($candidate in @(
        (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
        (Join-Path ${env:ProgramFiles} 'Inno Setup 6\ISCC.exe')
    )) {
    if ($candidate -and (Test-Path $candidate)) { $inno = $candidate; break }
}
if (-not $inno) {
    $found = where.exe ISCC.exe 2>$null
    if ($found) { $inno = ($found | Select-Object -First 1) }
}

if ($inno) {
    $iss = Join-Path $root 'installer\unblock.iss'
    & $inno "/DAppVersion=$version" "/DOutDir=$root\$OutDir" "/DExtractorPath=$sevenZip" $iss
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    Write-Host 'unblock-setup.exe skipped: Inno Setup 6 not found (winget install JRSoftware.InnoSetup)'
}

Write-Host "Release $version artifacts:"
Get-Item $archive | ForEach-Object { '  {0}  {1:N1} MB' -f $_.Name, ($_.Length / 1MB) }
$setup = Join-Path $root (Join-Path $OutDir 'unblock-setup.exe')
if (Test-Path $setup) {
    Get-Item $setup | ForEach-Object { '  {0}  {1:N1} MB' -f $_.Name, ($_.Length / 1MB) }
}

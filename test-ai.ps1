param(
    [string]$Name,
    [switch]$Release
)

$ErrorActionPreference = 'Stop'

if (-not $Release) {
    $curlBin = Join-Path (Get-Location) '_build_ai/curl/debug/bin'
    if (Test-Path $curlBin) {
        $env:PATH = "$curlBin;$env:PATH"
        Write-Host "PATH += $curlBin"
    }
}

$ctestArgs = @('--test-dir', '_build_ai', '--output-on-failure')
if ($Name) {
    $ctestArgs += '-R'
    $ctestArgs += $Name
}

ctest @ctestArgs
exit $LASTEXITCODE
param(
    [ValidateSet('debug', 'debug-min', 'release', 'release-min')]
    [string]$Preset = 'debug',
    [switch]$Clean,
    [string]$Target
)

$ErrorActionPreference = 'Stop'

if ($Clean -and (Test-Path '_build_ai')) {
    Write-Host 'Removing old build directory...'
    Remove-Item -Recurse -Force '_build_ai'
}

$env:INCLUDE = $null
$env:LIB = $null
$env:LIBPATH = $null

Write-Host "Configuring (preset: $Preset)..."
cmake -S . -B _build_ai --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host 'Building...'
if ($Target) {
    cmake --build _build_ai --target $Target
} else {
    cmake --build _build_ai
}
exit $LASTEXITCODE
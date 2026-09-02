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

# Re-configure only when the build directory is missing or was configured
# for another preset. Otherwise cmake --build re-generates on demand.
$cachePath = '_build_ai/CMakeCache.txt'
$needConfigure = $true
if (Test-Path $cachePath) {
    $cachedType  = Select-String -Path $cachePath -Pattern '^CMAKE_BUILD_TYPE:STRING=(.*)$' | ForEach-Object { $_.Matches[0].Groups[1].Value }
    $cachedTests = Select-String -Path $cachePath -Pattern '^BUILD_TESTS:BOOL=(.*)$' | ForEach-Object { $_.Matches[0].Groups[1].Value }

    $expectedType  = if ($Preset -like 'release*') { 'Release' } else { 'Debug' }
    $expectedTests = if ($Preset -like '*-min') { 'OFF' } else { 'ON' }

    if ($cachedType -eq $expectedType -and $cachedTests -eq $expectedTests) {
        $needConfigure = $false
    }
}

if ($needConfigure) {
    Write-Host "Configuring (preset: $Preset)..."
    cmake -S . -B _build_ai --preset $Preset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host 'Building...'
if ($Target) {
    cmake --build _build_ai --target $Target
} else {
    cmake --build _build_ai
}
exit $LASTEXITCODE
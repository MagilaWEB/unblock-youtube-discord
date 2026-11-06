param(
    [ValidateSet('debug', 'debug-min', 'release', 'release-min')]
    [string]$Preset = 'debug',
    [switch]$Clean,
    [string]$Target,
    # Ad-hoc version stamp without touching git history, e.g. -VersionOverride 1.5.0
    # to test the updater against a newer release. NEVER tag a release from such
    # a build; see cmake/GetUnblockVersion.cmake.
    [string]$VersionOverride
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

# An explicit version override always needs a fresh configure, otherwise the
# cached (or auto-computed) version would silently survive. Conversely, a
# stale override left in the cache must be dropped when no flag is given.
$clearOverride = $false
if ($VersionOverride) {
    $needConfigure = $true
} elseif (Test-Path $cachePath) {
    # Note: -D without an explicit :TYPE lands in the cache as UNINITIALIZED.
    $cachedOverride = Select-String -Path $cachePath -Pattern '^UNBLOCK_VERSION_OVERRIDE:(STRING|UNINITIALIZED)=(.+)$' | Select-Object -First 1
    if ($cachedOverride) {
        $needConfigure = $true
        $clearOverride = $true
    }
}

if ($needConfigure) {
    Write-Host "Configuring (preset: $Preset)..."
    $cmakeArgs = @('-S', '.', '-B', '_build_ai', '--preset', $Preset)
    if ($VersionOverride) {
        $cmakeArgs += "-DUNBLOCK_VERSION_OVERRIDE=$VersionOverride"
    } elseif ($clearOverride) {
        $cmakeArgs += '-UUNBLOCK_VERSION_OVERRIDE'
    }
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host 'Building...'
if ($Target) {
    cmake --build _build_ai --target $Target
    exit $LASTEXITCODE
}

cmake --build _build_ai
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Configuration wipes bin/ (see src/engine/CMakeLists.txt), but ninja still
# considers the deleted binaries up to date and never relinks them. Restore
# every bin output that is missing after the build, so bin/ never ends up
# half empty (a missing zapret_helper.exe is a run that silently misbehaves).
$ninjaFile = '_build_ai/build.ninja'
if (Test-Path $ninjaFile) {
    $root   = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
    $binDir = (Join-Path $root 'bin').Replace('/', '\').ToLowerInvariant()

    $binOutputs = Select-String -Path $ninjaFile -Pattern '^build (\S+?\.(?:exe|dll)):' |
        ForEach-Object { $_.Matches[0].Groups[1].Value.Replace('$:', ':').Replace('$ ', ' ') } |
        Where-Object {
            $full = $_.Replace('/', '\')
            $full.Contains('\') -and $full.Substring(0, $full.LastIndexOf('\')).ToLowerInvariant() -eq $binDir
        } |
        Select-Object -Unique

    $missing = $binOutputs | Where-Object { -not (Test-Path $_) }
    if ($missing) {
        Write-Host 'Restoring binaries removed from bin/ by configure (ninja thought them fresh):'
        foreach ($output in $missing) {
            Write-Host "  $output"
            & ninja -C _build_ai $output
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }
    }
}

exit 0
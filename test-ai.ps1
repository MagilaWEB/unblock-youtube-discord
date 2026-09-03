param(
    [string]$Name,
    [switch]$Release
)

$ErrorActionPreference = 'Stop'

$ctestArgs = @('--test-dir', '_build_ai', '--output-on-failure')
if ($Name) {
    $ctestArgs += '-R'
    $ctestArgs += $Name
}

ctest @ctestArgs
exit $LASTEXITCODE
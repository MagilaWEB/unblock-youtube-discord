param(
    [string]$Name,
    [switch]$Release
)

$ErrorActionPreference = 'Stop'

# Run from the repository root so the relative _build_ai path resolves.
Set-Location (Split-Path -Parent $PSScriptRoot)

$ctestArgs = @('--test-dir', '_build_ai', '--output-on-failure')
if ($Name) {
    $ctestArgs += '-R'
    $ctestArgs += $Name
}

ctest @ctestArgs
exit $LASTEXITCODE
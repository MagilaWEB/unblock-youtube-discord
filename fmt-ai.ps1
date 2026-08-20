param(
    [switch]$All,
    [switch]$Check
)

$ErrorActionPreference = 'Stop'

$exts = @('.cpp', '.h', '.hpp', '.c', '.cc', '.cxx', '.cu', '.cuh')

if ($All) {
    $files = Get-ChildItem -Recurse -File |
        Where-Object { $_.Extension -in $exts -and $_.FullName -notmatch '_build|_deps|\\bin\\' } |
        ForEach-Object { $_.FullName }
} else {
    $files = git status --porcelain | ForEach-Object {
        if ($_ -match '^.{1,3}\s+(.+)$') {
            $p = $Matches[1].Trim('"')
            if ([System.IO.Path]::GetExtension($p) -in $exts) { $p }
        }
    }
}

if (-not $files) {
    Write-Host 'No files to format.'
    exit 0
}

foreach ($f in $files) {
    if ($Check) {
        clang-format --dry-run -Werror $f
    } else {
        clang-format -i $f
    }
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "Done: $($files.Count) file(s) processed."
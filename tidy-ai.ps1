param(
    [switch]$All,
    [switch]$Fix,
    [string]$Filter,
    [int]$Jobs = 12,
    [string]$Checks
)

$ErrorActionPreference = 'Stop'

# Clear MSVC env vars so clang-tidy uses the pure clang toolchain (no rc.exe hangs).
$env:INCLUDE = $null
$env:LIB = $null
$env:LIBPATH = $null
$env:Path = "C:\Program Files\LLVM\bin;" + $env:Path

$exts = @('.cpp', '.c')

if ($All) {
    $files = Get-ChildItem -Recurse -File -Path src |
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

if ($Filter) {
    $files = $files | Where-Object { $_ -match $Filter }
}

if (-not $files) {
    Write-Host 'No files to check.'
    exit 0
}

$tidyArgs = @('-p', '_build_ai', '--header-filter=^src/.*', '--quiet')
if ($Checks) {
    $tidyArgs += "--checks=$Checks"
}
if ($Fix) {
    $tidyArgs += '--fix'
    $tidyArgs += '--fix-errors'
}

Write-Host "Checking $($files.Count) file(s) in $Jobs threads..."

$results = $files | ForEach-Object -Parallel {
    $f = $_
    $out = clang-tidy $using:tidyArgs $f 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { $failed = $true } else { $failed = $false }
    [PSCustomObject]@{ File = $f; Output = $out; Failed = $failed }
} -ThrottleLimit $Jobs

$failedCount = 0
foreach ($r in $results) {
    if ($r.Output.Trim()) {
        Write-Host "=== $($r.File) ==="
        Write-Host $r.Output
    }
    if ($r.Failed) { $failedCount++ }
}

Write-Host "Done: $($files.Count) file(s) checked, $failedCount with errors."
if ($failedCount -gt 0) { exit 1 }

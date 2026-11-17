param(
    [string]$WikiUrl,
    [string]$Message,
    [switch]$NoPush
)

$ErrorActionPreference = 'Stop'

$repoRoot = $PSScriptRoot
$sourceDir = Join-Path $repoRoot 'docs/wiki'

if (-not (Test-Path -LiteralPath $sourceDir)) {
    Write-Host "Wiki pages not found: $sourceDir"
    exit 1
}

# The wiki checkout is the project URL with a ".wiki" suffix.
if (-not $WikiUrl) {
    $origin = (git -C $repoRoot remote get-url origin).Trim()
    if ($origin -notmatch 'github\.com[:/](?<slug>.+?)(\.git)?$') {
        Write-Host "Cannot derive the wiki URL from origin: $origin"
        exit 1
    }
    $WikiUrl = "https://github.com/$($Matches['slug'] -replace '\.git$', '').wiki.git"
}

# Keep the checkout out of the repository: it is a separate git tree.
$workDir = Join-Path $env:TEMP 'unblock-wiki'

if (Test-Path -LiteralPath (Join-Path $workDir '.git')) {
    git -C $workDir remote set-url origin $WikiUrl
    git -C $workDir fetch --prune origin
    git -C $workDir reset --hard origin/master
} else {
    Remove-Item -LiteralPath $workDir -Recurse -Force -ErrorAction SilentlyContinue
    git clone $WikiUrl $workDir
}

if ($LASTEXITCODE -ne 0) {
    Write-Host ''
    Write-Host 'Wiki clone failed. GitHub does not create the wiki repository until the first page'
    Write-Host "is saved in the browser: open the Wiki tab, create a page named 'Home' and save it."
    exit 1
}

# Mirror docs/wiki: copy every page and drop the ones the source no longer has.
$pages = Get-ChildItem -LiteralPath $sourceDir -File -Filter '*.md'
$pageNames = $pages | ForEach-Object { $_.Name }

Get-ChildItem -LiteralPath $workDir -File -Filter '*.md' |
    Where-Object { $_.Name -notin $pageNames } |
    ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }

foreach ($page in $pages) {
    Copy-Item -LiteralPath $page.FullName -Destination (Join-Path $workDir $page.Name) -Force
}

git -C $workDir add --all
if (-not (git -C $workDir status --porcelain)) {
    Write-Host 'Wiki is already up to date.'
    exit 0
}

if (-not $Message) {
    $Message = 'docs: обновить страницы вики'
}
git -C $workDir commit -m $Message
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($NoPush) {
    Write-Host "Committed in $workDir, not pushed."
    exit 0
}

git -C $workDir push origin master
exit $LASTEXITCODE

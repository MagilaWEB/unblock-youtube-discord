param(
    # Release tag of Flowseal/tg-ws-proxy to build. Defaults to the latest release.
    [string]$Tag,
    # Install the Python packages the proxy needs into the py launcher environment.
    [switch]$InstallDeps
)

$ErrorActionPreference = 'Stop'

# unblock runs tg-ws-proxy as a service payload through SvcHost, so it needs a plain
# console onefile build - the upstream recipe builds a tray/GUI application instead.
# This is a maintainer tool on purpose: build-ai.ps1 and CMake never call it, run it
# by hand when the proxy needs updating.
$repo     = 'Flowseal/tg-ws-proxy'
$root     = Split-Path -Parent $PSScriptRoot
$target   = Join-Path $root 'binaries/tg-ws-proxy.exe'
$workDir  = Join-Path $env:TEMP 'unblock-tgws'

# tg_proxy.cpp passes exactly these, a renamed flag would break the service silently.
$flags    = '--secret', '--dc-ip', '--host', '--port'
# certifi must land inside the bundle (TLS cacert.pem) and h2 is needed by proxy.cf_h2.
$imports  = 'cryptography', 'httpx', 'certifi', 'h2'
$hidden   = 'proxy', 'proxy.tg_ws_proxy', 'cryptography',
            'cryptography.hazmat.primitives.ciphers.algorithms',
            'cryptography.hazmat.primitives.ciphers.modes'
$excludes = 'tkinter', 'PIL', 'numpy', 'matplotlib', 'scipy', 'pandas', 'IPython', 'setuptools',
            'pip', 'distutils', 'unittest', 'test', 'html', 'http.server', 'xmlrpc', 'pdb',
            'lib2to3', 'turtledemo', 'curses', 'venv', 'ensurepip', 'idlelib'

if (-not $Tag) {
    Write-Host "Resolving the latest $repo release..."
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases/latest" `
        -Headers @{ 'User-Agent' = 'unblock-tgws-build' }
    $Tag = $release.tag_name
}
if (-not $Tag) {
    Write-Host "Cannot resolve a release tag for $repo."
    exit 1
}
Write-Host "Building $repo $Tag"

py -m PyInstaller --version *> $null
if ($LASTEXITCODE -ne 0) {
    Write-Host 'PyInstaller is not available: py -m pip install pyinstaller'
    exit 1
}

$missing = foreach ($module in $imports) {
    py -c "import $module" *> $null
    if ($LASTEXITCODE -ne 0) { $module }
}
if ($missing) {
    if (-not $InstallDeps) {
        Write-Host "Missing Python packages: $($missing -join ', ')"
        Write-Host "Install them first:  py -m pip install $($missing -join ' ')   (or re-run with -InstallDeps)"
        exit 1
    }
    Write-Host "Installing: $($missing -join ', ')"
    py -m pip install @missing
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

New-Item -ItemType Directory -Force -Path $workDir | Out-Null
$srcRoot = Join-Path $workDir "src-$Tag"
Remove-Item -LiteralPath $srcRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $srcRoot | Out-Null

Write-Host 'Downloading sources...'
$zip = Join-Path $workDir "$Tag.zip"
Invoke-WebRequest -Uri "https://github.com/$repo/archive/refs/tags/$Tag.zip" -OutFile $zip
Expand-Archive -LiteralPath $zip -DestinationPath $srcRoot -Force

$src   = Get-ChildItem -LiteralPath $srcRoot -Directory | Select-Object -First 1
$entry = Join-Path $src.FullName 'proxy/tg_ws_proxy.py'
if (-not (Test-Path -LiteralPath $entry)) {
    Write-Host "The $Tag archive has no proxy/tg_ws_proxy.py."
    exit 1
}

foreach ($flag in $flags) {
    if (-not (Select-String -LiteralPath $entry -Pattern ([regex]::Escape($flag)) -Quiet)) {
        Write-Host "$repo $Tag does not accept $flag anymore, while src/unblock/tg_proxy.cpp still passes it."
        Write-Host 'Build stopped - the proxy would start and ignore its settings.'
        exit 1
    }
}

$pyInstallerArgs = @('-m', 'PyInstaller', '--onefile', '--console', '--name', 'tg-ws-proxy', '--strip', '--optimize=2')

$upx = (Get-Command upx -ErrorAction SilentlyContinue).Source
if (-not $upx) {
    $upx = Get-ChildItem "$env:USERPROFILE\AppData\Local\Programs\Python\Python3*\Scripts\upx.exe" `
        -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
}
if ($upx) {
    $pyInstallerArgs += "--upx-dir=$(Split-Path -Parent $upx)"
} else {
    Write-Host 'UPX not found, building without it (only the size changes).'
}

foreach ($module in $hidden) { $pyInstallerArgs += "--hidden-import=$module" }
foreach ($module in $excludes) { $pyInstallerArgs += "--exclude-module=$module" }
$pyInstallerArgs += 'tg_ws_proxy.py'

Push-Location (Join-Path $src.FullName 'proxy')
try {
    & py @pyInstallerArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}

$built = Join-Path $src.FullName 'proxy/dist/tg-ws-proxy.exe'
if (-not (Test-Path -LiteralPath $built)) {
    Write-Host 'PyInstaller produced no tg-ws-proxy.exe.'
    exit 1
}

# A GUI build would start too, just not as a service payload: check the subsystem.
$image     = [IO.File]::ReadAllBytes($built)
$subsystem = [BitConverter]::ToUInt16($image, [BitConverter]::ToInt32($image, 0x3C) + 0x5C)
if ($subsystem -ne 3) {
    Write-Host "The built exe is not a console binary (PE subsystem $subsystem)."
    exit 1
}

Copy-Item -LiteralPath $built -Destination $target -Force

$installed = Get-Item -LiteralPath $target
Write-Host "Installed $($installed.FullName)"
Write-Host "  tag    : $Tag"
Write-Host "  size   : $($installed.Length)"
Write-Host "  sha256 : $((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash)"
Write-Host 'Commit binaries/tg-ws-proxy.exe together with the update.'

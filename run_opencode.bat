@echo off
rem Portable launcher: ensure node and github-mcp-server are on PATH for MCP servers.
rem PATH зависит от того, кто и откуда запускает (терминал, IDE, планировщик) — чиним
rem известные дыры здесь, чтобы MCP не дохли с -32000 в «не той» среде.
where node >nul 2>nul
if errorlevel 1 if exist "%ProgramFiles%\nodejs\node.exe" set "PATH=%ProgramFiles%\nodejs;%PATH%"
if exist "%USERPROFILE%\.local\bin\github-mcp-server.exe" set "PATH=%USERPROFILE%\.local\bin;%PATH%"
opencode %*

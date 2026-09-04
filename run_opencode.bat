@echo off
rem Portable launcher: ensure node is on PATH and resolve per-machine clangd-mcp-server location.
rem opencode.json uses {env:CLANGD_MCP_SERVER_JS}, so no user-specific paths are committed.
rem Persistent home is %LOCALAPPDATA%\opencode\clangd-mcp-server - %TEMP% is only a legacy
rem fallback (anything can wipe it: Disk Cleanup, storage sense, etc).
rem NOTE: opencode substitutes {env:...} as raw text before JSON parsing, so the path must use
rem forward slashes - backslashes (C:\...) break JSON escapes. Normalized below via :\=/.
where node >nul 2>nul
if errorlevel 1 if exist "%ProgramFiles%\nodejs\node.exe" set "PATH=%ProgramFiles%\nodejs;%PATH%"
if not defined CLANGD_MCP_SERVER_JS if exist "%LOCALAPPDATA%\opencode\clangd-mcp-server\dist\index.js" set "CLANGD_MCP_SERVER_JS=%LOCALAPPDATA%\opencode\clangd-mcp-server\dist\index.js"
if not defined CLANGD_MCP_SERVER_JS if exist "%ProgramData%\opencode\clangd-mcp-server\dist\index.js" set "CLANGD_MCP_SERVER_JS=%ProgramData%\opencode\clangd-mcp-server\dist\index.js"
if not defined CLANGD_MCP_SERVER_JS if exist "%TEMP%\opencode\clangd-mcp-server\dist\index.js" set "CLANGD_MCP_SERVER_JS=%TEMP%\opencode\clangd-mcp-server\dist\index.js"
if defined CLANGD_MCP_SERVER_JS set "CLANGD_MCP_SERVER_JS=%CLANGD_MCP_SERVER_JS:\=/%"
if not defined CLANGD_MCP_SERVER_JS echo [warn] CLANGD_MCP_SERVER_JS is not set and no server found. Install it to %%LOCALAPPDATA%%\opencode\clangd-mcp-server or set the env var to its dist\index.js using forward slashes.
opencode %*

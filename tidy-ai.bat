@echo off
setlocal
cd /d "%~dp0"
pwsh -NoProfile -File "%~dp0tidy-ai.ps1" %*
exit /b %ERRORLEVEL%

ECHO off
SET CURRENT_DIR=%~dp0

goto wait_loop

:wait_loop
tasklist /fi "imagename eq engine.exe" /v | find /i "Unblock Version:" >nul
if %errorlevel% == 0 (
    timeout /t 1 /nobreak >nul
    goto wait_loop
) else (
   goto close_unblock
)

:close_unblock

RD %CURRENT_DIR%\bin /S /Q
RD %CURRENT_DIR%\binaries /S /Q
RD %CURRENT_DIR%\configs /S /Q
RD %CURRENT_DIR%\ui /S /Q

ROBOCOPY %CURRENT_DIR%update\unblock %CURRENT_DIR% /E /IS /IT /COPYALL /R:0 /W:0 /NP /NJH /NJS

RD %CURRENT_DIR%\update /S /Q

start %CURRENT_DIR%bin\engine.exe
exit
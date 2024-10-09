ECHO off
chcp 1251
goto check_Permissions
:check_Permissions
    net session >nul 2>&1
    if %errorLevel% == 0 (
       call :srvdel winws1
        goto :eof

        :srvdel
        net stop %1
        sc delete %1

        schtasks /End /TN winws1
        schtasks /Delete /TN winws1 /F
        pause
        goto :eof
    ) else (
        ECHO "!ОШИБКА: Запустите с правами администратора!"
        pause
    )
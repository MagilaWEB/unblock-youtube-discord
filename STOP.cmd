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
        net stop "GoodbyeDPI"
        sc delete "GoodbyeDPI"

        schtasks /End /TN %1
        schtasks /Delete /TN %1 /F
        schtasks /End /TN winws2
        schtasks /Delete /TN winws2 /F
        pause
        goto :eof
    ) else (
        ECHO "!ОШИБКА: Запустите с правами администратора!"
        pause
    )
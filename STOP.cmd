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
        net stop "WinDivert"

        schtasks /End /TN %1
        schtasks /Delete /TN %1 /F
        pause
        goto :eof
    ) else (
        ECHO Подтвердите запуск от имени администратора:
        powershell start -verb runas '%0' am_admin & exit /b
    )

    exit
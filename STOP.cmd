ECHO off
chcp 1251
goto check_Permissions
:check_Permissions
    net session >nul 2>&1
    if %errorLevel% == 0 (
       call :srvdel unblock
        goto :eof

        :srvdel
        net stop unblock1
        sc delete unblock1
        net stop unblock2
        sc delete unblock2
        net stop "GoodbyeDPI"
        sc delete "GoodbyeDPI"
        net stop "WinDivert"

        goto :eof
    ) else (
        ECHO Подтвердите запуск от имени администратора:
        powershell start -verb runas '%0' am_admin & exit /b
    )

    exit
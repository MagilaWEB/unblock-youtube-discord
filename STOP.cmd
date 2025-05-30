ECHO off
chcp 1251
net session >nul 2>&1

if %errorLevel% == 0 (
    net stop unblock1
    sc delete unblock1
    net stop unblock2
    sc delete unblock2
    net stop "GoodbyeDPI"
    sc delete "GoodbyeDPI"
    net stop "WinDivert"

    if not defined IS_RUN (
        ECHO Все службы остановлены и удалены!
        pause
    )
) else (
    ECHO Подтвердите запуск от имени администратора:
    powershell start -verb runas '%0' am_admin & exit /b
)
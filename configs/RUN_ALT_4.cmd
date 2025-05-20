ECHO off
chcp 1251
@REM Задействовать разблокировку на весь сетевой трафик.
set ARGS=--wf-tcp=80,443 --wf-udp=443,50000-65535 ^
--filter-udp=443 --dpi-desync=fake --dpi-desync-repeats=6 --dpi-desync-fake-quic="%~dp0..\bin\quic_initial_www_google_com.bin" --new ^
--filter-udp=50000-65535 --dpi-desync=fake --dpi-desync-any-protocol=1 --dpi-desync-cutoff=d3 --dpi-desync-repeats=6 --new ^
--filter-tcp=80 --dpi-desync=fake --dpi-desync-autottl=0 --dpi-desync-fooling=md5sig --new ^
--filter-tcp=443 --dpi-desync=fake --dpi-desync-autottl=0  --dpi-desync-skip-nosni=0 --dpi-desync-fooling=badseq --dpi-desync-repeats=3 --dpi-desync-fake-quic="%~dp0..\bin\quic_initial_www_google_com.bin" --dpi-desync-fake-tls="%~dp0..\bin\tls_clienthello_www_google_com.bin"

set ARGS2=--wf-tcp=443 ^
-filter-tcp=443 --dpi-desync=fake --dpi-desync-autottl=1 --dpi-desync-fooling=md5sig --dpi-desync-repeats=3 --dpi-desync-fake-tls="%~dp0..\bin\tls_clienthello_www_google_com.bin"

set SRVCNAME=unblock1
set SRVCNAME2=unblock2

goto check_Permissions
:check_Permissions
    net session >nul 2>&1
    if %errorLevel% == 0 (
         %~dp0"../STOP.cmd"

        sc create "%SRVCNAME%" binPath= "\"%~dp0..\bin\winws.exe\" %ARGS%" DisplayName= "DPI обход блокировки : %SRVCNAME%" start= auto
        sc description "%SRVCNAME%" "DPI программное обеспечение для обхода блокировки."
        sc start "%SRVCNAME%"

        sc create "%SRVCNAME2%" binPath= "\"%~dp0..\bin\winws.exe\" %ARGS2%" DisplayName= "DPI обход блокировки : %SRVCNAME2%" start= auto
        sc description "%SRVCNAME2%" "DPI программное обеспечение для обхода блокировки."
        sc start "%SRVCNAME2%"

        @REM start %~dp0bin\winws.exe %ARGS% DisplayName= "DPI обход блокировки : %SRVCNAME%"
        
        %~dp0"RUN_RESET.cmd"
    ) else (
        ECHO !ОШИБКА: Запустите с правами администратора!
        pause
    )
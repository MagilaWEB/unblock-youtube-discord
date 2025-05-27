ECHO off
chcp 1251
@REM 2 вариант, если не работает базовый.
set ARGS=--wf-tcp=80,443 --wf-udp=443,50000-65535 ^
--filter-udp=443 --hostlist="%~dp0russia-blacklist.txt" --dpi-desync=fake --dpi-desync-repeats=11 --dpi-desync-fake-quic="%~dp0..\bin\quic_initial_www_google_com.bin" --new ^
--filter-udp=50000-65535 --filter-l7=discord,stun --dpi-desync=fake --dpi-desync-repeats=6 --new ^
--filter-tcp=80 --hostlist="%~dp0russia-blacklist.txt" --dpi-desync=fake,fakedsplit --dpi-desync-autottl=1 --dpi-desync-fooling=md5sig --new ^
--filter-tcp=443 --hostlist="%~dp0russia-blacklist.txt" --dpi-desync=fake,multidisorder --dpi-desync-split-pos=1,midsld --dpi-desync-repeats=11 --dpi-desync-fooling=md5sig --dpi-desync-fake-tls-mod=rnd,dupsid,sni=www.google.com --new ^
--filter-udp=443 --ipset="%~dp0ip-blacklist.txt" --dpi-desync=fake --dpi-desync-repeats=11 --dpi-desync-fake-quic="%~dp0..\bin\quic_initial_www_google_com.bin" --new ^
--filter-tcp=80 --ipset="%~dp0ip-blacklist.txt" --dpi-desync=fake,fakedsplit --dpi-desync-autottl=1 --dpi-desync-fooling=md5sig --new ^
--filter-tcp=443 --ipset="%~dp0ip-blacklist.txt" --dpi-desync=fake,multidisorder --dpi-desync-split-pos=1,midsld --dpi-desync-repeats=11 --dpi-desync-fooling=md5sig

set SRVCNAME=unblock1

goto check_Permissions
:check_Permissions
    net session >nul 2>&1
    if %errorLevel% == 0 (
         %~dp0"../STOP.cmd"

        sc create "%SRVCNAME%" binPath= "\"%~dp0..\bin\winws.exe\" %ARGS%" DisplayName= "DPI обход блокировки : %SRVCNAME%" start= auto
        sc description "%SRVCNAME%" "DPI программное обеспечение для обхода блокировки."
        sc start "%SRVCNAME%"
        
        @REM start %~dp0bin\winws.exe %ARGS% DisplayName= "DPI обход блокировки : %SRVCNAME%"

        %~dp0"RUN_RESET.cmd"
    ) else (
        ECHO !ОШИБКА: Запустите с правами администратора!
        pause
    )
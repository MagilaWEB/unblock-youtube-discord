ECHO off
@REM С использованием GoodbyeDPI и winws, для тех у кого winws работает некорректно.
set ARGS=--wf-tcp=80 --wf-udp=50000-65535 ^
--filter-udp=50000-65535 --dpi-desync=fake,tamper --dpi-desync-any-protocol desync-fooling=md5sig --dpi-desync-fake-quic="%~dp0..\bin\quic_initial_www_google_com.bin" --new ^
--filter-tcp=80 --dpi-desync=fake,split2 --dpi-desync-autottl=2 --dpi-desync-fooling=md5sig
set ARGS2=-5 --auto-ttl=2 --blacklist %~dp0russia-blacklist.txt

set SRVCNAME=winws1
set SRVCNAME2=GoodbyeDPI

goto check_Permissions
:check_Permissions
    net session >nul 2>&1
    if %errorLevel% == 0 (
        schtasks /End /TN %SRVCNAME%
        schtasks /Delete /TN %SRVCNAME% /F

        net stop "%SRVCNAME%"
        sc delete "%SRVCNAME%"
        sc create "%SRVCNAME%" binPath= "\"%~dp0..\bin\winws.exe\" %ARGS%" DisplayName= "DPI обход блокировки tcp-udp : %SRVCNAME%" start= auto
        sc description "%SRVCNAME%" "DPI программное обеспечение для обхода блокировки."
        sc start "%SRVCNAME%"
        
        net stop "%SRVCNAME2%"
        sc delete "%SRVCNAME2%"
        sc create "%SRVCNAME2%" binPath= "\"%~dp0..\bin\goodbyedpi.exe\" %ARGS2%" DisplayName= "DPI обход блокировки http : %SRVCNAME2%" start= auto
        sc description "%SRVCNAME2%" "Passive Deep Packet Inspection blocker and Active DPI circumvention utility"
        sc start "%SRVCNAME2%"

        schtasks /Create /F /TN winws2 /NP /RU "" /SC onstart /TR "\"%~dp0RUN_ALT_1.cmd\""
        @REM start %~dp0bin\winws.exe %ARGS% DisplayName= "DPI обход блокировки : %SRVCNAME%"
        %~dp0"RUN_RESET.cmd"
    ) else (
        ECHO !ОШИБКА: Запустите с правами администратора!
        pause
    )
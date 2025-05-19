ECHO off
@REM 2 вариант, если не работает базовый.
set ARGS=--wf-tcp=80,443 --wf-udp=443,50000-65535 ^
--filter-udp=443 --hostlist="%~dp0russia-blacklist.txt" --dpi-desync=fake --dpi-desync-repeats=6 --dpi-desync-fake-quic="%~dp0..\bin\quic_initial_www_google_com.bin" --new ^
--filter-udp=50000-65535 --dpi-desync=fake --dpi-desync-any-protocol --dpi-desync-cutoff=d3 --dpi-desync-repeats=6 --new ^
--filter-tcp=80 --dpi-desync=fake,split2 --dpi-desync-autottl=2 --dpi-desync-fooling=md5sig --new ^
--filter-tcp=443 --hostlist="%~dp0russia-blacklist.txt" --dpi-desync=split2 --dpi-desync-split-seqovl=652 --dpi-desync-split-pos=2 --dpi-desync-fake-tls="%~dp0..\bin\tls_clienthello_www_google_com.bin"

set SRVCNAME=winws1

goto check_Permissions
:check_Permissions
    net session >nul 2>&1
    if %errorLevel% == 0 (
        net stop "GoodbyeDPI"
        sc delete "GoodbyeDPI"
        net stop "WinDivert"
        net stop "%SRVCNAME%"
        sc delete "%SRVCNAME%"
        sc create "%SRVCNAME%" binPath= "\"%~dp0..\bin\winws.exe\" %ARGS%" DisplayName= "DPI обход блокировки : %SRVCNAME%" start= auto
        sc description "%SRVCNAME%" "DPI программное обеспечение для обхода блокировки."
        sc start "%SRVCNAME%"

        schtasks /End /TN %SRVCNAME%
        schtasks /Delete /TN %SRVCNAME% /F
        schtasks /Create /F /TN %SRVCNAME% /NP /RU "" /SC onstart /TR "\"%~dp0RUN_2.cmd\""
        @REM start %~dp0bin\winws.exe %ARGS% DisplayName= "DPI обход блокировки : %SRVCNAME%"

        %~dp0"RUN_RESET.cmd"
    ) else (
        ECHO !ОШИБКА: Запустите с правами администратора!
        pause
    )
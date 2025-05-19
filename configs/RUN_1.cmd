ECHO off
@REM �������, �������� ��� �����������.
set ARGS=--wf-tcp=80,443 --wf-udp=443,50000-65535 ^
--filter-udp=443 --hostlist="%~dp0russia-blacklist.txt" --dpi-desync=fake --dpi-desync-udplen-increment=10 --dpi-desync-repeats=6 --dpi-desync-udplen-pattern=0xDEADBEEF --dpi-desync-fake-quic="%~dp0..\bin\quic_initial_www_google_com.bin" --new ^
--filter-udp=50000-65535 --dpi-desync=fake,tamper --dpi-desync-any-protocol desync-fooling=md5sig --dpi-desync-fake-quic="%~dp0..\bin\quic_initial_www_google_com.bin" --new ^
--filter-tcp=80 --dpi-desync=fake,split2 --dpi-desync-autottl=2 --dpi-desync-fooling=md5sig --new ^
--filter-tcp=443 --hostlist="%~dp0russia-blacklist.txt" --dpi-desync=fake,split --dpi-desync-autottl=5 --dpi-desync-repeats=6 --dpi-desync-fooling=md5sig --dpi-desync-fake-tls="%~dp0..\bin\tls_clienthello_www_google_com.bin"

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
        sc create "%SRVCNAME%" binPath= "\"%~dp0..\bin\winws.exe\" %ARGS%" DisplayName= "DPI ����� ���������� : %SRVCNAME%" start= auto
        sc description "%SRVCNAME%" "DPI ����������� ����������� ��� ������ ����������."
        sc start "%SRVCNAME%"

        schtasks /End /TN %SRVCNAME%
        schtasks /Delete /TN %SRVCNAME% /F
        schtasks /Create /F /TN winws1 /NP /RU "" /SC onstart /TR "\"%~dp0RUN_1.cmd\""
        @REM start %~dp0bin\winws.exe %ARGS% DisplayName= "DPI ����� ���������� : %SRVCNAME%"
        %~dp0"RUN_RESET.cmd"
    ) else (
        ECHO !������: ��������� � ������� ��������������!
        pause
    )
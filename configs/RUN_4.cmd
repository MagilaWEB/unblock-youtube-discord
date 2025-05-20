ECHO off
chcp 1251
@REM ���� �� �������� ������� �������� �� ������, ����������, ��������.
set ARGS=--wf-tcp=80,443 --wf-udp=443,50000-65535 ^
--filter-udp=443 --hostlist="%~dp0russia-blacklist.txt" --dpi-desync=fake --dpi-desync-repeats=6 --dpi-desync-fake-quic="%~dp0..\bin\quic_initial_www_google_com.bin" --new ^
--filter-udp=50000-65535 --dpi-desync=fake --dpi-desync-any-protocol --dpi-desync-cutoff=d3 --dpi-desync-repeats=6 --new ^
--filter-tcp=80 --dpi-desync=fake,split2 --dpi-desync-autottl=2 --dpi-desync-fooling=md5sig --new ^
--filter-tcp=443 --hostlist="%~dp0russia-blacklist.txt" --dpi-desync=fake,split2 --dpi-desync-ttl=1 --dpi-desync-autottl=5 --dpi-desync-autottl --dpi-desync-repeats=6 --dpi-desync-fake-tls="%~dp0..\bin\tls_clienthello_www_google_com.bin" ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata --dpi-desync-fake-syndata="%~dp0..\bin\tls_clienthello_iana_org.bin" ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,split2 ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,split2 --dpi-desync-fake-syndata="%~dp0..\bin\tls_clienthello_iana_org.bin" ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,disorder2 ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,disorder2 --dpi-desync-fake-syndata="%~dp0..\bin\tls_clienthello_iana_org.bin" ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata --wssize=1:6 ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata --dpi-desync-fake-syndata="%~dp0..\bin\tls_clienthello_iana_org.bin" --wssize=1:6 ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,split2 --wssize=1:6 ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,split2 --dpi-desync-fake-syndata="%~dp0..\bin\tls_clienthello_iana_org.bin" --wssize=1:6 ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,disorder2 --wssize=1:6 ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,disorder2 --dpi-desync-fake-syndata="%~dp0..\bin\tls_clienthello_iana_org.bin" --wssize=1:6

set SRVCNAME=unblock1

goto check_Permissions
:check_Permissions
    net session >nul 2>&1
    if %errorLevel% == 0 (
         %~dp0"../STOP.cmd"

        sc create "%SRVCNAME%" binPath= "\"%~dp0..\bin\winws.exe\" %ARGS%" DisplayName= "DPI ����� ���������� : %SRVCNAME%" start= auto
        sc description "%SRVCNAME%" "DPI ����������� ����������� ��� ������ ����������."
        sc start "%SRVCNAME%"

        @REM start %~dp0bin\winws.exe %ARGS% DisplayName= "DPI ����� ���������� : %SRVCNAME%"

        %~dp0"RUN_RESET.cmd"
    ) else (
        ECHO !������: ��������� � ������� ��������������!
        pause
    )
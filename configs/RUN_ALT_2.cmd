ECHO off
chcp 1251
@REM � �������������� GoodbyeDPI � winws, ��� ��� � ���� winws �������� �����������, ��������� ��� ��������� ����.
set ARGS=--wf-tcp=80 --wf-udp=50000-65535 ^
--filter-udp=50000-65535 --dpi-desync=fake,tamper --dpi-desync-any-protocol=1 desync-fooling=md5sig --dpi-desync-fake-quic="%~dp0..\bin\quic_initial_www_google_com.bin" --new ^
--filter-tcp=80 --dpi-desync=fake,split2 --dpi-desync-autottl=2 --dpi-desync-fooling=md5sig
set ARGS2=-e 1 --fake-from-hex 1603030135010001310303424143facf5c983ac8ff20b819cfd634cbf5143c0005b2b8b142a6cd335012c220008969b6b387683dedb4114d466ca90be3212b2bde0c4f56261a9801 ^
-q --native-frag --set-ttl 3 --fake-gen 15 --blacklist %~dp0russia-blacklist.txt

set SRVCNAME=unblock1
set SRVCNAME2=GoodbyeDPI

goto check_Permissions
:check_Permissions
    net session >nul 2>&1
    if %errorLevel% == 0 (
         %~dp0"../STOP.cmd"

        sc create "%SRVCNAME%" binPath= "\"%~dp0..\bin\winws.exe\" %ARGS%" DisplayName= "DPI ����� ���������� tcp-udp : %SRVCNAME%" start= auto
        sc description "%SRVCNAME%" "DPI ����������� ����������� ��� ������ ����������."
        sc start "%SRVCNAME%"
        
        sc create "%SRVCNAME2%" binPath= "\"%~dp0..\bin\goodbyedpi.exe\" %ARGS2%" DisplayName= "DPI ����� ���������� http : %SRVCNAME2%" start= auto
        sc description "%SRVCNAME2%" "Passive Deep Packet Inspection blocker and Active DPI circumvention utility"
        sc start "%SRVCNAME2%"

        @REM start %~dp0bin\winws.exe %ARGS% DisplayName= "DPI ����� ���������� : %SRVCNAME%"

        %~dp0"RUN_RESET.cmd"
    ) else (
        ECHO !������: ��������� � ������� ��������������!
        pause
    )
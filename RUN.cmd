@ECHO off
@chcp 1251
@setlocal

net session >nul 2>&1
    if not %errorLevel% == 0 (
        ECHO ����������� ������ �� ����� ��������������!
        powershell start -verb runas '%0' am_admin & exit /b
        exit
    )

goto START_MAIN

:START_MAIN
@CLS

@SET BIN=%~dp0bin\
@SET CONFIGS=%~dp0configs\

@MODE con: cols=160 lines=48

CALL %BIN%text_color.cmd

@ECHO %ESC%[92m�������� ������� ������ ����������:%ESC%[0m
@ECHO - %ESC%[93m������� 1, ��� ������� �������� ������� (����, ���, �������, Yota, ����������, Tele2).%ESC%[0m
@ECHO - %ESC%[93m������� 2, ��� ������� ��������������� ������� ���� 1 �� �������� (�������������).%ESC%[0m
@ECHO - %ESC%[93m������� 3, ��� ������� ��������������� �������, ����������� ��� ���� �� �������� 1 � 2 (�������������).%ESC%[0m
@ECHO - %ESC%[93m������� 4, ��� ������� ������� ��� ����������� (������, ����������, ��������, �������, Yota).%ESC%[0m
@ECHO - %ESC%[93m������� 5, ��� ������� ������� ��� ���������� (����, ���).%ESC%[0m
@ECHO - %ESC%[93m������� 6, ��� ������� ������� � �������������� ������ GoodbyeDPI � winws (���, �������, Yota, ����������).%ESC%[0m
@ECHO - %ESC%[93m������� 7, ��� ������� ������� � �������������� ������ GoodbyeDPI � winws (����, ����������, �������, Yota, Tele2).%ESC%[0m
@ECHO - %ESC%[93m������� 8, ��� ������� ������� (����, ���, �������, Yota, ����������, Tele2).%ESC%[0m
@ECHO - %ESC%[93m������� 9, ��� ������� ������� ������� ������� (�������������).%ESC%[0m
@ECHO:

@SET mt=5

@CHOICE /C 123456789 /T 100 /D 1 /M "���� �� �� �������� ������� ����� 100 ������ ����� ������ 1 ��� �� ���������!"

@SET mt2=%errorLevel%
@SET /a var1=(%mt2% - %mt%)

@SET SRVCNAME=unblock1
@SET SRVCNAME2=unblock2
@SET SRVCNAMEDPI=GoodbyeDPI

@SET SRVCNAMESTART=false
@SET SRVCNAMESTART2=false
@SET SRVCNAMESTARTDPI=false

@SET BLOCKLIST=
@SET FAKE_QUIC=
@SET FAKE_TLS=

@ECHO:
@ECHO:
@ECHO %ESC%[92m��������� ����� ���������� �� ���� ������� ������ ��?%ESC%[0m
@ECHO %ESC%[92m����� 1 ����� ��������� �������� �� ������ � ������ ������� �� ��������� � ����������!%ESC%[0m
@ECHO - %ESC%[93m������� 1, ��� ���� ����� ��������� ����� ���������� �� ���� ������� ������ ��.%ESC%[0m
@ECHO - %ESC%[93m������� 2, ��� ���� ����� ��������� ����� ���������� ������ ��� Discord.com, YouTube.com, x.com.%ESC%[0m
@CHOICE /C 12 /T 100 /D 2 /M "�������� �������, ����� 100 ������ ����� ������ ������� 2!"

@if %errorLevel% == 2 @SET BLOCKLIST=--hostlist="%CONFIGS%russia-blacklist.txt"

@ECHO:
@ECHO:
@ECHO %ESC%[92m��� ��� ����������� DPI ������?%ESC%[0m
@ECHO %ESC%[92m������ ����� �������� ������ ��������� � ������� ������ DPI ����� ������� ����� ��� ��������� ������, �������� vk.com, � ������ ������ ������� ��� ����� �������!%ESC%[0m
@ECHO - %ESC%[93m������� 1, ��� ���� ����� �� ����������� DPI.%ESC%[0m
@ECHO - %ESC%[93m������� 2, ��� ���� ����� ����������� DPI ��� vk.com.%ESC%[0m
@ECHO - %ESC%[93m������� 3, ��� ���� ����� ����������� DPI ��� google.com.%ESC%[0m

@CHOICE /C 123 /T 100 /D 1 /M "�������� �������, ����� 100 ������ ����� ������ ������� 1!"

@if %errorLevel% == 2 (
    @SET FAKE_QUIC=--dpi-desync-fake-quic="%BIN%quic_initial_vk_com.bin"
    @SET FAKE_TLS=--dpi-desync-fake-tls="%BIN%tls_clienthello_vk_com.bin"
)

@if %errorLevel% == 3 (
    @SET FAKE_QUIC=--dpi-desync-fake-quic="%BIN%quic_initial_www_google_com.bin"
    @SET FAKE_TLS=--dpi-desync-fake-tls="%BIN%tls_clienthello_www_google_com.bin"
)

@SET IS_RUN=true
CALL %~dp0STOP.cmd

@if %mt2% LEQ 5 (
   CALL %CONFIGS%RUN_%mt2%.cmd
) else (
   CALL %CONFIGS%RUN_ALT_%var1%.cmd
)

@if "%SRVCNAMESTART%"=="true" (
    sc create "%SRVCNAME%" binPath= "\"%BIN%winws.exe\" %ARGS%" DisplayName= "DPI ����� ���������� : %SRVCNAME%" start= auto
    sc description "%SRVCNAME%" "DPI ����������� ����������� ��� ������ ����������."
    sc start "%SRVCNAME%"
)

@if "%SRVCNAMESTART2%"=="true" (
    sc create "%SRVCNAME2%" binPath= "\"%BIN%winws.exe\" %ARGS2%" DisplayName= "DPI ����� ���������� : %SRVCNAME2%" start= auto
    sc description "%SRVCNAME2%" "DPI2 ����������� ����������� ��� ������ ����������."
    sc start "%SRVCNAME2%"
)

@if "%SRVCNAMESTARTDPI%"=="true" (
    sc create "%SRVCNAMEDPI%" binPath= "\"%BIN%goodbyedpi.exe\" %ARGSDPI%" DisplayName= "DPI ����� ���������� http : %SRVCNAMEDPI%" start= auto
    sc description "%SRVCNAMEDPI%" "DPI ����������� ����������� ��� ��������� �������� �������� �������, � ��� �� ��������� ������ DPI."
    sc start "%SRVCNAMEDPI%"
)

@SET ARGS=
@SET ARGS2=
@SET ARGSDPI=
@SET SRVCNAMESTART=false
@SET SRVCNAMESTART2=false
@SET SRVCNAMESTARTDPI=false
@SET BLOCKLIST=
@SET FAKE_QUIC=
@SET FAKE_TLS=

@ECHO:
@ECHO:
@ECHO %ESC%[92m��������� �����������������, ���� �� ��������:
@ECHO - ���������� ������� ������ ������� � ������� �����������. �������� ������� ����� DPI �� ����� google.com � vk.com
@ECHO - ���������� ������� ����� ���������� �� ��� ���� �� ��� ��������.%ESC%[0m
@ECHO - %ESC%[91m**���������**, %ESC%[92m���������� �� �� ����� �� ����� ��������������.%ESC%[0m
@ECHO - %ESC%[91m**���������**, %ESC%[92m��� � ���� � ������ ��� �������� � ������� ��������.%ESC%[0m
@ECHO - %ESC%[91m**���������**, %ESC%[92m��� � ��� �� ������� ����������, �� ����� ������ �������� �����.%ESC%[0m
@ECHO - %ESC%[91m**���������**, %ESC%[92m��� ��������������� �� �� ������� ����, �������������� � ������ ����� C:\ ��� � ������ ����� D:\ (�� ������ ����������).%ESC%[0m
@ECHO - %ESC%[92m���� ������ �� ��������, ������� 3 � ������� � ����� �� ������� � ����� ��� ���������, ������ � ���� ����������� ����� �������.%ESC%[0m
@ECHO:
@ECHO %ESC%[92m������� ������ �������?%ESC%[0m
@ECHO ������� 1, ����� ������� ������ �������.
@CHOICE /C 123 /T 1000 /D 2 /M "������� 2 ����� ���������."

@if %errorLevel% == 1 goto START_MAIN

@if %errorLevel% == 3 start https://github.com/MagilaWEB/unblock-youtube-discord/issues

EXIT /D 0
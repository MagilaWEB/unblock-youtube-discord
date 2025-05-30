@ECHO off
@chcp 1251
@setlocal

net session >nul 2>&1
    if not %errorLevel% == 0 (
        ECHO Подтвердите запуск от имени администратора!
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

@ECHO %ESC%[92mВыберите профиль обхода блокировки:%ESC%[0m
@ECHO - %ESC%[93mНажмите 1, для запуска базового профиля (МГТС, МТС, Мегафон, Yota, Ростелеком, Tele2).%ESC%[0m
@ECHO - %ESC%[93mНажмите 2, для запуска альтернативного способа если 1 не сработал (Универсальный).%ESC%[0m
@ECHO - %ESC%[93mНажмите 3, для запуска альтернативного способа, используйте его если не работает 1 и 2 (Универсальный).%ESC%[0m
@ECHO - %ESC%[93mНажмите 4, для запуска профиля для провайдеров (Билайн, Ростелеком, Инфолинк, Мегафон, Yota).%ESC%[0m
@ECHO - %ESC%[93mНажмите 5, для запуска профиля для провайдера (МГТС, МТС).%ESC%[0m
@ECHO - %ESC%[93mНажмите 6, для запуска профиля с использованием службы GoodbyeDPI и winws (МТС, Мегафон, Yota, Ростелеком).%ESC%[0m
@ECHO - %ESC%[93mНажмите 7, для запуска профиля с использованием службы GoodbyeDPI и winws (МГТС, Ростелеком, Мегафон, Yota, Tele2).%ESC%[0m
@ECHO - %ESC%[93mНажмите 8, для запуска профиля (МГТС, МТС, Мегафон, Yota, Ростелеком, Tele2).%ESC%[0m
@ECHO - %ESC%[93mНажмите 9, для запуска профиля двойным обходом (Универсальный).%ESC%[0m
@ECHO:

@SET mt=5

@CHOICE /C 123456789 /T 100 /D 1 /M "Если вы не выберите профиль через 100 секунд будет выбран 1 как по умолчанию!"

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
@ECHO %ESC%[92mПрименить обход блокировки на весь сетевой трафик ОС?%ESC%[0m
@ECHO %ESC%[92mРежим 1 может негативно повлиять на доступ к сайтам которые не находятся в блокировке!%ESC%[0m
@ECHO - %ESC%[93mНажмите 1, для того чтобы применить обход блокировки на весь сетевой трафик ОС.%ESC%[0m
@ECHO - %ESC%[93mНажмите 2, для того чтобы применить обход блокировки только для Discord.com, YouTube.com, x.com.%ESC%[0m
@CHOICE /C 12 /T 100 /D 2 /M "Выберите вариант, через 100 секунд будет выбран вариант 2!"

@if %errorLevel% == 2 @SET BLOCKLIST=--hostlist="%CONFIGS%russia-blacklist.txt"

@ECHO:
@ECHO:
@ECHO %ESC%[92mПод что маскировать DPI пакеты?%ESC%[0m
@ECHO %ESC%[92mДанный режим помогает службе подменять в пакетах данных DPI таким образом будто это сторонний ресурс, например vk.com, в крайне редких случаях это может вредить!%ESC%[0m
@ECHO - %ESC%[93mНажмите 1, для того чтобы не маскировать DPI.%ESC%[0m
@ECHO - %ESC%[93mНажмите 2, для того чтобы маскировать DPI под vk.com.%ESC%[0m
@ECHO - %ESC%[93mНажмите 3, для того чтобы маскировать DPI под google.com.%ESC%[0m

@CHOICE /C 123 /T 100 /D 1 /M "Выберите вариант, через 100 секунд будет выбран вариант 1!"

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
    sc create "%SRVCNAME%" binPath= "\"%BIN%winws.exe\" %ARGS%" DisplayName= "DPI обход блокировки : %SRVCNAME%" start= auto
    sc description "%SRVCNAME%" "DPI программное обеспечение для обхода блокировки."
    sc start "%SRVCNAME%"
)

@if "%SRVCNAMESTART2%"=="true" (
    sc create "%SRVCNAME2%" binPath= "\"%BIN%winws.exe\" %ARGS2%" DisplayName= "DPI обход блокировки : %SRVCNAME2%" start= auto
    sc description "%SRVCNAME2%" "DPI2 программное обеспечение для обхода блокировки."
    sc start "%SRVCNAME2%"
)

@if "%SRVCNAMESTARTDPI%"=="true" (
    sc create "%SRVCNAMEDPI%" binPath= "\"%BIN%goodbyedpi.exe\" %ARGSDPI%" DisplayName= "DPI обход блокировки http : %SRVCNAMEDPI%" start= auto
    sc description "%SRVCNAMEDPI%" "DPI программное обеспечение для пассивной глубокой проверки пакетов, а так же активного обхода DPI."
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
@ECHO %ESC%[92mПроверьте работоспособность, если не работает:
@ECHO - Попробуйте выбрать другой профиль с другими параметрами. Например выбрать обход DPI не через google.com а vk.com
@ECHO - Попробуйте выбрать обход блокировки на всю сеть ОС или наоборот.%ESC%[0m
@ECHO - %ESC%[91m**Проверьте**, %ESC%[92mзапускаете ли вы файлы от имени администратора.%ESC%[0m
@ECHO - %ESC%[91m**Проверьте**, %ESC%[92mчто в пути к файлам нет пробелов и русских символов.%ESC%[0m
@ECHO - %ESC%[91m**Убедитесь**, %ESC%[92mчто у вас не включён брандмауэр, он может мешать работать софту.%ESC%[0m
@ECHO - %ESC%[91m**Убедитесь**, %ESC%[92mчто разархивировали не на рабочий стол, разархивируйте в корень диска C:\ или в корень диска D:\ (по вашему усмотрению).%ESC%[0m
@ECHO - %ESC%[92mЕсли ничего не помогает, нажмите 3 и опишите в каком вы регионе и какой ваш провайдер, вместе с вами постараемся найти решение.%ESC%[0m
@ECHO:
@ECHO %ESC%[92mВыбрать другой профиль?%ESC%[0m
@ECHO Нажмите 1, чтобы выбрать другой профиль.
@CHOICE /C 123 /T 1000 /D 2 /M "Нажмите 2 чтобы завершить."

@if %errorLevel% == 1 goto START_MAIN

@if %errorLevel% == 3 start https://github.com/MagilaWEB/unblock-youtube-discord/issues

EXIT /D 0
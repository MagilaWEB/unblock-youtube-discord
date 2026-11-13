; Unblock — онлайн-установщик (Inno Setup 6.3+).
;
; Сам установщик маленький: payload (unblock.7z) он качает с последнего релиза
; GitHub в момент установки. Распаковка — встроенными средствами Inno
; (ExtractArchive + ArchiveExtraction=full), поэтому никакой внешний 7za.exe не
; нужен и в релиз ничего лишнего не добавляется.
;
; Сборка (вызывает release-ai.ps1):
;   ISCC.exe /DAppVersion=1.10.0 installer\unblock.iss
; Параметры-определения:
;   AppVersion  — версия для визарда и «Программ и компонентов»;
;   PayloadUrl  — откуда брать payload (по умолчанию — последний релиз).

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef PayloadUrl
  #define PayloadUrl "https://github.com/MagilaWEB/unblock-youtube-discord/releases/latest/download/unblock.7z"
#endif
#ifndef PayloadName
  #define PayloadName "unblock.7z"
#endif

[Setup]
AppId={{7C4A9E3B-2F51-4D8A-9B6E-5A1D3C8F0E72}
AppName=Unblock
AppVersion={#AppVersion}
AppVerName=Unblock
AppPublisher=MAGILA
AppPublisherURL=https://github.com/MagilaWEB/unblock-youtube-discord
AppSupportURL=https://github.com/MagilaWEB/unblock-youtube-discord/issues
AppUpdatesURL=https://github.com/MagilaWEB/unblock-youtube-discord/releases
DefaultDirName={sd}\unblock
DisableDirPage=no
DefaultGroupName=Unblock
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\release
OutputBaseFilename=unblock-setup
SetupIconFile=..\src\engine\unblock.ico
; Картинок в визарде нет: пустые значения убирают их, раскладка не меняется.
WizardImageFile=
WizardSmallImageFile=
UninstallDisplayIcon={app}\bin\engine.exe
LicenseFile=..\LICENSE.txt
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern dark
; Фон визарда — цвет из палитры приложения (тёмный), а не системный.
WizardBackColor=#1a1b2e
; Выбор языка показываем только когда Windows не совпал ни с русским, ни с
; английским: родной список Inno выделяет системным синим, а на тёмном фоне это
; выглядит чужеродно. Явно задать язык можно ключом /LANG=<имя>.
; Диалог выбора языка рисует выделение системным clHighlight — на тёмном фоне это
; ярко-синяя плашка, перекрасить её штатно нечем (нужен свой .vsf). Поэтому диалога
; нет: язык ставится по языку интерфейса Windows, первый в списке — запасной.
ShowLanguageDialog=no
LanguageDetectionMethod=uilanguage
; Полноценный распаковщик (значение full): умеет любые 7z, включая собранные с
; фильтром BCJ2, который 7-Zip сам включает для exe при -mx=9. Установщик от
; этого тяжелее примерно на полтора мегабайта.
ArchiveExtraction=full

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
russian.PayloadTitle=Загрузка Unblock
russian.PayloadSubtitle=Установщик скачивает последнюю версию с GitHub — это займёт меньше минуты.
russian.PayloadProgress=Скачиваю: %1 из %2 · %3 МБ/с
russian.PayloadProgressNoTotal=Скачано: %1 · %2 МБ/с
russian.PayloadDownloadFailed=Не удалось скачать архив с GitHub.%n%n%1
russian.PayloadUnpackFailed=Архив скачан, но распаковать его не удалось.%n%n%1
english.PayloadTitle=Downloading Unblock
english.PayloadSubtitle=The installer downloads the latest version from GitHub — this takes less than a minute.
english.PayloadProgress=Downloading: %1 of %2 · %3 MB/s
english.PayloadProgressNoTotal=Downloaded: %1 · %2 MB/s
english.PayloadDownloadFailed=Could not download the archive from GitHub.%n%n%1
english.PayloadUnpackFailed=The archive was downloaded but could not be unpacked.%n%n%1

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Icons]
Name: "{autoprograms}\Unblock"; Filename: "{app}\bin\engine.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Unblock"; Filename: "{app}\bin\engine.exe"; WorkingDir: "{app}"; Tasks: desktopicon

; Обновление поверх прошлой версии: код и ресурсы заменяются целиком, а user/
; и logs/ (настройки и журналы) остаются на месте.
[InstallDelete]
Type: filesandordirs; Name: "{app}\bin"
Type: filesandordirs; Name: "{app}\binaries"
Type: filesandordirs; Name: "{app}\configs"
Type: filesandordirs; Name: "{app}\ui"

; Payload распаковывается из кода прямо в папку установки (см. CurStepChanged),
; поэтому [Files] не нужен. А вот удаление должно снести то, что поставили.
[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Run]
Filename: "{app}\bin\engine.exe"; Description: "{cm:LaunchProgram,Unblock}"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent

; Уборку делает само приложение: только оно знает про hosts-пины и про возврат
; адаптерам их DNS. Список sc ниже — страховка на случай, когда файлов уже нет.
[UninstallRun]
Filename: "{app}\bin\engine.exe"; Parameters: "--uninstall"; Flags: runhidden waituntilterminated; RunOnceId: "AppUninstall"
Filename: "{sys}\sc.exe"; Parameters: "stop zapret1"; Flags: runhidden; RunOnceId: "StopZapret1"
Filename: "{sys}\sc.exe"; Parameters: "stop zapret2"; Flags: runhidden; RunOnceId: "StopZapret2"
Filename: "{sys}\sc.exe"; Parameters: "stop zapret2_helper"; Flags: runhidden; RunOnceId: "StopHelper"
Filename: "{sys}\sc.exe"; Parameters: "stop unblock_dns"; Flags: runhidden; RunOnceId: "StopDns"
Filename: "{sys}\sc.exe"; Parameters: "stop TgWsProxy"; Flags: runhidden; RunOnceId: "StopTg"
Filename: "{sys}\sc.exe"; Parameters: "delete zapret1"; Flags: runhidden; RunOnceId: "DelZapret1"
Filename: "{sys}\sc.exe"; Parameters: "delete zapret2"; Flags: runhidden; RunOnceId: "DelZapret2"
Filename: "{sys}\sc.exe"; Parameters: "delete zapret2_helper"; Flags: runhidden; RunOnceId: "DelHelper"
Filename: "{sys}\sc.exe"; Parameters: "delete unblock_dns"; Flags: runhidden; RunOnceId: "DelDns"
Filename: "{sys}\sc.exe"; Parameters: "delete TgWsProxy"; Flags: runhidden; RunOnceId: "DelTg"
Filename: "{sys}\sc.exe"; Parameters: "delete WinDivert"; Flags: runhidden; RunOnceId: "DelWinDivert"
Filename: "{sys}\netsh.exe"; Parameters: "interface tcp set global timestamps=disabled"; Flags: runhidden; RunOnceId: "TcpTimestamps"

[Code]
var
  DownloadPage:  TDownloadWizardPage;
  PayloadReady:  Boolean;
  LatestVersion: String;
  SpeedSample:   Int64;
  SpeedStamp:    Cardinal;

// Часы для замера скорости: в Pascal Script у Inno своих нет, берём Win32-счётчик
// миллисекунд напрямую (обёртка через 49 суток нам безразлична).
function WinGetTickCount: Cardinal; external 'GetTickCount@kernel32.dll stdcall';

function NowMs: Cardinal;
begin
  Result := WinGetTickCount;
end;

// «12,3 MB» / «512 KB» / «900 B» — единицы латиницей, они одинаковы везде.
// Важно: `%f` в Inno требует Real, поэтому делим на вещественное, а для KB/B
// округляем и печатаем как целое — целочисленный аргумент с `%f` несовместим.
function FormatSize(Bytes: Int64): String;
begin
  if Bytes >= 1024 * 1024 then
    Result := Format('%.1f MB', [Bytes / 1048576.0])
  else if Bytes >= 1024 then
    Result := Format('%d KB', [Round(Bytes / 1024.0)])
  else
    Result := Format('%d B', [Bytes]);
end;

// Прогресс скачивания. Страница загрузки штатная (CreateDownloadPage), поэтому
// ничего своего не рисуем: только меняем её текст и полоску. Своя надпись поверх
// тёмного стиля давала серые плашки, а тут фон рисует сам Inno.
function OnPayloadProgress(const Url, FileName: String; const Progress, ProgressMax: Int64): Boolean;
var
  Stamp, Elapsed: Cardinal;
  Speed: Double;
  Line: String;
begin
  Stamp := NowMs;

  if (Stamp - SpeedStamp >= 200) or ((ProgressMax > 0) and (Progress >= ProgressMax)) then
  begin
    Elapsed := Stamp - SpeedStamp;
    if Elapsed < 1 then
      Elapsed := 1;

    Speed := (Progress - SpeedSample) * 1000.0 / Elapsed / 1048576;

    if ProgressMax > 0 then
      Line := FmtMessage(CustomMessage('PayloadProgress'), [FormatSize(Progress), FormatSize(ProgressMax), Format('%.1f', [Speed])])
    else
      Line := FmtMessage(CustomMessage('PayloadProgressNoTotal'), [FormatSize(Progress), Format('%.1f', [Speed])]);

    if LatestVersion <> '' then
      Line := 'Unblock ' + LatestVersion + ' · ' + Line;

    // Вторая строка — та же, что пишет сама Inno (имя файла): если писать туда
    // своё, строки мигают, подменяя друг друга на каждом обновлении.
    DownloadPage.SetText(Line, '{#PayloadName}');

    SpeedSample := Progress;
    SpeedStamp  := Stamp;
  end;

  if ProgressMax > 0 then
    DownloadPage.SetProgress(Progress, ProgressMax);

  Result := True;
end;

// Прогресс распаковки: своей полоски больше нет, распаковка идёт молча.
function OnPayloadExtract(const ArchiveName, FileName: String; const Progress, ProgressMax: Int64): Boolean;
begin
  Result := True;
end;

// Достаёт строковое поле из JSON без парсера: нужен только tag_name.
function ExtractJsonField(const Json, Field: String): String;
var
  Start1, Start2: Integer;
begin
  Result := '';
  Start1 := Pos('"' + Field + '":"', Json);
  if Start1 = 0 then
    Exit;
  Start1 := Start1 + Length(Field) + 4;
  Start2 := Pos('"', Copy(Json, Start1, Length(Json)));
  if Start2 = 0 then
    Exit;
  Result := Copy(Json, Start1, Start2 - 1);
end;

// Версия последнего релиза — тем же способом, что и апдейтер приложения:
// запрос к API GitHub (для него обязателен User-Agent).
function FetchLatestVersion: String;
var
  Request: Variant;
begin
  Result := '';
  try
    Request := CreateOleObject('WinHttp.WinHttpRequest.5.1');
    Request.Open('GET', 'https://api.github.com/repos/MagilaWEB/unblock-youtube-discord/releases/latest', False);
    Request.SetRequestHeader('User-Agent', 'Unblock-Installer');
    Request.SetTimeouts(10000, 15000, 15000, 15000);
    Request.Send();
    if Request.Status = 200 then
      Result := ExtractJsonField(Request.ResponseText, 'tag_name');
  except
    Result := '';
  end;
end;

// Скачивание показывает штатная страница Inno: она сама рисует полоску, текст и
// кнопку отмены. Исключение ловим здесь, чтобы показать внятную ошибку и дать
// повторить попытку — страница при этом всегда скрывается (иначе визард залипнет).
function DownloadPayload: Boolean;
begin
  Result := False;
  PayloadReady := False;
  LatestVersion := FetchLatestVersion;
  SpeedSample := 0;
  SpeedStamp  := NowMs;

  DownloadPage.Clear;
  DownloadPage.Add('{#PayloadUrl}', '{#PayloadName}', '');
  DownloadPage.Show;
  try
    try
      DownloadPage.Download;
      PayloadReady := True;
      Result := True;
    except
      if DownloadPage.AbortedByUser then
        Log('Payload download aborted by user')
      else
        SuppressibleMsgBox(
          FmtMessage(CustomMessage('PayloadDownloadFailed'), [GetExceptionMessage]),
          mbCriticalError, MB_OK, IDOK
        );
    end;
  finally
    DownloadPage.Hide;
  end;
end;

procedure InitializeWizard;
begin
  DownloadPage := CreateDownloadPage(CustomMessage('PayloadTitle'), CustomMessage('PayloadSubtitle'), @OnPayloadProgress);
  DownloadPage.ShowBaseNameInsteadOfUrl := True;

  // Синее выделение в поле пути: Inno выделяет весь путь при входе на страницу,
  // и выделение оставалось висеть даже после ухода из поля.
  WizardForm.DirEdit.AutoSelect := False;
  WizardForm.DirEdit.HideSelection := True;
end;

// Перед заменой файлов службы надо остановить: они держат bin/. Ждём каждую
// (net stop дожидается остановки, движки гаснут не мгновенно), отсутствующие
// службы просто дают ошибку в скрытом выводе.
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Exec(ExpandConstant('{sys}\net.exe'), 'stop zapret1', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Exec(ExpandConstant('{sys}\net.exe'), 'stop zapret2', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Exec(ExpandConstant('{sys}\net.exe'), 'stop zapret2_helper', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Exec(ExpandConstant('{sys}\net.exe'), 'stop unblock_dns', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Exec(ExpandConstant('{sys}\net.exe'), 'stop TgWsProxy', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);

  Result := '';
end;

procedure CurPageChanged(CurPageID: Integer);
var
  DefaultDir: String;
begin
  // Inno по умолчанию (UsePreviousAppDir) подставляет папку прошлой установки,
  // поэтому удалённая установка оставляет после себя мёртвый путь. Если такой
  // папки на диске нет — показываем обычный путь по умолчанию.
  if CurPageID = wpSelectDir then
  begin
    DefaultDir := ExpandConstant('{sd}\unblock');
    if (WizardDirValue <> DefaultDir) and (not DirExists(WizardDirValue)) then
      WizardForm.DirEdit.Text := DefaultDir;
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;

  // Качаем на шаге «Всё готово»: страница прогресса показывается поверх и уходит
  // сама. Если сеть отвалилась — остаёмся здесь и повторяем по «Далее».
  if (CurPageID = wpReady) and (not PayloadReady) then
    Result := DownloadPayload;
end;

// Inno не умеет копировать каталоги из кода: обходим дерево сами. Внутри
// архива лежит папка unblock, её содержимое и есть файлы приложения.
procedure CopyDir(const SourceDir, DestDir: String);
var
  FindRec: TFindRec;
  Source, Dest: String;
begin
  if not ForceDirectories(DestDir) then
    RaiseException(Format('Cannot create "%s"', [DestDir]));

  if not FindFirst(SourceDir + '\*', FindRec) then
    Exit;

  try
    repeat
      if (FindRec.Name <> '.') and (FindRec.Name <> '..') then
      begin
        Source := SourceDir + '\' + FindRec.Name;
        Dest   := DestDir + '\' + FindRec.Name;

        if (FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
          CopyDir(Source, Dest)
        else if not FileCopy(Source, Dest, False) then
          RaiseException(Format('Cannot copy "%s"', [Source]));
      end;
    until not FindNext(FindRec);
  finally
    FindClose(FindRec);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  // Тихая установка: страницы «готово» нет, поэтому качаем напрямую. Ошибку не
  // глушим — пусть установка падает с внятным текстом.
  if (CurStep = ssInstall) and not PayloadReady then
  begin
    try
      DownloadTemporaryFile('{#PayloadUrl}', '{#PayloadName}', '', nil);
      PayloadReady := True;
    except
      RaiseException(FmtMessage(CustomMessage('PayloadDownloadFailed'), [GetExceptionMessage]));
    end;
  end;

  // Распаковка — после [InstallDelete], который чистит прежние bin/binaries.
  // Архив разворачиваем во временную папку (внутри него лежит unblock/), а
  // затем переносим содержимое в {app}: целиться в родителя {app} нельзя —
  // имя папки установки произвольное, и файлы уехали бы рядом с ней.
  if CurStep = ssPostInstall then
  begin
    try
      ExtractArchive(
        ExpandConstant('{tmp}\{#PayloadName}'),
        ExpandConstant('{tmp}\payload'),
        '',
        True,
        @OnPayloadExtract
      );
      CopyDir(ExpandConstant('{tmp}\payload\unblock'), ExpandConstant('{app}'));
    except
      RaiseException(FmtMessage(CustomMessage('PayloadUnpackFailed'), [GetExceptionMessage]));
    end;
  end;
end;

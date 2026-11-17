# Как внести вклад

Коротко: что нужно для сборки, как проверить правку и что прислать. Подробности — в [вики](https://github.com/MagilaWEB/unblock-youtube-discord/wiki), правила общения — в [кодексе поведения](CODE_OF_CONDUCT.md).

## Что нужно

- Windows 10/11 x64. Для сборки хватит обычных прав; для запуска приложения нужны администраторские — оно ставит службы, грузит драйвер WinDivert и меняет системный DNS.
- LLVM/clang, Ninja, CMake и PowerShell 7 (`pwsh`).
- Интернет: первый конфиг долгий, зависимости тянутся через FetchContent и собираются из исходников (saucer, curl+zlib, bit7z, zapret2, AdGuard DnsLibs, Catch2).

## Сборка и проверки

```powershell
scripts/configure-ninja-ai.bat             # чистый конфиг _build_ai пресетом debug
pwsh -NoProfile -File scripts/build-ai.ps1 # сборка
pwsh -NoProfile -File scripts/test-ai.ps1  # ctest в _build_ai
pwsh -NoProfile -File scripts/fmt-ai.ps1   # clang-format: изменённые файлы из git status
```

Пресеты: `debug` и `release`, плюс `*-min` — то же без тестов. Сборка одной цели — `scripts/build-ai.ps1 -Target <имя>`. Реконфиг чистит `bin/`, так что сразу после него тестовые exe отсутствуют, а `ctest` показывает `Not Run` — до первой сборки.

Ещё есть `scripts/tidy-ai.ps1` (clang-tidy) и `scripts/release-ai.ps1` (упаковка релиза) — порядок выпуска разобран в [вики](https://github.com/MagilaWEB/unblock-youtube-discord/wiki/Разработка-Релиз-и-версии). Конвенции для агентов и автоматизации лежат в `AGENTS.md`.

## Что где лежит

- `src/` — код по модулям: `engine`, `core`, `ui`, `unblock`, `helper`, `unblock_dns`, `dom`, `ext`. Исходники собираются глобом: новый `.cpp`/`.cppm` подхватится на сборке, `CMakeLists` править не надо.
- `resources/lua/` — Lua-скрипты обхода. В рантайме движок грузит копию из `binaries/lua/`, поэтому правка действует только после сборки.
- `configs/` — стратегии, фейк-блобы и списки хостов. Проверить, во что собрался конфиг, можно по `logs/strategy.args.txt`.
- `ui/` — интерфейс, `installer/` — Inno Setup, `docs/wiki/` — исходники вики (выливаются скриптом `scripts/sync-wiki-ai.ps1`).

## Бинарники в `binaries/`

Большую часть папки сборка наполняет сама — POST_BUILD-шагом цели `engine` из уже скачанного payload:

| Что | Откуда |
|---|---|
| `winws2.exe`, `cygwin1.dll`, `lua/*` | payload zapret2 (пин в `CMakeLists.txt`) |
| `winws.exe` | zapret-win-bundle (пин там же) |
| `WinDivert.dll`, `WinDivert64.sys` | payload zapret2; в git не лежат |
| фейк-блобы, названные в `configs/fake_bin.config` и `configs/fake_bin_zapret1.config` | `files/fake` payload zapret2 |

В git остаётся только то, чего в payload нет: фейк-блобы собственного производства плюс два бинарника, которые обновляются руками —

- `SvcHost.exe` — релиз [MagilaWEB/svc_host](https://github.com/MagilaWEB/svc_host);
- `tg-ws-proxy.exe` — консольная сборка [Flowseal/tg-ws-proxy](https://github.com/Flowseal/tg-ws-proxy), делается скриптом `scripts/build-tgws-ai.ps1`.

```powershell
pwsh -NoProfile -File scripts/build-tgws-ai.ps1                 # последний релиз апстрима
pwsh -NoProfile -File scripts/build-tgws-ai.ps1 -Tag vX.Y.Z    # или конкретный тег
```

Скрипт сам скачивает исходники релиза, собирает **консольный** onefile (апстрим-рецепт делает трей-приложение, негодное как payload службы), проверяет, что апстрим всё ещё принимает `--secret`, `--dc-ip`, `--host` и `--port` (их задаёт `src/unblock/tg_proxy.cpp`), смотрит PE-подсистему и только после этого кладёт exe в `binaries/`. В сборку проекта он не встроен — запускается руками.

Про два места, где легко ошибиться, скрипт знает сам, но помнить стоит: `--hidden-import=proxy` тянет только `__init__.py`, поэтому подмодуль входа перечислен отдельно (`proxy.tg_ws_proxy`) — иначе exe собирается, но падает на старте с `ModuleNotFoundError: No module named 'proxy.stats'`; а в окружении нужны `cryptography`, `httpx`, `certifi` (без `cacert.pem` в bundle TLS не работает) и `h2` (без него прокси не поднимается вовсе). Не хватает пакетов — скрипт скажет каких, поставить их можно ключом `-InstallDeps`.

## Стиль

Контракт по коду — `STYLE.md`, форматирование — `.clang-format` (табы, `ColumnLimit 150`, CRLF) и `scripts/fmt-ai.ps1`. Код, идентификаторы и комментарии — английский; git-сообщения — русский. Пользовательские строки живут только в `ui/text/<lang>.list`, ключи с префиксом `str_`.

## Коммиты

Заголовки — по Conventional Commits: `feat:`, `fix:`, `docs:`, `refactor:`, `chore:` и так далее; ломающее изменение помечается `!` или `BREAKING CHANGE:`. Версия проекта считается из них автоматически (`cmake/GetUnblockVersion.cmake`) — тег ставить не нужно и `src/engine/version.hpp` руками не правим, он генерируется на сборке.

## Чего не коммитить

`bin/`, `binaries/` (`*.lua`, `winws*.exe`, `cygwin1.dll`), `logs/`, `user/`, `_build*`, `.codegraph/` и `src/engine/version.hpp` — всё это в `.gitignore`, но редакторы иногда пихают такое силой.

## Pull request

Заполни шаблон, который подставится сам: CI прогонит Debug и Release с `ctest`. Мелкие правки можно присылать сразу, крупные — новая функция, смена архитектуры, другое поведение по умолчанию — сначала обсуди в [discussions](https://github.com/MagilaWEB/unblock-youtube-discord/discussions), чтобы не переделывать.

## Лицензия

Проект под MIT: присылая правку, вы соглашаетесь, что она распространяется под той же лицензией. Уязвимости — не в issue, а по [политике безопасности](SECURITY.md).

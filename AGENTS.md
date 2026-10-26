## Build

- Конфигурация: `configure-ninja-ai.bat` (использует `CMakePresets.json`, пресет `debug`)
- Бинарники в `bin/`, деп-зависимости через FetchContent (saucer, curl+zlib из исходников, bit7z, 7za, zapret2) — первый конфиг долгий
- Сборка без тестов: пресеты `debug-min`/`release-min` (BUILD_TESTS=OFF)
- Скрипты-обёртки: `build-ai.ps1` (сборка), `test-ai.ps1` (ctest), `fmt-ai.ps1` (clang-format) — slash-команды `/build`, `/test`, `/fmt`

## Modules

- Проект целиком на C++20-модулях: интерфейсы — `.cppm` (`export module <имя>;`), реализации — `.cpp` (implementation units, `module <имя>;` + `import`), заголовков и PCH нет. Имя модуля — каталог плюс имя файла (`core.debug`, `ui.ui_zapret_page`); у `dom`/`ui`/`unblock` есть умбреллы (`export import`), так что `import ui;` даёт всё, что давал `ui.h`
- Стандартная библиотека — `import std;`. Модуль собирает таргет `ub_std` из MSVC-шного `std.ixx`: CMake свой `import std` для clang умеет только с libc++/libstdc++, поэтому std.ixx объявлен обычным CXX_MODULES-файлом, гейт `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD` не нужен, а ABI остаётся MSVC. Патч — `cmake/MsvcStdModule.cmake`: `<vcruntime_new.h>` в глобальном фрагменте (llvm/llvm-project#214354: clang неявно объявляет `std::align_val_t`) и возврат `_VCRT_EXPORT_STD` в `export` (иначе `std::exception`/`std::bad_alloc` из модуля не видны). Файл обновляется при смене тулсета — путь ищется по списку инклудов компилятора
- Сторонние библиотеки — обёртки `src/ext/` (`ext.saucer`, `ext.coco`, `ext.curl`, `ext.bit7z`): заголовок в глобальном фрагменте плюс ручной реэкспорт имён. Список имён там же — это и есть поверхность, которую надо проверять при обновлении зависимости (у curl `CURL_GLOBAL_ALL` и `CURL_SSLVERSION_MAX_DEFAULT` — макросы, они стали константами с теми же именами)
- Текстуальными остаются только те заголовки, где иначе нельзя: `core/macros.hpp` и `ui/ui_macros.hpp` (макросы через границу модуля не проходят вообще — ни к импортёру, ни к своей же implementation-единице), `engine/version.hpp` (генерируется на сборке и нужен `engine.rc`), `unblock_dns/ag_dns.h` + `ag_bind.h` (C-ABI AdguardDns64.dll) и `tests/test_forwards.h`. Win32 и UCRT-расширения (`_popen`, `freopen_s`, `system`) — текстуально в глобальном фрагменте
- `import std` не отдаёт глобальные C-имена: фиксированные типажи нужны как `std::uint32_t`, глобальный `FILE` → `std::FILE`, а UCRT-расширения берутся из `<stdio.h>`/`<stdlib.h>`
- Форвард-декларация и `friend` на сущность из другого модуля не работают: `class X;` в модуле A и определение в модуле B — разные типы (clang: `declaration attached to named module ... cannot be attached to other modules`). Нужен `import`, а для тестов — объявление харнесса в глобальном модуле (см. `## Tests`)
- `VERSION_*` в C++ — константы модуля `engine.version` (таргет `ub_version`); макросом остаётся только `VERSION_NUMBER`, его читает `engine.rc` как список через запятую

## Tests

- Тесты каждого модуля — в `src/<module>/tests/`, свой exe (`test_helper`, `test_*` из core)
- curl/zlib собираются статически из исходников (Schannel, HTTP-only) — runtime DLL для `test_helper`/`zapret_helper` не нужны, PATH подмешивать не требуется
- `test_helper` линкует `core` (общий `tap_main.cpp` тянет конструкторы `File`/`CriticalSection`)
- Тесты линкуют библиотеку модуля, а не компилируют её исходники: файлсет модулей принадлежит одному таргету (`helper` — статическая библиотека, `zapret_helper.exe` — это только `main.cpp`; `test_unblock` линкует `unblock`). Catch2 подключается текстуально до импортов
- Доступ теста к приватным членам — `friend`-класс под `#ifdef HELPER_TESTS` (макрос задаётся только при BUILD_TESTS). Класс-харнесс объявляется в `src/<module>/tests/test_forwards.h`, который включается в глобальном фрагменте тестируемого модуля (под тем же `#ifdef`) и в тесте до импортов: иначе friend и определение класса — разные сущности

## Version

- Версия выводится из git (`cmake/GetUnblockVersion.cmake`, Conventional Commits + semantic-release: `fix:` → PATCH, `feat:` → MINOR, `!`/`BREAKING CHANGE:` → MAJOR; без них — значимость диффа ≥20 строк по `src/`+`cmake/` → PATCH, иначе релиза нет и версия равна базовому тегу). Руками не править никогда!
- `src/engine/version.hpp` — генерируется из `version.hpp.in` (git-ignored) на каждом билде целью `unblock_version` (`cmake/WriteVersionHeader.cmake`): перед пересчётом best-effort `git fetch --tags`, файл перезаписывается только при смене версии. В C++ значения приходят константами из модуля `engine.version` (`src/engine/version.cppm`, таргет `ub_version`): `VERSION_STR` (+`VERSION_FULL`/`BUMP`/`RELEASE`/`DISTANCE`/`DIRTY`/`HASH`/`BASE_TAG`). Макросом остаётся только `VERSION_NUMBER` — его читает `engine.rc` как список через запятую
- Вычисленная версия — в логе билда (`WriteVersionHeader: ...`) и в логе configure (`UNBLOCK_VERSION = ...`, там же `bump` и `release`). Релизный тег `v<триплет>` ставится вручную ПОСЛЕ сборки только при `release=1` и обязан равняться вычисленной версии (`VERSION_STR`)
- Ручные рычаги: `build-ai.ps1 -VersionOverride X.Y.Z` (тест обновлений со старой версией, тег НЕ ставить!) и `-DUNBLOCK_VERSION_BUMP_FORCE=major|minor|patch|none` (принудительный бамп от текущей базы)
- Версия всегда актуальна на билде: реконфиг после новых коммитов/тегов не нужен, теги подтягиваются сами. Фетч best-effort — оффлайн берутся локальные теги, так что свежий тег всё равно должен быть получен (`git fetch --tags`/`pull`)

## Architecture

- `src/engine/` — точка входа `engine.exe`, копирует lua/blobs в `binaries/` при сборке
- `src/ext/`, таргеты `ub_std` и `ub_version` — инфраструктура модулей (обёртки сторонних библиотек, std-модуль, версия); конвенции — в `## Modules`
- `src/core/` — база: File, Debug, Localization, Service, utils
- `src/ui/` — интерфейс на Ultralight
- `src/unblock/` — логика: Unblock, StrategiesDPI (генерация стратегий), DomainTesting (curl-проверка), DNSHost, IPCSignals (UDP 9999, Lua→C++)
- `src/helper/` — `zapret_helper.exe`: UDP-сервер (порт 10000), принимает LIST:/CHECK:/CONFIG:/VALID:/ERR:/EXHAUSTED:/READY:, гонит проверку хостов через curl (насос трафика), состоянием мёртвых/valid/exhausted/unjudged владеет по вердиктам Lua и релеит его в unblock снапшотами на 9999
  - до первого `READY` (пульс zapret из Lua раз в 5 c) helper спит: воркеры не подняты, пробы и снапшоты не идут; после минуты тишины работа по хостам приостанавливается
- Lua-скрипты: исходники в `resources/lua/`, в рантайме загружаются из `binaries/lua/` (копируются при сборке). Правки Lua применяются только после сборки!
- `winws2.exe` (zapret2) запускается как служба с аргументами из `_normalizeStrategyFinal()` (номера `strategy=N`), профиль zcheck и `--filter-udp=10000` больше не несёт, а в конфигах убраны `--wf-filter-loopback=0` и порт 10000 из списков `--wf-udp-in/out`
  - helper больше не присылает вердикты OK/FAIL и не судит работоспособность: по команде `CHECK:` он гонит трафик (насос), владеет состоянием мёртвых/exhausted/unjudged хостов и релеит его в unblock по UDP 9999 (unjudged — проба прошла, но вердикта от Lua нет)
  - Lua (`auto_strategy`) — единственный судья по пакетным уликам: ретрансмиссия, RST, DPI16KB (payload>=16000), DPI-redirect, throttle-after-handshake; на провале при наличии hostname Lua просит helper сделать `CHECK:` (насос), а положительный вердикт VALID рождается из throttle

## Style

- Личные правила общения и работы — `~/.config/opencode/rules/persona.md` и `delegation.md` (глобальные, подключены через `instructions`).

## MCP / Navigation (opencode)

- Навигация по C++: не выдумывай API — сигнатуры, типы и связи проверяй по коду (`rg`/`grep`/`read`); сомнительные места подтверждай сборкой. В opencode2 LSP (clangd) не работает — диагностика только компилятором (`build-ai.ps1`) и `clang-tidy` (`tidy-ai.ps1`, модульные флаги он берёт из modmap в `compile_commands.json`, `.cppm` тоже проверяет).
- Примеры извне: `gh_grep`, доки по либам: `context7`.
- После правки: `pwsh -NoProfile -File build-ai.ps1`, потом `ctest --test-dir _build_ai --output-on-failure`. Реконфиг чистит `bin/` (так задумано в `src/engine/CMakeLists.txt`), так что сразу после него тестовые exe отсутствуют, а `ctest` показывает `Not Run` — до первой сборки. Lua правится в `resources/lua/` + сборка (копия в `binaries/lua/`).
- Не индексируй мусор: `_build_ai/`, `_build/`, `_deps/`, `bin/`.
## Build

- Конфигурация: `configure-ninja-ai.bat` (использует `CMakePresets.json`, пресет `debug`)
- Бинарники в `bin/`, деп-зависимости через FetchContent (saucer, curl+zlib из исходников, bit7z, 7za, zapret2) — первый конфиг долгий
- Сборка без тестов: пресеты `debug-min`/`release-min` (BUILD_TESTS=OFF)
- Скрипты-обёртки: `build-ai.ps1` (сборка), `test-ai.ps1` (ctest), `fmt-ai.ps1` (clang-format) — slash-команды `/build`, `/test`, `/fmt`

## Tests

- Тесты каждого модуля — в `src/<module>/tests/`, свой exe (`test_helper`, `test_*` из core)
- curl/zlib собираются статически из исходников (Schannel, HTTP-only) — runtime DLL для `test_helper`/`zapret_helper` не нужны, PATH подмешивать не требуется
- `test_helper` линкует `core` (общий `tap_main.cpp` тянет конструкторы `File`/`CriticalSection`)
- Для доступа теста к приватным членам используется `friend`-класс, обёрнутый в `#ifdef HELPER_TESTS` (макрос задаётся только при BUILD_TESTS)

## Version

- Версия выводится из git (`cmake/GetUnblockVersion.cmake`, Conventional Commits + semantic-release: `fix:` → PATCH, `feat:` → MINOR, `!`/`BREAKING CHANGE:` → MAJOR; без них — значимость диффа ≥20 строк по `src/`+`cmake/` → PATCH, иначе релиза нет и версия равна базовому тегу). Руками не править никогда!
- `src/engine/version.hpp` — генерируется из `version.hpp.in` (git-ignored) на каждом билде целью `unblock_version` (`cmake/WriteVersionHeader.cmake`): перед пересчётом best-effort `git fetch --tags`, файл перезаписывается только при смене версии. Макросы `VERSION_STR`/`VERSION_NUMBER` (+`VERSION_FULL`/`BUMP`/`RELEASE`/`DISTANCE`/`DIRTY`/`HASH`/`BASE_TAG`)
- Вычисленная версия — в логе билда (`WriteVersionHeader: ...`) и в логе configure (`UNBLOCK_VERSION = ...`, там же `bump` и `release`). Релизный тег `v<триплет>` ставится вручную ПОСЛЕ сборки только при `release=1` и обязан равняться вычисленной версии (`VERSION_STR`)
- Ручные рычаги: `build-ai.ps1 -VersionOverride X.Y.Z` (тест обновлений со старой версией, тег НЕ ставить!) и `-DUNBLOCK_VERSION_BUMP_FORCE=major|minor|patch|none` (принудительный бамп от текущей базы)
- Версия всегда актуальна на билде: реконфиг после новых коммитов/тегов не нужен, теги подтягиваются сами. Фетч best-effort — оффлайн берутся локальные теги, так что свежий тег всё равно должен быть получен (`git fetch --tags`/`pull`)

## Architecture

- `src/engine/` — точка входа `engine.exe`, копирует lua/blobs в `binaries/` при сборке
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

- Личные правила общения и работы — см. глобальный скил `dev-communication`.

## MCP / Navigation (opencode)

- Навигация по C++: не выдумывай API — сигнатуры, типы и связи проверяй по коду (`rg`/`grep`/`read`) и через LSP-диагностику (clangd); сомнительные места подтверждай сборкой.
- Примеры извне: `gh_grep`, доки по либам: `context7`.
- После правки: `pwsh -NoProfile -File build-ai.ps1`, потом `ctest --test-dir _build_ai --output-on-failure`. Lua правится в `resources/lua/` + сборка (копия в `binaries/lua/`).
- Не индексируй мусор: `_build_ai/`, `_build/`, `_deps/`, `bin/`.
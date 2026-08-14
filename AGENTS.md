
## Build

- Использовать `--build _build_ai` вместо `_build`
- Использовать `Ninja` вместо `VS`
- Конфигурация: `configure-ninja-ai.bat` (использует `CMakePresets.json`, пресет `debug`)
- Компилятор: clang/clang++ (LLVM), линковка через lld-link
- Бинарники выходят в `bin/`, деп-зависимости тянутся через FetchContent (ultralight, curl, bit7z, 7za, zapret2) — первый конфиг долгий
- ВАЖНО: перед `cmake`/`ninja` очистить `INCLUDE`/`LIB`/`LIBPATH` из окружения. Если они указывают на x86-пути VS, clang падает с `undefined symbol: mainCRTStartup`
- Сборка без тестов: пресеты `debug-min`/`release-min` (BUILD_TESTS=OFF)

## Tests

- Catch2 (v3), репортер TAP; регистрируются через `add_test(... --reporter TAP)`
- Тесты каждого модуля — в `src/<module>/tests/`, свой exe (`test_helper`, `test_*` из core)
- Запуск одного теста: `ctest --test-dir _build_ai -R <name> --output-on-failure`
- ВАЖНО: Debug-сборка `test_helper.exe` требует `libcurl-d.dll` из `_build_ai/curl/debug/bin/` — добавить в `PATH` перед запуском
- Для доступа теста к приватным членам используется `friend`-класс, обёрнутый в `#ifdef HELPER_TESTS` (макрос задаётся только при BUILD_TESTS)

## Architecture

- `src/engine/` — точка входа `engine.exe`, копирует lua/blobs в `binaries/` при сборке
- `src/core/` — база: File, Debug, Localization, Service, utils
- `src/ui/` — интерфейс на Ultralight
- `src/unblock/` — логика: Unblock, StrategiesDPI (генерация стратегий), DomainTesting (curl-проверка), DNSHost, IPCSignals (UDP 9999, Lua→C++)
- `src/helper/` — `zapret_helper.exe`: UDP-сервер (порт 10000), принимает LIST:/CHECK:/STAT:/ERR:, батчит проверку хостов через curl, отвечает OK:/FAIL:, шлёт логи на 9999
- Lua-скрипты: исходники в `resources/lua/`, в рантайме загружаются из `binaries/lua/` (копируются при сборке). Правки Lua применяются только после сборки!
- `winws2.exe` (zapret2) запускается как служба с аргументами из `_normalizeStrategyFinal()` (добавляет `--lua-desync=zcheck`, `--filter-udp=10000` и номера `strategy=N`)

## Style

- AGENTS.md запрещено менять.
- Общайся на Русском.
- Никогда не коммитить и не пушить, разрешения всегда пришивать.
- Коммиты на Русском, описывай их подробно.
- Я МогилныйПатчер или альтернативные имена (Могила, могилыч, маджила), называй меня на выбор.
- Ругать меня если что-то делаешь не так (можно с матами и оскорблениями но любя).
- Ругать себя если что-то делаешь не так (можно с матами и оскорблениями но любя).
- Если есть проблемный говнокод не связанный с проектом (сторонние библиотеки) абассать автора.

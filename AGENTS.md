## Build

- Конфигурация: `configure-ninja-ai.bat` (использует `CMakePresets.json`, пресет `debug`)
- Бинарники в `bin/`, деп-зависимости через FetchContent (ultralight, curl, bit7z, 7za, zapret2) — первый конфиг долгий
- Сборка без тестов: пресеты `debug-min`/`release-min` (BUILD_TESTS=OFF)
- Скрипты-обёртки: `build-ai.ps1` (сборка), `test-ai.ps1` (ctest), `fmt-ai.ps1` (clang-format) — slash-команды `/build`, `/test`, `/fmt`

## Tests

- Тесты каждого модуля — в `src/<module>/tests/`, свой exe (`test_helper`, `test_*` из core)
- ВАЖНО: Debug-сборка `test_helper.exe` требует `libcurl-d.dll` из `_build_ai/curl/debug/bin/` — добавить в `PATH` перед запуском (делает `test-ai.ps1`)
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

- Личные правила общения и работы — см. глобальный скил `dev-communication`.
# Стиль кода

Контракт проекта unblock. Источник истины по форматированию — `.clang-format`; здесь описано то, что форматтер не выражает. Документ задаёт **целевое** состояние: часть старого кода ещё расходится с ним, расхождения вычищаются поэтапно.

## 1. Язык

- Код, идентификаторы и комментарии — английский. Исключений нет.
- Комментарий описывает назначение и контракт, а не историю правок: без ссылок на коммиты, «раньше было так», «в этой сессии», «ранее вынесли».
- Git-сообщения — русский.
- Пользовательские строки — только в `ui/text/<lang>.list`, ключи с префиксом `str_`.
- Эмодзи в коде (в т.ч. в строках логов и стектрейсов) не используются.

## 2. Форматирование

- `.clang-format`: база Microsoft, табы (`TabWidth`/`IndentWidth` 4), `ColumnLimit 150`, CRLF, `SortIncludes: Never`.
- Прогон: `pwsh -NoProfile -File fmt-ai.ps1`.
- Интерфейс — модуль `.cppm` (`export module <имя>;`), реализация — `.cpp` (implementation unit: `module <имя>;` и `import` вместо `#include`). Заголовки и PCH не используются, стандарт приходит через `import std;`.
- Имя модуля — каталог плюс имя файла: `core.debug`, `ui.ui_zapret_page`. У `dom`/`ui`/`unblock` есть умбреллы (`export import`) — `import ui;` даёт то же, что раньше давал `ui.h`.
- Системные и сторонние заголовки — только в глобальном фрагменте модуля (`module;` → `#include` → `export module ...;`): макросы и C-ABI через границу модуля не проходят. Сторонние библиотеки подключаются обёртками из `src/ext/` (`import ext.curl;`).
- Макросы проекта (ASSERT/VERIFY/LIMIT_UPDATE, BUTTON/CHECK_BOX/SECONDARY_WINDOW и прочие) живут в текстовых `core/macros.hpp` и `ui/ui_macros.hpp` — их включают в глобальном фрагменте те юниты, где макросы нужны.
- `#pragma once` в модулях не нужен; `using namespace` в интерфейсе модуля запрещён так же, как был запрещён в заголовке.
- Объявления и inline-логика — только в интерфейсных юнитах `.cppm`; реализация — в `.cpp`.
- Форвард-декларация и `friend` на сущность из другого модуля не работают: `class X;` в модуле A и определение в модуле B — разные типы. Вместо форварда — `import`.

## 3. Имена

Всё делится на **поля** и **методы**. Поля — всегда `snake_case` с префиксом по виду поля. Методы — `camelCase` (у приватных ведущее `_`).

| Сущность | Стиль | Пример |
|---|---|---|
| Тип (`class`/`struct`/`enum`/`using`/`concept`) | PascalCase | `Unblock`, `HelperStats` |
| Метод, свободная функция | camelCase | `startService`, `isUtf8` |
| Приватный/защищённый метод | `_camelCase` | `_dnsProxyWriteConfig`, `_getPrefix` |
| Поле (любое) | `snake_case` | `_string_list`, `str_id` |
| Приватное/защищённое поле | `_snake_case` | `_helper_state_lock`, `_command_line` |
| Публичное поле | `snake_case` | `str_id` |
| Константа (`constexpr`/`static const`) | `c_snake_case` | `c_helper_signal_ttl`, `c_min_workers` |
| Изменяемый статик | `s_snake_case` | `s_error_fatal`, `s_next_handle` |
| Локальная переменная, параметр | `snake_case` | `name_service` |
| Макрос | `UPPER_SNAKE` | `LIMIT_UPDATE`, `CHECK_BOX` |
| Значение `enum class` | PascalCase | `Technology::Zapret1` |
| Базовые алиасы | `u8/u16/u32/u64`, `s8…s64`, `pstr/pcstr/cpcstr` | `u32 count` |
| Пространство имён | lowercase | `utils`, `ui::dom` |
| Файл | snake_case | `ui_zapret_page.cpp` |

Правила:

- Перечисления — только `enum class`, по возможности с явным underlying-типом (`enum class Type : u8`). Значения — PascalCase. Plain `enum` и `e`-префиксы не используем.
- Новые целочисленные типы — через алиасы из `core.types`, а не `int`/`unsigned`.
- Публичные методы и поля не носят ведущего подчёркивания (`readLogTail`, а не `_readLogTail`).

## 4. C++23

Целевой стандарт — ISO C++23 (`CMAKE_CXX_EXTENSIONS OFF`). Максимально использовать стандарт вместо ручных циклов и C-приёмов:

- Обходы: `std::ranges::find/find_if/any_of/all_of/contains`, `std::views::split/enumerate/join_with/transform`, `std::ranges::to` вместо индексных циклов и ручной сборки строк.
- Строки: `.contains()`, `.starts_with()`, `.ends_with()` вместо `find(...) != npos` и `compare(...) == 0`.
- `std::string_view` и `std::span` для аргументов-представлений; прозрачные компараторы (`std::less<>`) для lookup по `string_view`.
- `std::to_underlying`, `std::bit_cast`, designated initializers, `constinit`/`consteval` — там, где это упрощает и не вредит.
- Формат строк — `std::format`.
- Разбор чисел — `std::from_chars`/`std::to_chars`, не `stringstream` без нужды.

## 5. Логирование и ошибки

- Только через `Debug` (`Debug::ok/warning/error/info`, `Debug::str_*`). Прямые `printf`/`std::cout` в коде приложения не используем.
- Запуск внешних процессов — через существующие обёртки (`_runHidden` и подобные) с `CREATE_NO_WINDOW`; `system()` запрещён.
- Проверки-ассерты — `ASSERT_ARGS`/`Debug`-механизм, а не голый `assert`.

## 6. Потоки и время жизни

- Никаких изменяемых функциональных `static`-ов. Общее состояние — либо под мьютексом, либо у единственного владельца-потока.
- Для разделяемого владения — `std::shared_ptr` под мьютексом, для счётчиков из разных потоков — `std::atomic`.
- UI-поток: блокирующие геттеры DOM (`rect`, `offsetSize`, `getAttr`, `valueStr`, `isChecked`, `hasClass`) нельзя звать с воркера; сеттеры (`setText`, `addClass`, `show`, `hide`) — fire-and-forget и допустимы с воркера.
- Долгие операции (службы, сеть, сканы SCM) — не на потоке UI.

## 7. Тесты

- Catch2 v3, репортер TAP, свой exe на модуль: `src/<module>/tests/`. Файлсет модулей живёт в одном таргете, поэтому тесты линкуют библиотеку модуля, а не компилируют его исходники (`helper` — статическая библиотека, `zapret_helper.exe` — это `main.cpp`; `test_unblock` линкует `unblock`). Catch2 подключается текстуально до импортов.
- Доступ к приватным членам — friend-харнесс под `#ifdef HELPER_TESTS`/`UNBLOCK_TESTS` (макрос задаётся только при `BUILD_TESTS`). Класс-харнесс объявляется в `src/<module>/tests/test_forwards.h`: этот заголовок включается в глобальном фрагменте тестируемого модуля (под тем же `#ifdef`) и в тесте до импортов — иначе friend и определение класса окажутся разными сущностями.
- При переименовании/изменении API тесты правятся тем же коммитом.

## 8. Сборка и проверки

- Сборка: `pwsh -NoProfile -File build-ai.ps1`. При OOM clang: очистить `$env:INCLUDE/$env:LIB/$env:LIBPATH` и `cmake --build _build_ai -j 2`.
- Тесты: `ctest --test-dir _build_ai --output-on-failure`. Реконфиг чистит `bin/` (так задумано в `src/engine/CMakeLists.txt`), поэтому сразу после него тестовые exe отсутствуют и `ctest` показывает `Not Run` — до первой сборки.
- Формат: `pwsh -NoProfile -File fmt-ai.ps1`.
- Версия выводится из git автоматически (`cmake/GetUnblockVersion.cmake`); вручную не правится.

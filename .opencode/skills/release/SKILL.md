---
name: release
description: Выпуск релиза unblock — сборка, тег, публикация GitHub-релиза и анонса-дискуссии в «Анонсах». Использовать, когда просят собрать, выпустить или опубликовать релиз или новую версию.
---

# release (unblock)

Пошаговый выпуск релиза. Команды — PowerShell из корня репозитория. Общие
правила версии/тега — в `AGENTS.md` (разделы Build/Version), здесь порядок
действий и то, что легко забыть.

Публикация релиза необратима: перед шагом 5 показать версию и текст релиза,
публиковать по подтверждению.

## 0. Проверки перед стартом

- Дерево чистое и `main` = `origin/main` (коммиты запушены): `git status -sb`.
- Если `git status` показывает всё дерево как новые файлы, а `git rev-parse HEAD`
  падает «failed to resolve HEAD» — это битая ссылка на ветку: ref содержит 40
  нулей и `git update-ref` по нему не сработает. Последний коммит ищем в
  `.git/logs/HEAD`, затем `Remove-Item .git/refs/heads/main` и
  `git update-ref refs/heads/main <hash>`.

## 1. Версия

Только из git, руками не править. Смотреть строку `UNBLOCK_VERSION = ...` в
логе сборки:

- `fix:` → PATCH, `feat:` → MINOR, `!`/`BREAKING CHANGE:` → MAJOR; без
  префиксов — значимость диффа ≥20 строк по `src/`+`cmake/` → PATCH.
- Тег — `v<триплет>`, и он обязан совпасть с триплетом: именно триплет лежит в
  `VERSION_STR` (он уходит в `ProductVersion`/`FileVersion`), а хвост
  `+N.gHASH` живёт только в `VERSION_FULL`.

## 2. Сборка и упаковка

    pwsh -NoProfile -File release-ai.ps1

Скрипт собирает пресет `release-min`, кладёт payload в `release/unblock.7z` и
собирает онлайн-установщик `release/unblock-setup.exe` (Inno Setup, ищет
`ISCC.exe`; без него просто пропустит установщик с сообщением). Откажется
паковать, если в `bin/` есть `*.pdb` — значит там debug-сборка.

Проверка номера: `(Get-Item bin/engine.exe).VersionInfo.ProductVersion` должен
быть ровно триплет.

## 3. Тег

    git tag vX.Y.Z
    git push origin vX.Y.Z

Тег легковесный (как `v1.9.0`/`v1.9.1`/`v1.8.0`). Ставится после сборки;
пересобирать после тега не нужно — `ProductVersion` берёт `VERSION_STR`, а он
уже триплет.

## 4. Текст релиза

Файл в UTF-8, стиль прошлых релизов — шаблон `references/release-notes.md`.
Коротко: заголовок `# ...`, вступление, разделы через `---` с эмодзи
(`## 🆕 Новое`, `## 🛠 Исправления`), пункты списка с жирным лидом, в конце
`---`, благодарность и блок `<details><summary>Служебное</summary>` со ссылкой
`[Исходники vX.Y.Z](https://github.com/MagilaWEB/unblock-youtube-discord/tree/vX.Y.Z)`.

В текст идёт только видимое пользователю; внутренние рефакторинги не тащим.

## 5. Публикация

    Remove-Item Env:GITHUB_TOKEN -ErrorAction SilentlyContinue
    gh release create vX.Y.Z --title "Unblock X.Y.Z" --notes-file <файл> --verify-tag --latest --discussion-category "Анонсы" release/unblock.7z release/unblock-setup.exe

- `--discussion-category "Анонсы"` создаёт анонс-дискуссию, привязанную к
  релизу: заголовок = имя релиза, в теле текст релиза и автофутер «created from
  the release». Это и есть «дискуссия к релизу».
- Дискуссия создаётся только вместе с релизом. Если релиз уже опубликован без
  неё — `gh release delete vX.Y.Z --yes` (тег не удаляет) и создать заново.
- `GITHUB_TOKEN` из окружения может не иметь права на релизы (403 `Resource not
  accessible by personal access token`); снимаем переменную, тогда `gh` берёт
  токен из keyring (scope `repo`). Комментарии/ассеты он тянет нормально.

## 6. Проверка

- Релиз: не draft, не prerelease, помечен latest; ассеты `unblock.7z` и
  `unblock-setup.exe` в состоянии `uploaded`.
- Дискуссия: категория «Анонсы», заголовок `Unblock X.Y.Z`, тело с текстом
  релиза и футером про release.
- `unblock.7z` обязателен и лежит под этим именем: апдейтер в приложении тянет
  `releases/latest/download/unblock.7z`.

---
description: Форматирование через clang-format (fmt-ai.ps1)
agent: build
---

Запусти из корня проекта: `pwsh -NoProfile -File fmt-ai.ps1 $ARGUMENTS`.

По умолчанию форматирует изменённые файлы из `git status`. С флагом `-All` — весь код проекта.
---
description: Сборка проекта (scripts/build-ai.ps1)
agent: build
---

Запусти из корня проекта: `pwsh -NoProfile -File scripts/build-ai.ps1 $ARGUMENTS`.

Дождись завершения. Если сборка упала — найди первый реальный `error` в логе (не тони в ворнингах), кратко объясни причину и предложи фикс.
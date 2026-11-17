# Стратегии — справочник разработчика

Практический справочник по конфигам `configs/strategy/<поколение>/` и инфраструктуре
обхода DPI на zapret2. Предназначен для тех, кто правит стратегии или отлаживает
обход.

## Как конфиг превращается в аргументы winws2

1. Движок читает `configs/strategy/<поколение>/<файл>.config` (`StrategiesDPI`).
2. `%TEMPLATES_<ИМЯ>%` заменяются строками одноимённого блока `[TEMPLATES:<ИМЯ>]` из
   `configs/templates.config` — это делается до скана секций, так что секции,
   `%BLOCKLIST%` и `use_in>>` внутри блока работают как написанные на месте. Блоки
   могут ссылаться друг на друга.
3. `%BLOCKLIST%`, `%DOMAINS-EXCLUDE%`, `%IP-EXCLUDE%`, `%IP-SETLIST%` разворачиваются
   в `--hostlist`, `--hostlist-exclude`, `--ipset-exclude`, `--ipset` из `user/`.
4. `%INIT_LUA%` → `--lua-init` для всех файлов из `binaries/lua/`;
   `%INIT_BLOB%` → `--blob=<ИМЯ>:<путь>` для блобов, реально используемых
   в стратегиях (маппинг имён — `configs/fake_bin.config`).
5. `--template=ИМЯ` объявляет шаблон плана, `--import=ИМЯ` подставляет его строки
   в текущий план. Каждая `--lua-desync=...:strategy=N` получает номер: нумерация
   сбрасывается по `--new` и по границам секций, а номера внутри плана должны идти
   монотонно и без дырок — иначе ротация `auto_strategy` наступает на пустые номера.
6. Секции `[имя]` — фильтры по портам/l7/hostlist; `use_in>>цель` включает одну
   секцию в другую.

## Из чего собран план

Общий каркас лежит в `configs/templates.config` (блоки `[TEMPLATES:<ИМЯ>]`), а в самом
конфиге стратегии остаётся то, что отличается от набора к набору. Актуальный набор
берёт из шаблонов `HEAD` и `TAIL`, а `START_QUIC`/`START_TLS` со списками стратегий
объявляет сам; часть старых наборов собирает планы вручную и дополнительно подключает
`DISCORD_MEDIA` и `YOUTUBE_TCP`.

| Блок | Назначение |
|---|---|
| `[TEMPLATES:HEAD]` | `[START]`: `--ctrack-disable=0`, WinDivert-фильтры портов (`[TEMPLATES:WF]`), `%INIT_LUA%`/`%INIT_BLOB%`, базовые блобы |
| `--template=START_QUIC` | UDP/443 QUIC-план: фейки, ipfrag, udplen + `auto_strategy` |
| `--template=START_TLS` | TCP 80/443 TLS-план: фейки, сплиты, disorder + `auto_strategy`. Сюда же ушла TCP-фаза голоса Discord |
| `--template=START_VOICE` | Голосовой UDP-план: `auto_strategy:fails=6:start=1` и две стратегии — фейк `VOICE_PRE` (в первой ещё `JUNK_00`/`JUNK_01`), затем `send:delay=50` и `drop` |
| `[TEMPLATES:DISCORD_UDP]` | Секция `[discord]`: `--filter-udp=%DiscordPortUPD%`, `--filter-l7=discord,stun,quic`; под неё идёт `--payload=all --import=START_VOICE` |
| `[TEMPLATES:DISCORD_MEDIA]` | TCP-фаза голоса: `--filter-tcp=%DiscordPort%`, `--hostlist-domains=discord.media` |
| `[TEMPLATES:YOUTUBE_TCP]`, `[TEMPLATES:END_TCP]` | Остальной TLS/HTTP: YouTube отдельной секцией, `[END]` — общим планом |
| `[TEMPLATES:GAME_TCP]`, `[TEMPLATES:GAME_UDP]` | Игры по ipset |

Отдельного `START_VOICE_TCP` нет, и голос Discord работает целиком: и в браузере,
и в десктопном приложении — UDP через `START_VOICE`, TCP-фаза через `START_TLS`.

## Порты Discord

Задаются в `configs/service_setting.config`: `DiscordPort:2053,2083,2087,2096,8443` (TCP)
и `DiscordPortUPD:19294-19344,50000-50100` (UDP — старый пул Cloudflare и новый пул
Google Cloud). От этих диапазонов зависят и WinDivert-фильтры в `[START]`, и секция
`[discord]`, так что при смене медиа-пула их надо синхронизировать.

## Авто-стратегия (`auto_strategy`)

Lua-оркестратор (`binaries/lua/unblock_auto_strategy.lua`):

- перебирает стратегии плана по счётчику FAIL (порог `fails=N`);
- `start=N` — начать с номера N (direct пропускается);
- `reset` — слать RST при переключении;
- `udp_out=N` — вердикт UDP после N исходящих без ответа;
- подтверждение (CONFIRMED) — сервер ответил (UDP) / helper OK (TCP);
- success-list хранит подтверждённые номера и применяется при переподключениях;
- план-отпечаток (`auto_plan_id`): записи success-list разделяются по числу
  стратегий в плане, чтобы чужие номера не применялись;
- записи UDP разделяются по классу протокола соединения (`proto_class`):
  STUN браузера и `discord_ip_discovery` десктопа — разные записи с разными
  вердиктами;
- голосовые хосты группируются в `discord.media` (все поддомены, формы
  с `c-` и без), googlevideo — в `googlevideo.com`.

## Что важно помнить при правке стратегий

- **conntrack обязателен**: `--ctrack-disable=0` — без него split-примитивы
  слепы (нет реконструкции потока). Плюс `--ipcache-lifetime=8400
  --ipcache-hostname=1` — кэш профилей по IP с hostname.
- **Серверный анти-replay**: батчи фейковых ClientHello (repeats=6–8) с чужим
  SNI вызывают TLS alert `handshake_failure` (02 28) от Cloudflare/Google.
  Лечится снижением repeats и `tls_mod=sni=%hostname` (SNI фейка = хост
  соединения).
- **Хост-специфичность**: универсальной стратегии нет. fakedsplit 681/726
  tcp_md5 живёт на одних хостах, multisplit 681 — на других, hostfakesplit — на
  третьих. Ротация нужна, но самые живые стратегии стоит держать в начале плана.
- **Голосовой UDP**: голому фейку киллер не верит — QUIC-фейки (любые repeats),
  ipfrag, udplen и подмена SSRC на STUN magic cookie на голосовом UDP не
  проходили. Работает последовательность из `START_VOICE`: фейк `VOICE_PRE` +
  `send:delay=50` + `drop`.
- **Деградация сети**: при активном переборе стратегий связь умирает постепенно
  и оживает через несколько минут после выключения zapret. При замерах делать
  паузы и не мерять результаты на подгаженной сети.
- **Правила DPI меняются внезапно**: рабочая стратегия может перестать пробивать
  за считанные дни. При «всё сломалось» сначала проверять, не сменились ли
  правила на стороне оператора, а не конфиг.

## Отладка

- `--debug` в `[START]` — пер-пакетный дамп в `binaries/winws2.exe.log`
  (растёт до сотен МБ; снимать после отладки).
- Полезные паттерны по логу: `CONFIRMED`, `FAIL UDP .* in=`, `change SNI`,
  `LUA ERROR`, `flags=SA|flags=AP|flags=R`. Счётчики `out=/in=` в FAIL UDP:
  `in=1` навсегда — потоковый киллер; входящие `TCP: len=7 : 15 03 ... 02 28` —
  серверный анти-replay; `flags=R` от CDN-адресов — RST-бомбардировка.
- Хелпер (`bin/zapret_helper.exe.log`): обычные хосты — curl-проверка,
  `*.discord.media` — WebSocket-upgrade к голосовому гейту (ping/pong ~6 c):
  вердикт отражает живость пути, а не только TLS-хендшейка.
- Тесты: `pwsh -NoProfile -File scripts/test-ai.ps1` (Catch2, TAP-репортер).

-- auto_strategy — адаптивный перебор стратегий для обхода DPI
-- Автор: MagilaWEB (МогилныйПатчер)
--
-- Принцип: helper (zapret_helper.exe) не выносит вердиктов. Он — насос и
-- исполнитель: Lua — единственный судья по пакетным уликам. На провале Lua
-- один раз на соединение просит helper сделать CHECK:<host> — свежее
-- соединение через текущую стратегию рождает новые пакеты. Положительный
-- вердикт (VALID) — только по ответу сервера, а не по helper OK.
--
-- ВАЖНО: профили обязаны отдавать этому инстансу весь трафик потока.
-- Внутрипрофильные фильтры zapret2 (--payload/--in-range/--out-range)
-- действуют на все последующие десинки, поэтому узкий --payload должен
-- стоять ПОСЛЕ auto_strategy (перед стратегиями). Иначе судья видит только
-- ClientHello, не может заметить троттлинг и никогда не шлёт VALID.
-- Монитору нужны --payload=all и --in-range=a; --out-range можно оставить
-- узким (поток байт сервера всё равно считается conntrack'ом), а стратегии —
-- на узком --payload.
--
-- Как работает:
--   1. Сначала пробуем прямое соединение (direct, nstrategy = 0). Если оно
--      ответило, хост остаётся на нём: номер 0 не пишется в рейтинг
--      (auto_rate_strategy), поэтому такой хост помнится флагом direct_ok и
--      держит direct во главе кандидатов. Иначе хост, которому обход не нужен,
--      после первого же сбоя уезжал на дез-стратегию и начинал подмешивать
--      фейковые пакеты в рабочее соединение. Провалившийся direct помечается
--      испробованным и возвращается только после полного круга. Потоковым
--      семействам (googlevideo) это не достаётся: у них выбором владеет замер
--      скорости, а удачное окно на direct доказывает только то, что шейпер
--      пропустил первый бурст.
--   2. TCP: вердикты по пакетам — ретрансмиссия до первых данных, RST в
--      начале рукопожатия и DPI-redirect (http_reply 302/307). Ретрансмиссия
--      судится только до ответа сервера (crec.handshake_ok): на живом
--      соединении это обычная потеря, а не перерезанный DPI путь — голосовые
--      и API-потоки до гейта в 4096 байт серверных данных не доходят никогда,
--      и каждый их сбой уводил стратегию из-под работающего соединения (а
--      звонок Discord после этого рвётся). Работающий TCP подтверждается
--      ответом сервера (п. 3), а троттлинг после рукопожатия ловит
--      throttle-судья (conntrack-счётчик байт сервера,
--      track.pos.server.pbcounter): потоковые хосты (googlevideo, fbcdn) судятся по
--      скорости и на TLS, и на QUIC (п. 5), а обычный TCP (API, long-poll) —
--      нет, он низкоскоростной по природе. Одно соединение докладывает
--      провал не чаще раза в 2 секунды, счётчик fails сбрасывается по времени
--      (time) или настоящим успехом.
--   3. Положительный вердикт — входящий tls_server_hello/http_reply плюс
--      прикладные данные после запроса (app_seen). Само рукопожатие ответом не
--      считается: сервер пожал руку и замолчал — соединение не подтверждается,
--      и серия провалов не обнуляется, иначе «открывшийся, но мёртвый» сайт
--      навсегда закреплял бы стратегию. Для потоковых хостов (googlevideo) и
--      QUIC рукопожатия тем более мало: успех ставит только здоровое окно
--      скорости.
--   3b. Рейтинг стратегий (в пределах плана и прогона winws2) — затухающий
--      скор: подтверждение даёт +1, провал — минус, и скор лениво тает по
--      времени (полураспад ~10 минут). Свежесть важнее стажа: стратегия,
--      которая постепенно перестала работать, сползает сама, а не сидит на
--      пьедестале вечно. Охват (сколько разных хостов подтвердили) и замер
--      скорости тоже учитываются только свежие (TTL); скорость копит EWMA по
--      замерам потоковых хостов, поэтому обычные, ничего не измеряющие хосты
--      берут данные видео. Кандидаты — одна лестница: скор убыв., при равенстве
--      скорость (у потоковых наоборот, скорость первична), ничьи — по номеру.
--      Никогда не подтверждавшиеся начинают с нуля и стоят внизу, так что
--      «вниз по рейтингу вплоть до ни разу не успешных» выходит само. Провал
--      на одном хосте штрафует план только когда провалились 2 разных хоста —
--      одиночный кривой SNI не роняет стратегию всем.
--      Работающая стратегия — не финал: у потоковых свежий замер скорости выше
--      текущего в 1.3 раза уводит запись на него (auto_upgrade_strategy).
--      Полный перебор без успеха паркует запись на лучшем по лестнице и
--      помечает EXHAUSTED (TCP) с кулдауном time секунд.
--   4. host_name неизвестен (чистый IP helper проверить не может): та же
--      пакетная логика, но насос CHECK не шлётся.
--   5. UDP (QUIC, STUN, WireGuard, ...) — helper не умеет проверять UDP:
--      никакой связи с helper, всё решает пакетная логика. Состояние своё:
--      autostate.auto_strategy_udp[host], свой счётчик fails и своя очередь
--      успехов (рейтинг отдельный на каждый план). Детект как в zapret2
--      (process_udp_fail): успех — новые пакеты сервера (для первого
--      вердикта — больше udp_in), фейл — udp_out исходящих подряд без новых
--      пакетов сервера. Вердикты перезаряжаемые: срабатывают на смене
--      состояния, не чаще одного раза на udp_out исходящих пакетов
--      (водяной знак udp_judged_client в lua_state соединения). Для QUIC
--      дополнительно работает throttle-окно: сервер отвечает, но медленно,
--      поэтому молчание такого потока не ловит... но мёртвую стратегию
--      ловит silence: если клиент шлёт, а сервер молчит udp_silence секунд —
--      это провал (backed-off QUIC клиент до udp_out пакетов не доживает,
--      а пауза в потоке не наказывается: надо, чтобы клиент говорил).
--      Долгоживущие потоки (голос Discord, WireGuard) перебирают стратегии
--      без переподключения. Голосовой план помечается udp_voice=1: в тихом
--      канале сервер молчит, пока никто не говорит, поэтому после первого
--      ответа пакетный счётчик голос больше не судит — иначе рабочая
--      стратегия валится за каждый промежуток без речи.
--   6. Все стратегии перебраны (exhausted) → EXHAUSTED хелперу, парковка на
--      лучшей по замеренной скорости стратегии и кулдаун time секунд:
--      полный прогон уже доказал, что лучше не будет, а немедленный новый
--      круг только рвёт сеть.
--   7. Таймеры — пульс READY и метр скорости; вердикты по троттлингу
--      считаются на пакетах, потому что без пакетов нет и улик.
--
-- Перебор: успешные по рейтингу (число успехов убыв.), затем неотмеченные 1..N
-- Параметры (через --lua-desync=auto_strategy:fails=4:...):
--   fails=N        — провалов до переключения (по умолч. 3)
--   time=N         — окно сброса счётчика провалов и кулдаун после
--                    исчерпания плана, сек (по умолч. 60)
--   maxseq=N       — макс. seq для проверок (по умолч. 32768)
--   throttle_window=N — окно замера скорости даунстрима, сек (по умолч. 3).
--                    Окно без серверных байт — провал только если клиент
--                    продолжает просить данные (его payload > 0); если клиент
--                    тоже молчит, это пауза с полным буфером, а не обрыв:
--                    вердикта нет и база стратегии не меняется
--   throttle_min_bps=N — ниже этой скорости (Б/с) окно плохое, и такой
--                    поток не подтверждается (по умолч. 50000: десятки КБ/с
--                    раньше считались рабочей стратегией, хотя видео на них
--                    стояло; планка низкая намеренно — на первом пройденном
--                    пороге семья не замирает, а идёт к самой быстрой
--                    замеренной стратегии, см. auto_upgrade_strategy)
--   throttle_collapse=N — доля пика соединения, ниже которой окно плохое
--                    (по умолч. 0.25; 0 — выключить проверку). Та же доля
--                    сравнивает окно с лучшей замеренной альтернативой:
--                    медленнее неё в 1/throttle_collapse раз — обвал
--   throttle_collapse_floor_bps=N — абсолютный потолок для проверки обвала,
--                    Б/с (по умолч. 100000): выше этой скорости падение
--                    относительно пика — полка провайдера, а не работа DPI.
--                    Это запасное правило для хоста, где ещё ничего не
--                    замерено: одна и та же полка 150 КБ/с на одном линке
--                    норма, на другом — стопор. Как только замерена другая
--                    стратегия, решает сравнение с ней (auto_best_other_rate):
--                    окно медленнее лучшей альтернативы в 1/throttle_collapse
--                    раз — обвал при любой абсолютной скорости, а если все
--                    стратегии ползут одинаково, это потолок линка, и
--                    перебирать их бессмысленно
--   throttle_windows=N — сколько плохих окон подряд до провала (по умолч. 2).
--                    Для потоковой семьи обвал ниже throttle_collapse_floor_bps
--                    решает сразу, не дожидаясь счётчика провалов: каждая новая
--                    сессия плеера начинается с бурста, два хороших окна
--                    подтверждают ту же стратегию и обнуляют серию, так что
--                    поток, который рвётся снова и снова, иначе не переедет
--   stream_min_bytes=N — объём серверных байт, с которого пото́к считается
--                    потоковым и его можно судить по скорости (по умолч.
--                    200000). Ниже метки это проба или API-запрос: объёма у
--                    них нет по природе, и вердикт им даёт ответ сервера, а не
--                    скорость (иначе 403 на пробе валит рабочую стратегию).
--   udp_stream_min_bytes=N — тот же порог для потокового хоста (googlevideo)
--                    на QUIC (по умолч. 16384). На UDP пробы хелпера нет:
--                    curl не умеет HTTP/3, поэтому объём googlevideo по QUIC —
--                    это само видео. Метка 200 КБ здесь только тормозит судью:
--                    ползущий поток добирается до неё за десятки секунд или не
--                    добирается вовсе, и переключения не происходит, хотя
--                    сервер уже отдаёт крохи.
--   udp_out=N      — исходящих UDP пакетов для вердикта FAIL (по умолч. 4)
--   udp_in=N       — входящих UDP пакетов: больше N — успех, иначе фейл (по умолч. 1)
--   udp_silence=N  — секунд молчания сервера при живом исходящем потоке
--                    (потоковые хосты и QUIC) до вердикта FAIL (по умолч. 3)
--   tcp_silence=N  — то же для TCP: клиент переспрашивает (ретрансмиссия
--                    после рукопожатия), сервер молчит N секунд, объёмного
--                    порога поток не набрал — вердикт FAIL и переключение
--                    (по умолч. 5; 0 — выключить). Это единственная улика
--                    для мелких потоков: рукопожатие прошло, ответа на
--                    запрос нет, а скорость на 200 КБ тут не померить.
--                    Запрос, который просто держат (long-poll), не
--                    переспрашивается — он подтверждён — и под правило
--                    не попадает. Отдельно: после запроса нет прикладных
--                    данных 2 × N секунд — один вердикт на соединение, и такое
--                    соединение не подтверждается (app_seen), поэтому серия
--                    провалов не обнуляется; long-poll, получивший заголовки,
--                    уже app_seen и под это правило не попадает. Для direct
--                    два вердикта SILENCE TCP за окно time — немедленный уход:
--                    успех между ними отдельный счётчик не обнуляет, а
--                    direct_ok снимается. У desync обычный порог fails
--   tcp_silence_windows=N — сколько таких окон подряд нужно до вердикта
--                    (по умолч. 2). Одно тихое окно ещё не провал: плеер
--                    между сегментами, догруженная страница и удержанный
--                    запрос молчат, а клиент при этом ничего не переспрашивает.
--                    Вердикт ставится, только когда клиент долбит через все
--                    окна подряд, а сервер молчит; любой байт сервера
--                    обнуляет серию. Потоковых семейств это не касается: у них
--                    окно одно, как было, — там стопор и есть остановившееся
--                    видео.
--   reset          — отправлять RST при ошибке
--   start=N        — начальный номер стратегии (0 = direct, по умолч. 0).
--                    Это пол, а не только первый шаг: после исчерпания плана
--                    запись не падает ниже него, и direct в кандидатах такого
--                    хоста не появляется
--   no_direct=зоны — зоны через запятую (например discord.media): хосты этих
--                    зон никогда не идут напрямую. Начальный номер становится
--                    не ниже start (минимум 1), а исчерпание плана не
--                    откатывает их в direct. Для фильтра, весь hostlist
--                    которого заведомо заблокирован: пробовать direct — это
--                    терять соединение ради улики, которая уже известна
--   udp_voice=1    — голосовой UDP: после первого ответа сервера пакетный
--                    счётчик больше не судит поток. В тихом голосовом канале
--                    сервер молчит, пока никто не говорит, — это не повод
--                    валить рабочую стратегию. Мёртвая стратегия видна до
--                    первого ответа (клиент шлёт, сервер не ответил ни разу);
--                    если путь умер позже, клиент переподключится и новый
--                    поток снова пройдёт проверку первого ответа
-- Общие зоны хостов задаются один раз при старте из templates.config:
--   --lua-init=unblock_host_policy[[stream=googlevideo.com,c.youtube.com,fbcdn.net;group=discord.media]]
-- unblock_host_policy пишет их в _G.unblock_stream_zones и
-- _G.unblock_group_zones, поэтому все auto_strategy читают одну конфигурацию.
-- stream — большие потоки этих зон судятся по скорости; group — поддомены
-- делят один hrec (group.<зона>), корень зоны остаётся отдельной записью.
-- Без вызова unblock_host_policy скрытых списков и группировки нет.
-- auto_host_group — ключ записи хоста. Группировка нужна, когда имя меняется
-- от сессии к сессии (c-<регион>-<хэш>.discord.media): без неё каждая новая
-- сессия начинала бы перебор заново. Признак потокового хоста (rec.stream)
-- не зависит от ключа записи.
-- Startup host policy, called once from templates.config via --lua-init after
-- this file is loaded:
--   unblock_host_policy[[stream=zone1,zone2;group=voice.zone]]
-- A long-bracket string, not a table: the service wrapper strips {} and quotes
-- from command-line Lua code, and this form survives it. Keeping the zones in
-- _G gives every auto_strategy profile the same host classes without repeating
-- the lists in every plan. A table call is kept for tests.
function unblock_host_policy(spec)
    local stream, group
    if type(spec) == "table" then
        stream, group = spec.stream, spec.group
    else
        for pair in tostring(spec):gmatch("[^;]+") do
            local key, value = pair:match("^%s*([^=]+)%s*=%s*(.-)%s*$")
            if key == "stream" then
                stream = value
            elseif key == "group" then
                group = value
            end
        end
    end
    _G.unblock_stream_zones = stream
    _G.unblock_group_zones = group
end

-- A zone named in stream=/no_direct=: the host itself or any subdomain.
-- The zone is matched on the hostname, never on the record key: grouping is a
-- local policy (see auto_host_group), while "direct is pointless here" belongs
-- to the zone and must survive any regrouping.
function auto_host_in_zones(host, zones)
    if not host or not zones then
        return false
    end
    host = host:lower()
    for zone in string.gmatch(zones, "[^,]+") do
        zone = zone:lower():match("^%s*(.-)%s*$")
        if zone ~= "" and (host == zone or host:sub(-#zone - 1) == "." .. zone) then
            return true
        end
    end
    return false
end

-- Subdomains of a group= zone share one record. The zone itself stays out:
-- discord.media is a website, while the ever-changing voice hosts below it
-- benefit from one remembered strategy. No configured zones, no grouping.
function auto_host_group(hostkey, zones)
    if not hostkey or not zones then
        return hostkey
    end
    local host = hostkey:lower()
    for zone in string.gmatch(zones, "[^,]+") do
        zone = zone:lower():match("^%s*(.-)%s*$")
        if zone ~= "" and host:sub(-#zone - 1) == "." .. zone then
            return "group." .. zone
        end
    end
    return hostkey
end

-- askey — table key inside autostate. TCP and UDP live in different winws2
-- profiles with their own strategy numbering, so their state is fully
-- separate: a UDP strategy switch must never touch TCP state and vice versa.
-- Profiles with different plans (broad TLS 29 strategies vs voice TCP 7) are
-- separated too: the fingerprint of the plan is appended to the askey, so a
-- host group record created by one profile can never poison another profile
-- whose plan has different strategy numbers.
function auto_host_record(desync, askey)
    local hostkey
    if desync.track and desync.track.hostname then
        hostkey = auto_host_group(desync.track.hostname, _G.unblock_group_zones)
    else
        hostkey = host_ip(desync)
        -- Same voice server, different client protocols: browser WebRTC
        -- (stun/dtls) works on direct while the desktop app
        -- (discord_ip_discovery) dies after one reply. Without separation
        -- the healthy STUN flow keeps confirming 'direct' and resets the
        -- shared fails counter, so the dying discovery flow never rotates
        -- past the first strategy. Key raw-IP UDP records by the
        -- connection's protocol class, fixed at the first packet.
        if askey and askey:find("_udp") and desync.track then
            local crec = auto_conn_record(desync)
            if crec then
                if not crec.proto_class then
                    local pt = desync.l7payload
                    crec.proto_class = (pt and pt ~= "unknown" and pt ~= "") and pt or "raw"
                end
                if crec.proto_class ~= "raw" then
                    hostkey = hostkey .. "#" .. crec.proto_class
                end
            end
        end
    end

    if not hostkey then
        return nil
    end

    if not autostate then
        autostate = {}
    end

    askey = askey or "auto_strategy"
    if not autostate[askey] then
        autostate[askey] = {}
    end

    if not autostate[askey][hostkey] then
        autostate[askey][hostkey] = {}
    end

    local rec = autostate[askey][hostkey]
    -- The key is kept on the record: a confirmation registers the host in the
    -- strategy's coverage, and the coverage is per host, not per connection.
    rec.key = hostkey
    -- Streaming host (googlevideo, Instagram's fbcdn media CDN): a server
    -- answer proves the site is reachable, not that its data path delivers.
    -- Such a host is confirmed only by a healthy goodput window (see
    -- auto_confirm_goodput) and judged by the throttle path on both
    -- transports - TLS and QUIC. Instagram's HTML/API can look perfect while
    -- reels and photos crawl on fbcdn; without this class there is no
    -- THROTTLE verdict for exactly the traffic the user came for.
    --
    -- The class is decided by the host's zone, never by the record key: the
    -- key only says what the record is pooled by (one family or one rr-host),
    -- and that is a local policy choice. Tying stream-ness to the family key
    -- meant that as soon as the googlevideo grouping was gone, hostkey became
    -- the bare rr-host, the equality check stopped matching and the whole
    -- video family silently lost the speed judge - on TCP confirmation and on
    -- QUIC goodput alike.
    local stream_host = desync.track and desync.track.hostname
    rec.stream = auto_host_in_zones(stream_host, _G.unblock_stream_zones)
    -- Zones the filter declared as never going direct (no_direct=discord.media):
    -- direct is not tried at all. Sticky on the record, so it also holds after
    -- the plan is exhausted and after a regrouping of the key.
    rec.no_direct = rec.no_direct or auto_host_in_zones(stream_host, desync.arg and desync.arg.no_direct)
    -- Voice gateways (*.discord.media) are long-lived low-volume websockets: a
    -- couple of kilobytes of handshake plus ping/pong must not look like a dead
    -- small flow, so the small-flow TCP silence rule is skipped for them.
    rec.voice_group = auto_host_in_zones(stream_host, _G.unblock_group_zones)
    return rec
end

-- Fingerprint of the profile's plan: the distinct strategy numbers still in it,
-- sorted and joined. The count alone was too weak: two different plans with the
-- same number of strategies (voice UDP and game UDP both carry two) got one
-- askey, so their host records, ratings and failure counters were pooled and the
-- verdicts of one plan steered the other.
function auto_plan_id(plan)
    local uniq = {}
    for _, instance in pairs(plan) do
        local s = tonumber(instance.arg.strategy)
        if s then
            uniq[s] = true
        end
    end

    local nums = {}
    for n in pairs(uniq) do
        nums[#nums + 1] = n
    end
    table.sort(nums)

    return table.concat(nums, ",")
end

function auto_conn_record(desync)
    if not desync.track then
        return nil
    end
    if not desync.track.lua_state.automate then
        desync.track.lua_state.automate = {}
    end
    return desync.track.lua_state.automate
end

-- Strategy name of the given per-protocol record.
function auto_strategy_name(rec)
    if rec.nstrategy == 0 then
        return "direct"
    end
    return "strategy_" .. rec.nstrategy
end

-- Log-only label of a strategy with its current weight: the decayed score of
-- the plan and, when fresh, the measured speed. The plain auto_strategy_name
-- stays for IPC signals; this one is only for human-readable lines, so the log
-- shows what the ladder is actually thinking (a strategy with w=0.00 is out of
-- favor, a decaying veteran loses its lead in front of your eyes).
function auto_strategy_label(is_udp, askey, n, now)
    if not n then
        return "strategy_?"
    end
    if n == 0 then
        return "direct"
    end
    now = now or os.time()
    local s = auto_score_value(auto_strategy_score(is_udp, askey)[n], now)
    local v = auto_strategy_speed_value(auto_strategy_speed(is_udp, askey), n, now)
    if v > 0 then
        return string.format("strategy_%d(w=%.2f v=%.0f)", n, s, v)
    end
    return string.format("strategy_%d(w=%.2f)", n, s)
end

-- Per-record fails counter. A connection may report a failure at most once
-- per 2 seconds (crec.failure_time): a single stuck handshake produces a
-- train of retransmissions and must not burn the whole threshold in one RTO,
-- while a long-lived stalled connection still accumulates failures. fast
-- makes one failure enough: during the search (the host has never worked yet)
-- waiting for the full threshold only stretches the sweep, and a strategy
-- that was judged wrongly is retried later. The counter is reset by time
-- (arg.time) and by a real success (the server's answer).
function auto_check_fails(rec, arg, crec, fast)
    if not arg.fails or arg.fails == 0 then
        return false
    end

    local now = os.time()
    if crec then
        if crec.failure_time and now - crec.failure_time < 2 then
            return false
        end
        crec.failure_time = now
    end

    if rec.fails and rec.fails_time and now - rec.fails_time > (tonumber(arg.time) or 60) then
        rec.fails = 0
    end
    -- The reset window counts from the first failure of the streak, not from
    -- the last one: otherwise a host that keeps failing with short gaps never
    -- resets and switches on failures spread over hours.
    if not rec.fails or rec.fails == 0 then
        rec.fails_time = now
    end
    rec.fails = (rec.fails or 0) + 1

    if rec.fails >= (fast and 1 or arg.fails) then
        rec.fails = nil
        return true
    end
    return false
end

-- Direct stall verdicts are counted apart from the generic failure counter,
-- and a success on direct does NOT erase them. The old rule let one good
-- connection wipe the whole streak, so direct clung to the host after a whole
-- series of stalls: handshake + some data on one connection, silence on the
-- next, and the strategy kept looking alive. Two unanswered requests inside
-- the fails window mean direct is unreliable and is abandoned at once, even
-- if other direct connections happened to succeed.
function auto_direct_stall_count(hrec, arg, now)
    if hrec.direct_stall_time and now - hrec.direct_stall_time > (tonumber(arg.time) or 60) then
        hrec.direct_stall_n = 0
    end
    if not hrec.direct_stall_n or hrec.direct_stall_n == 0 then
        hrec.direct_stall_time = now
    end
    hrec.direct_stall_n = (hrec.direct_stall_n or 0) + 1
    return hrec.direct_stall_n >= 2
end

-- Shared by both TCP silence verdicts. Desync strategies keep the ordinary
-- fails threshold; direct has no packet loss excuse after two mute answers.
function auto_direct_stall_decisive(hrec, arg, now)
    if hrec.nstrategy ~= 0 or not auto_direct_stall_count(hrec, arg, now) then
        return false
    end
    hrec.direct_ok = nil
    return true
end

-- Real success: reset the failure episode. The score (and its decay) is the
-- durable memory now, so there is no cycle ladder to clear here.
function auto_mark_success(hrec)
    hrec.fails = nil
    hrec.fails_time = nil
end

-- direct (strategy number 0) never reaches the rating: auto_rate_strategy keeps
-- number 0 out of it by design, and the candidate loop only walks 1..ctstrategy.
-- So the successful set of a host that answered on direct stays empty and the
-- ladder walks it onto a desync it does not need (the trap described in
-- auto_strategy_candidates). Remember the success here instead.
function auto_mark_direct(rec)
    if rec.nstrategy == 0 then
        rec.direct_ok = true
    end
end

-- direct at the head of the candidates. Only for a host that really answered on
-- it, and never for a streaming family: on googlevideo a healthy window on
-- direct proves only that the shaper let the first burst through, and leading
-- with direct pulled the family back onto no bypass at every failure - the
-- measured ladder exists exactly to keep a stream on the strategy that measured
-- best, so the stream keeps what it had (start=0 still tries direct first) and
-- after a failure moves on like before. A no_direct= zone (discord.media) is
-- the same policy from the config side: the host never goes direct, so direct
-- must not come back even if some old verdict marked it good.
function auto_direct_first(rec, list)
    if rec.stream or rec.no_direct or not rec.direct_ok or (rec.tried and rec.tried[0]) then
        return list
    end

    local out = { 0 }
    for _, n in ipairs(list) do
        out[#out + 1] = n
    end
    return out
end

-- Consecutive quiet windows of the TCP silence rule before the strategy is
-- blamed. One window is not evidence: a player between two segments, a page
-- that already loaded and a held request all leave windows with no server
-- bytes, and a verdict per lull reset a working connection and spent a
-- strategy switch (field logs: SILENCE raining on hosts that need no bypass at
-- all). Overridable per plan with tcp_silence_windows=N.
local c_tcp_silence_windows = 2

-- Score decay and freshness windows. The score halves every c_score_half_life
-- seconds of silence; coverage and measured speed count only entries inside
-- their TTL; a plan-wide failure penalty needs c_fail_hosts distinct hosts; the
-- score is capped so one long success streak cannot pin a strategy forever;
-- coverage is only a small additive prior for ordinary hosts.
local c_score_half_life = 600
local c_coverage_ttl = 1800
local c_speed_ttl = 900
local c_score_stale = 1800
local c_last_good_ttl = 3600
local c_fail_penalty = 2
local c_fail_hosts = 2
local c_score_cap = 10
local c_coverage_bonus = 0.5

-- Ordered candidates for the host's next strategy: known strategies first
-- (confirmed at least once in this run, see auto_strategy_last_good), then the
-- decayed success score, with the speed of streaming confirmations as the first
-- criterion for a streaming host and the second for an ordinary one (API calls
-- never measure themselves, so they borrow the video plan's measurements). The
-- known tier is what stops the ladder from forgetting a working strategy and
-- re-walking the plan from strategy 1 after a quiet spell; within the tier the
-- decayed score and the fresh speed still decide, so a strategy that stopped
-- working loses to a fresher known one. Never-confirmed strategies have no
-- last_good entry and rank below the known ones. Coverage is only a small prior
-- for ordinary hosts (see c_coverage_bonus). Only numbers present in THIS plan
-- are considered; already-tried numbers (rec.tried) are skipped.
function auto_strategy_candidates(rec, is_udp, askey, now)
    now = now or os.time()

    local scores   = auto_strategy_score(is_udp, askey)
    local coverage = auto_strategy_coverage(is_udp, askey)
    local speed    = auto_strategy_speed(is_udp, askey)
    local last     = auto_strategy_last_good(is_udp, askey)

    local list = {}
    for n = 1, rec.ctstrategy do
        if rec.strategy_set and rec.strategy_set[n] and not (rec.tried and rec.tried[n]) then
            local s = auto_score_value(scores[n], now)
            if not rec.stream then
                s = s + c_coverage_bonus * math.log(1 + auto_strategy_host_count(coverage, n, now))
            end
            local known = (last[n] and now - last[n] <= c_last_good_ttl) and 1 or 0
            list[#list + 1] = { n = n, s = s, v = auto_strategy_speed_value(speed, n, now), k = known }
        end
    end

    table.sort(list, function(a, b)
        if a.k ~= b.k then
            return a.k > b.k
        end
        if rec.stream then
            if a.v ~= b.v then
                return a.v > b.v
            end
            if a.s ~= b.s then
                return a.s > b.s
            end
        else
            if a.s ~= b.s then
                return a.s > b.s
            end
            if a.v ~= b.v then
                return a.v > b.v
            end
        end
        return a.n < b.n
    end)

    local out = {}
    for _, item in ipairs(list) do
        out[#out + 1] = item.n
    end

    -- A host that answered on direct leads with it, before any desync it might
    -- not need. The ladder still moves on once direct fails (rec.tried[0]).
    return auto_direct_first(rec, out)
end

-- Best known strategy of the host: the same ladder, used to park the host
-- somewhere sane after a full sweep instead of dropping it to direct.
function auto_best_strategy(rec, is_udp, askey, now)
    now = now or os.time()

    local scores   = auto_strategy_score(is_udp, askey)
    local coverage = auto_strategy_coverage(is_udp, askey)
    local speed    = auto_strategy_speed(is_udp, askey)
    local last     = auto_strategy_last_good(is_udp, askey)

    local best, best_s, best_v, best_k = nil, -1, -1, -1
    for n in pairs(rec.strategy_set or {}) do
        local s = auto_score_value(scores[n], now)
        if not rec.stream then
            s = s + c_coverage_bonus * math.log(1 + auto_strategy_host_count(coverage, n, now))
        end
        local v = auto_strategy_speed_value(speed, n, now)
        local k = (last[n] and now - last[n] <= c_last_good_ttl) and 1 or 0
        local better
        if k ~= best_k then
            better = k > best_k
        elseif rec.stream then
            better = v > best_v or (v == best_v and s > best_s)
        else
            better = s > best_s or (s == best_s and v > best_v)
        end
        if better then
            best, best_s, best_v, best_k = n, s, v, k
        end
    end
    return best
end

-- Rotate to the next strategy for the record of the current protocol.
-- send_exhausted: TCP reports a fully-tried host to the HELPER only
-- (EXHAUSTED to port 10000 — the spam channel). The helper owns the
-- dead-host state and relays it to unblock; there is deliberately no
-- direct signal to unblock. UDP never sends it: UDP exhaustion says
-- nothing about TCP reachability of the host.
function auto_do_switch(rec, is_udp, askey, reason, peer, dport, send_exhausted, crec)
    -- An upgrade is not a failure: the host moves to a measurably faster
    -- strategy while it works, so the strategy being left is not marked as
    -- tried and does not poison the plan-wide failure scoreboard below. The
    -- caller reports the move itself - it knows both ends of it.
    local upgrade = (reason == "UPGRADE")
    if not upgrade then
        ULOG("WARNING",
            "zapret:auto_strategy: FAIL " .. auto_strategy_label(is_udp, askey, rec.nstrategy) .. " " .. reason .. "->" ..
                peer .. ":" .. dport)
    end

    local now = os.time()

    -- The next strategy starts from a clean slate: the old connection's peak
    -- would otherwise mark every ramp-up window as a collapse, and the old
    -- rating would block rating the new strategy on this connection.
    if crec then
        crec.t_peak = nil
        crec.t_bad = nil
        crec.t_good = nil
        crec.t_window_start = nil
        crec.t_window_base = nil
        crec.t_window_bytes = nil
        crec.t_client_base = nil
        crec.t_client_window_bytes = nil
        crec.rated_n = nil
        crec.goodput_ok = nil
        crec.udp_success = nil
        -- Silence state: the new strategy gets a full window before it can
        -- be failed for silence, and the paused-flow guard starts over.
        crec.t_server = nil
        crec.t_seen = nil
        crec.t_silence = nil
        crec.udp_silence_client = nil
        -- TCP stall window: the new strategy starts its own silence timer and
        -- its own retransmission mark.
        crec.tcp_srv_seen = nil
        crec.tcp_srv_time = nil
        crec.tcp_stalled = nil
        crec.tcp_client_pkts = nil
        crec.tcp_quiet_windows = nil
        crec.tcp_quiet_client = nil
        crec.app_seen = nil
        crec.app_req_srv = nil
        crec.app_req_time = nil
        crec.stall_verdict = nil
        crec.hs_srv = nil
    end

    if not rec.tried then
        rec.tried = {}
    end
    if rec.nstrategy and not upgrade then
        -- direct is marked tried as well: once it failed for this host the
        -- ladder walks on, and direct returns only after a pass resets the list
        -- or the host succeeds on something else. Number 0 is never rated and
        -- never penalizes the plan-wide score, so it only lives in rec.tried.
        rec.tried[rec.nstrategy] = true
        auto_strategy_note_failure(is_udp, askey, rec, peer, now)
    end

    local candidates = auto_strategy_candidates(rec, is_udp, askey, now)

    if #candidates == 0 then
        -- Every strategy in the plan tried: park the host on the best of the
        -- ladder and cool down, an immediate new sweep only churns the network.
        -- The score decay keeps working while it is parked, so the next sweep
        -- starts from a fresher champion. When nothing was ever measured, the
        -- floor is the plan's start (a no_direct zone starts at 1): parking such
        -- a host on direct would hand it straight to the blocker it cannot pass.
        local floor = rec.start or 0
        if rec.no_direct and floor < 1 then
            floor = 1
        end
        rec.nstrategy = auto_best_strategy(rec, is_udp, askey, now) or floor
        if send_exhausted then
            send_signal("EXHAUSTED", peer, auto_strategy_label(is_udp, askey, rec.nstrategy), 10000)
        end
        rec.tried = {}
        rec.t_exhausted = now
        return
    end

    rec.nstrategy = candidates[1]
end

-- Per-plan strategy score: number -> { s = weighted success score, t = time of
-- the last event }. The score decays lazily with time (see auto_score_value),
-- so a strategy that stopped working slides down on its own instead of sitting
-- on the pedestal forever: DPI adapts, and yesterday's win says little about
-- today. Keyed by askey (protocol + plan fingerprint), because strategy numbers
-- mean different things in different profiles. In-run only: a winws2 restart
-- clears it.
function auto_strategy_score(is_udp, askey)
    local key = (is_udp and "udp_strategy_score" or "strategy_score") .. "#" .. tostring(askey or "default")
    _G[key] = _G[key] or {}
    return _G[key]
end

-- Per-plan host coverage: number -> { host key -> last confirmation time }.
-- Counted per host, not per connection, and only fresh entries count: a
-- strategy that worked on thirty hosts an hour ago is not ahead of one that
-- just proved itself.
function auto_strategy_coverage(is_udp, askey)
    local key = (is_udp and "udp_strategy_hosts" or "strategy_hosts") .. "#" .. tostring(askey or "default")
    _G[key] = _G[key] or {}
    return _G[key]
end

-- Per-plan strategy speed: number -> { r = EWMA goodput, t = last measurement }.
-- Fed by streaming confirmations; it gives ordinary (non-streaming) hosts the
-- speed data of the video flows they never measure themselves.
function auto_strategy_speed(is_udp, askey)
    local key = (is_udp and "udp_strategy_speed" or "strategy_speed") .. "#" .. tostring(askey or "default")
    _G[key] = _G[key] or {}
    return _G[key]
end

-- Per-plan last confirmation time of each strategy: number -> time. It is the
-- durable "this one worked in this run" mark: the decayed score and the speed
-- expire in minutes, but the ladder must not forget a working strategy and
-- re-walk the plan from strategy 1 after a quiet spell. Never-confirmed
-- strategies have no entry and always rank below the known ones.
function auto_strategy_last_good(is_udp, askey)
    local key = (is_udp and "udp_strategy_last_good" or "strategy_last_good") .. "#" .. tostring(askey or "default")
    _G[key] = _G[key] or {}
    return _G[key]
end

-- Distinct hosts that failed this strategy recently: number -> { host -> time }.
-- A single bad SNI or region must not sink a strategy for everyone, so the
-- plan-wide penalty waits for c_fail_hosts different hosts.
function auto_strategy_failed_hosts(is_udp, askey)
    local key = (is_udp and "udp_strategy_failed_hosts" or "strategy_failed_hosts") .. "#" .. tostring(askey or "default")
    _G[key] = _G[key] or {}
    return _G[key]
end

-- Lazy decay of a score to `now`: halves every c_score_half_life seconds of
-- silence. Nothing rewrites the score in the background; reading it is enough.
-- A score that has not been refreshed for c_score_stale seconds reads as 0, so
-- a strategy that stopped working does not keep a microscopic lead over the
-- ones that never confirmed anything.
function auto_score_value(entry, now)
    if not entry or not entry.s or entry.s <= 0 then
        return 0
    end

    local dt = now - (entry.t or now)
    if dt < 0 then
        dt = 0
    end
    if dt >= c_score_stale then
        return 0
    end
    return entry.s * 0.5 ^ (dt / c_score_half_life)
end

-- Fresh host count of a strategy: only confirmations inside the coverage TTL
-- count.
function auto_strategy_host_count(coverage, n, now)
    local hosts = coverage and coverage[n]
    if not hosts then
        return 0
    end

    local count = 0
    for _, t in pairs(hosts) do
        if now - t <= c_coverage_ttl then
            count = count + 1
        end
    end
    return count
end

-- Fresh measured speed of a strategy, 0 when the measurement went stale.
function auto_strategy_speed_value(speed, n, now)
    local entry = speed and speed[n]
    if not entry or not entry.r or (now - (entry.t or now)) > c_speed_ttl then
        return 0
    end
    return entry.r
end

-- One confirmation of rec's current strategy: +1 to its decayed score and the
-- host is added to the plan coverage. Called once per connection and strategy,
-- so the score grows with real confirmations, not with packet chatter.
function auto_rate_strategy(is_udp, askey, rec, now)
    if not rec or not rec.nstrategy or rec.nstrategy == 0 then
        return
    end
    now = now or os.time()

    local scores = auto_strategy_score(is_udp, askey)
    local s = auto_score_value(scores[rec.nstrategy], now) + 1
    if s > c_score_cap then
        s = c_score_cap
    end
    scores[rec.nstrategy] = { s = s, t = now }
    auto_strategy_last_good(is_udp, askey)[rec.nstrategy] = now

    if rec.key then
        local coverage = auto_strategy_coverage(is_udp, askey)
        local hosts = coverage[rec.nstrategy]
        if not hosts then
            hosts = {}
            coverage[rec.nstrategy] = hosts
        end
        hosts[rec.key] = now
    end
end

-- A measured goodput sample feeds the plan-wide speed EWMA. Half weight keeps
-- one lucky burst from pinning the strategy at its peak; stale entries are
-- ignored by auto_strategy_speed_value.
function auto_strategy_speed_update(is_udp, askey, n, rate, now)
    if not n or n == 0 or not rate or rate <= 0 then
        return
    end
    now = now or os.time()

    local speed = auto_strategy_speed(is_udp, askey)
    local entry = speed[n]
    local r = rate
    if entry and entry.r and (now - (entry.t or now)) <= c_speed_ttl then
        r = entry.r + (rate - entry.r) * 0.5
    end
    speed[n] = { r = r, t = now }
end

-- Plan-wide failure penalty, applied only once c_fail_hosts distinct hosts
-- failed the strategy inside the coverage TTL. The failed-host set resets after
-- the penalty, so the next batch needs its own witnesses.
function auto_strategy_penalty(is_udp, askey, n, now)
    if not n or n == 0 then
        return
    end
    now = now or os.time()

    local scores = auto_strategy_score(is_udp, askey)
    local s = auto_score_value(scores[n], now) - c_fail_penalty
    if s < 0 then
        s = 0
    end
    scores[n] = { s = s, t = now }
end

-- One host failed the strategy: remember it and, when c_fail_hosts different
-- hosts have failed inside the TTL, drop the plan-wide score. A lone failing
-- SNI is a local verdict only.
function auto_strategy_note_failure(is_udp, askey, rec, hostkey, now)
    if not rec or not rec.nstrategy or rec.nstrategy == 0 then
        return
    end
    -- Only a host that ever completed a handshake says something about the
    -- strategy: an endpoint that never answered on any strategy (curl=35 in the
    -- helper log, is_retransmission here) is dead on its own, and its failures
    -- must not drag a working strategy down for everyone else.
    if not rec.reachable then
        return
    end
    now = now or os.time()
    hostkey = hostkey or rec.key or "?"

    local failed = auto_strategy_failed_hosts(is_udp, askey)
    local hosts = failed[rec.nstrategy]
    if not hosts then
        hosts = {}
        failed[rec.nstrategy] = hosts
    end
    hosts[hostkey] = now

    local count = 0
    for _, t in pairs(hosts) do
        if now - t <= c_coverage_ttl then
            count = count + 1
        end
    end
    if count >= c_fail_hosts then
        auto_strategy_penalty(is_udp, askey, rec.nstrategy, now)
        failed[rec.nstrategy] = {}
    end
end

-- The helper is a pump and executor, not a judge: TCP asks it to open a
-- fresh connection for the host through the current strategy (CHECK:<host>)
-- so new packets keep flowing without user traffic. Rate-limited per host
-- record: a retransmission train across many connections must not spawn a
-- probe storm, but a long throttled connection still needs a fresh probe
-- after every switch. UDP is never sent: the helper cannot check UDP.
function auto_request_check(host_name, hrec)
    if not host_name then
        return
    end
    if hrec then
        local now = os.time()
        if hrec.t_check and now - hrec.t_check < 5 then
            return
        end
        hrec.t_check = now
    end
    send_signal("CHECK", host_name, nil, 10000)
end

-- TCP error report to the helper (port 10000 is the spam channel). ERR tells
-- the helper the strategy failed for this host; the verdict itself is made
-- by Lua from packet evidence.
function auto_fail_helper_strategy(name, host_name)
    if host_name then
        send_signal("ERR", host_name, name, 10000)
    end
end

-- Send RST to break the long wait (TCP only). force lifts the
-- outgoing/maxseq gates: a throttled stream is judged on an incoming packet
-- deep in the sequence, and the stalled connection must be killed, otherwise
-- the player keeps limping on the old strategy. An outgoing packet is
-- reversed so the RST looks like the server's reply and the local stack
-- aborts the connection; an incoming packet is sent back as is, toward the
-- client, for the same reason.
function auto_reset_connection(desync, arg, name, peer, dport, force)
    if not arg.reset or not desync.dis.tcp then
        return
    end
    if not desync.outgoing and not force then
        return
    end
    local seq = pos_get(desync, 's')
    if not force and (#desync.dis.payload == 0 or arg.maxseq <= 0 or seq > arg.maxseq) then
        return
    end

    local dis = deepcopy(desync.dis)
    dis.payload = nil
    local pos = desync.outgoing and desync.track and desync.track.pos.reverse or desync.track and desync.track.pos.direct
    if desync.outgoing then
        dis_reverse(dis)
    end
    dis.tcp.th_flags = TH_RST
    dis.tcp.th_win = pos and pos.tcp and pos.tcp.winsize or 64
    dis.tcp.options = nil

    if dis.ip6 then
        dis.ip6.ip6_flow = pos and pos.ip6_flow or 0x60000000;
    end
    ULOG("INFO", "zapret:reset_connection: " .. name .. "->" .. peer .. ":" .. dport)

    rawsend_dissect(dis, {
        ifout = desync.ifin or 0
    })
end

-- Pop and execute the plan instances of the current strategy. Returns the
-- aggregated verdict.
function auto_strategy_plan(desync, rec, verdict)
    while true do
        local inst = plan_instance_pop(desync)
        if not inst then
            break
        end

        if inst.arg.strategy and tonumber(inst.arg.strategy) == rec.nstrategy and rec.nstrategy ~= 0 then
            verdict = plan_instance_execute(desync, verdict, inst)
        end
    end
    return verdict
end

-- Goodput floor for a video host, B/s: below it the flow is not really
-- working. A strategy that only lets a crawl through must not be confirmed
-- (VALID) and must not outrank one that really delivered - tens of KB/s used
-- to pass this gate and the host was certified as working while its video
-- stood still. The floor is deliberately low (50 KB/s is where a video stops
-- being a slideshow, not a demand for HD): a slow line must not lose a working
-- strategy over it, because the plan does not settle for the first strategy
-- that clears the bar - it keeps moving to the fastest measured one (see
-- auto_upgrade_strategy). Tunable per plan with throttle_min_bps.
local c_throttle_min_bps = 50000

-- The peak-relative test needs an absolute ceiling, B/s. A provider that lets
-- a burst through and then holds a steady plateau is not doing DPI work, and a
-- 200 KB/s plateau carries 1080p: judged against a 1 MB/s burst it looks like
-- a collapse, and the family leaves a stream that is playing fine. The
-- collapse rule exists for a flow cut down to a crawl after the burst, so it
-- only fires below this rate; above it the window is a plateau and, being
-- above throttle_min_bps, a healthy one - it still rates the strategy.
-- Tunable per plan with throttle_collapse_floor_bps (0 - judge any drop).
local c_throttle_collapse_floor_bps = 100000

-- Volume mark for a streaming host on QUIC, bytes. The TCP mark (200000) is
-- high because the helper probes googlevideo over TCP and gets a small 403
-- page - judging that probe by speed used to fail a working strategy. There is
-- no HTTP/3 probe: curl speaks TCP only, so everything googlevideo sends over
-- QUIC is the video itself and the handshake is only a few KB. A crawl of a
-- few KB/s would need a minute to reach 200 KB (a flow that reconnects keeps
-- resetting it), and the judge would stay silent while the player starves.
-- Tunable per plan with udp_stream_min_bytes.
local c_udp_stream_min_bytes = 16384

-- Per-connection downstream goodput (throttling-after-handshake). The
-- conntrack counter is cumulative for the whole flow, so a packet that never
-- reaches Lua (another profile's filter, a skipped block) does not lose its
-- bytes. Server-side counter: video data is what matters here.
function auto_throttle_account(crec, desync, now)
    local pos = desync.track and desync.track.pos
    local srv = pos and pos.server
    local cli = pos and pos.client
    if not srv or not srv.pbcounter then
        return
    end

    local bytes = srv.pbcounter
    local cbytes = cli and cli.pbcounter or 0
    if not crec.t_window_start then
        crec.t_window_start = now
        crec.t_window_base = bytes
        crec.t_window_bytes = 0
        crec.t_client_base = cbytes
        crec.t_client_window_bytes = 0
    else
        if bytes >= crec.t_window_base then
            crec.t_window_bytes = bytes - crec.t_window_base
        else
            -- counter wrapped or reset, restart the window
            crec.t_window_base = bytes
            crec.t_window_bytes = 0
        end
        -- Client side of the same window: requests, ACKs, retransmissions.
        -- It is what separates a stalled server from a player that simply
        -- stopped asking because its buffer is full.
        if cbytes >= (crec.t_client_base or 0) then
            crec.t_client_window_bytes = cbytes - (crec.t_client_base or 0)
        else
            crec.t_client_base = cbytes
            crec.t_client_window_bytes = 0
        end
    end
end

-- Goodput (B/s) over the finished window, nil while the window is still
-- open. Three kinds of window are not a measurement: the still-open one, an
-- idle one before the connection ever delivered, and a long gap (bytes
-- accumulated over a minute of silence are not a throughput measurement).
--
-- An idle window after a healthy phase is evidence only while the client is
-- still talking: requests, ACKs, retransmissions mean it wants data and the
-- server does not deliver - a stall, the DPI cut the data path. A player with
-- a full buffer stops asking too, and then the window is just idle: no
-- verdict, no baseline change, no strategy switch. That is the case the bare
-- zero-byte rule got wrong: it called every quiet second a stall.
-- A fully paused connection produces no packets at all, so no window closes:
-- it lands in the long-gap branch instead of here.
function auto_throttle_rate(crec, arg, now)
    if not crec.t_window_start then
        return nil
    end

    local window = tonumber(arg.throttle_window) or 3
    local elapsed = now - crec.t_window_start
    if elapsed < window then
        return nil
    end

    local bytes = crec.t_window_bytes or 0
    local client_bytes = crec.t_client_window_bytes or 0
    crec.t_window_start = now
    crec.t_window_base = (crec.t_window_base or 0) + bytes
    crec.t_window_bytes = 0
    crec.t_client_base = (crec.t_client_base or 0) + client_bytes
    crec.t_client_window_bytes = 0

    if elapsed > window * 4 then
        crec.t_bad = 0
        return nil
    end

    if bytes == 0 then
        -- Nothing arrived from the server. Without client traffic this is not
        -- a verdict: the player has enough buffered and stopped asking, so the
        -- quiet window is idle, not a stall - and it must not zero the
        -- strategy's measured baseline either.
        if client_bytes == 0 then
            crec.t_bad = 0
            return nil
        end
        local healthy = (crec.t_peak or 0) >= (tonumber(arg.throttle_min_bps) or c_throttle_min_bps)
        if not healthy or elapsed > window * 2 then
            crec.t_bad = 0
            return nil
        end
        -- Stall: 0 B/s while the client keeps asking, counted as a bad window.
        return 0
    end

    return bytes / elapsed
end

-- Best goodput another strategy of this plan delivered on this host lately.
-- last_rate holds the latest window, not the all-time maximum: the moment the
-- alternative is tried and turns out just as slow, its baseline drops and the
-- host stops rotating. That is what separates a throttled strategy from a slow
-- link - if every strategy crawls at the same rate, none of them is a collapse.
function auto_best_other_rate(hrec)
    local best = 0
    for n, v in pairs(hrec.last_rate or {}) do
        if n ~= hrec.nstrategy and hrec.strategy_set and hrec.strategy_set[n] and v > best then
            best = v
        end
    end
    return best
end

-- Throttle verdict over rate windows of one connection. A window is bad when
-- it is below the absolute floor or collapsed against the best window this
-- connection has seen (DPI throttling arrives as a sharp drop after a fast
-- start, not as a uniformly slow link). The peak is per connection, so a
-- legitimately lower bitrate in a new connection is not mistaken for a
-- collapse.
--
-- The absolute floor (throttle_collapse_floor_bps) is only the fallback for a
-- host with nothing else measured: a plateau above it may be either the link's
-- ceiling or a throttled strategy, and no absolute number separates the two
-- (the same 150 KB/s is normal on one link and a stall on another). Once
-- another strategy has been measured, the comparison is relative: a window
-- that is throttle_collapse times slower than that alternative is a bad one at
-- any rate, and the floor no longer applies. Returns true when the switch
-- threshold is reached; the streak restarts then, so one verdict costs
-- throttle_windows bad windows. The third value marks a collapse against this
-- connection's own peak: only that is decisive for a video family (the burst
-- got through, the path was cut), because the failure counter cannot
-- accumulate there - every new connection's burst confirms the strategy and
-- clears the streak. An alternative-relative window is never decisive: the
-- alternative may be a stale burst, and killing a working strategy on it just
-- churns the plan.
function auto_throttle_judge(hrec, crec, arg, rate)
    local peak = crec.t_peak or 0
    local min = tonumber(arg.throttle_min_bps) or c_throttle_min_bps
    local ratio = tonumber(arg.throttle_collapse) or 0
    local floor = tonumber(arg.throttle_collapse_floor_bps)
        or c_throttle_collapse_floor_bps

    -- Latest window of the current strategy: the baseline for the next
    -- comparison, so a tried-and-slow alternative stops being attractive.
    hrec.last_rate = hrec.last_rate or {}
    hrec.last_rate[hrec.nstrategy] = rate

    local best_other = auto_best_other_rate(hrec)
    local worse_than_alternative = ratio > 0 and best_other > 0 and rate < best_other * ratio
    -- A collapse against the connection's own peak is DPI work: the burst got
    -- through and the path was cut. Being merely slower than another strategy
    -- is not: that alternative may be a stale burst from a connection that
    -- ended long ago, and killing the current flow on its first window against
    -- it churns the plan while the strategy works. Only the own-peak collapse
    -- is decisive; the alternative comparison accumulates through the ordinary
    -- window threshold and is handled by auto_upgrade_strategy once confirmed.
    local own_collapse = ratio > 0 and peak > 0 and rate < peak * ratio
        and (floor <= 0 or rate < floor)
    local collapsed = own_collapse or worse_than_alternative
    local bad = rate < min or collapsed
    if rate > peak then
        crec.t_peak = rate
    end

    if not bad then
        crec.t_bad = 0
        return false, false, false
    end
    -- A bad window breaks the healthy streak: the success confirmation must
    -- see two good windows in a row, otherwise a strategy that is collapsing
    -- right now would still be rated (and VALID sent).
    crec.t_good = 0
    crec.t_bad = (crec.t_bad or 0) + 1
    if crec.t_bad >= (tonumber(arg.throttle_windows) or 2) then
        crec.t_bad = 0
        return true, true, own_collapse, best_other
    end
    return false, true, own_collapse, best_other
end

-- Healthy window: rate the strategy by real goodput. A streaming connection
-- is confirmed only after two healthy windows in a row: DPI throttling
-- arrives after a short fast burst, and the first window alone would rate a
-- strategy that is about to collapse. Once per connection and strategy.
function auto_confirm_goodput(hrec, crec, arg, rate, is_udp, askey)
    if crec.rated_n == hrec.nstrategy or rate < (tonumber(arg.throttle_min_bps) or c_throttle_min_bps) then
        return
    end

    crec.t_good = (crec.t_good or 0) + 1
    if crec.t_good < 2 then
        return
    end

    crec.goodput_ok = true
    if hrec.nstrategy == 0 then
        auto_mark_direct(hrec)
        return
    end

    crec.rated_n = hrec.nstrategy
    local now = os.time()
    auto_rate_strategy(is_udp, askey, hrec, now)
    auto_strategy_speed_update(is_udp, askey, hrec.nstrategy, rate, now)

    hrec.had_success = true
    auto_mark_success(hrec)
end

-- A confirmed strategy is not the end of the search. The plan-wide speed EWMA
-- keeps the goodput of every strategy that ever delivered, and a video family
-- that sits on a crawl while a measurably faster strategy exists is moved to
-- that one: a strategy can rot while the connection still lives, so the family
-- must not stay on the first one that answered. The margin keeps two strategies
-- whose speeds differ only within measurement noise from handing the family
-- back and forth on every window; a stale measurement is ignored entirely
-- (auto_strategy_speed_value), so an old champion cannot hold the pedestal.
local c_upgrade_ratio = 1.3

-- Move the host to a measurably faster strategy, called right after a healthy
-- window confirmed the current one. Returns true when the host moved.
function auto_upgrade_strategy(hrec, crec, is_udp, askey, peer, dport)
    if not hrec.stream or not hrec.nstrategy or hrec.nstrategy == 0 then
        return false
    end

    local now = os.time()
    local speed = auto_strategy_speed(is_udp, askey)
    local cur = auto_strategy_speed_value(speed, hrec.nstrategy, now)
    local best, best_rate = nil, 0
    for n in pairs(hrec.strategy_set or {}) do
        local v = auto_strategy_speed_value(speed, n, now)
        if v > best_rate then
            best, best_rate = n, v
        end
    end

    if cur <= 0 or not best or best == hrec.nstrategy or best_rate < cur * c_upgrade_ratio then
        return false
    end

    local from_n = hrec.nstrategy
    auto_do_switch(hrec, is_udp, askey, "UPGRADE", peer, dport, false, crec)

    -- The plan may have had nothing better to offer after all (the fastest
    -- measured one is already in use, or it is marked as tried): then the host
    -- stays put and this is not worth a line in the log.
    if hrec.nstrategy == from_n then
        return false
    end

    ULOG("OK",
        "zapret:auto_strategy: SWITCH " .. auto_strategy_label(is_udp, askey, from_n) .. "->" ..
            auto_strategy_label(is_udp, askey, hrec.nstrategy) .. " " .. peer .. ":" .. dport)
    return true
end

-- Live throughput of the traffic the engine carries, split by direction, for
-- the status line. auto_rate_beat flushes both counters once a second.
--
-- The counters come from the conntrack byte counters, not from the payload of
-- the packet that woke Lua up. The QUIC profile has no --in-range=a (only the
-- TLS and voice templates enable incoming calls), so on a UDP download Lua is
-- only called for the client's outgoing packets: a per-packet payload account
-- counts upload only and the status line reads "0 B/s" while the video plays.
-- The conntrack counters are advanced by C for every packet of the flow, so
-- their delta since the previous call covers the download as well, no matter
-- which direction woke Lua up. The baseline lives on the connection record.
-- Retransmissions are included, just as in the counter itself: this is the
-- traffic the engine carries, not the amount of unique data.
function auto_rate_account(crec, desync)
    local pos = desync.track and desync.track.pos
    if not pos then
        return
    end

    local up_now = pos.client and pos.client.pbcounter or 0
    local down_now = pos.server and pos.server.pbcounter or 0
    if not crec.rate_up_last then
        crec.rate_up_last = up_now
        crec.rate_down_last = down_now
        return
    end

    local up = up_now - crec.rate_up_last
    local down = down_now - crec.rate_down_last
    crec.rate_up_last = up_now
    crec.rate_down_last = down_now

    if up > 0 then
        _G.zapret_rate_up = (_G.zapret_rate_up or 0) + up
    end
    if down > 0 then
        _G.zapret_rate_down = (_G.zapret_rate_down or 0) + down
    end
end

function auto_strategy(ctx, desync)
    orchestrate(ctx, desync)
    if not desync.track then
        return
    end

    -- TCP and UDP run in different winws2 profiles with different strategy
    -- numbering, so the host record (and all queues) is per protocol.
    -- Profiles with different plans (e.g. broad TLS vs voice TCP) are further
    -- separated by the plan fingerprint: a shared host group (discord.media)
    -- must keep per-profile records, otherwise the record's nstrategy points
    -- to strategy numbers that do not exist in the other profile's plan and
    -- no instances are executed at all.
    local is_udp = desync.dis.udp ~= nil
    local crec = auto_conn_record(desync)
    local plen = #(desync.dis.payload or "")

    -- Bytes of the flow since the previous packet: the status line rate.
    auto_rate_account(crec, desync)

    -- The plan fingerprint is a property of the profile, and this runs for every
    -- packet: keep it on the connection record, where the profile cannot change,
    -- and pay for the walk over the plan once per connection.
    local plan_id = crec and crec.plan_id
    if not plan_id then
        plan_id = auto_plan_id(desync.plan)
        if crec then
            crec.plan_id = plan_id
        end
    end

    local askey = (is_udp and "auto_strategy_udp" or "auto_strategy") .. "#" .. plan_id
    local hrec = auto_host_record(desync, askey)
    if not hrec then
        return
    end

    if not hrec.ctstrategy then
        local uniq = {}
        local n
        for i, instance in pairs(desync.plan) do
            if instance.arg.strategy then
                n = tonumber(instance.arg.strategy)

                if not n or n < 1 then
                    error("circular: strategy number '" .. tostring(instance.arg.strategy) .. "' is invalid")
                end

                uniq[tonumber(instance.arg.strategy)] = true

                if instance.arg.final then
                    hrec.final = n
                end
            end
        end

        n = 0
        for i, v in pairs(uniq) do
            n = n + 1
        end

        if n ~= #uniq then
            error("circular: strategies numbers must start from 1 and increment. gaps are not allowed.")
        end
        hrec.ctstrategy = n
        -- the set of strategy numbers that exist in THIS plan; strategy
        -- numbers from other plans must never be applied here
        hrec.strategy_set = uniq
    end

    if not hrec.nstrategy then
        -- start=N skips the direct phase (arg from the desync line): the
        -- profile's traffic is known to be blocked, e.g. voice media over TCP
        -- 8443 that DPI throttles by SNI. The number is kept as the record's
        -- floor, so the exhaustion path below never parks the host on a lower
        -- strategy. A no_direct= zone is the same floor keyed by hostname: the
        -- filter says its hosts are blocked for everyone, so the ladder starts
        -- at least at strategy 1 and never returns to direct.
        local start = tonumber(desync.arg.start) or 0
        if hrec.no_direct and start < 1 then
            start = 1
        end
        hrec.start = start
        hrec.nstrategy = start
    end

    if hrec.ctstrategy == 0 then
        return
    end

    local verdict = VERDICT_PASS
    local arg = args_defaults(desync.arg)

    -- Post-exhaustion cooldown: a full sweep proved nothing, so new failures
    -- must not restart it immediately. Success paths stay alive: a parked
    -- strategy that actually works can still confirm the host and send VALID.
    if hrec.t_exhausted then
        if os.time() - hrec.t_exhausted < (tonumber(arg.time) or 60) then
            arg.fails = nil
        else
            hrec.t_exhausted = nil
            hrec.tried = {}
        end
    end

    local host_name = desync.track and desync.track.hostname

    if crec then
        local host_or_ip = host_or_ip(desync)

        local dport = tostring(desync.dis.tcp and desync.dis.tcp.th_dport or desync.dis.udp and desync.dis.udp.uh_dport)

        -- Strategy name of this packet. Nothing below logs after a switch:
        -- auto_do_switch logs the new name itself.
        local name = auto_strategy_name(hrec)

        DLOG("auto_strategy: " .. auto_strategy_label(is_udp, askey, hrec.nstrategy) .. "->" .. host_or_ip .. ":" .. dport)

        if desync.dis.tcp then
            -- Downstream goodput account first: the server-side byte counter
            -- covers the whole flow, so skipped packets lose nothing.
            local now = os.time()
            auto_throttle_account(crec, desync, now)

            -- Flow class: a streaming host's flow is judged by goodput only
            -- once it really streamed. The helper and the app's test probe
            -- rr*.googlevideo.com with /videoplayback?expire=1 and get an
            -- "access denied" page, so such a flow never carries video volume:
            -- judging it by speed failed a working strategy over a 403. The
            -- class is fixed once the flow crosses the volume mark or the
            -- server closes it; until then the flow is not judged by speed.
            local srv_bytes = desync.track.pos.server and desync.track.pos.server.pbcounter or 0
            if not crec.streamed and srv_bytes >= arg.stream_min_bytes then
                crec.streamed = true
            end
            if bitand(desync.dis.tcp.th_flags, TH_FIN) ~= 0 then
                crec.closed = true
            end

            -- Positive evidence: the server answered. An outgoing ClientHello
            -- proves nothing (DPI lets the request out and kills the reply),
            -- so only an incoming handshake/HTTP reply may confirm the
            -- strategy for the helper. Data beyond the handshake (inseq 4096,
            -- the zapret2 success gate) is what lets retransmissions be
            -- treated as ordinary loss instead of a DPI cut.
            if not desync.outgoing and plen > 0 then
                if desync.l7payload == "tls_server_hello" or desync.l7payload == "http_reply" then
                    crec.handshake_ok = true
                    hrec.reachable = true
                    if not crec.hs_srv then
                        crec.hs_srv = srv_bytes
                    end
                end
                -- Application data after the client's request: the handshake
                -- itself does not count. A TLS ServerHello (and the certificate
                -- that follows it) is not a response, so a connection whose
                -- server only shook hands and then went quiet is NOT a working
                -- one. For plain HTTP the reply is the application data itself.
                --
                -- A request that left before the handshake completed never sets
                -- app_req_srv (the outgoing-payload branch below requires
                -- handshake_ok), and then 4xx/5xx pages and video segments alike
                -- looked like "no application data" and were cut by the
                -- post-handshake silence rule. Server bytes well past the
                -- handshake are application data by themselves.
                if desync.l7payload == "http_reply" or (crec.app_req_srv and srv_bytes > crec.app_req_srv) then
                    crec.app_seen = true
                end
                if crec.handshake_ok and not crec.app_seen and srv_bytes > (crec.hs_srv or srv_bytes) + 16384 then
                    crec.app_seen = true
                end
                if pos_get(desync, 's') > 4096 then
                    crec.data_ok = true
                end
            end

            -- Local error evidence. Retransmission of a packet that carries
            -- data, before any real downstream data, means the handshake is
            -- stuck (DPI dropped it). The payload gate keeps pure ACKs and
            -- SYN retransmits out; the maxseq gate keeps ordinary loss deep in
            -- the stream out. A connection the server already answered is a
            -- live dialog, not a stuck handshake: its retransmissions are
            -- ordinary loss (small flows like voice or API calls never reach
            -- the 4096-byte data gate, so without this every hiccup read as a
            -- DPI cut and rotated the strategy under a working connection).
            -- A stall after the handshake belongs to the throttle path.
            local seq = pos_get(desync, 's')
            if desync.outgoing and plen > 0 and is_retransmission(desync) and not crec.data_ok and not crec.handshake_ok
                and seq > 0 and seq <= arg.maxseq then
                auto_reset_connection(desync, arg, name, host_or_ip, dport)
                auto_request_check(host_name, hrec)

                if auto_check_fails(hrec, arg, crec, not hrec.had_success) then
                    auto_fail_helper_strategy(auto_strategy_label(false, askey, hrec.nstrategy), host_name)
                    auto_do_switch(hrec, false, askey, "is_retransmission", host_or_ip, dport, true, crec)
                end

                return auto_strategy_plan(desync, hrec, verdict)
            end

            -- RST early in the handshake (seq 1..8192) — DPI cut the
            -- connection. Incoming only: a client RST is a local abort, not a
            -- DPI cut. Only when the server never answered: a close RST after
            -- a real response is normal, not a cut.
            if not desync.outgoing and bitand(desync.dis.tcp.th_flags, TH_RST) ~= 0 and seq >= 1 and seq <= 8192
                and not crec.handshake_ok then
                auto_reset_connection(desync, arg, name, host_or_ip, dport)
                auto_request_check(host_name, hrec)

                if auto_check_fails(hrec, arg, crec, not hrec.had_success) then
                    auto_fail_helper_strategy(auto_strategy_label(false, askey, hrec.nstrategy), host_name)
                    auto_do_switch(hrec, false, askey, "RST", host_or_ip, dport, true, crec)
                end

                return auto_strategy_plan(desync, hrec, verdict)
            end

            -- DPI redirect: the reply points at a foreign host. Runs for every
            -- connection whose request carried a hostname.
            if desync.l7payload == "http_reply" and desync.track and desync.track.hostname then
                local hdis = http_dissect_reply(desync.dis.payload)
                if hdis and (hdis.code == 302 or hdis.code == 307) then
                    local idx_loc = array_field_search(hdis.headers, "header_low", "location")
                    if idx_loc and is_dpi_redirect(desync.track.hostname, hdis.headers[idx_loc].value) then
                        auto_reset_connection(desync, arg, name, host_or_ip, dport)
                        auto_request_check(host_name, hrec)

                        if auto_check_fails(hrec, arg, crec, not hrec.had_success) then
                            auto_fail_helper_strategy(auto_strategy_label(false, askey, hrec.nstrategy), host_name)
                            auto_do_switch(hrec, false, askey, "DPI_redirect", host_or_ip, dport, true, crec)
                        end

                        return auto_strategy_plan(desync, hrec, verdict)
                    end
                end
            end

            -- Throttling-after-handshake, streaming hosts only, and only on a
            -- flow that really streamed: the DPI lets the handshake through
            -- and then chokes the data path. Two bad windows in a row (see
            -- auto_throttle_judge) fail the strategy; a healthy window is the
            -- real success for a video stream. Ordinary TCP (long-poll,
            -- websocket, API) and probe flows are low-rate by design and must
            -- not be judged by bitrate: their verdict is the handshake below.
            if hrec.stream and crec.streamed then
                local rate = auto_throttle_rate(crec, arg, now)
                if rate then
                    local fire, bad, collapse, alt = auto_throttle_judge(hrec, crec, arg, rate)
                    if fire then
                        -- alt is the alternative's measured goodput the window
                        -- was compared against; 0 means the absolute-floor
                        -- fallback fired instead.
                        ULOG("WARNING",
                            "zapret:auto_strategy: THROTTLE " .. auto_strategy_label(is_udp, askey, hrec.nstrategy) .. " " ..
                                math.floor(rate) .. "B/s peak=" .. math.floor(crec.t_peak or 0) .. " alt=" ..
                                math.floor(alt or 0) .. "->" .. host_or_ip .. ":" .. dport)
                        auto_request_check(host_name, hrec)

                        -- A stream cut to a crawl is dead now, not one failure
                        -- to accumulate until the threshold: the counter is
                        -- cleared by every new connection's burst (the player
                        -- reconnects, two healthy windows confirm the same
                        -- strategy, the streak resets), so a family that keeps
                        -- collapsing would never move. A plateau above the
                        -- collapse floor never reaches this branch, and the
                        -- verdict already costs throttle_windows bad windows.
                        --
                        -- Kill the stalled connection only when the strategy
                        -- actually changes: an intermediate fire under the
                        -- fails threshold leaves the flow alone.
                        local decisive = collapse
                        if auto_check_fails(hrec, arg, crec, decisive or not hrec.had_success) then
                            auto_reset_connection(desync, arg, name, host_or_ip, dport, true)
                            auto_fail_helper_strategy(auto_strategy_label(false, askey, hrec.nstrategy), host_name)
                            auto_do_switch(hrec, false, askey, "THROTTLE", host_or_ip, dport,
                                true, crec)
                        end

                        return auto_strategy_plan(desync, hrec, verdict)
                    end

                    if not bad then
                        auto_confirm_goodput(hrec, crec, arg, rate, false, askey)
                    end
                end
            end

            -- Stall after the handshake, on a flow too small for the volume
            -- judges: the DPI let the handshake through and then dropped the
            -- dialog. The client proves it is still there by retransmitting its
            -- request - an unanswered packet repeats - while the server has been
            -- quiet since its last byte. This is the TCP twin of the QUIC
            -- silence rule below and the only evidence a small flow has:
            -- post-handshake retransmissions are deliberately ordinary loss,
            -- and speed needs the 200 KB a request never carries.
            -- www.discordapp.com is the case: the handshake completes, the
            -- answer never comes, and the strategy looked confirmed while the
            -- app hung on "starting". A request that is merely held (long-poll)
            -- was ACKed and is not retransmitted, so it does not trip this rule.
            if desync.outgoing and plen > 0 then
                crec.tcp_client_pkts = (crec.tcp_client_pkts or 0) + 1
                if crec.handshake_ok and not crec.app_req_srv then
                    -- First application request after the handshake: the server
                    -- bytes seen so far are handshake bytes, so anything above
                    -- them later is a real response.
                    crec.app_req_srv = srv_bytes
                    crec.app_req_time = now
                end
                if crec.handshake_ok and is_retransmission(desync) then
                    crec.tcp_stalled = true
                end
            end

            if srv_bytes > (crec.tcp_srv_seen or 0) then
                crec.tcp_srv_seen = srv_bytes
                crec.tcp_srv_time = now
                crec.tcp_stalled = nil
                -- The server moved: whatever the client was repeating got
                -- through, so the quiet run is over.
                crec.tcp_quiet_windows = 0
                crec.tcp_quiet_client = crec.tcp_client_pkts
            end

            local tcp_silence = tonumber(arg.tcp_silence) or 0

            -- Request sent, no application response: the strategy is dead even
            -- though the handshake completed. The old rule needed a
            -- retransmission to see this, but a server that ACKs the request
            -- and then says nothing gets no retransmission, and the handshake
            -- confirmation kept clearing the failure counter - the app hung for
            -- minutes on a strategy that was "working". One verdict per
            -- connection, and only while no application data arrived: a held
            -- long-poll that got its headers is already app_seen and never
            -- trips this, and a late answer wipes the streak via success. The
            -- window is doubled: a merely slow server (seconds to the first
            -- byte) must not look like a cut path.
            -- Streaming hosts are judged by goodput and the throttle/silence
            -- rules of their own: a video connection may deliver a small
            -- init/manifest and go quiet while the player thinks, and cutting it
            -- here killed a working session (field log: SILENCE TCP on a video
            -- host after 5.6 KB). A server-closed flow is over by definition and
            -- is not evidence of a dead strategy either.
            if tcp_silence > 0 and crec.handshake_ok and not crec.app_seen and crec.app_req_time
                and not hrec.voice_group and not hrec.stream and not crec.closed
                and not crec.stall_verdict and now - crec.app_req_time >= tcp_silence * 2 then
                crec.stall_verdict = true

                ULOG("WARNING",
                    "zapret:auto_strategy: SILENCE TCP " .. auto_strategy_label(false, askey, hrec.nstrategy) .. "->" ..
                        host_or_ip .. ":" .. dport .. " srv=" .. srv_bytes)

                -- Two stalls in the fails window on DIRECT: switch right now,
                -- do not wait for the full fails threshold. One is tolerated
                -- (a slow server), two is a pattern, and a success on direct no
                -- longer hides it - direct_ok goes too, so the next candidate
                -- list does not lead with it again. Desync strategies keep the
                -- normal fails counter: a working one is not churned on a
                -- merely slow link.
                local decisive = auto_direct_stall_decisive(hrec, arg, now)

                if auto_check_fails(hrec, arg, crec, decisive or not hrec.had_success) then
                    auto_reset_connection(desync, arg, name, host_or_ip, dport, true)
                    auto_fail_helper_strategy(auto_strategy_label(false, askey, hrec.nstrategy), host_name)
                    auto_do_switch(hrec, false, askey, "SILENCE", host_or_ip, dport, false, crec)
                end

                return auto_strategy_plan(desync, hrec, verdict)
            end

            if tcp_silence > 0 and crec.tcp_stalled and crec.tcp_srv_time and not crec.streamed and not crec.closed
                and not hrec.voice_group and now - crec.tcp_srv_time >= tcp_silence then
                crec.tcp_srv_time = now
                crec.tcp_stalled = nil

                -- One quiet window is not a verdict. Only a run of them with
                -- the client pushing through every one counts: tcp_silence_windows
                -- full windows of client packets against a mute server. A lull
                -- (player between segments, loaded page, held request) sends
                -- nothing, so its window breaks the run instead of ending the
                -- connection.
                --
                -- A streaming family reacts on the first window, but only once
                -- the flow really streamed: a video host that merely delivered a
                -- small init/manifest response and went quiet is not a stopped
                -- playback, and one lost packet plus a five-second lull used to
                -- end it. Until the volume mark is crossed the ordinary window
                -- count applies (the player's manifest calls are exactly those
                -- small flows).
                local needed = (hrec.stream and crec.streamed) and 1 or (tonumber(arg.tcp_silence_windows) or c_tcp_silence_windows)
                local quiet = (crec.tcp_quiet_client or 0) < (crec.tcp_client_pkts or 0)
                crec.tcp_quiet_client = crec.tcp_client_pkts
                crec.tcp_quiet_windows = quiet and ((crec.tcp_quiet_windows or 0) + 1) or 0

                if crec.tcp_quiet_windows >= needed then
                    crec.tcp_quiet_windows = 0

                    ULOG("WARNING",
                        "zapret:auto_strategy: SILENCE TCP " .. auto_strategy_label(false, askey, hrec.nstrategy) .. "->" ..
                            host_or_ip .. ":" .. dport .. " srv=" .. srv_bytes)

                    local decisive = auto_direct_stall_decisive(hrec, arg, now)

                    if auto_check_fails(hrec, arg, crec, decisive or not hrec.had_success) then
                        auto_reset_connection(desync, arg, name, host_or_ip, dport, true)
                        auto_fail_helper_strategy(auto_strategy_label(false, askey, hrec.nstrategy), host_name)
                        auto_do_switch(hrec, false, askey, "SILENCE", host_or_ip, dport, false, crec)
                    end

                    return auto_strategy_plan(desync, hrec, verdict)
                end
            end

            -- Confirm once per connection: the server answered and the failure
            -- branches above did not fire. A handshake alone confirms only a
            -- non-streaming host, plus a probe flow: for googlevideo the
            -- goodput window above owns the success (otherwise a throttled
            -- stream would be pinned as a working strategy and VALID would hide
            -- it from the outer autopick), but a flow the server closed before
            -- it carried the volume mark (the probe above) had no data path to
            -- measure - its answer is the verdict, and that is exactly the
            -- preliminary signal the probe hosts exist for. A handshake without
            -- application data is not an answer at all: confirming it would
            -- clear the failure streak of a strategy that let the TLS through
            -- and then dropped the dialog.
            local probe = not crec.streamed and crec.closed == true
            local confirmed = crec.handshake_ok and crec.app_seen and (not hrec.stream or crec.goodput_ok or probe)
            if host_name and confirmed and not crec.valid_sent then
                crec.valid_sent = true
                send_signal("VALID", host_name, auto_strategy_label(false, askey, hrec.nstrategy), 10000)
                ULOG("OK",
            "zapret:auto_strategy: CONFIRMED TCP " .. auto_strategy_label(false, askey, hrec.nstrategy) .. "->" ..
                host_or_ip .. ":" .. dport)

                if crec.rated_n ~= hrec.nstrategy and hrec.nstrategy ~= 0 then
                    auto_rate_strategy(false, askey, hrec)
                    crec.rated_n = hrec.nstrategy
                end
                auto_mark_direct(hrec)
                hrec.had_success = true
                auto_mark_success(hrec)

                -- Confirmed, but not necessarily the best: a measurably faster
                -- strategy wins the family even while this one works.
                auto_upgrade_strategy(hrec, crec, false, askey, host_or_ip, dport)
            end
        end

        if is_udp then
            -- Counters are read from client/server positions directly:
            -- pos_get direct/reverse swap depending on the current packet
            -- direction, client/server are stable.
            local pos_client = desync.track.pos.client and desync.track.pos.client.pcounter or 0
            local pos_server = desync.track.pos.server and desync.track.pos.server.pcounter or 0

            -- Long-lived flows (Discord voice, WireGuard, game UDP) must be
            -- re-judged as their state changes: a one-shot per-connection
            -- verdict freezes the strategy until reconnect, and a single
            -- fail never accumulates to the switch threshold. Verdicts fire
            -- on state transitions instead, at most once per udp_out
            -- outgoing packets (judged_client watermark in lua_state).
            local prev_server = crec.udp_judged_server or 0
            local judged_client = crec.udp_judged_client or 0

            local now = os.time()
            local silence = tonumber(arg.udp_silence) or 3

            -- Silence clock: t_server is the last time the server answered.
            -- A flow that was quiet for a whole window and then speaks again
            -- starts over (player paused, app reconnected), so the clock
            -- restarts with it: otherwise a resumed stream would be failed
            -- on its very first outgoing packet.
            if not crec.t_seen or now - crec.t_seen >= silence then
                crec.t_server = now
            end
            crec.t_seen = now
            crec.t_server = crec.t_server or now
            if pos_server > prev_server then
                crec.t_server = now
            end

            -- googlevideo and QUIC are the video transports: a throttled stream
            -- still receives server packets, so goodput is the only evidence of
            -- throttling there. Voice/game UDP keeps the packet-count verdict
            -- below: its bitrate is legitimately low, but its packet flow is
            -- steady.
            --
            -- Same flow class as on the TCP side: a QUIC flow that never
            -- carried the volume mark (an API call, a handshake-only reply) is
            -- not judged by speed either. The mark is lower for a streaming
            -- host: googlevideo over QUIC has no helper probe (curl is TCP
            -- only), so every byte there is the video, and waiting for the TCP
            -- mark left a crawling stream unjudged for tens of seconds while
            -- the player starved.
            local streaming = hrec.stream or (desync.track.l7proto == "quic")
            local srv_bytes = desync.track.pos.server and desync.track.pos.server.pbcounter or 0
            local stream_mark = arg.stream_min_bytes
            if hrec.stream then
                stream_mark = arg.udp_stream_min_bytes
            end
            if not crec.streamed and srv_bytes >= stream_mark then
                crec.streamed = true
            end
            if streaming and crec.streamed then
                auto_throttle_account(crec, desync, now)
                local rate = auto_throttle_rate(crec, arg, now)
                if rate then
                    local fire, bad, collapse, alt = auto_throttle_judge(hrec, crec, arg, rate)
                    if fire then
                        ULOG("WARNING",
                            "zapret:auto_strategy: THROTTLE-UDP " .. auto_strategy_label(true, askey, hrec.nstrategy) .. " " ..
                            math.floor(rate) .. "B/s peak=" ..
                                math.floor(crec.t_peak or 0) .. " alt=" .. math.floor(alt or 0) ..
                                "->" .. host_or_ip .. ":" .. dport)
                        -- Same as on TCP, for the video family only: a generic
                        -- QUIC flow (voice, game) is low-rate by nature and
                        -- keeps the failure counter.
                        local decisive = hrec.stream and collapse
                        if auto_check_fails(hrec, arg, crec, decisive or not hrec.had_success) then
                            auto_do_switch(hrec, true, askey, "THROTTLE-UDP", host_or_ip, dport,
                                false, crec)
                        end
                        return auto_strategy_plan(desync, hrec, verdict)
                    end

                    if not bad then
                        auto_confirm_goodput(hrec, crec, arg, rate, true, askey)
                        if crec.goodput_ok then
                            auto_upgrade_strategy(hrec, crec, true, askey, host_or_ip, dport)
                        end
                    end
                end
            end

            -- Silence verdict for streaming UDP (QUIC): a dead strategy gets
            -- no reply at all, and the packet-count verdict below would need
            -- udp_out outgoing packets that a backing-off QUIC client never
            -- reaches before it gives up and reconnects. Continuous outgoing
            -- traffic with no server reply for udp_silence seconds is enough.
            -- The outgoing watermark keeps a paused stream out (player idle,
            -- client silent): silence alone is not a failure, only silence
            -- while the client keeps talking. The cooldown matches the window
            -- so one stalled flow costs one verdict per window, not one per
            -- packet. Confirmed hosts still need their full fails threshold,
            -- which is the guard against a legitimately quiet QUIC flow.
            if streaming and now - (crec.t_server or now) >= silence
                and pos_client > (crec.udp_silence_client or 0)
                and (not crec.t_silence or now - crec.t_silence >= silence) then
                crec.t_silence = now
                crec.udp_silence_client = pos_client
                crec.udp_judged_client = pos_client
                crec.udp_judged_server = pos_server

                ULOG("WARNING",
                    "zapret:auto_strategy: SILENCE " .. auto_strategy_label(true, askey, hrec.nstrategy) .. "->" ..
                        host_or_ip .. ":" .. dport .. " out=" ..
                        pos_client .. " in=" .. pos_server)

                if auto_check_fails(hrec, arg, crec, not hrec.had_success) then
                    auto_do_switch(hrec, true, askey, "SILENCE", host_or_ip, dport, false, crec)
                    return auto_strategy_plan(desync, hrec, verdict)
                end
            end

            -- New server packets since the last verdict: the flow is alive.
            -- The watermark moves with every reply, so a silent flow is judged
            -- by the outgoing packets accumulated after the last reply. The
            -- first verdict still honours udp_in (enough server packets to
            -- call the flow alive). This branch must not swallow the failure
            -- branch: a reply received once must not freeze the strategy.
            if pos_server > prev_server and (prev_server > 0 or pos_server > arg.udp_in) then
                crec.udp_judged_server = pos_server
                crec.udp_judged_client = pos_client

                if not crec.udp_success then
                    crec.udp_success = true
                    if not streaming then
                        ULOG("OK",
            "zapret:auto_strategy: CONFIRMED UDP " .. auto_strategy_label(true, askey, hrec.nstrategy) .. "->" ..
                host_or_ip .. ":" .. dport)

                        auto_rate_strategy(true, askey, hrec)
                        auto_mark_direct(hrec)
                        hrec.had_success = true
                        crec.rated_n = hrec.nstrategy
                        auto_mark_success(hrec)
                    end
                end

                return auto_strategy_plan(desync, hrec, verdict)
            end

            -- Failure: many outgoing packets with no new server reply since
            -- the last verdict. Outgoing only, re-armed every udp_out outgoing
            -- packets, so a broken long-lived flow keeps switching strategies
            -- instead of freezing on the first verdict.
            --
            -- Not for a streaming flow: a video client ACKs its burst while the
            -- server pauses between segments, so a handful of client packets
            -- without a fresh reply is the normal shape of a download, not a
            -- dead strategy (field logs: googlevideo QUIC failed after thousands
            -- of exchanged packets, in the pause of a healthy stream). Its
            -- verdict belongs to the measured speed above and the silence rule
            -- below - both need the flow itself to go quiet.
            --
            -- Not for a settled voice flow either (udp_voice=1): the server in
            -- a quiet Discord channel sends nothing until someone speaks, so
            -- outgoing packets without a fresh reply are exactly what a working
            -- call looks like. The first server reply proved the path; after it
            -- the packet count is not a verdict. A dead strategy is visible
            -- before that reply, and a path that dies mid-call makes the client
            -- reconnect - the new flow repeats the first-reply test.
            local voice_settled = arg.udp_voice == 1 and (crec.udp_success or pos_server > 0)
            if not streaming and not voice_settled and desync.outgoing
                and pos_client >= arg.udp_out and (pos_client - judged_client) >= arg.udp_out then
                crec.udp_judged_client = pos_client
                crec.udp_judged_server = pos_server

                ULOG("WARNING",
                    "zapret:auto_strategy: FAIL UDP " .. auto_strategy_label(true, askey, hrec.nstrategy) .. "->" ..
                        host_or_ip .. ":" .. dport .. " out=" ..
                        pos_client .. " in=" .. pos_server)

                crec.udp_success = nil

                if auto_check_fails(hrec, arg, crec, not hrec.had_success) then
                    auto_do_switch(hrec, true, askey, "UDP", host_or_ip, dport, false, crec)
                end
            end

            -- Not enough evidence yet: pass with the current strategy.
            return auto_strategy_plan(desync, hrec, verdict)
        end
    end

    return auto_strategy_plan(desync, hrec, verdict)
end

function args_defaults(arg)
    return {
        fails = tonumber(arg.fails) or 3,
        time = tonumber(arg.time) or 60,
        maxseq = tonumber(arg.maxseq) or 32768,
        udp_in = tonumber(arg.udp_in) or 1,
        udp_out = tonumber(arg.udp_out) or 4,
        udp_silence = tonumber(arg.udp_silence) or 3,
        udp_voice = tonumber(arg.udp_voice) or 0,
        no_direct = arg.no_direct,
        tcp_silence = tonumber(arg.tcp_silence) or 5,
        tcp_silence_windows = tonumber(arg.tcp_silence_windows) or c_tcp_silence_windows,
        throttle_window = tonumber(arg.throttle_window) or 3,
        throttle_min_bps = tonumber(arg.throttle_min_bps) or c_throttle_min_bps,
        throttle_collapse = tonumber(arg.throttle_collapse) or 0.25,
        throttle_collapse_floor_bps =
            tonumber(arg.throttle_collapse_floor_bps) or c_throttle_collapse_floor_bps,
        throttle_windows = tonumber(arg.throttle_windows) or 2,
        stream_min_bytes = tonumber(arg.stream_min_bytes) or 200000,
        udp_stream_min_bytes = tonumber(arg.udp_stream_min_bytes) or c_udp_stream_min_bytes,
        reset = arg.reset ~= nil or false
    }
end

-- Heartbeat to the helper: "zapret is running". The helper stays dormant
-- (no worker pool, no probes, no snapshots) until the first beat, so it can
-- never probe a host through a strategy that is not filtering yet. Beats
-- repeat while winws2 lives; the helper suspends host work after a minute of
-- silence. Timers run only inside the packet loop, i.e. after interception
-- is armed, so the first beat is an honest readiness signal.
function auto_ready_beat(name, data)
    send_signal("READY", "zapret", nil, 10000)
end

timer_set("auto_ready_beat", "auto_ready_beat", 5000, false, {})

-- Throughput meter: auto_rate_account turns the conntrack byte counters into
-- per-direction deltas (see there), so the meter follows the whole flow, not
-- only the packets that reach Lua. This timer flushes both counters once a
-- second as "zapret_rate" (download|upload bytes/s) over the IPC to unblock's
-- UI, then resets the counters so the value is the last-second rate.
function auto_rate_beat(name, data)
    send_signal("LATEST", "zapret_rate", tostring(_G.zapret_rate_down or 0) .. "|" .. tostring(_G.zapret_rate_up or 0),
        9999)
    _G.zapret_rate_down = 0
    _G.zapret_rate_up = 0
end

timer_set("auto_rate_beat", "auto_rate_beat", 1000, false, {})

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
--   1. Сначала пробуем прямое соединение (direct, nstrategy = 0)
--   2. TCP: вердикты по пакетам — ретрансмиссия до первых данных, RST в
--      начале рукопожатия и DPI-redirect (http_reply 302/307). Ретрансмиссия
--      судится только до ответа сервера (crec.handshake_ok): на живом
--      соединении это обычная потеря, а не перерезанный DPI путь — голосовые
--      и API-потоки до гейта в 4096 байт серверных данных не доходят никогда,
--      и каждый их сбой уводил стратегию из-под работающего соединения (а
--      звонок Discord после этого рвётся). Работающий TCP подтверждается
--      ответом сервера (п. 3), а троттлинг после рукопожатия ловит
--      throttle-судья (conntrack-счётчик байт сервера,
--      track.pos.server.pbcounter): потоковые хосты (googlevideo) судятся по
--      скорости и на TLS, и на QUIC (п. 5), а обычный TCP (API, long-poll) —
--      нет, он низкоскоростной по природе. Одно соединение докладывает
--      провал не чаще раза в 2 секунды, счётчик fails сбрасывается по времени
--      (time) или настоящим успехом.
--   3. Положительный вердикт — входящий tls_server_hello/http_reply: он
--      подтверждает стратегию для helper (VALID) и рейтинга — для обычного TCP
--      этого достаточно. Для потоковых хостов (googlevideo) и QUIC рукопожатия
--      мало: успех ставит только здоровое окно скорости, иначе «сайт открылся,
--      видео стоит» навсегда закрепляло бы троттлящую стратегию.
--   3b. Рейтинг стратегий (в пределах плана и прогона winws2): +1 к счётчику
--      успехов стратегии. При переключении кандидаты берутся в порядке: успешные
--      (по числу успехов убыв., ничьи — по номеру), затем неотмеченные 1..N.
--      Хост, который уже срабатывал (had_success), ротируется по успешным,
--      но после c_success_cycles (2) полных кругов без успеха выходит в
--      полный перебор (сначала неотмеченные), и если и там всё исчерпано —
--      помечается EXHAUSTED.
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
--      без переподключения.
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
--   throttle_window=N — окно замера скорости даунстрима, сек (по умолч. 3)
--   throttle_min_bps=N — ниже этой скорости (Б/с) окно плохое, и такой
--                    поток не подтверждается (по умолч. 20000: под любым
--                    играбельным видео, но выше мусора — 500 Б/с проходил
--                    даже ползущий ручеёк)
--   throttle_collapse=N — доля пика соединения, ниже которой окно плохое
--                    (по умолч. 0.25; 0 — выключить проверку)
--   throttle_windows=N — сколько плохих окон подряд до провала (по умолч. 2)
--   stream_min_bytes=N — объём серверных байт, с которого пото́к считается
--                    потоковым и его можно судить по скорости (по умолч.
--                    200000). Ниже метки это проба или API-запрос: объёма у
--                    них нет по природе, и вердикт им даёт ответ сервера, а не
--                    скорость (иначе 403 на пробе валит рабочую стратегию).
--   udp_out=N      — исходящих UDP пакетов для вердикта FAIL (по умолч. 4)
--   udp_in=N       — входящих UDP пакетов: больше N — успех, иначе фейл (по умолч. 1)
--   udp_silence=N  — секунд молчания сервера при живом исходящем потоке
--                    (потоковые хосты и QUIC) до вердикта FAIL (по умолч. 3)
--   reset          — отправлять RST при ошибке
--   start=N        — начальный номер стратегии (0 = direct, по умолч. 0)
-- auto_host_group — группирует хосты одного семейства в общую стратегию.
-- googlevideo (rr*-sn-*.googlevideo.com) — все такие хосты делят один hrec:
-- подобранная стратегия применяется ко всем сразу, при сбое/FAIL переключается
-- для всей группы, а не для каждого rr-хоста отдельно.
-- Голос Discord — вся семья *.discord.media: и c-<регион>-<хэш> (хэш уникален
-- на сессию), и формы без c- вроде finland14144.discord.media. Без группировки
-- каждая новая сессия/форма начинала бы перебор заново; с группировкой
-- подобранная стратегия помнится на всю семью (ключ stream.discord.media;
-- префикс stream. отделяет голосовые поддомены от сайта discord.media).
-- Судится как обычный TCP-хост — по ответу сервера (см. auto_host_record).
function auto_host_group(hostkey)
    if hostkey and hostkey:find("googlevideo%.com$") then
        return "googlevideo.com"
    end

    if hostkey and hostkey:find("%.discord%.media$") then
        return "stream.discord.media"
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
        hostkey = auto_host_group(desync.track.hostname)
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
    -- Streaming host (googlevideo): a server answer proves the site is
    -- reachable, not that its data path delivers. Such a host is confirmed
    -- only by a healthy goodput window (see auto_confirm_goodput) and judged
    -- by the throttle path on both transports - TLS and QUIC.
    rec.stream = (hostkey == "googlevideo.com")
    return rec
end

-- Fingerprint of the profile's plan: number of distinct strategy numbers in
-- it. Profiles with the same protocol but different plans (broad TLS 29
-- strategies vs voice TCP 7) get different askeys through this.
function auto_plan_id(plan)
    local uniq = {}
    local n = 0
    for _, instance in pairs(plan) do
        local s = tonumber(instance.arg.strategy)
        if s and not uniq[s] then
            uniq[s] = true
            n = n + 1
        end
    end
    return n
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

-- Real success: reset the failure episode and let the host leave the
-- enumeration ladder.
function auto_mark_success(hrec)
    hrec.fails = nil
    hrec.fails_time = nil
    hrec.success_cycles = nil
    hrec.full_enum = nil
end

-- How many full passes a known-good host may make over the successful set
-- (with no success) before it is allowed to fall back to strategies that
-- never succeeded. Reset on every success, like the fails counter.
local c_success_cycles = 2

-- Ordered candidates for the host's next strategy. Successful strategies are
-- ranked by measured throughput first (see auto_confirm_goodput), then by
-- success count (desc, ties by number asc); never-successful ones
-- follow, fewest *global* failures first: a new host must not start with a
-- strategy that just failed for every other host in this plan. Only numbers
-- present in THIS plan are considered. Already-tried numbers (rec.tried) are
-- skipped. A host that has succeeded before stays on the successful set
-- (rec.had_success) until it burns c_success_cycles full passes with no
-- success; then rec.full_enum makes it try the never-successful strategies
-- first (the proven ones only as a last resort).
function auto_strategy_candidates(rec, rating, failures)
    local proven  = {}
    local unknown = {}
    for n = 1, rec.ctstrategy do
        if rec.strategy_set and rec.strategy_set[n] and not (rec.tried and rec.tried[n]) then
            if (rating[n] or 0) > 0 then
                proven[#proven + 1] = n
            else
                unknown[#unknown + 1] = n
            end
        end
    end

    -- Proven strategies: the measured goodput wins first, so a strategy that
    -- really streamed is preferred over one that only answered the handshake.
    -- Ties fall back to the success count and then to the number.
    table.sort(proven, function(a, b)
        local ra = (rec.rate and rec.rate[a]) or 0
        local rb = (rec.rate and rec.rate[b]) or 0
        if ra ~= rb then
            return ra > rb
        end
        if rating[a] ~= rating[b] then
            return rating[a] > rating[b]
        end
        return a < b
    end)

    table.sort(unknown, function(a, b)
        local fa = failures and failures[a] or 0
        local fb = failures and failures[b] or 0
        if fa ~= fb then
            return fa < fb
        end
        return a < b
    end)

    -- A host whose only success was on 'direct' has no rated strategy (the
    -- rating is never recorded for nstrategy 0), so the successful set is
    -- empty. Falling through to the normal enumeration keeps such a host
    -- rotating: returning the empty set would trap it on direct forever,
    -- because the no-candidates branch below resets the success-cycle ladder
    -- on every call and full_enum can never complete.
    if rec.had_success and not rec.full_enum and #proven > 0 then
        return proven
    end

    local first, second
    if rec.full_enum then
        first, second = unknown, proven
    else
        first, second = proven, unknown
    end

    local out = {}
    for _, n in ipairs(first) do
        out[#out + 1] = n
    end
    for _, n in ipairs(second) do
        out[#out + 1] = n
    end

    return out
end

-- Best known strategy of the host: highest measured goodput first, then the
-- highest rating. Used to park the host somewhere sane after a full sweep
-- instead of dropping it to direct.
function auto_best_strategy(rec, rating)
    local best, best_rate = nil, 0
    if rec.rate then
        for n, v in pairs(rec.rate) do
            if rec.strategy_set and rec.strategy_set[n] and v > best_rate then
                best, best_rate = n, v
            end
        end
    end
    if best then
        return best
    end

    local best_n, best_count = nil, 0
    for n, v in pairs(rating or {}) do
        if rec.strategy_set and rec.strategy_set[n] and v > best_count then
            best_n, best_count = n, v
        end
    end
    return best_n
end

-- Rotate to the next strategy for the record of the current protocol.
-- send_exhausted: TCP reports a fully-tried host to the HELPER only
-- (EXHAUSTED to port 10000 — the spam channel). The helper owns the
-- dead-host state and relays it to unblock; there is deliberately no
-- direct signal to unblock. UDP never sends it: UDP exhaustion says
-- nothing about TCP reachability of the host.
function auto_do_switch(rec, is_udp, askey, reason, peer, dport, send_exhausted, crec)
    ULOG("WARNING",
        "zapret:auto_strategy: FAIL " .. auto_strategy_name(rec) .. " " .. reason .. "->" .. peer .. ":" .. dport)

    local rating = auto_strategy_rating(is_udp, askey)
    local failures = auto_strategy_failures(is_udp, askey)

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
        crec.rated_n = nil
        crec.goodput_ok = nil
        crec.udp_success = nil
        -- Silence state: the new strategy gets a full window before it can
        -- be failed for silence, and the paused-flow guard starts over.
        crec.t_server = nil
        crec.t_seen = nil
        crec.t_silence = nil
        crec.udp_silence_client = nil
    end

    if not rec.tried then
        rec.tried = {}
    end
    if rec.nstrategy and rec.nstrategy ~= 0 then
        rec.tried[rec.nstrategy] = true
        -- Shared across hosts of the plan: new hosts skip the strategies that
        -- already failed elsewhere, so a dead plan is not re-walked from 1.
        failures[rec.nstrategy] = (failures[rec.nstrategy] or 0) + 1
    end

    local candidates = auto_strategy_candidates(rec, rating, failures)

    if #candidates == 0 then
        if rec.had_success and not rec.full_enum then
            -- Known-good host: one full pass over the successful set done
            -- with no success. After c_success_cycles such passes, stop
            -- restricting to the successful set and try every strategy.
            rec.success_cycles = (rec.success_cycles or 0) + 1
            rec.tried = {}
            if rec.success_cycles >= c_success_cycles then
                rec.full_enum = true
            end
            candidates = auto_strategy_candidates(rec, rating, failures)
        end

        if #candidates == 0 then
            -- Every strategy in the plan tried (full enumeration): the host
            -- has no working strategy. Park it on the best measured one and
            -- cool down: an immediate new sweep only churns the network.
            rec.nstrategy = auto_best_strategy(rec, rating) or 0
            if send_exhausted then
                send_signal("EXHAUSTED", peer, auto_strategy_name(rec), 10000)
            end
            rec.tried = {}
            rec.full_enum = nil
            rec.success_cycles = nil
            rec.t_exhausted = os.time()
            return
        end
    end

    rec.nstrategy = candidates[1]
end

-- Per-plan rating of strategies: number -> success count. Keyed by askey
-- (protocol + plan fingerprint), because strategy numbers mean different
-- things in different profiles: a win in the broad TLS plan must not mark
-- the same number as proven in the voice TCP plan. A success is counted when
-- the strategy is really confirmed (server answer / healthy goodput), once
-- per connection and strategy. In-run only: a winws2 restart clears it.
function auto_strategy_rating(is_udp, askey)
    local key = (is_udp and "udp_strategy_success" or "strategy_success") .. "#" .. tostring(askey or "default")
    _G[key] = _G[key] or {}
    return _G[key]
end

-- Per-plan failure scoreboard: number -> failed verdicts across all hosts.
-- Ordering hint only: a new host picks the strategies that have not failed
-- elsewhere first, instead of re-walking a dead plan from strategy 1.
function auto_strategy_failures(is_udp, askey)
    local key = (is_udp and "udp_strategy_failures" or "strategy_failures") .. "#" .. tostring(askey or "default")
    _G[key] = _G[key] or {}
    return _G[key]
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

-- Goodput floor for a video host, B/s. Below it the flow is not really
-- working: a strategy that only lets a crawl through must not be confirmed
-- (VALID) and must not outrank one that really delivered. 20 KB/s is under
-- any playable stream, but far above the old 500 B/s - a crawl used to count
-- as a healthy window, so a host could be certified as working while its
-- video never played. Tunable per plan with throttle_min_bps.
local c_throttle_min_bps = 20000

-- Per-connection downstream goodput (throttling-after-handshake). The
-- conntrack counter is cumulative for the whole flow, so a packet that never
-- reaches Lua (another profile's filter, a skipped block) does not lose its
-- bytes. Server-side counter: video data is what matters here.
function auto_throttle_account(crec, desync, now)
    local srv = desync.track and desync.track.pos.server
    if not srv or not srv.pbcounter then
        return
    end

    local bytes = srv.pbcounter
    if not crec.t_window_start then
        crec.t_window_start = now
        crec.t_window_base = bytes
        crec.t_window_bytes = 0
    elseif bytes >= crec.t_window_base then
        crec.t_window_bytes = bytes - crec.t_window_base
    else
        -- counter wrapped or reset, restart the window
        crec.t_window_base = bytes
        crec.t_window_bytes = 0
    end
end

-- Goodput (B/s) over the finished window, nil while the window is still
-- open. Three kinds of window are not a measurement: the still-open one, an
-- idle one before the connection ever delivered, and a long gap (bytes
-- accumulated over a minute of silence are not a throughput measurement).
--
-- An idle window after a healthy phase is evidence: the server stops sending
-- while the client keeps the window closing (ACKs, retransmissions). That is
-- a stall - the DPI cut the data path - and the old code threw it away, so a
-- video that stopped mid-stream was never judged and the strategy stayed.
-- A paused player sends nothing at all, so no window closes: it lands in the
-- long-gap branch instead of here.
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
    crec.t_window_start = now
    crec.t_window_base = (crec.t_window_base or 0) + bytes
    crec.t_window_bytes = 0

    if elapsed > window * 4 then
        crec.t_bad = 0
        return nil
    end

    if bytes == 0 then
        local healthy = (crec.t_peak or 0) >= (tonumber(arg.throttle_min_bps) or c_throttle_min_bps)
        if not healthy or elapsed > window * 2 then
            crec.t_bad = 0
            return nil
        end
        -- Stall: 0 B/s, counted as a bad window below.
        return 0
    end

    return bytes / elapsed
end

-- Throttle verdict over rate windows of one connection. A window is bad when
-- it is below the absolute floor or collapsed against the best window this
-- connection has seen (DPI throttling arrives as a sharp drop after a fast
-- start, not as a uniformly slow link). The peak is per connection, so a
-- legitimately lower bitrate in a new connection is not mistaken for a
-- collapse. Returns true when the switch threshold is reached; the streak
-- restarts then, so one verdict costs throttle_windows bad windows.
function auto_throttle_judge(crec, arg, rate)
    local peak = crec.t_peak or 0
    local min = tonumber(arg.throttle_min_bps) or c_throttle_min_bps
    local ratio = tonumber(arg.throttle_collapse) or 0

    local bad = rate < min or (ratio > 0 and peak > 0 and rate < peak * ratio)
    if rate > peak then
        crec.t_peak = rate
    end

    if not bad then
        crec.t_bad = 0
        return false, false
    end
    -- A bad window breaks the healthy streak: the success confirmation must
    -- see two good windows in a row, otherwise a strategy that is collapsing
    -- right now would still be rated (and VALID sent).
    crec.t_good = 0
    crec.t_bad = (crec.t_bad or 0) + 1
    if crec.t_bad >= (tonumber(arg.throttle_windows) or 2) then
        crec.t_bad = 0
        return true, true
    end
    return false, true
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
        return
    end

    crec.rated_n = hrec.nstrategy
    local rating = auto_strategy_rating(is_udp, askey)
    rating[hrec.nstrategy] = (rating[hrec.nstrategy] or 0) + 1

    hrec.rate = hrec.rate or {}
    if rate > (hrec.rate[hrec.nstrategy] or 0) then
        hrec.rate[hrec.nstrategy] = rate
    end
    hrec.had_success = true
    auto_mark_success(hrec)
end

function auto_strategy(ctx, desync)
    orchestrate(ctx, desync)
    if not desync.track then
        return
    end

    -- Live throughput of the traffic the applied strategies actually process,
    -- split by direction: every packet auto_strategy sees is one a desync
    -- profile handled. Raw per-packet payload (not reasm_data) so a
    -- multi-segment message is not counted once per segment. auto_rate_beat
    -- flushes both counters once a second.
    local plen = #(desync.dis.payload or "")
    if desync.outgoing then
        _G.zapret_rate_up = (_G.zapret_rate_up or 0) + plen
    else
        _G.zapret_rate_down = (_G.zapret_rate_down or 0) + plen
    end

    -- TCP and UDP run in different winws2 profiles with different strategy
    -- numbering, so the host record (and all queues) is per protocol.
    -- Profiles with different plans (e.g. broad TLS vs voice TCP) are further
    -- separated by the plan fingerprint: a shared host group (discord.media)
    -- must keep per-profile records, otherwise the record's nstrategy points
    -- to strategy numbers that do not exist in the other profile's plan and
    -- no instances are executed at all.
    local is_udp = desync.dis.udp ~= nil
    local askey = (is_udp and "auto_strategy_udp" or "auto_strategy") .. "#" .. auto_plan_id(desync.plan)
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
        -- start=N skips the direct phase (arg from the desync line).
        -- Useful for profiles where direct is known-broken, e.g. voice
        -- media over TCP 8443 that DPI throttles by SNI.
        hrec.nstrategy = tonumber(desync.arg.start) or 0
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
            hrec.full_enum = nil
            hrec.success_cycles = nil
        end
    end

    local crec = auto_conn_record(desync)

    local host_name = desync.track and desync.track.hostname

    if crec then
        local host_or_ip = host_or_ip(desync)

        local dport = tostring(desync.dis.tcp and desync.dis.tcp.th_dport or desync.dis.udp and desync.dis.udp.uh_dport)

        -- Strategy name of this packet. Nothing below logs after a switch:
        -- auto_do_switch logs the new name itself.
        local name = auto_strategy_name(hrec)

        DLOG("auto_strategy: " .. name .. "->" .. host_or_ip .. ":" .. dport)

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
                    auto_fail_helper_strategy(name, host_name)
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
                    auto_fail_helper_strategy(name, host_name)
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
                            auto_fail_helper_strategy(name, host_name)
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
                    local fire, bad = auto_throttle_judge(crec, arg, rate)
                    if fire then
                        ULOG("WARNING",
                            "zapret:auto_strategy: THROTTLE " .. name .. " " .. math.floor(rate) .. "B/s peak=" ..
                                math.floor(crec.t_peak or 0) .. "->" .. host_or_ip .. ":" .. dport)
                        auto_request_check(host_name, hrec)

                        -- Kill the stalled connection only when the strategy
                        -- actually changes: an intermediate fire under the
                        -- fails threshold leaves the flow alone.
                        if auto_check_fails(hrec, arg, crec, not hrec.had_success) then
                            auto_reset_connection(desync, arg, name, host_or_ip, dport, true)
                            auto_fail_helper_strategy(name, host_name)
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

            -- Confirm once per connection: the server answered and the failure
            -- branches above did not fire. A handshake alone confirms only a
            -- non-streaming host, plus a probe flow: for googlevideo the
            -- goodput window above owns the success (otherwise a throttled
            -- stream would be pinned as a working strategy and VALID would hide
            -- it from the outer autopick), but a flow the server closed before
            -- it carried the volume mark (the probe above) had no data path to
            -- measure - its answer is the verdict, and that is exactly the
            -- preliminary signal the probe hosts exist for.
            local probe = not crec.streamed and crec.closed == true
            local confirmed = crec.handshake_ok and (not hrec.stream or crec.goodput_ok or probe)
            if host_name and confirmed and not crec.valid_sent then
                crec.valid_sent = true
                send_signal("VALID", host_name, name, 10000)
                ULOG("OK", "zapret:auto_strategy: CONFIRMED TCP " .. name .. "->" .. host_or_ip .. ":" .. dport)

                if crec.rated_n ~= hrec.nstrategy and hrec.nstrategy ~= 0 then
                    local rating = auto_strategy_rating(false, askey)
                    rating[hrec.nstrategy] = (rating[hrec.nstrategy] or 0) + 1
                    crec.rated_n = hrec.nstrategy
                end
                hrec.had_success = true
                auto_mark_success(hrec)
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
            -- not judged by speed either.
            local streaming = hrec.stream or (desync.track.l7proto == "quic")
            local srv_bytes = desync.track.pos.server and desync.track.pos.server.pbcounter or 0
            if not crec.streamed and srv_bytes >= arg.stream_min_bytes then
                crec.streamed = true
            end
            if streaming and crec.streamed then
                auto_throttle_account(crec, desync, now)
                local rate = auto_throttle_rate(crec, arg, now)
                if rate then
                    local fire, bad = auto_throttle_judge(crec, arg, rate)
                    if fire then
                        ULOG("WARNING",
                            "zapret:auto_strategy: THROTTLE-UDP " .. name .. " " .. math.floor(rate) .. "B/s peak=" ..
                                math.floor(crec.t_peak or 0) .. "->" .. host_or_ip .. ":" .. dport)
                        if auto_check_fails(hrec, arg, crec, not hrec.had_success) then
                            auto_do_switch(hrec, true, askey, "THROTTLE-UDP", host_or_ip, dport,
                                false, crec)
                        end
                        return auto_strategy_plan(desync, hrec, verdict)
                    end

                    if not bad then
                        auto_confirm_goodput(hrec, crec, arg, rate, true, askey)
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
                    "zapret:auto_strategy: SILENCE " .. name .. "->" .. host_or_ip .. ":" .. dport .. " out=" ..
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
                        ULOG("OK", "zapret:auto_strategy: CONFIRMED UDP " .. name .. "->" .. host_or_ip .. ":" .. dport)

                        if hrec.nstrategy ~= 0 then
                            local rating = auto_strategy_rating(true, askey)
                            rating[hrec.nstrategy] = (rating[hrec.nstrategy] or 0) + 1
                        end
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
            if desync.outgoing and pos_client >= arg.udp_out and (pos_client - judged_client) >= arg.udp_out then
                crec.udp_judged_client = pos_client
                crec.udp_judged_server = pos_server

                ULOG("WARNING",
                    "zapret:auto_strategy: FAIL UDP " .. name .. "->" .. host_or_ip .. ":" .. dport .. " out=" ..
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
        throttle_window = tonumber(arg.throttle_window) or 3,
        throttle_min_bps = tonumber(arg.throttle_min_bps) or c_throttle_min_bps,
        throttle_collapse = tonumber(arg.throttle_collapse) or 0.25,
        throttle_windows = tonumber(arg.throttle_windows) or 2,
        stream_min_bytes = tonumber(arg.stream_min_bytes) or 200000,
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

-- Throughput meter: auto_strategy sums the payload of every packet it sees
-- (the desynced traffic), split into download/upload. This timer flushes both
-- once a second as "zapret_rate" (download|upload bytes/s) over the IPC to
-- unblock's UI, then resets the counters so the value is the last-second rate.
function auto_rate_beat(name, data)
    send_signal("LATEST", "zapret_rate", tostring(_G.zapret_rate_down or 0) .. "|" .. tostring(_G.zapret_rate_up or 0),
        9999)
    _G.zapret_rate_down = 0
    _G.zapret_rate_up = 0
end

timer_set("auto_rate_beat", "auto_rate_beat", 1000, false, {})

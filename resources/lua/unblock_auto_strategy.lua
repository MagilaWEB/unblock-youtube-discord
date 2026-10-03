-- auto_strategy — адаптивный перебор стратегий для обхода DPI
-- Автор: MagilaWEB (МогилныйПатчер)
--
-- Принцип: helper (zapret_helper.exe) больше не выносит вердиктов. Он —
-- насос и исполнитель: Lua — единственный судья по пакетным уликам. На
-- провале Lua просит helper сделать CHECK:<host> — свежее соединение через
-- текущую стратегию рождает новые пакеты, чтобы счётчик ошибок набрался без
-- трафика пользователя. Положительный вердикт (VALID) рождается из ответа
-- сервера, а не из helper OK.
--
-- Как работает:
--   1. Сначала пробуем прямое соединение (direct, nstrategy = 0)
--   2. TCP, host_name известен: Lua судит по пакетам — ретрансмиссия, RST
--      (seq 1..8192), payload >= 16000 (DPI16KB), DPI-redirect
--      (http_reply 302/307), throttle-thin и throttle-silence watchdog.
--      Каждый провал просит у helper CHECK:<host> (насос), копит локальный
--      счётчик fails и по порогу переключает стратегию (ERR хелперу, порт
--      10000 — спам-канал).
--   3. Положительный вердикт — по ответу сервера: входящий tls_server_hello
--      или http_reply выставляет server_responded, и только тогда шлём VALID
--      хелперу. Исходящий ClientHello не подтверждает (DPI пропускает запрос
--      и режет ответ). Throttle-after-handshake: здоровое окно
--      (rate >= throttle_min_bps) — признак восстановления. Тоньше порога —
--      evidence в общий
--      счётчик fails (THROTTLE), плюс взводится одноразовый watchdog-таймер:
--      если пакеты встанут вообще (ни капель для вердикта, нечего
--      ретранслировать — пакетный путь глух по построению), срабатывает
--      таймер (THROTTLE-SILENCE). Здоровое окно снимает watchdog.
--   3b. Рейтинг стратегий (в пределах прогона winws2): +1 к счётчику успехов
--      стратегии, когда сервер реально ответил (гейт CONFIRMED для TCP,
--      ответ сервера для UDP). При переключении кандидаты берутся в порядке:
--      успешные (по числу успехов убыв., ничьи — по номеру), затем
--      неотмеченные 1..N. Хост, который уже срабатывал (had_success),
--      ротируется по успешным, но после c_success_cycles (2) полных кругов
--      без успеха выходит в полный перебор (сначала неотмеченные), и если и
--      там всё исчерпано — помечается EXHAUSTED. Счётчик кругов сбрасывается
--      на успехе, как и счётчик fails.
--   4. host_name неизвестен (чистый IP helper проверить не может): та же
--      пакетная логика (RST, 16KB, redirect, retrans) по счётчику fails, но
--      насос CHECK не шлётся.
--   5. UDP (QUIC, STUN, WireGuard, ...) — helper не умеет проверять UDP:
--      никакой связи с helper, всё решает пакетная логика. Состояние своё:
--      autostate.auto_strategy_udp[host], своя счётчик-очередь fails и своя
--      очередь успехов _G.udp_strategy_success. Детект как в zapret2
--      (process_udp_fail): успех — только входящий пакет (server.pcounter >
--      udp_in), фейл — только исходящий (client.pcounter >= udp_out при
--      server.pcounter <= udp_in). Вердикты перезаряжаемые: срабатывают на
--      смене состояния, не чаще одного раза на udp_out исходящих пакетов
--      (водяной знак udp_judged_client в lua_state соединения). Долгоживущие
--      потоки (голос Discord, WireGuard) перебирают стратегии без
--      переподключения.
--   6. Все стратегии перебраны (exhausted) → прямой трафик + сигнал EXHAUSTED
--      хелперу (он владеет мёртвыми хостами и докладывает в unblock сам).
--      Хосты с had_success доходят сюда только после c_success_cycles
--      неудачных кругов по успешным и полного перебора.
--   7. Таймеры — единственный источник выполнения без пакетов:
--      timer_set/timer_del, колбэк по имени.
--
-- Перебор: успешные по рейтингу (число успехов убыв.), затем неотмеченные 1..N
-- Параметры (через --lua-desync=auto_strategy:fails=4:...):
--   fails=N        — локальных ошибок до переключения (по умолч. 3)
--   maxseq=N       — макс. seq для проверок (по умолч. 32768)
--   throttle_window=N — окно замера скорости даунстрима в сек (по умолч. 8)
--   throttle_min_bps=N — ниже этой скорости за окно — throttling (по умолч. 500)
--   udp_out=N      — исходящих UDP пакетов для вердикта FAIL (по умолч. 4)
--   udp_in=N       — входящих UDP пакетов: больше N — успех, иначе фейл (по умолч. 1)
--   reset          — отправлять RST при ошибке
--   start=N        — начальный номер стратегии (0 = direct, по умолч. 0)
-- auto_host_group — группирует хосты одного семейства в общую стратегию.
-- googlevideo (rr*-sn-*.googlevideo.com) — все такие хосты делят один hrec:
-- подобранная стратегия применяется ко всем сразу, при сбое/FAIL переключается
-- для всей группы, а не для каждого rr-хоста отдельно.
-- Голос Discord — вся семья *.discord.media: и c-<регион>-<хэш> (хэш уникален
-- на сессию), и формы без c- вроде finland14144.discord.media. Без группировки
-- каждая новая сессия/форма начинала бы перебор заново; с группировкой
-- подобранная стратегия помнится на всю семью .discord.media.
function auto_host_group(hostkey)
    if hostkey and hostkey:find("googlevideo%.com$") then
        return "googlevideo.com"
    end
    if hostkey and hostkey:find("%.discord%.media$") then
        return "discord.media"
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

    return autostate[askey][hostkey]
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

-- Per-record fails counter. Each protocol keeps its own counter. The counter
-- is not reset by time: it accumulates until it reaches the threshold (then
-- it fires and clears) or until the host is confirmed working (VALID /
-- CONFIRMED), which clears it in the success branches.
function auto_check_fails(rec, arg)
    if arg.fails then
        if not rec.fails then
            rec.fails = 0
        end

        rec.fails = rec.fails + 1

        if rec.fails >= arg.fails then
            rec.fails = nil
            return true
        end
    end
    return false
end

-- How many full passes a known-good host may make over the successful set
-- (with no success) before it is allowed to fall back to strategies that
-- never succeeded. Reset on every success, like the fails counter.
local c_success_cycles = 2

-- Ordered candidates for the host's next strategy. Successful strategies are
-- ranked by success count (desc, ties by number asc); never-successful ones
-- follow ascending. Only numbers present in THIS plan are considered
-- (successes may come from another profile). Already-tried numbers (rec.tried)
-- are skipped. A host that has succeeded before stays on the successful set
-- (rec.had_success) until it burns c_success_cycles full passes with no
-- success; then rec.full_enum makes it try the never-successful strategies
-- first (the proven ones only as a last resort).
function auto_strategy_candidates(rec, rating)
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

    table.sort(proven, function(a, b)
        if rating[a] ~= rating[b] then
            return rating[a] > rating[b]
        end
        return a < b
    end)

    if rec.had_success and not rec.full_enum then
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

-- Rotate to the next strategy for the record of the current protocol.
-- send_exhausted: TCP reports a fully-tried host to the HELPER only
-- (EXHAUSTED to port 10000 — the spam channel). The helper owns the
-- dead-host state and relays it to unblock; there is deliberately no
-- direct signal to unblock. UDP never sends it: UDP exhaustion says
-- nothing about TCP reachability of the host.
function auto_do_switch(rec, rating, reason, peer, dport, send_exhausted)
    ULOG("WARNING",
        "zapret:auto_strategy: FAIL " .. auto_strategy_name(rec) .. " " .. reason .. "->" .. peer .. ":" .. dport)

    if not rec.tried then
        rec.tried = {}
    end
    if rec.nstrategy and rec.nstrategy ~= 0 then
        rec.tried[rec.nstrategy] = true
    end

    local candidates = auto_strategy_candidates(rec, rating)

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
            candidates = auto_strategy_candidates(rec, rating)
        end

        if #candidates == 0 then
            -- Every strategy in the plan tried (full enumeration): the host
            -- has no working strategy.
            if send_exhausted then
                send_signal("EXHAUSTED", peer, auto_strategy_name(rec), 10000)
            end
            rec.nstrategy = 0
            rec.tried = {}
            rec.full_enum = nil
            rec.success_cycles = nil
            return
        end
    end

    rec.nstrategy = candidates[1]
end

-- Per-protocol rating of strategies: number -> success count. A success is
-- counted when the server actually answers (the CONFIRMED gate for TCP, the
-- server-reply branch for UDP), once per connection. In-run only: a winws2
-- restart clears it.
function auto_strategy_rating(is_udp)
    if is_udp then
        _G.udp_strategy_success = _G.udp_strategy_success or {}
        return _G.udp_strategy_success
    end
    _G.strategy_success = _G.strategy_success or {}
    return _G.strategy_success
end

-- The helper is a pump and executor, not a judge: TCP asks it to open a
-- fresh connection for the host through the current strategy (CHECK:<host>)
-- so new packets keep the local error counter filling without user traffic.
-- UDP is never sent: the helper cannot check UDP traffic.
function auto_request_check(host_name)
    if host_name then
        send_signal("CHECK", host_name, nil, 10000)
    end
end

-- TCP error report to the helper (port 10000 is the spam channel). ERR tells
-- the helper the strategy failed for this host; the verdict itself is made
-- by Lua from packet evidence.
function auto_fail_helper_strategy(name, host_name)
    if host_name then
        send_signal("ERR", host_name, name, 10000)
    end
end

-- Send RST to the retransmitter to break the long wait (TCP only).
function auto_reset_connection(desync, arg, name, peer, dport)
    if arg.reset and desync.dis.tcp and desync.outgoing then
        local seq = pos_get(desync, 's')
        if #desync.dis.payload > 0 and arg.maxseq > 0 and seq <= arg.maxseq then
            local dis = deepcopy(desync.dis)
            dis.payload = nil
            dis_reverse(dis)
            dis.tcp.th_flags = TH_RST
            dis.tcp.th_win = desync.track and desync.track.pos.reverse.tcp.winsize or 64
            dis.tcp.options = nil

            if dis.ip6 then
                dis.ip6.ip6_flow = (desync.track and desync.track.pos.reverse.ip6_flow) and
                                       desync.track.pos.reverse.ip6_flow or 0x60000000;
            end
            ULOG("INFO", "zapret:reset_connection: " .. name .. "->" .. peer .. ":" .. dport)

            rawsend_dissect(dis, {
                ifout = desync.ifin or 0
            })
        end
    end
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

-- Per-connection downstream throughput (throttling-after-handshake).
-- Retransmitted duplicates inflate the byte count, which only errs toward
-- fewer false verdicts. os.time() has 1s resolution: fine for windows of
-- several seconds catching order-of-magnitude stalls (~100 B/s vs live).
function auto_throttle_account(crec, desync, now)
    if not crec.t_window_start then
        crec.t_window_start = now
        crec.t_down_bytes = 0
    end
    crec.t_last_activity = now
    if not desync.outgoing then
        -- reassembled data when the dissector holds it, raw segment otherwise
        local payload = desync.reasm_data or desync.dis.payload
        local plen = payload and #payload or 0
        if plen > 0 then
            crec.t_down_bytes = (crec.t_down_bytes or 0) + plen
        end
    end
end

-- Downstream B/s over the last window when complete, nil otherwise. The
-- activity gate spans the whole window on purpose: real starvation arrives
-- as sparse drips (a segment every few seconds), not a steady thin stream.
-- Fully silent windows (idle) and byte-less windows (uploads, keepalives)
-- rearm without verdict; dead flows are owned by retransmission logic.
function auto_throttle_rate(crec, arg, now)
    local window = tonumber(arg.throttle_window) or 8
    local start = crec.t_window_start or now
    if now - start < window then
        return nil
    end
    local bytes = crec.t_down_bytes or 0
    crec.t_window_start = now
    crec.t_down_bytes = 0
    if bytes == 0 then
        return nil
    end
    if not crec.t_last_activity or now - crec.t_last_activity > window then
        return nil
    end
    return bytes / (now - start)
end

-- Watchdog timer name: deterministic per profile+host+port, so re-arming
-- replaces the previous timer instead of stacking them.
function auto_throttle_watch_name(askey, host, dport)
    local function clean(s)
        return tostring(s or "?"):gsub("[^%w_.-]", "_")
    end
    return "as_thr_" .. clean(askey) .. "_" .. clean(host) .. "_" .. clean(dport)
end

-- Silence watchdog: fires only when no packet refreshed the host since
-- arming. That is the full-freeze case — no drips for the packet verdict,
-- nothing outstanding to retransmit, so the packet path is deaf by design
-- (timers are the only execution source left). hrec.t_last_progress vetoes
-- stale fires after recovery; the cooldown vetoes switch bursts.
function auto_throttle_watchdog(name, data)
    local hrec = data and data.hrec
    if not hrec then
        return
    end
    local window = tonumber(data.window) or 8
    local now = os.time()
    if hrec.t_last_progress and now - hrec.t_last_progress < window then
        return
    end
    if hrec.t_watch_fired and now - hrec.t_watch_fired < 2 * window then
        return
    end
    hrec.t_watch_fired = now

    local strat_name = (hrec.nstrategy == 0) and "direct" or ("strategy_" .. tostring(hrec.nstrategy or "?"))
    ULOG("WARNING", "zapret:auto_strategy: THROTTLE-SILENCE " .. strat_name .. "->" .. tostring(data.peer) .. ":" ..
        tostring(data.dport))
    auto_request_check(data.host_name)

    if auto_check_fails(hrec, data.arg or {}) then
        auto_fail_helper_strategy(strat_name, data.host_name)
        auto_do_switch(hrec, auto_strategy_rating(false), "THROTTLE-SILENCE", tostring(data.peer), tostring(data.dport),
            true)
    end
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
            -- Throttling-after-handshake account first: every packet feeds the
            -- downstream window, and any packet proves the host alive (vetoes
            -- stale watchdog fires). Failures are judged next, the throttle
            -- verdict last.
            local now = os.time()
            auto_throttle_account(crec, desync, now)
            hrec.t_last_progress = now

            -- Positive verdict evidence: the server actually answered. An
            -- outgoing ClientHello alone proves nothing (DPI lets the request
            -- out and kills the reply), so only an incoming handshake/HTTP
            -- reply may confirm the strategy.
            local from_server = (not desync.outgoing) and
                                    (desync.l7payload == "tls_server_hello" or desync.l7payload == "http_reply")
            if from_server then
                crec.server_responded = true
            end

            -- Local error evidence. Retransmission: a frozen handshake keeps
            -- retransmitting unacked data. Once the server has answered on
            -- this connection, retransmissions are ordinary loss, not a DPI
            -- block, so they must not fail the strategy.
            if desync.outgoing and is_retransmission(desync) and not crec.server_responded then
                auto_reset_connection(desync, arg, name, host_or_ip, dport)
                auto_request_check(host_name)

                if auto_check_fails(hrec, arg) then
                    auto_fail_helper_strategy(name, host_name)
                    auto_do_switch(hrec, auto_strategy_rating(false), "is_retransmission", host_or_ip, dport, true)
                end

                return auto_strategy_plan(desync, hrec, verdict)
            end

            -- RST early in the handshake (seq 1..8192) — DPI cut the
            -- connection. Only when the server never answered: a close RST
            -- after a real response is normal, not a cut.
            local seq = pos_get(desync, 's')
            if bitand(desync.dis.tcp.th_flags, TH_RST) ~= 0 and seq >= 1 and seq <= 8192
                and not crec.server_responded then
                auto_reset_connection(desync, arg, name, host_or_ip, dport)
                -- auto_request_check(host_name)
                auto_fail_helper_strategy(name, host_name)

                if auto_check_fails(hrec, arg) then
                    auto_do_switch(hrec, auto_strategy_rating(false), "RST", host_or_ip, dport, true)
                end

                return auto_strategy_plan(desync, hrec, verdict)
            end

            -- Large downstream burst (DPI16KB) — classic DPI payload cut.
            local payload = desync.reasm_data or desync.dis.payload
            local plen = payload and #payload or 0
            if plen >= 16000 then
                auto_reset_connection(desync, arg, name, host_or_ip, dport)
                -- auto_request_check(host_name)
                auto_fail_helper_strategy(name, host_name)

                if auto_check_fails(hrec, arg) then
                    auto_do_switch(hrec, auto_strategy_rating(false), "DPI16KB", host_or_ip, dport, true)
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
                        -- auto_request_check(host_name)
                        auto_fail_helper_strategy(name, host_name)

                        if auto_check_fails(hrec, arg) then
                            auto_do_switch(hrec, auto_strategy_rating(false), "DPI_redirect", host_or_ip, dport, true)
                        end

                        return auto_strategy_plan(desync, hrec, verdict)
                    end
                end
            end

            -- Throttle verdict last: a healthy window is the positive verdict.
            -- Thin but alive downstream while the connection stands: arm the
            -- silence watchdog (if packets stop entirely only the timer can
            -- react), pump a CHECK, count the evidence. A healthy window
            -- confirms the strategy and sends VALID to the helper.
            local rate = auto_throttle_rate(crec, arg, now)
            local window = tonumber(arg.throttle_window) or 8
            local watch = auto_throttle_watch_name(askey, host_name or host_or_ip, dport)
            if rate then
                if rate < (tonumber(arg.throttle_min_bps) or 500) then
                    -- Re-arming replaces the previous timer. pcall: a timer
                    -- failure must never break the packet path.
                    pcall(timer_set, watch, "auto_throttle_watchdog", 2 * window * 1000, true, {
                        hrec = hrec,
                        host_name = host_name,
                        peer = host_or_ip,
                        dport = dport,
                        window = window,
                        arg = arg
                    })
                    auto_reset_connection(desync, arg, name, host_or_ip, dport)
                    -- auto_request_check(host_name)
                    auto_fail_helper_strategy(name, host_name)

                    if auto_check_fails(hrec, arg) then
                        auto_do_switch(hrec, auto_strategy_rating(false), "THROTTLE", host_or_ip, dport, true)
                    end

                    return auto_strategy_plan(desync, hrec, verdict)
                end

                -- Healthy window: flow recovered, drop the watchdog and
                -- confirm the current strategy from packet evidence.
                pcall(timer_del, watch)

                -- The healthy window no longer feeds the rating: a success is
                -- counted at the server-response gate below (once per
                -- connection), so the rating reflects real answers rather
                -- than a single healthy throughput window.
            end

            -- Confirm once per connection: the server answered and the
            -- failure branches above did not fire. Repeated packets must not
            -- spam VALID/CONFIRMED for the whole life of the connection.
            if host_name and crec.server_responded then
                if not crec.valid_sent then
                    crec.valid_sent = true
                    send_signal("VALID", host_name, name, 10000)
                    ULOG("OK", "zapret:auto_strategy: CONFIRMED TCP " .. name .. "->" .. host_or_ip .. ":" .. dport)

                    -- Rating: the server answered through this strategy, so it
                    -- counts as a success (once per connection). The host is now
                    -- known-good and stays inside the successful set.
                    if hrec.nstrategy ~= 0 then
                        local rating = auto_strategy_rating(false)
                        rating[hrec.nstrategy] = (rating[hrec.nstrategy] or 0) + 1
                    end
                    hrec.had_success = true
                end

                -- Working verdict: the failure episode is over, so both the
                -- fails counter and the successful-cycle counter start from
                -- zero next time (no time-based reset).
                hrec.fails = nil
                hrec.success_cycles = nil
                hrec.full_enum = nil
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
            local judged_client = crec.udp_judged_client or 0

            -- Success: the server answers. Incoming packets only, the same
            -- semantics as zapret2 process_udp_fail (server.pcounter >
            -- udp_in). Confirmed once per success streak: re-judging only
            -- happens when the server goes silent again (fail branch).
            if pos_server > arg.udp_in then
                crec.udp_judged_client = pos_client

                if not crec.udp_success then
                    crec.udp_success = true

                    ULOG("OK", "zapret:auto_strategy: CONFIRMED UDP " .. name .. "->" .. host_or_ip .. ":" .. dport)

                    if hrec.nstrategy ~= 0 then
                        local rating = auto_strategy_rating(true)
                        rating[hrec.nstrategy] = (rating[hrec.nstrategy] or 0) + 1
                    end
                    hrec.had_success = true

                    hrec.fails = nil
                    hrec.success_cycles = nil
                    hrec.full_enum = nil
                end

                return auto_strategy_plan(desync, hrec, verdict)
            end

            -- Failure: many outgoing packets with no server reply. Outgoing
            -- only, re-armed every udp_out outgoing packets, so a broken
            -- long-lived flow keeps switching strategies instead of freezing
            -- on the first verdict.
            if desync.outgoing and pos_client >= arg.udp_out and (pos_client - judged_client) >= arg.udp_out then
                crec.udp_judged_client = pos_client

                ULOG("WARNING",
                    "zapret:auto_strategy: FAIL UDP " .. name .. "->" .. host_or_ip .. ":" .. dport .. " out=" ..
                        pos_client .. " in=" .. pos_server)

                if crec.udp_success then
                    crec.udp_success = nil
                end

                if auto_check_fails(hrec, arg) then
                    auto_do_switch(hrec, auto_strategy_rating(true), "UDP", host_or_ip, dport, false)
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
        maxseq = tonumber(arg.maxseq) or 32768,
        udp_in = tonumber(arg.udp_in) or 1,
        udp_out = tonumber(arg.udp_out) or 4,
        throttle_window = tonumber(arg.throttle_window) or 8,
        throttle_min_bps = tonumber(arg.throttle_min_bps) or 500,
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

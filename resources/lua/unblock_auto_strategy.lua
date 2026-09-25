-- auto_strategy — адаптивный перебор стратегий для обхода DPI
-- Автор: MagilaWEB (МогилныйПатчер)
--
-- Как работает:
--   1. Сначала пробуем прямое соединение (direct, nstrategy = 0)
--   2. TCP, host_name известен: шлём helper только ERR (порт 10000 —
--      спам-канал), CHECK не шлём вообще. Helper обо всех узнает и
--      перепроверит по таймеру.
--   3. Helper ответил OK → пускаем с текущей стратегией, is_retransmission
--      не проверяем вообще.
--   4. Helper ответил FAIL + is_retransmission → reset + ERR, переключение
--      по локальному счётчику fails (защита от шторма пакетов).
--   5. Пакеты без retrans при висящем FAIL → ждём вердикт helper; свежий FAIL
--      копится в отдельном счётчике helper_fails (по умолч. 2, против ложных
--      коротких FAIL), switch только по нему, мимо локального счётчика.
--   6. host_name неизвестен (чистый IP helper проверить не может) → только
--      пакетная логика zapret (RST, 16KB, redirect, retrans) по счётчику fails.
--   7. UDP (QUIC, STUN, WireGuard, ...) — helper не умеет проверять UDP:
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
--   8. Все стратегии перебраны (exhausted) → прямой трафик + сигнал EXHAUSTED
--      хелперу (он владеет мёртвыми хостами и докладывает в unblock сам)
--   9. Throttling-after-handshake: даунстрим тоньше throttle_min_bps за окно
--      throttle_window → evidence в общий счётчик fails (THROTTLE), плюс
--      взводится одноразовый watchdog-таймер: если пакеты встанут вообще
--      (ни капель для вердикта, нечего ретранслировать — пакетный путь глух
--      по построению), срабатывает таймер (THROTTLE-SILENCE). Здоровое окно
--      снимает watchdog. Таймеры — единственный источник выполнения без
--      пакетов: timer_set/timer_del, колбэк по имени.
--
-- Перебор: 1, 2, 3, 4, ..., N (последовательно)
-- Параметры (через --lua-desync=auto_strategy:fails=4:...):
--   fails=N        — локальных ошибок до переключения (по умолч. 3)
--   helper_fails=N — подряд FAIL от helper до переключения (по умолч. 2)
--   time=N         — время через которое произойдет сброс ошибок (по умолч. 300 сек)
--   helper_time=N  — окно сброса счётчика helper (по умолч. = time)
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

-- Per-record fails counter queue. Each protocol keeps its own counter;
-- counting is reset when the previous failure is older than arg.time.
function auto_check_fails(rec, arg)
    if arg.fails then
        local now = os.time()
        if rec.last_fail_time and now - rec.last_fail_time > (tonumber(arg.time) or 60) then
            rec.fails = nil
        end

        if not rec.fails then
            rec.fails = 0
        end

        rec.fails = rec.fails + 1
        rec.last_fail_time = now

        if rec.fails >= arg.fails then
            rec.fails = nil
            return true
        end
    end
    return false
end

-- true when rec.nstrategy is not in the confirmed success queue yet.
function auto_check_valid_strategy(rec, success_list)
    for _, nstrategy in ipairs(success_list or {}) do
        if nstrategy == rec.nstrategy then
            return false
        end
    end

    return true
end

-- Rotate to the next strategy for the record of the current protocol.
-- send_exhausted: TCP reports a fully-tried host to the HELPER only
-- (EXHAUSTED to port 10000 — the spam channel). The helper owns the
-- dead-host state and relays it to unblock; there is deliberately no
-- direct signal to unblock. UDP never sends it: UDP exhaustion says
-- nothing about TCP reachability of the host.
function auto_do_switch(rec, success_list, reason, peer, dport, send_exhausted)
    ULOG("WARNING", "zapret:auto_strategy: FAIL " .. auto_strategy_name(rec) .. " " .. reason .. "->" .. peer ..
        ":" .. dport)

    if rec.strategy_success_fail then
        if rec.nstrategy < rec.ctstrategy then
            rec.nstrategy = rec.nstrategy + 1

            -- climb stays inside this plan's numbers: a number above
            -- ctstrategy belongs to another profile and executes nothing
            while rec.nstrategy < rec.ctstrategy and not auto_check_valid_strategy(rec, success_list) do
                rec.nstrategy = rec.nstrategy + 1
            end
        else
            if send_exhausted then
                send_signal("EXHAUSTED", peer, auto_strategy_name(rec), 10000)
            end
            rec.nstrategy = 0
            rec.sstrategy = 1
            rec.strategy_success_fail = false
        end

        return
    end

    if not rec.sstrategy then
        rec.sstrategy = 1
    end

    -- Apply only strategy numbers that exist in THIS plan's strategy set:
    -- success entries were confirmed by possibly another profile (broad TLS
    -- vs voice TCP), and applying a foreign number here would execute no
    -- instances at all.
    while rec.sstrategy <= #success_list do
        local n = success_list[rec.sstrategy] or 0
        if rec.strategy_set and rec.strategy_set[n] then
            break
        end
        rec.sstrategy = rec.sstrategy + 1
    end

    local count_strategy_success = #success_list
    if (count_strategy_success == 0) or (count_strategy_success < rec.sstrategy) then
        rec.strategy_success_fail = true
        rec.sstrategy = 1
        rec.nstrategy = 1
        return
    end

    rec.nstrategy = success_list[rec.sstrategy]
    rec.sstrategy = rec.sstrategy + 1
end

-- Per-protocol queue of strategies confirmed to work: TCP fills it from
-- helper OK verdicts, UDP from connections with a server reply.
function auto_strategy_success_list(is_udp)
    if is_udp then
        _G.udp_strategy_success = _G.udp_strategy_success or {}
        return _G.udp_strategy_success
    end
    _G.strategy_success = _G.strategy_success or {}
    return _G.strategy_success
end

-- Separate counter for helper FAIL verdicts (TCP only). The helper may
-- briefly report a working host as broken, so a single FAIL never switches:
-- only helper_fails consecutive fresh verdicts do.
function auto_check_helper_fails(rec, arg)
    local threshold = arg.helper_fails or 2
    if threshold then
        local now = os.time()
        local window = tonumber(arg.helper_time) or (tonumber(arg.time) or 60)
        if rec.helper_last_fail_time and now - rec.helper_last_fail_time > window then
            rec.helper_fails = nil
        end

        if not rec.helper_fails then
            rec.helper_fails = 0
        end

        rec.helper_fails = rec.helper_fails + 1
        rec.helper_last_fail_time = now

        if rec.helper_fails >= threshold then
            rec.helper_fails = nil
            return true
        end
    end
    return false
end

-- Only TCP talks to the helper: only ERR is sent (port 10000 is the spam
-- channel). CHECK is never sent: the helper learns every host from ERR/VALID
-- and rechecks everyone by its own timer. UDP is never sent: the helper
-- cannot check UDP traffic.
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
    ULOG("WARNING", "zapret:auto_strategy: THROTTLE-SILENCE " .. strat_name .. "->" .. tostring(data.peer) .. ":" .. tostring(data.dport))
    auto_fail_helper_strategy(strat_name, data.host_name)

    if auto_check_fails(hrec, data.arg or {}) then
        auto_do_switch(hrec, auto_strategy_success_list(false), "THROTTLE-SILENCE", tostring(data.peer), tostring(data.dport), true)
        -- Same-episode guard, see THROTTLE.
        hrec.helper_fails = nil
        if data.host_name and _G.helper_check then
            _G.helper_check[data.host_name] = true
        end
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
            -- Helper verdict storage (zcheck fills it). TCP only: the UDP
            -- handler never talks to the helper and never consumes its
            -- verdicts.
            if not _G.zapret_ipc then
                _G.zapret_ipc = {}
            end

            if not _G.helper_check then
                _G.helper_check = {}
            end

            -- Throttling-after-handshake: thin but alive downstream while the
            -- connection stands. Runs before the helper-OK fast path so it
            -- can dethrone a locked strategy (e.g. direct) whose body is
            -- starved. Debounced by the shared fails counter like every
            -- other local evidence.
            local now = os.time()
            auto_throttle_account(crec, desync, now)
            -- Any packet proves the host alive: vetoes stale watchdog fires.
            hrec.t_last_progress = now
            local rate = auto_throttle_rate(crec, arg, now)
            local window = tonumber(arg.throttle_window) or 8
            local watch = auto_throttle_watch_name(askey, host_name or host_or_ip, dport)
            if rate then
                if rate < (tonumber(arg.throttle_min_bps) or 500) then
                    -- Thin flow: if packets stop entirely from here, only the
                    -- timer below can still react. Re-arming replaces the
                    -- previous timer. pcall: a timer failure must never break
                    -- the packet path.
                    pcall(
                        timer_set, watch, "auto_throttle_watchdog", 2 * window * 1000, true,
                        { hrec = hrec, host_name = host_name, peer = host_or_ip, dport = dport, window = window, arg = arg }
                    )
                    auto_reset_connection(desync, arg, name, host_or_ip, dport)
                    auto_fail_helper_strategy(name, host_name)

                    if auto_check_fails(hrec, arg) then
                        auto_do_switch(hrec, auto_strategy_success_list(false), "THROTTLE", host_or_ip, dport, true)
                        -- Same-episode guard: our ERR above (re)triggers a
                        -- helper verdict for this exact stall; counting it
                        -- again would rotate twice. Later fresh FAILs count.
                        hrec.helper_fails = nil
                        if host_name and _G.helper_check then
                            _G.helper_check[host_name] = true
                        end
                    end

                    return auto_strategy_plan(desync, hrec, verdict)
                else
                    -- Healthy window: flow recovered, drop the watchdog.
                    pcall(timer_del, watch)
                end
            end

            -- Retransmissions count even when the helper locked this strategy:
            -- a frozen flow keeps retransmitting unacked data while the
            -- helper keeps saying OK (headers fly, body dead). Silent count
            -- here: no RST on a helper-blessed strategy, the shared fails
            -- counter plus the helper recheck (via ERR) decide.
            if host_name and desync.outgoing and is_retransmission(desync) and _G.zapret_ipc[host_name] == true then
                auto_fail_helper_strategy(name, host_name)

                if auto_check_fails(hrec, arg) then
                    auto_do_switch(hrec, auto_strategy_success_list(false), "RETRANSMIT", host_or_ip, dport, true)
                    -- Same-episode guard, see THROTTLE above.
                    hrec.helper_fails = nil
                    if host_name and _G.helper_check then
                        _G.helper_check[host_name] = true
                    end
                end

                return auto_strategy_plan(desync, hrec, verdict)
            end

            -- Helper says the host works: pass with the current strategy.
            -- Retransmissions are handled above, so they are never ignored
            -- on a locked strategy; anything else passes straight through.
            if host_name and _G.zapret_ipc[host_name] == true then
                verdict = auto_strategy_plan(desync, hrec, verdict)

                send_signal("VALID", host_name, name, 10000)

                if hrec.nstrategy ~= 0 and auto_check_valid_strategy(hrec, auto_strategy_success_list(false)) then
                    hrec.strategy_success_fail = false

                    table.insert(auto_strategy_success_list(false), hrec.nstrategy)
                end

                -- NOTE: hrec.fails is intentionally NOT reset here. A fresh
                -- helper OK clears helper_fails (its own counter), but local
                -- packet evidence must survive across interleaved healthy
                -- packets or retrans counts could never reach the threshold.
                -- Staleness is bounded by the time window in auto_check_fails.
                hrec.helper_fails = nil;

                return verdict
            end

            -- Local error evidence. ERR spam to port 10000 is by design;
            -- the helper dedups it. Switching here uses only the local
            -- fails counter, so traffic volume alone cannot force a switch
            -- while we wait for the helper verdict.
            if desync.outgoing and is_retransmission(desync) then
                auto_reset_connection(desync, arg, name, host_or_ip, dport)
                auto_fail_helper_strategy(name, host_name)

                if auto_check_fails(hrec, arg) then
                    auto_do_switch(hrec, auto_strategy_success_list(false), "is_retransmission", host_or_ip, dport, true)
                end

                return auto_strategy_plan(desync, hrec, verdict)
            end

            -- No hostname: pure IP, the helper cannot check it.
            -- Packet-level zapret logic only, helper is not involved.
            if not host_name then
                local seq = pos_get(desync, 's')
                if bitand(desync.dis.tcp.th_flags, TH_RST) ~= 0 and seq >= 1 and seq <= 8192 then
                    auto_reset_connection(desync, arg, name, host_or_ip, dport)

                    if auto_check_fails(hrec, arg) then
                        auto_do_switch(hrec, auto_strategy_success_list(false), "RST", host_or_ip, dport, true)
                    end

                    return auto_strategy_plan(desync, hrec, verdict)
                end

                local payload = desync.reasm_data or desync.dis.payload
                local plen = payload and #payload or 0
                if plen >= 16000 then
                    auto_reset_connection(desync, arg, name, host_or_ip, dport)

                    if auto_check_fails(hrec, arg) then
                        auto_do_switch(hrec, auto_strategy_success_list(false), "DPI16KB", host_or_ip, dport, true)
                    end

                    return auto_strategy_plan(desync, hrec, verdict)
                end

                if desync.l7payload == "http_reply" and desync.track and desync.track.hostname then
                    local hdis = http_dissect_reply(desync.dis.payload)
                    if hdis and (hdis.code == 302 or hdis.code == 307) then
                        local idx_loc = array_field_search(hdis.headers, "header_low", "location")
                        if idx_loc and is_dpi_redirect(desync.track.hostname, hdis.headers[idx_loc].value) then
                            auto_reset_connection(desync, arg, name, host_or_ip, dport)

                            if auto_check_fails(hrec, arg) then
                                auto_do_switch(hrec, auto_strategy_success_list(false), "DPI_redirect", host_or_ip, dport, true)
                            end

                            return auto_strategy_plan(desync, hrec, verdict)
                        end
                    end
                end
            end

            -- Helper verdict consumption (edge-triggered, not level):
            -- zcheck sets helper_check=false on every fresh OK/FAIL, we set
            -- it to true once the FAIL is counted. Repeated packets with the
            -- same hanging FAIL never switch, no matter how many arrive.
            -- A single transient FAIL is filtered by helper_fails (default 2).
            if host_name then
                if _G.zapret_ipc[host_name] == false then
                    if _G.helper_check[host_name] == false then
                        _G.helper_check[host_name] = true

                        if auto_check_helper_fails(hrec, arg) then
                            auto_do_switch(hrec, auto_strategy_success_list(false), "HELPER FAIL", host_or_ip, dport, true)
                            -- Symmetric: a helper-driven switch consumes the
                            -- local evidence of the abandoned episode; fresh
                            -- retransmissions re-accumulate from zero.
                            hrec.fails = nil
                        end
                    end

                    return auto_strategy_plan(desync, hrec, verdict)
                else
                    send_signal("VALID", host_name, name, 10000)
                end
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

                    ULOG("OK", "zapret:auto_strategy: CONFIRMED UDP " .. name .. "->" .. host_or_ip ..
                        ":" .. dport)

                    local list = auto_strategy_success_list(true)
                    if hrec.nstrategy ~= 0 and auto_check_valid_strategy(hrec, list) then
                        hrec.strategy_success_fail = false

                        table.insert(list, hrec.nstrategy)
                    end

                    hrec.fails = nil
                end

                return auto_strategy_plan(desync, hrec, verdict)
            end

            -- Failure: many outgoing packets with no server reply. Outgoing
            -- only, re-armed every udp_out outgoing packets, so a broken
            -- long-lived flow keeps switching strategies instead of freezing
            -- on the first verdict.
            if desync.outgoing and pos_client >= arg.udp_out and (pos_client - judged_client) >= arg.udp_out then
                crec.udp_judged_client = pos_client

                ULOG("WARNING", "zapret:auto_strategy: FAIL UDP " .. name .. "->" .. host_or_ip ..
                    ":" .. dport .. " out=" .. pos_client .. " in=" .. pos_server)

                if crec.udp_success then
                    crec.udp_success = nil
                end

                if auto_check_fails(hrec, arg) then
                    auto_do_switch(hrec, auto_strategy_success_list(true), "UDP", host_or_ip, dport, false)
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
        helper_fails = tonumber(arg.helper_fails) or 2,
        maxseq = tonumber(arg.maxseq) or 32768,
        udp_in = tonumber(arg.udp_in) or 1,
        udp_out = tonumber(arg.udp_out) or 4,
        throttle_window = tonumber(arg.throttle_window) or 8,
        throttle_min_bps = tonumber(arg.throttle_min_bps) or 500,
        reset = arg.reset ~= nil or false,
        time = arg.time or 300,
        helper_time = arg.helper_time or arg.time or 300
    }
end




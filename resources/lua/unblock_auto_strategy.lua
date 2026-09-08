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
--      server.pcounter <= udp_in). Вердикт финализируется один раз на
--      соединение, поэтому объём трафика сам по себе не переключает стратегии.
--   8. Все стратегии перебраны (exhausted) → прямой трафик
--
-- Перебор: 1, 2, 3, 4, ..., N (последовательно)
-- Параметры (через --lua-desync=auto_strategy:fails=4:...):
--   fails=N        — локальных ошибок до переключения (по умолч. 3)
--   helper_fails=N — подряд FAIL от helper до переключения (по умолч. 2)
--   time=N         — время через которое произойдет сброс ошибок (по умолч. 300 сек)
--   helper_time=N  — окно сброса счётчика helper (по умолч. = time)
--   maxseq=N       — макс. seq для проверок (по умолч. 32768)
--   udp_out=N      — исходящих UDP пакетов для вердикта FAIL (по умолч. 4)
--   udp_in=N       — входящих UDP пакетов: больше N — успех, иначе фейл (по умолч. 1)
--   reset          — отправлять RST при ошибке
-- auto_host_group — группирует хосты одного семейства в общую стратегию.
-- googlevideo (rr*-sn-*.googlevideo.com) — все такие хосты делят один hrec:
-- подобранная стратегия применяется ко всем сразу, при сбое/FAIL переключается
-- для всей группы, а не для каждого rr-хоста отдельно.
function auto_host_group(hostkey)
    if hostkey and hostkey:find("googlevideo%.com$") then
        return "googlevideo.com"
    end
    return hostkey
end

-- askey — table key inside autostate. TCP and UDP live in different winws2
-- profiles with their own strategy numbering, so their state is fully
-- separate: a UDP strategy switch must never touch TCP state and vice versa.
function auto_host_record(desync, askey)
    local hostkey
    if desync.track and desync.track.hostname then
        hostkey = auto_host_group(desync.track.hostname)
    else
        hostkey = host_ip(desync)
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
-- send_exhausted: TCP sends the "exhausted" IPC string to unblock (domain
-- testing there is TCP/curl based). UDP never sends it: UDP exhaustion says
-- nothing about TCP reachability of the host.
function auto_do_switch(rec, success_list, reason, peer, dport, send_exhausted)
    ULOG("WARNING", "zapret:auto_strategy: FAIL " .. auto_strategy_name(rec) .. " " .. reason .. "->" .. peer ..
        ":" .. dport)

    if rec.strategy_success_fail then
        if rec.nstrategy < rec.ctstrategy then
            rec.nstrategy = rec.nstrategy + 1

            while not auto_check_valid_strategy(rec, success_list) do
                rec.nstrategy = rec.nstrategy + 1
            end
        else
            if send_exhausted then
                send_signal("STRING", "exhausted", peer)
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

function auto_strategy(ctx, desync)
    orchestrate(ctx, desync)
    if not desync.track then
        return
    end

    -- TCP and UDP run in different winws2 profiles with different strategy
    -- numbering, so the host record (and all queues) is per protocol.
    local is_udp = desync.dis.udp ~= nil
    local hrec = auto_host_record(desync, is_udp and "auto_strategy_udp" or "auto_strategy")
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
    end

    if not hrec.nstrategy then
        hrec.nstrategy = 0
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

            -- Helper says the host works: pass with the current strategy,
            -- never inspect is_retransmission for this packet.
            if host_name and _G.zapret_ipc[host_name] == true then
                verdict = auto_strategy_plan(desync, hrec, verdict)

                send_signal("VALID", host_name, name, 10000)

                if hrec.nstrategy ~= 0 and auto_check_valid_strategy(hrec, auto_strategy_success_list(false)) then
                    hrec.strategy_success_fail = false

                    table.insert(auto_strategy_success_list(false), hrec.nstrategy)
                end

                hrec.fails = 0;
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

            -- Success: the server answers. Incoming packets only, the same
            -- semantics as zapret2 process_udp_fail (server.pcounter >
            -- udp_in).
            if pos_server > arg.udp_in then
                -- One verdict per connection (failure_detect_finalized
                -- semantics): late packets of an already judged connection
                -- are never counted again.
                if not crec.udp_done then
                    crec.udp_done = true

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
            -- only, finalized once per connection, so packet volume alone
            -- cannot force a switch (the same principle as TCP retrans).
            if desync.outgoing and pos_client >= arg.udp_out then
                if not crec.udp_done then
                    crec.udp_done = true

                    ULOG("WARNING", "zapret:auto_strategy: FAIL UDP " .. name .. "->" .. host_or_ip ..
                        ":" .. dport .. " out=" .. pos_client .. " in=" .. pos_server)

                    if auto_check_fails(hrec, arg) then
                        auto_do_switch(hrec, auto_strategy_success_list(true), "UDP", host_or_ip, dport, false)
                    end
                end

                return auto_strategy_plan(desync, hrec, verdict)
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
        reset = arg.reset ~= nil or false,
        time = arg.time or 300,
        helper_time = arg.helper_time or arg.time or 300
    }
end

-- auto_strategy — адаптивный перебор стратегий для обхода DPI
-- Автор: MagilaWEB (МогилныйПатчер)
--
-- Как работает:
--   1. Сначала пробуем прямое соединение (direct, nstrategy = 0)
--   2. Ошибка (RST, 16KB, redirect, retrans) → перебор стратегий 1..N
--   3. При подтверждении соединения (CONFIRMED) хост проверяется через zapret-helper
--   4. Helper ответил OK → стратегия закрепляется (fixed_strategy), применяется всегда
--   5. Закреплённая стратегия не меняется при локальных сбоях — ждём FAIL от helper
--   6. Helper ответил FAIL → фиксация снимается, перебор начинается заново с direct
--   7. Все стратегии перебраны (exhausted) → прямой трафик
--
-- Перебор: 1, 2, 3, 4, ..., N (последовательно)
-- Параметры (через --lua-desync=auto_strategy:fails=3:...):
--   fails=N     — ошибок до переключения (по умолч. 3)
--   time=N      — время через которое произойдет сброс ошибок (по умолч. 60 сек)
--   maxseq=N    — макс. seq для проверок (по умолч. 32768)
--   reset       — отправлять RST при ошибке
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

function auto_host_record(desync)
    local hostkey
    if desync.track and desync.track.hostname then
        hostkey = desync.track.hostname
    else
        hostkey = host_ip(desync)
    end

    if not hostkey then
        return nil
    end

    hostkey = auto_host_group(hostkey)

    if not autostate then
        autostate = {}
    end

    local askey = "auto_strategy"
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

function auto_strategy(ctx, desync)
    orchestrate(ctx, desync)
    if not desync.track then
        return
    end

    local hrec = auto_host_record(desync)
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

    local function strategy_name()
        if hrec.nstrategy == 0 then
            return "direct"
        end
        return "strategy_" .. hrec.nstrategy
    end

    local function fail_helper_stretegy()
        if host_name then
            send_signal("ERR", host_name, strategy_name(), 10000)
        end
    end

    local function check_helper_stretegy()
        if host_name then
            _G.helper_check[host_name] = true
            send_signal("CHECK", host_name, nil, 10000)
            ULOG("INFO", "zapret-helper:check " .. host_name)
        end
    end

    local function strategy_plan()
        while true do
            local inst = plan_instance_pop(desync)
            if not inst then
                break
            end

            if inst.arg.strategy and tonumber(inst.arg.strategy) == hrec.nstrategy and hrec.nstrategy ~= 0 then
                verdict = plan_instance_execute(desync, verdict, inst)
            end
        end
    end

    if crec then
        local host_or_ip = host_or_ip(desync)

        local dport = tostring(desync.dis.tcp and desync.dis.tcp.th_dport or desync.dis.udp and desync.dis.udp.uh_dport)

        DLOG("auto_strategy: " .. strategy_name() .. "->" .. host_or_ip .. ":" .. dport)

        local function reset_conection()
            if arg.reset then
                if desync.dis.tcp then
                    local seq = pos_get(desync, 's')
                    if desync.outgoing then
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
                            ULOG("INFO",
                                "zapret:reset_conection: " .. strategy_name() .. "->" .. host_or_ip .. ":" .. dport)

                            rawsend_dissect(dis, {
                                ifout = desync.ifin or 0
                            })
                        end
                    end
                end
            end
        end

        local function check_fails()
            if arg.fails then
                local now = os.time()
                if hrec.last_fail_time and now - hrec.last_fail_time > (tonumber(arg.time) or 60) then
                    hrec.fails = nil
                end

                if not hrec.fails then
                    hrec.fails = 0
                end

                hrec.fails = hrec.fails + 1
                hrec.last_fail_time = now

                if hrec.fails >= arg.fails then
                    hrec.fails = nil
                    return true
                end
            end
            return false
        end

        local function do_switch(reason)

            ULOG("WARNING", "zapret:auto_strategy: FAIL " .. strategy_name() .. " " .. reason .. "->" .. host_or_ip ..
                ":" .. dport)

            if hrec.nstrategy < hrec.ctstrategy then
                hrec.nstrategy = hrec.nstrategy + 1
            else
                send_signal("STRING", "exhausted", host_or_ip)
                hrec.nstrategy = 0
            end
        end

        if not _G.zapret_ipc then
            _G.zapret_ipc = {}
        end

        if desync.dis.tcp then
            if host_name and _G.zapret_ipc[host_name] then
                strategy_plan()

                if host_name then
                    send_signal("VALID", host_name, strategy_name(), 10000)
                end
                return verdict
            end

            if desync.outgoing and is_retransmission(desync) then
                reset_conection()
                fail_helper_stretegy()
                -- check_helper_stretegy()

                if check_fails() then
                    do_switch("is_retransmission")
                end

                strategy_plan()
                return verdict
            end

            if not host_name then
                local seq = pos_get(desync, 's')
                if bitand(desync.dis.tcp.th_flags, TH_RST) ~= 0 and seq >= 1 and seq <= 8192 then
                    reset_conection()
                    fail_helper_stretegy()
                    -- check_helper_stretegy()

                    if check_fails() then
                        do_switch("RST")
                    end

                    strategy_plan()
                    return verdict
                end

                local payload = desync.reasm_data or desync.dis.payload
                local plen = payload and #payload or 0
                if plen >= 16000 then
                    reset_conection()
                    fail_helper_stretegy()
                    -- check_helper_stretegy()

                    if check_fails() then
                        do_switch("DPI16KB")
                    end

                    strategy_plan()
                    return verdict
                end

                if desync.l7payload == "http_reply" and desync.track and desync.track.hostname then
                    local hdis = http_dissect_reply(desync.dis.payload)
                    if hdis and (hdis.code == 302 or hdis.code == 307) then
                        local idx_loc = array_field_search(hdis.headers, "header_low", "location")
                        if idx_loc and is_dpi_redirect(desync.track.hostname, hdis.headers[idx_loc].value) then
                            reset_conection()
                            fail_helper_stretegy()
                            -- check_helper_stretegy()

                            if check_fails() then
                                do_switch("DPI_redirect")
                            end

                            strategy_plan()
                            return verdict
                        end
                    end
                end
            end

            if not _G.helper_check then
                _G.helper_check = {}
            end

            if host_name then
                if _G.zapret_ipc[host_name] == false then
                    if _G.helper_check[host_name] == false then
                        _G.helper_check[host_name] = true
                        reset_conection()
                        fail_helper_stretegy()

                        if check_fails() then
                            do_switch("HELPER ZAPRET HOST FAIL")
                        end
                    end

                    strategy_plan()
                    return verdict
                else
                    send_signal("VALID", host_name, strategy_name(), 10000)
                end
            end
        end

        if desync.dis.udp and desync.outgoing then
            local pos_out = pos_get(desync, 'n', false)
            local pos_in = pos_get(desync, 'n', true)
            if pos_out >= 4 and pos_in <= 1 then
                fail_helper_stretegy()

                if check_fails() then
                    do_switch("UDP");
                    strategy_plan()
                    return verdict
                end
            else
                if host_name then
                    send_signal("VALID", host_name, strategy_name(), 10000)
                end
                ULOG("OK",
                    "zapret:auto_strategy: CONFIRMED UDP " .. strategy_name() .. "->" .. host_or_ip .. ":" .. dport)
            end

            -- ULOG("INFO", "zapret:udp out=" .. pos_out .. " in=" .. pos_in .. " " .. host_or_ip(desync))
        end
    end

    strategy_plan()

    return verdict
end

function args_defaults(arg)
    return {
        fails = tonumber(arg.fails) or 3,
        maxseq = tonumber(arg.maxseq) or 32768,
        udp_in = tonumber(arg.udp_in) or 1,
        udp_out = tonumber(arg.udp_out) or 4,
        reset = arg.reset ~= nil or false,
        time = arg.time or 60
    }
end

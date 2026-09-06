-- auto_strategy — адаптивный перебор стратегий для обхода DPI
-- Автор: MagilaWEB (МогилныйПатчер)
--
-- Как работает:
--   1. Сначала пробуем прямое соединение (direct, nstrategy = 0)
--   2. host_name известен: шлём helper только ERR (порт 10000 — спам-канал),
--      CHECK не шлём вообще. Helper обо всех узнает и перепроверит по таймеру.
--   3. Helper ответил OK → пускаем с текущей стратегией, is_retransmission
--      не проверяем вообще.
--   4. Helper ответил FAIL + is_retransmission → reset + ERR, переключение
--      по локальному счётчику fails (защита от шторма пакетов).
--   5. Пакеты без retrans при висящем FAIL → ждём вердикт helper; свежий FAIL
--      копится в отдельном счётчике helper_fails (по умолч. 2, против ложных
--      коротких FAIL), switch только по нему, мимо локального счётчика.
--   6. host_name неизвестен (чистый IP helper проверить не может) → только
--      пакетная логика zapret (RST, 16KB, redirect, retrans) по счётчику fails.
--   7. Все стратегии перебраны (exhausted) → прямой трафик
--
-- Перебор: 1, 2, 3, 4, ..., N (последовательно)
-- Параметры (через --lua-desync=auto_strategy:fails=4:...):
--   fails=N        — локальных ошибок до переключения (по умолч. 3)
--   helper_fails=N — подряд FAIL от helper до переключения (по умолч. 2)
--   time=N         — время через которое произойдет сброс ошибок (по умолч. 60 сек)
--   helper_time=N  — окно сброса счётчика helper (по умолч. = time)
--   maxseq=N       — макс. seq для проверок (по умолч. 32768)
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

    -- Only ERR is sent to the helper (port 10000 is the spam channel).
    -- CHECK is never sent: the helper learns every host from ERR/VALID
    -- and rechecks everyone by its own timer.
    local function fail_helper_stretegy()
        if host_name then
            send_signal("ERR", host_name, strategy_name(), 10000)
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

        -- Separate counter for helper FAIL verdicts. The helper may briefly
        -- report a working host as broken, so a single FAIL never switches:
        -- only helper_fails consecutive fresh verdicts do.
        local function check_helper_fails()
            local threshold = arg.helper_fails or 2
            if threshold then
                local now = os.time()
                local window = tonumber(arg.helper_time) or (tonumber(arg.time) or 60)
                if hrec.helper_last_fail_time and now - hrec.helper_last_fail_time > window then
                    hrec.helper_fails = nil
                end

                if not hrec.helper_fails then
                    hrec.helper_fails = 0
                end

                hrec.helper_fails = hrec.helper_fails + 1
                hrec.helper_last_fail_time = now

                if hrec.helper_fails >= threshold then
                    hrec.helper_fails = nil
                    return true
                end
            end
            return false
        end

        local function check_valid_strategy()
            if _G.strategy_success then
                for _, nstrategy in ipairs(_G.strategy_success) do
                    if nstrategy == hrec.nstrategy then
                        return false
                    end
                end
            end

            return true
        end

        local function do_switch(reason)

            ULOG("WARNING", "zapret:auto_strategy: FAIL " .. strategy_name() .. " " .. reason .. "->" .. host_or_ip ..
                ":" .. dport)

            if hrec.strategy_success_fail then
                if hrec.nstrategy < hrec.ctstrategy then
                    hrec.nstrategy = hrec.nstrategy + 1

                    while not check_valid_strategy() do
                        hrec.nstrategy = hrec.nstrategy + 1
                    end
                else
                    send_signal("STRING", "exhausted", host_or_ip)
                    hrec.nstrategy = 0
                    hrec.sstrategy = 1
                    hrec.strategy_success_fail = false
                end

                return
            end

            if not hrec.sstrategy then
                hrec.sstrategy = 1
            end

            local count_strategy_success = _G.strategy_success and #_G.strategy_success or 0
            if (count_strategy_success == 0) or (count_strategy_success < hrec.sstrategy) then
                hrec.strategy_success_fail = true
                hrec.sstrategy = 1
                hrec.nstrategy = 1
                return
            end

            hrec.nstrategy = _G.strategy_success[hrec.sstrategy]
            hrec.sstrategy = hrec.sstrategy + 1
        end

        if not _G.zapret_ipc then
            _G.zapret_ipc = {}
        end

        if not _G.helper_check then
            _G.helper_check = {}
        end

        if desync.dis.tcp then
            -- Helper says the host works: pass with the current strategy,
            -- never inspect is_retransmission for this packet.
            if host_name and _G.zapret_ipc[host_name] == true then
                strategy_plan()

                send_signal("VALID", host_name, strategy_name(), 10000)

                if hrec.nstrategy ~= 0 and check_valid_strategy() then
                    if not _G.strategy_success then
                        _G.strategy_success = {}
                    end

                    hrec.strategy_success_fail = false

                    table.insert(_G.strategy_success, hrec.nstrategy)
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
                reset_conection()
                fail_helper_stretegy()

                if check_fails() then
                    do_switch("is_retransmission")
                end

                strategy_plan()
                return verdict
            end

            -- No hostname: pure IP, the helper cannot check it.
            -- Packet-level zapret logic only, helper is not involved.
            if not host_name then
                local seq = pos_get(desync, 's')
                if bitand(desync.dis.tcp.th_flags, TH_RST) ~= 0 and seq >= 1 and seq <= 8192 then
                    reset_conection()

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

                            if check_fails() then
                                do_switch("DPI_redirect")
                            end

                            strategy_plan()
                            return verdict
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

                        if check_helper_fails() then
                            do_switch("HELPER FAIL")
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
            -- Fresh helper FAIL on UDP: same edge-triggered helper counter
            -- as TCP, bypasses the local counter.
            if host_name and _G.zapret_ipc[host_name] == false then
                if _G.helper_check[host_name] == false then
                    _G.helper_check[host_name] = true

                    if check_helper_fails() then
                        do_switch("HELPER FAIL UDP")
                    end
                end

                strategy_plan()
                return verdict
            end

            local pos_out = pos_get(desync, 'n', false)
            local pos_in = pos_get(desync, 'n', true)
            if pos_out >= 4 and pos_in <= 1 then
                -- Helper says OK: trust it, never switch on UDP timeout.
                if host_name and _G.zapret_ipc[host_name] == true then
                    send_signal("VALID", host_name, strategy_name(), 10000)
                    hrec.fails = 0
                    hrec.helper_fails = nil
                    strategy_plan()
                    return verdict
                end

                fail_helper_stretegy()

                if check_fails() then
                    do_switch("UDP");
                    strategy_plan()
                    return verdict
                end
            else
                if host_name then
                    send_signal("VALID", host_name, strategy_name(), 10000)
                    hrec.helper_fails = nil
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
        helper_fails = tonumber(arg.helper_fails) or 2,
        maxseq = tonumber(arg.maxseq) or 32768,
        udp_in = tonumber(arg.udp_in) or 1,
        udp_out = tonumber(arg.udp_out) or 4,
        reset = arg.reset ~= nil or false,
        time = arg.time or 300,
        helper_time = arg.helper_time or arg.time or 300
    }
end

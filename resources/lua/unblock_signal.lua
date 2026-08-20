-- send_signal — отправка UDP пакета на 127.0.0.1
-- signal_type: тип сигнала (STRING, BOOL, U32, LOG, CHECK, FAIL)
-- key: имя сигнала (необязательно)
-- val: значение (необязательно)
-- port: порт назначения (по умолчанию 9999 для IPC/unblock)
function send_signal(signal_type, key, val, port)
    local payload = signal_type
    if key then
        payload = payload .. ":" .. key
    end
    if val then
        payload = payload .. ":" .. tostring(val)
    end
    local dis = {
        ip = {
            ip_src = pton("127.0.0.1"),
            ip_dst = pton("127.0.0.1"),
            ip_p = IPPROTO_UDP,
            ip_id = 0,
            ip_off = 0,
            ip_ttl = 64,
            ip_tos = 0
        },
        udp = {
            uh_sport = 0,
            uh_dport = port or 9999
        },
        payload = payload
    }
    rawsend_dissect(dis, {
        repeats = 1
    })
end

-- ULOG — отправляет лог в unblock (через IPC) и в DLOG (winws2.exe.log)
-- key: уровень сообщения — "OK", "WARNING", "INFO"
-- val: текст сообщения (рекомендуется префикс zapret:)
-- Пример:
--   ULOG("OK", "zapret:auto_strategy: CONFIRMED strategy_1->discord.com")
--   ULOG("WARNING", "zapret:auto_strategy: FAIL strategy_1 RST->discord.com")
--   ULOG("INFO", "zapret:check youtube.com")
function ULOG(key, val)
    send_signal("LOG", key, val)
    DLOG(val)
end

function unblock_ipc(ctx, desync)
    zcheck(ctx, desync)
end

-- zcheck — обработчик проверки хостов от zapret-helper
-- Автор: MagilaWEB (МогилныйПатчер)
-- Используется: auto_strategy.lua через _G.zapret_ipc
--
-- zapret-helper (отдельное консольное приложение) тестирует хосты через curl
-- и шлёт FAIL:hostname обратно на UDP 10000.
-- WinDivert перехватывает пакет, запускает zcheck.
-- zcheck парсит FAIL, сохраняет в _G.zapret_ipc[host] = "FAIL".
-- auto_strategy при каждом подключении проверяет _G.zapret_ipc.
-- Если для хоста есть FAIL — стратегия бракуется, следует ротация.
-- Флаг _G.zapret_checking предотвращает повторную отправку CHECK
-- пока предыдущий не обработан.
function zcheck(ctx, desync)
    local payload = desync.reasm_data or desync.dis.payload
    if payload and #payload > 0 then
        local msg = payload

        if not _G.zapret_ipc then
            _G.zapret_ipc = {}
        end

        if not _G.helper_check then
            _G.helper_check = {}
        end

        if msg:find("FAIL:") == 1 then
            local host = msg:sub(6)
            _G.zapret_ipc[host] = false
            _G.helper_check[host] = false

            ULOG("WARNING", "zcheck: FAIL " .. host)
        elseif msg:find("OK:") == 1 then
            local host = msg:sub(4)
            _G.zapret_ipc[host] = true
            _G.helper_check[host] = false
            ULOG("OK", "zcheck host ok -> " .. host)
        end
    end
end

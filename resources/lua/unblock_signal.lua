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

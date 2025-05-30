@SET ARGS=--wf-tcp=80 --wf-udp=50000-65535 ^
--filter-udp=50000-65535 --dpi-desync=fake,tamper --dpi-desync-any-protocol=1 desync-fooling=md5sig %FAKE_QUIC% --new ^
--filter-tcp=80 --dpi-desync=fake,split2 --dpi-desync-autottl=2 --dpi-desync-fooling=md5sig
@SET ARGSDPI=-5 --auto-ttl=2 --blacklist %CONFIGS%russia-blacklist.txt

@SET SRVCNAMESTART=true
@SET SRVCNAMESTARTDPI=true

@SET ARGS=--wf-tcp=80,443 --wf-udp=443,50000-65535 ^
--filter-udp=443 %BLOCKLIST% --dpi-desync=fake --dpi-desync-repeats=6 %FAKE_QUIC% --new ^
--filter-udp=50000-65535 --dpi-desync=fake,tamper --dpi-desync-any-protocol=1 --dpi-desync-cutoff=d3 --dpi-desync-repeats=6 --new ^
--filter-tcp=80 --dpi-desync=fake,split2 --dpi-desync-autottl=0 --dpi-desync-fooling=md5sig --new ^
--filter-tcp=443 %BLOCKLIST% --dpi-desync=fake --dpi-desync-autottl=0 --dpi-desync-skip-nosni=0 --dpi-desync-fooling=badseq --dpi-desync-repeats=6 %FAKE_TLS%

@SET ARGS2=--wf-tcp=443 ^
-filter-tcp=443 --hostlist="%CONFIGS%russia-blacklist.txt" --dpi-desync=fake,split --dpi-desync-repeats=6 --dpi-desync-fooling=md5sig %FAKE_TLS%

@SET SRVCNAMESTART=true
@SET SRVCNAMESTART2=true
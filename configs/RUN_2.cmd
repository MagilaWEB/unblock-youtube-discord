@SET ARGS=--wf-tcp=80,443 --wf-udp=443,50000-65535 ^
--filter-udp=443 %BLOCKLIST% --dpi-desync=fake --dpi-desync-repeats=11 %FAKE_QUIC% --new ^
--filter-udp=50000-65535 --filter-l7=discord,stun --dpi-desync=fake --dpi-desync-repeats=6 --new ^
--filter-tcp=80 %BLOCKLIST% --dpi-desync=fake,fakedsplit --dpi-desync-autottl=1 --dpi-desync-fooling=md5sig --new ^
--filter-tcp=443 %BLOCKLIST% --dpi-desync=fake,multidisorder --dpi-desync-split-pos=1,midsld --dpi-desync-repeats=11 --dpi-desync-fooling=md5sig --dpi-desync-fake-tls-mod=rnd,dupsid,sni=www.google.com --new ^
--filter-udp=443 --ipset="%CONFIGS%ip-blacklist.txt" --dpi-desync=fake --dpi-desync-repeats=11 %FAKE_QUIC% --new ^
--filter-tcp=80 --ipset="%CONFIGS%ip-blacklist.txt" --dpi-desync=fake,fakedsplit --dpi-desync-autottl=1 --dpi-desync-fooling=md5sig --new ^
--filter-tcp=443 --ipset="%CONFIGS%ip-blacklist.txt" --dpi-desync=fake,multidisorder --dpi-desync-split-pos=1,midsld --dpi-desync-repeats=11 --dpi-desync-fooling=md5sig

@SET SRVCNAMESTART=true
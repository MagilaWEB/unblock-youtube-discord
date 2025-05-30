@SET ARGS=--wf-tcp=80,443 --wf-udp=443,50000-65535 ^
--filter-udp=443 %BLOCKLIST% --dpi-desync=fake --dpi-desync-repeats=6 %FAKE_QUIC% --new ^
--filter-udp=50000-65535 --dpi-desync=fake --dpi-desync-any-protocol=1 --dpi-desync-cutoff=d3 --dpi-desync-repeats=6 --new ^
--filter-tcp=80 --dpi-desync=fake,split2 --dpi-desync-autottl=1 --dpi-desync-fooling=md5sig --new ^
--filter-tcp=443 %BLOCKLIST% --dpi-desync=fake,split2 --dpi-desync-ttl=5 --dpi-desync-autottl=1 --dpi-desync-repeats=6 %FAKE_TLS% ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata --dpi-desync-fake-syndata="%BIN%tls_clienthello_iana_org.bin" ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,split2 ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,split2 --dpi-desync-fake-syndata="%BIN%tls_clienthello_iana_org.bin" ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,disorder2 ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,disorder2 --dpi-desync-fake-syndata="%BIN%tls_clienthello_iana_org.bin" ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata --wssize=1:6 ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata --dpi-desync-fake-syndata="%BIN%tls_clienthello_iana_org.bin" --wssize=1:6 ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,split2 --wssize=1:6 ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,split2 --dpi-desync-fake-syndata="%BIN%tls_clienthello_iana_org.bin" --wssize=1:6 ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,disorder2 --wssize=1:6 ^
--wf-l3=ipv4 --wf-tcp=443 --dpi-desync=syndata,disorder2 --dpi-desync-fake-syndata="%BIN%tls_clienthello_iana_org.bin" --wssize=1:6

@SET SRVCNAMESTART=true
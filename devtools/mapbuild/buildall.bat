echo off
:start

@rem	Sleep time is arbitrary, it just prevents us from spamming Perforce.

call buildmod ep2 -all

sleep 15

call buildmod portal -all

sleep 15

call buildmod hl2 -all

sleep 15

call buildmod tf -all

sleep 15

goto start

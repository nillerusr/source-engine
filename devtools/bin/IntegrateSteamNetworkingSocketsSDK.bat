SET SRC=//Steam/sdr_public
SET DEST=../../..

:: 32 bit windows
p4 copy %SRC%/bin/win32/steamnetworkingsockets.lib %DEST%/src/lib/public/steamnetworkingsockets.lib
p4 copy %SRC%/bin/win32/steamnetworkingsockets.dll %DEST%/game/bin/steamnetworkingsockets.dll

:: 64-bit windows
::p4 copy %SRC%/bin/win64/steamnetworkingsockets.lib %DEST%/src/lib/public/x64/steamnetworkingsockets.lib
::p4 copy %SRC%/bin/win64/steamnetworkingsockets.dll %DEST%/game/bin/x64/steamnetworkingsockets.dll

:: GC.  TF doesn't use SDR for actual connectivity, so we don't need to generate any tickets
::p4 copy %SRC%/bin/win64/steamdatagram_ticketgen.lib %DEST%/src/lib/public/x64/steamdatagram_ticketgen.lib
::p4 copy %SRC%/bin/win64/steamdatagram_ticketgen.dll %DEST%/game/csgo/bin/gc/x64/steamdatagram_ticketgen.dll
::p4 copy %SRC%/bin/win32/steamdatagram_ticketgen.lib %DEST%/src/lib/public/steamdatagram_ticketgen.lib
::p4 copy %SRC%/bin/win32/steamdatagram_ticketgen.dll %DEST%/game/csgo/bin/gc/steamdatagram_ticketgen.dll

:: 32-bit Linux, older toolchain.  That should work just fine for the linux client.
p4 copy %SRC%/bin/linux32/libsteamnetworkingsockets.so %DEST%/game/bin/libsteamnetworkingsockets.so
p4 copy %SRC%/bin/linux32/libsteamnetworkingsockets.so %DEST%/src/lib/public/linux32/libsteamnetworkingsockets.so

:: 64-bit Linux, depends on Steam runtime
::p4 copy %SRC%/bin/ubuntu12_64/libsteamnetworkingsockets.so %DEST%/game/bin/linux64/libsteamnetworkingsockets.so
::p4 copy %SRC%/bin/ubuntu12_64/libsteamnetworkingsockets.so %DEST%/src/lib/public/linux64/libsteamnetworkingsockets.so

:: OSX (32- and 64-bit fat binaries)
p4 copy %SRC%/client/Steam.AppBundle/Steam/Contents/MacOS/libsteamnetworkingsockets.dylib %DEST%/game/bin/libsteamnetworkingsockets.dylib
p4 copy %SRC%/client/Steam.AppBundle/Steam/Contents/MacOS/libsteamnetworkingsockets.dylib %DEST%/src/lib/public/osx32/libsteamnetworkingsockets.dylib

:: Tool
::p4 copy %SRC%/bin/win64/steamdatagram_certtool.exe %DEST%/src/devtools/bin/steamdatagram_certtool.exe

p4 integrate %SRC%/src/public/steamnetworkingsockets/... %DEST%/src/public/steamnetworkingsockets/...
p4 resolve -as

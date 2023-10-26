@echo off

rem Update GM Script function documentation

..\..\bin\gme.exe gmdoc.gm gmdoc.txt gmdoc.html

copy gmdoc.html ..\..\doc

rem Done.
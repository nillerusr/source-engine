@echo off

setlocal


:: // If they've disabled xbcp, then don't use it.
if NOT "%VALVE_NO_XBCP%"=="" (
	echo [valve_xbcp_wrapper] VALVE_NO_XBCP defined. Avoiding the copy.
	endlocal
	exit /b 0
)


set localFilename=%1
set remoteFilename=%2

"%XEDK%\bin\win32\xbcp" /y /t "%localFilename%" "%remoteFilename%"

if %ERRORLEVEL%==1 (
	echo XBCP failed. If you don't have an X360 but want to compile its sources, define VALVE_NO_XBCP.
	del /q %1%
	exit 1
)


:End
endlocal
exit /b 0

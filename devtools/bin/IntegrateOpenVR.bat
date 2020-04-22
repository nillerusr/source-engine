@echo off

:: Use this batch file to integrate steam client binaries directly from
:: //steam/rel/client.  This only really affects the binaries that gameservers
:: use.  (The client should use the binaries from the steam client that is
:: running.)  We do this when we want some feature or bugfix in the steam
:: binaries, but don't want to integarte a whole new SDK.

::
:: Set source paths
::

SET VRP4Path=%1
SET IntegDate=%2
set BINS_ONLY=0

if !%IntegDate%!==!/bins! (
SET IntegDate=
SET BINS_ONLY=1
)


if !%VRP4Path%!==!! (
	echo Usage: %0 VRP4Path
	echo.
	echo     VRP4Path should be the perforce server path to the branch you want to integrate from,
	echo     e.g. "//vr/steamvr/sdk_release/"
	goto :end
)

:: Use this when copying from official distribution.
SET DestRoot=..\..\..
set P4Root=%VRP4Path%
set SRCDIR_HEADERS=headers/...
set SRCDIR_DLL=bin
set SRCDIR_LIB=lib

::
:: Copy files
::

:: Client Win32 binaries
call :CopyOneFile %SRCDIR_DLL%/win32 openvr_api.dll game\bin
call :CopyOneFile %SRCDIR_LIB%/win32 openvr_api.lib src\lib\public

:: Client Linux binaries
call :CopyOneFile %SRCDIR_DLL%/linux32 libopenvr_api.so game\bin
call :CopyOneFile %SRCDIR_LIB%/linux32 libopenvr_api.so src\lib\public\linux32

:: Client Mac binaries.  Note that there's no dedicated server on the Mac,
:: so we can ship a smaller set
call :CopyOneFile %SRCDIR_DLL%/osx32 libopenvr_api.dylib game\bin
call :CopyOneFile %SRCDIR_LIB%/osx32 libopenvr_api.dylib src\lib\public\osx32

if !%BINS_ONLY%!==!1! (
 goto :end
)

:: Headers
ECHO ---------------------------------------------
ECHO Integrating Steam Headers from %P4Root%/%SRCDIR_HEADERS%
ECHO                             to %DestRoot%\src\public\steam\...

p4 integrate -d -i %P4Root%/%SRCDIR_HEADERS%%IntegDate% %DestRoot%\src\public\openvr\...
p4 resolve -at %DestRoot%\src\public\openvr\...

goto :end

:CopyOneFile
	ECHO ---------------------------------------------
	ECHO Integrating %P4Root%/%1/%2
	ECHO          to %DestRoot%\%3\%2
	P4 integrate -d -i %P4Root%/%1/%2%IntegDate% %DestRoot%\%3\%2
	P4 resolve -at %DestRoot%\%3\%2
	echo.

:end

@echo off

:: Use this batch file to integrate panorama and associated libs from //Steam/main/

::
:: Set source paths
::

SET SteamP4Path=%1

if !%SteamP4Path%!==!! (
	echo Usage: %0 SteamP4Path
	echo.
	echo     SteamP4Path should be the perforce server path to the branch you want to integrate from,
	echo     e.g. "//Steam/rel/client" or "//Steam/main"
	goto :end
)

set ThirdPartyPath=//thirdpartycode/nonredist
set V8Path=%ThirdPartyPath%/v8
set V8Bin=%V8Path%/out/ia32.release
set V8Headers=%V8Path%/include

set DestRoot=../../..
set DestLibs=%DestRoot%/src/lib/common/linux32/release
set DestHeaders=%DestRoot%/src/public/panorama
set DestSrc=%DestRoot%/src/panorama/...
set DestV8Headers=%DestRoot%/src/external/v8/include

set SrcHeaders=src/public/panorama
set SrcMain=src/panorama/...

::
:: Copy files
::

:: Client Linux binaries
call :CopyOneFile %V8Bin%            libicudata.a        %DestLibs%
call :CopyOneFile %V8Bin%            libv8_libplatform.a %DestLibs%
call :CopyOneFile %V8Bin%/lib.target libicui18n.so       %DestLibs%
call :CopyOneFile %V8Bin%/lib.target libv8.so            %DestLibs%
call :CopyOneFile %V8Bin%/lib.target libicuuc.so         %DestLibs%

:: Client Win32 binaries
:: TODO

:: Client Mac binaries.  Note that there's no dedicated server on the Mac,
:: so we can ship a smaller set
:: TODO

:: V8 Headers
ECHO ---------------------------------------------
ECHO Integrating V8 Headers from %V8Headers%/...
ECHO                          to %DestV8Headers%/...

p4 integrate -d -i %V8Headers%/... %DestV8Headers%/...
p4 resolve -at %DestV8Headers%/...

:: Headers
ECHO ---------------------------------------------
ECHO Integrating Panorama Headers from %SteamP4Path%/%SrcHeaders%/...
ECHO                                to %DestHeaders%/...

p4 integrate -d -i %SteamP4Path%/%SrcHeaders%/... %DestHeaders%/...
p4 resolve -at %DestHeaders%/...

:: Src
ECHO ---------------------------------------------
ECHO Integrating Panorama Sources from %SteamP4Path%/%SrcMain%/...
ECHO                                to %DestSrc%/...

p4 integrate -d -i %SteamP4Path%/%SrcMain%/... %DestSrc%/...
p4 resolve -at %DestSrc%/...

goto :end

:CopyOneFile
	ECHO ---------------------------------------------
	ECHO Integrating %1/%2
	ECHO          to %3
	P4 integrate -d -i %1/%2 %3/%2
	P4 resolve -at %3/%2
	echo.

:end

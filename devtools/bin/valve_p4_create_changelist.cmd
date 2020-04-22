@echo off

:: // This will make all new env variables local to this script 
setlocal

:: // Make sure we have 2 args
if .%2.==.. (
	echo  *** [valve_p4_create_changelist] Error calling command! No file or changelist specified for checkout! Usage: valve_p4_create_changelist.cmd fileOrPath "Description"
	endlocal
	exit /b 1
)

:: // Get file info
set valveTmpPathOnly="%~d1%~p1"
if "%valveTmpPathOnly%"==""	(
	echo  *** [valve_p4_create_changelist] Error! Can't parse file or path "%1"!
	endlocal
	exit /b 1
)

:: // Change directories so that the p4 set commands give use useful data
pushd %valveTmpPathOnly%

:: // Find user
for /f "tokens=2 delims== " %%A in ('p4 set ^| find /i "P4USER="') do set valveP4User=%%A
if "%valveP4User%"=="" goto RegularCheckout
rem //echo User="%valveP4User%"

:: // Find client
for /f "tokens=2 delims== " %%A in ('p4 set ^| find /i "P4CLIENT="') do set valveP4Client=%%A
if "%valveP4Client%"=="" goto RegularCheckout
rem //echo Client="%valveP4Client%"

:: // Search for existing changelist that matches command line arg
set valveP4ChangelistName=%2%
set valveP4ChangelistName=%valveP4ChangelistName:~1,-1%
for /f "tokens=2 delims= " %%A in ('p4 changes -u %valveP4User% -s pending -c %valveP4Client% ^| sort /r ^| find /i "'%valveP4ChangelistName%"') do set valveP4ChangelistNumber=%%A
if NOT "%valveP4ChangelistNumber%"=="" goto HaveChangelist

:: // We didn't find a matching changelist but we did figure enough out to create a new changelist
rem //echo Creating New Changelist
for /f "tokens=2 delims= " %%A in ('^( echo Change: new ^& echo Client: %valveP4Client% ^& echo User: %valveP4User% ^& echo Status: new ^& echo Description: %valveP4ChangelistName%^&echo.^) ^| p4 change -i') do set valveP4ChangelistNumberJustCreated=%%A
if "%valveP4ChangelistNumberJustCreated%"=="" goto RegularCheckout

:: // Now search for the changelist number even though we already have it to try to clean up after the race condition when it's hit
:: // This way, if more than one changelist is created in parallel, this will hopefully cause them to be checked out into the same changelist and the empty one deleted
for /f "tokens=2 delims= " %%A in ('p4 changes -u %valveP4User% -s pending -c %valveP4Client% ^| sort /r ^| find /i "'%valveP4ChangelistName%"') do set valveP4ChangelistNumber=%%A
if "%valveP4ChangelistNumber%"=="" goto RegularCheckout

if NOT "%valveP4ChangelistNumber%"=="%valveP4ChangelistNumberJustCreated%" p4 change -d %valveP4ChangelistNumberJustCreated% 2>&1 >nul

:: // We have a changelist number
:HaveChangelist
echo %valveP4ChangelistNumber%
goto End

:: // Can't find or create the changelist, output 0
:RegularCheckout
echo 0
goto End

:End
popd
endlocal
exit /b 0

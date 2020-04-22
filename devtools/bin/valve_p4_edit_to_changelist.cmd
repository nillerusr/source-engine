@echo off

:: // This will make all new env variables local to this script 
setlocal

:: // If called with the start command, we need to exit, so also make sure you pass EXIT as the third param!
:: // Also, if you modify this script, make sure that endlocal and exit are within ()'s so valveExitArg works!
:: // Type 'help set' at a command prompt if you don't understand why.
if NOT "%3"=="EXIT" set valveExitArg=/b

:: // Make sure we have 2 args
if .%2.==.. (
	echo  *** [valve_p4_edit_to_changelist] Error calling command! No file or changelist specified for checkout! Usage: valve_p4_edit_to_changelist.cmd file "Description" [EXIT]
	endlocal
	exit %valveExitArg% 1
)

:: // Get file info
set valveTmpFileName="%~n1%~x1"
set valveTmpFullFilePath="%~f1"
set valveTmpPathOnly="%~d1%~p1"

if "%valveTmpFileName%"==""	(
	echo  *** [valve_p4_edit_to_changelist] Error! Can't parse filename "%1"!
	endlocal
	exit %valveExitArg% 1
)

if "%valveTmpFullFilePath%"==""	(
	echo  *** [valve_p4_edit_to_changelist] Error! Can't parse filename "%1"!
	endlocal
	exit %valveExitArg% 1
)

if "%valveTmpPathOnly%"==""	(
	echo  *** [valve_p4_edit_to_changelist] Error! Can't parse filename "%1"!
	endlocal
	exit %valveExitArg% 1
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
set valveP4ChangelistArg=-c %valveP4ChangelistNumber%
rem //echo valveP4ChangelistArg="%valveP4ChangelistArg%"
rem //echo ChangelistNumber="%valveP4ChangelistNumber%"
rem //echo ChangelistName="%valveP4ChangelistName%"

:: // Check the file out
:RegularCheckout
SET SEDPATH=%~dp0sed.exe
rem The sed command replaces '//' with the empty string and replaces '...' with '---'
rem This is necessary so that VS will not interpret "//source2/main" or "... source2/main" as
rem a path. Interpreting it as a path means that pressing F8 in the VS error window will stop on
rem these output lines, which is a nuisance when looking for real errors and warnings, and it means
rem that VS will try to open \\source2\main\... which will often cause a 2-3 second hang!
if "%VALVE_WAIT_ON_P4%"=="" (
	p4 edit %valveP4ChangelistArg% %valveTmpFullFilePath% 2>&1 | %SEDPATH% -e s!//!! -e s!\.\.\.!---! | find /v /i "- currently opened for edit" | find /v /i "- also opened by" | find /v /i "- file(s) not on client" | find /v /i "- can't change from"
) ELSE (
	:: // Filter out largely benign messages unless we're explicitly waiting on p4 results a la buildbot
	p4 edit %valveP4ChangelistArg% %valveTmpFullFilePath% 2>&1 | %SEDPATH% -e s!//!! -e s!\.\.\.!---! | find /v /i "- also opened by"
)

goto End

:End
popd
( endlocal
  exit %valveExitArg% 0 )

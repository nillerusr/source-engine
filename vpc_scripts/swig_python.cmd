:: //
:: // Batch file to build .py & .cpp modules from swig.i source files
:: //
:: // SYNTAX:  swig_python.cmd file srcdir outdir pythonver
:: //
:: // EXAMPLE: swig_python.cmd base.i ..\..\.. d:\dev\main\game\sdktools\python\site-packages\vs pythonver 2.5
:: //

@echo off

setlocal

:: // Make sure we have 4 args

if .%1.==.. (
	goto Usage
)

if .%2.==.. (
	goto Usage
)

if .%3.==.. (
	goto Usage
)

if .%4.==.. (
	goto Usage
)

:Main

set SWIGFILE=%1%
set SRCDIR=%2%
set OUTDIR=%3%
set PYTHONVER=%4%
set SWIG=%SRCDIR%\devtools\swigwin-1.3.34\swig.exe
set SWIGDIR=swig_python%PYTHONVER%
set SWIGC=%SWIGFILE%_wrap_python%PYTHONVER%.cpp
set DIFF=%SRCDIR%\devtools\bin\diff.exe -q
set P4EDIT=%SRCDIR%\vpc_scripts\valve_p4_edit.cmd

:: // Check to see if things don't exist and issue inteligible error messages
if NOT EXIST %SWIG% goto NoSwig

if NOT EXIST %DIFF% goto NoDiff

:: // Make the output directory if necessary
if NOT EXIST %SWIGDIR% mkdir %SWIGDIR%

if NOT EXIST %SWIGDIR% goto NoSwigDir

if EXIST %SWIGC% attrib -R %SWIGC%

echo *** [swig_python] %SWIG% -small -Fmicrosoft -ignoremissing -c++ -Iswig_python%PYTHONVER% -I%SRCDIR%\public -o "%SWIGC%" -outdir "%SWIGDIR%" -python "%SWIGFILE%.i" ***
call %SWIG% -small -Fmicrosoft -ignoremissing -c++ -Iswig_python%PYTHONVER% -I%SRCDIR%\public -o "%SWIGC%" -outdir "%SWIGDIR%" -python "%SWIGFILE%.i"
if ERRORLEVEL 1 goto SwigFailed

if EXIST %SWIGC% attrib -R %SWIGC%

if NOT EXIST "%SWIGDIR%\%SWIGFILE%.py" echo "Can't Find diff SRC %SWIGDIR%\%SWIGFILE%.py"
if NOT EXIST "%OUTDIR%\%SWIGFILE%.py" echo "Can't Find diff DST %OUTDIR%\%SWIGFILE%.py"
echo *** [swig_python] %DIFF% "%SWIGDIR%\%SWIGFILE%.py" "%OUTDIR%\%SWIGFILE%.py" ***
call %DIFF% "%SWIGDIR%\%SWIGFILE%.py" "%OUTDIR%\%SWIGFILE%.py" > NUL:
if ERRORLEVEL 1 goto CopyPy
goto EndOk

:CopyPy
echo *** [swig_python] %P4EDIT% %OUTDIR%\%SWIGFILE%.py %SRCDIR% ***
call %P4EDIT% %OUTDIR%\%SWIGFILE%.py %SRCDIR%
if ERRORLEVEL 1 goto P4Failed

echo *** [swig_python] "%SWIGDIR%\%SWIGFILE%.py" "%OUTDIR%\%SWIGFILE%.py" ***
call copy "%SWIGDIR%\%SWIGFILE%.py" "%OUTDIR%\%SWIGFILE%.py" > NUL:
if ERRORLEVEL 1 goto CopyFailed

:EndOk
echo *** [swig_python] Swig Complete!
endlocal
exit /b 0

:Usage
echo  *** [swig_python] Error calling command! No file specified for swig! Usage: swig_python.cmd file srcdir outdir pythonver
endlocal
exit 1

:SwigFailed
echo  *** [swig_python] swig command failed
goto EndError

:P4Failed
echo *** [swig_python] ERROR: %P4EDIT% %OUTDIR%\%SWIGFILE%.py %SRCDIR%
goto EndError

:CopyFailed
echo *** [swig_python] ERROR: copy "%SWIGDIR%\%SWIGFILE%.py" "%OUTDIR%\%SWIGFILE%.py"
goto EndError

:NoSwig
echo *** [swig_python] ERROR: Can't Find SWIG executable "%SWIG%", ensure src/devtools is synced
goto EndError

:NoDiff
echo *** [swig_python] ERROR: Can't Find DIFF executable "%DIFF%", ensure src/devtools is synced
goto EndError

:NoSwigDir
echo *** [swig_python] ERROR: Can't Find Or Create Swig Intermediate Directory "%SWIGDIR%"
goto EndError

:EndError
endlocal
exit 1

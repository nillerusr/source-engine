@echo off
if .%1.== ."". goto usage
if .%2.==.. goto hl2
setlocal
set dest=%2

if %1 == all 		goto all
if %1 == hl2 		goto hl2
if %1 == hl1 		goto hl1
if %1 == cstrike	goto cstrike
if %1 == goldsrc	goto goldsrc


:all
set source=\\hl2vss\hl2vss_mirror\hl2\release\dev
time /t
echo Syncing from $/hl2/release/dev to %dest% (via %source%)
set source=\\hl2vss\hl2vss_mirror\hl2\release\dev
\\hl2vss\hl2vss\win32\robocopy %source% %dest% /e /xo /copy:dat /w:5 /xx /ns /nc /ndl /np /fp /l /njh /njs | \\hl2vss\hl2vss\win32\getmirror.exe %source% %dest%
set source=\\hl2vss\hl1ports_mirror\hl1ports\release\dev\hl1
echo Syncing from $/hl1ports/release/dev/hl1 to %dest%\hl1 (via %source%)
\\hl2vss\hl2vss\win32\robocopy %source% %dest%\hl1 /e /xo /copy:dat /w:5 /xx /ns /nc /ndl /np /fp /l /njh /njs | \\hl2vss\hl2vss\win32\getmirror.exe %source% %dest%\hl1
set source=\\hl2vss\hl1ports_mirror\hl1ports\release\dev\cstrike
echo Syncing from $/hl1ports/release/dev/cstrike to %dest%\cstrike (via %source%)
\\hl2vss\hl2vss\win32\robocopy %source% %dest%\cstrike /e /xo /copy:dat /w:5 /xx /ns /nc /ndl /np /fp /l /njh /njs | \\hl2vss\hl2vss\win32\getmirror.exe %source% %dest%\cstrike
time /t
goto end

:hl2
if .%2.==.. set dest=%1
set source=\\hl2vss\hl2vss_mirror\hl2\release\dev
echo Syncing from $/hl2/release/dev to %dest% (via %source%)
echo.
time /t
\\hl2vss\hl2vss\win32\robocopy %source% %dest% /e /xo /copy:dat /w:5 /xx /ns /nc /ndl /np /fp /l /njh /njs | \\hl2vss\hl2vss\win32\getmirror.exe %source% %dest%
time /t
goto end

:hl1
set source=\\hl2vss\hl1ports_mirror\hl1ports\release\dev\hl1
echo Syncing from $/hl1ports\release\dev\cstrike to %dest%\hl1 (via %source%)
echo.
time /t
\\hl2vss\hl2vss\win32\robocopy %source% %dest%\hl1 /e /xo /copy:dat /w:5 /xx /ns /nc /ndl /np /fp /l /njh /njs | \\hl2vss\hl2vss\win32\getmirror.exe %source% %dest%\hl1
time /t
goto end

:cstrike
set source=\\hl2vss\hl1ports_mirror\hl1ports\release\dev\cstrike
echo Syncing from $/hl1ports\release\dev\cstrike to %dest%\cstrike (via %source%)
echo.
time /t
\\hl2vss\hl2vss\win32\robocopy %source% %dest%\cstrike /e /xo /copy:dat /w:5 /xx /ns /nc /ndl /np /fp /l /njh /njs | \\hl2vss\hl2vss\win32\getmirror.exe %source% %dest%\cstrike
time /t
goto end

:goldsrc
set source=\\goldsource\goldsrc_mirror\hl1\release\dev
echo Syncing from $/hl1\release\dev\ to %dest% (via %source%)
echo.
time /t
\\goldsource\gldsrc\win32\robocopy %source% %dest% /e /xo /copy:dat /w:5 /xx /ns /nc /ndl /np /fp /l /njh /njs | \\goldsource\gldsrc\win32\getmirror.exe %source% %dest%
time /t
goto end

:needdest
:Usage
echo . 	Usage: syncfrommirror.bat [all][hl2][hl1][cstrike][goldsrc][local destination]
echo .
echo .
echo .
echo .	Where		all		sync up to all mirror targets into your destination folder
echo .			hl2		sync up to $/hl2/release/dev into your destination folder
echo .			hl1		sync up to $/hl1ports/release/dev/hl1 into your destination folder
echo .			cstrike		sync up to $/h1ports/release/dev/cstrike into your destination folder
echo .			goldsrc		sync up to $/hl1/release/dev into your destination folder
echo .
echo . 	If you are syncing to hl1 or cstrike, your destination should be your base 
echo . 	folder with the engine in it (e.g. \hl2\release\dev\)
echo .
goto end


:end

@echo off
setlocal

@rem Move to the batch file directory so we can run it from anywhere
cd %~dp0

call "%VS120COMNTOOLS%vsvars32.bat"

p4 edit *.vcxproj
p4 edit *.sln

@rem This will upgrade from v110_xp to v120_xp
devenv cryptest2010.sln /upgrade
@rem Remove upgrade files
rmdir Backup /s/q
rmdir _UpgradeReport_Files /s/q
del UpgradeLog*.*

devenv cryptest2010.sln /rebuild "release|Win32" /project cryptlib
devenv cryptest2010.sln /rebuild "release|x64" /project cryptlib

@rem Temporarily copy the 2013 libraries to a different location to allow
@rem VS 2010 and VS 2012 and VS 2013 to exist side-by-side.
p4 edit ..\..\lib\public\2013\cryptlib.lib
xcopy /y ..\..\lib\public\cryptlib.lib ..\..\lib\public\2013
p4 add ..\..\lib\public\2013\cryptlib.lib
p4 revert ..\..\lib\public\cryptlib.lib

p4 edit ..\..\lib\public\x64\2013\cryptlib.lib
xcopy /y ..\..\lib\public\x64\cryptlib.lib ..\..\lib\public\x64\2013
p4 add ..\..\lib\public\x64\2013\cryptlib.lib
p4 revert ..\..\lib\public\x64\cryptlib.lib

@rem Revert the upgraded libraries
p4 revert *.vcxproj
p4 revert *.sln

@echo Check in the changed libraries in src\lib if you are done.

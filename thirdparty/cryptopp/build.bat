@echo off
setlocal

@rem Move to the batch file directory so we can run it from anywhere
cd %~dp0

@rem Add VS 2013 to the path.
call "%VS120COMNTOOLS%vsvars32.bat"

@rem Build the project -- it's actually VS 2013 project files.
devenv cryptest2010.sln /rebuild "release|Win32" /project cryptlib
devenv cryptest2010.sln /rebuild "release|x64" /project cryptlib

@rem The copying dance is no longer needed but the code is retained 
@rem Temporarily copy the 2013 libraries to a different location to allow
@rem VS 2010 and VS 2013 to exist side-by-side.
@rem p4 edit ..\..\lib\public\2013\cryptlib.lib
@rem xcopy /y ..\..\lib\public\cryptlib.lib ..\..\lib\public\2013\cryptlib.lib
@rem p4 revert ..\..\lib\public\cryptlib.lib

@rem p4 edit ..\..\lib\public\x64\2013\cryptlib.lib
@rem xcopy /y ..\..\lib\public\x64\cryptlib.lib ..\..\lib\public\x64\2013\cryptlib.lib
@rem p4 revert ..\..\lib\public\x64\cryptlib.lib

@echo Check in the changed libraries in src\lib if you are done.

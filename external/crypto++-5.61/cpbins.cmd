
for %%i in (debug release) do (
    ..\..\filecopy_p4.bat Win32\output\%%i\cryptlib.lib   ..\..\lib\win32\%%i\cryptlib.lib ..\..
    ..\..\filecopy_p4.bat  x64\output\%%i\cryptlib.lib    ..\..\lib\win64\%%i\cryptlib.lib ..\..
)

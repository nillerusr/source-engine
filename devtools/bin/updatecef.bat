rem script to update libcef from Steam/main to this project
rem brings over all the dll's and import libraries and support libraries plus updates the header files
rem run this, then resolve the changelist in perforce and submit
SET SRCDIR=//Steam/main
SET PUBLISH_SCRIPT=//thirdpartycode/nonredist/chromeemb/dev/publish_cef_to_steam.py

rem Resolve this to an absolute path
pushd %~dp0..\..\..
SET DSTDIR=%CD%
popd

p4 print -q -o "%TMP%\publish.py" %PUBLISH_SCRIPT%
python "%TMP%\publish.py" --game=tf2 "%DSTDIR%" "%SRCDIR%"
p4 merge %SRCDIR%/client/bin/icudt.dll %DSTDIR%/game/bin/icudt.dll

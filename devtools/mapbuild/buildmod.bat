echo off

if "%1" == "" goto usage

@rem	**********************************************************************
@REM 	If the mapbuild directory ever moves out of src\devools,
@REM 	update this relative path to the tree's "main" directory.
@rem	**********************************************************************
set maindir=..\..\..

if not exist %maindir%\game\%1 goto usage

set vproject=%maindir%\game\%1

@rem	**********************************************************************
@rem	build options are -reslist, -nodegraph, -bsp, and -forcebuild.
@rem 	The "-forcebuild" flag is used to build all changed maps, even if they
@rem	didn't use the 'autocompile' keyword. This is currently being used
@rem 	to build only reslists and nodegraphs of changed maps each night.	
@rem	**********************************************************************

set defaultflags=%2
set buildflags=%defaultflags%

set TIME=
for /F "tokens=1-4 delims=:., " %%a in ("%TIME%") do set TIME=%%a%%b%%c
@rem if %TIME% GTR 030000 set buildflags="-forcebuild -reslist -nodegraph"
@rem if %TIME% GTR 060000 set buildflags=%defaultflags%

@rem	**********************************************************************
@rem	Generate a list of changed vmf's without actually syncing them
@rem	**********************************************************************

p4 sync -n %maindir%\content\%1\maps\*.vmf >> %1_buildlist.txt

@rem	**********************************************************************
@rem	Sync specified vmf's only. If "-forcebuild" flag is set,
@rem	all changed vmf's will be synced. Otherwise, only maps
@rem	that had the "autocompile" keyword in the checkin comments
@rem	will be synced.
@rem	**********************************************************************

syncChangedMaps.pl %1 %buildflags%

if errorlevel 1 goto end

@rem	**********************************************************************
@rem	Sync all other files
@rem	**********************************************************************

p4 sync %maindir%\game\...
p4 sync %maindir%\src\...

@rem	**********************************************************************
@rem	Build bsp's, cubemaps, and checkin
@rem	**********************************************************************

echo compiling %1 maps >> log.txt
time /t >> log.txt

buildMaps.pl -mod %1 -maindir %maindir% %buildflags% %2

echo Finished %1 >> log.txt
time /t >> log.txt
echo. >> log.txt
echo. >> log.txt

goto end

:usage
echo Usage: buildmod [modname]

:end
echo > %1_buildlist.txt

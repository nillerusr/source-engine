#//-----------------------------------------------------------------------------
#//	VSCRIPT.VPC
#//
#//	Project Script
#//-----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)
project(vscript_client)
set(SRCDIR "${CMAKE_SOURCE_DIR}")
set(CMAKE_MODULE_PATH ${SRCDIR}/cmake)
set(OUTBINNAME "vscript_client")
set(OUTBINDIR ${SRCDIR}/../game/bin)

include(${CMAKE_MODULE_PATH}/detect_platform.cmake)
include(${CMAKE_MODULE_PATH}/source_dll_base.cmake)

add_definitions(-DVSCRIPT_DLL_EXPORT)
add_definitions(-DLUA_MOD_CASE_INSENSITIVE)

if( POSIX )
    include_directories(./languages/squirrel/include)
    include_directories(./languages/squirrel/sqplus)
    #include_directories(./languages/lua/lua-5.1.4/src)
endif()


target_sources(${OUTBINNAME} PRIVATE vscript.cpp)
#target_sources(${OUTBINNAME} PRIVATE ./languages/gm/vgm/vgm.cpp)
#if( NOT LINUX )
#    target_sources(${OUTBINNAME} PRIVATE ./languages/lua/vlua/vlua.cpp)
#endif()
target_sources(${OUTBINNAME} PRIVATE ./languages/squirrel/vsquirrel/vsquirrel.cpp)

#TODO: nut builder for nut_init.h? Or hardcode like the others
#			$File	".\languages\squirrel\vsquirrel\init.nut"
#					$CustomBuildStep
#					{
#						$CommandLine	"$SRCDIR\devtools\srcsrv\perl\bin\perl.exe $SRCDIR\devtools\bin\texttoarray.pl $(InputPath) g_Script_$(InputName)> $(InputDir)$(InputName)_nut.h" [$WINDOWS]
#						$CommandLine	"perl $SRCDIR\devtools\bin\texttoarray.pl $(InputPath) g_Script_$(InputName)> $(InputDir)$(InputName)_nut.h" [$POSIX]
#						$Description	"$(InputFileName) produces $(InputName)_nut.h"
#						$Outputs		"$(InputDir)$(InputName)_nut.h"
#					}

#GameMonkey
#EXCLUDED FROM BUILD!
#$ExcludedFromBuild					"Yes"
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmArraySimple.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmByteCode.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmByteCodeGen.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmCodeGen.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmCodeGenHooks.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmCodeTree.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmCrc.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmDebug.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmFunctionObject.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmHash.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmIncGC.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmLibHooks.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmListDouble.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmLog.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmMachine.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmMachineLib.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmMem.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmMemChain.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmMemFixed.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmMemFixedSet.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmOperators.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmParser.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmScanner.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmStream.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmStreamBuffer.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmStringObject.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmTableObject.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmThread.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmUserObject.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmUtil.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/gm/gmVariable.cpp")

#BINDS
#EXCLUDED FROM BUILD!
#$ExcludedFromBuild					"Yes"
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/binds/gmArrayLib.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/binds/gmCall.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/binds/gmGCRoot.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/binds/gmGCRootUtil.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/binds/gmHelpers.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/binds/gmMathLib.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/binds/gmStringLib.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/binds/gmSystemLib.cpp" )
#target_sources(${OUTBINNAME} PRIVATE "./languages/gm/src/binds/gmVector3Lib.cpp")

#Squirrel
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/squirrel/sqapi.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/squirrel/sqbaselib.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/squirrel/sqclass.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/squirrel/sqcompiler.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/squirrel/sqdebug.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/squirrel/sqfuncstate.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/squirrel/sqlexer.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/squirrel/sqmem.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/squirrel/sqobject.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/squirrel/sqstate.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/squirrel/sqtable.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/squirrel/sqvm.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/sqdbg/sqrdbg.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/sqdbg/sqdbgserver.cpp" )

#sqstdlib
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/sqstdlib/sqstdaux.cpp")
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/sqstdlib/sqstdblob.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/sqstdlib/sqstdmath.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/sqstdlib/sqstdrex.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/sqstdlib/sqstdstream.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/sqstdlib/sqstdstring.cpp" )

#sqplus
#				$File ".\languages\squirrel\sqplus\SquirrelBindingsUtilsWin32.cpp" [!$PS3&&!$POSIX]
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/sqplus/SqPlus.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/sqplus/SquirrelBindingsUtils.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/sqplus/SquirrelObject.cpp" )
target_sources(${OUTBINNAME} PRIVATE "./languages/squirrel/sqplus/SquirrelVM.cpp" )


#	$Folder "Lua" [ !$X360 && !$PS3  && !$LINUX]

target_link_libraries(${OUTBINNAME} libvstdlib_client mathlib_client)
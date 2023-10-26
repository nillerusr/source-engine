# Microsoft Developer Studio Project File - Name="Minimal" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=Minimal - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Minimal.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Minimal.mak" CFG="Minimal - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Minimal - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Minimal - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "Perforce Project"
# PROP Scc_LocalPath "..\.."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Minimal - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\..\gm" /I "..\..\platform\win32msvc" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "Minimal - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\gm" /I "..\..\platform\win32msvc" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Minimal - Win32 Release"
# Name "Minimal - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\main.cpp
# End Source File
# End Group
# Begin Group "gm"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\gm\gmArraySimple.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmArraySimple.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmByteCode.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmByteCode.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmByteCodeGen.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmByteCodeGen.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmCodeGen.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmCodeGen.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmCodeGenHooks.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmCodeGenHooks.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmCodeTree.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmCodeTree.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmConfig.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmCrc.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmCrc.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmDebug.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmDebug.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmFunctionObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmFunctionObject.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmHash.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmHash.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmIncGC.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmIncGC.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmIterator.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmLibHooks.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmLibHooks.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmListDouble.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmListDouble.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmLog.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmLog.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmMachine.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmMachine.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmMachineLib.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmMachineLib.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmMem.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmMem.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmMemChain.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmMemChain.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmMemFixed.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmMemFixed.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmMemFixedSet.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmMemFixedSet.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmOperators.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmOperators.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmParser.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmParser.cpp.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmScanner.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmScanner.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmStream.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmStreamBuffer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmStreamBuffer.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmStringObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmStringObject.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmTableObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmTableObject.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmThread.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmThread.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmUserObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmUserObject.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmUtil.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmUtil.h
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmVariable.cpp
# End Source File
# Begin Source File

SOURCE=..\..\gm\gmVariable.h
# End Source File
# End Group
# Begin Group "win32"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\platform\win32msvc\gmConfig_p.h
# End Source File
# End Group
# End Target
# End Project

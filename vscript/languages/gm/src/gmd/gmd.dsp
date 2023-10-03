# Microsoft Developer Studio Project File - Name="gmd" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=gmd - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "gmd.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "gmd.mak" CFG="gmd - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "gmd - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "gmd - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "gmd"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "gmd - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\gm" /I "..\gmd" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 Ws2_32.lib /nologo /subsystem:windows /machine:I386 /out:"..\..\bin\gmd.exe"

!ELSEIF  "$(CFG)" == "gmd - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\gm" /I "..\gmd" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 Ws2_32.lib /nologo /subsystem:windows /debug /machine:I386 /out:"..\..\bin\gmd.debug.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "gmd - Win32 Release"
# Name "gmd - Win32 Debug"
# Begin Group "src"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\ChildView.cpp
# End Source File
# Begin Source File

SOURCE=.\ChildView.h
# End Source File
# Begin Source File

SOURCE=.\gmd.cpp
# End Source File
# Begin Source File

SOURCE=.\gmd.h
# End Source File
# Begin Source File

SOURCE=.\gmd.rc
# End Source File
# Begin Source File

SOURCE=.\GMLangSettings.cpp
# End Source File
# Begin Source File

SOURCE=.\GMLangSettings.h
# End Source File
# Begin Source File

SOURCE=.\LanguageSettings.cpp
# End Source File
# Begin Source File

SOURCE=.\LanguageSettings.h
# End Source File
# Begin Source File

SOURCE=.\MainFrm.cpp
# End Source File
# Begin Source File

SOURCE=.\MainFrm.h
# End Source File
# Begin Source File

SOURCE=.\NetServer.cpp
# End Source File
# Begin Source File

SOURCE=.\NetServer.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\ScintillaEdit.cpp
# End Source File
# Begin Source File

SOURCE=.\ScintillaEdit.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\gmd.ico
# End Source File
# Begin Source File

SOURCE=.\res\gmd.rc2
# End Source File
# Begin Source File

SOURCE=.\res\Toolbar.bmp
# End Source File
# End Group
# Begin Group "scintilla"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\include\Accessor.h
# End Source File
# Begin Source File

SOURCE=.\include\Face.py
# End Source File
# Begin Source File

SOURCE=.\include\HFacer.py
# End Source File
# Begin Source File

SOURCE=.\include\KeyWords.h
# End Source File
# Begin Source File

SOURCE=.\include\Platform.h
# End Source File
# Begin Source File

SOURCE=.\include\PropSet.h
# End Source File
# Begin Source File

SOURCE=.\include\SciLexer.h
# End Source File
# Begin Source File

SOURCE=.\include\Scintilla.h
# End Source File
# Begin Source File

SOURCE=.\include\Scintilla.iface
# End Source File
# Begin Source File

SOURCE=.\include\ScintillaWidget.h
# End Source File
# Begin Source File

SOURCE=.\include\SString.h
# End Source File
# Begin Source File

SOURCE=.\include\WindowAccessor.h
# End Source File
# End Group
# Begin Group "gm"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\gm\gmDebugger.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\gm\gmDebugger.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\res\bangbang.wav
# End Source File
# End Target
# End Project

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: dummy main.cpp
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include "TFStatsApplication.h"


//------------------------------------------------------------------------------------------------------
// Function:	main
// Purpose:	dummy main. passes off execution to TFstats main
// Input:	argc - argument count
//				argv[] - argument list
//------------------------------------------------------------------------------------------------------

void main(int argc, const char* argv[])
{
	//make OS application object, and operating system interface
	g_pApp=new CTFStatsApplication;
	g_pApp->majorVer=1;
	g_pApp->minorVer=5;

#ifdef WIN32
	g_pApp->os=new CTFStatsWin32Interface();
#else
	g_pApp->os=new CTFStatsLinuxInterface();
#endif
	//hand off execution to real main
	g_pApp->main(argc,argv);
}

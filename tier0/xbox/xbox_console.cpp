//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Xbox console link
//
//=====================================================================================//

#include "pch_tier0.h"
#include "xbox/xbox_console.h"
#include "tier0/memdbgon.h"

IXboxConsole *g_pXboxConsole;

typedef IXboxConsole * (WINAPI *CONSOLEINTERFACEFUNC)( void );

void XboxConsoleInit()
{
	g_pXboxConsole = NULL;

	HMODULE hDLL = ::LoadLibrary( "vxbdm_360.dll" );
	if ( !hDLL )
	{
		return;
	}

	CONSOLEINTERFACEFUNC fpnGetConsoleInterface = (CONSOLEINTERFACEFUNC) ::GetProcAddress( hDLL, "GetConsoleInterface" );

	if ( fpnGetConsoleInterface )
	{
		g_pXboxConsole = fpnGetConsoleInterface();
	}
}

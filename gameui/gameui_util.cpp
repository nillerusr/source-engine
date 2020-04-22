//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//

#include <stdarg.h>
#include "gameui_util.h"
#include "strtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Performs a var args printf into a static return buffer
// Input  : *format - 
//			... - 
// Output : char
//-----------------------------------------------------------------------------
char *VarArgs( const char *format, ... )
{
	va_list		argptr;
	static char		string[1024];
	
	va_start (argptr, format);
	Q_vsnprintf (string, sizeof( string ), format,argptr);
	va_end (argptr);

	return string;	
}

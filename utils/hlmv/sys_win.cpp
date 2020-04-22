//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: abstract system dependent functions
//
// $NoKeywords: $
//=============================================================================//
#include "sys.h"
#include <windows.h>
#include "tier1/strtools.h"


void Sys_CopyStringToClipboard( const char *pOut )
{
	if ( !pOut || !OpenClipboard( NULL ) )
	{
		return;
	}
	// Remove the current Clipboard contents
	if( !EmptyClipboard() )
	{
		return;
	}
	HGLOBAL clipbuffer;
	char *buffer;
	EmptyClipboard();
	
	int len = Q_strlen(pOut)+1;
	clipbuffer = GlobalAlloc(GMEM_DDESHARE, len );
	buffer = (char*)GlobalLock( clipbuffer );
	Q_strncpy( buffer, pOut, len );
	GlobalUnlock( clipbuffer );

	SetClipboardData( CF_TEXT,clipbuffer );

	CloseClipboard();
}


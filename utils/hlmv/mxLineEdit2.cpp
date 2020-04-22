//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <windows.h>
#include <richedit.h>
#include "mxLineEdit2.h"



mxLineEdit2::mxLineEdit2( mxWindow *parent, int x, int y, int w, int h, const char *label, int id, int style )
	: mxLineEdit( parent, x, y, w, h, label, id, style )
{
}


void mxLineEdit2::getText( char *pOut, int len )
{
	GetWindowText( (HWND) getHandle (), pOut, len );
	pOut[len-1] = 0;
}


void mxLineEdit2::setText( const char *pText )
{
	setLabel( "%s", pText );
}



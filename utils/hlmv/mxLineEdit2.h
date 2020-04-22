//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MXLINEEDIT2_H
#define MXLINEEDIT2_H
#ifdef _WIN32
#pragma once
#endif


#ifndef INCLUDED_MXWINDOW
#include <mxtk/mxWindow.h>
#endif
#include <mxtk/mxLineEdit.h>


// Extends the (mostly unimplemented) mxLineEdit control.
class mxLineEdit2 : public mxLineEdit
{
public:
	mxLineEdit2( mxWindow *parent, int x, int y, int w, int h, const char *label = 0, int id = 0, int style = 0 );

	void getText( char *pOut, int len );
	void setText( const char *pText );
};


#endif // MXLINEEDIT2_H

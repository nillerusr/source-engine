//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( MXBITMAPWINDOW_H )
#define MXBITMAPWINDOW_H
#ifdef _WIN32
#pragma once
#endif

#include "mxBitmapTools.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class mxBitmapWindow : public mxWindow
{
public:
						mxBitmapWindow( mxWindow *parent, int x, int y, int w, int h, int style = 0, const char *bitmap = 0 );
	virtual				~mxBitmapWindow ( void );

	virtual void		setImage( const char *bitmap );

	virtual void		redraw ();

	virtual bool		Load( const char *bitmap );
private:

	mxbitmapdata_t		m_Bitmap;

};

#endif // MXBITMAPWINDOW_H
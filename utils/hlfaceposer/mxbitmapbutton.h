//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MXBITMAPBUTTON_H
#define MXBITMAPBUTTON_H
#ifdef _WIN32
#pragma once
#endif


#include <mxtk/mx.h>
#include "mxBitmapTools.h"

class mxBitmapButton : public mxWindow
{
public:
	mxBitmapButton( mxWindow *parent, int x, int y, int w, int h, int id = 0, const char *bitmap = 0 );
	~mxBitmapButton( void );

	virtual void redraw();
	virtual int handleEvent( mxEvent * event );

	void SetImage( const char *bitmapname );

private:
	void DeleteImage( void );

	mxbitmapdata_t	m_bmImage;
};
#endif // MXBITMAPBUTTON_H

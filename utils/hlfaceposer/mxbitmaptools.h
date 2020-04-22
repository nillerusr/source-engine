//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( MXBITMAPTOOLS_H )
#define MXBITMAPTOOLS_H
#ifdef _WIN32
#pragma once
#endif

struct mxbitmapdata_t
{
	mxbitmapdata_t()
	{
		valid = false;
		image = 0;
		width = 0;
		height = 0;
	}

	bool		valid;
	void		*image;
	int			width;
	int			height;
};

class mxWindow;

bool LoadBitmapFromFile( const char *relative, mxbitmapdata_t& bitmap );
void DrawBitmapToWindow( mxWindow *wnd, int x, int y, int w, int h, mxbitmapdata_t& bitmap );
void DrawBitmapToDC( void *hdc, int x, int y, int w, int h, mxbitmapdata_t& bitmap );

#endif // MXBITMAPTOOLS_H
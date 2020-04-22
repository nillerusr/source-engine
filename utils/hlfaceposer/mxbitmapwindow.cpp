//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include <mxtk/mxWindow.h>
#include "mxBitmapWindow.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "tier0/dbg.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			x - 
//			y - 
//			w - 
//			h - 
//			0 - 
//			0 - 
//-----------------------------------------------------------------------------
mxBitmapWindow::mxBitmapWindow(mxWindow *parent, int x, int y, int w, int h, int style /*= 0*/, const char *bitmap /*=0*/ )
: mxWindow( parent, x, y, w, h, "", style )
{
	m_Bitmap.valid = false;

	if ( bitmap && bitmap[ 0 ] )
	{
		Load( bitmap );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
mxBitmapWindow::~mxBitmapWindow( void )
{
	if ( m_Bitmap.valid )
	{
		DeleteObject( m_Bitmap.image );
		m_Bitmap.valid = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *bitmap - 
// Output : virtual void
//-----------------------------------------------------------------------------
void mxBitmapWindow::setImage( const char *bitmap )
{
	if ( m_Bitmap.valid )
	{
		DeleteObject( m_Bitmap.image );
		m_Bitmap.valid = NULL;
	}
	if ( bitmap && bitmap[ 0 ] )
	{
		Load( bitmap );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *bitmap - 
// Output : mxImage
//-----------------------------------------------------------------------------
bool mxBitmapWindow::Load( const char *bitmap )
{
	Assert( !m_Bitmap.valid );
	return LoadBitmapFromFile( bitmap, m_Bitmap );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void mxBitmapWindow::redraw
//-----------------------------------------------------------------------------
void mxBitmapWindow::redraw ()
{
	DrawBitmapToWindow( this, 0, 0, w(), h(), m_Bitmap );
}
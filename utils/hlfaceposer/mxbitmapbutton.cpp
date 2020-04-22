//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "mxBitmapButton.h"
#include "hlfaceposer.h"


mxBitmapButton::mxBitmapButton( mxWindow *parent, int x, int y, int w, int h, int id /*= 0*/, const char *bitmap /* = 0 */ )
: mxWindow( parent, x, y, w, h, "" )
{
	setId( id );

	m_bmImage.valid = false;

	SetImage( bitmap );

	HWND wnd = (HWND)getHandle();

	DWORD style = GetWindowLong( wnd, GWL_STYLE );
	style |= WS_CLIPSIBLINGS;
	SetWindowLong( wnd, GWL_STYLE, style );
}

mxBitmapButton::~mxBitmapButton( void )
{
	DeleteImage();
}

void mxBitmapButton::redraw()
{
	HWND wnd = (HWND)getHandle();
	if ( !wnd )
		return;

	if ( !m_bmImage.valid )
		return;

	RECT rc;
	GetClientRect( wnd, &rc );
	
	HDC dc = GetDC( wnd );

	DrawBitmapToDC( dc, 0, 0, w(), h(), m_bmImage );

	ReleaseDC( wnd, dc );

	ValidateRect( wnd, &rc );
}

int mxBitmapButton::handleEvent( mxEvent * event )
{
	int iret = 0;

	switch (event->event)
	{
	case mxEvent::MouseUp:
		// Send message to parent
		HWND parent = (HWND)( getParent() ? getParent()->getHandle() : NULL );
		if ( parent )
		{
			LPARAM lp;
			WPARAM wp;

			wp = MAKEWPARAM( getId(), BN_CLICKED );
			lp = (long)getHandle();

			SendMessage( parent, WM_COMMAND, wp, lp );
			iret = 1;
		}
		break;
	}

	return iret;
}

void mxBitmapButton::SetImage( const char *bitmapname )
{
	if ( m_bmImage.valid )
	{
		DeleteImage();
	}

	LoadBitmapFromFile( bitmapname, m_bmImage );
}

void mxBitmapButton::DeleteImage( void )
{
	if ( m_bmImage.valid )
	{
		DeleteObject( m_bmImage.image );
		m_bmImage.valid = false;
	}
}

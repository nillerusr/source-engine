//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "BitmapImagePanel.h"
#include <vgui/ISurface.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

CBitmapImagePanel::CBitmapImagePanel( Panel *parent, char const *panelName, 
	char const *filename /*= NULL*/ ) : Panel( parent, panelName )
{
	m_szTexture[ 0 ] = 0;
	m_bUploaded = false;
	m_nTextureId = -1;

	SetBounds( 0, 0, 100, 100 );

	if ( filename && filename[ 0 ] )
	{
		Q_strncpy( m_szTexture, filename, sizeof( m_szTexture ) );
	}
}

void CBitmapImagePanel::PaintBackground()
{
	if (!m_szTexture[0])
		return;

	if ( !m_bUploaded )
		forceUpload();

	int w, h;
	GetSize( w, h );
	surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
	surface()->DrawSetTexture( m_nTextureId );
	surface()->DrawTexturedRect( 0, 0, w, h );
}

void CBitmapImagePanel::setTexture( char const *filename )
{
	Q_strncpy( m_szTexture, filename, sizeof( m_szTexture ) );

	if ( m_bUploaded )
	{
		forceReload();
	}
	else
	{
		forceUpload();
	}
}

void CBitmapImagePanel::forceUpload()
{
	if ( !m_szTexture[ 0 ] )
		return;

	m_bUploaded = true;

	m_nTextureId = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_nTextureId, m_szTexture, false, true);
}

void CBitmapImagePanel::forceReload( void )
{
	if ( !m_bUploaded )
		return;

	// Force texture to re-upload to video card
	surface()->DrawSetTextureFile( m_nTextureId, m_szTexture, false, true);
}

//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_textwindow.h"
#include <cdll_client_int.h>

#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <filesystem.h>
#include <KeyValues.h>
#include <convar.h>
#include <vgui_controls/ImageList.h>

#include <vgui_controls/Panel.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/BuildGroup.h>
#include <vgui_controls/ImagePanel.h>

#include "tf_controls.h"
#include "tf_shareddefs.h"

#include "IGameUIFuncs.h" // for key bindings
#include <igameresources.h>
extern IGameUIFuncs *gameuifuncs; // for key binding details

#include <game/client/iviewport.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFTextWindow::CTFTextWindow( IViewPort *pViewPort ) : CTextWindow( pViewPort )
{
	m_pTFTextMessage = new CTFRichText( this, "TFTextMessage" );

	SetProportional( true );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTFTextWindow::~CTFTextWindow()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTextWindow::ApplySchemeSettings( IScheme *pScheme )
{
	Frame::ApplySchemeSettings( pScheme );  // purposely skipping the CTextWindow version

	LoadControlSettings("Resource/UI/TextWindow.res");

	Reset();

	if ( m_pHTMLMessage )
	{
		m_pHTMLMessage->SetBgColor( pScheme->GetColor( "HTMLBackground", Color( 255, 0, 0, 255 ) ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTextWindow::Reset( void )
{
	Update();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTextWindow::Update()
{
	CTFLabel *pTitle = dynamic_cast<CTFLabel *>( FindChildByName( "TFMessageTitle" ) );
	if ( pTitle )
	{
		pTitle->SetText( m_szTitle );
	}

	if ( m_pTFTextMessage )
	{
		m_pTFTextMessage->SetVisible( false );
	}

	BaseClass::Update();

	Panel *pOK = FindChildByName( "ok" );
	if ( pOK )
	{
		pOK->RequestFocus();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//---------------------------------------------------------------------------
void CTFTextWindow::SetVisible( bool state )
{
	BaseClass::SetVisible( state );

	if ( state )
	{
		Panel *pOK = FindChildByName( "ok" );
		if ( pOK )
		{
			pOK->RequestFocus();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: shows the text window
//-----------------------------------------------------------------------------
void CTFTextWindow::ShowPanel( bool bShow )
{
	if ( IsVisible() == bShow )
		return;

	BaseClass::ShowPanel( bShow );

	if ( m_pViewPort )
	{
		m_pViewPort->ShowBackGround( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTextWindow::OnKeyCodePressed( KeyCode code )
{
	if ( code == KEY_XBUTTON_A )
	{
		OnCommand( "okay" );		
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: The background is painted elsewhere, so we should do nothing
//-----------------------------------------------------------------------------
void CTFTextWindow::PaintBackground()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTextWindow::OnCommand( const char *command )
{
	if ( !Q_strcmp( command, "okay" ) )
	{
		m_pViewPort->ShowPanel( this, false );
		m_pViewPort->ShowPanel( PANEL_MAPINFO, true );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTextWindow::ShowText( const char *text )
{
	ShowTitleLabel( true );

	if ( m_pTFTextMessage )
	{
		m_pTFTextMessage->SetVisible( true );
		m_pTFTextMessage->SetText( text );
		m_pTFTextMessage->GotoTextStart();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTextWindow::ShowURL( const char *URL, bool bAllowUserToDisable /*= true*/ )
{
	ShowTitleLabel( false )	;
	BaseClass::ShowURL( URL, bAllowUserToDisable );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTextWindow::ShowFile( const char *filename )
{
	ShowTitleLabel( false )	;
	BaseClass::ShowFile( filename );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTextWindow::ShowTitleLabel( bool show )
{
	CTFLabel *pTitle = dynamic_cast<CTFLabel *>( FindChildByName( "TFMessageTitle" ) );
	if ( pTitle )
	{
		pTitle->SetVisible( show );
	}
}

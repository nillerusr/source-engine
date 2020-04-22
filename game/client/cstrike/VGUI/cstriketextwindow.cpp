//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "cstriketextwindow.h"
#include "backgroundpanel.h"
#include <cdll_client_int.h>

#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <filesystem.h>
#include <KeyValues.h>
#include <convar.h>
#include <vgui_controls/ImageList.h>

#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/BuildGroup.h>

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
CCSTextWindow::CCSTextWindow(IViewPort *pViewPort) : CTextWindow( pViewPort )
{
	SetProportional( true );

	m_iScoreBoardKey = BUTTON_CODE_INVALID; // this is looked up in Activate()

	CreateBackground( this );
	m_backgroundLayoutFinished = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSTextWindow::~CCSTextWindow()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSTextWindow::Update()
{
	BaseClass::Update();

	m_pOK->RequestFocus();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSTextWindow::SetVisible(bool state)
{
	BaseClass::SetVisible(state);

	if ( state )
	{
		m_pOK->RequestFocus();
	}
}

//-----------------------------------------------------------------------------
// Purpose: shows the text window
//-----------------------------------------------------------------------------
void CCSTextWindow::ShowPanel(bool bShow)
{
	if ( bShow )
	{
		// get key binding if shown
		if ( m_iScoreBoardKey == BUTTON_CODE_INVALID ) // you need to lookup the jump key AFTER the engine has loaded
		{
			m_iScoreBoardKey = gameuifuncs->GetButtonCodeForBind( "showscores" );
		}
	}

	BaseClass::ShowPanel( bShow );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSTextWindow::OnKeyCodePressed( KeyCode code )
{
	//We manually intercept the ENTER key so in case the button loses focus
	//ENTER still moves you through the MOTD screen.
	if ( code == KEY_ENTER || code == KEY_XBUTTON_A || code == KEY_XBUTTON_B )
	{
		m_pOK->DoClick();
	}	
	else if ( m_iScoreBoardKey != BUTTON_CODE_INVALID && m_iScoreBoardKey == code )
	{
		gViewPortInterface->ShowPanel( PANEL_SCOREBOARD, true );
		gViewPortInterface->PostMessageToPanel( PANEL_SCOREBOARD, new KeyValues( "PollHideCode", "code", code ) );
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: The CS background is painted by image panels, so we should do nothing
//-----------------------------------------------------------------------------
void CCSTextWindow::PaintBackground()
{
}

//-----------------------------------------------------------------------------
// Purpose: Scale / center the window
//-----------------------------------------------------------------------------
void CCSTextWindow::PerformLayout()
{
	BaseClass::PerformLayout();

	// stretch the window to fullscreen
	if ( !m_backgroundLayoutFinished )
		LayoutBackgroundPanel( this );
	m_backgroundLayoutFinished = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSTextWindow::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	ApplyBackgroundSchemeSettings( this, pScheme );
}


//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "dodtextwindow.h"
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

#include "dodbutton.h"

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
CDODTextWindow::CDODTextWindow(IViewPort *pViewPort) : CTextWindow( pViewPort )
{
	SetProportional( true );

	m_iScoreBoardKey = BUTTON_CODE_INVALID;

	m_pBackground = SETUP_PANEL( new CDODMenuBackground( this ) );

	// Do this again ( base class already does it )
	// If we don't, custom controls that we catch in our CreateControlByName
	// will not go to the CDODTextWindow version as we haven't instantiated 
	// ourselves as a CDODTextWindow yet.
	LoadControlSettings("Resource/UI/TextWindow.res");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDODTextWindow::~CDODTextWindow()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODTextWindow::Update()
{
	BaseClass::Update();

	Panel *ok = FindChildByName("okbutton");
	if (ok)
	{
		ok->RequestFocus();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODTextWindow::SetVisible(bool state)
{
	BaseClass::SetVisible(state);

	if ( state )
	{
		Panel *ok = FindChildByName("okbutton");
		if (ok)
		{
			ok->RequestFocus();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: shows the text window
//-----------------------------------------------------------------------------
void CDODTextWindow::ShowPanel(bool bShow)
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
void CDODTextWindow::OnKeyCodePressed(KeyCode code)
{
	if ( m_iScoreBoardKey != BUTTON_CODE_INVALID && m_iScoreBoardKey == code )
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
// Purpose: The background is painted elsewhere, so we should do nothing
//-----------------------------------------------------------------------------
void CDODTextWindow::PaintBackground()
{
}

//-----------------------------------------------------------------------------
// Purpose: Scale / center the window
//-----------------------------------------------------------------------------
void CDODTextWindow::PerformLayout()
{
	BaseClass::PerformLayout();	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODTextWindow::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Panel *CDODTextWindow::CreateControlByName( const char *controlName )
{
	if( !Q_stricmp( "DODButton", controlName ) )
	{
		return new CDODButton(this);
	}
	else
	{
		return BaseClass::CreateControlByName( controlName );
	}
}
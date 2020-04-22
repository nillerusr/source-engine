//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client DLL VGUI2 Viewport
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#pragma warning( disable : 4800  )  // disable forcing int to bool performance warning

// VGUI panel includes
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include <KeyValues.h>
#include <vgui/Cursor.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <vgui/VGUI.h>

// client dll/engine defines
#include "hud.h"
#include <voice_status.h>

// viewport definitions
#include <baseviewport.h>
#include "TFCViewport.h"
#include "tfcteammenu.h"

#include "vguicenterprint.h"
#include "text_message.h"
#include "tfcclassmenu.h"



//
// This is the main function of the viewport. Right here is where we create our class menu, 
// team menu, and anything else that we want to turn on and off in the UI.
//
void TFCViewport::CreateDefaultPanels( void )
{
	AddNewPanel( new CTFCTeamMenu( this ), "CTFCTeamMenu" );
	AddNewPanel( new CTFCClassMenu( this ), "CTFCClassMenu" );

	BaseClass::CreateDefaultPanels();
}


void TFCViewport::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	gHUD.InitColors( pScheme );

	SetPaintBackgroundEnabled( false );
}


IViewPortPanel* TFCViewport::CreatePanelByName(const char *szPanelName)
{
	IViewPortPanel* newpanel = NULL;

// Up here, strcmp against each type of panel we know how to create.
//	else if ( Q_strcmp(PANEL_OVERVIEW, szPanelName) == 0 )
//	{
//		newpanel = new CCSMapOverview( this );
//	}

	// create a generic base panel, don't add twice
	newpanel = BaseClass::CreatePanelByName( szPanelName );

	return newpanel; 
}

int TFCViewport::GetDeathMessageStartHeight( void )
{
	int x = YRES(2);

	IViewPortPanel *spectator = gViewPortInterface->FindPanelByName( PANEL_SPECGUI );

	//TODO: Link to actual height of spectator bar
	if ( spectator && spectator->IsVisible() )
	{
		x += YRES(52);
	}

	return x;
}


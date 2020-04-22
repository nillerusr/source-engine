//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tfcclassmenu.h"

#include <KeyValues.h>
#include <filesystem.h>
#include <vgui_controls/Button.h>
#include <vgui/IVGui.h>

#include "hud.h" // for gEngfuncs
#include "tfc_gamerules.h"
#include "viewport_panel_names.h"
#include "c_tfc_player.h"


using namespace vgui;


// ----------------------------------------------------------------------------- //
// CTFCClassMenu
// ----------------------------------------------------------------------------- //

CTFCClassMenu::CTFCClassMenu(IViewPort *pViewPort) : CClassMenu(pViewPort)
{
}

const char *CTFCClassMenu::GetName( void ) 
{ 
	return PANEL_CLASS; 
}

void CTFCClassMenu::ShowPanel(bool bShow)
{
	if ( bShow)
	{
		engine->CheckPoint( "ClassMenu" );
	}

	BaseClass::ShowPanel( bShow );

}

void CTFCClassMenu::Update()
{
	// Force them to pick a class if they haven't picked one yet.
	if ( C_TFCPlayer::GetLocalTFCPlayer()->m_Shared.GetPlayerClass() == PC_UNDEFINED )
	{
		SetVisibleButton( "CancelButton", false );
	}
	else
	{
		SetVisibleButton( "CancelButton", true ); 
	}
}


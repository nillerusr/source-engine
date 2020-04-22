//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "BasePanel.h"
#include "ControllerDialog.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


CControllerDialog::CControllerDialog( vgui::Panel *parent ) : BaseClass( parent, true )	// TRUE second param says we want the controller options
{
}

void CControllerDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	SetControlString( "TitleLabel", "#GameUI_Controller" );
}

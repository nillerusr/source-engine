//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "cstrikebuyequipmenu.h"
#include "cs_shareddefs.h"
#include "cstrikebuysubmenu.h"
#include "backgroundpanel.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor for CT Equipment menu
//-----------------------------------------------------------------------------
CCSBuyEquipMenu_CT::CCSBuyEquipMenu_CT(IViewPort *pViewPort) : CBuyMenu( pViewPort )
{
	SetTitle( "#Cstrike_Buy_Menu", true);

	SetProportional( true );

	m_pMainMenu = new CCSBuySubMenu( this, "BuySubMenu" );
	m_pMainMenu->LoadControlSettings( "Resource/UI/BuyEquipment_CT.res" );
	m_pMainMenu->SetVisible( false );

	m_iTeam = TEAM_CT;

	CreateBackground( this );
	m_backgroundLayoutFinished = false;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor for Terrorist Equipment menu
//-----------------------------------------------------------------------------
CCSBuyEquipMenu_TER::CCSBuyEquipMenu_TER(IViewPort *pViewPort) : CBuyMenu( pViewPort )
{
	SetTitle( "#Cstrike_Buy_Menu", true);

	SetProportional( true );

	m_pMainMenu = new CCSBuySubMenu( this, "BuySubMenu" );
	m_pMainMenu->LoadControlSettings( "Resource/UI/BuyEquipment_TER.res" );
	m_pMainMenu->SetVisible( false );

	m_iTeam = TEAM_TERRORIST;

	CreateBackground( this );
	m_backgroundLayoutFinished = false;
}

//-----------------------------------------------------------------------------
// Purpose: The CS background is painted by image panels, so we should do nothing
//-----------------------------------------------------------------------------
void CCSBuyEquipMenu_CT::PaintBackground()
{
}

//-----------------------------------------------------------------------------
// Purpose: Scale / center the window
//-----------------------------------------------------------------------------
void CCSBuyEquipMenu_CT::PerformLayout()
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
void CCSBuyEquipMenu_CT::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	ApplyBackgroundSchemeSettings( this, pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: The CS background is painted by image panels, so we should do nothing
//-----------------------------------------------------------------------------
void CCSBuyEquipMenu_TER::PaintBackground()
{
}

//-----------------------------------------------------------------------------
// Purpose: Scale / center the window
//-----------------------------------------------------------------------------
void CCSBuyEquipMenu_TER::PerformLayout()
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
void CCSBuyEquipMenu_TER::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	ApplyBackgroundSchemeSettings( this, pScheme );
}


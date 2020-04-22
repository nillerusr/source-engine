//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_obj_base_manned_gun.h"
#include "hudelement.h"
#include "tf_movedata.h"
#include "bone_setup.h"
#include "hud_ammo.h"
#include "vgui_bitmapbutton.h"

extern ConVar mannedgun_usethirdperson;

//=================================================================================================
// Control Screen
//=================================================================================================

DECLARE_VGUI_SCREEN_FACTORY( CMannedPlasmagunControlPanel, "manned_plasmagun_control_panel" );

//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CMannedPlasmagunControlPanel::CMannedPlasmagunControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CMannedPlasmagunControlPanel" ) 
{
}


//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CMannedPlasmagunControlPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	m_pMannedLabel = new vgui::Label( this, "MannedReadout", "" );
	m_pOccupyButton = new CBitmapButton( this, "OccupyButton", "Occupy" );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CMannedPlasmagunControlPanel::OnTick()
{
	BaseClass::OnTick();

	C_BaseObject *pObj = GetOwningObject();
	if (!pObj)
		return;

	Assert( dynamic_cast<C_ObjectBaseMannedGun*>(pObj) );
	C_ObjectBaseMannedGun *pGun = static_cast<C_ObjectBaseMannedGun*>(pObj);

	char buf[256];
	// Update the currently manned player label
	if ( pGun->GetDriverPlayer() )
	{
		Q_snprintf( buf, sizeof( buf ), "Manned by %s", pGun->GetDriverPlayer()->GetPlayerName() );
		m_pMannedLabel->SetText( buf );
		m_pMannedLabel->SetVisible( true );
	}
	else
	{
		m_pMannedLabel->SetVisible( false );
	}

	// Update the get in button
	if ( pGun->GetDriverPlayer() )
	{
		// Owners can boot other players to get in
		if ( pGun->GetOwner() == C_BaseTFPlayer::GetLocalPlayer() && C_BaseTFPlayer::GetLocalPlayer() != pGun->GetDriverPlayer() )
		{
			Q_snprintf( buf, sizeof( buf ), "Get In (Ejecting %s)", pGun->GetDriverPlayer()->GetPlayerName() );
			m_pMannedLabel->SetText( buf );
			m_pOccupyButton->SetEnabled( true );
		}
		else
		{
			// Disable the button
			m_pOccupyButton->SetEnabled( false );
		}
	}
	else
	{
		m_pOccupyButton->SetText( "Get In" );
		m_pOccupyButton->SetEnabled( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle clicking on the Occupy button
//-----------------------------------------------------------------------------
void CMannedPlasmagunControlPanel::GetInGun( void )
{
	C_BaseObject *pObj = GetOwningObject();
	if (pObj)
	{
		pObj->SendClientCommand( "toggle_use" );
	}
}

//-----------------------------------------------------------------------------
// Button click handlers
//-----------------------------------------------------------------------------
void CMannedPlasmagunControlPanel::OnCommand( const char *command )
{
	if (!Q_strnicmp(command, "Occupy", 7))
	{
		GetInGun();
		return;
	}

	BaseClass::OnCommand(command);
}


//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_basefourwheelvehicle.h"
#include "tf_movedata.h"
#include "ObjectControlPanel.h"
#include <vgui_controls/Label.h>
#include "vgui_bitmapbutton.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_VehicleBatteringRam : public C_BaseTFFourWheelVehicle
{
	DECLARE_CLASS( C_VehicleBatteringRam, C_BaseTFFourWheelVehicle );
public:
	DECLARE_CLIENTCLASS();

	C_VehicleBatteringRam();

public:
	// IClientVehicle overrides
	virtual bool IsPassengerUsingStandardWeapons( int nRole );
	virtual void UpdateViewAngles( C_BasePlayer *pLocalPlayer, CUserCmd *pCmd );

private:
	C_VehicleBatteringRam( const C_VehicleBatteringRam & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_VehicleBatteringRam, DT_VehicleBatteringRam, CVehicleBatteringRam)
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_VehicleBatteringRam::C_VehicleBatteringRam()
{
}


//-----------------------------------------------------------------------------
// Does the player use his normal weapons while in this mode?
//-----------------------------------------------------------------------------
bool C_VehicleBatteringRam::IsPassengerUsingStandardWeapons( int nRole )
{
	return (nRole > 1);
}

//-----------------------------------------------------------------------------
// Clamps the view angles while manning the gun 
//-----------------------------------------------------------------------------
void C_VehicleBatteringRam::UpdateViewAngles( C_BasePlayer *pLocalPlayer, CUserCmd *pCmd )
{
	int nRole = GetPassengerRole( pLocalPlayer );

	// Restrict the view of the 1st passenger
	if (nRole == 1)
	{
		RestrictView( nRole, -90, 90, pCmd->viewangles );
	}
}

//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CVehicleBatteringRamControlPanel : public CObjectControlPanel
{
	DECLARE_CLASS( CVehicleBatteringRamControlPanel, CObjectControlPanel );

public:
	CVehicleBatteringRamControlPanel( vgui::Panel *parent, const char *panelName );
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();
	virtual void OnCommand( const char *command );

private:
	void GetInRam( void );

private:
	vgui::Label		*m_pDriverLabel;
	vgui::Label		*m_pPassengerLabel;
	vgui::Button	*m_pOccupyButton;
};


DECLARE_VGUI_SCREEN_FACTORY( CVehicleBatteringRamControlPanel, "vehicle_battering_ram_control_panel" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CVehicleBatteringRamControlPanel::CVehicleBatteringRamControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CVehicleBatteringRamControlPanel" ) 
{
}


//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CVehicleBatteringRamControlPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	m_pDriverLabel = new vgui::Label( this, "DriverReadout", "" );
	m_pPassengerLabel = new vgui::Label( this, "PassengerReadout", "" );
	m_pOccupyButton = new CBitmapButton( this, "OccupyButton", "Occupy" );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CVehicleBatteringRamControlPanel::OnTick()
{
	BaseClass::OnTick();

	C_BaseObject *pObj = GetOwningObject();
	if (!pObj)
		return;

	Assert( dynamic_cast<C_VehicleBatteringRam*>(pObj) );
	C_VehicleBatteringRam *pRam = static_cast<C_VehicleBatteringRam*>(pObj);

	char buf[256];
	// Update the currently manned player label
	if ( pRam->GetDriverPlayer() )
	{
		Q_snprintf( buf, sizeof( buf ), "Driven by %s", pRam->GetDriverPlayer()->GetPlayerName() );
		m_pDriverLabel->SetText( buf );
		m_pDriverLabel->SetVisible( true );
	}
	else
	{
		m_pDriverLabel->SetVisible( false );
	}

	int nPassengerCount = pRam->GetPassengerCount();
	int nMaxPassengerCount = pRam->GetMaxPassengerCount();

	Q_snprintf( buf, sizeof( buf ), "Passengers %d/%d", nPassengerCount >= 1 ? nPassengerCount - 1 : 0, nMaxPassengerCount - 1 );
	m_pPassengerLabel->SetText( buf );

	// Update the get in button
	if ( pRam->IsPlayerInVehicle( C_BaseTFPlayer::GetLocalPlayer() ) ) 
	{
		m_pOccupyButton->SetEnabled( false );
		return;
	}

	if ( pRam->GetOwner() == C_BaseTFPlayer::GetLocalPlayer() )
	{
		if (nPassengerCount == nMaxPassengerCount)
		{
			// Owners can boot other players to get in
			C_BaseTFPlayer *pPlayer = static_cast<C_BaseTFPlayer*>(pRam->GetPassenger( VEHICLE_ROLE_DRIVER ));
			Q_snprintf( buf, sizeof( buf ), "Get In (Ejecting %s)", pPlayer->GetPlayerName() );
			m_pDriverLabel->SetText( buf );
			m_pOccupyButton->SetEnabled( true );
		}
		else
		{
			m_pOccupyButton->SetText( "Get In" );
			m_pOccupyButton->SetEnabled( true );
		}
	}
	else
	{
		m_pOccupyButton->SetText( "Get In" );
		m_pOccupyButton->SetEnabled( pRam->GetPassengerCount() < pRam->GetMaxPassengerCount() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle clicking on the Occupy button
//-----------------------------------------------------------------------------
void CVehicleBatteringRamControlPanel::GetInRam( void )
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
void CVehicleBatteringRamControlPanel::OnCommand( const char *command )
{
	if (!Q_strnicmp(command, "Occupy", 7))
	{
		GetInRam();
		return;
	}

	BaseClass::OnCommand(command);
}


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
#include <vgui_controls/Button.h>


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_VehicleWagon : public C_BaseTFFourWheelVehicle
{
	DECLARE_CLASS( C_VehicleWagon, C_BaseTFFourWheelVehicle );
public:
	DECLARE_CLIENTCLASS();

	C_VehicleWagon();

private:
	C_VehicleWagon( const C_VehicleWagon & );		// not defined, not accessible

};


IMPLEMENT_CLIENTCLASS_DT(C_VehicleWagon, DT_VehicleWagon, CVehicleWagon)
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_VehicleWagon::C_VehicleWagon()
{
}


//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CVehicleWagonControlPanel : public CObjectControlPanel
{
	DECLARE_CLASS( CVehicleWagonControlPanel, CObjectControlPanel );

public:
	CVehicleWagonControlPanel( vgui::Panel *parent, const char *panelName );
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


DECLARE_VGUI_SCREEN_FACTORY( CVehicleWagonControlPanel, "vehicle_wagon_control_panel" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CVehicleWagonControlPanel::CVehicleWagonControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CVehicleBatteringRamControlPanel" ) 
{
}


//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CVehicleWagonControlPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	m_pDriverLabel = new vgui::Label( this, "DriverReadout", "" );
	m_pPassengerLabel = new vgui::Label( this, "PassengerReadout", "" );
	m_pOccupyButton = new vgui::Button( this, "OccupyButton", "Occupy" );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CVehicleWagonControlPanel::OnTick()
{
	BaseClass::OnTick();

	C_BaseObject *pObj = GetOwningObject();
	if (!pObj)
		return;

	Assert( dynamic_cast<C_VehicleWagon*>(pObj) );
	C_VehicleWagon *pWagon = static_cast<C_VehicleWagon*>(pObj);

	char buf[256];
	// Update the currently manned player label
	if ( pWagon->GetDriverPlayer() )
	{
		Q_snprintf( buf, sizeof( buf ), "Driven by %s", pWagon->GetDriverPlayer()->GetPlayerName() );
		m_pDriverLabel->SetText( buf );
		m_pDriverLabel->SetVisible( true );
	}
	else
	{
		m_pDriverLabel->SetVisible( false );
	}

	int nPassengerCount = pWagon->GetPassengerCount();
	int nMaxPassengerCount = pWagon->GetMaxPassengerCount();

	Q_snprintf( buf, sizeof( buf ), "Passengers %d/%d", nPassengerCount >= 1 ? nPassengerCount - 1 : 0, nMaxPassengerCount - 1 );
	m_pPassengerLabel->SetText( buf );

	// Update the get in button
	if ( pWagon->IsPlayerInVehicle( C_BaseTFPlayer::GetLocalPlayer() ) ) 
	{
		m_pOccupyButton->SetEnabled( false );
		return;
	}

	if ( pWagon->GetOwner() == C_BaseTFPlayer::GetLocalPlayer() )
	{
		if (nPassengerCount == nMaxPassengerCount)
		{
			// Owners can boot other players to get in
			C_BaseTFPlayer *pPlayer = static_cast<C_BaseTFPlayer*>(pWagon->GetPassenger( VEHICLE_ROLE_DRIVER ));
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
		m_pOccupyButton->SetEnabled( pWagon->GetPassengerCount() < pWagon->GetMaxPassengerCount() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle clicking on the Occupy button
//-----------------------------------------------------------------------------
void CVehicleWagonControlPanel::GetInRam( void )
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
void CVehicleWagonControlPanel::OnCommand( const char *command )
{
	if (!Q_strnicmp(command, "Occupy", 7))
	{
		GetInRam();
		return;
	}

	BaseClass::OnCommand(command);
}


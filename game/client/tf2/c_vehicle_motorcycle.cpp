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
class C_VehicleMotorcycle : public C_BaseTFFourWheelVehicle
{
	DECLARE_CLASS( C_VehicleMotorcycle, C_BaseTFFourWheelVehicle );
public:
	DECLARE_CLIENTCLASS();

	C_VehicleMotorcycle();

private:
	C_VehicleMotorcycle( const C_VehicleMotorcycle & );		// not defined, not accessible

};


IMPLEMENT_CLIENTCLASS_DT(C_VehicleMotorcycle, DT_VehicleMotorcycle, CVehicleMotorcycle)
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_VehicleMotorcycle::C_VehicleMotorcycle()
{
}



//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CVehicleMotorcycleControlPanel : public CObjectControlPanel
{
	DECLARE_CLASS( CVehicleMotorcycleControlPanel, CObjectControlPanel );

public:
	CVehicleMotorcycleControlPanel( vgui::Panel *parent, const char *panelName );
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();
	virtual void OnCommand( const char *command );

private:
	void GetInCycle( void );

private:
	vgui::Label		*m_pDriverLabel;
	vgui::Label		*m_pPassengerLabel;
	vgui::Button	*m_pOccupyButton;
};


DECLARE_VGUI_SCREEN_FACTORY( CVehicleMotorcycleControlPanel, "vehicle_wagon_control_panel" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CVehicleMotorcycleControlPanel::CVehicleMotorcycleControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CVehicleMotorcycleControlPanel" ) 
{
}


//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CVehicleMotorcycleControlPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
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
void CVehicleMotorcycleControlPanel::OnTick()
{
	BaseClass::OnTick();

	C_BaseObject *pObj = GetOwningObject();
	if (!pObj)
		return;

	Assert( dynamic_cast<C_VehicleMotorcycle*>(pObj) );
	C_VehicleMotorcycle *pCycle = static_cast<C_VehicleMotorcycle*>(pObj);

	char buf[256];
	// Update the currently manned player label
	if ( pCycle->GetDriverPlayer() )
	{
		Q_snprintf( buf, sizeof( buf ), "Driven by %s", pCycle->GetDriverPlayer()->GetPlayerName() );
		m_pDriverLabel->SetText( buf );
		m_pDriverLabel->SetVisible( true );
	}
	else
	{
		m_pDriverLabel->SetVisible( false );
	}

	int nPassengerCount = pCycle->GetPassengerCount();
	int nMaxPassengerCount = pCycle->GetMaxPassengerCount();

	Q_snprintf( buf, sizeof( buf ), "Passengers %d/%d", nPassengerCount >= 1 ? nPassengerCount - 1 : 0, nMaxPassengerCount - 1 );
	m_pPassengerLabel->SetText( buf );

	// Update the get in button
	if ( pCycle->IsPlayerInVehicle( C_BaseTFPlayer::GetLocalPlayer() ) ) 
	{
		m_pOccupyButton->SetEnabled( false );
		return;
	}

	if ( pCycle->GetOwner() == C_BaseTFPlayer::GetLocalPlayer() )
	{
		if (nPassengerCount == nMaxPassengerCount)
		{
			// Owners can boot other players to get in
			C_BaseTFPlayer *pPlayer = static_cast<C_BaseTFPlayer*>(pCycle->GetPassenger( VEHICLE_ROLE_DRIVER ));
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
		m_pOccupyButton->SetEnabled( pCycle->GetPassengerCount() < pCycle->GetMaxPassengerCount() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle clicking on the Occupy button
//-----------------------------------------------------------------------------
void CVehicleMotorcycleControlPanel::GetInCycle( void )
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
void CVehicleMotorcycleControlPanel::OnCommand( const char *command )
{
	if (!Q_strnicmp(command, "Occupy", 7))
	{
		GetInCycle();
		return;
	}

	BaseClass::OnCommand(command);
}


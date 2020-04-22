//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Siege Tower Vechicle
//
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
class C_VehicleSiegeTower : public C_BaseTFFourWheelVehicle
{
	DECLARE_CLASS( C_VehicleSiegeTower, C_BaseTFFourWheelVehicle );

public:

	DECLARE_CLIENTCLASS();

	C_VehicleSiegeTower();

// C_BaseEntity overrides.
public:

//	virtual void	DebugMessages( void );

private:

	C_VehicleSiegeTower( const C_VehicleSiegeTower & );		// not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT( C_VehicleSiegeTower, DT_VehicleSiegeTower, CVehicleSiegeTower )
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_VehicleSiegeTower::C_VehicleSiegeTower()
{
}

#if 0
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_VehicleSiegeTower::DebugMessages( void )
{
	Msg( "Seq: %d, Play: %f, Cycle: %f, Anim %f, Done: %d\n", GetSequence(), m_flPlaybackRate, GetCycle(), m_flAnimTime,
		  ( int )m_fSequenceFinished );
}
#endif

//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CVehicleSiegeTowerControlPanel : public CObjectControlPanel
{
	DECLARE_CLASS( CVehicleSiegeTowerControlPanel, CObjectControlPanel );

public:

	CVehicleSiegeTowerControlPanel( vgui::Panel *parent, const char *panelName );
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();
	virtual void OnCommand( const char *command );

private:

	void GetInTower( void );

private:

	vgui::Label		*m_pDriverLabel;
	vgui::Label		*m_pPassengerLabel;
	vgui::Button	*m_pOccupyButton;
};

DECLARE_VGUI_SCREEN_FACTORY( CVehicleSiegeTowerControlPanel, "vehicle_siege_tower_control_panel" );

//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CVehicleSiegeTowerControlPanel::CVehicleSiegeTowerControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CVehicleSiegeTowerControlPanel" ) 
{
}

//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CVehicleSiegeTowerControlPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	m_pDriverLabel = new vgui::Label( this, "DriverReadout", "" );
	m_pPassengerLabel = new vgui::Label( this, "PassengerReadout", "" );
	m_pOccupyButton = new CBitmapButton( this, "OccupyButton", "Occupy" );

	if ( !BaseClass::Init( pKeyValues, pInitData ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CVehicleSiegeTowerControlPanel::OnTick()
{
	BaseClass::OnTick();

	C_BaseObject *pObj = GetOwningObject();
	if ( !pObj )
		return;

	Assert( dynamic_cast<C_VehicleSiegeTower*>( pObj ) );
	C_VehicleSiegeTower *pTower = static_cast<C_VehicleSiegeTower*>( pObj );

	char buf[256];
	// Update the currently manned player label
	if ( pTower->GetDriverPlayer() )
	{
		Q_snprintf( buf, sizeof( buf ), "Driven by %s", pTower->GetDriverPlayer()->GetPlayerName() );
		m_pDriverLabel->SetText( buf );
		m_pDriverLabel->SetVisible( true );
	}
	else
	{
		m_pDriverLabel->SetVisible( false );
	}

	int nPassengerCount = pTower->GetPassengerCount();
	int nMaxPassengerCount = pTower->GetMaxPassengerCount();

	Q_snprintf( buf, sizeof( buf ), "Passengers %d/%d", nPassengerCount >= 1 ? nPassengerCount - 1 : 0, nMaxPassengerCount - 1 );
	m_pPassengerLabel->SetText( buf );

	// Update the get in button
	if ( pTower->IsPlayerInVehicle( C_BaseTFPlayer::GetLocalPlayer() ) ) 
	{
		m_pOccupyButton->SetEnabled( false );
		return;
	}

	if ( pTower->GetOwner() == C_BaseTFPlayer::GetLocalPlayer() )
	{
		if ( nPassengerCount == nMaxPassengerCount )
		{
			// Owners can boot other players to get in
			C_BaseTFPlayer *pPlayer = static_cast<C_BaseTFPlayer*>( pTower->GetPassenger( VEHICLE_ROLE_DRIVER ) );
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
		m_pOccupyButton->SetEnabled( pTower->GetPassengerCount() < pTower->GetMaxPassengerCount() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle clicking on the Occupy button
//-----------------------------------------------------------------------------
void CVehicleSiegeTowerControlPanel::GetInTower( void )
{
	C_BaseObject *pObj = GetOwningObject();
	if ( pObj )
	{
		pObj->SendClientCommand( "toggle_use" );
	}
}

//-----------------------------------------------------------------------------
// Button click handlers
//-----------------------------------------------------------------------------
void CVehicleSiegeTowerControlPanel::OnCommand( const char *command )
{
	if ( !Q_strnicmp( command, "Occupy", 7 ) )
	{
		GetInTower();
		return;
	}

	BaseClass::OnCommand( command );
}

//=============================================================================
//
// Siege Ladder
//

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ObjectSiegeLadder : public C_BaseAnimating
{
	DECLARE_CLASS( C_ObjectSiegeLadder, C_BaseAnimating );

public:

	DECLARE_CLIENTCLASS();

	C_ObjectSiegeLadder();

private:
	C_ObjectSiegeLadder( const C_ObjectSiegeLadder& ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT( C_ObjectSiegeLadder, DT_ObjectSiegeLadder, CObjectSiegeLadder )
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_ObjectSiegeLadder::C_ObjectSiegeLadder()
{
}


//=============================================================================
//
// Siege Platform
//

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ObjectSiegePlatform : public C_BaseAnimating
{
	DECLARE_CLASS( C_ObjectSiegePlatform, C_BaseAnimating );

public:

	DECLARE_CLIENTCLASS();

	C_ObjectSiegePlatform();

private:
	C_ObjectSiegePlatform( const C_ObjectSiegePlatform& ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT( C_ObjectSiegePlatform, DT_ObjectSiegePlatform, CObjectSiegePlatform )
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_ObjectSiegePlatform::C_ObjectSiegePlatform()
{
}


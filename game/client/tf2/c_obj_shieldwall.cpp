//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's CObjectSentrygun
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "CommanderOverlay.h"
#include "c_baseobject.h"
#include "vgui_healthbar.h"
#include "ObjectControlPanel.h"
#include "c_shield.h"

//-----------------------------------------------------------------------------
// Purpose: Resupply Station object
//-----------------------------------------------------------------------------
class C_ObjectShieldWallBase : public C_BaseObject
{
	DECLARE_CLASS( C_ObjectShieldWallBase, C_BaseObject );

public:
	C_ObjectShieldWallBase() {}

public:
	CHandle<C_Shield>	m_hDeployedShield;

private:
	C_ObjectShieldWallBase( const C_ObjectShieldWallBase & ); // not defined, not accessible
};


//-----------------------------------------------------------------------------
//
// Projected shield wall
//
//-----------------------------------------------------------------------------
class C_ObjectShieldWall : public C_ObjectShieldWallBase
{
	DECLARE_CLASS( C_ObjectShieldWall, C_ObjectShieldWallBase );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_ENTITY_PANEL();

	C_ObjectShieldWall( void );

	virtual void SetDormant( bool bDormant );
	virtual void RecalculateIDString( void );

private:
	C_ObjectShieldWall( const C_ObjectShieldWall & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_ObjectShieldWall, DT_ObjectShieldWall, CObjectShieldWall)
	RecvPropEHandle(RECVINFO(m_hDeployedShield)),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectShieldWall::C_ObjectShieldWall( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectShieldWall::SetDormant( bool bDormant )
{
	BaseClass::SetDormant( bDormant );
	ENTITY_PANEL_ACTIVATE( "shield_wall", !bDormant );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectShieldWall::RecalculateIDString( void )
{
	if ( m_hDeployedShield )
	{
		// Report shield strength
		Q_snprintf( m_szIDString, sizeof(m_szIDString), "Shield Strength: %.0f percent", m_hDeployedShield->GetPowerLevel() * 100 );
	}

	BaseClass::RecalculateIDString();
}

//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CShieldWallControlPanel : public CRotatingObjectControlPanel
{
	DECLARE_CLASS( CShieldWallControlPanel, CRotatingObjectControlPanel );

public:
	CShieldWallControlPanel( vgui::Panel *parent, const char *panelName );
};


DECLARE_VGUI_SCREEN_FACTORY( CShieldWallControlPanel, "shieldwall_control_panel" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CShieldWallControlPanel::CShieldWallControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CShieldWallControlPanel" ) 
{
}



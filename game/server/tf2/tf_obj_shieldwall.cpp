//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Medic's resupply beacon
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_obj.h"
#include "tf_player.h"
#include "tf_team.h"
#include "techtree.h"
#include "tf_shield.h"
#include "VGuiScreen.h"

//-----------------------------------------------------------------------------
// Shield wall defines
//-----------------------------------------------------------------------------

#define SHIELDWALL_MINS				Vector(-20, -20, 0)
#define SHIELDWALL_MAXS				Vector( 20,  20, 100)

#define SHIELD_WALL_PITCH -10.0f


ConVar	obj_shieldwall_health( "obj_shieldwall_health","200", FCVAR_NONE, "Shield wall health" );


//-----------------------------------------------------------------------------
// Shield wall object that's built by the player
//-----------------------------------------------------------------------------
class CObjectShieldWallBase : public CBaseObject
{
DECLARE_CLASS( CObjectShieldWallBase, CBaseObject );

public:
	CObjectShieldWallBase();

	virtual void	UpdateOnRemove( void );

	virtual void	Spawn();

	virtual void	PowerupStart( int iPowerup, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier );
	virtual	void	PowerupEnd( int iPowerup );

	// Team change
	virtual void	ChangeTeam( int nTeamNumber ) OVERRIDE;

public:
	CNetworkHandle( CShield, m_hDeployedShield );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectShieldWallBase::CObjectShieldWallBase()
{
	m_hDeployedShield.Set(0);
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectShieldWallBase::UpdateOnRemove( void )
{
	if ( m_hDeployedShield.Get() )
	{
		UTIL_Remove( m_hDeployedShield );
		m_hDeployedShield.Set( NULL );
	}

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectShieldWallBase::Spawn()
{
	m_takedamage = DAMAGE_YES;

	SetType( OBJ_SHIELDWALL );

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just started
//-----------------------------------------------------------------------------
void CObjectShieldWallBase::PowerupStart( int iPowerup, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier )
{
	switch( iPowerup )
	{
	case POWERUP_BOOST:
		// Increase our shield's energy
		if ( m_hDeployedShield )
		{
			m_hDeployedShield->SetPower( m_hDeployedShield->GetPower() + (flAmount * 3) );
		}
		break;

	case POWERUP_EMP:
		if (m_hDeployedShield)
		{
			m_hDeployedShield->SetEMPed(true);
		}
		break;

	default:
		break;
	}

	BaseClass::PowerupStart( iPowerup, flAmount, pAttacker, pDamageModifier );
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just started
//-----------------------------------------------------------------------------
void CObjectShieldWallBase::PowerupEnd( int iPowerup )
{
	switch ( iPowerup )
	{
	case POWERUP_EMP:
		if (m_hDeployedShield)
		{
			m_hDeployedShield->SetEMPed(false);
		}
		break;

	default:
		break;
	}

	BaseClass::PowerupEnd( iPowerup );
}


//-----------------------------------------------------------------------------
// Team change
//-----------------------------------------------------------------------------
void CObjectShieldWallBase::ChangeTeam( int nTeamNumber )
{
	BaseClass::ChangeTeam( nTeamNumber );
	if ( m_hDeployedShield )
	{
		m_hDeployedShield->ChangeTeam( nTeamNumber );
	}
}


//-----------------------------------------------------------------------------
// Shield wall object that's built by the player
//-----------------------------------------------------------------------------
class CObjectShieldWall : public CObjectShieldWallBase
{
DECLARE_CLASS( CObjectShieldWall, CObjectShieldWallBase );

public:
	DECLARE_SERVERCLASS();

					CObjectShieldWall();

	virtual void	Spawn();
	virtual void	Precache();
	virtual void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	virtual void	ObjectMoved( );
	virtual void	FinishedBuilding( void );
};

IMPLEMENT_SERVERCLASS_ST(CObjectShieldWall, DT_ObjectShieldWall)
	SendPropEHandle(SENDINFO(m_hDeployedShield)),
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS(obj_shieldwall, CObjectShieldWall);
PRECACHE_REGISTER(obj_shieldwall);

CObjectShieldWall::CObjectShieldWall()
{
	UseClientSideAnimation();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectShieldWall::Spawn()
{
	SetModel( "models/objects/obj_shieldwall.mdl" );
	SetSolid( SOLID_BBOX );
	UTIL_SetSize(this, SHIELDWALL_MINS, SHIELDWALL_MAXS);
	m_iHealth = obj_shieldwall_health.GetInt();
	m_hDeployedShield = NULL;

	SetType( OBJ_SHIELDWALL );

	BaseClass::Spawn();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectShieldWall::Precache()
{
	PrecacheModel( "models/objects/obj_shieldwall.mdl" );
	PrecacheVGuiScreen( "screen_obj_shieldwall" );
}


//-----------------------------------------------------------------------------
// Purpose: Gets info about the control panels
//-----------------------------------------------------------------------------
void CObjectShieldWall::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "screen_obj_shieldwall";
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectShieldWall::FinishedBuilding( void )
{
	BaseClass::FinishedBuilding();

	int nAttachmentIndex = LookupAttachment( "projectionpoint" );

	m_hDeployedShield = CreateMobileShield( this );
	m_hDeployedShield->SetAlwaysOrient( false );
	m_hDeployedShield->SetAttachmentIndex( nAttachmentIndex );
	m_hDeployedShield->SetAngularSpringConstant( 20 );
	ObjectMoved();
}


//-----------------------------------------------------------------------------
// Called when the builder rotates this object...
//-----------------------------------------------------------------------------
void CObjectShieldWall::ObjectMoved( )
{
	if (m_hDeployedShield)
	{
		VMatrix matangles;
		VMatrix matoffset;
		VMatrix matfinal;

		// This represents how much to pitch the shield up from the attachment point
		QAngle angleOffset( SHIELD_WALL_PITCH, 0, 0 );

		// Get the location and angles of the attachment point
		// Attachment point position is the origin of the shield
		QAngle angles;

		// Rotate the angles of the attachment point by the angle offset
		MatrixFromAngles( GetAbsAngles(), matangles );
		MatrixFromAngles( angleOffset, matoffset );
		MatrixMultiply( matangles, matoffset, matfinal );
		MatrixToAngles( matfinal, angles );
		m_hDeployedShield->SetCenterAngles( angles );

		m_hDeployedShield->ShieldMoved();
	}
	BaseClass::ObjectMoved();
}





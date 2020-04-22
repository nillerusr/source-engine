//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A stationary tower that players can take cover in.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_obj.h"
#include "tf_obj_tower.h"
#include "tf_team.h"
#include "tf_obj.h"
#include "engine/IEngineSound.h"

#define TOWER_MINS			Vector(-100, -100, 0)
#define TOWER_MAXS			Vector( 100,  100, 200)
#define TOWER_MODEL			"models/objects/obj_tower.mdl"
#define TOWER_LADDER_MODEL	"models/objects/obj_tower_ladder.mdl"

IMPLEMENT_SERVERCLASS_ST(CObjectTower, DT_ObjectTower)
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS(obj_tower, CObjectTower);
PRECACHE_REGISTER(obj_tower);

IMPLEMENT_SERVERCLASS_ST( CObjectTowerLadder, DT_ObjectTowerLadder )
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS( obj_tower_ladder, CObjectTowerLadder );
PRECACHE_REGISTER( obj_tower_ladder );

// CVars
ConVar	obj_tower_health( "obj_tower_health","100", FCVAR_NONE, "Tower health" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectTower::CObjectTower( void )
{
	m_hLadder = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectTower::Spawn( void )
{
	Precache();
	SetModel( TOWER_MODEL );
	SetSolid( SOLID_BBOX );

	UTIL_SetSize(this, TOWER_MINS, TOWER_MAXS);
	m_takedamage = DAMAGE_YES;
	m_iHealth = obj_tower_health.GetInt();

	m_fObjectFlags |= OF_DONT_PREVENT_BUILD_NEAR_OBJ | OF_DOESNT_NEED_POWER;
	SetType( OBJ_TOWER );

	SetSolid( SOLID_VPHYSICS );
	VPhysicsInitStatic();

	BaseClass::Spawn();

	SetCollisionGroup( COLLISION_GROUP_VEHICLE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectTower::Precache( void )
{
	PrecacheModel( TOWER_MODEL );
	PrecacheModel( TOWER_LADDER_MODEL );
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CObjectTower::UpdateOnRemove( void )
{
	if ( m_hLadder.Get() )
	{
		UTIL_Remove( m_hLadder );
		m_hLadder = NULL;
	}

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectTower::FinishedBuilding( void )
{
	BaseClass::FinishedBuilding();

	// Create the ladder.
	Vector vecOrigin;
	QAngle vecAngles;
	GetAttachment( "ladder", vecOrigin, vecAngles );
	m_hLadder = CObjectTowerLadder::Create( vecOrigin, vecAngles, this );
	m_hLadder->ChangeTeam( GetTeamNumber() );
}

//-----------------------------------------------------------------------------
// Purpose: Gets info about the control panels
//-----------------------------------------------------------------------------
void CObjectTower::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "screen_basic_with_disable";
}


//==============================================================================
//	Tower Ladder 
//==============================================================================

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
CObjectTowerLadder::CObjectTowerLadder()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectTowerLadder *CObjectTowerLadder::Create( const Vector &vOrigin, const QAngle &vAngles, CBaseEntity *pParent )
{
	CObjectTowerLadder *pLadder = static_cast<CObjectTowerLadder*>( CBaseObject::Create( "obj_tower_ladder", vOrigin, vAngles ) );
	if ( pLadder )
	{
		pLadder->m_hTower = pParent;
	}

	return pLadder;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectTowerLadder::Spawn()
{
	Precache();
	SetModel( TOWER_LADDER_MODEL );
	SetSolid( SOLID_VPHYSICS );
	m_takedamage = DAMAGE_NO;

	BaseClass::Spawn();

	CollisionProp()->SetSurroundingBoundsType( USE_BEST_COLLISION_BOUNDS );
	IPhysicsObject *pPhysics = VPhysicsInitStatic();
	if ( pPhysics )
	{
		pPhysics->EnableMotion( false );
	}
	SetCollisionGroup( COLLISION_GROUP_VEHICLE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectTowerLadder::Precache()
{
	PrecacheModel( TOWER_LADDER_MODEL );
}

//-----------------------------------------------------------------------------
// Purpose: Pass all damage back to the tower
//-----------------------------------------------------------------------------
int	CObjectTowerLadder::OnTakeDamage( const CTakeDamageInfo &info )
{
	return m_hTower->OnTakeDamage( info );
}
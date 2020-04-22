//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Siege Tower Vehicle
//
//=============================================================================//

#include "cbase.h"
#include "tf_vehicle_siege_tower.h"
#include "engine/IEngineSound.h"
#include "VGuiScreen.h"
#include "ammodef.h"
#include "in_buttons.h"

#define SIEGE_TOWER_MINS			Vector(-30, -50, -10)
#define SIEGE_TOWER_MAXS			Vector( 30,  50, 55)
#define SIEGE_TOWER_MODEL			"models/objects/vehicle_siege_tower.mdl"
#define SIEGE_TOWER_LADDER_MODEL	"models/objects/vehicle_siege_ladder.mdl"
#define SIEGE_TOWER_PLATFORM_MODEL	"models/objects/vehicle_siege_plat.mdl"

IMPLEMENT_SERVERCLASS_ST( CVehicleSiegeTower, DT_VehicleSiegeTower )
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS( vehicle_siege_tower, CVehicleSiegeTower );
PRECACHE_REGISTER( vehicle_siege_tower );

IMPLEMENT_SERVERCLASS_ST( CObjectSiegeLadder, DT_ObjectSiegeLadder )
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS( obj_siege_ladder, CObjectSiegeLadder );
PRECACHE_REGISTER( obj_siege_ladder );

IMPLEMENT_SERVERCLASS_ST( CObjectSiegePlatform, DT_ObjectSiegePlatform )
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS( obj_siege_platform, CObjectSiegePlatform );
PRECACHE_REGISTER( obj_siege_platform );

// CVars
ConVar	vehicle_siege_tower_health( "vehicle_siege_tower_health","600", FCVAR_NONE, "Siege tower health" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVehicleSiegeTower::CVehicleSiegeTower()
{
	m_hLadder = NULL;
	m_hPlatform = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleSiegeTower::Precache()
{
	PrecacheModel( SIEGE_TOWER_MODEL );

	PrecacheVGuiScreen( "screen_vehicle_siege_tower" );
	PrecacheVGuiScreen( "screen_vulnerable_point" );
	
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleSiegeTower::Spawn()
{
	SetModel( SIEGE_TOWER_MODEL );
	
	// This size is used for placement only...
	UTIL_SetSize( this, SIEGE_TOWER_MINS, SIEGE_TOWER_MAXS );
	m_takedamage = DAMAGE_YES;
	m_iHealth = vehicle_siege_tower_health.GetInt();

	SetType( OBJ_SIEGE_TOWER );
	SetMaxPassengerCount( 4 );

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: Gets info about the control panels
//-----------------------------------------------------------------------------
void CVehicleSiegeTower::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	//pPanelName = "screen_vehicle_siege_tower";
	pPanelName = "screen_vulnerable_point";
}

//-----------------------------------------------------------------------------
// Here's where we deal with weapons, ladders, etc.
//-----------------------------------------------------------------------------
void CVehicleSiegeTower::OnItemPostFrame( CBaseTFPlayer *pDriver )
{
	// I can't do anything if I'm not active
	if ( !ShouldBeActive() )
		return;

	if ( GetPassengerRole( pDriver ) != VEHICLE_ROLE_DRIVER )
		return;

	if ( !IsDeployed() && ( pDriver->m_afButtonPressed & IN_ATTACK ) )
	{
		InternalDeploy();
	}
	else if ( IsDeployed() && ( pDriver->m_afButtonPressed & IN_ATTACK ) )
	{
		InternalUnDeploy();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVehicleSiegeTower::InternalDeploy( void )
{
	// Deploy
	if ( !Deploy() )
		return;

	InputTurnOff( inputdata_t() );

	// Create the ladder.
	Vector vecOrigin;
	QAngle vecAngle;
	GetAttachment( "ladder", vecOrigin, vecAngle );
	CreateLadder( vecOrigin, vecAngle );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVehicleSiegeTower::InternalUnDeploy( void )
{
	// Undeploy
	UnDeploy();
	InputTurnOn( inputdata_t() );

	// Destory the ladder.
	DestroyLadder();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVehicleSiegeTower::CreateLadder( const Vector &vecOrigin, const QAngle &vecAngles )
{
	// NOTE: This ladder and platform code is a total hack to test vehicles with.  This
	//       is not the correct way to handle this problem at all.
	m_hLadder = CObjectSiegeLadder::Create( vecOrigin, vecAngles, this );
	m_hPlatform = CObjectSiegePlatform::Create( vecOrigin, vecAngles, this );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVehicleSiegeTower::DestroyLadder( void )
{
	if ( m_hLadder.Get() )
	{
		UTIL_Remove( m_hLadder );
		m_hLadder = NULL;
	}

	if ( m_hPlatform.Get() )
	{
		UTIL_Remove( m_hPlatform );
		m_hPlatform = NULL;
	}
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CVehicleSiegeTower::Killed( void )
{
	DestroyLadder();
	BaseClass::Killed();
}

//==============================================================================
//
//	Object Siege Ladder 
//

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
CObjectSiegeLadder::CObjectSiegeLadder()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectSiegeLadder *CObjectSiegeLadder::Create( const Vector &vOrigin, const QAngle &vAngles, CBaseEntity *pParent )
{
	CObjectSiegeLadder *pLadder = static_cast<CObjectSiegeLadder*>( CBaseObject::Create( "obj_siege_ladder", vOrigin, vAngles ) );
	if ( pLadder )
	{
		pLadder->m_hTower = pParent;
	}

	return pLadder;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSiegeLadder::Spawn()
{
	Precache();
	SetModel( SIEGE_TOWER_LADDER_MODEL );
	SetSolid( SOLID_VPHYSICS );
	m_takedamage = DAMAGE_NO;

	BaseClass::Spawn();

	CollisionProp()->SetSurroundingBoundsType( USE_BEST_COLLISION_BOUNDS );
	IPhysicsObject *pPhysics = VPhysicsInitStatic();
	if ( pPhysics )
	{
		pPhysics->EnableMotion( false );
	}
	SetCollisionGroup( TFCOLLISION_GROUP_OBJECT_SOLIDTOPLAYERMOVEMENT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSiegeLadder::Precache()
{
	PrecacheModel( SIEGE_TOWER_LADDER_MODEL );
}

//-----------------------------------------------------------------------------
// Purpose: Pass all damage back to the siege tower
//-----------------------------------------------------------------------------
int	CObjectSiegeLadder::OnTakeDamage( const CTakeDamageInfo &info )
{
	return m_hTower->OnTakeDamage( info );
}

//==============================================================================
//
//	Object Siege Platform
//

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
CObjectSiegePlatform::CObjectSiegePlatform()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectSiegePlatform *CObjectSiegePlatform::Create( const Vector &vOrigin, const QAngle &vAngles, CBaseEntity *pParent )
{
	CObjectSiegePlatform *pPlatform = static_cast<CObjectSiegePlatform*>( CBaseObject::Create( "obj_siege_platform", vOrigin, vAngles ) );
	if ( pPlatform )
	{
		pPlatform->m_hTower = pParent;
	}

	return pPlatform;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSiegePlatform::Spawn()
{
	Precache();
	SetModel( SIEGE_TOWER_PLATFORM_MODEL );
	SetSolid( SOLID_VPHYSICS );
	m_takedamage = DAMAGE_NO;

	BaseClass::Spawn();
	CollisionProp()->SetSurroundingBoundsType( USE_BEST_COLLISION_BOUNDS );
	IPhysicsObject *pPhysics = VPhysicsInitStatic();
	if ( pPhysics )
	{
		pPhysics->EnableMotion( false );
	}
	SetCollisionGroup( TFCOLLISION_GROUP_OBJECT_SOLIDTOPLAYERMOVEMENT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSiegePlatform::Precache()
{
	PrecacheModel( SIEGE_TOWER_PLATFORM_MODEL );
}

//-----------------------------------------------------------------------------
// Purpose: Pass all damage back to the siege tower
//-----------------------------------------------------------------------------
int	CObjectSiegePlatform::OnTakeDamage( const CTakeDamageInfo &info )
{
	return m_hTower->OnTakeDamage( info );
}
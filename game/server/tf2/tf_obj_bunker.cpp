//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A stationary bunker that players can take cover in.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_obj.h"
#include "tf_team.h"
#include "tf_obj_bunker.h"
#include "engine/IEngineSound.h"

#define BUNKER_MINS				Vector(-170, -170, 0)
#define BUNKER_MAXS				Vector( 170,  170, 150)
#define BUNKER_MODEL			"models/objects/obj_bunker.mdl"
#define BUNKER_LADDER_MODEL		"models/objects/obj_bunker_ladder.mdl"

IMPLEMENT_SERVERCLASS_ST(CObjectBunker, DT_ObjectBunker)
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS(obj_bunker, CObjectBunker);
PRECACHE_REGISTER(obj_bunker);

IMPLEMENT_SERVERCLASS_ST( CObjectBunkerLadder, DT_ObjectBunkerLadder )
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS( obj_bunker_ladder, CObjectBunkerLadder );
PRECACHE_REGISTER( obj_bunker_ladder );

// CVars
ConVar	obj_bunker_health( "obj_bunker_health","100", FCVAR_NONE, "Bunker health" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectBunker::CObjectBunker( void )
{
	m_hLadder = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBunker::Spawn( void )
{
	Precache();
	SetModel( BUNKER_MODEL );

	UTIL_SetSize(this, BUNKER_MINS, BUNKER_MAXS);
	m_takedamage = DAMAGE_YES;
	m_iHealth = obj_bunker_health.GetInt();

	m_fObjectFlags |= OF_DOESNT_NEED_POWER;
	SetType( OBJ_BUNKER );

	SetSolid( SOLID_VPHYSICS );
	VPhysicsInitStatic();

	BaseClass::Spawn();

	SetCollisionGroup( COLLISION_GROUP_VEHICLE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBunker::Precache( void )
{
	PrecacheModel( BUNKER_MODEL );
	PrecacheModel( BUNKER_LADDER_MODEL );
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CObjectBunker::UpdateOnRemove( void )
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
void CObjectBunker::FinishedBuilding( void )
{
	BaseClass::FinishedBuilding();

	// Create the ladder.
	Vector vecOrigin;
	QAngle vecAngles;
	GetAttachment( "ladder", vecOrigin, vecAngles );
	m_hLadder = CObjectBunkerLadder::Create( vecOrigin, vecAngles, this );
	m_hLadder->ChangeTeam( GetTeamNumber() );
}

//-----------------------------------------------------------------------------
// Purpose: Gets info about the control panels
//-----------------------------------------------------------------------------
void CObjectBunker::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "screen_basic_with_disable";
}

//==============================================================================
//	Bunker Ladder 
//==============================================================================

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
CObjectBunkerLadder::CObjectBunkerLadder()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectBunkerLadder *CObjectBunkerLadder::Create( const Vector &vOrigin, const QAngle &vAngles, CBaseEntity *pParent )
{
	CObjectBunkerLadder *pLadder = static_cast<CObjectBunkerLadder*>( CBaseObject::Create( "obj_bunker_ladder", vOrigin, vAngles ) );
	if ( pLadder )
	{
		pLadder->m_hBunker = pParent;
	}

	return pLadder;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectBunkerLadder::Spawn()
{
	Precache();
	SetModel( BUNKER_LADDER_MODEL );
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
void CObjectBunkerLadder::Precache()
{
	PrecacheModel( BUNKER_LADDER_MODEL );
}


//-----------------------------------------------------------------------------
// Purpose: Pass all damage back to the bunker
//-----------------------------------------------------------------------------
int	CObjectBunkerLadder::OnTakeDamage( const CTakeDamageInfo &info )
{
	return m_hBunker->OnTakeDamage( info );
}
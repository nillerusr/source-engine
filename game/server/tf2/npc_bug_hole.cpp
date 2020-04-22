//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Bug hole
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "AI_Task.h"
#include "AI_Default.h"
#include "AI_Schedule.h"
#include "AI_Hull.h"
#include "AI_Hint.h"
#include "activitylist.h"
#include "soundent.h"
#include "game.h"
#include "NPCEvent.h"
#include "tf_player.h"
#include "EntityList.h"
#include "ndebugoverlay.h"
#include "shake.h"
#include "monstermaker.h"
#include "decals.h"
#include "vstdlib/random.h"
#include "tf_obj.h"
#include "engine/IEngineSound.h"
#include "IEffects.h"
#include "npc_bug_warrior.h"
#include "npc_bug_builder.h"
#include "npc_bug_hole.h"

LINK_ENTITY_TO_CLASS( npc_bughole, CMaker_BugHole );

IMPLEMENT_SERVERCLASS_ST(CMaker_BugHole, DT_Maker_BugHole)
END_SEND_TABLE();

BEGIN_DATADESC( CMaker_BugHole )

	DEFINE_KEYFIELD( m_iMaxPool, FIELD_INTEGER, "PoolSize" ),
	DEFINE_KEYFIELD( m_flPoolRegenTime, FIELD_FLOAT, "PoolRegen" ),
	DEFINE_KEYFIELD( m_flPatrolTime, FIELD_FLOAT, "PatrolTime" ),
	DEFINE_KEYFIELD( m_iszPatrolPathName, FIELD_STRING, "PatrolName" ),
	DEFINE_KEYFIELD( m_iMaxNumberOfPatrollers, FIELD_INTEGER, "MaxPatrollers" ),
	DEFINE_KEYFIELD( m_iMaxNumberOfBuilders, FIELD_INTEGER, "MaxBuilders" ),

END_DATADESC()

// Maximum speed at which a bughole thinks. Regen/Spawn times faster than this won't make it work faster.
#define BUGHOLE_THINK_SPEED		3.0

static ConVar	npc_bughole_health( "npc_bughole_health","300", FCVAR_NONE, "Bug hole's health." );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMaker_BugHole::CMaker_BugHole( void)
{
	m_iszNPCClassname_Warrior = MAKE_STRING( "npc_bug_warrior" );
	m_iszNPCClassname_Builder = MAKE_STRING( "npc_bug_builder" );
	m_iszNPCClassname = m_iszNPCClassname_Warrior;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaker_BugHole::Spawn( void )
{
	// Set these up before calling base spawn
	m_spawnflags |= SF_NPCMAKER_INF_CHILD;
	m_bDisabled = true;

	// Start with a full pool
	m_iPool = m_iMaxPool;

	BaseClass::Spawn();

	// Bug holes are destroyable
	SetSolid( SOLID_BBOX );
	SetModel( "models/npcs/bugs/bug_hole.mdl" );
	m_takedamage = DAMAGE_YES;
	m_iHealth = npc_bughole_health.GetInt();

	// Setup spawn & regen times
	m_flNextSpawnTime = 0;
	m_flNextRegenTime = gpGlobals->curtime + m_flPoolRegenTime;
	m_flNextPatrolTime = gpGlobals->curtime + m_flPatrolTime;

	// Override the base class think, and think with some random so bugholes don't all think at the same time
	SetThink ( BugHoleThink );
	SetNextThink( gpGlobals->curtime + BUGHOLE_THINK_SPEED + random->RandomFloat( -0.5, 0.5 ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaker_BugHole::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( "models/npcs/bugs/bug_hole.mdl" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaker_BugHole::BugHoleThink( void )
{
	// Regenerate our bug pool
	if ( m_flNextRegenTime < gpGlobals->curtime )
	{
		if ( m_iPool < m_iMaxPool )
		{
			m_iPool++;
		}
		m_flNextRegenTime += m_flPoolRegenTime;
	}

	// Spawn if we're set to
	if ( m_flNextSpawnTime && m_flNextSpawnTime < gpGlobals->curtime )
	{
		m_flNextSpawnTime = 0;
		MakeNPC();
	}
	else
	{
		// If I can see a player, try and spawn a bug
		CBaseEntity *pList[100];
		Vector vecDelta( 768, 768, 768 );
		int count = UTIL_EntitiesInBox( pList, 32, GetAbsOrigin() - vecDelta, GetAbsOrigin() + vecDelta, FL_CLIENT|FL_OBJECT );
		for ( int i = 0; i < count; i++ )
		{
			CBaseEntity *pEnt = pList[i];
			if ( pEnt->IsAlive() && FVisible(pEnt) )
			{
				BugHoleUnderAttack();
				break;
			}
		}
	}

	// Send out a patrol if we haven't spawned anything for a long time
	if ( m_flNextPatrolTime < gpGlobals->curtime )
	{
		m_flNextPatrolTime = gpGlobals->curtime + m_flPatrolTime;
		StartPatrol();
		CheckBuilder();
	}

	SetNextThink( gpGlobals->curtime + BUGHOLE_THINK_SPEED );
}

//-----------------------------------------------------------------------------
// Purpose: Spawn a bug, if we're not waiting to spawn one already
//-----------------------------------------------------------------------------
void CMaker_BugHole::SpawnBug( float flTime )
{
	// If no time was passed in, spawn immediately
	if ( !flTime )
	{
		MakeNPC();
	}
	else if ( !m_flNextSpawnTime )
	{
		m_flNextSpawnTime = gpGlobals->curtime + flTime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaker_BugHole::SpawnWarrior( float flTime )
{
	m_iszNPCClassname = m_iszNPCClassname_Warrior;
	SpawnBug( flTime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaker_BugHole::SpawnBuilder( float flTime )
{
	m_iszNPCClassname = m_iszNPCClassname_Builder;
	SpawnBug( flTime );
}

//-----------------------------------------------------------------------------
// Purpose: BugHole has spotted a player near it
//-----------------------------------------------------------------------------
void CMaker_BugHole::BugHoleUnderAttack( void )
{
	// Call any patrollers back to defend the base
	for ( int i = 0; i < m_aWarriorBugs.Size(); i++ )
	{
		if ( m_aWarriorBugs[i]->IsPatrolling() )
		{
			m_aWarriorBugs[i]->ReturnToBugHole();
		}
	}

	// Try and spawn a warrior
	SpawnWarrior( random->RandomFloat( 1.0, 3.0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Send a bug out to patrol 
//-----------------------------------------------------------------------------
void CMaker_BugHole::StartPatrol( void )
{
	// Don't patrol if I don't have a patrol name
 	if ( !m_iszPatrolPathName || !m_iMaxNumberOfPatrollers )
		return;

	// If I don't have any children, spawn one for to patrol with
	if ( m_nLiveChildren < m_iMaxNumberOfPatrollers )
	{
		SpawnWarrior(0);

		// Think again to use the bug we just created
		m_flNextPatrolTime = gpGlobals->curtime + 2.0;
	}

	// We might have failed due to having none in the pool
	if ( !m_aWarriorBugs.Size() )
		return;

	// Count number of bugs patrolling
	int i, iCount;
	iCount = 0;
	for ( i = 0; i < m_aWarriorBugs.Size(); i++ )
	{
		if ( m_aWarriorBugs[i]->IsPatrolling() )
		{
			iCount++;
		}
	}

	// Find bugs willing to patrol
	for ( i = 0; i < m_aWarriorBugs.Size(); i++ )
	{
		// Make sure we don't have too many
		if ( iCount >= m_iMaxNumberOfPatrollers )
			return;

		if ( m_aWarriorBugs[i]->StartPatrolling( m_iszPatrolPathName ) )
		{
			iCount++;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: See if we should spawn a builder bug
//-----------------------------------------------------------------------------
void CMaker_BugHole::CheckBuilder( void )
{
	// If my pool is full, and I have spaw builder spots, spawn a builder bug
	/*
	if ( m_iPool == m_iMaxPool && m_aBuilderBugs.Size() < m_iMaxNumberOfBuilders )
	{
		SpawnBuilder(0);
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: Bughole makes multiple types of bugs
//-----------------------------------------------------------------------------
void CMaker_BugHole::MakeNPC( void )
{
	// Don't try and patrol for a while
	m_flNextPatrolTime = gpGlobals->curtime + m_flPatrolTime;

	// If my pool is empty, don't spawn a bug
	if ( !m_iPool )
		return;

	BaseClass::MakeNPC();
}

//-----------------------------------------------------------------------------
// Purpose: Hook to allow bugs to move before they spawn
//-----------------------------------------------------------------------------
void CMaker_BugHole::ChildPreSpawn( CAI_BaseNPC *pChild )
{
	BaseClass::ChildPreSpawn( pChild );

	// Drop the bug down and remove it's onground flag
	Vector origin = GetLocalOrigin();
	origin.z -= 64;
	pChild->SetLocalOrigin( origin );
	pChild->SetGroundEntity( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Hook to allow bugs to pack specific data into their known class fields
// Input  : *pChild - pointer to the spawned entity to work on
//-----------------------------------------------------------------------------
void CMaker_BugHole::ChildPostSpawn( CAI_BaseNPC *pChild )
{
	BaseClass::ChildPostSpawn( pChild );

	// May be a warrior or a builder
	CNPC_Bug_Warrior *pWarrior = dynamic_cast<CNPC_Bug_Warrior*>((CAI_BaseNPC*)pChild);
	if ( pWarrior )
	{
		pWarrior->SetBugHole( this );

		// Add him to my bug list
		WarriorHandle_t hHandle;
		hHandle = pWarrior;
		m_aWarriorBugs.AddToTail( hHandle );
	}
	else
	{
		CNPC_Bug_Builder *pBuilder = dynamic_cast<CNPC_Bug_Builder*>((CAI_BaseNPC*)pChild);
		ASSERT( pBuilder );
		pBuilder->SetBugHole( this );

		// Add him to my bug list
		BuilderHandle_t hHandle;
		hHandle = pBuilder;
		m_aBuilderBugs.AddToTail( hHandle );
	}

	m_iPool--;
}

//-----------------------------------------------------------------------------
// Purpose: Remove the bug from our list of bugs
//-----------------------------------------------------------------------------
void CMaker_BugHole::DeathNotice( CBaseEntity *pVictim )
{
	BaseClass::DeathNotice( pVictim );

	// May be a warrior or a builder
	CNPC_Bug_Warrior *pWarrior = dynamic_cast<CNPC_Bug_Warrior*>((CAI_BaseNPC*)pVictim);
	if ( pWarrior )
	{
		// Remove him from my list
		WarriorHandle_t hHandle;
		hHandle = pWarrior;
		m_aWarriorBugs.FindAndRemove( hHandle );
	}
	else
	{
		CNPC_Bug_Builder *pBuilder = dynamic_cast<CNPC_Bug_Builder*>((CAI_BaseNPC*)pVictim);
		ASSERT( pBuilder );

		// Remove him from my list
		BuilderHandle_t hHandle;
		hHandle = pBuilder;
		m_aBuilderBugs.FindAndRemove( hHandle );
	}
}

//-----------------------------------------------------------------------------
// Purpose: On death, fall to the ground and vanish
//-----------------------------------------------------------------------------
void CMaker_BugHole::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed( info );

	SetMoveType( MOVETYPE_FLYGRAVITY );
	SetGroundEntity( NULL );

	SetThink( SUB_Remove );
	SetNextThink( gpGlobals->curtime + 5.0 );
}

//-----------------------------------------------------------------------------
// Purpose: A bug is fleeing to me, see if I want to do anything
//-----------------------------------------------------------------------------
void CMaker_BugHole::IncomingFleeingBug( CAI_BaseNPC *pBug )
{
	SpawnWarrior( random->RandomFloat( 3.0, 5.0 ) );

	// If I have available warriors, tell them to engage
	for ( int i = 0; i < m_aWarriorBugs.Size(); i++ )
	{
		m_aWarriorBugs[i]->Assist( pBug );
	}
}

//-----------------------------------------------------------------------------
// Purpose: One of my bugs has returned to me
//-----------------------------------------------------------------------------
void CMaker_BugHole::BugReturned( void )
{
	if ( m_iPool < m_iMaxPool )
	{
		m_iPool++;
	}
}

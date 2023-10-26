#include "cbase.h"
#include "baseentity.h"
#include "asw_base_spawner.h"
#include "asw_marine.h"
#include "asw_gamerules.h"
#include "asw_marine_resource.h"
#include "asw_game_resource.h"
#include "entityapi.h"
#include "entityoutput.h"
#include "props.h"
#include "asw_alien.h"
#include "asw_director.h"
#include "asw_fail_advice.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_debug_spawners("asw_debug_spawners", "0", FCVAR_CHEAT, "Displays debug messages for the asw_spawners");

BEGIN_DATADESC( CASW_Base_Spawner )
	DEFINE_KEYFIELD( m_bSpawnIfMarinesAreNear,	FIELD_BOOLEAN,	"SpawnIfMarinesAreNear" ),
	DEFINE_KEYFIELD( m_flNearDistance,			FIELD_FLOAT,	"NearDistance" ),
	DEFINE_KEYFIELD( m_AlienOrders,				FIELD_INTEGER,	"AlienOrders" ),	
	DEFINE_KEYFIELD( m_AlienOrderTargetName,	FIELD_STRING,	"AlienOrderTargetName" ),
	DEFINE_KEYFIELD( m_AlienName,				FIELD_STRING,	"AlienNameTag" ),
	DEFINE_KEYFIELD( m_bStartBurrowed,			FIELD_BOOLEAN,	"StartBurrowed" ),
	DEFINE_KEYFIELD( m_UnburrowIdleActivity,	FIELD_STRING,	"UnburrowIdleActivity" ),
	DEFINE_KEYFIELD( m_UnburrowActivity,		FIELD_STRING,	"UnburrowActivity" ),
	DEFINE_KEYFIELD( m_bCheckSpawnIsClear,		FIELD_BOOLEAN,	"ClearCheck" ),
	DEFINE_KEYFIELD( m_bLongRangeNPC,			FIELD_BOOLEAN,  "LongRange" ),
	DEFINE_KEYFIELD( m_iMinSkillLevel,	FIELD_INTEGER,	"MinSkillLevel" ),
	DEFINE_KEYFIELD( m_iMaxSkillLevel,	FIELD_INTEGER,	"MaxSkillLevel" ),

	DEFINE_OUTPUT( m_OnSpawned,			"OnSpawned" ),

	DEFINE_FIELD( m_iMoveAsideCount, FIELD_INTEGER ),
	DEFINE_FIELD( m_hAlienOrderTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bEnabled, FIELD_BOOLEAN ),

	DEFINE_INPUTFUNC( FIELD_VOID,	"Enable",	InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"Disable",	InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"ToggleEnabled",		InputToggleEnabled ),
END_DATADESC()

CASW_Base_Spawner::CASW_Base_Spawner()
{
	m_hAlienOrderTarget = NULL;
	m_flLastSpawnTime = 0.0f;
	m_bEnabled = true;
}

CASW_Base_Spawner::~CASW_Base_Spawner()
{

}

void CASW_Base_Spawner::Spawn()
{
	SetSolid( SOLID_NONE );
	m_iMoveAsideCount = 0;
	Precache();
	//SetModel( "models/editor/asw_spawner/asw_spawner.mdl" );
}

void CASW_Base_Spawner::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "Spawner.Horde" );
	PrecacheScriptSound( "Spawner.AreaClear" );
	//PrecacheModel( "models/editor/asw_spawner/asw_spawner.mdl" );
}

bool CASW_Base_Spawner::CanSpawn( const Vector &vecHullMins, const Vector &vecHullMaxs )
{
	if ( !m_bEnabled )
		return false;

	// is a marine too near?
	if ( !m_bSpawnIfMarinesAreNear && m_flNearDistance > 0 )
	{		
		CASW_Game_Resource* pGameResource = ASWGameResource();
		float distance = 0.0f;
		for ( int i = 0; i < ASW_MAX_MARINE_RESOURCES; i++ )
		{
			CASW_Marine_Resource* pMR = pGameResource->GetMarineResource(i);
			if ( pMR && pMR->GetMarineEntity() && pMR->GetMarineEntity()->GetHealth() > 0 )
			{
				distance = pMR->GetMarineEntity()->GetAbsOrigin().DistTo( GetAbsOrigin() );
				if ( distance < m_flNearDistance )
				{
					if ( asw_debug_spawners.GetBool() )
						Msg("asw_spawner(%s): Alien can't spawn because a marine (%d) is %f away\n", GetEntityName(), i, distance);
					return false;
				}
			}
		}
	}

	Vector mins = GetAbsOrigin() - Vector( 23, 23, 0 );
	Vector maxs = GetAbsOrigin() + Vector( 23, 23, 0 );
	CBaseEntity *pList[128];
	int count = UTIL_EntitiesInBox( pList, 128, mins, maxs, FL_CLIENT|FL_NPC );
	if ( count )
	{
		//Iterate through the list and check the results
		for ( int i = 0; i < count; i++ )
		{
			//Don't build on top of another entity
			if ( pList[i] == NULL )
				continue;

			//If one of the entities is solid, then we may not be able to spawn now
			if ( ( pList[i]->GetSolidFlags() & FSOLID_NOT_SOLID ) == false )
			{
				// Since the outer method doesn't work well around striders on account of their huge bounding box.
				// Find the ground under me and see if a human hull would fit there.
				trace_t tr;
				UTIL_TraceHull( GetAbsOrigin() + Vector( 0, 0, 1 ),
								GetAbsOrigin() - Vector( 0, 0, 1 ),
								vecHullMins,
								vecHullMaxs,
								MASK_NPCSOLID,
								NULL,
								COLLISION_GROUP_NONE,
								&tr );

				if (tr.fraction < 1.0f && tr.DidHitNonWorldEntity())
				{
					// some non-world entity is blocking the spawn point, so don't spawn
					if (tr.m_pEnt)
					{
						if ( m_iMoveAsideCount < 6 )	// don't send 'move aside' commands more than 5 times in a row, else you'll stop blocked NPCs going to sleep.
						{
							IASW_Spawnable_NPC *pSpawnable = dynamic_cast<IASW_Spawnable_NPC*>(tr.m_pEnt);
							if (pSpawnable)
							{
								pSpawnable->MoveAside();		// try and make him move aside
								m_iMoveAsideCount++;
							}
						}
						if (asw_debug_spawners.GetBool())
							Msg("asw_spawner(%s): Alien can't spawn because a non-world entity is blocking the spawn point: %s\n", GetEntityName(), tr.m_pEnt->GetClassname());
					}
					else
					{
						if (asw_debug_spawners.GetBool())
							Msg("asw_spawner(%s): Alien can't spawn because a non-world entity is blocking the spawn point.\n", GetEntityName());
					}
						
					return false;
				}
			}
		}
	}

	// is there something blocking the spawn point?
	if ( m_bCheckSpawnIsClear )
	{
		if ( asw_debug_spawners.GetBool() )
		{
			Msg("Checking spawn is clear...\n");
		}

		trace_t tr;
		UTIL_TraceHull( GetAbsOrigin(),
					GetAbsOrigin() + Vector( 0, 0, 1 ),
					vecHullMins,
					vecHullMaxs,
					MASK_NPCSOLID,
					this,
					COLLISION_GROUP_NONE,
					&tr );

		if( tr.fraction != 1.0 )
		{
			if ( asw_debug_spawners.GetBool() )
				Msg("asw_spawner(%s): Alien can't spawn because he wouldn't fit in the spawn point.\n", GetEntityName());
			// TODO: If we were trying to spawn an uber, change to spawning a regular instead
			return false;
		}
	}

	m_iMoveAsideCount = 0;
	return true;
}

void CASW_Base_Spawner::RemoveObstructingProps( CBaseEntity *pChild )
{
	// If I'm stuck inside any props, remove them
	bool bFound = true;
	while ( bFound )
	{
		trace_t tr;
		UTIL_TraceHull( pChild->GetAbsOrigin(), pChild->GetAbsOrigin(), pChild->WorldAlignMins(), pChild->WorldAlignMaxs(), MASK_NPCSOLID, pChild, COLLISION_GROUP_NONE, &tr );
		if (asw_debug_spawners.GetBool())
		{
			NDebugOverlay::Box( pChild->GetAbsOrigin(), pChild->WorldAlignMins(), pChild->WorldAlignMaxs(), 0, 255, 0, 32, 5.0 );
		}
		if ( tr.fraction != 1.0 && tr.m_pEnt )
		{
			if ( dynamic_cast<CBaseProp*>(tr.m_pEnt) )
			{
				// Set to non-solid so this loop doesn't keep finding it
				tr.m_pEnt->AddSolidFlags( FSOLID_NOT_SOLID );
				UTIL_RemoveImmediate( tr.m_pEnt );
				continue;
			}
		}

		bFound = false;
	}
}

CBaseEntity* CASW_Base_Spawner::GetOrderTarget()
{
	// find entity with name m_AlienOrderTargetName
	if (GetAlienOrderTarget() == NULL &&
		(m_AlienOrders == AOT_MoveTo || m_AlienOrders == AOT_MoveToIgnoringMarines )
		)
	{
		m_hAlienOrderTarget = gEntList.FindEntityByName( NULL, m_AlienOrderTargetName, NULL );

		if( !GetAlienOrderTarget() )
		{
			DevWarning("%s: asw_spawner can't find order object: %s\n", GetDebugName(), STRING(m_AlienOrderTargetName) );
			return NULL;
		}
	}
	return GetAlienOrderTarget();
}

IASW_Spawnable_NPC* CASW_Base_Spawner::SpawnAlien( const char *szAlienClassName, const Vector &vecHullMins, const Vector &vecHullMaxs )
{
	if ( !IsValidOnThisSkillLevel() )
	{
		UTIL_Remove(this);		// delete ourself if this spawner isn't valid on this difficulty level
		return NULL;
	}

	if ( !CanSpawn( vecHullMins, vecHullMaxs ) )	// this may turn off m_bCurrentlySpawningUber if there's no room
		return NULL;

	CBaseEntity	*pEntity = CreateEntityByName( szAlienClassName );
	if ( !pEntity )
	{
		Msg( "Failed to spawn %s\n", szAlienClassName );
		return NULL;
	}

	CAI_BaseNPC	*pNPC = pEntity->MyNPCPointer();
	if ( pNPC )
	{
		pNPC->AddSpawnFlags( SF_NPC_FALL_TO_GROUND );		
	}

	// check if he can see far
	if ( m_bLongRangeNPC )
		pEntity->AddSpawnFlags( SF_NPC_LONG_RANGE );

	if ( HasSpawnFlags( ASW_SF_NEVER_SLEEP ) )
		pEntity->AddSpawnFlags( SF_NPC_ALWAYSTHINK );

	// Strip pitch and roll from the spawner's angles. Pass only yaw to the spawned NPC.
	QAngle angles = GetAbsAngles();
	angles.x = 0.0;
	angles.z = 0.0;	
	pEntity->SetAbsOrigin( GetAbsOrigin() );	
	pEntity->SetAbsAngles( angles );

	IASW_Spawnable_NPC* pSpawnable = dynamic_cast<IASW_Spawnable_NPC*>(pEntity);
	Assert( pSpawnable );	
	if ( !pSpawnable )
	{
		Warning( "NULL Spawnable Ent in asw_spawner! AlienClass = %s\n", szAlienClassName );
		UTIL_Remove( pEntity );
		return NULL;
	}
	m_flLastSpawnTime = gpGlobals->curtime;
	if ( m_bStartBurrowed )
	{
		pSpawnable->StartBurrowed();
	}

	if ( m_bStartBurrowed )
	{
		pSpawnable->SetUnburrowIdleActivity( m_UnburrowIdleActivity );
		pSpawnable->SetUnburrowActivity( m_UnburrowActivity );
	}

	DispatchSpawn( pEntity );	

	pEntity->SetOwnerEntity( this );
	pEntity->Activate();

	if ( m_AlienName != NULL_STRING )
	{
		pEntity->SetName( m_AlienName );
	}
	
	pSpawnable->SetSpawner( this );

	RemoveObstructingProps( pEntity );	
	
	// give our aliens the orders
	pSpawnable->SetAlienOrders( m_AlienOrders, vec3_origin, GetOrderTarget() );

	m_OnSpawned.FireOutput(pEntity, this);

	return pSpawnable;
}

CBaseEntity* CASW_Base_Spawner::GetAlienOrderTarget()
{
	return m_hAlienOrderTarget.Get();
}

bool CASW_Base_Spawner::IsValidOnThisSkillLevel()
{
	// treat difficulty 5 and 4 as the same
	int nSkillLevel = ASWGameRules()->GetSkillLevel();
	nSkillLevel = clamp<int>( nSkillLevel, 1, 4 );
	if (m_iMinSkillLevel > 0 && nSkillLevel < m_iMinSkillLevel)
		return false;
	if (m_iMaxSkillLevel > 0 && m_iMaxSkillLevel < 10
			&& nSkillLevel > m_iMaxSkillLevel)
		return false;
	return true;
}

bool CASW_Base_Spawner::HasRecentlySpawned( float flRecent )
{
	return m_flLastSpawnTime > 0.0f && ( ( gpGlobals->curtime - m_flLastSpawnTime ) < flRecent );
}


//------------------------------------------------------------------------------
// Inputs
//------------------------------------------------------------------------------
void CASW_Base_Spawner::InputToggleEnabled( inputdata_t &inputdata )
{
	if ( !m_bEnabled )
	{
		InputEnable( inputdata );
	}
	else
	{
		InputDisable( inputdata );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Base_Spawner::InputEnable( inputdata_t &inputdata )
{
	if ( !m_bEnabled )
	{
		m_bEnabled = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Base_Spawner::InputDisable( inputdata_t &inputdata )
{
	if ( m_bEnabled )
	{
		m_bEnabled = false;
	}
}
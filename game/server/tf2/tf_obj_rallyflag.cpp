//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_team.h"
#include "tf_gamerules.h"
#include "tf_obj.h"
#include "tf_obj_rallyflag.h"
#include "ndebugoverlay.h"

BEGIN_DATADESC( CObjectRallyFlag )

	DEFINE_THINKFUNC( RallyThink ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CObjectRallyFlag, DT_ObjectRallyFlag)
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS(obj_rallyflag, CObjectRallyFlag);
PRECACHE_REGISTER(obj_rallyflag);

ConVar	obj_rallyflag_health( "obj_rallyflag_health","100", FCVAR_NONE, "Rally Flag health" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectRallyFlag::CObjectRallyFlag()
{
	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectRallyFlag::Spawn()
{
	Precache();
	SetModel( RALLYFLAG_MODEL );
	SetSolid( SOLID_BBOX );
	SetCollisionGroup( TFCOLLISION_GROUP_COMBATOBJECT );

	UTIL_SetSize(this, RALLYFLAG_MINS, RALLYFLAG_MAXS);
	m_takedamage = DAMAGE_YES;
	m_iHealth = obj_rallyflag_health.GetInt();

	SetThink( RallyThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
	m_flExpiresAt = gpGlobals->curtime + RALLYFLAG_LIFETIME;

	SetType( OBJ_RALLYFLAG );
	m_fObjectFlags |= OF_SUPPRESS_NOTIFY_UNDER_ATTACK | OF_SUPPRESS_TECH_ANALYZER | 
		OF_DONT_AUTO_REPAIR | OF_DONT_PREVENT_BUILD_NEAR_OBJ | OF_DOESNT_NEED_POWER;

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectRallyFlag::Precache()
{
	PrecacheModel( RALLYFLAG_MODEL );
}


//-----------------------------------------------------------------------------
// Purpose: Look for friendlies to rally
//-----------------------------------------------------------------------------
void CObjectRallyFlag::RallyThink( void )
{
	if ( !GetTeam() )
		return;

	// Time to die?
	if ( gpGlobals->curtime > m_flExpiresAt )
	{
		UTIL_Remove( this );
		return;
	}

	// Look for nearby players to rally
	for ( int i = 0; i < GetTFTeam()->GetNumPlayers(); i++ )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)GetTFTeam()->GetPlayer(i);
		assert(pPlayer);

		// Is it within range?
		if ( ((pPlayer->GetAbsOrigin() - GetAbsOrigin()).Length() < RALLYFLAG_RADIUS ) && pPlayer->IsAlive() )
		{
			// Can I see it?
			trace_t tr;
			UTIL_TraceLine( EyePosition(), pPlayer->EyePosition(), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);
			CBaseEntity *pEntity = tr.m_pEnt;
			if ( (tr.fraction == 1.0) || ( pEntity == pPlayer ) )
			{
				pPlayer->AttemptToPowerup( POWERUP_RUSH, RALLYFLAG_ADRENALIN_TIME );
			}
		}
	}

	SetNextThink( gpGlobals->curtime + RALLYFLAG_RATE );
}


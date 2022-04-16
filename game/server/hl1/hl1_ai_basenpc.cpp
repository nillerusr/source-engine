//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "hl1_ai_basenpc.h"
#include "scripted.h"
#include "soundent.h"
#include "animation.h"
#include "entitylist.h"
#include "ai_navigator.h"
#include "ai_motor.h"
#include "player.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "npcevent.h"

#include "effect_dispatch_data.h"
#include "te_effect_dispatch.h"
#include "cplane.h"
#include "ai_squad.h"

#define HUMAN_GIBS 1
#define ALIEN_GIBS 2

//=========================================================
// NoFriendlyFire - checks for possibility of friendly fire
//
// Builds a large box in front of the grunt and checks to see 
// if any squad members are in that box. 
//=========================================================
bool CHL1BaseNPC::NoFriendlyFire( void )
{
	if ( !m_pSquad )
	{
		return true;
	}

	CPlane	backPlane;
	CPlane  leftPlane;
	CPlane	rightPlane;

	Vector	vecLeftSide;
	Vector	vecRightSide;
	Vector	v_left;

	Vector  vForward, vRight, vUp;
	QAngle  vAngleToEnemy;

	if ( GetEnemy() != NULL )
	{
		//!!!BUGBUG - to fix this, the planes must be aligned to where the monster will be firing its gun, not the direction it is facing!!!
		VectorAngles( ( GetEnemy()->WorldSpaceCenter() - GetAbsOrigin() ), vAngleToEnemy );

		AngleVectors ( vAngleToEnemy, &vForward, &vRight, &vUp );
	}
	else
	{
		// if there's no enemy, pretend there's a friendly in the way, so the grunt won't shoot.
		return false;
	}
	
	vecLeftSide = GetAbsOrigin() - ( vRight * ( WorldAlignSize().x * 1.5 ) );
	vecRightSide = GetAbsOrigin() + ( vRight * ( WorldAlignSize().x * 1.5 ) );
	v_left = vRight * -1;

	leftPlane.InitializePlane ( vRight, vecLeftSide );
	rightPlane.InitializePlane ( v_left, vecRightSide );
	backPlane.InitializePlane ( vForward, GetAbsOrigin() );

	AISquadIter_t iter;
	for ( CAI_BaseNPC *pSquadMember = m_pSquad->GetFirstMember( &iter ); pSquadMember; pSquadMember = m_pSquad->GetNextMember( &iter ) )
	{
		if ( pSquadMember == NULL )
			 continue;

		if ( pSquadMember == this )
			 continue;

		if ( backPlane.PointInFront  ( pSquadMember->GetAbsOrigin() ) &&
				 leftPlane.PointInFront  ( pSquadMember->GetAbsOrigin() ) && 
				 rightPlane.PointInFront ( pSquadMember->GetAbsOrigin()) )
			{
				// this guy is in the check volume! Don't shoot!
				return false;
			}
	}

	return true;
}

void CHL1BaseNPC::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	if ( info.GetDamage() >= 1.0 && !(info.GetDamageType() & DMG_SHOCK ) )
	{
		UTIL_BloodSpray( ptr->endpos, vecDir, BloodColor(), 4, FX_BLOODSPRAY_ALL );	
	}

	BaseClass::TraceAttack( info, vecDir, ptr, pAccumulator );
}


bool CHL1BaseNPC::ShouldGib( const CTakeDamageInfo &info )
{
	if ( info.GetDamageType() & DMG_NEVERGIB )
		 return false;

	if ( ( g_pGameRules->Damage_ShouldGibCorpse( info.GetDamageType() ) && m_iHealth < GIB_HEALTH_VALUE ) || ( info.GetDamageType() & DMG_ALWAYSGIB ) )
		 return true;
	
	return false;
	
}

bool CHL1BaseNPC::HasHumanGibs( void )
{
	Class_T myClass = Classify();

	if ( myClass == CLASS_HUMAN_MILITARY ||
		 myClass == CLASS_PLAYER_ALLY	||
		 myClass == CLASS_HUMAN_PASSIVE  ||
		 myClass == CLASS_PLAYER )

		 return true;

	return false;
}


bool CHL1BaseNPC::HasAlienGibs( void )
{
	Class_T myClass = Classify();

	if ( myClass == CLASS_ALIEN_MILITARY ||
		 myClass == CLASS_ALIEN_MONSTER	||
		 myClass == CLASS_INSECT  ||
		 myClass == CLASS_ALIEN_PREDATOR  ||
		 myClass == CLASS_ALIEN_PREY )

		 return true;

	return false;
}


void CHL1BaseNPC::Precache( void )
{
	PrecacheModel( "models/gibs/agibs.mdl" );
	PrecacheModel( "models/gibs/hgibs.mdl" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHL1BaseNPC::CorpseGib( const CTakeDamageInfo &info )
{
	CEffectData	data;
	
	data.m_vOrigin = WorldSpaceCenter();
	data.m_vNormal = data.m_vOrigin - info.GetDamagePosition();
	VectorNormalize( data.m_vNormal );
	
	data.m_flScale = RemapVal( m_iHealth, 0, -500, 1, 3 );
	data.m_flScale = clamp( data.m_flScale, 1, 3 );

	if ( HasAlienGibs() )
		 data.m_nMaterial = ALIEN_GIBS;
	else if ( HasHumanGibs() )
		 data.m_nMaterial = HUMAN_GIBS;
	
	data.m_nColor = BloodColor();

	DispatchEffect( "HL1Gib", data );

	CSoundEnt::InsertSound( SOUND_MEAT, GetAbsOrigin(), 256, 0.5f, this );

///	BaseClass::CorpseGib( info );

	return true;
}

int	CHL1BaseNPC::IRelationPriority( CBaseEntity *pTarget )
{
	return BaseClass::IRelationPriority( pTarget );
}

void CHL1BaseNPC::EjectShell( const Vector &vecOrigin, const Vector &vecVelocity, float rotation, int iType )
{
	CEffectData	data;
	data.m_vStart	= vecVelocity;
	data.m_vOrigin	= vecOrigin;
	data.m_vAngles	= QAngle( 0, rotation, 0 );
	data.m_fFlags	= iType;

	DispatchEffect( "HL1ShellEject", data );
}

// HL1 version - never return Ragdoll as the automatic schedule at the end of a 
// scripted sequence
int CHL1BaseNPC::SelectDeadSchedule()
{
	// Alread dead (by animation event maybe?)
	// Is it safe to set it to SCHED_NONE?
	if ( m_lifeState == LIFE_DEAD )
		 return SCHED_NONE;

	CleanupOnDeath();
	return SCHED_DIE;
}

#include "cbase.h"
#include "asw_trace_filter_melee.h"
#ifdef CLIENT_DLL
#include "c_asw_alien.h"
#include "c_asw_marine.h"
#include "c_basecombatcharacter.h"
#include "c_asw_weapon.h"
#define CASW_Alien C_ASW_Alien
#define CASW_Marine C_ASW_Marine
#else
#include "asw_alien.h"
#include "asw_marine.h"
#include "asw_colonist.h"
#include "asw_weapon.h"
//#include "iservervehicle.h"
//#include "util.h"
#endif
#include "takedamageinfo.h"
#include "asw_melee_system.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_marine_melee_kick_lift;
#ifdef GAME_DLL
extern ConVar asw_debug_marine_damage;
extern ConVar ai_show_hull_attacks;
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pHandleEntity - 
//			contentsMask - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CASW_Trace_Filter_Melee::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	// Don't test if the game code tells us we should ignore this collision...
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
	const CBaseEntity *pEntPass = EntityFromEntityHandle( m_pPassEnt );

#ifdef GAME_DLL
	if ( ai_show_hull_attacks.GetBool() )
	{
		NDebugOverlay::EntityBounds( pEntity, 128, 128, 0, 20, 1.0f );
	}
#endif

	if ( !StandardFilterRules( pHandleEntity, contentsMask ) )
		return false;

	
	// don't hurt ourself
	if (pEntPass == pEntity)
		return false;

	if ( m_pPassVec )
	{
		if ( m_pPassVec->Find( pEntity ) != m_pPassVec->InvalidIndex() )
		{
			return false;
		}
	}
	
	if ( pEntity )
	{
		//Msg("%f CASW_Trace_Filter_Melee::ShouldHitEntity %s\n", gpGlobals->curtime, pEntity->GetClassname());
		if ( !pEntity->ShouldCollide( m_collisionGroup, contentsMask ) )
			return false;
		
		if ( !g_pGameRules->ShouldCollide( m_collisionGroup, pEntity->GetCollisionGroup() ) )
			return false;

		if ( pEntity->m_takedamage == DAMAGE_NO )
			return false;

		if ( m_nNumHits == ASW_MAX_HITS_PER_TRACE )
		{
#ifdef GAME_DLL
			if ( asw_debug_marine_damage.GetBool() )
			{
				Warning( "%s(): This trace filter already hit the maximum (=%d) number of entities, not considering %d:%s\n", __FUNCTION__, ASW_MAX_HITS_PER_TRACE, pEntity->entindex(), pEntity->GetClassname() );
			}
#endif

			return false;
		}

		CBaseCombatCharacter *pAttackerBCC = m_hAttacker->MyCombatCharacterPointer();
#ifndef CLIENT_DLL
		CBaseCombatCharacter *pVictimBCC = pEntity->MyCombatCharacterPointer();
		// Only do these comparisons between NPCs
		if ( pAttackerBCC && pVictimBCC )
		{
			// stop aliens from meleeing each other, unless they're on different factions
			if ( pAttackerBCC->GetFaction() == pVictimBCC->GetFaction() )
				return false;

			// Can only damage other NPCs that we hate
			if ( !m_bDamageAnyNPC && pAttackerBCC->IRelationType( pEntity ) != D_HT )
			{
				return false;
			}
		}
#endif // CLIENT_DLL

		Vector vecAttackerCenter = m_hAttacker->WorldSpaceCenter();
		Vector vecAttackDir = pEntity->WorldSpaceCenter() - vecAttackerCenter;
		Vector vecAttackDirFlat = vecAttackDir;

		vecAttackDirFlat.z = 0.0f;
		VectorNormalize( vecAttackDirFlat );
		VectorNormalize( vecAttackDir );

		Vector vecAttackerForward, vecAttackerRight;
		AngleVectors( m_hAttacker->GetAbsAngles(), &vecAttackerForward, &vecAttackerRight, NULL );

		float flAttackDot = vecAttackerForward.Dot( vecAttackDir );

		// check it's in front
		CASW_Marine *pMarine = CASW_Marine::AsMarine( pAttackerBCC );
		if ( pMarine )
		{			
			// stop marines melee'ing each other
			if ( CASW_Marine::AsMarine( pEntity ) != NULL )
			{
				return false;
			}

			if ( flAttackDot < 0.0f && !m_bHitBehind )
			{
				return false;
			}
		}

		if ( m_pAttackDir && m_bConeFilter )
		{
			float flAttackDot = m_pAttackDir->Dot( vecAttackDirFlat );

			if ( flAttackDot < m_flMinAttackDot )
			{
				return false;
			}
		}
		
		// check there's not actually an obstruction between us
		trace_t tr;
		if ( pMarine )
		{
			CASW_Trace_Filter_Skip_Marines skip_marines_filter( COLLISION_GROUP_NONE );
			UTIL_TraceLine( vecAttackerCenter, pEntity->WorldSpaceCenter(),	// check center to center
				MASK_SOLID, &skip_marines_filter, &tr );

			if ( tr.DidHit() && tr.m_pEnt && tr.m_pEnt != pEntity )
			{
				UTIL_TraceLine( m_hAttacker->GetAbsOrigin() + Vector(0,0, 60), pEntity->GetAbsOrigin() + Vector(0,0, 60),	// check top to top
					MASK_SOLID, &skip_marines_filter, &tr );
				if ( tr.DidHit() && tr.m_pEnt && tr.m_pEnt != pEntity )
				{
					return false;
				}
			}
		}
		else
		{	
			CASW_Trace_Filter_Skip_Aliens skip_aliens_filter( COLLISION_GROUP_NONE );
			//ASW_COLLISION_GROUP_ALIENS
			UTIL_TraceLine( vecAttackerCenter, pEntity->WorldSpaceCenter(),	// check center to center
				MASK_SOLID, &skip_aliens_filter, &tr );

			if ( tr.DidHit() && tr.m_pEnt && tr.m_pEnt != pEntity )
			{
				UTIL_TraceLine( m_hAttacker->GetAbsOrigin() + Vector(0,0, 60), pEntity->GetAbsOrigin() + Vector(0,0, 60),	// check top to top
					MASK_SOLID, &skip_aliens_filter, &tr );
				if ( tr.DidHit() && tr.m_pEnt && tr.m_pEnt != pEntity )
				{
					return false;
				}
			}			
		}
		
		trace_t *tr2 = &m_HitTraces[ m_nNumHits++ ];
		// do one last trace to get the actual impact point
		if ( pMarine )
		{
			CASW_Trace_Filter_Skip_Marines skip_marines_filter( COLLISION_GROUP_NONE );
			UTIL_TraceLine( vecAttackerCenter, pEntity->WorldSpaceCenter(),	// check center to center
				MASK_BLOCKLOS_AND_NPCS, &skip_marines_filter, tr2 );
		}
		else
		{
			UTIL_TraceLine( vecAttackerCenter, pEntity->WorldSpaceCenter(),	// check center to center
				MASK_BLOCKLOS_AND_NPCS, m_hAttacker, COLLISION_GROUP_NONE, tr2 );
		}
		if ( !tr2->DidHit() || tr2->m_pEnt != pEntity )
		{
			// if trace didn't hit, then fake endpos
			tr2->endpos = pEntity->WorldSpaceCenter();
			tr2->m_pEnt = pEntity;
		}		
		m_pHit = pEntity;

		// store the hit entity most 'in front' of us
		if ( flAttackDot > m_fBestHitDot && !pEntity->IsWorld() )
		{
			m_hBestHit = pEntity;
			m_fBestHitDot = flAttackDot;
			m_pBestTrace = tr2;
		}

#ifdef GAME_DLL
		if ( ai_show_hull_attacks.GetBool() )
		{
			NDebugOverlay::EntityBounds( pEntity, 128, 0, 0, 120, 3.0f );
		}
#endif

		// we return false here so the trace will continue to its end, hitting each entity within the ray
		// NOTE: This means to detect a hit from this trace filter, you must use the m_HitTraces array.
		return false;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Trace filter that skips marines
//-----------------------------------------------------------------------------
CASW_Trace_Filter_Skip_Marines::CASW_Trace_Filter_Skip_Marines( int collisionGroup ) :
	CTraceFilterSimple( NULL, collisionGroup )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CASW_Trace_Filter_Skip_Marines::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
	if ( pEntity && CASW_Marine::AsMarine( pEntity ) != NULL )
		return false;

	return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
}

//-----------------------------------------------------------------------------
// Trace filter that skips aliens
//-----------------------------------------------------------------------------
CASW_Trace_Filter_Skip_Aliens::CASW_Trace_Filter_Skip_Aliens( int collisionGroup ) :
CTraceFilterSimple( NULL, collisionGroup )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CASW_Trace_Filter_Skip_Aliens::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
	if ( pEntity && dynamic_cast<CASW_Alien*>( pEntity ) != NULL )
		return false;

	return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
}
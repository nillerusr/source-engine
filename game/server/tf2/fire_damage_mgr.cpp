//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "fire_damage_mgr.h"
#include "entity_burn_effect.h"
#include "gasoline_blob.h"
#include "tf_obj.h"
#include "ai_basenpc.h"
#include "tf_gamerules.h"


#define FIRE_DAMAGE_APPLY_INTERVAL	0.5	// Apply the damage at this interval.
#define FIRE_DECAY_END_VALUE		0.00001


// No more damage from fire can be applied to a player per second.
#define MAX_FIRE_DAMAGE_PER_SECOND	15

// The fire heat uses exponential decay. It goes from MAX_FIRE_DAMAGE_PER_SECOND to
// FIRE_DECAY_END_VALUE in FIRE_DECAY_SECONDS.
#define FIRE_DECAY_SECONDS			3


ConVar fire_damageall( "fire_damageall", "0", 0, "Enable fire damaging team members." );


bool CFireDamageMgr::Init()
{
	m_flApplyDamageCountdown = FIRE_DAMAGE_APPLY_INTERVAL;

	// Fire decays exponentially: B = A * e^(-kt)
	// So we set B=FIRE_DECAY_END_VALUE, A=flMaxDamagePerSecond, and t=flFireDecaySeconds, then solve for K.
	m_flMaxDamagePerSecond = MAX_FIRE_DAMAGE_PER_SECOND;
	m_flDecayConstant = -log( FIRE_DECAY_END_VALUE / m_flMaxDamagePerSecond ) / FIRE_DECAY_SECONDS;

	return true;
}


void CFireDamageMgr::AddDamage( CBaseEntity *pTarget, CBaseEntity *pAttacker, float flDamageAccel, bool bMakeBurnEffect )
{
	FOR_EACH_LL( m_DamageEnts, iDamageEnt )
	{
		CDamageEnt *pEnt = &m_DamageEnts[iDamageEnt];

		if ( pEnt->m_hEnt != pTarget )
			continue;
	
		for ( int i=0; i < pEnt->m_nAttackers; i++ )
		{
			if ( pEnt->m_Attackers[i].m_hAttacker == pAttacker )
			{
				pEnt->m_Attackers[i].m_flVelocity += flDamageAccel * gpGlobals->frametime;
				return;
			}
		}

		
		if ( pEnt->m_nAttackers < CDamageEnt::MAX_ATTACKERS )
		{
			// Add a new attacker.
			pEnt->m_Attackers[pEnt->m_nAttackers].Init( pAttacker, flDamageAccel * gpGlobals->frametime );
			++pEnt->m_nAttackers;
			return;
		}
		else
		{
			// No room for more attackers.
			Warning( "CFireDamageMgr: ran out of attackers\n" );
			return;
		}
	}

	// Add a new CDamageEnt.
	int iNew = m_DamageEnts.AddToTail();
	CDamageEnt *pEnt = &m_DamageEnts[iNew];
	pEnt->m_hEnt = pTarget;
	pEnt->m_bWasAlive = pTarget->IsAlive();
	pEnt->m_nAttackers = 1;
	pEnt->m_Attackers[0].Init( pAttacker, flDamageAccel * gpGlobals->frametime );
	if ( bMakeBurnEffect )
		pEnt->m_pBurnEffect = CEntityBurnEffect::Create( pTarget );
	else
		pEnt->m_pBurnEffect = NULL;
}


void CFireDamageMgr::RemoveDamageEnt( int iEnt )
{
	UTIL_Remove( m_DamageEnts[iEnt].m_pBurnEffect );
	m_DamageEnts.Remove( iEnt );
}


void CFireDamageMgr::FrameUpdatePostEntityThink()
{
	VPROF( "CFireDamageMgr::FrameUpdatePostEntityThink" );
	float frametime = gpGlobals->frametime;
	
	// Update the damage countdown.
	m_flApplyDamageCountdown -= gpGlobals->frametime;
	bool bApplyDamageThisFrame = false;
	if ( m_flApplyDamageCountdown <= 0 )
	{
		bApplyDamageThisFrame = true;
		m_flApplyDamageCountdown += FIRE_DAMAGE_APPLY_INTERVAL;
	}


	//   													   (-kt)
	// Figure out how much all the damage decays this frame:  e
	float flFrameDecay = pow( 2.718281828459045235360, -m_flDecayConstant * frametime );


	int iNext;
	for ( int iCur = m_DamageEnts.Head(); iCur != m_DamageEnts.InvalidIndex(); iCur = iNext )
	{
		iNext = m_DamageEnts.Next( iCur );
		CDamageEnt *pEnt = &m_DamageEnts[iCur];


		// If the entity was dead and is now alive, stop damage to them so their new body doesn't burn.
		if ( !pEnt->m_hEnt.Get() || ( !pEnt->m_bWasAlive && pEnt->m_hEnt->IsAlive() ) )
		{
			RemoveDamageEnt( iCur );
			pEnt = NULL;
			continue;
		}
		
		pEnt->m_bWasAlive = pEnt->m_hEnt->IsAlive();

		// Sum up each attacker's velocity.
		float flTotalVelocity = 0;
		for ( int i=0; i < pEnt->m_nAttackers; i++ )
			flTotalVelocity += pEnt->m_Attackers[i].m_flVelocity;


		// Figure out each attacker's contribution.
		float flContributionPercent[CDamageEnt::MAX_ATTACKERS];
		for ( i=0; i < pEnt->m_nAttackers; i++ )
			flContributionPercent[i] = pEnt->m_Attackers[i].m_flVelocity / flTotalVelocity;
		
		
		// Decay each attacker's velocity.
		flTotalVelocity *= flFrameDecay;

		// Uniformly scale each attacker's velocity down so the sum total doesn't exceed our maximum.
		float flPercentScale = 1;
		if ( flTotalVelocity > m_flMaxDamagePerSecond )
			flPercentScale = m_flMaxDamagePerSecond / flTotalVelocity;

		for ( i=0; i < pEnt->m_nAttackers; i++ )
		{
			CDamageAttacker *pAttacker = &pEnt->m_Attackers[i];

			pAttacker->m_flVelocity *= flFrameDecay * flPercentScale;

			bool bEntsValid = (pEnt->m_Attackers[i].m_hAttacker.Get() != NULL);
			if ( !bEntsValid ||
				pEnt->m_Attackers[i].m_flVelocity <= 0.001 )
			{
				if ( bEntsValid )
					ApplyCollectedDamage( pEnt, i );	// Apply the last-remaining damage from this guy.

				Q_memmove( &pEnt->m_Attackers[i], &pEnt->m_Attackers[i+1], sizeof( pEnt->m_Attackers[0] ) * (pEnt->m_nAttackers-i-1) );
				Q_memmove( &flContributionPercent[i], &flContributionPercent[i+1], sizeof( flContributionPercent[0] ) * (pEnt->m_nAttackers-i-1) );
				
				--pEnt->m_nAttackers;
				if ( pEnt->m_nAttackers == 0 )
				{
					// This ent isn't being damaged anymore.
					RemoveDamageEnt( iCur );
					break;
				}

				--i;
			}

			// Update their current damage sum and maybe apply the damage.
			pAttacker->m_flDamageSum += pAttacker->m_flVelocity * frametime;
			if ( bApplyDamageThisFrame )
			{
				ApplyCollectedDamage( pEnt, i );
			}
		}
	}
}


float GetFireDamageScale( CBaseEntity *pEnt )
{
	// Objects have a lot more health and we want them to take damage faster.
	if ( dynamic_cast< CBaseObject* >( pEnt ) )
		return 4;
	else
		return 1;
}


void CFireDamageMgr::ApplyCollectedDamage( CFireDamageMgr::CDamageEnt *pEnt, int iAttacker )
{
	CDamageAttacker *pAttacker = &pEnt->m_Attackers[iAttacker];

	CTakeDamageInfo info( NULL, pAttacker->m_hAttacker, pAttacker->m_flDamageSum * GetFireDamageScale( pEnt->m_hEnt ), DMG_BURN );
	pEnt->m_hEnt->TakeDamage( info );

	pAttacker->m_flDamageSum = 0;
}


// ------------------------------------------------------------------------------------------------ //
// Global functions.
// ------------------------------------------------------------------------------------------------ //

bool IsBurnableEnt( CBaseEntity *pEntity, int iIgnoreTeam )
{
	if ( pEntity->m_takedamage == DAMAGE_NO )
		return false;

	CGasolineBlob *pBlob = dynamic_cast< CGasolineBlob* >( pEntity );
	if ( pBlob )
	{
		return !pBlob->IsLit();
	}

	if ( pEntity->GetTeamNumber() == iIgnoreTeam && !fire_damageall.GetInt() )
	{
		// Don't damage anyone on the pyro's team (including the pyro himself).
		return false;
	}

	// Now only allow specific types of objects to be damaged.
	if ( dynamic_cast< CBasePlayer* >( pEntity ) || 
		dynamic_cast< CAI_BaseNPC* >( pEntity ) || 
		dynamic_cast< CBaseObject* >( pEntity ) )
	{
		return true;
	}

	return false;
}


int FindBurnableEntsInSphere(
	CBaseEntity **ents,
	float *dists,
	int nMaxEnts,
	const Vector &vecCenter,
	float flSearchRadius,
	CBaseEntity *pOwner )
{
	Assert( nMaxEnts > 0 );
	int nOutEnts = 0;

	CBaseEntity *pEntity;
	for ( CEntitySphereQuery sphere( vecCenter, flSearchRadius ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		if ( !IsBurnableEnt( pEntity, pOwner->GetTeamNumber() ) )
			continue;
	
		// Make sure it's not blocked.
		trace_t tr;
		Vector vCenter = pEntity->WorldSpaceCenter();
		
		UTIL_TraceLine ( vecCenter, vCenter, MASK_SHOT & (~CONTENTS_HITBOX), NULL, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction != 1.0 && tr.m_pEnt != pEntity )
			continue;

		if ( TFGameRules()->IsTraceBlockedByWorldOrShield( vecCenter, vCenter, pOwner, DMG_BURN | DMG_PROBE, &tr ) )
			continue;

		// Make sure it's in range.
		const Vector &mins = pEntity->WorldAlignMins();
		const Vector &maxs = pEntity->WorldAlignMaxs();
		float approxTargetRadius = ( Vector( maxs.x, maxs.y, 0 ) - Vector( mins.x, mins.y, 0 )).Length() * 0.5f;

		float flDistFromCenter = ( vecCenter - tr.endpos ).Length() - approxTargetRadius;

		ents[nOutEnts] = pEntity;
		dists[nOutEnts] = flDistFromCenter;
		nOutEnts++;
		if ( nOutEnts >= nMaxEnts )
			return nOutEnts;
	}

	return nOutEnts;
}


CFireDamageMgr g_FireDamageMgr;

CFireDamageMgr* GetFireDamageMgr()
{
	return &g_FireDamageMgr;
}


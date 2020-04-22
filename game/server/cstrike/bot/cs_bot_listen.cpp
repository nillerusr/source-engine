//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "cs_bot.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//--------------------------------------------------------------------------------------------------------------
bool CCSBot::IsNoiseHeard( void ) const
{
	if (m_noiseTimestamp <= 0.0f)
		return false;

	// primitive reaction time simulation - cannot "hear" noise until reaction time has elapsed
	if (gpGlobals->curtime - m_noiseTimestamp >= GetProfile()->GetReactionTime())
		return true;

	return false;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Listen for enemy noises, and determine if we should react to them.
 * Returns true if heard a noise and should move to investigate.
 */
bool CCSBot::HeardInterestingNoise( void )
{
	if (IsBlind())
		return false;

	// don't investigate noises during safe time
	if (!IsWellPastSafe())
		return false;

	// if our disposition is not to investigate, dont investigate
	if (GetDisposition() != ENGAGE_AND_INVESTIGATE)
		return false;

	// listen for enemy noises
	if (IsNoiseHeard())
	{
		// if we are hiding, only react to noises very nearby, depending on how aggressive we are
		if (IsAtHidingSpot() && GetNoiseRange() > 100.0f + 400.0f * GetProfile()->GetAggression())
			return false;
		
		// chance of investigating is inversely proportional to distance
		const float maxNoiseDist = 3000.0f;
		float chance = 100.0f * (1.0f - (GetNoiseRange()/maxNoiseDist));

		// modify chance by number of friends remaining
		// if we have lots of friends, presumably one of them is closer and will check it out
		if (GetFriendsRemaining() >= 3)
		{
			float friendFactor = 5.0f * GetFriendsRemaining();
			if (friendFactor > 50.0f)
				friendFactor = 50.0f;

			chance -= friendFactor;
		}

		if (RandomFloat( 0.0f, 100.0f ) <= chance)
		{
			return true;
		}
	}

	return false;
}




//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we hear nearby threatening enemy gunfire within given range
 * -1 == infinite range
 */
bool CCSBot::CanHearNearbyEnemyGunfire( float range ) const
{
	Vector myOrigin = GetCentroid( this );

	// only attend to noise if it just happened
	if (gpGlobals->curtime - m_noiseTimestamp > 0.5f)
		return false;

	// gunfire is high priority
	if (m_noisePriority < PRIORITY_HIGH)
		return false;

	// check noise range
	if (range > 0.0f && (myOrigin - m_noisePosition).IsLengthGreaterThan( range ))
		return false;

	// if we dont have line of sight, it's not threatening (cant get shot)
	if (!CanSeeNoisePosition())
		return false;

	if (IsAttacking() && m_enemy != NULL && GetTimeSinceLastSawEnemy() < 1.0f)
	{
		// gunfire is only threatening if it is closer than our current enemy
		float gunfireDistSq = (m_noisePosition - myOrigin).LengthSqr();
		float enemyDistSq = (GetCentroid( m_enemy ) - myOrigin).LengthSqr();
		const float muchCloserSq = 100.0f * 100.0f;
		if (gunfireDistSq > enemyDistSq - muchCloserSq)
			return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we directly see where we think the noise came from
 * NOTE: Dont check FOV, since this is used to determine if we should turn our head to look at the noise
 * NOTE: Dont use IsVisible(), because smoke shouldnt cause us to not look toward noises
 */
bool CCSBot::CanSeeNoisePosition( void ) const
{
	trace_t result;
	CTraceFilterNoNPCsOrPlayer traceFilter( this, COLLISION_GROUP_NONE );
	UTIL_TraceLine( EyePositionConst(), m_noisePosition + Vector( 0, 0, HalfHumanHeight ), MASK_VISIBLE_AND_NPCS, &traceFilter, &result );
	if (result.fraction == 1.0f)
	{
		// we can see the source of the noise
		return true;
	}

	return false;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we decided to look towards the most recent noise source
 * Assumes m_noisePosition is valid.
 */
bool CCSBot::UpdateLookAtNoise( void )
{
	// make sure a noise exists
	if (!IsNoiseHeard())
	{
		return false;
	}

	Vector spot;

	// if we have clear line of sight to noise position, look directly at it
	if (CanSeeNoisePosition())
	{
		/// @todo adjust noise Z to keep consistent with current height while fighting
		spot = m_noisePosition + Vector( 0, 0, HalfHumanHeight );

		// since we can see the noise spot, forget about it
		ForgetNoise();
	}
	else
	{
		// line of sight is blocked, bend it

		// the bending algorithm is very expensive, throttle how often it is done
		if (m_noiseBendTimer.IsElapsed())
		{
			const float noiseBendLOSInterval = RandomFloat( 0.2f, 0.3f );
			m_noiseBendTimer.Start( noiseBendLOSInterval );

			// line of sight is blocked, bend it
			if (BendLineOfSight( EyePosition(), m_noisePosition, &spot ) == false)
			{
				m_bendNoisePositionValid = false;
				return false;
			}

			m_bentNoisePosition = spot;
			m_bendNoisePositionValid = true;
		}
		else if (m_bendNoisePositionValid)
		{
			// use result of prior bend computation
			spot = m_bentNoisePosition;
		}
		else
		{
			// prior bend failed
			return false;
		}
	}

	// it's always important to look at enemy noises, because they come from ... enemies!
	PriorityType pri = PRIORITY_HIGH;

	// look longer if we're hiding
	if (IsAtHidingSpot())
	{
		// if there is only one enemy left, look for a long time
		if (GetEnemiesRemaining() == 1)
		{
			SetLookAt( "Noise", spot, pri, RandomFloat( 5.0f, 15.0f ), true );
		}
		else
		{
			SetLookAt( "Noise", spot, pri, RandomFloat( 3.0f, 5.0f ), true );
		}
	}
	else
	{
		const float closeRange = 500.0f;
		if (GetNoiseRange() < closeRange)
		{
			// look at nearby enemy noises for a longer time
			SetLookAt( "Noise", spot, pri, RandomFloat( 3.0f, 5.0f ), true );
		}
		else
		{
			SetLookAt( "Noise", spot, pri, RandomFloat( 1.0f, 2.0f ), true );
		}
	}

	return true;
}


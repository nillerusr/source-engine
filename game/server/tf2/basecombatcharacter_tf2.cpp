//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: TF2 specific CBaseCombatCharacter code.
//
//=============================================================================//
#include "cbase.h"
#include "basecombatcharacter.h"
#include "engine/IEngineSound.h"
#include "tf_player.h"
#include "tf_stats.h"

extern char *g_pszEMPPulseStart;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::HasPowerup( int iPowerup )
{
	Assert( iPowerup >= 0 && iPowerup < MAX_POWERUPS );
	return ( m_iPowerups & (1 << iPowerup) ) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::CanPowerupEver( int iPowerup )
{
	Assert( iPowerup >= 0 && iPowerup < MAX_POWERUPS );

	// Only objects use power
	if ( iPowerup == POWERUP_POWER )
		return false;

	// Accept everything else
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::CanPowerupNow( int iPowerup )
{
	Assert( iPowerup >= 0 && iPowerup < MAX_POWERUPS );

	if ( !CanPowerupEver(iPowerup) )
		return false;

	switch( iPowerup )
	{
	case POWERUP_BOOST:
		{
			// Am I taking EMP damage, or is a technician trying to drain me?
			if ( HasPowerup( POWERUP_EMP ) || ( (m_flPowerupAttemptTimes[POWERUP_EMP] + 0.5) > gpGlobals->curtime ) )
			{
				// Reduce EMP time
				m_flPowerupEndTimes[POWERUP_EMP] -= 0.05;

				// Don't apply any boost effects
				return false;
			}
		}
		break;

	case POWERUP_EMP:
		{
			// Was I just boosted? If so, I don't take EMP damage for a bit
			if ( (m_flPowerupAttemptTimes[POWERUP_BOOST] + 0.5) > gpGlobals->curtime )
				return false;
		}
		break;

	default:
		break;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::SetPowerup( int iPowerup, bool bState, float flTime, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier )
{
	Assert( iPowerup >= 0 && iPowerup < MAX_POWERUPS );

	// Some powerups trigger their on state continuously, as opposed to turning it on for some time.
	bool bTriggerStart = ( bState && !HasPowerup( iPowerup ) );
	if ( bState && iPowerup == POWERUP_BOOST )
	{
		// Health boost always triggers
		bTriggerStart = true;
	}

	bool bHadPowerup = false;
	if ( HasPowerup( iPowerup ) && !bState )
	{
		bHadPowerup = true;
	}

	if ( bState )
	{
		m_iPowerups |= (1 << iPowerup);
	}
	else
	{
		m_iPowerups &= ~(1 << iPowerup);
	}

	// Fire start/end triggers
	if ( bTriggerStart  )
	{
		PowerupStart( iPowerup, flAmount, pAttacker, pDamageModifier );
	}
	else if ( bHadPowerup )
	{
		PowerupEnd( iPowerup );
	}

	// If we've got an active powerup, keep thinking
	if ( m_iPowerups )
	{
		SetContextThink( PowerupThink, gpGlobals->curtime + 0.1, POWERUP_THINK_CONTEXT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::PowerupThink( void )
{
	// If we don't have any powerups, stop thinking
	if ( !m_iPowerups )
		return;

	// Check all the powerups
	for ( int i = 0; i < MAX_POWERUPS; i++ )
	{
		// Don't check power, because it never runs out naturally
		if ( i == POWERUP_POWER )
			continue;

		if ( m_iPowerups & (1 << i) )
		{
			// Should it finish now?
			if ( m_flPowerupEndTimes[i] < gpGlobals->curtime )
			{
				SetPowerup( i, false );
			}
		}
	}

	SetNextThink( gpGlobals->curtime + 0.1, POWERUP_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseCombatCharacter::AttemptToPowerup( int iPowerup, float flTime, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier )
{
	Assert( iPowerup >= 0 && iPowerup < MAX_POWERUPS );

	// Ignore it if I'm dead
	if ( !IsAlive() )
		return false;

	m_flPowerupAttemptTimes[iPowerup] = gpGlobals->curtime;

	// If we can't be powerup this type, abort
	if ( !CanPowerupNow( iPowerup ) )
		return false;

	// Get the correct duration
	flTime = PowerupDuration( iPowerup, flTime );
	m_flPowerupEndTimes[iPowerup] = MAX( m_flPowerupEndTimes[iPowerup], gpGlobals->curtime + flTime );

	// Turn it on
	SetPowerup( iPowerup, true, flTime, flAmount, pAttacker, pDamageModifier );

	// Add the damage modifier to the player
	if ( pDamageModifier )
	{
		pDamageModifier->AddModifierToEntity( this );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just started
//-----------------------------------------------------------------------------
void CBaseCombatCharacter::PowerupStart( int iPowerup, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier )
{
	Assert( iPowerup >= 0 && iPowerup < MAX_POWERUPS );

	switch( iPowerup )
	{
	case POWERUP_BOOST:
		{
			// Players can be boosted over their max
			int iMaxBoostedHealth;
			if ( IsPlayer() )
			{
				iMaxBoostedHealth = GetMaxHealth() + GetMaxHealth() / 2;
			}
			else
			{
				iMaxBoostedHealth = GetMaxHealth();
			}

			// Can we boost health further?
			if ( GetHealth() < iMaxBoostedHealth )
			{
				int maxHealthToAdd = iMaxBoostedHealth - GetHealth();

				// It uses floating point in here so it doesn't lose the fractional healing part on small frame times.
				float flHealthToAdd = flAmount + m_flFractionalBoost;
				int nHealthToAdd = (int)flHealthToAdd;
				m_flFractionalBoost = flHealthToAdd - nHealthToAdd; 
				if ( nHealthToAdd )
				{
					int nHealthAdded = MIN( nHealthToAdd, maxHealthToAdd );
					if ( IsPlayer() )
					{
						((CBaseTFPlayer*)this)->TakeHealthBoost( nHealthAdded, GetMaxHealth(), 25 );
					}
					else
					{
						TakeHealth( nHealthAdded, DMG_GENERIC );
					}

					TFStats()->IncrementPlayerStat( pAttacker, TF_PLAYER_STAT_HEALTH_GIVEN, nHealthAdded );
				}
			}
		}
		break;

	case POWERUP_EMP:
		{
			// EMP removes adrenalin rush
			SetPowerup( POWERUP_RUSH, false );
		}
		break;

	default:
		break;
	}
}


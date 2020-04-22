//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CBaseTFPlayer functions dealing with death and reinforcement
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "player.h"
#include "tf_player.h"
#include "gamerules.h"
#include "basecombatweapon.h"
#include "EntityList.h"
#include "tf_shareddefs.h"
#include "tf_team.h"
#include "baseviewmodel.h"
#include "tf_class_infiltrator.h"
#include "in_buttons.h"
#include "globals.h"

int				g_iNumberOfCorpses;

//-----------------------------------------------------------------------------
// Purpose: The player was just killed
//-----------------------------------------------------------------------------
void CBaseTFPlayer::Event_Killed( const CTakeDamageInfo &info )
{
	// TODO don't use temp entities to transmit messages
	CPASFilter filter( GetLocalOrigin() );
	te->KillPlayerAttachments( filter, 0.0, entindex() );

	// Remove the player from any vehicle they're in
	if ( IsInAVehicle() )
	{
		LeaveVehicle();
	}

	// Holster weapon immediately, to allow it to cleanup
	if (GetActiveWeapon())
	{
		GetActiveWeapon()->Holster( );
	}

	// Stop attaching sappers
	if ( IsAttachingSapper() )
	{
		StopAttaching();
	}

	// stop them touching anything
	AddFlag( FL_DONTTOUCH );

	g_pGameRules->PlayerKilled( this, info );

	ClearUseEntity();

	// If I'm ragdolling due to a knockdown, don't play any animations
	if ( m_hRagdollShadow == NULL )
	{
		if ( PlayerClass() != TFCLASS_INFILTRATOR )
		{
			// Calculate death force
			Vector forceVector = CalcDamageForceVector( info );

			BecomeRagdollOnClient( forceVector );
		}
		else
		{
			SetAnimation( PLAYER_DIE );
		}
	}

	DeathSound( info );

	SetViewOffset( VEC_DEAD_VIEWHEIGHT_SCALED( this ) );
	m_lifeState		= LIFE_DYING;
	pl.deadflag = true;

	// Enter dying state
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	QAngle angles = GetLocalAngles();
	angles.x = angles.z = 0;
	SetLocalAngles( angles );
	m_takedamage = DAMAGE_NO;

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate(NULL, false, 0);

	// reset FOV
	SetFOV( this, 0 );

	// Setup for respawn
	m_flTimeOfDeath = gpGlobals->curtime;

	SetThink(TFPlayerDeathThink);
	SetNextThink( gpGlobals->curtime + 0.1f );

	SetPowerup(POWERUP_EMP,false);

	// Tell the playerclass that the player died
	if ( GetPlayerClass()  )
	{
		GetPlayerClass()->PlayerDied( info.GetAttacker() );
	}

	// Tell the attacker's playerclass that he killed someone
	if ( info.GetAttacker() && info.GetAttacker()->IsPlayer() )
	{
		CBaseTFPlayer *pPlayerAttacker = (CBaseTFPlayer*)info.GetAttacker();
		pPlayerAttacker->KilledPlayer( this );
	}

	DropAllResourceChunks();

	// Tell all teams to update their orders
	COrderEvent_PlayerKilled order( this );
	GlobalOrderEvent( &order );
}

//-----------------------------------------------------------------------------
// Purpose: Think function for dead/dying players.
//			Play their death animation, then set up for reinforcement.
//-----------------------------------------------------------------------------
void CBaseTFPlayer::TFPlayerDeathThink(void)
{
	float flForward;

	SetNextThink( gpGlobals->curtime + 0.1f );

	if ( GetFlags() & FL_ONGROUND )
	{
		flForward = GetAbsVelocity().Length() - 20;
		if (flForward <= 0)
		{
			SetAbsVelocity( vec3_origin );
		}
		else
		{
			Vector vecNewVelocity = GetAbsVelocity();
			VectorNormalize( vecNewVelocity );
			vecNewVelocity *= flForward;
			SetAbsVelocity( vecNewVelocity );
		}
	}

	StudioFrameAdvance();

	if (GetModelIndex() && (!IsSequenceFinished()) && (m_lifeState == LIFE_DYING))
	{
		m_iRespawnFrames++;
		if ( m_iRespawnFrames < 60 )  // animations should be no longer than this
			return;
	}

	// Start looping dying state
	SetAnimation( PLAYER_DIE );

	// ROBIN: Everyone respawns immediately now. Maps will define respawns in the future.

	if ( (gpGlobals->curtime - m_flTimeOfDeath) < 3 ) 
		return;

	m_lifeState = LIFE_RESPAWNABLE;
		
	// Respawn on button press, but not if they're checking the scores
	// Also respawn if they're not looking at scores, and they've been dead for over 5 seconds
	bool bButtonDown = (m_nButtons & ~IN_SCORE) > 0;
	if ( (bButtonDown || (gpGlobals->curtime - m_flTimeOfDeath) > 5 ) )
	{
		PlayerRespawn();
	}

	/*
	// Aliens become respawnable immediately, because they're waiting for a reinforcement wave.
	// Humans have to wait a short time.
	if ( (GetTeamNumber() == TEAM_HUMANS) && (gpGlobals->curtime - m_flTimeOfDeath) < 3 ) 
		return;
	if ( (GetTeamNumber() == TEAM_ALIENS) && (gpGlobals->curtime - m_flTimeOfDeath) < 1 ) 
		return;

	// Humans can respawn, Aliens can't (reinforcement wave for their kind)
	// Aliens stop thinking now and wait for the reinforcement wave
	if ( GetTeamNumber() == TEAM_ALIENS )
	{
		m_lifeState = LIFE_RESPAWNABLE;
		SetThink( NULL );
	}
	else
	{
		m_lifeState = LIFE_RESPAWNABLE;
		
		// Respawn on button press
		if ( m_nButtons & ~IN_SCORE )
		{
			PlayerRespawn();
		}
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this player is ready to reinforce
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsReadyToReinforce( void )
{
	// Only Aliens reinforce in waves, humans respawn normally
	if ( (GetTeamNumber() == TEAM_ALIENS) && (m_lifeState == LIFE_RESPAWNABLE) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Bring the player back to life in a reinforcement wave
//-----------------------------------------------------------------------------
void CBaseTFPlayer::Reinforce( void )
{
	// Tell all teams to update their orders
	COrderEvent_PlayerRespawned order( this );
	GlobalOrderEvent( &order );

	StopAnimation();
	IncrementInterpolationFrame();
	m_flPlaybackRate = 0.0;

	PlayerRespawn();
}


//========= Copyright Valve Corporation, All rights reserved. ============//
// tf_bot_mission_reprogrammed.cpp
// Move to target and explode
// Michael Booth, October 2011

#include "cbase.h"
#include "tf_team.h"
#include "nav_mesh.h"
#include "tf_player.h"
#include "bot/tf_bot.h"
#include "bot/behavior/missions/tf_bot_mission_reprogrammed.h"
#include "particle_parse.h"
#include "tf_obj_sentrygun.h"

#ifdef STAGING_ONLY

extern ConVar tf_bot_path_lookahead_range;
extern ConVar tf_bot_suicide_bomb_range;

ConVar tf_bot_reprogrammed_time( "tf_bot_reprogrammed_time", "8", FCVAR_CHEAT );

//---------------------------------------------------------------------------------------------
CTFBotMissionReprogrammed::CTFBotMissionReprogrammed( void )
{
}


//---------------------------------------------------------------------------------------------
ActionResult< CTFBot >	CTFBotMissionReprogrammed::OnStart( CTFBot *me, Action< CTFBot > *priorAction )
{
	m_path.SetMinLookAheadDistance( me->GetDesiredPathLookAheadRange() );
	m_detonateTimer.Invalidate();
	m_detonateSeekTimer.Invalidate();
	m_reprogrammedTimer.Invalidate();
	m_hasDetonated = false;
	m_consecutivePathFailures = 0;
	m_wasSuccessful = false;

	if ( me->HasTheFlag() )
		me->DropFlag();
	me->SetFlagTarget( NULL );

	m_victim = me->GetMissionTarget();

	// Find nearest, accessible teammate
	if ( !m_victim )
	{
		CTFPlayer *pTarget = FindNearestEnemy( me );
		if ( pTarget )
		{
			me->SetMissionTarget( pTarget );
			m_victim = pTarget;
		}
	}

	if ( m_victim )
	{
		m_lastKnownVictimPosition = m_victim->GetAbsOrigin();
	}

	// We're guaranteed to stay alive for a period of time
	me->SetHealth( 1 );
	me->m_takedamage = DAMAGE_NO;

	// Duration
	m_reprogrammedTimer.Start( tf_bot_reprogrammed_time.GetFloat() );

	return Continue();
}


//---------------------------------------------------------------------------------------------
ActionResult< CTFBot >	CTFBotMissionReprogrammed::Update( CTFBot *me, float interval )
{
	bool bDetonate = false;

	// one we start detonating, there's no turning back
	if ( m_detonateTimer.HasStarted() )
	{
		if ( m_detonateTimer.IsElapsed() )
		{
			m_vecDetLocation = me->GetAbsOrigin();
			Detonate( me );

			return Done( "KABOOM!" );
		}

		return Continue();
	}

	// Find nearest, accessible teammate
	if ( !m_victim || !m_victim->IsAlive() )
	{
		m_victim.Set( NULL );

		CTFPlayer *pTarget = FindNearestEnemy( me );
		if ( pTarget )
		{
			me->SetMissionTarget( pTarget );
			m_victim = pTarget;
		}
	}
	
	if ( m_victim )
	{
		me->m_Shared.RemoveCond( TF_COND_STUNNED );

		// update chase destination
		if ( m_victim->IsAlive() && !m_victim->IsEffectActive( EF_NODRAW ) )
		{
			m_lastKnownVictimPosition = m_victim->GetAbsOrigin();
		}
	}

	if ( m_talkTimer.IsElapsed() )
	{
		m_talkTimer.Start( 4.0f );
		me->EmitSound( "MVM.SentryBusterIntro" );
	}

	if ( m_repathTimer.IsElapsed() )
	{
		m_repathTimer.Start( RandomFloat( 0.5f, 1.0f ) );

		CTFBotPathCost cost( me, FASTEST_ROUTE );
		m_path.Compute( me, m_lastKnownVictimPosition, cost );

		if ( m_path.Compute( me, m_lastKnownVictimPosition, cost ) == false )
		{
			++m_consecutivePathFailures;

			if ( m_consecutivePathFailures >= 3 )
			{
				bDetonate = true;
			}
		}
	}

	// move to the victim
	m_path.Update( me );

	// Reprogramming time is up.  Find nearest and detonate.
	if ( m_reprogrammedTimer.IsElapsed() )
	{
		// Limit this mode to 5 seconds
		if ( !m_detonateSeekTimer.HasStarted() )
		{
			m_detonateSeekTimer.Start( 5.f );
		}
		else if ( m_detonateSeekTimer.IsElapsed() )
		{
			bDetonate = true;
		}

		// Get to a third of the damage range before detonating
		const float detonateRange = tf_bot_suicide_bomb_range.GetFloat() / 3.0f;
		if ( me->IsDistanceBetweenLessThan( m_lastKnownVictimPosition, detonateRange ) )
		{
			if ( me->IsLineOfFireClear( m_lastKnownVictimPosition + Vector( 0, 0, StepHeight ) ) )
			{
				bDetonate = true;
			}
		}
	}

	if ( bDetonate )
	{
		StartDetonate( me, true );
	}

	return Continue();
}


//---------------------------------------------------------------------------------------------
void CTFBotMissionReprogrammed::OnEnd( CTFBot *me, Action< CTFBot > *nextAction )
{
	if ( me->IsAlive() )
	{
		me->ForceChangeTeam( TEAM_SPECTATOR );
	}
}


//---------------------------------------------------------------------------------------------
EventDesiredResult< CTFBot > CTFBotMissionReprogrammed::OnKilled( CTFBot *me, const CTakeDamageInfo &info )
{
	// Keep us alive, but run to nearest enemy
	if ( !m_hasDetonated )
	{
		if ( !m_detonateTimer.HasStarted() )
		{
			StartDetonate( me );
		}
		else
		{
			Detonate( me );
		}
	}

	return TryContinue();
}


//---------------------------------------------------------------------------------------------
EventDesiredResult< CTFBot > CTFBotMissionReprogrammed::OnStuck( CTFBot *me )
{
	// we're stuck, decide to detonate now!
	if ( !m_hasDetonated && !m_detonateTimer.HasStarted() )
	{
		StartDetonate( me );
	}

	return TryContinue();
}


//---------------------------------------------------------------------------------------------
void CTFBotMissionReprogrammed::StartDetonate( CTFBot *me, bool wasSuccessful /* = false */ )
{
	if ( m_detonateTimer.HasStarted() )
		return;

	if ( !me->IsAlive() || me->GetHealth() < 1 )
	{
		if ( me->GetTeamNumber() != TEAM_SPECTATOR)
		{
			me->m_lifeState = LIFE_ALIVE;
			me->SetHealth( 1 );
		}
	}

	m_wasSuccessful = wasSuccessful;

	me->Taunt( TAUNT_BASE_WEAPON );
	m_detonateTimer.Start( 2.f );
	me->EmitSound( "MvM.SentryBusterSpin" );
}


//---------------------------------------------------------------------------------------------
void CTFBotMissionReprogrammed::Detonate( CTFBot *me )
{
	// BLAST!
	m_hasDetonated = true;
 
	DispatchParticleEffect( "explosionTrail_seeds_mvm", me->GetAbsOrigin(), me->GetAbsAngles() );
	DispatchParticleEffect( "fluidSmokeExpl_ring_mvm", me->GetAbsOrigin(), me->GetAbsAngles() );

	me->EmitSound( "MVM.SentryBusterExplode" );

	UTIL_ScreenShake( me->GetAbsOrigin(), 25.0f, 5.0f, 5.0f, 1000.0f, SHAKE_START );

	if ( !m_wasSuccessful )
	{
		if ( TFGameRules() && TFGameRules()->IsMannVsMachineMode() )
		{
			TFGameRules()->HaveAllPlayersSpeakConceptIfAllowed( MP_CONCEPT_MVM_SENTRY_BUSTER_DOWN, TF_TEAM_PVE_DEFENDERS );
		}
	}

	CUtlVector< CTFPlayer* > playerVector;
	CUtlVector< CBaseCombatCharacter* > victimVector;

	// Only damage our original team (reprogramming switches team)
	CTFTeam *pTeam = me->GetOpposingTFTeam();
	if ( pTeam )
	{
		CollectPlayers( &playerVector, pTeam->GetTeamNumber(), COLLECT_ONLY_LIVING_PLAYERS );

		// objects
		for ( int i = 0; i < pTeam->GetNumObjects(); ++i )
		{
			CBaseObject *object = pTeam->GetObject( i );
			if ( object )
			{
				victimVector.AddToTail( object );
			}
		}
	}

	// players
	for ( int i = 0; i < playerVector.Count(); ++i )
	{
		victimVector.AddToTail( playerVector[i] );
	}

	// non-player bots
	CUtlVector< INextBot * > botVector;
	TheNextBots().CollectAllBots( &botVector );
	for( int i = 0; i < botVector.Count(); ++i )
	{
		CBaseCombatCharacter *bot = botVector[i]->GetEntity();

		if ( !bot->IsPlayer() && bot->IsAlive() )
		{
			victimVector.AddToTail( bot );
		}
	}

	// Clear my mission before we have everyone take damage so I will die with the rest
	me->SetMission( CTFBot::NO_MISSION, MISSION_DOESNT_RESET_BEHAVIOR_SYSTEM );
	me->m_takedamage = DAMAGE_YES;

	// kill victims (including me)
	for( int i = 0; i < victimVector.Count(); ++i )
	{
		CBaseCombatCharacter *victim = victimVector[i];

		Vector toVictim = victim->WorldSpaceCenter() - me->WorldSpaceCenter();

		if ( toVictim.IsLengthGreaterThan( tf_bot_suicide_bomb_range.GetFloat() ) )
			continue;

		if ( victim->IsPlayer() )
		{
			color32 colorHit = { 255, 255, 255, 255 };
			UTIL_ScreenFade( victim, colorHit, 1.0f, 0.1f, FFADE_IN );
		}

		if ( me->IsLineOfFireClear( victim ) )
		{
			toVictim.NormalizeInPlace();

			const int nDamage = 1000;
			CTakeDamageInfo info( me, me, nDamage, DMG_BLAST, TF_DMG_CUSTOM_NONE );

			CalculateMeleeDamageForce( &info, toVictim, me->WorldSpaceCenter(), 1.0f );
			victim->TakeDamage( info );
		}
	}

	// make sure we're removed (in case we detonated in our spawn area where we are invulnerable)
	me->ForceChangeTeam( TEAM_SPECTATOR );
}

//---------------------------------------------------------------------------------------------
CTFPlayer *CTFBotMissionReprogrammed::FindNearestEnemy( CTFBot *me )
{
	CUtlVector< CTFPlayer* > playerVector;

	CollectPlayers( &playerVector, GetEnemyTeam( me->GetTeamNumber() ), COLLECT_ONLY_LIVING_PLAYERS );

	CTFPlayer *pClosestPlayer = NULL;
	float flClosestPlayerDist = FLT_MAX;

	FOR_EACH_VEC( playerVector, i )
	{
		if ( !playerVector[i] )
			continue;

		if ( playerVector[i] == me )
			continue;

		me->GetDistanceBetween( playerVector[i] );

		// Find closest
		float flDist = me->GetDistanceBetween( playerVector[i] );
		if ( flDist < flClosestPlayerDist )
		{
			pClosestPlayer = playerVector[i];
			flClosestPlayerDist = flDist;
		}
	}

	return pClosestPlayer;
}


//---------------------------------------------------------------------------------------------
QueryResultType CTFBotMissionReprogrammed::ShouldAttack( const INextBot *me, const CKnownEntity *them ) const
{
	return ( m_detonateTimer.HasStarted() ) ? ANSWER_NO : ANSWER_YES;
}

#endif // STAGING_ONLY


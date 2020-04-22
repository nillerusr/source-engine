//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "cs_bot.h"
#include "cs_nav_path.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//--------------------------------------------------------------------------------------------------------------
/**
 * This method is the ONLY legal way to change a bot's current state
 */
void CCSBot::SetState( BotState *state )
{
	PrintIfWatched( "%s: SetState: %s -> %s\n", GetPlayerName(), (m_state) ? m_state->GetName() : "NULL", state->GetName() );

	/*
	if ( IsDefusingBomb() )
	{
		const Vector *bombPos = GetGameState()->GetBombPosition();
		if ( bombPos != NULL )
		{
			if ( TheCSBots()->GetBombDefuser() == this )
			{
				if ( TheCSBots()->IsBombPlanted() )
				{
					Msg( "Bot %s is switching from defusing the bomb to %s\n",
						GetPlayerName(), state->GetName() );
				}
			}
		}
	}
	*/

	// if we changed state from within the special Attack state, we are no longer attacking
	if (m_isAttacking)
		StopAttacking();

	if (m_state)
		m_state->OnExit( this );

	state->OnEnter( this );

	m_state = state;
	m_stateTimestamp = gpGlobals->curtime;
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::Idle( void )
{
	SetTask( SEEK_AND_DESTROY );
	SetState( &m_idleState );
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::EscapeFromBomb( void )
{
	SetTask( ESCAPE_FROM_BOMB );
	SetState( &m_escapeFromBombState );
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::Follow( CCSPlayer *player )
{
	if (player == NULL)
		return;

	// note when we began following
	if (!m_isFollowing || m_leader != player)
		m_followTimestamp = gpGlobals->curtime;

	m_isFollowing = true;
	m_leader = player;

	SetTask( FOLLOW );
	m_followState.SetLeader( player );
	SetState( &m_followState );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Continue following our leader after finishing what we were doing
 */
void CCSBot::ContinueFollowing( void )
{
	SetTask( FOLLOW );

	m_followState.SetLeader( m_leader );

	SetState( &m_followState );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Stop following
 */
void CCSBot::StopFollowing( void )
{
	m_isFollowing = false;
	m_leader = NULL;
	m_allowAutoFollowTime = gpGlobals->curtime + 10.0f;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Begin process of rescuing hostages
 */
void CCSBot::RescueHostages( void )
{
	SetTask( RESCUE_HOSTAGES );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Use the entity
 */
void CCSBot::UseEntity( CBaseEntity *entity )
{
	m_useEntityState.SetEntity( entity );
	SetState( &m_useEntityState );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Open the door.
 * This assumes the bot is directly in front of the door with no obstructions.
 * NOTE: This state is special, like Attack, in that it suspends the current behavior and returns to it when done.
 */
void CCSBot::OpenDoor( CBaseEntity *door )
{
	m_openDoorState.SetDoor( door );
	m_isOpeningDoor = true;
	m_openDoorState.OnEnter( this );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * DEPRECATED: Use TryToHide() instead.
 * Move to a hiding place.
 * If 'searchFromArea' is non-NULL, hiding spots are looked for from that area first.
 */
void CCSBot::Hide( CNavArea *searchFromArea, float duration, float hideRange, bool holdPosition )
{
	DestroyPath();

	CNavArea *source;
	Vector sourcePos;
	if (searchFromArea)
	{
		source = searchFromArea;
		sourcePos = searchFromArea->GetCenter();
	}
	else
	{
		source = m_lastKnownArea;
		sourcePos = GetCentroid( this );
	}

	if (source == NULL)
	{
		PrintIfWatched( "Hide from area is NULL.\n" );
		Idle();
		return;
	}

	m_hideState.SetSearchArea( source );
	m_hideState.SetSearchRange( hideRange );
	m_hideState.SetDuration( duration );
	m_hideState.SetHoldPosition( holdPosition );

	// search around source area for a good hiding spot
	Vector useSpot;

	const Vector *pos = FindNearbyHidingSpot( this, sourcePos, hideRange, IsSniper() );
	if (pos == NULL)
	{
		PrintIfWatched( "No available hiding spots.\n" );
		// hide at our current position
		useSpot = GetCentroid( this );
	}
	else
	{
		useSpot = *pos;
	}

	m_hideState.SetHidingSpot( useSpot );

	// build a path to our new hiding spot
	if (ComputePath( useSpot, FASTEST_ROUTE ) == false)
	{
		PrintIfWatched( "Can't pathfind to hiding spot\n" );
		Idle();
		return;
	}

	SetState( &m_hideState );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Move to the given hiding place
 */
void CCSBot::Hide( const Vector &hidingSpot, float duration, bool holdPosition )
{
	CNavArea *hideArea = TheNavMesh->GetNearestNavArea( hidingSpot );
	if (hideArea == NULL)
	{
		PrintIfWatched( "Hiding spot off nav mesh\n" );
		Idle();
		return;
	}

	DestroyPath();

	m_hideState.SetSearchArea( hideArea );
	m_hideState.SetSearchRange( 750.0f );
	m_hideState.SetDuration( duration );
	m_hideState.SetHoldPosition( holdPosition );
	m_hideState.SetHidingSpot( hidingSpot );

	// build a path to our new hiding spot
	if (ComputePath( hidingSpot, FASTEST_ROUTE ) == false)
	{
		PrintIfWatched( "Can't pathfind to hiding spot\n" );
		Idle();
		return;
	}

	SetState( &m_hideState );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Try to hide nearby.  Return true if hiding, false if can't hide here.
 * If 'searchFromArea' is non-NULL, hiding spots are looked for from that area first.
 */
bool CCSBot::TryToHide( CNavArea *searchFromArea, float duration, float hideRange, bool holdPosition, bool useNearest )
{
	CNavArea *source;
	Vector sourcePos;
	if (searchFromArea)
	{
		source = searchFromArea;
		sourcePos = searchFromArea->GetCenter();
	}
	else
	{
		source = m_lastKnownArea;
		sourcePos = GetCentroid( this );
	}

	if (source == NULL)
	{
		PrintIfWatched( "Hide from area is NULL.\n" );
		return false;
	}

	m_hideState.SetSearchArea( source );
	m_hideState.SetSearchRange( hideRange );
	m_hideState.SetDuration( duration );
	m_hideState.SetHoldPosition( holdPosition );

	// search around source area for a good hiding spot
	const Vector *pos = FindNearbyHidingSpot( this, sourcePos, hideRange, IsSniper(), useNearest );
	if (pos == NULL)
	{
		PrintIfWatched( "No available hiding spots.\n" );
		return false;
	}

	m_hideState.SetHidingSpot( *pos );

	// build a path to our new hiding spot
	if (ComputePath( *pos, FASTEST_ROUTE ) == false)
	{
		PrintIfWatched( "Can't pathfind to hiding spot\n" );
		return false;
	}

	SetState( &m_hideState );
	return true;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Retreat to a nearby hiding spot, away from enemies
 */
bool CCSBot::TryToRetreat( float maxRange, float duration )
{
	const Vector *spot = FindNearbyRetreatSpot( this, maxRange );
	if (spot)
	{
		// ignore enemies for a second to give us time to hide
		// reaching our hiding spot clears our disposition
		IgnoreEnemies( 10.0f );

		if (duration < 0.0f)
		{
			duration = RandomFloat( 3.0f, 15.0f );
		}

		StandUp();
		Run();
		Hide( *spot, duration );

		PrintIfWatched( "Retreating to a safe spot!\n" );

		return true;
	}

	return false;
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::Hunt( void )
{
	SetState( &m_huntState );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Attack our the given victim
 * NOTE: Attacking does not change our task.
 */
void CCSBot::Attack( CCSPlayer *victim )
{
	if (victim == NULL)
		return;

	// zombies never attack
	if (cv_bot_zombie.GetBool())
		return;

	// cannot attack if we are reloading
	if (IsReloading())
		return;

	// change enemy
	SetBotEnemy( victim );

	//
	// Do not "re-enter" the attack state if we are already attacking
	//
	if (IsAttacking())
		return;

	// if we're holding a grenade, throw it at the victim
	if (IsUsingGrenade())
	{
		// throw towards their feet
		ThrowGrenade( victim->GetAbsOrigin() );
		return;
	}


	// if we are currently hiding, increase our chances of crouching and holding position
	if (IsAtHidingSpot())
		m_attackState.SetCrouchAndHold( (RandomFloat( 0.0f, 100.0f ) < 60.0f) ? true : false );
	else
		m_attackState.SetCrouchAndHold( false );

	//SetState( &m_attackState );
	//PrintIfWatched( "ATTACK BEGIN (reaction time = %g (+ update time), surprise time = %g, attack delay = %g)\n", 
	//				GetProfile()->GetReactionTime(), m_surpriseDelay, GetProfile()->GetAttackDelay() );
	m_isAttacking = true;
	m_attackState.OnEnter( this );


	Vector victimOrigin = GetCentroid( victim );

	// cheat a bit and give the bot the initial location of its victim
	m_lastEnemyPosition = victimOrigin;
	m_lastSawEnemyTimestamp = gpGlobals->curtime;
	m_aimSpreadTimestamp = gpGlobals->curtime;

	// compute the angle difference between where are looking, and where we need to look
	Vector toEnemy = victimOrigin - GetCentroid( this );

	QAngle idealAngle;
	VectorAngles( toEnemy, idealAngle );

	float deltaYaw = (float)fabs(m_lookYaw - idealAngle.y);

	while( deltaYaw > 180.0f )
		deltaYaw -= 360.0f;

	if (deltaYaw < 0.0f)
		deltaYaw = -deltaYaw;

	// immediately aim at enemy - accuracy penalty depending on how far we must turn to aim
	// accuracy is halved if we have to turn 180 degrees
	float turn = deltaYaw / 180.0f;
	float accuracy = GetProfile()->GetSkill() / (1.0f + turn);

	SetAimOffset( accuracy );

	// define time when aim offset will automatically be updated
	// longer time the more we had to turn (surprise)
	m_aimOffsetTimestamp = gpGlobals->curtime + RandomFloat( 0.25f + turn, 1.5f );

	// forget any look at targets we have
	ClearLookAt();
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Exit the Attack state
 */
void CCSBot::StopAttacking( void )
{
	PrintIfWatched( "ATTACK END\n" );
	m_attackState.OnExit( this );
	m_isAttacking = false;

	// if we are following someone, go to the Idle state after the attack to decide whether we still want to follow
	if (IsFollowing())
	{
		Idle();
	}
}


//--------------------------------------------------------------------------------------------------------------
bool CCSBot::IsAttacking( void ) const
{
	return m_isAttacking;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are escaping from the bomb
 */
bool CCSBot::IsEscapingFromBomb( void ) const
{
	if (m_state == static_cast<const BotState *>( &m_escapeFromBombState ))
		return true;

	return false;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are defusing the bomb
 */
bool CCSBot::IsDefusingBomb( void ) const
{
	if (m_state == static_cast<const BotState *>( &m_defuseBombState ))
		return true;

	return false;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are hiding
 */
bool CCSBot::IsHiding( void ) const
{
	if (m_state == static_cast<const BotState *>( &m_hideState ))
		return true;

	return false;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are hiding and at our hiding spot
 */
bool CCSBot::IsAtHidingSpot( void ) const
{
	if (!IsHiding())
		return false;

	return m_hideState.IsAtSpot();
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return number of seconds we have been at our current hiding spot
 */
float CCSBot::GetHidingTime( void ) const
{
	if (IsHiding())
	{
		return m_hideState.GetHideTime();
	}

	return 0.0f;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are huting
 */
bool CCSBot::IsHunting( void ) const
{
	if (m_state == static_cast<const BotState *>( &m_huntState ))
		return true;

	return false;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are in the MoveTo state
 */
bool CCSBot::IsMovingTo( void ) const
{
	if (m_state == static_cast<const BotState *>( &m_moveToState ))
		return true;

	return false;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if we are buying
 */
bool CCSBot::IsBuying( void ) const
{
	if (m_state == static_cast<const BotState *>( &m_buyState ))
		return true;

	return false;
}


//--------------------------------------------------------------------------------------------------------------
bool CCSBot::IsInvestigatingNoise( void ) const
{
	if (m_state == static_cast<const BotState *>( &m_investigateNoiseState ))
		return true;

	return false;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Move to potentially distant position
 */
void CCSBot::MoveTo( const Vector &pos, RouteType route )
{
	m_moveToState.SetGoalPosition( pos );
	m_moveToState.SetRouteType( route );
	SetState( &m_moveToState );
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::PlantBomb( void )
{
	SetState( &m_plantBombState );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Bomb has been dropped - go get it
 */
void CCSBot::FetchBomb( void )
{
	SetState( &m_fetchBombState );
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::DefuseBomb( void )
{
	SetState( &m_defuseBombState );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Investigate recent enemy noise
 */
void CCSBot::InvestigateNoise( void )
{
	SetState( &m_investigateNoiseState );
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::Buy( void )
{
	SetState( &m_buyState );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Move to a hiding spot and wait for initial encounter with enemy team.
 * Return false if can't do this behavior (ie: no hiding spots available).
 */
bool CCSBot::MoveToInitialEncounter( void )
{
	int myTeam = GetTeamNumber();
	int enemyTeam = OtherTeam( myTeam );

	// build a path to an enemy spawn point
	CBaseEntity *enemySpawn = TheCSBots()->GetRandomSpawn( enemyTeam );

	if (enemySpawn == NULL)
	{
		PrintIfWatched( "MoveToInitialEncounter: No enemy spawn points?\n" );
		return false;
	}

	// build a path from us to the enemy spawn
	CCSNavPath path;
	PathCost cost( this, FASTEST_ROUTE );
	path.Compute( WorldSpaceCenter(), enemySpawn->GetAbsOrigin(), cost );

	if (!path.IsValid())
	{
		PrintIfWatched( "MoveToInitialEncounter: Pathfind failed.\n" );
		return false;
	}

	// find battlefront area where teams will first meet along this path
	int i;
	for( i=0; i<path.GetSegmentCount(); ++i )
	{
		if (path[i]->area->GetEarliestOccupyTime( myTeam ) > path[i]->area->GetEarliestOccupyTime( enemyTeam ))
		{
			break;
		}
	}

	if (i == path.GetSegmentCount())
	{
		PrintIfWatched( "MoveToInitialEncounter: Can't find battlefront!\n" );
		return false;
	}

	/// @todo Remove this evil side-effect
	SetInitialEncounterArea( path[i]->area );

	// find a hiding spot on our side of the battlefront that has LOS to it
	const float maxRange = 1500.0f;
	const HidingSpot *spot = FindInitialEncounterSpot( this, path[i]->area->GetCenter(), path[i]->area->GetEarliestOccupyTime( enemyTeam ), maxRange, IsSniper() );

	if (spot == NULL)
	{
		PrintIfWatched( "MoveToInitialEncounter: Can't find a hiding spot\n" );
		return false;
	}

	float timeToWait = path[i]->area->GetEarliestOccupyTime( enemyTeam ) - spot->GetArea()->GetEarliestOccupyTime( myTeam );
	float minWaitTime = 4.0f * GetProfile()->GetAggression() + 3.0f;
	if (timeToWait < minWaitTime)
	{
		timeToWait = minWaitTime;
	}

	Hide( spot->GetPosition(), timeToWait );

	return true;
}


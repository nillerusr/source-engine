//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "cs_gamerules.h"
#include "KeyValues.h"

#include "cs_bot.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * Checks if the bot can hear the event
 */
void CCSBot::OnAudibleEvent( IGameEvent *event, CBasePlayer *player, float range, PriorityType priority, bool isHostile, bool isFootstep, const Vector *actualOrigin )
{
	/// @todo Listen to non-player sounds
	if (player == NULL)
		return;

	// don't pay attention to noise that friends make
	if (!IsEnemy( player ))
		return;

	Vector playerOrigin = GetCentroid( player );
	Vector myOrigin = GetCentroid( this );

	// If the event occurs far from the triggering player, it may override the origin
	if ( actualOrigin )
	{
		playerOrigin = *actualOrigin;
	}

	// check if noise is close enough for us to hear
	const Vector *newNoisePosition = &playerOrigin;
	float newNoiseDist = (myOrigin - *newNoisePosition).Length();
	if (newNoiseDist < range)
	{
		// we heard the sound
		if ((IsLocalPlayerWatchingMe() && cv_bot_debug.GetInt() == 3) || cv_bot_debug.GetInt() == 4)
		{
			PrintIfWatched( "Heard noise (%s from %s, pri %s, time %3.1f)\n", 
											(FStrEq( "weapon_fire", event->GetName() )) ? "Weapon fire " : "",
											(player) ? player->GetPlayerName() : "NULL",
											(priority == PRIORITY_HIGH) ? "HIGH" : ((priority == PRIORITY_MEDIUM) ? "MEDIUM" : "LOW"),
											gpGlobals->curtime );
		}

		// should we pay attention to it
		// if noise timestamp is zero, there is no prior noise
		if (m_noiseTimestamp > 0.0f)
		{
			// only overwrite recent sound if we are louder (closer), or more important - if old noise was long ago, its faded
			const float shortTermMemoryTime = 3.0f;
			if (gpGlobals->curtime - m_noiseTimestamp < shortTermMemoryTime)
			{
				// prior noise is more important - ignore new one
				if (priority < m_noisePriority)
					return;

				float oldNoiseDist = (myOrigin - m_noisePosition).Length();
				if (newNoiseDist >= oldNoiseDist)
					return;
			}
		}

		// find the area in which the noise occured
		/// @todo Better handle when noise occurs off the nav mesh
		/// @todo Make sure noise area is not through a wall or ceiling from source of noise
		/// @todo Change GetNavTravelTime to better deal with NULL destination areas
		CNavArea *noiseArea = TheNavMesh->GetNearestNavArea( *newNoisePosition );
		if (noiseArea == NULL)
		{
			PrintIfWatched( "  *** Noise occurred off the nav mesh - ignoring!\n" );
			return;
		}

		m_noiseArea = noiseArea;

		// remember noise priority
		m_noisePriority = priority;

		// randomize noise position in the area a bit - hearing isn't very accurate
		// the closer the noise is, the more accurate our placement
		/// @todo Make sure not to pick a position on the opposite side of ourselves.
		const float maxErrorRadius = 400.0f;
		const float maxHearingRange = 2000.0f;
		float errorRadius = maxErrorRadius * newNoiseDist/maxHearingRange;

		m_noisePosition.x = newNoisePosition->x + RandomFloat( -errorRadius, errorRadius );
		m_noisePosition.y = newNoisePosition->y + RandomFloat( -errorRadius, errorRadius );

		// note the *travel distance* to the noise
		m_noiseTravelDistance = GetTravelDistanceToPlayer( (CCSPlayer *)player );

		// make sure noise position remains in the same area
		m_noiseArea->GetClosestPointOnArea( m_noisePosition, &m_noisePosition );

		// note when we heard the noise
		m_noiseTimestamp = gpGlobals->curtime;

		// if we hear a nearby enemy, become alert
		const float nearbyNoiseRange = 1000.0f;
		if (m_noiseTravelDistance < nearbyNoiseRange && m_noiseTravelDistance > 0.0f)
		{
			BecomeAlert();
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnHEGrenadeDetonate( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 99999.0f, PRIORITY_HIGH, true ); // hegrenade_detonate
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnFlashbangDetonate( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 1000.0f, PRIORITY_LOW, true ); // flashbang_detonate
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnSmokeGrenadeDetonate( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 1000.0f, PRIORITY_LOW, true ); // smokegrenade_detonate
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnGrenadeBounce( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 500.0f, PRIORITY_LOW, true ); // grenade_bounce
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBulletImpact( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	// Construct an origin for the sound, since it can be far from the originating player
	Vector actualOrigin;
	actualOrigin.x = event->GetFloat( "x", 0.0f );
	actualOrigin.y = event->GetFloat( "y", 0.0f );
	actualOrigin.z = event->GetFloat( "z", 0.0f );

	/// @todo Ignoring bullet impact events for now - we dont want bots to look directly at them!
	//OnAudibleEvent( event, player, 1100.0f, PRIORITY_MEDIUM, true, false, &actualOrigin ); // bullet_impact
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBreakProp( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 1100.0f, PRIORITY_MEDIUM, true ); // break_prop
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBreakBreakable( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 1100.0f, PRIORITY_MEDIUM, true ); // break_glass
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnDoorMoving( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	OnAudibleEvent( event, player, 1100.0f, PRIORITY_MEDIUM, false ); // door_moving
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnHostageFollows( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	// player_follows needs a player
	if (player == NULL)
		return;

	// don't pay attention to noise that friends make
	if (!IsEnemy( player ))
		return;

	Vector playerOrigin = GetCentroid( player );
	Vector myOrigin = GetCentroid( this );
	const float range = 1200.0f;

	// this is here so T's not only act on the noise, but look at it, too
	if (GetTeamNumber() == TEAM_TERRORIST)
	{
		// make sure we can hear the noise
		if ((playerOrigin - myOrigin).IsLengthGreaterThan( range ))
			return;

		// tell our teammates that the hostages are being taken
		GetChatter()->HostagesBeingTaken();

		// only move if we hear them being rescued and can't see any hostages
		if (GetGameState()->GetNearestVisibleFreeHostage() == NULL)
		{			
			// since we are guarding the hostages, presumably we know where they are
			// if we're close enough to "hear" this event, either go to where the event occured,
			// or head for an escape zone to head them off
			if (GetTask() != CCSBot::GUARD_HOSTAGE_RESCUE_ZONE)
			{
				//const float headOffChance = 33.3f;
				if (true) // || RandomFloat( 0, 100 ) < headOffChance)
				{
					// head them off at a rescue zone
					if (GuardRandomZone())
					{
						SetTask( CCSBot::GUARD_HOSTAGE_RESCUE_ZONE );
						SetDisposition( CCSBot::OPPORTUNITY_FIRE );
						PrintIfWatched( "Trying to beat them to an escape zone!\n" );
					}
				}
				else
				{
					SetTask( SEEK_AND_DESTROY );
					StandUp();
					Run();
					MoveTo( playerOrigin, FASTEST_ROUTE );
				}
			}
		}
	}
	else
	{
		// CT's don't care about this noise
		return;
	}

	OnAudibleEvent( event, player, range, PRIORITY_MEDIUM, false ); // hostage_follows
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnRoundEnd( IGameEvent *event )
{
	// Morale adjustments happen even for dead players
	int winner = event->GetInt( "winner" );
	switch ( winner )
	{
	case WINNER_TER:
		if (GetTeamNumber() == TEAM_CT)
		{
			DecreaseMorale();
		}
		else
		{
			IncreaseMorale();
		}
		break;

	case WINNER_CT:
		if (GetTeamNumber() == TEAM_CT)
		{
			IncreaseMorale();
		}
		else
		{
			DecreaseMorale();
		}
		break;

	default:
		break;
	}

	m_gameState.OnRoundEnd( event );

	if ( !IsAlive() )
		return;

	if ( event->GetInt( "winner" ) == WINNER_TER )
	{
		if (GetTeamNumber() == TEAM_TERRORIST)
			GetChatter()->CelebrateWin();
	}
	else if ( event->GetInt( "winner" ) == WINNER_CT )
	{
		if (GetTeamNumber() == TEAM_CT)
			GetChatter()->CelebrateWin();
	}
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnRoundStart( IGameEvent *event )
{
	m_gameState.OnRoundStart( event );
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnHostageRescuedAll( IGameEvent *event )
{
	m_gameState.OnHostageRescuedAll( event );
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnNavBlocked( IGameEvent *event )
{
	if ( event->GetBool( "blocked" ) )
	{
		unsigned int areaID = event->GetInt( "area" );
		if ( areaID )
		{
			// An area was blocked off.  Reset our path if it has this area on it.
			for( int i=0; i<m_pathLength; ++i )
			{
				const ConnectInfo *info = &m_path[ i ];
				if ( info->area && info->area->GetID() == areaID )
				{
					DestroyPath();
					return;
				}
			}
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Invoked when bot enters a nav area
 */
void CCSBot::OnEnteredNavArea( CNavArea *newArea )
{
	// assume that we "clear" an area of enemies when we enter it
	newArea->SetClearedTimestamp( GetTeamNumber()-1 );

	// if we just entered a 'stop' area, set the flag
	if ( newArea->GetAttributes() & NAV_MESH_STOP )
	{
		m_isStopping = true;
	}

	/// @todo Flag these areas as spawn areas during load
	if (IsAtEnemySpawn())
	{
		m_hasVisitedEnemySpawn = true;
	}
}

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
void CCSBot::OnBombPickedUp( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	if (GetTeamNumber() == TEAM_CT && player)
	{
		// check if we're close enough to hear it
		const float bombPickupHearRangeSq = 1000.0f * 1000.0f;
		Vector myOrigin = GetCentroid( this );

		if ((myOrigin - player->GetAbsOrigin()).LengthSqr() < bombPickupHearRangeSq)
		{
			GetChatter()->TheyPickedUpTheBomb();
			GetGameState()->UpdateBomber( player->GetAbsOrigin() );
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBombPlanted( IGameEvent *event )
{
	m_gameState.OnBombPlanted( event );

	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	// if we're a TEAM_CT, forget what we're doing and go after the bomb
	if (GetTeamNumber() == TEAM_CT)
	{
		Idle();
	}

	// if we are following someone, stop following
	if (IsFollowing())
	{
		StopFollowing();
		Idle();
	}
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBombBeep( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	CBaseEntity *entity = UTIL_EntityByIndex( event->GetInt( "entindex" ) );
	Vector myOrigin = GetCentroid( this );

	// if we don't know where the bomb is, but heard it beep, we've discovered it
	if (GetGameState()->IsPlantedBombLocationKnown() == false && entity)
	{
		// check if we're close enough to hear it
		const float bombBeepHearRangeSq = 1500.0f * 1500.0f;
		if ((myOrigin - entity->GetAbsOrigin()).LengthSqr() < bombBeepHearRangeSq)
		{
			// radio the news to our team
			if (GetTeamNumber() == TEAM_CT && GetGameState()->GetPlantedBombsite() == CSGameState::UNKNOWN)
			{
				const CCSBotManager::Zone *zone = TheCSBots()->GetZone( entity->GetAbsOrigin() );
				if (zone)
					GetChatter()->FoundPlantedBomb( zone->m_index );
			}

			// remember where the bomb is
			GetGameState()->UpdatePlantedBomb( entity->GetAbsOrigin() );
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBombDefuseBegin( IGameEvent *event )
{
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBombDefused( IGameEvent *event )
{
	m_gameState.OnBombDefused( event );

	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	if (GetTeamNumber() == TEAM_CT)
	{
		if (TheCSBots()->GetBombTimeLeft() < 2.0f)
			GetChatter()->Say( "BarelyDefused" );
	}
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBombDefuseAbort( IGameEvent *event )
{
	if ( !IsAlive() )
		return;

	// don't react to our own events
	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player == this )
		return;

	PrintIfWatched( "BOMB DEFUSE ABORTED\n" );
}


//--------------------------------------------------------------------------------------------------------------
void CCSBot::OnBombExploded( IGameEvent *event )
{
	m_gameState.OnBombExploded( event );
}




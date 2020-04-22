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

extern int gmsgBotVoice;

//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if the radio message is an order to do something
 * NOTE: "Report in" is not considered a "command" because it doesnt ask the bot to go somewhere, or change its mind
 */
bool CCSBot::IsRadioCommand( RadioType event ) const
{
	if (event == RADIO_AFFIRMATIVE ||
		event == RADIO_NEGATIVE ||
		event == RADIO_ENEMY_SPOTTED ||
		event == RADIO_SECTOR_CLEAR ||
		event == RADIO_REPORTING_IN ||
		event == RADIO_REPORT_IN_TEAM ||
		event == RADIO_ENEMY_DOWN ||
		event == RADIO_INVALID )
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Respond to radio commands from HUMAN players
 */
void CCSBot::RespondToRadioCommands( void )
{
	// bots use the chatter system to respond to each other
	if (m_radioSubject != NULL && m_radioSubject->IsPlayer())
	{
		CCSPlayer *player = m_radioSubject;
		if (player->IsBot())
		{
			m_lastRadioCommand = RADIO_INVALID;
			return;
		}
	}
	
	if (m_lastRadioCommand == RADIO_INVALID)
		return;

	// a human player has issued a radio command
	GetChatter()->ResetRadioSilenceDuration();


	// if we are doing something important, ignore the radio
	// unless it is a "report in" request - we can do that while we continue to do other things
	/// @todo Create "uninterruptable" flag
	if (m_lastRadioCommand != RADIO_REPORT_IN_TEAM)
	{
		if (IsBusy())
		{
			// consume command
			m_lastRadioCommand = RADIO_INVALID;
			return;
		}
	}

	// wait for reaction time before responding
	// delay needs to be long enough for the radio message we're responding to to finish
	float respondTime = 1.0f + 2.0f * GetProfile()->GetReactionTime();
	if (IsRogue())
		respondTime += 2.0f;

	if (gpGlobals->curtime - m_lastRadioRecievedTimestamp < respondTime)
		return;

	// rogues won't follow commands, unless already following the player
	if (!IsFollowing() && IsRogue())
	{
		if (IsRadioCommand( m_lastRadioCommand ))
		{
			GetChatter()->Negative();
		}

		// consume command
		m_lastRadioCommand = RADIO_INVALID;
		return;
	}

	CCSPlayer *player = m_radioSubject;
	if (player == NULL)
		return;

	// respond to command
	bool canDo = false;
	const float inhibitAutoFollowDuration = 60.0f;
	switch( m_lastRadioCommand )
	{
		case RADIO_REPORT_IN_TEAM:
		{
			GetChatter()->ReportingIn();
			break;
		}

		case RADIO_FOLLOW_ME:
		case RADIO_COVER_ME:
		case RADIO_STICK_TOGETHER_TEAM:
		case RADIO_REGROUP_TEAM:
		{
			if (!IsFollowing())
			{
				Follow( player );
				player->AllowAutoFollow();
				canDo = true;
			}
			break;
		}

		case RADIO_ENEMY_SPOTTED:
		case RADIO_NEED_BACKUP:
		case RADIO_TAKING_FIRE:
			if (!IsFollowing())
			{
				Follow( player );
				GetChatter()->Say( "OnMyWay" );
				player->AllowAutoFollow();
				canDo = false;
			}
			break;

		case RADIO_TEAM_FALL_BACK:
		{
			if (TryToRetreat())
				canDo = true;
			break;
		}

		case RADIO_HOLD_THIS_POSITION:
		{
			// find the leader's area 
			SetTask( HOLD_POSITION );
			StopFollowing();
			player->InhibitAutoFollow( inhibitAutoFollowDuration );
			Hide( TheNavMesh->GetNearestNavArea( m_radioPosition ) );
			canDo = true;
			break;
		}

		case RADIO_GO_GO_GO:
		case RADIO_STORM_THE_FRONT:
			StopFollowing();
			Hunt();
			canDo = true;
			player->InhibitAutoFollow( inhibitAutoFollowDuration );
			break;

		case RADIO_GET_OUT_OF_THERE:
			if (TheCSBots()->IsBombPlanted())
			{
				EscapeFromBomb();
				player->InhibitAutoFollow( inhibitAutoFollowDuration );
				canDo = true;
			}
			break;

		case RADIO_SECTOR_CLEAR:
		{
			// if this is a defusal scenario, and the bomb is planted, 
			// and a human player cleared a bombsite, check it off our list too
			if (TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_DEFUSE_BOMB)
			{
				if (GetTeamNumber() == TEAM_CT && TheCSBots()->IsBombPlanted())
				{
					const CCSBotManager::Zone *zone = TheCSBots()->GetClosestZone( player );

					if (zone)
					{
						GetGameState()->ClearBombsite( zone->m_index );

						// if we are huting for the planted bomb, re-select bombsite
						if (GetTask() == FIND_TICKING_BOMB)
							Idle();

						canDo = true;
					}
				}
			}			
			break;
		}

		default:
			// ignore all other radio commands for now
			return;
	}

	if (canDo)
	{
		// affirmative
		GetChatter()->Affirmative();

		// if we agreed to follow a new command, put away our grenade
		if (IsRadioCommand( m_lastRadioCommand ) && IsUsingGrenade())
		{
			EquipBestWeapon();
		}
	}

	// consume command
	m_lastRadioCommand = RADIO_INVALID;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Decide if we should move to help the player, return true if we will
 */
bool CCSBot::RespondToHelpRequest( CCSPlayer *them, Place place, float maxRange )
{
	if (IsRogue())
		return false;

	// if we're busy, ignore
	if (IsBusy())
		return false;

	Vector themOrigin = GetCentroid( them );

	// if we are too far away, ignore
	if (maxRange > 0.0f)
	{
		// compute actual travel distance
		PathCost cost(this);
		float travelDistance = NavAreaTravelDistance( m_lastKnownArea, TheNavMesh->GetNearestNavArea( themOrigin ), cost );
		if (travelDistance < 0.0f)
			return false;

		if (travelDistance > maxRange)
			return false;
	}


	if (place == UNDEFINED_PLACE)
	{
		// if we have no "place" identifier, go directly to them

		// if we are already there, ignore
		float rangeSq = (them->GetAbsOrigin() - GetAbsOrigin()).LengthSqr();
		const float close = 750.0f * 750.0f;
		if (rangeSq < close)
			return true;

		MoveTo( themOrigin, FASTEST_ROUTE );
	}
	else
	{
		// if we are already there, ignore
		if (GetPlace() == place)
			return true;

		// go to where help is needed
		const Vector *pos = GetRandomSpotAtPlace( place );
		if (pos)
		{
			MoveTo( *pos, FASTEST_ROUTE );
		}
		else
		{
			MoveTo( themOrigin, FASTEST_ROUTE );
		}
	}

	// acknowledge
	GetChatter()->Say( "OnMyWay" );

	return true;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Send a radio message
 */
void CCSBot::SendRadioMessage( RadioType event )
{
	// make sure this is a radio event
	if (event <= RADIO_START_1 || event >= RADIO_END)
		return;

	PrintIfWatched( "%3.1f: SendRadioMessage( %s )\n", gpGlobals->curtime, RadioEventName[ event ] );

	// note the time the message was sent
	TheCSBots()->SetRadioMessageTimestamp( event, GetTeamNumber() );

	m_lastRadioSentTimestamp = gpGlobals->curtime;

	char slot[2];
	slot[1] = '\000';

	if (event > RADIO_START_1 && event < RADIO_START_2)
	{
		HandleMenu_Radio1( event - RADIO_START_1 );
	}
	else if (event > RADIO_START_2 && event < RADIO_START_3)
	{
		HandleMenu_Radio2( event - RADIO_START_2 );
	}
	else
	{
		HandleMenu_Radio3( event - RADIO_START_3 );
	}
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Send voice chatter.  Also sends the entindex and duration for voice feedback.
 */
void CCSBot::SpeakAudio( const char *voiceFilename, float duration, int pitch )
{
	if( !IsAlive() )
		return;

	if ( IsObserver() )
		return;

	CRecipientFilter filter;
	ConstructRadioFilter( filter );

	UserMessageBegin ( filter, "RawAudio" );
		WRITE_BYTE( pitch );
		WRITE_BYTE( entindex() );
		WRITE_FLOAT( duration );
		WRITE_STRING( voiceFilename );
	MessageEnd();

	GetChatter()->ResetRadioSilenceDuration();

	m_voiceEndTimestamp = gpGlobals->curtime + duration;
}


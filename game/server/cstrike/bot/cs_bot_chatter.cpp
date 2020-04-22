//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "cs_gamerules.h"
#include "cs_player.h"
#include "shared_util.h"
#include "engine/IEngineSound.h"
#include "KeyValues.h"

#include "bot.h"
#include "bot_util.h"
#include "cs_bot.h"
#include "cs_bot_chatter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


/**
 * @todo Fix this
 */
const Vector *GetRandomSpotAtPlace( Place place )
{
	int count = 0;

	FOR_EACH_VEC( TheNavAreas, it )
	{
		CNavArea *area = TheNavAreas[ it ];

		if (area->GetPlace() == place)
			++count;
	}

	if (count == 0)
		return NULL;

	int which = RandomInt( 0, count-1 );

	FOR_EACH_VEC( TheNavAreas, rit )
	{
		CNavArea *area = TheNavAreas[ rit ];

		if (area->GetPlace() == place && which == 0)
			return &area->GetCenter();
	}

	return NULL;
}


//---------------------------------------------------------------------------------------------------------------
/**
 * Transmit meme to other bots
 */
void BotMeme::Transmit( CCSBot *sender ) const
{
	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == NULL)
			continue;

//		if (FNullEnt( player->pev ))
//			continue;

//		if (FStrEq( STRING( player->pev->netname ), "" ))
//			continue;

		// skip self
		if (sender == player)
			continue;

		// ignore dead humans
		if (!player->IsBot() && !player->IsAlive())
			continue;

		// ignore enemies, since we can't hear them talk
		if (!player->InSameTeam( sender ))
			continue;

		// if not a bot, fail the test
		if (!player->IsBot())
			continue;

		CCSBot *bot = dynamic_cast<CCSBot *>( player );

		if ( !bot )
			continue;

		// allow bot to interpret our meme
		Interpret( sender, bot );		
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate called for help - respond
 */
void BotHelpMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	const float maxHelpRange = 3000.0f;		// 2000
	receiver->RespondToHelpRequest( sender, m_place, maxHelpRange );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate reported information about a bombsite
 */
void BotBombsiteStatusMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	// remember this bombsite's status
	if (m_status == CLEAR)
		receiver->GetGameState()->ClearBombsite( m_zoneIndex );
	else
		receiver->GetGameState()->MarkBombsiteAsPlanted( m_zoneIndex );

	// if we were heading to the just-cleared bombsite, pick another one to search
	// if our target bombsite wasn't cleared, will will continue going to it, 
	// because GetNextBombsiteToSearch() will return the same zone (since its not cleared)
	// if the bomb was planted, we will head to that bombsite
	if (receiver->GetTask() == CCSBot::FIND_TICKING_BOMB)
	{
		receiver->Idle();
		receiver->GetChatter()->Affirmative();
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate reported information about the bomb
 */
void BotBombStatusMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	// update our gamestate based on teammate's report
	switch( m_state )
	{
		case CSGameState::MOVING:
			receiver->GetGameState()->UpdateBomber( m_pos );

			// if we are hunting and see no enemies, respond
			if (!receiver->IsRogue() && receiver->IsHunting() && receiver->GetNearbyEnemyCount() == 0)
				receiver->RespondToHelpRequest( sender, TheNavMesh->GetPlace( m_pos ) );

			break;

		case CSGameState::LOOSE:
			receiver->GetGameState()->UpdateLooseBomb( m_pos );

			if (receiver->GetTask() == CCSBot::GUARD_BOMB_ZONE)
			{
				receiver->Idle();
				receiver->GetChatter()->Affirmative();		
			}
			break;
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate has asked that we follow him
 */
void BotFollowMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	if (receiver->IsRogue())
		return;

	// if we're busy, ignore
	if (receiver->IsBusy())
		return;

	// if we are too far away, ignore
	// compute actual travel distance
	Vector senderOrigin = GetCentroid( sender );
	PathCost cost( receiver );
	float travelDistance = NavAreaTravelDistance( receiver->GetLastKnownArea(), 
												  TheNavMesh->GetNearestNavArea( senderOrigin ),
												  cost );
	if (travelDistance < 0.0f)
		return;

	const float tooFar = 1000.0f;
	if (travelDistance > tooFar)
		return;

	// begin following
	receiver->Follow( sender );

	// acknowledge
	receiver->GetChatter()->Say( "CoveringFriend" );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate has asked us to defend a place
 */
void BotDefendHereMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	if (receiver->IsRogue())
		return;

	// if we're busy, ignore
	if (receiver->IsBusy())
		return;

	Place place = TheNavMesh->GetPlace( m_pos );
	if (place != UNDEFINED_PLACE)
	{
		// pick a random hiding spot in this place
		const Vector *spot = FindRandomHidingSpot( receiver, place, receiver->IsSniper() );
		if (spot)
		{
			receiver->SetTask( CCSBot::HOLD_POSITION );
			receiver->Hide( *spot );
			return;
		}
	}

	// hide nearby
	receiver->SetTask( CCSBot::HOLD_POSITION );
	receiver->Hide( TheNavMesh->GetNearestNavArea( m_pos ) );

	// acknowledge
	receiver->GetChatter()->Say( "Affirmative" );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate has asked where the bomb is planted
 */
void BotWhereBombMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	int zone = receiver->GetGameState()->GetPlantedBombsite();

	if (zone != CSGameState::UNKNOWN)
		receiver->GetChatter()->FoundPlantedBomb( zone );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate has asked us to report in
 */
void BotRequestReportMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	receiver->GetChatter()->ReportingIn();
}


//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate told us all the hostages are gone
 */
void BotAllHostagesGoneMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	receiver->GetGameState()->AllHostagesGone();

	// acknowledge
	receiver->GetChatter()->Say( "Affirmative" );
}


//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate told us a CT is talking to a hostage
 */
void BotHostageBeingTakenMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	receiver->GetGameState()->HostageWasTaken();

	// if we're busy, ignore
	if (receiver->IsBusy())
		return;

	receiver->Idle();

	// acknowledge
	receiver->GetChatter()->Say( "Affirmative" );
}


//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate heard a noise, so we shouldn't report noises for a while
 */
void BotHeardNoiseMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	receiver->GetChatter()->FriendHeardNoise();
}


//---------------------------------------------------------------------------------------------------------------
/**
 * A teammate warned about snipers, so we shouldn't warn again for awhile
 */
void BotWarnSniperMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	receiver->GetChatter()->FriendSpottedSniper();
}


//---------------------------------------------------------------------------------------------------------------
BotSpeakable::BotSpeakable()
{
	m_phrase = NULL;
}

//---------------------------------------------------------------------------------------------------------------
BotSpeakable::~BotSpeakable()
{
	if ( m_phrase )
	{
		delete[] m_phrase;
		m_phrase = NULL;
	}
}

//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------

BotPhrase::BotPhrase( bool isPlace )
{
	m_name = NULL;
	m_place = UNDEFINED_PLACE;
	m_isPlace = isPlace;
	m_radioEvent = RADIO_INVALID;
	m_isImportant = false;
	ClearCriteria();
	m_numVoiceBanks = 0;
	InitVoiceBank( 0 );
}

BotPhrase::~BotPhrase()
{
	for( int bank=0; bank<m_voiceBank.Count(); ++bank )
	{
		for( int speakable=0; speakable<m_voiceBank[bank]->Count(); ++speakable )
		{
			delete (*m_voiceBank[bank])[speakable];
		}
		delete m_voiceBank[bank];
	}

	if ( m_name )
		delete [] m_name;
}

void BotPhrase::InitVoiceBank( int bankIndex )
{
	while ( m_numVoiceBanks <= bankIndex )
	{
		m_count.AddToTail(0);
		m_index.AddToTail(0);
		m_voiceBank.AddToTail( new BotSpeakableVector );
		++m_numVoiceBanks;
	}
}

/**
 * Return a random speakable - avoid repeating
 */
char *BotPhrase::GetSpeakable( int bankIndex, float *duration ) const
{
	if (bankIndex < 0 || bankIndex >= m_numVoiceBanks || m_count[bankIndex] == 0)
	{
		if (duration)
			*duration = 0.0f;

		return NULL;
	}

	// find phrase that meets the current criteria
	int start = m_index[bankIndex];
	while(true)
	{
		BotSpeakableVector *speakables = m_voiceBank[bankIndex];
		int& index = m_index[bankIndex];
		const BotSpeakable *speak = (*speakables)[index++];

		if (m_index[bankIndex] >= m_count[bankIndex])
			m_index[bankIndex] = 0;

		// check place criteria
		// if this speakable has a place criteria, it must match to be used
		// speakables with Place of ANY will match any place
		// speakables with a specific Place will only be used if Place matches
		// speakables with Place of UNDEFINED only match Place of UNDEFINED
		if (speak->m_place == ANY_PLACE || speak->m_place == m_placeCriteria)
		{
			// check count criteria
			// if this speakable has a count criteria, it must match to be used
			// if this speakable does not have a count criteria, we dont care what the count is set to
			if (speak->m_count == UNDEFINED_COUNT || speak->m_count == MIN( m_countCriteria, COUNT_MANY ))
			{
				if (duration)
					*duration = speak->m_duration;

				return speak->m_phrase;
			}
		}

		// check if we exhausted all speakables
		if (m_index[bankIndex] == start)
		{
			if (duration)
				*duration = 0.0f;

			return NULL;
		}
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Randomly shuffle the speakable order
 */
void BotPhrase::Randomize( void )
{
	for ( int bank = 0; bank < m_voiceBank.Count(); ++bank )
	{
		BotSpeakableVector *speakables = m_voiceBank[bank];
		if ( speakables->Count() == 1 )
			continue;

		// A simple shuffle: for each array index, swap it with a random index
		for ( int index = 0; index < speakables->Count(); ++index )
		{
			int newIndex = RandomInt( 0, speakables->Count()-1 );

			BotSpeakable *speakable = (*speakables)[index];
			(*speakables)[index] = (*speakables)[newIndex];
			(*speakables)[newIndex] = speakable;
		}
	}
}


//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------

BotPhraseManager *TheBotPhrases = NULL;

BotPhraseManager::BotPhraseManager( void )
{
	m_placeCount = 0;
}


/**
 * Invoked when map changes
 */
void BotPhraseManager::OnMapChange( void )
{
	m_placeCount = 0;
}

/**
 * Removes everything from memory
 */
void BotPhraseManager::Reset( void )
{
	int i;

	// free phrase resources
	for( i=0; i<m_list.Count(); ++i )
	{
		delete m_list[i];
	}

	for( i=0; i<m_placeList.Count(); ++i )
	{
		delete m_placeList[i];
	}

	m_list.RemoveAll();
	m_placeList.RemoveAll();

	m_painPhrase = NULL;
	m_agreeWithPlanPhrase = NULL;
}


/**
 * Invoked when the round resets 
 */
void BotPhraseManager::OnRoundRestart( void )
{
	// effectively reset all interval timers
	m_placeCount = 0;

	// shuffle all the speakables
	int i;
	for( i=0; i<m_placeList.Count(); ++i )
		m_placeList[i]->Randomize();

	for( i=0; i<m_list.Count(); ++i )
		m_list[i]->Randomize();
}

BotChatterOutputType BotPhraseManager::GetOutputType( int voiceBank ) const
{
	if ( voiceBank >= 0 && voiceBank < m_output.Count() )
	{
		return m_output[voiceBank];
	}
	return BOT_CHATTER_RADIO;
}

/**
 * Initialize phrase system from database file
 */
bool BotPhraseManager::Initialize( const char *filename, int bankIndex )
{
	bool isDefault = (bankIndex == 0);

	FileHandle_t file = filesystem->Open( filename, "r" );
	if (!file)
	{
		CONSOLE_ECHO( "WARNING: Cannot access bot phrase database '%s'\n", filename );
		return false;
	}

	// BOTPORT: Redo file reading to avoid loading whole file into memory at once
	int phraseDataLength = filesystem->Size( filename );
	char *phraseDataFile = new char[ phraseDataLength ];
		
	int dataReadLength = filesystem->Read( phraseDataFile, phraseDataLength, file );	

	filesystem->Close( file );

	if ( dataReadLength > 0 )
	{
		// NULL-terminate based on the length read in, since Read() can transform \r\n to \n and
		// return fewer bytes than we were expecting.
		phraseDataFile[ dataReadLength - 1 ] = 0;
	}

	const char *phraseData = phraseDataFile;


	const int RadioPathLen = 128; // wav filenames need to be shorter than this to go over the net anyway.
	char baseDir[RadioPathLen] = "";
	char compositeFilename[RadioPathLen];

	//
	// Parse the BotChatter.db into BotPhrase collections
	//
	while( true )
	{
		phraseData = SharedParse( phraseData );
		if (!phraseData)
			break;

		char *token = SharedGetToken();

		if ( !stricmp( token, "Output" ) )
		{
			// get name of this output device
			phraseData = SharedParse( phraseData );
			if (!phraseData)
			{
				CONSOLE_ECHO( "Error parsing '%s' - expected identifier\n", filename );
				delete [] phraseDataFile;
				return false;
			}

			while ( m_output.Count() <= bankIndex )
			{
				m_output.AddToTail(BOT_CHATTER_RADIO);
			}

			char *token = SharedGetToken();
			if ( !stricmp( token, "Voice" ) )
			{
				m_output[bankIndex] = BOT_CHATTER_VOICE;
			}
		}
		else if ( !stricmp( token, "BaseDir" ) )
		{
			// get name of this output device
			phraseData = SharedParse( phraseData );
			if (!phraseData)
			{
				CONSOLE_ECHO( "Error parsing '%s' - expected identifier\n", filename );
				delete [] phraseDataFile;
				return false;
			}
			char *token = SharedGetToken();
			Q_strncpy( baseDir, token, RadioPathLen );
			Q_strncat( baseDir, "\\", RadioPathLen, -1 );
			baseDir[RadioPathLen-1] = 0;
		}
		else if (!stricmp( token, "Place" ) || !stricmp( token, "Chatter" ))
		{
			bool isPlace = (stricmp( token, "Place" )) ? false : true;

			// encountered a new phrase collection
			BotPhrase *phrase = NULL;
			if ( isDefault )
			{
				phrase = new BotPhrase( isPlace );
			}

			// get name of this phrase
			phraseData = SharedParse( phraseData );
			if (!phraseData)
			{
				CONSOLE_ECHO( "Error parsing '%s' - expected identifier\n", filename );
				delete [] phraseDataFile;
				return false;
			}
			if ( isDefault )
			{
				phrase->m_name = CloneString( SharedGetToken() );

				phrase->m_place = (isPlace) ? TheNavMesh->NameToPlace( phrase->m_name ) : UNDEFINED_PLACE;
			}
			else // look up the existing phrase
			{
				if ( isPlace )
				{
					phrase = const_cast<BotPhrase *>(GetPlace( SharedGetToken() ));
				}
				else
				{
					phrase = const_cast<BotPhrase *>(GetPhrase( SharedGetToken() ));
				}

				if ( !phrase )
				{
					CONSOLE_ECHO( "Error parsing '%s' - phrase '%s' is invalid\n", filename, SharedGetToken() );
					delete [] phraseDataFile;
					return false;
				}
			}
			phrase->InitVoiceBank( bankIndex );

			PlaceCriteria placeCriteria = ANY_PLACE;
			CountCriteria countCriteria = UNDEFINED_COUNT;
			RadioType radioEvent = RADIO_INVALID;
			bool isImportant = false;

			// read attributes of this phrase
			while( true )
			{
				// get next token
				phraseData = SharedParse( phraseData );
				if (!phraseData)
				{
					CONSOLE_ECHO( "Error parsing %s - expected 'End'\n", filename );
					delete [] phraseDataFile;
					return false;
				}
				token = SharedGetToken();

				// check for Place criteria
				if (!stricmp( token, "Place" ))
				{
					phraseData = SharedParse( phraseData );
					if (!phraseData)
					{
						CONSOLE_ECHO( "Error parsing %s - expected Place name\n", filename );
						delete [] phraseDataFile;
						return false;
					}
					token = SharedGetToken();

					// update place criteria for subsequent speak lines
					// NOTE: this assumes places must be first in the chatter database

					// check for special identifiers
					if (!stricmp( "ANY", token ))
						placeCriteria = ANY_PLACE;
					else if (!stricmp( "UNDEFINED", token ))
						placeCriteria = UNDEFINED_PLACE;
					else
						placeCriteria = TheNavMesh->NameToPlace( token );

					continue;
				}

				// check for Count criteria
				if (!stricmp( token, "Count" ))
				{
					phraseData = SharedParse( phraseData );
					if (!phraseData)
					{
						CONSOLE_ECHO( "Error parsing %s - expected Count value\n", filename );
						delete [] phraseDataFile;
						return false;
					}
					token = SharedGetToken();

					// update count criteria for subsequent speak lines
					if (!stricmp( token, "Many" ))
						countCriteria = COUNT_MANY;
					else 
						countCriteria = atoi( token );

					continue;
				}

				// check for radio equivalent
				if (!stricmp( token, "Radio" ))
				{
					phraseData = SharedParse( phraseData );
					if (!phraseData)
					{
						CONSOLE_ECHO( "Error parsing %s - expected radio event\n", filename );
						delete [] phraseDataFile;
						return false;
					}
					token = SharedGetToken();

					RadioType event = NameToRadioEvent( token );
					if (event <= RADIO_START_1 || event >= RADIO_END)
					{
						CONSOLE_ECHO( "Error parsing %s - invalid radio event '%s'\n", filename, token );
						delete [] phraseDataFile;
						return false;
					}

					radioEvent = event;

					continue;
				}

				// check for "important" flag
				if (!stricmp( token, "Important" ))
				{
					isImportant = true;
					continue;
				}

				// check for End delimiter
				if (!stricmp( token, "End" ))
					break;

				// found a phrase - add it to the collection
				BotSpeakable *speak = new BotSpeakable;
				if ( baseDir[0] )
				{
					Q_snprintf( compositeFilename, RadioPathLen, "%s%s", baseDir, token );
					speak->m_phrase = CloneString( compositeFilename );
				}
				else
				{
					speak->m_phrase = CloneString( token );
				}
				speak->m_place = placeCriteria;
				speak->m_count = countCriteria;
#ifdef POSIX
				Q_FixSlashes( speak->m_phrase );
				Q_strlower( speak->m_phrase );
#endif

				speak->m_duration = enginesound->GetSoundDuration( speak->m_phrase );

				if (speak->m_duration <= 0.0f)
				{
					if ( !engine->IsDedicatedServer() )
					{
						DevMsg( "Warning: Couldn't get duration of phrase '%s'\n", speak->m_phrase );
					}
					speak->m_duration = 1.0f;
				}

				BotSpeakableVector * speakables = phrase->m_voiceBank[ bankIndex ];
				speakables->AddToTail( speak );

				++phrase->m_count[ bankIndex ];
			}

			if ( isDefault )
			{
				phrase->m_radioEvent = radioEvent;
				phrase->m_isImportant = isImportant;
			}

			// add phrase collection to the appropriate master list
			if (isPlace)
				m_placeList.AddToTail( phrase );
			else
				m_list.AddToTail( phrase );
		}
	}

	delete [] phraseDataFile;

	m_painPhrase = GetPhrase( "Pain" );
	m_agreeWithPlanPhrase = GetPhrase( "AgreeWithPlan" );

	return true;
}

BotPhraseManager::~BotPhraseManager()
{
	Reset();
}

/**
 * Given a name, return the associated phrase collection
 */
const BotPhrase *BotPhraseManager::GetPhrase( const char *name ) const
{
	for( int i=0; i<m_list.Count(); ++i )
	{
		if (!stricmp( m_list[i]->m_name, name ))
			return m_list[i]; 
	}

	//CONSOLE_ECHO( "GetPhrase: ERROR - Invalid phrase '%s'\n", name );
	return NULL;
}

/**
 * Given an id, return the associated phrase collection
 * @todo Store phrases in a vector to make this fast
 */
/*
const BotPhrase *BotPhraseManager::GetPhrase( unsigned int place ) const
{
	for( BotPhraseList::const_iterator iter = m_list.begin(); iter != m_list.end(); ++iter )
	{
		const BotPhrase *phrase = *iter;
		if (phrase->m_place == id)
			return phrase; 
	}

	CONSOLE_ECHO( "GetPhrase: ERROR - Invalid phrase id #%d\n", id );
	return NULL;
}
*/

/**
 * Given a name, return the associated Place phrase collection
 */
const BotPhrase *BotPhraseManager::GetPlace( const char *name ) const
{
	if (name == NULL)
		return NULL;

	for( int i=0; i<m_placeList.Count(); ++i )
	{
		if (!stricmp( m_placeList[i]->m_name, name ))
			return m_placeList[i];
	}

	return NULL;
}

/**
 * Given a name, return the associated Place phrase collection
 */
const BotPhrase *BotPhraseManager::GetPlace( PlaceCriteria place ) const
{
	if (place == UNDEFINED_PLACE)
		return NULL;

	for( int i=0; i<m_placeList.Count(); ++i )
	{
		if (m_placeList[i]->m_place == place)
			return m_placeList[i];
	}

	return NULL;
}


//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------

BotStatement::BotStatement( BotChatterInterface *chatter, BotStatementType type, float expireDuration )
{
	m_chatter = chatter;

	m_next = NULL;
	m_prev = NULL;
	m_timestamp = gpGlobals->curtime;
	m_speakTimestamp = 0.0f;

	m_type = type;
	m_subject = UNDEFINED_SUBJECT;
	m_place = UNDEFINED_PLACE;
	m_meme = NULL;

	m_startTime = gpGlobals->curtime;
	m_expireTime = gpGlobals->curtime + expireDuration;
	m_isSpeaking = false;

	m_nextTime = 0.0f;
	m_index = -1;
	m_count = 0;

	m_conditionCount = 0;
}

BotStatement::~BotStatement() 
{
	if (m_meme)
		delete m_meme; 
}


//---------------------------------------------------------------------------------------------------------------
CCSBot *BotStatement::GetOwner( void ) const
{ 
	return m_chatter->GetOwner(); 
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Attach a meme to this statement, to be transmitted to other friendly bots when spoken
 */
void BotStatement::AttachMeme( BotMeme *meme )
{
	m_meme = meme;
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Add a conditions that must be true for the statement to be spoken
 */
void BotStatement::AddCondition( ConditionType condition )
{
	if (m_conditionCount < MAX_BOT_CONDITIONS)
		m_condition[ m_conditionCount++ ] = condition;
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Return true if this statement is "important" and not personality chatter
 */
bool BotStatement::IsImportant( void ) const
{
	// if a statement contains any important phrases, it is important
	for( int i=0; i<m_count; ++i )
	{
		if (m_statement[i].isPhrase && m_statement[i].phrase->IsImportant())
			return true;

		// hack for now - phrases with enemy counts are important
		if (!m_statement[i].isPhrase && m_statement[i].context == BotStatement::CURRENT_ENEMY_COUNT)
			return true;
	}

	return false;
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Verify all attached conditions 
 */
bool BotStatement::IsValid( void ) const
{
	for( int i=0; i<m_conditionCount; ++i )
	{
		switch( m_condition[i] )
		{
			case IS_IN_COMBAT:
			{
				if (!GetOwner()->IsAttacking())
					return false;
				break;
			}

/*
			case RADIO_SILENCE:
			{
				if (GetOwner()->GetChatter()->GetRadioSilenceDuration() < 10.0f)
					return false;
				break;
			}
*/

			case ENEMIES_REMAINING:
			{
				if (GetOwner()->GetEnemiesRemaining() == 0)
					return false;
				break;
			}
		}
	}

	return true;
}


//---------------------------------------------------------------------------------------------------------------
/**
 * Return true if this statement is essentially the same as the given one
 */
bool BotStatement::IsRedundant( const BotStatement *say ) const
{
	// special cases
	if (GetType() == REPORT_MY_PLAN ||
			GetType() == REPORT_REQUEST_HELP ||
			GetType() == REPORT_CRITICAL_EVENT ||
			GetType() == REPORT_ACKNOWLEDGE)
		return false;

	// check if topics are different
	if (say->GetType() != GetType())
		return false;

	if (!say->HasPlace() && !HasPlace() && !say->HasSubject() && !HasSubject())
	{
		// neither has place or subject, so they are the same
		return true;
	}

	// check if subject matter is the same
	if (say->HasPlace() && HasPlace() && say->GetPlace() == GetPlace())
	{
		// talking about the same place
		return true;
	}

	if (say->HasSubject() && HasSubject() && say->GetSubject() == GetSubject())
	{
		// talking about the same player
		return true;
	}

	return false;
}


//---------------------------------------------------------------------------------------------------------------
/**
 * Return true if this statement is no longer appropriate to say
 */
bool BotStatement::IsObsolete( void ) const
{
	// if the round is over, the only things we should say are emotes
	if (GetOwner()->GetGameState()->IsRoundOver())
	{
		if (m_type != REPORT_EMOTE)
			return true;
	}

	// If we're wanting to say "I lost him" but we've spotted another enemy,
	// we no longer need to report losing someone.
	if ( GetOwner()->GetChatter()->SeesAtLeastOneEnemy() && m_type == REPORT_ENEMY_LOST )
	{
		return true;
	}

	// check if statement lifetime has expired
	return (gpGlobals->curtime > m_expireTime);
}


//---------------------------------------------------------------------------------------------------------------
/**
 * Possibly change what were going to say base on what teammate is saying
 */
void BotStatement::Convert( const BotStatement *say )
{
	if (GetType() == REPORT_MY_PLAN && say->GetType() == REPORT_MY_PLAN)
	{
		const BotPhrase *meToo = TheBotPhrases->GetAgreeWithPlanPhrase();

		// don't reconvert
		if (m_statement[0].phrase == meToo)
			return;

		// if our plans are the same, change our statement to "me too"
		if (m_statement[0].phrase == say->m_statement[0].phrase)
		{
			if (m_place == say->m_place)
			{
				// same plan at the same place - convert to "me too"
				m_statement[0].phrase = meToo;
				m_startTime = gpGlobals->curtime + RandomFloat( 0.5f, 1.0f );
			}
			else
			{
				// same plan at different place - wait a bit to allow others to respond "me too"
				m_startTime = gpGlobals->curtime + RandomFloat( 3.0f, 4.0f );
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------------------
void BotStatement::AppendPhrase( const BotPhrase *phrase )
{
	if (phrase == NULL)
		return;

	if (m_count < MAX_BOT_PHRASES)
	{
		m_statement[ m_count ].isPhrase = true;
		m_statement[ m_count++ ].phrase = phrase;
	}
}

/**
 * Special phrases that depend on the context
 */
void BotStatement::AppendPhrase( ContextType contextPhrase )
{
	if (m_count < MAX_BOT_PHRASES)
	{
		m_statement[ m_count ].isPhrase = false;
		m_statement[ m_count++ ].context = contextPhrase;
	}
}

/**
 * Say our statement
 * m_index refers to the phrase currently being spoken, or -1 if we havent started yet
 */
bool BotStatement::Update( void )
{
	CCSBot *me = GetOwner();

	// if all of our teammates are dead, the only non-redundant statements are emotes
	if (me->GetFriendsRemaining() == 0 && GetType() != REPORT_EMOTE)
		return false;

	if (!m_isSpeaking)
	{
		m_isSpeaking = true;
		m_speakTimestamp = gpGlobals->curtime;
	}

	// special case - context dependent delay
	if (m_index >= 0 && m_statement[ m_index ].context == ACCUMULATE_ENEMIES_DELAY)
	{
		// report if we see a lot of enemies, or if enough time has passed
		const float reportTime = 2.0f;		// 1
		if (me->GetNearbyEnemyCount() > 3 || gpGlobals->curtime - m_speakTimestamp > reportTime)
		{
			// enough enemies have accumulated to expire this delay
			m_nextTime = 0.0f;
		}
	}


	if (gpGlobals->curtime > m_nextTime)
	{
		// check for end of statement
		if (++m_index == m_count)
		{
			// transmit any memes carried in this statement to our teammates
			if (m_meme)
				m_meme->Transmit( me );

			return false;
		}

		// start next part of statement
		float duration = 0.0f;
		const BotPhrase *phrase = NULL;

		if (m_statement[ m_index ].isPhrase)
		{
			// normal phrase
			phrase = m_statement[ m_index ].phrase;
		}
		else
		{
			// context-dependant phrase
			switch( m_statement[ m_index ].context )
			{
				case CURRENT_ENEMY_COUNT:
				{
					int enemyCount = me->GetNearbyEnemyCount();

					// if we are outnumbered, ask for help
					if (enemyCount-1 > me->GetNearbyFriendCount())
					{
						phrase = TheBotPhrases->GetPhrase( "Help" );
						AttachMeme( new BotHelpMeme() );
					}
					else if (enemyCount > 1)
					{
						phrase = TheBotPhrases->GetPhrase( "EnemySpotted" );
						phrase->SetCountCriteria( enemyCount );
					}
					break;
				}

				case REMAINING_ENEMY_COUNT:
				{
					static const char *speak[] = 
					{
						"NoEnemiesLeft", "OneEnemyLeft", "TwoEnemiesLeft", "ThreeEnemiesLeft"
					};

					int enemyCount = me->GetEnemiesRemaining();

					// dont report if there are lots of enemies left
					if (enemyCount < 0 || enemyCount > 3)
					{
						phrase = NULL;
					}
					else
					{
						phrase = TheBotPhrases->GetPhrase( speak[ enemyCount ] );
					}
					break;
				}

				case SHORT_DELAY:
				{
					m_nextTime = gpGlobals->curtime + RandomFloat( 0.1f, 0.5f );
					return true;
				}

				case LONG_DELAY:
				{
					m_nextTime = gpGlobals->curtime + RandomFloat( 1.0f, 2.0f );
					return true;
				}

				case ACCUMULATE_ENEMIES_DELAY:
				{
					// wait until test becomes true
					m_nextTime = 99999999.9f;
					return true;
				}
			}
		}

		if (phrase)
		{
			// if chatter system is in "standard radio" mode, send the equivalent radio command
			if (me->GetChatter()->GetVerbosity() == BotChatterInterface::RADIO)
			{
				RadioType radioEvent = phrase->GetRadioEquivalent();
				if (radioEvent == RADIO_INVALID)
				{
					// skip directly to the next phrase
					m_nextTime = 0.0f;
				}
				else
				{
					// use the standard radio
					me->GetChatter()->ResetRadioSilenceDuration();
					me->SendRadioMessage( radioEvent );
					duration = 2.0f;
				}
			}
			else
			{
				// set place criteria
				phrase->SetPlaceCriteria( m_place );

				const char *filename = phrase->GetSpeakable( me->GetProfile()->GetVoiceBank(), &duration );
				// CONSOLE_ECHO( "%s: Radio( '%s' )\n", STRING( me->pev->netname ), filename );

				bool sayIt = true;

				if (phrase->IsPlace())
				{
					// don't repeat the place if someone just mentioned it not too long ago
					float timeSince = TheBotPhrases->GetPlaceStatementInterval( phrase->GetPlace() );
					const float minRepeatTime = 20.0f;		// 30
					if (timeSince < minRepeatTime)
					{
						sayIt = false;
					}
					else
					{
						TheBotPhrases->ResetPlaceStatementInterval( phrase->GetPlace() );
					}
				}

				if (sayIt)
				{
					if ( !filename )
					{
						RadioType radioEvent = phrase->GetRadioEquivalent();
						if (radioEvent == RADIO_INVALID)
						{
							// skip directly to the next phrase
							m_nextTime = 0.0f;
						}
						else
						{
							// use the standard radio
							me->SendRadioMessage( radioEvent );
							me->GetChatter()->ResetRadioSilenceDuration();
							duration = 2.0f;
						}
					}
					/* BOTPORT: Wire up bot voice over IP
					else if ( g_engfuncs.pfnPlayClientVoice && TheBotPhrases->GetOutputType( me->GetProfile()->GetVoiceBank() ) == BOT_CHATTER_VOICE )
					{
						me->GetChatter()->ResetRadioSilenceDuration();
						g_engfuncs.pfnPlayClientVoice( me->entindex() - 1, filename );
					}
					*/
					else
					{
						me->SpeakAudio( filename, duration + 1.0f, me->GetProfile()->GetVoicePitch() );
					}
				}
			}

			const float gap = 0.1f;
			m_nextTime = gpGlobals->curtime + duration + gap;
		}
		else
		{
			// skip directly to the next phrase
			m_nextTime = 0.0f;
		}
	}
	
	return true;
}

/**
 * If this statement refers to a specific place, return that place
 * Places can be implicit in the statement, or explicitly defined
 */
unsigned int BotStatement::GetPlace( void ) const
{
	// return any explicitly set place if we have one
	if (m_place != UNDEFINED_PLACE)
		return m_place;

	// look for an implicit place in our statement
	for( int i=0; i<m_count; ++i )
		if (m_statement[i].isPhrase && m_statement[i].phrase->IsPlace())
			return m_statement[i].phrase->GetPlace();
	
	return 0;
}

/**
 * Return true if this statement has an associated count
 */
bool BotStatement::HasCount( void ) const
{
	for( int i=0; i<m_count; ++i )
		if (!m_statement[i].isPhrase && m_statement[i].context == CURRENT_ENEMY_COUNT)
			return true;

	return false;
}

//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------

CountdownTimer BotChatterInterface::m_encourageTimer;
IntervalTimer BotChatterInterface::m_radioSilenceInterval[ 2 ];


enum PitchHack 
{ 
	P_HI, 
	P_NORMAL, 
	P_LOW 
};

static int nextPitch = P_HI;

BotChatterInterface::BotChatterInterface( CCSBot *me )
{
	m_me = me;
	m_statementList = NULL;

	switch( nextPitch )
	{
		case P_HI:
			m_pitch = RandomInt( 105, 110 );
			break;

		case P_NORMAL:
			m_pitch = RandomInt( 95, 105 );	
			break;

		case P_LOW:
			m_pitch = RandomInt( 85, 95 );
			break;
	}

	nextPitch = (nextPitch + 1) % 3;

	Reset();
}

//---------------------------------------------------------------------------------------------------------------
BotChatterInterface::~BotChatterInterface()
{
	// free pending statements
	BotStatement *next;
	for( BotStatement *msg = m_statementList; msg; msg = next )
	{
		next = msg->m_next;
		delete msg;
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Reset to initial state
 */
void BotChatterInterface::Reset( void )
{
	BotStatement *msg, *nextMsg;

	// removing pending statements - except for those about the round results
	for( msg = m_statementList; msg; msg = nextMsg )
	{
		nextMsg = msg->m_next;

		if (msg->GetType() != REPORT_ROUND_END)
			RemoveStatement( msg );
	}

	m_seeAtLeastOneEnemy = false;
	m_timeWhenSawFirstEnemy = 0.0f;
	m_reportedEnemies = false;
	m_requestedBombLocation = false;

	ResetRadioSilenceDuration();

	m_needBackupInterval.Invalidate();
	m_spottedBomberInterval.Invalidate();
	m_spottedLooseBombTimer.Invalidate();
	m_heardNoiseTimer.Invalidate();
	m_scaredInterval.Invalidate();
	m_planInterval.Invalidate();
	m_encourageTimer.Invalidate();
	m_escortingHostageTimer.Invalidate();
	m_warnSniperTimer.Invalidate();
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Register a statement for speaking
 */
void BotChatterInterface::AddStatement( BotStatement *statement, bool mustAdd )
{
	// don't add statements if bot chatter is shut off
	if (GetVerbosity() == OFF)
	{
		delete statement;
		return;	
	}

	// if we only want mission-critical radio chatter, ignore non-important phrases
	if (GetVerbosity() == MINIMAL && !statement->IsImportant())
	{
		delete statement;
		return;
	}

	// don't add statements if we're dead
	if (!m_me->IsAlive() && !mustAdd)
	{
		delete statement;
		return;
	}

	// don't add empty statements
	if (statement->m_count == 0)
	{
		delete statement;
		return;
	}

	// don't add statements that are redundant with something we're already waiting to say
	BotStatement *s;
	for( s=m_statementList; s; s = s->m_next )
	{
		if (statement->IsRedundant( s ))
		{
			m_me->PrintIfWatched( "I tried to say something I'm already saying.\n" );
			delete statement;
			return;
		}
	}

	// keep statements in order of start time

	// check list is empty
	if (m_statementList == NULL)
	{
		statement->m_next = NULL;
		statement->m_prev = NULL;
		m_statementList = statement;
		return;
	}

	// list has at least one statement on it

	// insert into list in order
	BotStatement *earlier = NULL;
	for( s=m_statementList; s; s = s->m_next )
	{
		if (s->GetStartTime() > statement->GetStartTime())
			break;

		earlier = s;
	}

	// insert just after "earlier"
	if (earlier)
	{
		if (earlier->m_next)
			earlier->m_next->m_prev = statement;

		statement->m_next = earlier->m_next;

		earlier->m_next = statement;
		statement->m_prev = earlier;
	}
	else
	{
		// insert at head
		statement->m_prev = NULL;
		statement->m_next = m_statementList;
		m_statementList->m_prev = statement;
		m_statementList = statement;
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Remove a statement
 */
void BotChatterInterface::RemoveStatement( BotStatement *statement )
{
	if (statement->m_next)
		statement->m_next->m_prev = statement->m_prev;

	if (statement->m_prev)
		statement->m_prev->m_next = statement->m_next;
	else
		m_statementList = statement->m_next;

	delete statement;
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Track nearby enemy count and report enemy activity
 */
void BotChatterInterface::ReportEnemies( void )
{
	if (!m_me->IsAlive())
		return;

	if (m_me->GetNearbyEnemyCount() == 0)
	{
		m_seeAtLeastOneEnemy = false;
		m_reportedEnemies = false;
	}
	else if (!m_seeAtLeastOneEnemy)
	{
		m_seeAtLeastOneEnemy = true;
		m_timeWhenSawFirstEnemy = gpGlobals->curtime;
	}

	// determine whether we should report enemy activity
	if (!m_reportedEnemies && m_seeAtLeastOneEnemy)
	{
		// request backup if we're outnumbered
		if (m_me->IsOutnumbered() && NeedBackup())
		{
			m_reportedEnemies = true;
			return;
		}

		m_me->GetChatter()->EnemySpotted();
		m_reportedEnemies = true;
	}
}


//---------------------------------------------------------------------------------------------------------------
/**
 * Invoked when we die
 */
void BotChatterInterface::OnDeath( void )
{
	if (IsTalking())
	{
		if (m_me->GetChatter()->GetVerbosity() == BotChatterInterface::MINIMAL ||
				m_me->GetChatter()->GetVerbosity() == BotChatterInterface::NORMAL)
		{
			// we've died mid-sentance - emit a gargle of pain
			const BotPhrase *pain = TheBotPhrases->GetPainPhrase();
			if (pain)
			{
				/*
				if ( g_engfuncs.pfnPlayClientVoice && TheBotPhrases->GetOutputType( m_me->GetProfile()->GetVoiceBank() ) == BOT_CHATTER_VOICE )
				{
					g_engfuncs.pfnPlayClientVoice( m_me->entindex() - 1, pain->GetSpeakable(m_me->GetProfile()->GetVoiceBank()) );
					m_me->GetChatter()->ResetRadioSilenceDuration();
				}
				else
				*/
				{
					m_me->SpeakAudio( pain->GetSpeakable( m_me->GetProfile()->GetVoiceBank() ), 0.0f, m_me->GetProfile()->GetVoicePitch() );
				}
			}
		}
	}

	// remove all of our statements
	Reset();
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Process ongoing chatter for this bot
 */
void BotChatterInterface::Update( void )
{
	// report enemy activity
	ReportEnemies();

	// ask team to report in if we havent heard anything in awhile
	if (ShouldSpeak())
	{
		const float longTime = 30.0f;
		if (m_me->GetEnemiesRemaining() > 0 && GetRadioSilenceDuration() > longTime)
		{
			ReportIn();
		}
	}

	// speak if it is our turn
	BotStatement *say = GetActiveStatement();

	if (say)
	{
		// if our statement is active, speak it
		if (say->GetOwner() == m_me)
		{
			if (say->Update() == false)
			{
				// this statement is complete - destroy it
				RemoveStatement( say );
			}
		}
	}


	//
	// Process active statements.
	// Removed expired statements, re-order statements according to their relavence and importance
	// Remove redundant statements (ie: our teammates already said them)
	//
	const BotStatement *friendSay = GetActiveStatement();
	if (friendSay && friendSay->GetOwner() == m_me)
		friendSay = NULL;

	BotStatement *nextSay;
	for( say = m_statementList; say; say = nextSay )
	{
		nextSay = say->m_next;

		// check statement conditions
		if (!say->IsValid())
		{
			RemoveStatement( say );
			continue;
		}
			
		// don't interrupt ourselves
		if (say->IsSpeaking())
			continue;

		// check for obsolete statements
		if (say->IsObsolete())
		{
			m_me->PrintIfWatched( "Statement obsolete - removing.\n" );
			RemoveStatement( say );
			continue;
		}

		// if a teammate is saying what we were going to say, dont repeat it
		if (friendSay)
		{
			// convert what we're about to say based on what our teammate is currently saying
			say->Convert( friendSay );

			// don't say things our teammates have just said
			if (say->IsRedundant( friendSay ))
			{
				// thie statement is redundant - destroy it
				//m_me->PrintIfWatched( "Teammate said what I was going to say - shutting up.\n" );
				m_me->PrintIfWatched( "Teammate said what I was going to say - shutting up.\n" );
				RemoveStatement( say );
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Returns the statement that is being spoken, or is next to be spoken if no-one is speaking now
 */
BotStatement *BotChatterInterface::GetActiveStatement( void )
{
	// keep track of statement waiting longest to be spoken - it is next
	BotStatement *earliest = NULL;
	float earlyTime = 999999999.9f;

	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

		if (player == NULL)
			continue;

		// ignore dead humans
		if (!player->IsBot() && !player->IsAlive())
			continue;

		// ignore enemies, since we can't hear them talk
		if (!m_me->InSameTeam( player ))
			continue;

		CCSBot *bot = dynamic_cast<CCSBot *>(player);

		// if not a bot, fail the test
		/// @todo Check if human is currently talking
		if (!bot)
			continue;

		for( BotStatement *say = bot->GetChatter()->m_statementList; say; say = say->m_next )
		{
			// if this statement is currently being spoken, return it		
			if (say->IsSpeaking())
				return say;

			// keep track of statement that has been waiting longest to be spoken of anyone on our team
			if (say->GetStartTime() < earlyTime)
			{
				earlyTime = say->GetTimestamp();
				earliest = say;
			}
		}
	}

	// make sure it is time to start this statement
	if (earliest && earliest->GetStartTime() > gpGlobals->curtime)
		return NULL;

	return earliest;
}

/**
 * Return true if we speaking makes sense now
 */
bool BotChatterInterface::ShouldSpeak( void ) const
{
	// don't talk to non-existent friends
	if (m_me->GetFriendsRemaining() == 0)
		return false;

	// if everyone is together, no need to tell them what's going on
	if (m_me->GetNearbyFriendCount() == m_me->GetFriendsRemaining())
		return false;

	return true;
}

//---------------------------------------------------------------------------------------------------------------
float BotChatterInterface::GetRadioSilenceDuration( void )
{
	return m_radioSilenceInterval[ m_me->GetTeamNumber() % 2 ].GetElapsedTime();
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::ResetRadioSilenceDuration( void )
{
	m_radioSilenceInterval[ m_me->GetTeamNumber() % 2 ].Reset(); 
}



//---------------------------------------------------------------------------------------------------------------
inline const BotPhrase *GetPlacePhrase( CCSBot *me )
{
	Place place = me->GetPlace();
	if (place != UNDEFINED_PLACE)
		return TheBotPhrases->GetPlace( place );

	return NULL;
}


inline void SayWhere( BotStatement *say, Place place )
{
	say->AppendPhrase( TheBotPhrases->GetPlace( place ) );
}

/**
 * Report enemy sightings
 */
void BotChatterInterface::EnemySpotted( void )
{
	// NOTE: This could be a few seconds out of date (enemy is in an adjacent place)
	Place place = m_me->GetEnemyPlace();

	BotStatement *say = new BotStatement( this, REPORT_VISIBLE_ENEMIES, 10.0f );

	// where are the enemies
	say->AppendPhrase( TheBotPhrases->GetPlace( place ) );

	// how many are there
	say->AppendPhrase( BotStatement::ACCUMULATE_ENEMIES_DELAY );
	say->AppendPhrase( BotStatement::CURRENT_ENEMY_COUNT );
	say->AddCondition( BotStatement::IS_IN_COMBAT );

	AddStatement( say );
}


//---------------------------------------------------------------------------------------------------------------
/**
 * If a friend warned of snipers, don't warn again for awhile
 */
void BotChatterInterface::FriendSpottedSniper( void )
{
	m_warnSniperTimer.Start( 60.0f );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Warn of an enemy sniper
 */
void BotChatterInterface::SpottedSniper( void )
{
	if (!m_warnSniperTimer.IsElapsed())
	{
		return;
	}

	if (m_me->GetFriendsRemaining() == 0)
	{
		// no-one to warn
		return;
	}

	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "SniperWarning" ) );
	say->AttachMeme( new BotWarnSniperMeme() );

	AddStatement( say );
}


//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::Clear( Place place )
{
	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	SayWhere( say, place );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "Clear" ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Request enemy activity report
 */
void BotChatterInterface::ReportIn( void )
{
	BotStatement *say = new BotStatement( this, REPORT_REQUEST_INFORMATION, 10.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "RequestReport" ) );
	say->AddCondition( BotStatement::RADIO_SILENCE );
	say->AttachMeme( new BotRequestReportMeme() );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Report our situtation
 */
void BotChatterInterface::ReportingIn( void )
{
	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	// where are we
	Place place = m_me->GetPlace();
	SayWhere( say, place );

	// what are we doing
	switch( m_me->GetTask() )
	{
		case CCSBot::PLANT_BOMB:
		{
			m_me->GetChatter()->GoingToPlantTheBomb( UNDEFINED_PLACE );
			break;
		}

		case CCSBot::DEFUSE_BOMB:
		{
			m_me->GetChatter()->Say( "DefusingBomb" );
			break;
		}

		case CCSBot::GUARD_LOOSE_BOMB:
		{
			if (TheCSBots()->GetLooseBomb())
			{
				say->AppendPhrase( TheBotPhrases->GetPhrase( "GuardingLooseBomb" ) );
				say->AttachMeme( new BotBombStatusMeme( CSGameState::LOOSE, TheCSBots()->GetLooseBomb()->GetAbsOrigin() ) );
			}
			break;
		}

		case CCSBot::GUARD_HOSTAGES:
		{
			m_me->GetChatter()->GuardingHostages( UNDEFINED_PLACE, !m_me->IsAtHidingSpot() );
			break;
		}

		case CCSBot::GUARD_HOSTAGE_RESCUE_ZONE:
		{
			m_me->GetChatter()->GuardingHostageEscapeZone( !m_me->IsAtHidingSpot() );
			break;
		}

		case CCSBot::COLLECT_HOSTAGES:
		{
			break;
		}

		case CCSBot::RESCUE_HOSTAGES:
		{
			m_me->GetChatter()->EscortingHostages();
			break;
		}

		case CCSBot::GUARD_VIP_ESCAPE_ZONE:
		{
			break;
		}

	}


	// what do we see
	if (m_me->IsAttacking())
	{
		if (m_me->IsOutnumbered())
		{
			// in trouble in a firefight
			say->AppendPhrase( TheBotPhrases->GetPhrase( "Help" ) );
			say->AttachMeme( new BotHelpMeme( place ) );
		}
		else
		{
			// battling enemies
			say->AppendPhrase( TheBotPhrases->GetPhrase( "InCombat" ) );
		}
	}
	else
	{
		// not in combat, start our report a little later
		say->SetStartTime( gpGlobals->curtime + 2.0f );

		const float recentTime = 10.0f;
		if (m_me->GetEnemyDeathTimestamp() < recentTime && m_me->GetEnemyDeathTimestamp() >= m_me->GetTimeSinceLastSawEnemy() + 0.5f)
		{
			// recently saw an enemy die
			say->AppendPhrase( TheBotPhrases->GetPhrase( "EnemyDown" ) );
		}
		else if (m_me->GetTimeSinceLastSawEnemy() < recentTime)
		{
			// recently saw an enemy
			say->AppendPhrase( TheBotPhrases->GetPhrase( "EnemySpotted" ) );
		}
		else
		{
			// haven't seen enemies
			say->AppendPhrase( TheBotPhrases->GetPhrase( "Clear" ) );
		}
	}
	
	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
bool BotChatterInterface::NeedBackup( void )
{
	const float minRequestInterval = 10.0f;
	if (m_needBackupInterval.IsLessThen( minRequestInterval ))
		return false;

	m_needBackupInterval.Reset();

	if (m_me->GetFriendsRemaining() == 0)
	{
		// we're all alone...
		Scared();
		return true;
	}
	else
	{
		// ask friends for help
		BotStatement *say = new BotStatement( this, REPORT_REQUEST_HELP, 10.0f );

		// where are we
		Place place = m_me->GetPlace();
		SayWhere( say, place );

		say->AppendPhrase( TheBotPhrases->GetPhrase( "Help" ) );
		say->AttachMeme( new BotHelpMeme( place ) );

		AddStatement( say );
	}

	return true;
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::PinnedDown( void )
{
	// this is a form of "need backup"
	const float minRequestInterval = 10.0f;
	if (m_needBackupInterval.IsLessThen( minRequestInterval ))
		return;

	m_needBackupInterval.Reset();

	BotStatement *say = new BotStatement( this, REPORT_REQUEST_HELP, 10.0f );

	// where are we
	Place place = m_me->GetPlace();
	SayWhere( say, place );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "PinnedDown" ) );
	say->AttachMeme( new BotHelpMeme( place ) );
	say->AddCondition( BotStatement::IS_IN_COMBAT );

	AddStatement( say );
}


//---------------------------------------------------------------------------------------------------------------
/**
 * If a friend said that they heard something, we don't want to say something similar
 * for a while.
 */
void BotChatterInterface::FriendHeardNoise( void )
{
	m_heardNoiseTimer.Start( 20.0f );
}


//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::HeardNoise( const Vector &pos )
{
	if (TheCSBots()->IsRoundOver())
		return;

	if (m_heardNoiseTimer.IsElapsed())
	{
		// throttle frequency
		m_heardNoiseTimer.Start( 20.0f );

		// make rare, since many teammates may try to say this
		if (RandomFloat( 0, 100 ) < 33)
		{
			BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 5.0f );

			say->AppendPhrase( TheBotPhrases->GetPhrase( "HeardNoise" ) );
			say->SetPlace( TheNavMesh->GetPlace( pos ) );
			say->AttachMeme( new BotHeardNoiseMeme() );

			AddStatement( say );
		}
	}
}


//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::KilledMyEnemy( int victimID )
{
	// only report if we killed the last enemy in the area
	if (m_me->GetNearbyEnemyCount() <= 1)
		return;

	BotStatement *say = new BotStatement( this, REPORT_ENEMY_ACTION, 3.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "KilledMyEnemy" ) );
	say->SetSubject( victimID );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::EnemiesRemaining( void )
{
	// only report if we killed the last enemy in the area
	if (m_me->GetNearbyEnemyCount() > 1)
		return;

	BotStatement *say = new BotStatement( this, REPORT_ENEMIES_REMAINING, 5.0f );
	say->AppendPhrase( BotStatement::REMAINING_ENEMY_COUNT );
	say->SetStartTime( gpGlobals->curtime + RandomFloat( 2.0f, 4.0f ) );

	AddStatement( say );
}


//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::Affirmative( void )
{
	BotStatement *say = new BotStatement( this, REPORT_ACKNOWLEDGE, 3.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "Affirmative" ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::Negative( void )
{
	BotStatement *say = new BotStatement( this, REPORT_ACKNOWLEDGE, 3.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "Negative" ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::GoingToPlantTheBomb( Place place )
{
	if (TheCSBots()->IsRoundOver())
		return;

	const float minInterval = 20.0f;
	if (m_planInterval.IsLessThen( minInterval ))
		return;

	m_planInterval.Reset();

	BotStatement *say = new BotStatement( this, REPORT_CRITICAL_EVENT, 10.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "GoingToPlantBomb" ) );
	say->SetPlace( place );
	say->AttachMeme( new BotFollowMeme() );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::PlantingTheBomb( Place place )
{
	if (TheCSBots()->IsRoundOver())
		return;

	BotStatement *say = new BotStatement( this, REPORT_CRITICAL_EVENT, 10.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "PlantingBomb" ) );
	say->SetPlace( place );

	Vector myOrigin = GetCentroid( m_me );
	say->AttachMeme( new BotDefendHereMeme( myOrigin ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::TheyPickedUpTheBomb( void )
{
	if (TheCSBots()->IsRoundOver())
		return;

	// if we already know the bomb is not loose, this is old news
	if (!m_me->GetGameState()->IsBombLoose())
		return;

	// update our gamestate - use our own position for now
	const Vector &myOrigin = GetCentroid( m_me );
	m_me->GetGameState()->UpdateBomber( myOrigin );

	// tell our teammates
	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "TheyPickedUpTheBomb" ) );

	say->AttachMeme( new BotBombStatusMeme( CSGameState::MOVING, myOrigin ) ); 

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::SpottedBomber( CBasePlayer *bomber )
{
	const Vector &bomberOrigin = GetCentroid( bomber );

	if (m_me->GetGameState()->IsBombMoving())
	{
		// if we knew where the bomber was, this is old news
		const Vector *bomberPos = m_me->GetGameState()->GetBombPosition();
		const float closeRangeSq = 1000.0f * 1000.0f;
		if (bomberPos && (bomberOrigin - *bomberPos).LengthSqr() < closeRangeSq)
			return;
	}

	// update our gamestate
	m_me->GetGameState()->UpdateBomber( bomberOrigin );

	// tell our teammates
	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	// where is the bomber
	Place place = TheNavMesh->GetPlace( bomberOrigin );
	SayWhere( say, place );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "SpottedBomber" ) );

	say->SetSubject( bomber->entindex() );

	//say->AttachMeme( new BotHelpMeme( place ) );
	say->AttachMeme( new BotBombStatusMeme( CSGameState::MOVING, bomberOrigin ) ); 

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::SpottedLooseBomb( CBaseEntity *bomb )
{
	if (TheCSBots()->IsRoundOver())
		return;

	// if we already know the bomb is loose, this is old news
	if (m_me->GetGameState()->IsBombLoose())
		return;

	// update our gamestate
	m_me->GetGameState()->UpdateLooseBomb( bomb->GetAbsOrigin() );

	if (m_spottedLooseBombTimer.IsElapsed())
	{
		// throttle frequency 
		m_spottedLooseBombTimer.Start( 10.0f );

		// tell our teammates
		BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

		// where is the bomb
		Place place = TheNavMesh->GetPlace( bomb->GetAbsOrigin() );
		SayWhere( say, place );

		say->AppendPhrase( TheBotPhrases->GetPhrase( "SpottedLooseBomb" ) );

		if (TheCSBots()->GetLooseBomb())
			say->AttachMeme( new BotBombStatusMeme( CSGameState::LOOSE, bomb->GetAbsOrigin() ) );

		AddStatement( say );
	}
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::GuardingLooseBomb( CBaseEntity *bomb )
{
	// if we already know the bomb is loose, this is old news
//	if (m_me->GetGameState()->IsBombLoose())
//		return;

	if (TheCSBots()->IsRoundOver() || !bomb)
		return;

	const float minInterval = 20.0f;
	if (m_planInterval.IsLessThen( minInterval ))
		return;

	m_planInterval.Reset();

	// update our gamestate
	m_me->GetGameState()->UpdateLooseBomb( bomb->GetAbsOrigin() );

	// tell our teammates
	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	// where is the bomb
	Place place = TheNavMesh->GetPlace( bomb->GetAbsOrigin() );
	SayWhere( say, place );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "GuardingLooseBomb" ) );

	if (TheCSBots()->GetLooseBomb())
		say->AttachMeme( new BotBombStatusMeme( CSGameState::LOOSE, bomb->GetAbsOrigin() ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::RequestBombLocation( void )
{
	// only ask once per round
	if (m_requestedBombLocation)
		return;

	m_requestedBombLocation = true;

	// tell our teammates
	BotStatement *say = new BotStatement( this, REPORT_REQUEST_INFORMATION, 10.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "WhereIsTheBomb" ) );

	say->AttachMeme( new BotWhereBombMeme() );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::BombsiteClear( int zoneIndex )
{
	const CCSBotManager::Zone *zone = TheCSBots()->GetZone( zoneIndex );
	if (zone == NULL)
		return;

	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 10.0f );

	SayWhere( say, TheNavMesh->GetPlace( zone->m_center ) );
	say->AppendPhrase( TheBotPhrases->GetPhrase( "BombsiteClear" ) );

	say->AttachMeme( new BotBombsiteStatusMeme( zoneIndex, BotBombsiteStatusMeme::CLEAR ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::FoundPlantedBomb( int zoneIndex )
{
	const CCSBotManager::Zone *zone = TheCSBots()->GetZone( zoneIndex );
	if (zone == NULL)
		return;

	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 3.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "PlantedBombPlace" ) );
	say->SetPlace( TheNavMesh->GetPlace( zone->m_center ) );

	say->AttachMeme( new BotBombsiteStatusMeme( zoneIndex, BotBombsiteStatusMeme::PLANTED ) );

	AddStatement( say );
}


//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::Scared( void )
{
	const float minInterval = 10.0f;
	if (m_scaredInterval.IsLessThen( minInterval ))
		return;

	m_scaredInterval.Reset();

	BotStatement *say = new BotStatement( this, REPORT_EMOTE, 1.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "ScaredEmote" ) );
	say->AddCondition( BotStatement::IS_IN_COMBAT );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::CelebrateWin( void )
{
	BotStatement *say = new BotStatement( this, REPORT_EMOTE, 15.0f );

	// wait a bit before speaking
	say->SetStartTime( gpGlobals->curtime + RandomFloat( 2.0f, 5.0f ) );

	const float quickRound = 45.0f;

	if (m_me->GetFriendsRemaining() == 0)
	{
		// we were the last man standing
		if (TheCSBots()->GetElapsedRoundTime() < quickRound)
			say->AppendPhrase( TheBotPhrases->GetPhrase( "WonRoundQuickly" ) );
		else if (RandomFloat( 0.0f, 100.0f ) < 33.3f)
			say->AppendPhrase( TheBotPhrases->GetPhrase( "LastManStanding" ) );
	}
	else
	{
		if (TheCSBots()->GetElapsedRoundTime() < quickRound)
		{
			if (RandomFloat( 0.0f, 100.0f ) < 33.3f)
				say->AppendPhrase( TheBotPhrases->GetPhrase( "WonRoundQuickly" ) );
		}
		else if (RandomFloat( 0.0f, 100.0f ) < 10.0f)
		{
			say->AppendPhrase( TheBotPhrases->GetPhrase( "WonRound" ) );
		}
	}

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::AnnouncePlan( const char *phraseName, Place place )
{
	if (TheCSBots()->IsRoundOver())
		return;

	BotStatement *say = new BotStatement( this, REPORT_MY_PLAN, 10.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( phraseName ) );
	say->SetPlace( place );

	// wait at least a short time after round start
	say->SetStartTime( TheCSBots()->GetRoundStartTime() + RandomFloat( 2.0, 3.0f ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::GuardingBombsite( Place place )
{
	if (TheCSBots()->IsRoundOver())
		return;

	const float minInterval = 20.0f;
	if (m_planInterval.IsLessThen( minInterval ))
		return;

	m_planInterval.Reset();

	AnnouncePlan( "GoingToDefendBombsite", place );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::GuardingHostages( Place place, bool isPlan )
{
	if (TheCSBots()->IsRoundOver())
		return;

	const float minInterval = 20.0f;
	if (m_planInterval.IsLessThen( minInterval ))
		return;

	m_planInterval.Reset();

	if (isPlan)
		AnnouncePlan( "GoingToGuardHostages", place );
	else
		Say( "GuardingHostages" );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::GuardingHostageEscapeZone( bool isPlan )
{
	if (TheCSBots()->IsRoundOver())
		return;

	const float minInterval = 20.0f;
	if (m_planInterval.IsLessThen( minInterval ))
		return;

	m_planInterval.Reset();

	if (isPlan)
		AnnouncePlan( "GoingToGuardHostageEscapeZone", UNDEFINED_PLACE );
	else
		Say( "GuardingHostageEscapeZone" );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::HostagesBeingTaken( void )
{
	if (TheCSBots()->IsRoundOver())
		return;

	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 3.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "HostagesBeingTaken" ) );
	say->AttachMeme( new BotHostageBeingTakenMeme() );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::HostagesTaken( void )
{
	if (TheCSBots()->IsRoundOver())
		return;

	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 3.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "HostagesTaken" ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::TalkingToHostages( void )
{
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::EscortingHostages( void )
{
	if (TheCSBots()->IsRoundOver())
		return;

	if (m_escortingHostageTimer.IsElapsed())
	{
		// throttle frequency
		m_escortingHostageTimer.Start( 10.0f );

		BotStatement *say = new BotStatement( this, REPORT_MY_PLAN, 5.0f );

		say->AppendPhrase( TheBotPhrases->GetPhrase( "EscortingHostages" ) );

		AddStatement( say );
	}
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::HostageDown( void )
{
	if (TheCSBots()->IsRoundOver())
		return;

	BotStatement *say = new BotStatement( this, REPORT_INFORMATION, 3.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "HostageDown" ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::Encourage( const char *phraseName, float repeatInterval, float lifetime )
{
	if (m_encourageTimer.IsElapsed())
	{
		Say( phraseName, lifetime );
		m_encourageTimer.Start( repeatInterval );
	}
}


//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::KilledFriend( void )
{
	BotStatement *say = new BotStatement( this, REPORT_KILLED_FRIEND, 2.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "KilledFriend" ) );

	// give them time to react
	say->SetStartTime( gpGlobals->curtime + RandomFloat( 0.5f, 1.0f ) );

	AddStatement( say );
}

//---------------------------------------------------------------------------------------------------------------
void BotChatterInterface::FriendlyFire( void )
{
	if ( !friendlyfire.GetBool() )
		return;

	BotStatement *say = new BotStatement( this, REPORT_FRIENDLY_FIRE, 1.0f );

	say->AppendPhrase( TheBotPhrases->GetPhrase( "FriendlyFire" ) );

	// give them time to react
	say->SetStartTime( gpGlobals->curtime + RandomFloat( 0.3f, 0.5f ) );

	AddStatement( say );
}



//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Bot radio chatter system
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#ifndef CS_BOT_CHATTER_H
#define CS_BOT_CHATTER_H

#pragma warning( disable : 4786 )	// long STL names get truncated in browse info.

#include "nav_mesh.h"
#include "cs_gamestate.h"

class CCSBot;
class BotChatterInterface;

#define MAX_PLACES_PER_MAP 64

typedef unsigned int PlaceCriteria;

typedef unsigned int CountCriteria;
#define UNDEFINED_COUNT 0xFFFF
#define COUNT_CURRENT_ENEMIES	0xFF		// use the number of enemies we see right when we speak
#define COUNT_MANY 4						// equal to or greater than this is "many"

#define UNDEFINED_SUBJECT (-1)

/// @todo Make Place a class with member fuctions for this
const Vector *GetRandomSpotAtPlace( Place place );

//----------------------------------------------------------------------------------------------------
/**
 * A meme is a unit information that bots use to 
 * transmit information to each other via the radio
 */
class BotMeme
{
public:
	void Transmit( CCSBot *sender ) const;									///< transmit meme to other bots
	// It is a best practice to always have a virtual destructor in an interface
	// class. Otherwise if the derived classes have destructors they will not be
	// called.
	virtual ~BotMeme() {}
	virtual void Interpret( CCSBot *sender, CCSBot *receiver ) const = 0;	///< cause the given bot to act on this meme
};

//----------------------------------------------------------------------------------------------------
class BotHelpMeme : public BotMeme
{
public:
	BotHelpMeme( Place place = UNDEFINED_PLACE )
	{
		m_place = place;
	}

	virtual void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme

private:
	Place m_place;			///< where the help is needed
};

//----------------------------------------------------------------------------------------------------
class BotBombsiteStatusMeme : public BotMeme
{
public:
	enum StatusType { CLEAR, PLANTED };

	BotBombsiteStatusMeme( int zoneIndex, StatusType status )
	{
		m_zoneIndex = zoneIndex;
		m_status = status;
	}

	virtual void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme

private:
	int m_zoneIndex;			///< the bombsite
	StatusType m_status;		///< whether it is cleared or the bomb is there (planted)
};

//----------------------------------------------------------------------------------------------------
class BotBombStatusMeme : public BotMeme
{
public:
	BotBombStatusMeme( CSGameState::BombState state, const Vector &pos )
	{
		m_state = state;
		m_pos = pos;
	}

	virtual void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme

private:
	CSGameState::BombState m_state;
	Vector m_pos;
};

//----------------------------------------------------------------------------------------------------
class BotFollowMeme : public BotMeme
{
public:
	virtual void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme
};

//----------------------------------------------------------------------------------------------------
class BotDefendHereMeme : public BotMeme
{
public:
	BotDefendHereMeme( const Vector &pos )
	{
		m_pos = pos;
	}

	virtual void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme

private:
	Vector m_pos;
};

//----------------------------------------------------------------------------------------------------
class BotWhereBombMeme : public BotMeme
{
public:
	virtual void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme
};

//----------------------------------------------------------------------------------------------------
class BotRequestReportMeme : public BotMeme
{
public:
	virtual void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme
};

//----------------------------------------------------------------------------------------------------
class BotAllHostagesGoneMeme : public BotMeme
{
public:
	virtual void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme
};

//----------------------------------------------------------------------------------------------------
class BotHostageBeingTakenMeme : public BotMeme
{
public:
	virtual void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme
};

//----------------------------------------------------------------------------------------------------
class BotHeardNoiseMeme : public BotMeme
{
public:
	virtual void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme
};

//----------------------------------------------------------------------------------------------------
class BotWarnSniperMeme : public BotMeme
{
public:
	virtual void Interpret( CCSBot *sender, CCSBot *receiver ) const;		///< cause the given bot to act on this meme
};

//----------------------------------------------------------------------------------------------------
enum BotStatementType
{
	REPORT_VISIBLE_ENEMIES,
	REPORT_ENEMY_ACTION,
	REPORT_MY_CURRENT_TASK,
	REPORT_MY_INTENTION,
	REPORT_CRITICAL_EVENT,
	REPORT_REQUEST_HELP,
	REPORT_REQUEST_INFORMATION,
	REPORT_ROUND_END,
	REPORT_MY_PLAN,
	REPORT_INFORMATION,
	REPORT_EMOTE,
	REPORT_ACKNOWLEDGE,			///< affirmative or negative
	REPORT_ENEMIES_REMAINING,
	REPORT_FRIENDLY_FIRE,
	REPORT_KILLED_FRIEND,
	REPORT_ENEMY_LOST,

	NUM_BOT_STATEMENT_TYPES
};

//----------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------
/**
 * BotSpeakables are the smallest unit of bot chatter.
 * They represent a specific wav file of a phrase, and the criteria for which it is useful
 */
class BotSpeakable
{
public:
	BotSpeakable();
	~BotSpeakable();
	char *m_phrase;
	float m_duration;
	PlaceCriteria m_place;
	CountCriteria m_count;
};
typedef CUtlVector< BotSpeakable * > BotSpeakableVector;
typedef CUtlVector< BotSpeakableVector * > BotVoiceBankVector;


//----------------------------------------------------------------------------------------------------
/**
 * The BotPhrase class is a collection of Speakables associated with a name, ID, and criteria
 */
class BotPhrase
{
public:
	char *GetSpeakable( int bankIndex, float *duration = NULL ) const;		///< return a random speakable and its duration in seconds that meets the current criteria

	// NOTE: Criteria must be set just before the GetSpeakable() call, since they are shared among all bots
	void ClearCriteria( void ) const;
	void SetPlaceCriteria( PlaceCriteria place ) const;	///< all returned phrases must have this place criteria
	void SetCountCriteria( CountCriteria count ) const;	///< all returned phrases must have this count criteria

	const char *GetName( void ) const					{ return m_name; }
	const unsigned int GetPlace( void ) const		{ return m_place; }
	RadioType GetRadioEquivalent( void ) const	{ return m_radioEvent; }			///< return equivalent "standard radio" event
	bool IsImportant( void ) const						{ return m_isImportant; }	///< return true if this phrase is part of an important statement

	bool IsPlace( void ) const								{ return m_isPlace; }

	void Randomize( void );									///< randomly shuffle the speakable order

private:
	friend class BotPhraseManager;
	BotPhrase( bool isPlace );
	~BotPhrase();

	char *m_name;
	Place m_place;
	bool m_isPlace;											///< true if this is a Place phrase
	RadioType m_radioEvent;									///< equivalent radio event
	bool m_isImportant;										///< mission-critical statement

	mutable BotVoiceBankVector m_voiceBank;					///< array of voice banks (arrays of speakables)
	CUtlVector< int > m_count;								///< number of speakables
	mutable CUtlVector< int > m_index;						///< index of next speakable to return
	int m_numVoiceBanks;									///< number of voice banks that have been initialized
	void InitVoiceBank( int bankIndex );					///< sets up the vector of voice banks for the first bankIndex voice banks

	mutable PlaceCriteria m_placeCriteria;
	mutable CountCriteria m_countCriteria;
};
typedef CUtlVector<BotPhrase *> BotPhraseList;

inline void BotPhrase::ClearCriteria( void ) const
{
	m_placeCriteria = ANY_PLACE;
	m_countCriteria = UNDEFINED_COUNT;
}

inline void BotPhrase::SetPlaceCriteria( PlaceCriteria place ) const
{
	m_placeCriteria = place;
}

inline void BotPhrase::SetCountCriteria( CountCriteria count ) const
{
	m_countCriteria = count;
}

enum BotChatterOutputType
{
	BOT_CHATTER_RADIO,
	BOT_CHATTER_VOICE
};
typedef CUtlVector<BotChatterOutputType> BotOutputList;

//----------------------------------------------------------------------------------------------------
/**
 * The BotPhraseManager is a singleton that provides an interface to all BotPhrase collections
 */
class BotPhraseManager
{
public:
	BotPhraseManager( void );
	~BotPhraseManager();

	bool Initialize( const char *filename, int bankIndex );	///< initialize phrase system from database file for a specific voice bank (0 is the default voice bank)

	void OnRoundRestart( void );												///< invoked when round resets
	void OnMapChange( void );														///< invoked when map changes
	void Reset( void );

	const BotPhrase *GetPhrase( const char *name ) const;		///< given a name, return the associated phrase collection
	const BotPhrase *GetPainPhrase( void ) const { return m_painPhrase; }					///< optimization, replaces a static pointer to the phrase
	const BotPhrase *GetAgreeWithPlanPhrase( void ) const { return m_agreeWithPlanPhrase; }	///< optimization, replaces a static pointer to the phrase

	const BotPhrase *GetPlace( const char *name ) const;		///< given a name, return the associated Place phrase collection
	const BotPhrase *GetPlace( unsigned int id ) const;			///< given an id, return the associated Place phrase collection

	const BotPhraseList *GetPlaceList( void ) const	{ return &m_placeList; }

	float GetPlaceStatementInterval( Place where ) const;	///< return time last statement of given type was emitted by a teammate for the given place
	void ResetPlaceStatementInterval( Place where );			///< set time of last statement of given type was emitted by a teammate for the given place

	BotChatterOutputType GetOutputType( int voiceBank ) const;

private:
	BotPhraseList m_list;																	///< master list of all phrase collections
	BotPhraseList m_placeList;														///< master list of all Place phrases

	BotOutputList m_output;

	const BotPhrase *m_painPhrase;
	const BotPhrase *m_agreeWithPlanPhrase;

	struct PlaceTimeInfo
	{
		Place placeID;
		IntervalTimer timer;
	};
	mutable PlaceTimeInfo m_placeStatementHistory[ MAX_PLACES_PER_MAP ];
	mutable int m_placeCount;
	int FindPlaceIndex( Place where ) const;
};

inline int BotPhraseManager::FindPlaceIndex( Place where ) const
{
	for( int i=0; i<m_placeCount; ++i )
		if (m_placeStatementHistory[i].placeID == where)
			return i;

	// no such place - allocate it
	if (m_placeCount < MAX_PLACES_PER_MAP)
	{
		m_placeStatementHistory[ ++m_placeCount ].placeID = where;
		m_placeStatementHistory[ ++m_placeCount ].timer.Invalidate();
		return m_placeCount-1;
	}

	// place directory is full
	return -1;
}

/**
 * Return time last statement of given type was emitted by a teammate for the given place
 */
inline float BotPhraseManager::GetPlaceStatementInterval( Place place ) const
{
	int index = FindPlaceIndex( place );

	if (index < 0)
		return 999999.9f;

	if (index >= m_placeCount)
		return 999999.9f;

	return m_placeStatementHistory[ index ].timer.GetElapsedTime();
}

/**
 * Set time of last statement of given type was emitted by a teammate for the given place
 */
inline void BotPhraseManager::ResetPlaceStatementInterval( Place place )
{
	int index = FindPlaceIndex( place );

	if (index < 0)
		return;

	if (index >= m_placeCount)
		return;

	// update entry
	m_placeStatementHistory[ index ].timer.Reset();
}

extern BotPhraseManager *TheBotPhrases;



//----------------------------------------------------------------------------------------------------
/**
 * Statements are meaningful collections of phrases
 */
class BotStatement
{
public:
	BotStatement( BotChatterInterface *chatter, BotStatementType type, float expireDuration );
	~BotStatement();

	BotChatterInterface *GetChatter( void ) const	{ return m_chatter; }
	CCSBot *GetOwner( void ) const;

	BotStatementType GetType( void ) const	{ return m_type; }	///< return the type of statement this is
	bool IsImportant( void ) const;								///< return true if this statement is "important" and not personality chatter

	bool HasSubject( void ) const	{ return (m_subject == UNDEFINED_SUBJECT) ? false : true; }
	void SetSubject( int playerID )	{ m_subject = playerID; }	///< who this statement is about
	int GetSubject( void ) const	{ return m_subject; }		///< who this statement is about

	bool HasPlace( void ) const		{ return (GetPlace()) ? true : false; }
	Place GetPlace( void ) const;								///< if this statement refers to a specific place, return that place
	void SetPlace( Place where )	{ m_place = where; }		///< explicitly set place

	bool HasCount( void ) const;								///< return true if this statement has an associated count

	bool IsRedundant( const BotStatement *say ) const;			///< return true if this statement is the same as the given one
	bool IsObsolete( void ) const;								///< return true if this statement is no longer appropriate to say
	void Convert( const BotStatement *say );					///< possibly change what were going to say base on what teammate is saying
	
	void AppendPhrase( const BotPhrase *phrase );

	void SetStartTime( float timestamp )	{ m_startTime = timestamp; }	///< define the earliest time this statement can be spoken
	float GetStartTime( void ) const		{ return m_startTime; }

	enum ConditionType
	{
		IS_IN_COMBAT,
		RADIO_SILENCE,
		ENEMIES_REMAINING,

		NUM_CONDITIONS
	};

	void AddCondition( ConditionType condition );				///< conditions must be true for the statement to be spoken
	bool IsValid( void ) const;									///< verify all attached conditions 

	enum ContextType
	{
		CURRENT_ENEMY_COUNT,
		REMAINING_ENEMY_COUNT,
		SHORT_DELAY,
		LONG_DELAY,
		ACCUMULATE_ENEMIES_DELAY
	};
	void AppendPhrase( ContextType contextPhrase );				///< special phrases that depend on the context

	bool Update( void );											///< emit statement over time, return false if statement is done
	bool IsSpeaking( void ) const		{ return m_isSpeaking; }	///< return true if this statement is currently being spoken
	float GetTimestamp( void ) const	{ return m_timestamp; }		///< get time statement was created (but not necessarily started talking)

	void AttachMeme( BotMeme *meme );							///< attach a meme to this statement, to be transmitted to other friendly bots when spoken

private:
	friend class BotChatterInterface;

	BotChatterInterface *m_chatter;								///< the chatter system this statement is part of

	BotStatement *m_next, *m_prev;								///< linked list hooks

	BotStatementType m_type;									///< what kind of statement this is
	int m_subject;												///< who this subject is about
	Place m_place;												///< explicit place - note some phrases have implicit places as well
	BotMeme *m_meme;											///< a statement can only have a single meme for now

	float m_timestamp;											///< time when message was created
	float m_startTime;											///< the earliest time this statement can be spoken
	float m_expireTime;											///< time when this statement is no longer valid
	float m_speakTimestamp;										///< time when message began being spoken
	bool m_isSpeaking;											///< true if this statement is current being spoken

	float m_nextTime;											///< time for next phrase to begin

	enum { MAX_BOT_PHRASES = 4 };
	struct
	{
		bool isPhrase;
		union
		{
			const BotPhrase *phrase;
			ContextType context;
		};
	}
	m_statement[ MAX_BOT_PHRASES ];

	enum { MAX_BOT_CONDITIONS = 4 };
	ConditionType m_condition[ MAX_BOT_CONDITIONS ];			///< conditions that must be true for the statement to be said
	int m_conditionCount;

	int m_index;												///< m_index refers to the phrase currently being spoken, or -1 if we havent started yet
	int m_count;
};

//----------------------------------------------------------------------------------------------------
/**
 * This class defines the interface to the bot radio chatter system
 */
class BotChatterInterface
{
public:
	BotChatterInterface( CCSBot *me );
	~BotChatterInterface( );

	void Reset( void );											///< reset to initial state
	void Update( void );										///< process ongoing chatter

	/// invoked when event occurs in the game (some events have NULL entities)
	void OnDeath( void );										///< invoked when we die

	enum VerbosityType
	{
		NORMAL,				///< full chatter
		MINIMAL,			///< only scenario-critical events
		RADIO,				///< use the standard radio instead
		OFF						///< no chatter at all
	};
	VerbosityType GetVerbosity( void ) const;					///< return our current level of verbosity

	CCSBot *GetOwner( void ) const							{ return m_me; }

	bool IsTalking( void ) const;								///< return true if we are currently talking
	float GetRadioSilenceDuration( void );						///< return time since any teammate said anything
	void ResetRadioSilenceDuration( void );

	enum { MUST_ADD = 1 };
	void AddStatement( BotStatement *statement, bool mustAdd = false );			///< register a statement for speaking
	void RemoveStatement( BotStatement *statement );			///< remove a statement

	BotStatement *GetActiveStatement( void );					///< returns the statement that is being spoken, or is next to be spoken if no-one is speaking now
	BotStatement *GetStatement( void ) const;					///< returns our current statement, or NULL if we aren't speaking

	int GetPitch( void ) const			{ return m_pitch; }


	//-- things the bots can say ---------------------------------------------------------------------
	void Say( const char *phraseName, float lifetime = 3.0f, float delay = 0.0f );

	void AnnouncePlan( const char *phraseName, Place where );
	void Affirmative( void );
	void Negative( void );

	void EnemySpotted( void );									///< report enemy sightings
	void KilledMyEnemy( int victimID );
	void EnemiesRemaining( void );

	void SpottedSniper( void );
	void FriendSpottedSniper( void );

	void Clear( Place where );

	void ReportIn( void );										///< ask for current situation
	void ReportingIn( void );									///< report current situation

	bool NeedBackup( void );
	void PinnedDown( void );
	void Scared( void );
	void HeardNoise( const Vector &pos );
	void FriendHeardNoise( void );

	void TheyPickedUpTheBomb( void );
	void GoingToPlantTheBomb( Place where );
	void BombsiteClear( int zoneIndex );
	void FoundPlantedBomb( int zoneIndex );
	void PlantingTheBomb( Place where );
	void SpottedBomber( CBasePlayer *bomber );
	void SpottedLooseBomb( CBaseEntity *bomb );
	void GuardingLooseBomb( CBaseEntity *bomb );
	void RequestBombLocation( void );

	#define IS_PLAN true
	void GuardingHostages( Place where, bool isPlan = false );
	void GuardingHostageEscapeZone( bool isPlan = false );
	void HostagesBeingTaken( void );
	void HostagesTaken( void );
	void TalkingToHostages( void );
	void EscortingHostages( void );
	void HostageDown( void );
	void GuardingBombsite( Place where );

	void CelebrateWin( void );

	void Encourage( const char *phraseName, float repeatInterval = 10.0f, float lifetime = 3.0f );	///< "encourage" the player to do the scenario

	void KilledFriend( void );
	void FriendlyFire( void );

	bool SeesAtLeastOneEnemy( void ) const { return m_seeAtLeastOneEnemy; }

private:
	BotStatement *m_statementList;										///< list of all active/pending messages for this bot

	void ReportEnemies( void );											///< track nearby enemy count and generate enemy activity statements
	bool ShouldSpeak( void ) const;										///< return true if we speaking makes sense now

	CCSBot *m_me;														///< the bot this chatter is for

	bool m_seeAtLeastOneEnemy;
	float m_timeWhenSawFirstEnemy;
	bool m_reportedEnemies;
	bool m_requestedBombLocation;										///< true if we already asked where the bomb has been planted

	int m_pitch;

	static IntervalTimer m_radioSilenceInterval[ 2 ];					///< one timer for each team

	IntervalTimer m_needBackupInterval;
	IntervalTimer m_spottedBomberInterval;
	IntervalTimer m_scaredInterval;
	IntervalTimer m_planInterval;
	CountdownTimer m_spottedLooseBombTimer;
	CountdownTimer m_heardNoiseTimer;
	CountdownTimer m_escortingHostageTimer;
	CountdownTimer m_warnSniperTimer;
	static CountdownTimer m_encourageTimer;								///< timer to know when we can "encourage" the human player again - shared by all bots
};

inline BotChatterInterface::VerbosityType BotChatterInterface::GetVerbosity( void ) const
{
	const char *string = cv_bot_chatter.GetString();

	if (string == NULL)
		return NORMAL;

	if (string[0] == 'm' || string[0] == 'M')
		return MINIMAL;

	if (string[0] == 'r' || string[0] == 'R')
		return RADIO;

	if (string[0] == 'o' || string[0] == 'O')
		return OFF;

	return NORMAL;
}


inline bool BotChatterInterface::IsTalking( void ) const	
{ 
	if (m_statementList)
		return m_statementList->IsSpeaking();

	return false;
}

inline BotStatement *BotChatterInterface::GetStatement( void ) const
{
	return m_statementList;
}


inline void BotChatterInterface::Say( const char *phraseName, float lifetime, float delay )
{
	BotStatement *say = new BotStatement( this, REPORT_MY_INTENTION, lifetime );

	say->AppendPhrase( TheBotPhrases->GetPhrase( phraseName ) );

	if (delay > 0.0f)
		say->SetStartTime( gpGlobals->curtime + delay );

	AddStatement( say );
}




#endif // CS_BOT_CHATTER_H

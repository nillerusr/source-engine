//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#ifndef _GAME_STATE_H_
#define _GAME_STATE_H_


#include "bot_util.h"


class CHostage;
class CCSBot;

/**
 * This class represents the game state as known by a particular bot
 */
class CSGameState
{
public:
	CSGameState( CCSBot *owner );

	void Reset( void );

	// Event handling
	void OnHostageRescuedAll( IGameEvent *event );
	void OnRoundEnd( IGameEvent *event );
	void OnRoundStart( IGameEvent *event );
	void OnBombPlanted( IGameEvent *event );
	void OnBombDefused( IGameEvent *event );
	void OnBombExploded( IGameEvent *event );

	bool IsRoundOver( void ) const;								///< true if round has been won or lost (but not yet reset)

	// bomb defuse scenario -----------------------------------------------------------------------------

	enum BombState
	{
		MOVING,					///< being carried by a Terrorist
		LOOSE,					///< loose on the ground somewhere
		PLANTED,				///< planted and ticking
		DEFUSED,				///< the bomb has been defused
		EXPLODED				///< the bomb has exploded
	};

	bool IsBombMoving( void ) const						{ return (m_bombState == MOVING); }
	bool IsBombLoose( void ) const						{ return (m_bombState == LOOSE); }
	bool IsBombPlanted( void ) const					{ return (m_bombState == PLANTED); }
	bool IsBombDefused( void ) const					{ return (m_bombState == DEFUSED); }
	bool IsBombExploded( void ) const					{ return (m_bombState == EXPLODED); }

	void UpdateLooseBomb( const Vector &pos );					///< we see the loose bomb
	float TimeSinceLastSawLooseBomb( void ) const;				///< how long has is been since we saw the loose bomb
	bool IsLooseBombLocationKnown( void ) const;				///< do we know where the loose bomb is

	void UpdateBomber( const Vector &pos );						///< we see the bomber
	float TimeSinceLastSawBomber( void ) const;					///< how long has is been since we saw the bomber

	void UpdatePlantedBomb( const Vector &pos );				///< we see the planted bomb
	bool IsPlantedBombLocationKnown( void ) const;				///< do we know where the bomb was planted
	void MarkBombsiteAsPlanted( int zoneIndex );				///< mark bombsite as the location of the planted bomb

	enum { UNKNOWN = -1 };
	int GetPlantedBombsite( void ) const;						///< return the zone index of the planted bombsite, or UNKNOWN
	bool IsAtPlantedBombsite( void ) const;						///< return true if we are currently in the bombsite where the bomb is planted

	int GetNextBombsiteToSearch( void );						///< return the zone index of the next bombsite to search
	bool IsBombsiteClear( int zoneIndex ) const;				///< return true if given bombsite has been cleared
	void ClearBombsite( int zoneIndex );						///< mark bombsite as clear

	const Vector *GetBombPosition( void ) const;				///< return where we think the bomb is, or NULL if we don't know
	
	// hostage rescue scenario ------------------------------------------------------------------------
	CHostage *GetNearestFreeHostage( Vector *knowPos = NULL ) const;	///< return the closest free hostage, and where we think it is (knowPos)
	const Vector *GetRandomFreeHostagePosition( void ) const;
	bool AreAllHostagesBeingRescued( void ) const;				///< return true if there are no free hostages
	bool AreAllHostagesGone( void ) const;						///< all hostages have been rescued or are dead
	void AllHostagesGone( void );								///< someone told us all the hostages are gone
	bool HaveSomeHostagesBeenTaken( void ) const				///< return true if one or more hostages have been moved by the CT's
	{
		return m_haveSomeHostagesBeenTaken;
	}
	void HostageWasTaken( void )								///< someone told us a CT is talking to a hostage
	{
		m_haveSomeHostagesBeenTaken = true;
	}

	CHostage *GetNearestVisibleFreeHostage( void ) const;

	enum ValidateStatusType
	{ 
		NO_CHANGE = 0x00,
		HOSTAGE_DIED = 0x01,
		HOSTAGE_GONE = 0x02,
		HOSTAGES_ALL_GONE = 0x04
	};
	unsigned char ValidateHostagePositions( void );				///< update our knowledge with what we currently see - returns bitflag events

private:
	CCSBot *m_owner;											///< who owns this gamestate

	bool m_isRoundOver;											///< true if round is over, but no yet reset

	// bomb defuse scenario ---------------------------------------------------------------------------
	void SetBombState( BombState state );
	BombState GetBombState( void ) const			{ return m_bombState; }

	BombState m_bombState;										///< what we think the bomb is doing

	IntervalTimer m_lastSawBomber;
	Vector m_bomberPos;

	IntervalTimer m_lastSawLooseBomb;
	Vector m_looseBombPos;

	bool m_isBombsiteClear[ CCSBotManager::MAX_ZONES ];			///< corresponds to zone indices in CCSBotManager
	int m_bombsiteSearchOrder[ CCSBotManager::MAX_ZONES ];		///< randomized order of bombsites to search
	int m_bombsiteCount;
	int m_bombsiteSearchIndex;									///< the next step in the search

	int m_plantedBombsite;										///< zone index of the bombsite where the planted bomb is
	
	bool m_isPlantedBombPosKnown;								///< if true, we know the exact location of the bomb
	Vector m_plantedBombPos;

	// hostage rescue scenario ------------------------------------------------------------------------
	struct HostageInfo
	{
		CHandle<CHostage> hostage;
		Vector knownPos;
		bool isValid;
		bool isAlive;
		bool isFree;		///< not being escorted by a CT
	}
	m_hostage[ MAX_HOSTAGES ];
	int m_hostageCount;											///< number of hostages left in map
	CountdownTimer m_validateInterval;

	CBaseEntity *GetNearestHostage( void ) const;				///< return the closest live hostage
	void InitializeHostageInfo( void );							///< initialize our knowledge of the number and location of hostages

	bool m_allHostagesRescued;
	bool m_haveSomeHostagesBeenTaken;							///< true if a hostage has been moved by a CT (and we've seen it)
};

#endif // _GAME_STATE_

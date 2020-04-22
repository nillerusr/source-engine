//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef PORTAL_GAMESTATS_H
#define PORTAL_GAMESTATS_H
#ifdef _WIN32
#pragma once
#endif

#include "gamestats.h"

#define PORTAL_GAMESTATS_VERSION 001

//#define PORTAL_GAMESTATS_VERBOSE //verbose logging of extra stats, only a good idea during internal tests, centralized here for easy on/off



//NEVER REMOVE A STRUCTURE OR CHANGE A CHUNKID. If you need to change how a chunk works, make a new structure with a new ID

struct Portal_Gamestats_LevelStats_t //most of this is only tracked in verbose mode
{
	static const unsigned short CHUNKID = 1;

	//substructures 
	struct PlayerDeaths_t
	{
		static const unsigned short CHUNKID = 1; //subchunks start over with id's
		static const unsigned short TRIMSIZE = 200; //trim logs if more than this many entries exist for a single map
		Vector ptPositionOfDeath;
		int iDamageType;
		char szAttackerClassName[32];
	};

	struct PortalPlacement_t
	{
		static const unsigned short CHUNKID = 2;
		static const unsigned short TRIMSIZE = 1000; //trim logs if more than this many entries exist for a single map
		Vector ptPlayerFiredFrom;
		Vector ptPlacementPosition;
		char iSuccessCode;
	};

	struct PlayerUse_t
	{
		static const unsigned short CHUNKID = 3;
		static const unsigned short TRIMSIZE = 500; //trim logs if more than this many entries exist for a single map
		Vector ptTraceStart;
		Vector vTraceDelta;
		char szUseEntityClassName[32]; 
	};

	struct StuckEvent_t
	{
		static const unsigned short CHUNKID = 4;
		static const unsigned short TRIMSIZE = 100; //trim logs if more than this many entries exist for a single map
		Vector ptPlayerPosition;
		QAngle qPlayerAngles;
		bool bNearPortal;
		bool bDucking;
	};

	struct JumpEvent_t
	{
		static const unsigned short CHUNKID = 5;
		static const unsigned short TRIMSIZE = 1000; //trim logs if more than this many entries exist for a single map
		Vector ptPlayerPositionAtJumpStart;
		Vector vPlayerVelocityAtJumpStart;
	};

	struct LeafTimes_t
	{
		static const unsigned short CHUNKID = 6;
		static const unsigned short TRIMSIZE = 10000; //trim logs if more than this many entries exist for a single map
		float fTimeSpentInVisLeaf;
		LeafTimes_t( void ) : fTimeSpentInVisLeaf( 0.0f ) { };
	};

	//these are created/destroyed by parent CPortalGameStats to avoid create/copy/destroy confusion
	CUtlVector<PlayerDeaths_t> *m_pDeaths;
	CUtlVector<PortalPlacement_t> *m_pPlacements;
	CUtlVector<PlayerUse_t> *m_pUseEvents;
	CUtlVector<StuckEvent_t> *m_pStuckSpots;
	CUtlVector<JumpEvent_t> *m_pJumps;
	CUtlVector<LeafTimes_t> *m_pTimeSpentInVisLeafs;

	void AppendSubChunksToBuffer( CUtlBuffer &SaveBuffer );
	void LoadSubChunksFromBuffer( CUtlBuffer &LoadBuffer, unsigned int iChunkEndPosition );
	void Clear( void );
};


class CPortal_Player;

class CPortalGameStats : CBaseGameStats
{
	typedef CBaseGameStats BaseClass;

public:
	~CPortalGameStats( void );
	CPortalGameStats( void );

	void Clear( void );

	virtual void Event_LevelInit( void );
	virtual void Event_MapChange( const char *szOldMapName, const char *szNewMapName );
	virtual void Event_PlayerKilled( CBasePlayer *pPlayer, const CTakeDamageInfo &info );
	void Event_PortalPlacement( const Vector &ptPlayerFiredFrom, const Vector &ptAttemptedPosition, char iSuccessCode );
	void Event_PlayerJump( const Vector &ptStartPosition, const Vector &vStartVelocity );
	void Event_PlayerUsed( const Vector &ptTraceStart, const Vector &vTraceDelta, CBaseEntity *pUsedEntity );
	void Event_PlayerStuck( CPortal_Player *pPlayer );

	virtual bool StatTrackingEnabledForMod( void ) { return true; }
	virtual bool UserPlayedAllTheMaps( void );

#ifdef _DEBUG
	virtual bool AutoUpload_OnShutdown( void ) { return false; } //don't upload while we're debugging
#endif

	virtual void AppendCustomDataToSaveBuffer( CUtlBuffer &SaveBuffer );
	virtual void LoadCustomDataFromBuffer( CUtlBuffer &LoadBuffer );

	Portal_Gamestats_LevelStats_t *m_pCurrentMapStats;

protected:
	CUtlDict< Portal_Gamestats_LevelStats_t, unsigned short > m_CustomMapStats;
	Portal_Gamestats_LevelStats_t *FindOrAddMapStats( const char *szMapName );
};

extern CPortalGameStats g_PortalGameStats;


void CreateLevelStatPointers( Portal_Gamestats_LevelStats_t *pFillIn );
void DestroyLevelStatPointers( Portal_Gamestats_LevelStats_t *pDestroyFrom );


#endif // PORTAL_GAMESTATS_H

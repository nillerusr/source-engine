//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: game stat gathering
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_STATS_H
#define TF_STATS_H

#ifdef _WIN32
#pragma once
#endif

#include "tf_shareddefs.h"

// NOTE: If you change these enums, change the string lists in tf_stats.cpp!
enum TFStatId_t
{
	TF_STAT_FERRY_CONTROL = 0,
	TF_STAT_RESOURCE_CHUNKS_SPAWNED,
	TF_STAT_RESOURCE_PROCESSED_CHUNKS_SPAWNED,
	TF_STAT_RESOURCE_CHUNKS_RETIRED,

	// NOTE: These should go last
	TF_STAT_FIRST_OBJECT_BUILT,
	TF_STAT_LAST_OBJECT_BUILT = TF_STAT_FIRST_OBJECT_BUILT + OBJ_LAST - 1,

	TF_STAT_COUNT,
};

enum TFTeamStatId_t
{
	// First TFCLASS_CLASS_COUNT entries are the # of players of each class.

	TF_TEAM_STAT_PLAYER_COUNT = TFCLASS_CLASS_COUNT,

	TF_TEAM_STAT_RESOURCES_COLLECTED,
	TF_TEAM_STAT_RESOURCES_HARVESTED,
	TF_TEAM_STAT_RESOURCE_CHUNKS_DROPPED,
	TF_TEAM_STAT_RESOURCE_CHUNKS_COLLECTED,

	TF_TEAM_STAT_KILL_COUNT,
	TF_TEAM_STAT_DESTROYED_OBJECT_COUNT,

	TF_TEAM_STAT_FERRY_CONTROL_TIME,

	TF_TEAM_STAT_COUNT,
};

enum TFPlayerStatId_t
{
	// Player count
	TF_PLAYER_STAT_PLAYER_COUNT = 0,
	TF_PLAYER_STAT_PLAYER_SECONDS,
	TF_PLAYER_STAT_EXISTING_SECONDS,
	TF_PLAYER_STAT_FIRST_AVERAGE_STAT,

	// Economy-based stats
	TF_PLAYER_STAT_RESOURCES_ACQUIRED = TF_PLAYER_STAT_FIRST_AVERAGE_STAT,
	TF_PLAYER_STAT_RESOURCES_ACQUIRED_FROM_CHUNKS,
	TF_PLAYER_STAT_RESOURCES_CARRIED,
 	TF_PLAYER_STAT_RESOURCES_SPENT,
	TF_PLAYER_STAT_CURRENT_OBJECT_VALUE,
	TF_PLAYER_STAT_OBJECT_COUNT,

	// Combat based stats
	TF_PLAYER_STAT_KILL_COUNT,
	TF_PLAYER_STAT_DESTROYED_OBJECT_COUNT,
	TF_PLAYER_STAT_HEALTH_GIVEN,

	// Animation-based stats
	TF_PLAYER_STAT_ANIMATION_IDLE,
	TF_PLAYER_STAT_ANIMATION_WALKING,
	TF_PLAYER_STAT_ANIMATION_RUNNING,
	TF_PLAYER_STAT_ANIMATION_CROUCHING,
	TF_PLAYER_STAT_ANIMATION_JUMPING,
	TF_PLAYER_STAT_ANIMATION_OTHER,

	TF_PLAYER_STAT_COUNT,
};


class ITFStats
{
public:
	// Clear out the stats + their history
	virtual void ResetStats() = 0;

	virtual void IncrementStat( TFStatId_t stat, int nIncrement ) = 0;
	virtual void SetStat( TFStatId_t stat, int nAmount ) = 0;

	virtual void IncrementTeamStat( int nTeam, TFTeamStatId_t stat, int nIncrement ) = 0;
	virtual void SetTeamStat( int nTeam, TFTeamStatId_t stat, int nAmount ) = 0;

	virtual void IncrementPlayerStat( CBaseEntity *pPlayer, TFPlayerStatId_t stat, int nIncrement ) = 0;
};


//-----------------------------------------------------------------------------
// Accessor method
//-----------------------------------------------------------------------------
ITFStats *TFStats();


#endif // TF_STATS_H

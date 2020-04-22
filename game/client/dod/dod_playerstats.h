//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_PLAYERSTATS_H
#define DOD_PLAYERSTATS_H
#ifdef _WIN32
#pragma once
#endif

#include "steam/steam_api.h"
#include "GameEventListener.h"
#include "dod_shareddefs.h"
#include "weapon_dodbase.h"

typedef struct
{
	dod_stat_accumulator_t stats;
	int iWeaponID;
} weapon_stat_t;

class CDODPlayerStats : public CAutoGameSystem, public CGameEventListener
{
public:
	CDODPlayerStats();
	virtual void PostInit();
	virtual void LevelShutdownPreEntity();

	void UploadStats();

	void UpdateStats( int iPlayerClass, int iTeam, dod_stat_accumulator_t *playerStats, CUtlVector<weapon_stat_t> *vecWeaponStats );

	void MsgFunc_DODPlayerStatsUpdate( bf_read &msg );

private:
	void FireGameEvent( IGameEvent *event );

	void SetNextForceUploadTime();
	float m_flTimeNextForceUpload;

	// 6 x dod_stat_accumulator_t
	dod_stat_accumulator_t m_PlayerStats[2][NUM_DOD_PLAYERCLASSES];

	// num_weapons x dod_stat_accumulator_t
	dod_stat_accumulator_t m_WeaponStats[WEAPON_MAX];
};

extern CDODPlayerStats g_DODPlayerStats;

#endif //DOD_PLAYERSTATS_H
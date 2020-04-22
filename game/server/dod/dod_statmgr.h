//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_STATMGR_H
#define DOD_STATMGR_H
#ifdef _WIN32
#pragma once
#endif

#include "GameEventListener.h"
#include <igamesystem.h>

// Structure that holds all information related to how a player related to a 
// 
typedef struct
{
	// shots taken
	int m_iNumShotsTaken;

	// shots hit
	int m_iNumShotsHit;

	// total damage given
	int m_iTotalDamageGiven;

	// average distance of hit shots
	float m_flAverageHitDistance;

	// total kills
	int m_iNumKills;

	// times we hit each hitgroup
	int m_iBodygroupsHit[7];

	// times hit by this weapon
	int m_iNumHitsTaken;

	// total damage given to us
	int m_iTotalDamageTaken;

	// times killed by this weapon
	int m_iTimesKilled;

	// times we were hit in each hitgroup
	int m_iHitInBodygroups[8];
} weaponstat_t;

// Can be used to represent a victim and how many times we killed him,
// or as an attacker, how many times we were killed by this person
typedef struct 
{
	char m_szPlayerName[MAX_PLAYER_NAME_LENGTH];	// when we add this player, store the player name
	int m_iUserID;		// player that did the killing
	int m_iKills;			// number of kills
	int m_iTotalDamage;	// amount of damage
} playerstat_t;

class CDODStatManager : public CGameEventListener, public CAutoGameSystem
{
private:
	typedef CBaseGameSystem BaseClass;

public:
	CDODStatManager();

public: // IGameEventListener Interface
	virtual void FireGameEvent( IGameEvent * event );

public: // CBaseGameSystem overrides
	virtual bool Init();	
	//virtual void Shutdown() {}
};


#endif // DOD_STATMGR_H

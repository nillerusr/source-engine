#ifndef _INCLUDED_ASW_ARENA_H
#define _INCLUDED_ASW_ARENA_H
#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"

struct CASW_ArenaAlien
{
public:
	CASW_ArenaAlien( const char *szAlienClass, int iQuantityMin, int iQuantityMax )
	{
		Q_strcpy( m_szAlienClass, szAlienClass );
		m_iQuantityMin = iQuantityMin;
		m_iQuantityMax = iQuantityMax;
	}
	char m_szAlienClass[128];
	int m_iQuantityMin;
	int m_iQuantityMax;
};

// This class will spawn varying waves of enemies at the player (requires aiarena map)

class CASW_Arena : public CAutoGameSystemPerFrame
{
public:
	CASW_Arena();
	~CASW_Arena();

	virtual bool Init();
	virtual void Shutdown();

	virtual void LevelInitPreEntity();
	virtual void FrameUpdatePostEntityThink();

	void SpawnArenaWave();
	void Event_AlienKilled( CBaseEntity *pAlien, const CTakeDamageInfo &info );

protected:
	void InitArenaAlienTypes();
	void UpdateArena();
	void RefillMarineAmmo();
	void ShuffleArenaWalls();
	void TeleportPlayersToSpawn();

private:
	CUtlVector<CASW_ArenaAlien*> m_ArenaAliens;
	CountdownTimer m_ArenaCheckTimer;
	CountdownTimer m_ArenaRestTimer;
	CountdownTimer m_ArenaShuffleWallsTimer;
	int m_iArenaWave;
	bool m_bStartedArenaMode;
};

CASW_Arena* ASWArena();

#endif // _INCLUDED_ASW_ARENA_H
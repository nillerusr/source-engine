//====== Copyright © 1996-2006, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
//=============================================================================//
#ifndef TF_GAMESTATS_H
#define TF_GAMESTATS_H
#ifdef _WIN32
#pragma once
#endif

#include "gamestats.h"
#include "tf_gamestats_shared.h"
#include "GameEventListener.h"

class CTFPlayer;

//=============================================================================
//
// TF Game Stats Class
//
class CTFGameStats : public CBaseGameStats, public CAutoGameSystemPerFrame
{
public:

	// Constructor/Destructor.
	CTFGameStats( void );
	~CTFGameStats( void );

	virtual void Clear( void );

	// Stats enable.
	virtual bool StatTrackingEnabledForMod( void );
	virtual bool ShouldTrackStandardStats( void ) { return false; } 
	virtual bool AutoUpload_OnLevelShutdown( void ) { return true; }
	virtual bool AutoSave_OnShutdown( void ) { return false; }
	virtual bool AutoUpload_OnShutdown( void ) { return false; }

	virtual bool LoadFromFile( void );

	// Buffers.
	virtual void AppendCustomDataToSaveBuffer( CUtlBuffer &SaveBuffer );
	virtual void LoadCustomDataFromBuffer( CUtlBuffer &LoadBuffer );

	virtual bool Init();

	// Events.
	virtual void Event_LevelInit( void );
	virtual void Event_LevelShutdown( float flElapsed );
	virtual void Event_PlayerKilled( CBasePlayer *pPlayer, const CTakeDamageInfo &info );
	void Event_RoundEnd( int iWinningTeam, bool bFullRound, float flRoundTime, bool bWasSuddenDeathWin );
	void Event_PlayerConnected( CBasePlayer *pPlayer );
	void Event_PlayerDisconnected( CBasePlayer *pPlayer );
	void Event_PlayerChangedClass( CTFPlayer *pPlayer );
	void Event_PlayerSpawned( CTFPlayer *pPlayer );
	void Event_PlayerForceRespawn( CTFPlayer *pPlayer );
	void Event_PlayerLeachedHealth( CTFPlayer *pPlayer, bool bDispenserHeal, float amount );
	void Event_PlayerHealedOther( CTFPlayer *pPlayer, float amount );
	void Event_AssistKill( CTFPlayer *pPlayer, CBaseEntity *pVictim );
	void Event_PlayerInvulnerable( CTFPlayer *pPlayer );
	void Event_PlayerCreatedBuilding( CTFPlayer *pPlayer, CBaseObject *pBuilding );
	void Event_PlayerDestroyedBuilding( CTFPlayer *pPlayer, CBaseObject *pBuilding );
	void Event_AssistDestroyBuilding( CTFPlayer *pPlayer, CBaseObject *pBuilding );
	void Event_Headshot( CTFPlayer *pKiller );
	void Event_Backstab( CTFPlayer *pKiller );
	void Event_PlayerUsedTeleport( CTFPlayer *pTeleportOwner, CTFPlayer *pTeleportingPlayer );
	void Event_PlayerFiredWeapon( CTFPlayer *pPlayer, bool bCritical );
	void Event_PlayerDamage( CBasePlayer *pPlayer, const CTakeDamageInfo &info, int iDamageTaken );
	void Event_PlayerKilledOther( CBasePlayer *pAttacker, CBaseEntity *pVictim, const CTakeDamageInfo &info );
	void Event_PlayerCapturedPoint( CTFPlayer *pPlayer );
	void Event_PlayerDefendedPoint( CTFPlayer *pPlayer );
	void Event_PlayerDominatedOther( CTFPlayer *pAttacker );
	void Event_PlayerRevenge( CTFPlayer *pAttacker );
	void Event_MaxSentryKills( CTFPlayer *pAttacker, int iMaxKills );
	void Event_GameEnd( void );

	virtual void FrameUpdatePostEntityThink();

	void AccumulateGameData();
	void ClearCurrentGameData();
	bool ShouldSendToClient( TFStatType_t statType );

	// Utilities.
	TF_Gamestats_LevelStats_t	*GetCurrentMap( void )			{ return m_reportedStats.m_pCurrentGame; }

	struct PlayerStats_t *		FindPlayerStats( CBasePlayer *pPlayer );
	void						ResetPlayerStats( CTFPlayer *pPlayer );
	void						ResetKillHistory( CTFPlayer *pPlayer );
	void						ResetRoundStats();
protected:	
	void						IncrementStat( CTFPlayer *pPlayer, TFStatType_t statType, int iValue );
	void						SendStatsToPlayer( CTFPlayer *pPlayer, int iMsgType );
	void						AccumulateAndResetPerLifeStats( CTFPlayer *pPlayer );
	void						TrackKillStats( CBasePlayer *pAttacker, CBasePlayer *pVictim );

protected:

public:
	TFReportedStats_t			m_reportedStats;		// Stats which are uploaded from TF server to Steam
protected:

	PlayerStats_t				m_aPlayerStats[MAX_PLAYERS+1];	// List of stats for each player for current life - reset after each death or class change
};

extern CTFGameStats CTF_GameStats;

#endif // TF_GAMESTATS_H

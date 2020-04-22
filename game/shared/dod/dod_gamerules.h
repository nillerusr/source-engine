//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The TF Game rules object
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_GAMERULES_H
#define DOD_GAMERULES_H

#ifdef _WIN32
#pragma once
#endif

#include "teamplay_gamerules.h"
#include "convar.h"
#include "dod_shareddefs.h"
#include "gamevars_shared.h"
#include "weapon_dodbase.h"
#include "dod_round_timer.h"

#ifdef CLIENT_DLL
	#include "c_baseplayer.h"
#else
	#include "player.h"
	#include "dod_player.h"
	#include "utlqueue.h"
	#include "playerclass_info_parse.h"
	#include "voice_gamemgr.h"
	#include "dod_gamestats.h"
#endif

#ifdef CLIENT_DLL
	#define CDODGameRules C_DODGameRules
	#define CDODGameRulesProxy C_DODGameRulesProxy
#else
	extern IVoiceGameMgrHelper *g_pVoiceGameMgrHelper;
	extern IUploadGameStats *gamestatsuploader;
#endif

#ifndef CLIENT_DLL

	class CSpawnPoint : public CPointEntity
	{
	public:
		bool IsDisabled() { return m_bDisabled; }
		void InputEnable( inputdata_t &inputdata ) { m_bDisabled = false; }
		void InputDisable( inputdata_t &inputdata ) { m_bDisabled = true; }

	private:
		bool m_bDisabled;
		DECLARE_DATADESC();
	};

#endif

class CDODGameRulesProxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS( CDODGameRulesProxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();
};

class CDODGameRules;

class CDODRoundStateInfo
{
public:
	DODRoundState m_iRoundState;
	const char *m_pStateName;
	
	void (CDODGameRules::*pfnEnterState)();	// Init and deinit the state.
	void (CDODGameRules::*pfnLeaveState)();
	void (CDODGameRules::*pfnThink)();	// Do a PreThink() in this state.
};

typedef enum
{
	STARTROUND_ATTACK = 0,
	STARTROUND_DEFEND,

	STARTROUND_BEACH,

	STARTROUND_ATTACK_TIMED,
	STARTROUND_DEFEND_TIMED,

	STARTROUND_FLAGS,
} startround_voice_t;
 
class CDODGamePlayRules
{
public:
	DECLARE_CLASS_NOBASE( CDODGamePlayRules );
	DECLARE_EMBEDDED_NETWORKVAR();

	DECLARE_SIMPLE_DATADESC();

	CDODGamePlayRules()
	{
		Reset();
	}

	// This virtual method is necessary to generate a vtable in all cases
	// (DECLARE_PREDICTABLE will generate a vtable also)!
	virtual ~CDODGamePlayRules() {}

	void Reset( void )
	{
		//RespawnFactor	
		m_fAlliesRespawnFactor	= 1.0f;
		m_fAxisRespawnFactor	= 1.0f;
	}
	
	//Respawn Factors
	float			m_fAlliesRespawnFactor;	//How delayed are respawning players 
	float			m_fAxisRespawnFactor;	//1.0 is normal, 2.0 is twice as long

	int				m_iAlliesStartRoundVoice;	// Which voice to play at round start
	int				m_iAxisStartRoundVoice;
};


//Mapper interface for gamerules
class CDODDetect : public CBaseEntity
{
public:
	DECLARE_CLASS( CDODDetect, CBaseEntity );

	CDODDetect();
	void Spawn( void );
	virtual bool KeyValue( const char *szKeyName, const char *szValue );

	bool IsMasteredOn( void );

	inline CDODGamePlayRules *GetGamePlay() { return &m_GamePlayRules; }
	CDODGamePlayRules m_GamePlayRules;

private:
//	string_t m_sMaster;
};

class CDODViewVectors : public CViewVectors
{
public:
	CDODViewVectors( 
		Vector vView,
		Vector vHullMin,
		Vector vHullMax,
		Vector vDuckHullMin,
		Vector vDuckHullMax,
		Vector vDuckView,
		Vector vObsHullMin,
		Vector vObsHullMax,
		Vector vDeadViewHeight,
		Vector vProneHullMin,
		Vector vProneHullMax ) :
			CViewVectors( 
				vView,
				vHullMin,
				vHullMax,
				vDuckHullMin,
				vDuckHullMax,
				vDuckView,
				vObsHullMin,
				vObsHullMax,
				vDeadViewHeight )
	{
		m_vProneHullMin = vProneHullMin;
		m_vProneHullMax = vProneHullMax;
	}

	Vector m_vProneHullMin;
	Vector m_vProneHullMax;	
};

//GAMERULES
class CDODGameRules : public CTeamplayRules
{
public:
	DECLARE_CLASS( CDODGameRules, CTeamplayRules );

	virtual bool ShouldCollide( int collisionGroup0, int collisionGroup1 );

	inline DODRoundState State_Get( void ) { return m_iRoundState; }

	int GetSubTeam( int team );

	bool IsGameUnderTimeLimit( void );
	int GetTimeLeft( void );

	int GetReinforcementTimerSeconds( int team, float flSpawnEligibleTime );

	bool IsFriendlyFireOn( void );
	bool IsInBonusRound( void );

	// Get the view vectors for this mod.
	virtual const CViewVectors* GetViewVectors() const;
	virtual const CDODViewVectors *GetDODViewVectors() const;
	virtual const unsigned char *GetEncryptionKey( void ) { return (unsigned char *)"Wl0u5B3F"; }

	bool AwaitingReadyRestart( void ) { return m_bAwaitingReadyRestart; }
	float GetRoundRestartTime( void ) { return m_flRestartRoundTime; }

	bool IsInWarmup( void ) { return m_bInWarmup; }

	bool IsBombingTeam( int team );

	virtual bool IsConnectedUserInfoChangeAllowed( CBasePlayer *pPlayer );

#ifndef CLIENT_DLL
	float GetPresentDropChance( void );	// holiday 2011, presents instead of ammo boxes
#endif

#ifdef CLIENT_DLL

	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.

	void SetRoundState( int iRoundState );
	float m_flLastRoundStateChangeTime;

#else

	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.

	CDODGameRules();
	virtual ~CDODGameRules();

	virtual void LevelShutdown( void );
	void UploadLevelStats( void );

	virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrc, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore );
	virtual void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore, bool bIgnoreWorld = false );
	void RadiusStun( const CTakeDamageInfo &info, const Vector &vecSrc, float flRadius );
	virtual void Think();
	virtual void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual void ClientDisconnected( edict_t *pClient );
	virtual float FlPlayerFallDamage( CBasePlayer *pPlayer );

	virtual const char *GetGameDescription( void )
	{
		return "Day of Defeat: Source";
	}

	void CreateStandardEntities( void );

	virtual const char *GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer );
	
	CBaseEntity *GetPlayerSpawnSpot( CBasePlayer *pPlayer );
	bool IsSpawnPointValid( CBaseEntity *pSpot, CBasePlayer *pPlayer );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );

	int DODPointsForKill( CBasePlayer *pVictim, const CTakeDamageInfo &info );

	//Round state machine
	void State_Transition( DODRoundState newState );
	void State_Enter( DODRoundState newState );	// Initialize the new state.
	void State_Leave();							// Cleanup the previous state.
	void State_Think();						// Update the current state.

	CDODRoundStateInfo *m_pCurStateInfo;	//Fn ptrs for the current state
	float m_flStateTransitionTime;			//Timer for round states
	
	// Find the state info for the specified state.
	static CDODRoundStateInfo* State_LookupInfo( DODRoundState state );

	//State Functions
	void State_Enter_INIT( void );
	void State_Think_INIT( void );

	void State_Enter_PREGAME( void );
	void State_Think_PREGAME( void );

	void State_Enter_STARTGAME( void );
	void State_Think_STARTGAME( void );

	void State_Enter_PREROUND( void );
	void State_Think_PREROUND( void );

	void State_Enter_RND_RUNNING( void );
	void State_Think_RND_RUNNING( void );

	void State_Enter_ALLIES_WIN( void );
	void State_Think_ALLIES_WIN( void );

	void State_Enter_AXIS_WIN( void );
	void State_Think_AXIS_WIN( void );

	void State_Enter_RESTART( void );
	void State_Think_RESTART( void );

	void SetInWarmup( bool bWarmup );
	void CheckWarmup( void );
	void CheckRestartRound( void );
	void CheckRespawnWaves( void );

	void InitTeams( void );

	void RoundRespawn( void );
	void CleanUpMap( void );
	void ResetScores( void );

	// Respawn everyone regardless of state - round reset
	inline void RespawnAllPlayers( void ) { RespawnPlayers( true ); }

	// Respawn only one team, players that are ready to spawn - wave reset
	inline void RespawnTeam( int iTeam ) { RespawnPlayers( false, true, iTeam ); }

	void RespawnPlayers( bool bForceRespawn, bool bTeam = false, int iTeam = TEAM_UNASSIGNED );

	void FailSafeSpawnPlayersOnTeam( int iTeam );

	bool IsPlayerClassOnTeam( int cls, int team );
	bool CanPlayerJoinClass( CDODPlayer *pPlayer, int cls );
	void ChooseRandomClass( CDODPlayer *pPlayer );
	bool ReachedClassLimit( int team, int cls );
	int CountPlayerClass( int team, int cls );
	int GetClassLimit( int team, int cls );

	int CountActivePlayers( void );	//How many players have chosen a team?

	void SetWinningTeam( int team );
	void PlayWinSong( int team );
	void PlayStartRoundVoice( void );
	void BroadcastSound( const char *sound );
	void PlaySpawnSoundToTeam( const char *sound, int team );

	int SelectDefaultTeam( void );

	void CopyGamePlayLogic( const CDODGamePlayRules otherGamePlay );


	void DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info );

	virtual bool CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );

	bool TeamFull( int team_id );
	bool TeamStacked( int iNewTeam, int iCurTeam );

	const char *GetPlayerClassName( int cls, int team );

	virtual void ClientSettingsChanged( CBasePlayer *pPlayer );

	void CheckChatForReadySignal( CDODPlayer *pPlayer, const char *chatmsg );
	bool AreAlliesReady( void ) { return m_bHeardAlliesReady; }
	bool AreAxisReady( void ) { return m_bHeardAxisReady; }

	void CreateOrJoinRespawnWave( CDODPlayer *pPlayer );
	
	virtual bool InRoundRestart( void );

	void SendTeamScoresEvent( void );

	void WriteStatsFile( const char *pszLogName );

	void AddTimerSeconds( int iSecondsToAdd );
	int GetTimerSeconds( void );

	void CapEvent( int event, int team );
	int m_iLastAlliesCapEvent;
	int m_iLastAxisCapEvent;

	// Set the time at which the map was reset to 'now'
	// and send an event with the time remaining until map change
	void ResetMapTime( void );

	virtual CBaseCombatWeapon *GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon );

	virtual bool CanEntityBeUsePushed( CBaseEntity *pEnt );

	virtual void CalcDominationAndRevenge( CDODPlayer *pAttacker, CDODPlayer *pVictim, int *piDeathFlags );

	float m_flNextFailSafeWaveCheckTime;

	CUtlVector<EHANDLE> *GetSpawnPointListForTeam( int iTeam );

	virtual void GetTaggedConVarList( KeyValues *pCvarTagList );

protected:	
	virtual void GoToIntermission( void );
	virtual bool UseSuicidePenalty() { return false; }

	void CheckPlayerPositions( void );

private:
	bool CheckTimeLimit( void );
	bool CheckWinLimit( void );

	void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, bool bIgnoreWorld );
	float GetExplosionDamageAdjustment(Vector & vecSrc, Vector & vecEnd, CBaseEntity *pTarget, CBaseEntity *pEntityToIgnore); // returns multiplier between 0.0 and 1.0 that is the percentage of any damage done from vecSrc to vecEnd that actually makes it.
	float GetAmountOfEntityVisible(Vector & src, CBaseEntity *pTarget, CBaseEntity *pEntityToIgnore); // returns a value from 0 to 1 that is the percentage of player visible from src.

	void CheckLevelInitialized( void );
	bool m_bLevelInitialized;

	int	m_iSpawnPointCount_Allies;	//number of allies spawns on the map
	int	m_iSpawnPointCount_Axis;	//number of axis spawns on the map

	#define MAX_PLAYERCLASSES_PER_TEAM	16

	PLAYERCLASS_FILE_INFO_HANDLE m_hPlayerClassInfoHandles[2][MAX_PLAYERCLASSES_PER_TEAM];

	// restart and warmup variables
	float m_flWarmupTimeEnds;
	float m_flNextPeriodicThink;

	Vector2D	m_vecPlayerPositions[MAX_PLAYERS];
	

	//BELOW HERE NEED TO BE HOOKED UP


	int				m_iNumAlliesAlive;	//the number of players alive on each team
	int				m_iNumAxisAlive;
	int				m_iNumAlliesOnTeam;	//the number of players on each team
	int				m_iNumAxisOnTeam;

	bool			m_bClanMatch;
	bool			m_bClanMatchActive;

	float			GetMaxWaveTime( int iTeam );
	float			GetWaveTime( int iTeam );
	void 			AddWaveTime( int team, float flTime );
	void 			PopWaveTime( int team );

	void DetectGameRules( void );

	bool m_bHeardAlliesReady;
	bool m_bHeardAxisReady;

	bool m_bUsingTimer;
	int m_iTimerWinTeam;
	CHandle< CDODRoundTimer >	m_pRoundTimer;

	bool m_bPlayTimerWarning_1Minute;
	bool m_bPlayTimerWarning_2Minute;

	bool m_bInitialSpawn;	// first time activating? longer wait time for people to join

	bool m_bChangeLevelOnRoundEnd;

#endif	//CLIENT_DLL

	CNetworkVarEmbedded( CDODGamePlayRules, m_GamePlayRules );

	CNetworkVar( DODRoundState, m_iRoundState );

	#define DOD_RESPAWN_QUEUE_SIZE	10

	CNetworkArray( float, m_AlliesRespawnQueue, DOD_RESPAWN_QUEUE_SIZE );
	CNetworkArray( float, m_AxisRespawnQueue, DOD_RESPAWN_QUEUE_SIZE );

	CNetworkVar( int, m_iAlliesRespawnHead );
	CNetworkVar( int, m_iAlliesRespawnTail );

	CNetworkVar( int, m_iAxisRespawnHead );
	CNetworkVar( int, m_iAxisRespawnTail );

	int m_iNumAlliesRespawnWaves;
	int m_iNumAxisRespawnWaves;

	CNetworkVar( bool, m_bInWarmup );
	CNetworkVar( bool, m_bAwaitingReadyRestart );
	CNetworkVar( float, m_flRestartRoundTime );
	CNetworkVar( float, m_flMapResetTime );	// time that the map was reset

	CNetworkVar( bool, m_bAlliesAreBombing );
	CNetworkVar( bool, m_bAxisAreBombing );

#ifndef CLIENT_DLL
public:
	// Stats
	void Stats_PlayerKill( int team, int cls );
	void Stats_PlayerCap( int team, int cls );
	void Stats_PlayerDefended( int team, int cls );
	void Stats_WeaponFired( int weaponID );
	void Stats_WeaponHit( int weaponID, float flDist );
	int Stats_WeaponDistanceToBucket( int weaponID, float flDist );

	float m_flSecondsPlayedPerClass_Allies[7];
	float m_flSecondsPlayedPerClass_Axis[7];

	int m_iStatsKillsPerClass_Allies[6];
	int m_iStatsKillsPerClass_Axis[6];

	int m_iStatsSpawnsPerClass_Allies[6];
	int m_iStatsSpawnsPerClass_Axis[6];

	int m_iStatsCapsPerClass_Allies[6];
	int m_iStatsCapsPerClass_Axis[6];

	int m_iStatsDefensesPerClass_Allies[6];
	int m_iStatsDefensesPerClass_Axis[6];

	int m_iWeaponShotsFired[WEAPON_MAX];
	int m_iWeaponShotsHit[WEAPON_MAX];
	int m_iWeaponDistanceBuckets[WEAPON_MAX][DOD_NUM_WEAPON_DISTANCE_BUCKETS];	// distances of buckets are defined per-weapon

	// List of spawn points
	CUtlVector<EHANDLE> m_AlliesSpawnPoints;
	CUtlVector<EHANDLE> m_AxisSpawnPoints;

	bool m_bWinterHolidayActive;

#endif // ndef CLIENTDLL
};

//-----------------------------------------------------------------------------
// Gets us at the team fortress game rules
//-----------------------------------------------------------------------------

inline CDODGameRules* DODGameRules()
{
	return static_cast<CDODGameRules*>(g_pGameRules);
}

#ifdef CLIENT_DLL

#else	
	bool EntityPlacementTest( CBaseEntity *pMainEnt, const Vector &vOrigin, Vector &outPos, bool bDropToGround );
#endif //CLIENT_DLL


#endif // DOD_GAMERULES_H

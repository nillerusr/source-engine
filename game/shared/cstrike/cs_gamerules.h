//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The TF Game rules object
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef CS_GAMERULES_H
#define CS_GAMERULES_H

#ifdef _WIN32
#pragma once
#endif


#include "teamplay_gamerules.h"
#include "convar.h"
#include "cs_shareddefs.h"
#include "gamevars_shared.h"

#ifdef CLIENT_DLL
	#include "c_cs_player.h"
	#include "networkstringtable_clientdll.h"
#else
	#include "cs_player.h"
	#include "funfactmgr_cs.h"
#endif

#include "cs_urlretrieveprices.h"

//extern ConVar mp_dynamicpricing;

#define CS_GAMERULES_BLACKMARKET_TABLE_NAME "BlackMarketTable"

#define	WINNER_NONE		0
#define WINNER_DRAW		1
#define WINNER_TER		TEAM_TERRORIST
#define WINNER_CT		TEAM_CT

//=============================================================================
// HPE_BEGIN:
// [tj] Forward declaration so we can track bot suicides in the game rules.
//=============================================================================

class CCSBot;

//=============================================================================
// HPE_END
//=============================================================================

extern ConVar mp_startmoney;
extern ConVar mp_tkpunish;
extern ConVar mp_c4timer;
extern ConVar mp_buytime;
extern ConVar mp_freezetime;
extern ConVar mp_playerid;

#ifndef CLIENT_DLL
	extern ConVar mp_autoteambalance;
#endif // !CLIENT_DLL


#ifdef CLIENT_DLL
	#define CCSGameRules C_CSGameRules
	#define CCSGameRulesProxy C_CSGameRulesProxy
#endif

#ifndef CLIENT_DLL
	struct playerscore_t
	{
		int iPlayerIndex;
		int iScore;
	};
#endif


class CCSGameRulesProxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS( CCSGameRulesProxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();
};


class CCSGameRules : public CTeamplayRules
{
public:
	DECLARE_CLASS( CCSGameRules, CTeamplayRules );

	// Stuff that is shared between client and server.
	bool IsFreezePeriod();

	virtual bool ShouldCollide( int collisionGroup0, int collisionGroup1 );

	float GetMapRemainingTime();	// time till end of map, -1 if timelimit is disabled
	float GetMapElapsedTime();	// How much time has elapsed since the map started.
	float GetRoundRemainingTime();	// time till end of round
	float GetRoundStartTime();		// When this round started.
	float GetRoundElapsedTime();	// How much time has elapsed since the round started.
	float GetBuyTimeLength() const;
	int GetRoundLength() const { return m_iRoundTime; }
	int   SelectDefaultTeam( bool ignoreBots = false );
	int   GetHumanTeam();			// TEAM_UNASSIGNED if no restrictions

	bool IsVIPMap() const;
	bool IsBombDefuseMap() const;
	bool IsHostageRescueMap() const;
	bool IsIntermission() const;
	bool IsLogoMap() const;
	bool IsSpawnPointValid( CBaseEntity *pSpot, CBasePlayer *pPlayer );

	bool IsBuyTimeElapsed();

	virtual int	DefaultFOV();

	// Get the view vectors for this mod.
	virtual const CViewVectors* GetViewVectors() const;

	void UploadGameStats( void );
	int  GetStartMoney( void );

	virtual bool IsConnectedUserInfoChangeAllowed( CBasePlayer *pPlayer );

private:

	float GetExplosionDamageAdjustment(Vector & vecSrc, Vector & vecEnd, CBaseEntity *pEntityToIgnore); // returns multiplier between 0.0 and 1.0 that is the percentage of any damage done from vecSrc to vecEnd that actually makes it.
	float GetAmountOfEntityVisible(Vector & src, CBaseEntity *player); // returns a value from 0 to 1 that is the percentage of player visible from src.

	CNetworkVar( bool, m_bFreezePeriod );	 // TRUE at beginning of round, set to FALSE when the period expires
	CNetworkVar( int, m_iRoundTime );		 // (From mp_roundtime) - How many seconds long this round is.
	CNetworkVar( float, m_fRoundStartTime ); // time round has started
	CNetworkVar( float, m_flGameStartTime );
	CNetworkVar( int, m_iHostagesRemaining );
	CNetworkVar( bool, m_bMapHasBombTarget );
	CNetworkVar( bool, m_bMapHasRescueZone );
	CNetworkVar( bool, m_bLogoMap );		 // If there's an info_player_logo entity, then it's a logo map.
	CNetworkVar( bool, m_bBlackMarket );

	bool		m_bDontUploadStats;

public:

	bool IsBlackMarket( void ) { return m_bBlackMarket; }
	int GetNumHostagesRemaining( void ) { return m_iHostagesRemaining; }

	virtual CBaseCombatWeapon *GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon );

	virtual const unsigned char *GetEncryptionKey( void ) { return (unsigned char *)"d7NSuLq2"; } // both the client and server need this key

#ifdef CLIENT_DLL

	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.
	CCSGameRules();

#else

	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.

	CCSGameRules();
	virtual ~CCSGameRules();

	void DumpTimers( void ) const;	// debugging to help track down a stuck server (rare?)

	CBaseEntity *GetPlayerSpawnSpot( CBasePlayer *pPlayer );

	static void EndRound();

	virtual void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual void Think();

	// Called at the end of GameFrame (i.e. after all game logic has run this frame)
	virtual void EndGameFrame( void );

	// Called when game rules are destroyed by CWorld
	virtual void LevelShutdown( void );

	virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
			void ShowSpawnPoints();

	virtual void ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues );

	//=============================================================================
	// HPE_BEGIN:
	// [menglish] Set up anything for all players that changes based on new players spawning mid-game
	//				Find and return fun fact data
	// [pfreese] Tracking of "pistol" round
	//=============================================================================
	virtual void SpawningLatePlayer(CCSPlayer* pLatePlayer);

	bool IsPistolRound();

	void HostageKilled() { m_hostageWasKilled = true; }
	void HostageInjured() { m_hostageWasInjured = true; }

	bool WasHostageKilled() { return m_hostageWasKilled; }
	bool WasHostageInjured() { return m_hostageWasInjured; }

	//=============================================================================
	// HPE_END
	//=============================================================================

    //=============================================================================
    // HPE_BEGIN:
    // [tj] So game rules can react to damage taken
    //=============================================================================

    void PlayerTookDamage(CCSPlayer* player, const CTakeDamageInfo &damageInfo);

    //=============================================================================
    // HPE_END
    //=============================================================================


	virtual bool PlayTextureSounds( void ) { return true; }
	// Let the game rules specify if fall death should fade screen to black
	virtual bool  FlPlayerFallDeathDoesScreenFade( CBasePlayer *pl ) { return FALSE; }

	virtual void  RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore );
	void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, bool bIgnoreWorld );

	virtual void UpdateClientData( CBasePlayer *pl );

	// Death notices
	virtual void		DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info );

	virtual void			InitDefaultAIRelationships( void );

	virtual const char *GetGameDescription( void ) { return "Counter-Strike: Source"; }  // this is the game name that gets seen in the server browser
	virtual const char *AIClassText(int classType);

	virtual bool FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );

	virtual const char *SetDefaultPlayerTeam( CBasePlayer *pPlayer );

	// Called before entities are created
	virtual void LevelInitPreEntity();

	// Called after the map has finished loading.
	virtual void LevelInitPostEntity();

	virtual float FlPlayerFallDamage( CBasePlayer *pPlayer );

	virtual void ClientDisconnected( edict_t *pClient );

	// Recreate all the map entities from the map data (preserving their indices),
	// then remove everything else except the players.
	// Also get rid of all world decals.
	void CleanUpMap();

	void CheckFreezePeriodExpired();
	void CheckRoundTimeExpired();

	// check if the scenario has been won/lost
	// return true if the scenario is over, false if the scenario is still in progress
	bool CheckWinConditions( void );

	void TerminateRound( float tmDelay, int reason );

	//=============================================================================
	// HPE_BEGIN:
	// [tj] A place to check achievements that occur at the end of the round
	//=============================================================================
	void ProcessEndOfRoundAchievements(int iWinnerTeam, int iReason);
	//=============================================================================
	// HPE_END
	//=============================================================================

	void RestartRound( void );
	void BalanceTeams( void );
	void MoveHumansToHumanTeam( void );
	bool TeamFull( int team_id );
	bool TeamStacked( int newTeam_id, int curTeam_id  );
	bool FPlayerCanRespawn( CBasePlayer *pPlayer );
	void UpdateTeamScores();
	void CheckMapConditions();
	void MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound(int team);

	// Check various conditions to end the map.
	bool CheckGameOver();
	bool CheckMaxRounds();
	bool CheckWinLimit();
	bool CheckFragLimit();

	void CheckLevelInitialized();
	void CheckRestartRound();


	// Checks if it still needs players to start a round, or if it has enough players to start rounds.
	// Starts a round and returns true if there are enough players.
	bool NeededPlayersCheck( bool &bNeededPlayers );

	// Setup counts for m_iNumTerrorist, m_iNumCT, m_iNumSpawnableTerrorist, m_iNumSpawnableCT, etc.
	void InitializePlayerCounts(
		int &NumAliveTerrorist,
		int &NumAliveCT,
		int &NumDeadTerrorist,
		int &NumDeadCT
		);

	// Check to see if the round is over for the various game types. Terminates the round
	// and returns true if the round should end.
	bool PrisonRoundEndCheck();
	bool BombRoundEndCheck( bool bNeededPlayers );
	bool HostageRescueRoundEndCheck( bool bNeededPlayers );

	// Check to see if the teams exterminated each other. Ends the round and returns true if so.
	bool TeamExterminationCheck(
		int NumAliveTerrorist,
		int NumAliveCT,
		int NumDeadTerrorist,
		int NumDeadCT,
		bool bNeededPlayers
		);

	void ReadMultiplayCvars();
	void SwapAllPlayers();

	void BroadcastSound( const char *sound, int team = -1 );


	// VIP FUNCTIONS
	bool VIPRoundEndCheck( bool bNeededPlayers );
	void PickNextVIP();


	// BOMB MAP FUNCTIONS
	void GiveC4();
	bool IsThereABomber();
	bool IsThereABomb();

	// HOSTAGE MAP FUNCTIONS
	void HostageTouched();


	// Sets up g_pPlayerResource.
	virtual void CreateStandardEntities();
	virtual const char *GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer );
	virtual const char *GetChatLocation( bool bTeamOnly, CBasePlayer *pPlayer );
	virtual const char *GetChatFormat( bool bTeamOnly, CBasePlayer *pPlayer );
	void ClientSettingsChanged( CBasePlayer *pPlayer );

	bool IsCareer( void ) const		{ return false; }		// returns true if this is a CZ "career" game

	virtual bool FAllowNPCs( void );

protected:
	virtual void GoToIntermission( void );

public:

	bool IsFriendlyFireOn();

	virtual void	SetAllowWeaponSwitch( bool allow );
	virtual bool	GetAllowWeaponSwitch( void );

	// VARIABLES FOR ALL TYPES OF MAPS
	bool m_bLevelInitialized;
	int m_iRoundWinStatus;		// 1 == CT's won last round, 2 == Terrorists did, 3 == Draw, no winner
	int m_iTotalRoundsPlayed;
	int m_iUnBalancedRounds;	// keeps track of the # of consecutive rounds that have gone by where one team outnumbers the other team by more than 2

	// GAME TIMES
	int m_iFreezeTime;		// (From mp_freezetime) - How many seconds long the intro round (when players are frozen) is.
	float m_flRestartRoundTime;	// the global time when the round is supposed to end, if this is not 0

	int m_iNumTerrorist;		// The number of terrorists on the team (this is generated at the end of a round)
	int m_iNumCT;				// The number of CTs on the team (this is generated at the end of a round)
	int m_iNumSpawnableTerrorist;
	int m_iNumSpawnableCT;

	bool m_bFirstConnected;
	bool m_bCompleteReset;		// Set to TRUE to have the scores reset next time round restarts

	int m_iAccountTerrorist;
	int m_iAccountCT;

	short m_iNumCTWins;
	short m_iNumTerroristWins;

	int m_iNumConsecutiveCTLoses;		//SupraFiend: the number of rounds the CTs have lost in a row.
	int m_iNumConsecutiveTerroristLoses;//SupraFiend: the number of rounds the Terrorists have lost in a row.

	int m_iSpawnPointCount_Terrorist;		// Number of Terrorist spawn points
	int m_iSpawnPointCount_CT;				// Number of CT spawn points

	bool m_bTCantBuy;			// Who can and can't buy.
	bool m_bCTCantBuy;
	bool m_bMapHasBuyZone;

	int m_iLoserBonus;			// SupraFiend: the amount of money the losing team gets. This scales up as they lose more rounds in a row
	float m_tmNextPeriodicThink;


	// HOSTAGE RESCUE VARIABLES
	int		m_iHostagesRescued;
	int		m_iHostagesTouched;
	float	m_flNextHostageAnnouncement;

    //=============================================================================
    // HPE_BEGIN
    //=============================================================================

    // [tj] Accessor for weapons donation ability
    bool GetCanDonateWeapon() { return m_bCanDonateWeapons; }

    // [tj] flawless and lossless round related flags
    bool m_bNoTerroristsKilled;
    bool m_bNoCTsKilled;
    bool m_bNoTerroristsDamaged;
    bool m_bNoCTsDamaged;

    // [tj] Find out if dropped weapons count as donations
    bool m_bCanDonateWeapons;

	// [tj] Keep track of first kill
	CHandle<CCSPlayer> m_pFirstKill;
	float m_firstKillTime;

	// [menglish] Keep track of first blood
	CHandle<CCSPlayer> m_pFirstBlood;
	float m_firstBloodTime;


    // [dwenger] Rescue-related achievement values
    CHandle<CCSPlayer> m_pLastRescuer;
    int     m_iNumRescuers;

	bool m_hostageWasInjured;
	bool m_hostageWasKilled;

	// [menglish] Fun Fact Manager
	CCSFunFactMgr *m_pFunFactManager;

	// [tj] To avoid rewriting the same piece of code, we can get all the information
	//		we want from one call that fills in an array of structures.
	struct TeamPlayerCounts
	{
		int totalPlayers;
		int totalAlivePlayers;
		int totalDeadPlayers; //sum of killedPlayers + suicidedPlayers + unenteredPlayers
		int killedPlayers;
		int suicidedPlayers;
		int unenteredPlayers;
	};

	void GetPlayerCounts(TeamPlayerCounts teamCounts[TEAM_MAXCOUNT]);

    //=============================================================================
    // HPE_END
    //=============================================================================


	// PRISON ESCAPE VARIABLES
	int		m_iHaveEscaped;
	bool	m_bMapHasEscapeZone;
	int		m_iNumEscapers;
	int		m_iNumEscapeRounds;		// keeps track of the # of consecutive rounds of escape played.. Teams will be swapped after 8 rounds


	// VIP VARIABLES
	int		m_iMapHasVIPSafetyZone;	// 0 = uninitialized;   1 = has VIP safety zone;   2 = DOES not have VIP safetyzone
	CHandle<CCSPlayer> m_pVIP;
	int		m_iConsecutiveVIP;


	// BOMB MAP VARIABLES
	bool	m_bTargetBombed;	// whether or not the bomb has been bombed
	bool	m_bBombDefused;	// whether or not the bomb has been defused
	bool	m_bMapHasBombZone;
	bool	m_bBombDropped;
	bool	m_bBombPlanted;
	EHANDLE m_pLastBombGuy;

private:

	// Don't allow switching weapons while gaining new technologies
	bool			m_bAllowWeaponSwitch;

public:



	void AddPricesToTable( weeklyprice_t prices );
	virtual void CreateCustomNetworkStringTables( void );

#endif


#ifdef GAME_DLL
public:
	virtual void	GetTaggedConVarList( KeyValues *pCvarTagList );
#endif

public:
	const weeklyprice_t *GetBlackMarketPriceList( void );

	int GetBlackMarketPriceForWeapon( int iWeaponID );
	int GetBlackMarketPreviousPriceForWeapon( int iWeaponID );

	void SetBlackMarketPrices( bool bSetDefaults );

	// Black market
	INetworkStringTable *m_StringTableBlackMarket;
	const weeklyprice_t *m_pPrices;
};

//-----------------------------------------------------------------------------
// Gets us at the team fortress game rules
//-----------------------------------------------------------------------------

inline CCSGameRules* CSGameRules()
{
	return static_cast<CCSGameRules*>(g_pGameRules);
}

#define IGNORE_SPECTATORS true
int UTIL_HumansInGame( bool ignoreSpectators = false );



//-----------------------------------------------------------------------------
// Purpose: Useful utility functions
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL

#else

	class CTFTeam;
	CTFTeam *GetOpposingTeam( CTeam *pTeam );
	bool EntityPlacementTest( CBaseEntity *pMainEnt, const Vector &vOrigin, Vector &outPos, bool bDropToGround );

#endif

#endif // TF_GAMERULES_H

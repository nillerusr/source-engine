//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The CS game stats header
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_GAMESTATS_H
#define CS_GAMESTATS_H
#ifdef _WIN32
#pragma once
#endif

#include "cs_blackmarket.h"
#include "gamestats.h"
#include "cs_gamestats_shared.h"
#include "GameEventListener.h"
#include "steamworks_gamestats.h"
#include "weapon_csbase.h"

// forward declares
class CBreakableProp;

#define CS_STATS_BLOB_VERSION 3

const float cDisseminationTimeHigh = 0.25f;		// Time interval for high priority stats sent to the player
const float cDisseminationTimeLow = 2.5f;	// Time interval for medium priority stats sent to the player

int GetCSLevelIndex( const char *pLevelName );

typedef struct
{
	char szGameName[8];
	byte iVersion;
	char szMapName[32];
	char ipAddr[4];
	short port;
	int serverid;
} gamestats_header_t;

typedef struct
{
	gamestats_header_t	header;
	short	iMinutesPlayed;

	short	iTerroristVictories[CS_NUM_LEVELS];
	short	iCounterTVictories[CS_NUM_LEVELS];
	short	iBlackMarketPurchases[WEAPON_MAX];

	short	iAutoBuyPurchases;
	short	iReBuyPurchases;
	short	iAutoBuyM4A1Purchases;
	short	iAutoBuyAK47Purchases;
	short	iAutoBuyFamasPurchases;
	short	iAutoBuyGalilPurchases;
	short	iAutoBuyVestHelmPurchases;
	short	iAutoBuyVestPurchases;

} cs_gamestats_t;

extern short	g_iWeaponPurchases[WEAPON_MAX];
extern float	g_flGameStatsUpdateTime;
extern short	g_iTerroristVictories[CS_NUM_LEVELS];
extern short	g_iCounterTVictories[CS_NUM_LEVELS];
extern short	g_iWeaponPurchases[WEAPON_MAX];

extern short	g_iAutoBuyPurchases;
extern short	g_iReBuyPurchases;
extern short	g_iAutoBuyM4A1Purchases;
extern short	g_iAutoBuyAK47Purchases;
extern short	g_iAutoBuyFamasPurchases;
extern short	g_iAutoBuyGalilPurchases;
extern short	g_iAutoBuyVestHelmPurchases;
extern short	g_iAutoBuyVestPurchases;


struct sHappyCamperSnipePosition
{
	sHappyCamperSnipePosition( int userid, Vector pos ) : m_iUserID(userid), m_vPos(pos) {}

	int		m_iUserID;
	Vector	m_vPos;
};

struct SMarketPurchases : public BaseStatData 
{
	SMarketPurchases( uint64 ulPlayerID, int iPrice, const char *pName ) : m_nPlayerID(ulPlayerID), ItemCost(iPrice)
	{
		if ( pName )
		{
			Q_strncpy( ItemID, pName, ARRAYSIZE(ItemID) );
		}
		else
		{
			Q_strncpy( ItemID, "unknown", ARRAYSIZE(ItemID) );
		}
	}
	uint64	m_nPlayerID;
	int		ItemCost;
	char	ItemID[64];

	BEGIN_STAT_TABLE( "CSSMarketPurchase" )
		REGISTER_STAT( m_nPlayerID )
		REGISTER_STAT( ItemCost )
		REGISTER_STAT_STRING( ItemID )
	END_STAT_TABLE()
};
typedef CUtlVector< SMarketPurchases* > VectorMarketPurchaseData;

struct WeaponStats
{
	int shots;
	int hits;
	int kills;
	int damage;
};

struct SCSSWeaponData : public BaseStatData 
{
	SCSSWeaponData( const char *pWeaponName, const WeaponStats &wpnData )
	{
		if ( pWeaponName )
		{
			Q_strncpy( WeaponID, pWeaponName, ARRAYSIZE(WeaponID) );
		}
		else
		{
			Q_strncpy( WeaponID, "unknown", ARRAYSIZE(WeaponID) );
		}

		Shots = wpnData.shots;
		Hits = wpnData.hits;
		Kills = wpnData.kills;
		Damage = wpnData.damage;
	}
	
	char	WeaponID[64];
	int		Shots;
	int		Hits;
	int		Kills;
	int		Damage;

	BEGIN_STAT_TABLE( "CSSWeaponData" )
		REGISTER_STAT_STRING( WeaponID )
		REGISTER_STAT( Shots )
		REGISTER_STAT( Hits )
		REGISTER_STAT( Kills )
		REGISTER_STAT( Damage )
	END_STAT_TABLE()
};
typedef CUtlVector< SCSSWeaponData* > CSSWeaponData;

struct SCSSDeathData : public BaseStatData
{
	SCSSDeathData( CBasePlayer *pVictim, const CTakeDamageInfo &info )
	{
		m_bUseGlobalData = false;

		m_DeathPos = info.GetDamagePosition();
		m_iVictimTeam = pVictim->GetTeamNumber();
		
		CCSPlayer *pCSPlayer = ToCSPlayer( info.GetAttacker() );
		if ( pCSPlayer )
		{
			m_iKillerTeam = pCSPlayer->GetTeamNumber();
		}
		else
		{
			m_iKillerTeam = -1;
		}

		const char *pszWeaponName = info.GetInflictor() ? info.GetInflictor()->GetClassname() : "unknown";
		
		if ( pszWeaponName )
		{
			if ( V_strcmp(pszWeaponName, "player") == 0 )
			{
				// get the player's weapon
				if ( pCSPlayer && pCSPlayer->GetActiveCSWeapon() )
				{
					pszWeaponName = pCSPlayer->GetActiveCSWeapon()->GetClassname();
				}
			}
		}


		m_uiDeathParam = WEAPON_NONE;
		if ( (m_uiDeathParam = AliasToWeaponID( pszWeaponName )) == WEAPON_NONE )
		{
			m_uiDeathParam = AliasToWeaponID( pszWeaponName );
		}

		const char *pszMapName = gpGlobals->mapname.ToCStr() ? gpGlobals->mapname.ToCStr() : "unknown";
		Q_strncpy( m_szMapName, pszMapName, ARRAYSIZE(m_szMapName) );
	}
	Vector	m_DeathPos;
	int		m_iVictimTeam;
	int		m_iKillerTeam;
	int		m_iDamageType;
	uint64	m_uiDeathParam;
	char	m_szMapName[64];
	
	BEGIN_STAT_TABLE( "Deaths" )
		REGISTER_STAT_NAMED( m_DeathPos.x, "XCoord" )
		REGISTER_STAT_NAMED( m_DeathPos.y, "YCoord" )
		REGISTER_STAT_NAMED( m_DeathPos.z, "ZCoord" )
		REGISTER_STAT_NAMED( m_iVictimTeam, "Team" )
		REGISTER_STAT_NAMED( m_iKillerTeam, "DeathCause" )
		REGISTER_STAT_NAMED( m_uiDeathParam, "DeathParam" )
		REGISTER_STAT_NAMED( m_szMapName, "DeathMap" )
	END_STAT_TABLE()
};
typedef CUtlVector< SCSSDeathData* > CSSDeathData;

//=============================================================================
//
// CS Game Stats Class
//
class CCSGameStats : public CBaseGameStats, public CGameEventListener, public CAutoGameSystemPerFrame, public IGameStatTracker
{
public:

	// Constructor/Destructor.
	CCSGameStats( void );
	~CCSGameStats( void );

	virtual void Clear( void );
	virtual bool Init();
	virtual void PreClientUpdate();
	virtual void PostInit();
	virtual void LevelShutdownPreClearSteamAPIContext();

	void UploadRoundStats( void );
	// Overridden events
	virtual void Event_LevelInit( void );
	virtual void Event_LevelShutdown( float flElapsed );
	virtual void Event_ShotFired( CBasePlayer *pPlayer, CBaseCombatWeapon* pWeapon );
	virtual void Event_ShotHit( CBasePlayer *pPlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerKilled( CBasePlayer *pPlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerKilled_PreWeaponDrop( CBasePlayer *pPlayer, const CTakeDamageInfo &info );
	void UpdatePlayerRoundStats(int winner);
	virtual void Event_PlayerConnected( CBasePlayer *pPlayer );
	virtual void Event_PlayerDisconnected( CBasePlayer *pPlayer );
	virtual void Event_WindowShattered( CBasePlayer *pPlayer );
	virtual void Event_PlayerKilledOther( CBasePlayer *pAttacker, CBaseEntity *pVictim, const CTakeDamageInfo &info );
			

	// CSS specific events
    void Event_BombPlanted( CCSPlayer *pPlayer );
    void Event_BombDefused( CCSPlayer *pPlayer );
	void Event_PlayerDamage( CBasePlayer *pBasePlayer, const CTakeDamageInfo &info );
	void Event_BombExploded( CCSPlayer *pPlayer );
	void Event_MoneyEarned( CCSPlayer *pPlayer, int moneyEarned );
	void Event_MoneySpent( CCSPlayer *pPlayer, int moneySpent, const char *pItemName );
    void Event_HostageRescued( CCSPlayer *pPlayer );
    void Event_PlayerSprayedDecal( CCSPlayer*pPlayer );
	void Event_AllHostagesRescued();
	void Event_BreakProp( CCSPlayer *pPlayer, CBreakableProp *pProp );
    void Event_PlayerDonatedWeapon (CCSPlayer* pPlayer);
    void Event_PlayerDominatedOther( CCSPlayer* pAttacker, CCSPlayer* pVictim);
    void Event_PlayerRevenge( CCSPlayer* pAttacker );
    void Event_PlayerAvengedTeammate( CCSPlayer* pAttacker, CCSPlayer* pAvengedPlayer );
	void Event_MVPEarned( CCSPlayer* pPlayer );	
	void Event_KnifeUse( CCSPlayer* pPlayer, bool bStab, int iDamage );

	virtual void FireGameEvent( IGameEvent *event );

	void DumpMatchWeaponMetrics();

	const PlayerStats_t&		FindPlayerStats( CBasePlayer *pPlayer ) const;
	void						ResetPlayerStats( CBasePlayer *pPlayer );
	void						ResetKillHistory( CBasePlayer *pPlayer );
	void						ResetRoundStats();
	void						ResetPlayerClassMatchStats();

	const StatsCollection_t&	GetTeamStats( int iTeamIndex ) const;
	void						ResetAllTeamStats();
    void						ResetAllStats();
	void						ResetWeaponStats();
 	void						IncrementTeamStat( int iTeamIndex, int iStatIndex, int iAmount );
	void                        CalcDominationAndRevenge( CCSPlayer *pAttacker, CCSPlayer *pVictim, int *piDeathFlags );
    void                        CalculateOverkill( CCSPlayer* pAttacker, CCSPlayer* pVictim );
	void						PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );

	void						IncrementStat( CCSPlayer* pPlayer, CSStatType_t statId, int iValue, bool bPlayerOnly = false );
	// Steamworks Gamestats 
	virtual void SubmitGameStats( KeyValues *pKV );

	virtual StatContainerList_t* GetStatContainerList( void )
	{
		return s_StatLists;
	}

protected:	
	void						SetStat( CCSPlayer *pPlayer, CSStatType_t statId, int iValue );
    void						TrackKillStats( CCSPlayer *pAttacker, CCSPlayer *pVictim );
    void                        ComputeRollingStatAverages();
	void                        ComputeDirectStatAverages();
    void                        SendRollingStatsAveragesToAllPlayers();
	void                        SendDirectStatsAveragesToAllPlayers();
	void						SendStatsToPlayer( CCSPlayer * pPlayer, int iMinStatPriority );

private:
	PlayerStats_t				m_aPlayerStats[MAX_PLAYERS+1];	// List of stats for each player for current life - reset after each death    
	StatsCollection_t			m_aTeamStats[TEAM_MAXCOUNT - FIRST_GAME_TEAM];

    RoundStatsRollingAverage_t  m_rollingCTStatAverages;	
    RoundStatsRollingAverage_t  m_rollingTStatAverages;	
    RoundStatsRollingAverage_t	m_rollingPlayerStatAverages;	

	RoundStatsDirectAverage_t	m_directCTStatAverages;
	RoundStatsDirectAverage_t	m_directTStatAverages;
	RoundStatsDirectAverage_t	m_directPlayerStatAverages;

	float						m_fDisseminationTimerLow;		// how long since last medium priority stat update
	float						m_fDisseminationTimerHigh;		// how long since last high priority stat update

	int							m_numberOfRoundsForDirectAverages;
	int							m_numberOfTerroristEntriesForDirectAverages;
	int							m_numberOfCounterTerroristEntriesForDirectAverages;

	CUtlDict< CSStatType_t, short >	m_PropStatTable;

	CUtlLinkedList<sHappyCamperSnipePosition, int>		m_PlayerSnipedPosition;
	WeaponStats					m_weaponStats[WEAPON_MAX][2];
	
	// Steamworks Gamestats
	VectorMarketPurchaseData	m_MarketPurchases;
	CSSWeaponData				m_WeaponData;
	CSSDeathData				m_DeathData;

	// A static list of all the stat containers, one for each data structure being tracked
	static StatContainerList_t * s_StatLists;

	bool						m_bInRound;
};

extern CCSGameStats CCS_GameStats;

#endif // CS_GAMESTATS_H

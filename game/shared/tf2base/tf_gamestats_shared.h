//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#ifndef TF_GAMESTATS_SHARED_H
#define TF_GAMESTATS_SHARED_H
#ifdef _WIN32
#pragma once
#endif
#include "cbase.h"
#include "tier1/utlvector.h"
#include "tier1/utldict.h"
#include "shareddefs.h"
#include "tf_shareddefs.h"

//=============================================================================
//
// TF Game Stats Enums
//
enum TFStatType_t
{
	TFSTAT_UNDEFINED = 0,
	TFSTAT_SHOTS_HIT,
	TFSTAT_SHOTS_FIRED,
	TFSTAT_KILLS,
	TFSTAT_DEATHS,
	TFSTAT_DAMAGE,
	TFSTAT_CAPTURES,
	TFSTAT_DEFENSES,
	TFSTAT_DOMINATIONS,
	TFSTAT_REVENGE,
	TFSTAT_POINTSSCORED,
	TFSTAT_BUILDINGSDESTROYED,
	TFSTAT_HEADSHOTS,
	TFSTAT_PLAYTIME,
	TFSTAT_HEALING,
	TFSTAT_INVULNS,
	TFSTAT_KILLASSISTS,
	TFSTAT_BACKSTABS,
	TFSTAT_HEALTHLEACHED,
	TFSTAT_BUILDINGSBUILT,
	TFSTAT_MAXSENTRYKILLS,
	TFSTAT_TELEPORTS,
	TFSTAT_MAX
};

#define TFSTAT_FIRST (TFSTAT_UNDEFINED+1)
#define TFSTAT_LAST (TFSTAT_MAX-1)


//=============================================================================
//
// TF Player Round Stats
//
struct RoundStats_t
{
	int		m_iStat[TFSTAT_MAX];

	RoundStats_t() { Reset(); };

	void Reset() {
		for ( int i = 0; i < ARRAYSIZE( m_iStat ); i++ )
		{
			m_iStat[i] = 0;
		}
	};

	void AccumulateRound( const RoundStats_t &other )
	{
		for ( int i = 0; i < ARRAYSIZE( m_iStat ); i++ )
		{
			m_iStat[i] += other.m_iStat[i];
		}
	};
};

enum TFGameStatsVersions_t
{
	TF_GAMESTATS_FILE_VERSION = 006,
	TF_GAMESTATS_MAGIC = 0xDEADBEEF
};

enum TFGameStatsLumpIds_t
{
	TFSTATS_LUMP_VERSION = 1,
	TFSTATS_LUMP_MAPHEADER,
	TFSTATS_LUMP_MAPDEATH,
	TFSTATS_LUMP_MAPDAMAGE,
	TFSTATS_LUMP_CLASS,	
	TFSTATS_LUMP_WEAPON,
	TFSTATS_LUMP_ENDTAG,
	MAX_LUMP_COUNT
};

struct TF_Gamestats_Version_t
{
	int m_iMagic;			// always TF_GAMESTATS_MAGIC
	int m_iVersion;
};

struct TF_Gamestats_ClassStats_t
{
	static const unsigned short LumpId = TFSTATS_LUMP_CLASS;	// Lump ids.
	int iSpawns;												// total # of spawns of this class
	int iTotalTime;												// aggregate player time in seconds in this class
	int iScore;													// total # of points scored by this class
	int iKills;													// total # of kills by this class
	int iDeaths;												// total # of deaths by this class
	int iAssists;												// total # of assists by this class
	int iCaptures;												// total # of captures by this class

	void Accumulate( TF_Gamestats_ClassStats_t &other )
	{
		iSpawns += other.iSpawns;
		iTotalTime += other.iTotalTime;
		iScore += other.iScore;
		iKills += other.iKills;
		iDeaths += other.iDeaths;
		iAssists += other.iAssists;
		iCaptures += other.iCaptures;
	}
};

struct TF_Gamestats_WeaponStats_t
{
	static const unsigned short LumpId = TFSTATS_LUMP_WEAPON;	// Lump ids.
	int iShotsFired;
	int iCritShotsFired;
	int	iHits;
	int iTotalDamage;
	int iHitsWithKnownDistance;
	int64 iTotalDistance;

	void Accumulate( TF_Gamestats_WeaponStats_t &other )
	{
		iShotsFired += other.iShotsFired;
		iCritShotsFired += other.iCritShotsFired;
		iHits += other.iHits;
		iTotalDamage += other.iTotalDamage;
		iHitsWithKnownDistance += other.iHitsWithKnownDistance;
		iTotalDistance += other.iTotalDistance;
	}
};

//=============================================================================
//
// TF Game Level Stats Data
//
struct TF_Gamestats_LevelStats_t
{
public:

	TF_Gamestats_LevelStats_t();
	~TF_Gamestats_LevelStats_t();
	TF_Gamestats_LevelStats_t( const TF_Gamestats_LevelStats_t &stats );

	// Level start and end
	void Init( const char *pszMapName, int nIPAddr, short nPort, float flStartTime );
	void Shutdown( float flEndTime );

	void Accumulate( TF_Gamestats_LevelStats_t *pOther )
	{
		m_Header.Accumulate( pOther->m_Header );
		m_aPlayerDeaths.AddVectorToTail( pOther->m_aPlayerDeaths );
		m_aPlayerDamage.AddVectorToTail( pOther->m_aPlayerDamage );
		int i;
		for ( i = 0; i < ARRAYSIZE( m_aClassStats ); i++ )
		{
			m_aClassStats[i].Accumulate( pOther->m_aClassStats[i] );
		}
		for ( i = 0; i < ARRAYSIZE( m_aWeaponStats ); i++ )
		{
			m_aWeaponStats[i].Accumulate( pOther->m_aWeaponStats[i] );
		}

	}
public:

	// Level header data.
	struct LevelHeader_t
	{
		static const unsigned short LumpId = TFSTATS_LUMP_MAPHEADER;	// Lump ids.
		char			m_szMapName[64];							// Name of the map.
		unsigned int	m_nIPAddr;									// IP Address of the server - 4 bytes stored as an int.
		unsigned short	m_nPort;									// Port the server is using.	
		int				m_iRoundsPlayed;							// # of rounds played
		int				m_iTotalTime;								// total # of seconds of all rounds
		int				m_iBlueWins;								// # of blue team wins
		int				m_iRedWins;									// # of red team wins
		int				m_iStalemates;								// # of stalemates
		int				m_iBlueSuddenDeathWins;						// # of blue team wins during sudden death
		int				m_iRedSuddenDeathWins;						// # of red team wins during sudden death

		void Accumulate( LevelHeader_t &other )
		{
			m_iRoundsPlayed += other.m_iRoundsPlayed;
			m_iTotalTime += other.m_iTotalTime;
			m_iBlueWins += other.m_iBlueWins;
			m_iRedWins += other.m_iRedWins;
			m_iStalemates += other.m_iStalemates;
			m_iBlueSuddenDeathWins += other.m_iBlueSuddenDeathWins;
			m_iRedSuddenDeathWins += other.m_iRedSuddenDeathWins;
		}
	};

	// Player deaths.
	struct PlayerDeathsLump_t
	{
		static const unsigned short LumpId = TFSTATS_LUMP_MAPDEATH;	// Lump ids.
		short			nPosition[3];								// Position of death.
		short			iWeapon;									// Weapon that killed the player.
		unsigned short	iDistance;									// Distance the attacker was from the player.
		byte			iAttackClass;								// Class that killed the player.
		byte			iTargetClass;								// Class of the player killed.
	};

	// Player damage.
	struct PlayerDamageLump_t
	{
		static const unsigned short LumpId = TFSTATS_LUMP_MAPDAMAGE;	// Lump ids.
		float			fTime;										// Time of the damage event
		short			nTargetPosition[3];							// Position of target.
		short			nAttackerPosition[3];						// Position of attacker.
		short			iDamage;									// Total damage.
		short			iWeapon;									// Weapon used.
		byte			iAttackClass;								// Class of the attacker
		byte			iTargetClass;								// Class of the target
		byte			iCrit;										// was the shot a crit?
		byte			iKill;										// did the shot kill the target?
	};

	// Data.
	LevelHeader_t					m_Header;							// Level header.
	CUtlVector<PlayerDeathsLump_t>	m_aPlayerDeaths;					// Vector of player deaths.
	CUtlVector<PlayerDamageLump_t>	m_aPlayerDamage;					// Vector of player damage.	
	TF_Gamestats_ClassStats_t		m_aClassStats[TF_CLASS_COUNT_ALL];	// Vector of class data
	TF_Gamestats_WeaponStats_t		m_aWeaponStats[TF_WEAPON_COUNT];	// Vector of weapon data
	// Temporary data.
	bool							m_bInitialized;		// Has the map Map Stat Data been initialized.
	float							m_flRoundStartTime;
	int								m_iPeakPlayerCount[TF_TEAM_COUNT];
};

struct KillStats_t
{
	KillStats_t() { Reset(); }

	void Reset()
	{
		Q_memset( iNumKilled, 0, sizeof( iNumKilled ) );
		Q_memset( iNumKilledBy, 0, sizeof( iNumKilledBy ) );
		Q_memset( iNumKilledByUnanswered, 0, sizeof( iNumKilledByUnanswered ) );
	}

	int iNumKilled[MAX_PLAYERS+1];					// how many times this player has killed every other player
	int iNumKilledBy[MAX_PLAYERS+1];				// how many times this player has been killed by every other player
	int iNumKilledByUnanswered[MAX_PLAYERS+1];		// how many unanswered kills this player has been dealt by every other player
};


//=============================================================================
//
// TF Player Stats 
//
struct PlayerStats_t
{
	PlayerStats_t()	
	{
		Reset();
	};

	void Reset()
	{
		statsCurrentLife.Reset();
		statsCurrentRound.Reset();
		statsAccumulated.Reset();
		statsKills.Reset();
	}

	PlayerStats_t( const PlayerStats_t &other )
	{
		statsCurrentLife	= other.statsCurrentLife;
		statsCurrentRound	= other.statsCurrentRound;
		statsAccumulated	= other.statsAccumulated;
	}

	RoundStats_t	statsCurrentLife;
	RoundStats_t	statsCurrentRound;
	RoundStats_t	statsAccumulated;
	int				iStatsChangedBits;			// bit mask of which stats have changed
	float			m_flTimeLastSend;			// time we last sent stat update to this player

	KillStats_t		statsKills;
};

// reported stats structure that contains all stats data uploaded from TF server to Steam.  Note that this
// code is shared between TF server and processgamestats, which cracks the data file on the back end
struct TFReportedStats_t
{
	TFReportedStats_t();
	~TFReportedStats_t();
	void Clear();
	TF_Gamestats_LevelStats_t	*FindOrAddMapStats( const char *szMapName );
#ifdef GAME_DLL
	void AppendCustomDataToSaveBuffer( CUtlBuffer &SaveBuffer );
	bool LoadCustomDataFromBuffer( CUtlBuffer &LoadBuffer );
#endif 

	TF_Gamestats_LevelStats_t								*m_pCurrentGame;
	CUtlDict<TF_Gamestats_LevelStats_t, unsigned short>		m_dictMapStats;
};

enum { 
	STATMSG_UPDATE,
	STATMSG_RESET,
	STATMSG_PLAYERSPAWN,
	STATMSG_PLAYERRESPAWN,
	STATMSG_PLAYERDEATH
};

#endif // TF_GAMESTATS_SHARED_H

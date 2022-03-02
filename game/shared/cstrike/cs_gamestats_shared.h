//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================
#ifndef CS_GAMESTATS_SHARED_H
#define CS_GAMESTATS_SHARED_H
#ifdef _WIN32
#pragma once
#endif
#include "cbase.h"
// #include "tier1/utlvector.h"
// #include "tier1/utldict.h"
#include "shareddefs.h"
#include "cs_shareddefs.h"
#include "cs_weapon_parse.h"
#include "fmtstr.h"


#define CS_NUM_LEVELS 18

//=============================================================================
// Helper class for simple manipulation of bit arrays.
// Used for server->client packets containing delta stats
//=============================================================================

template <int BitLength>
class BitArray
{
	enum { ByteLength = (BitLength + 7) / 8 };
public:
	BitArray() { ClearAll(); }

	void SetBit(int n) { m_bytes[n / 8] |= 1 << (n & 7); }
	void ClearBit(int n) { m_bytes[n / 8] &= (~(1 << (n & 7))); }
	bool IsBitSet(int n) const { return (m_bytes[n / 8] & (1 << (n & 7))) != 0;}

	void ClearAll() { V_memset(m_bytes, 0, sizeof(m_bytes)); }
	int NumBits() { return BitLength; }
	int NumBytes() { return ByteLength; }

	byte* RawPointer() { return m_bytes; }

private:
	byte m_bytes[ByteLength];
};


//=============================================================================
//
// CS Game Stats Enums
//
// WARNING! ANY CHANGE TO THE ORDERING OR NUMBER OF STATS WILL REQUIRE 
// SYNCHRONOUS UPDATE OF CLIENT AND SERVER DLLS. If you change these enums
// (including adding new stats) without forcing an update of both the client 
// and server, stats will become corrupted on the game client and these 
// corrupted values will be uploaded to steam, which is very very bad.
//
// If you add new stats, if will be safest to add them at the end of the enum
// (although this will still require a server update); make sure you also add
// the stats to the CSStatProperty_Table in cs_gamestats_shared.cpp at the
// appropriate location.

enum CSStatType_t
{
	CSSTAT_UNDEFINED = -1,
	CSSTAT_SHOTS_HIT,
	CSSTAT_SHOTS_FIRED,
	CSSTAT_KILLS,
	CSSTAT_DEATHS,
	CSSTAT_DAMAGE,
    CSSTAT_NUM_BOMBS_PLANTED,
    CSSTAT_NUM_BOMBS_DEFUSED,
	CSSTAT_PLAYTIME,
	CSSTAT_ROUNDS_WON,
	CSSTAT_T_ROUNDS_WON,
	CSSTAT_CT_ROUNDS_WON,
	CSSTAT_ROUNDS_PLAYED,
	CSSTAT_PISTOLROUNDS_WON,
	CSSTAT_MONEY_EARNED,
	CSSTAT_OBJECTIVES_COMPLETED,
	CSSTAT_BOMBS_DEFUSED_WITHKIT,

	CSSTAT_KILLS_DEAGLE,
	CSSTAT_KILLS_USP,
	CSSTAT_KILLS_GLOCK,
	CSSTAT_KILLS_P228,
	CSSTAT_KILLS_ELITE,
	CSSTAT_KILLS_FIVESEVEN,
	CSSTAT_KILLS_AWP,
	CSSTAT_KILLS_AK47,
	CSSTAT_KILLS_M4A1,
	CSSTAT_KILLS_AUG,
	CSSTAT_KILLS_SG552,
	CSSTAT_KILLS_SG550,
	CSSTAT_KILLS_GALIL,
	CSSTAT_KILLS_FAMAS,
	CSSTAT_KILLS_SCOUT,
	CSSTAT_KILLS_G3SG1,
	CSSTAT_KILLS_P90,
	CSSTAT_KILLS_MP5NAVY,
	CSSTAT_KILLS_TMP,
	CSSTAT_KILLS_MAC10,
	CSSTAT_KILLS_UMP45,
	CSSTAT_KILLS_M3,
	CSSTAT_KILLS_XM1014,
	CSSTAT_KILLS_M249,
	CSSTAT_KILLS_KNIFE,
	CSSTAT_KILLS_HEGRENADE,

	CSSTAT_SHOTS_DEAGLE,
	CSSTAT_SHOTS_USP,
	CSSTAT_SHOTS_GLOCK,
	CSSTAT_SHOTS_P228,
	CSSTAT_SHOTS_ELITE,
	CSSTAT_SHOTS_FIVESEVEN,
	CSSTAT_SHOTS_AWP,
	CSSTAT_SHOTS_AK47,
	CSSTAT_SHOTS_M4A1,
	CSSTAT_SHOTS_AUG,
	CSSTAT_SHOTS_SG552,
	CSSTAT_SHOTS_SG550,
	CSSTAT_SHOTS_GALIL,
	CSSTAT_SHOTS_FAMAS,
	CSSTAT_SHOTS_SCOUT,
	CSSTAT_SHOTS_G3SG1,
	CSSTAT_SHOTS_P90,
	CSSTAT_SHOTS_MP5NAVY,
	CSSTAT_SHOTS_TMP,
	CSSTAT_SHOTS_MAC10,
	CSSTAT_SHOTS_UMP45,
	CSSTAT_SHOTS_M3,
	CSSTAT_SHOTS_XM1014,
	CSSTAT_SHOTS_M249,
	CSSTAT_SHOTS_KNIFE,
	CSSTAT_SHOTS_HEGRENADE,

	CSSTAT_HITS_DEAGLE,
	CSSTAT_HITS_USP,
	CSSTAT_HITS_GLOCK,
	CSSTAT_HITS_P228,
	CSSTAT_HITS_ELITE,
	CSSTAT_HITS_FIVESEVEN,
	CSSTAT_HITS_AWP,
	CSSTAT_HITS_AK47,
	CSSTAT_HITS_M4A1,
	CSSTAT_HITS_AUG,
	CSSTAT_HITS_SG552,
	CSSTAT_HITS_SG550,
	CSSTAT_HITS_GALIL,
	CSSTAT_HITS_FAMAS,
	CSSTAT_HITS_SCOUT,
	CSSTAT_HITS_G3SG1,
	CSSTAT_HITS_P90,
	CSSTAT_HITS_MP5NAVY,
	CSSTAT_HITS_TMP,
	CSSTAT_HITS_MAC10,
	CSSTAT_HITS_UMP45,
	CSSTAT_HITS_M3,
	CSSTAT_HITS_XM1014,
	CSSTAT_HITS_M249,
	CSSTAT_HITS_KNIFE,
	CSSTAT_HITS_HEGRENADE,

	CSSTAT_DAMAGE_DEAGLE,
	CSSTAT_DAMAGE_USP,
	CSSTAT_DAMAGE_GLOCK,
	CSSTAT_DAMAGE_P228,
	CSSTAT_DAMAGE_ELITE,
	CSSTAT_DAMAGE_FIVESEVEN,
	CSSTAT_DAMAGE_AWP,
	CSSTAT_DAMAGE_AK47,
	CSSTAT_DAMAGE_M4A1,
	CSSTAT_DAMAGE_AUG,
	CSSTAT_DAMAGE_SG552,
	CSSTAT_DAMAGE_SG550,
	CSSTAT_DAMAGE_GALIL,
	CSSTAT_DAMAGE_FAMAS,
	CSSTAT_DAMAGE_SCOUT,
	CSSTAT_DAMAGE_G3SG1,
	CSSTAT_DAMAGE_P90,
	CSSTAT_DAMAGE_MP5NAVY,
	CSSTAT_DAMAGE_TMP,
	CSSTAT_DAMAGE_MAC10,
	CSSTAT_DAMAGE_UMP45,
	CSSTAT_DAMAGE_M3,
	CSSTAT_DAMAGE_XM1014,
	CSSTAT_DAMAGE_M249,
	CSSTAT_DAMAGE_KNIFE,
	CSSTAT_DAMAGE_HEGRENADE,	

	CSSTAT_KILLS_HEADSHOT,
	CSSTAT_KILLS_ENEMY_BLINDED,
	CSSTAT_KILLS_WHILE_BLINDED,
	CSSTAT_KILLS_WITH_LAST_ROUND,
	CSSTAT_KILLS_ENEMY_WEAPON,
	CSSTAT_KILLS_KNIFE_FIGHT,
	CSSTAT_KILLS_WHILE_DEFENDING_BOMB,

	CSSTAT_DECAL_SPRAYS,
	CSSTAT_TOTAL_JUMPS,
	CSSTAT_NIGHTVISION_DAMAGE,
	CSSTAT_KILLS_WHILE_LAST_PLAYER_ALIVE,
	CSSTAT_KILLS_ENEMY_WOUNDED,
	CSSTAT_FALL_DAMAGE,

	CSSTAT_NUM_HOSTAGES_RESCUED,

	CSSTAT_NUM_BROKEN_WINDOWS,
	CSSTAT_PROPSBROKEN_ALL,
	CSSTAT_PROPSBROKEN_MELON,
	CSSTAT_PROPSBROKEN_OFFICEELECTRONICS,
	CSSTAT_PROPSBROKEN_OFFICERADIO,
	CSSTAT_PROPSBROKEN_OFFICEJUNK,
	CSSTAT_PROPSBROKEN_ITALY_MELON,

	CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER,

	CSSTAT_WEAPONS_DONATED,

	CSSTAT_ITEMS_PURCHASED,
	CSSTAT_MONEY_SPENT,

	CSSTAT_DOMINATIONS,
	CSSTAT_DOMINATION_OVERKILLS,
	CSSTAT_REVENGES,

	CSSTAT_MVPS,

	CSSTAT_GRENADE_DAMAGE,
	CSSTAT_GRENADE_POSTHUMOUSKILLS,
	CSSTAT_GRENADES_THROWN,

    CSTAT_ITEMS_DROPPED_VALUE,

	//Map win stats
	CSSTAT_MAP_WINS_CS_ASSAULT,
	CSSTAT_MAP_WINS_CS_COMPOUND,
	CSSTAT_MAP_WINS_CS_HAVANA,
	CSSTAT_MAP_WINS_CS_ITALY,
	CSSTAT_MAP_WINS_CS_MILITIA,
	CSSTAT_MAP_WINS_CS_OFFICE,
	CSSTAT_MAP_WINS_DE_AZTEC,
	CSSTAT_MAP_WINS_DE_CBBLE,
	CSSTAT_MAP_WINS_DE_CHATEAU,
	CSSTAT_MAP_WINS_DE_DUST2,
	CSSTAT_MAP_WINS_DE_DUST,
	CSSTAT_MAP_WINS_DE_INFERNO,
	CSSTAT_MAP_WINS_DE_NUKE,
	CSSTAT_MAP_WINS_DE_PIRANESI,
	CSSTAT_MAP_WINS_DE_PORT,
	CSSTAT_MAP_WINS_DE_PRODIGY,
	CSSTAT_MAP_WINS_DE_TIDES,
	CSSTAT_MAP_WINS_DE_TRAIN,

	CSSTAT_MAP_ROUNDS_CS_ASSAULT,
	CSSTAT_MAP_ROUNDS_CS_COMPOUND,
	CSSTAT_MAP_ROUNDS_CS_HAVANA,
	CSSTAT_MAP_ROUNDS_CS_ITALY,
	CSSTAT_MAP_ROUNDS_CS_MILITIA,
	CSSTAT_MAP_ROUNDS_CS_OFFICE,
	CSSTAT_MAP_ROUNDS_DE_AZTEC,
	CSSTAT_MAP_ROUNDS_DE_CBBLE,
	CSSTAT_MAP_ROUNDS_DE_CHATEAU,
	CSSTAT_MAP_ROUNDS_DE_DUST2,
	CSSTAT_MAP_ROUNDS_DE_DUST,
	CSSTAT_MAP_ROUNDS_DE_INFERNO,
	CSSTAT_MAP_ROUNDS_DE_NUKE,
	CSSTAT_MAP_ROUNDS_DE_PIRANESI,
	CSSTAT_MAP_ROUNDS_DE_PORT,
	CSSTAT_MAP_ROUNDS_DE_PRODIGY,
	CSSTAT_MAP_ROUNDS_DE_TIDES,
	CSSTAT_MAP_ROUNDS_DE_TRAIN,

	CSSTAT_LASTMATCH_T_ROUNDS_WON,
	CSSTAT_LASTMATCH_CT_ROUNDS_WON,
	CSSTAT_LASTMATCH_ROUNDS_WON,
	CSSTAT_LASTMATCH_KILLS,
	CSSTAT_LASTMATCH_DEATHS,
	CSSTAT_LASTMATCH_MVPS,
	CSSTAT_LASTMATCH_DAMAGE,
	CSSTAT_LASTMATCH_MONEYSPENT,
	CSSTAT_LASTMATCH_DOMINATIONS,
	CSSTAT_LASTMATCH_REVENGES,
	CSSTAT_LASTMATCH_MAX_PLAYERS,
	CSSTAT_LASTMATCH_FAVWEAPON_ID,
	CSSTAT_LASTMATCH_FAVWEAPON_SHOTS,
	CSSTAT_LASTMATCH_FAVWEAPON_HITS,
	CSSTAT_LASTMATCH_FAVWEAPON_KILLS,

	CSSTAT_MAX	//Must be last entry.
};


#define CSSTAT_FIRST (CSSTAT_UNDEFINED+1)
#define CSSTAT_LAST (CSSTAT_MAX-1)

//
// CS Game Stats Flags
//
#define CSSTAT_PRIORITY_MASK		0x000F
#define CSSTAT_PRIORITY_NEVER		0x0000		// not sent to client
#define CSSTAT_PRIORITY_ENDROUND	0x0001		// sent at end of round
#define CSSTAT_PRIORITY_LOW			0x0002		// sent every 2500ms
#define CSSTAT_PRIORITY_HIGH		0x0003		// sent every 250ms

struct CSStatProperty
{
	int statId;							// verify that table ordering is correct
	const char*	szSteamName;			// name of the stat on steam
	const char*	szLocalizationToken;   // localization token for the stat
	uint flags;							// priority flags for sending to client
};

extern CSStatProperty CSStatProperty_Table[];


//=============================================================================
//
// CS Player Round Stats
//
struct StatsCollection_t
{
	StatsCollection_t() { Reset(); }

	inline int Get( int i ) const
	{
		AssertMsg( i >= CSSTAT_FIRST && i < CSSTAT_MAX, "Stat index out of range!" );
		if ( i >= 0 )
			return m_iValue[ i ];
		return 0;
	}

	inline void Set( int i, int nValue )
	{
		AssertMsg( i >= CSSTAT_FIRST && i < CSSTAT_MAX, "Stat index out of range!" );
		if ( i >= 0 )
			m_iValue[ i ] = nValue;
	}

	void Reset()
	{
		for ( int i = 0; i < ARRAYSIZE( m_iValue ); i++ )
		{
			m_iValue[i] = 0;
		}
	}

	int operator[] ( int index ) const
	{
		Assert(index >= 0 && index < ARRAYSIZE(m_iValue));
		return m_iValue[index];
	}

	int& operator[] ( int index )
	{
		Assert(index >= 0 && index < ARRAYSIZE(m_iValue));
		return m_iValue[index];
	}

	void Aggregate( const StatsCollection_t& other );

private:
	int m_iValue[CSSTAT_MAX];
};


//=============================================================================
// HPE_BEGIN:
// [tj] A couple variations on the RoundStats structure to handle extra operations
//		for averaging and accumulating
//=============================================================================
struct RoundStatsDirectAverage_t
{
	float m_fStat[CSSTAT_MAX];


	RoundStatsDirectAverage_t()
	{
		Reset();
	}

	void Reset()
	{
		for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
		{
			m_fStat[i] = 0;
		}
	}

	RoundStatsDirectAverage_t& operator +=( const StatsCollection_t &other )
	{
		for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
		{
			m_fStat[i] += other[i];
		}
		return *this;
	}

	RoundStatsDirectAverage_t& operator /=( const float &divisor)
	{
		if (divisor > 0)
		{
			for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
			{
				m_fStat[i] /= divisor;
			}
		}
		return *this;
	}

	RoundStatsDirectAverage_t& operator *=( const float &divisor)
	{
		for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
		{
			m_fStat[i] *= divisor;
		}
		return *this;
	}
};


struct RoundStatsRollingAverage_t
{
    float m_fStat[CSSTAT_MAX];
    int m_numberOfDataSets;

    RoundStatsRollingAverage_t()
    {
        Reset();
    }

    void Reset()
    {
        for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
        {
            m_fStat[i] = 0;
        }
        m_numberOfDataSets = 0;
    }

    RoundStatsRollingAverage_t& operator +=( const RoundStatsRollingAverage_t &other )
    {
        for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
        {
            m_fStat[i] += other.m_fStat[i];
        }
        return *this;
    }

    RoundStatsRollingAverage_t& operator +=( const StatsCollection_t &other )
    {
        for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
        {
            m_fStat[i] += other[i];
        }
        return *this;
    }

    RoundStatsRollingAverage_t& operator /=( const float &divisor)
    {
        if (divisor > 0)
        {
            for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
            {
                m_fStat[i] /= divisor;
            }
        }
        return *this;
    }

    void RollDataSetIntoAverage ( const RoundStatsRollingAverage_t &other )
    {
        for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
        {
            m_fStat[i] *= m_numberOfDataSets;
            m_fStat[i] += other.m_fStat[i];
            m_fStat[i] /= (m_numberOfDataSets + 1);
        }
        m_numberOfDataSets++;
    }
};
//=============================================================================
// HPE_END
//=============================================================================

enum CSGameStatsVersions_t
{
	CS_GAMESTATS_FILE_VERSION = 006,
	CS_GAMESTATS_MAGIC = 0xDEADBEEF
};

struct CS_Gamestats_Version_t
{
	int m_iMagic;			// always CS_GAMESTATS_MAGIC
	int m_iVersion;
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

	int iNumKilled[MAX_PLAYERS+1];					// how many times this player has killed each other player
	int iNumKilledBy[MAX_PLAYERS+1];				// how many times this player has been killed by each other player
	int iNumKilledByUnanswered[MAX_PLAYERS+1];		// how many unanswered kills this player has been dealt by each other player
};

//=============================================================================
//
// CS Player Stats
//
struct PlayerStats_t
{
	PlayerStats_t()
	{
		Reset();
	}

	void Reset()
	{
		statsDelta.Reset();
		statsCurrentRound.Reset();
		statsCurrentMatch.Reset();
		statsKills.Reset();
	}

	PlayerStats_t( const PlayerStats_t &other )
	{
		statsDelta			= other.statsDelta;
		statsCurrentRound	= other.statsCurrentRound;
		statsCurrentMatch	= other.statsCurrentMatch;
	}

	StatsCollection_t	statsDelta;
	StatsCollection_t	statsCurrentRound;
	StatsCollection_t	statsCurrentMatch;
 	KillStats_t		statsKills;
};


struct WeaponName_StatId
{
	CSWeaponID   weaponId;
	CSStatType_t killStatId;
	CSStatType_t shotStatId;
	CSStatType_t hitStatId;
	CSStatType_t damageStatId;
};

struct MapName_MapStatId
{
	const char* szMapName;
	CSStatType_t statWinsId;
	CSStatType_t statRoundsId;
};

extern const MapName_MapStatId MapName_StatId_Table[];

//A mapping from weapon names to weapon stat IDs
extern const WeaponName_StatId WeaponName_StatId_Table[];

//Used to look up the appropriate entry by the ID of the actual weapon
const WeaponName_StatId& GetWeaponTableEntryFromWeaponId(CSWeaponID id);

#include "steamworks_gamestats.h"

//=============================================================================
//
// Helper functions for creating key values
//
void AddDataToKV( KeyValues* pKV, const char* name, int data );
void AddDataToKV( KeyValues* pKV, const char* name, uint64 data );
void AddDataToKV( KeyValues* pKV, const char* name, float data );
void AddDataToKV( KeyValues* pKV, const char* name, bool data );
void AddDataToKV( KeyValues* pKV, const char* name, const char* data );
void AddDataToKV( KeyValues* pKV, const char* name, const Color& data );
void AddDataToKV( KeyValues* pKV, const char* name, short data );
void AddDataToKV( KeyValues* pKV, const char* name, unsigned data );
void AddDataToKV( KeyValues* pKV, const char* name, const Vector& data );
void AddPositionDataToKV( KeyValues* pKV, const char* name, const Vector &data );
//=============================================================================

//=============================================================================
//
// Helper functions for creating key values from arrays
//
void AddArrayDataToKV( KeyValues* pKV, const char* name, const short *data, unsigned size );
void AddArrayDataToKV( KeyValues* pKV, const char* name, const byte *data, unsigned size );
void AddArrayDataToKV( KeyValues* pKV, const char* name, const unsigned *data, unsigned size );
void AddStringDataToKV( KeyValues* pKV, const char* name, const char *data );

//=============================================================================

// Macros to ease the creation of SendData method for stats structs/classes
#define BEGIN_STAT_TABLE( tableName ) \
	static const char* GetStatTableName( void ) { return tableName; } \
	void BuildGamestatDataTable( KeyValues* pKV ) \
{ \
	pKV->SetName( GetStatTableName() ); 

#define REGISTER_STAT( varName ) \
	AddDataToKV(pKV, #varName, varName);

#define REGISTER_STAT_NAMED( varName, dbName ) \
	AddDataToKV(pKV, dbName, varName);

#define REGISTER_STAT_POSITION( varName ) \
	AddPositionDataToKV(pKV, #varName, varName);

#define REGISTER_STAT_POSITION_NAMED( varName, dbName ) \
	AddPositionDataToKV(pKV, dbName, varName);

#define REGISTER_STAT_ARRAY( varName ) \
	AddArrayDataToKV( pKV, #varName, varName, ARRAYSIZE( varName ) );

#define REGISTER_STAT_ARRAY_NAMED( varName, dbName ) \
	AddArrayDataToKV( pKV, dbName, varName, ARRAYSIZE( varName ) );

#define REGISTER_STAT_STRING( varName ) \
	AddStringDataToKV( pKV, #varName, varName );

#define REGISTER_STAT_STRING_NAMED( varName, dbName ) \
	AddStringDataToKV( pKV, dbName, varName );

#define AUTO_STAT_TABLE_KEY() \
	pKV->SetInt( "TimeSubmitted", GetUniqueIDForStatTable( *this ) );

#define END_STAT_TABLE() \
	pKV->SetUint64( ::BaseStatData::m_bUseGlobalData ? "TimeSubmitted" : "SessionTime", ::BaseStatData::TimeSubmitted ); \
	GetSteamWorksSGameStatsUploader().AddStatsForUpload( pKV ); \
}

//-----------------------------------------------------------------------------
// Purpose: Templatized class for getting unique ID's for stat tables that need
//			to be submitted multiple times per-session.
//-----------------------------------------------------------------------------

template < typename T >
class UniqueStatID_t
{
public:
	static unsigned GetNext( void )
	{
		return ++s_nLastID;
	}

	static void Reset( void )
	{
		s_nLastID = 0;
	}

private:
	static unsigned s_nLastID;
};

template < typename T >
unsigned UniqueStatID_t< T >::s_nLastID = 0;

template < typename T >
unsigned GetUniqueIDForStatTable( const T &table )
{
	return UniqueStatID_t< T >::GetNext();
}


//=============================================================================
//
// An interface for tracking gamestats.
//
class IGameStatTracker
{
public:

	//-----------------------------------------------------------------------------
	// Templatized methods to track a per-mission stat.
	// The stat is copied, then deleted after it's sent to the SQL server.
	//-----------------------------------------------------------------------------
	template < typename T >
	void SubmitStat( T& stat )
	{
		// Make a copy of the stat. All of the stat lists require pointers,
		// so we need to protect against a stat allocated on the stack
		T* pT = new T();
		if( !pT )
			return;

		*pT = stat;
		SubmitStat( pT );
	}

	//-----------------------------------------------------------------------------
	// Templatized methods to track a per-mission stat (by pointer)
	// The stat is deleted after it's sent to the SQL server
	//-----------------------------------------------------------------------------
	template < typename T >
	void SubmitStat( T* pStat )
	{
		// Get the static stat table for this type and add the stat to it
		GetStatTable<T>()->AddToTail( pStat );
	}

	//-----------------------------------------------------------------------------
	// Add all stats to an existing key value file for submit.
	//-----------------------------------------------------------------------------
	virtual void SubmitGameStats( KeyValues *pKV ) = 0;

	//-----------------------------------------------------------------------------
	// Prints the memory usage of all of the stats being tracked
	//-----------------------------------------------------------------------------
	void PrintGamestatMemoryUsage( void );

protected:
	//=============================================================================
	//
	// Used as a base interface to store a list of all templatized stat containers
	//
	class IStatContainer
	{
	public:
		virtual void SendData( KeyValues *pKV ) = 0;
		virtual void Clear( void ) = 0;
		virtual void PrintMemoryUsage( void ) = 0;
	};

	// Defines a list of stat containers.
	typedef CUtlVector< IStatContainer* > StatContainerList_t;

	//-----------------------------------------------------------------------------
	// Used to get a list of all stats containers being tracked by the deriving class
	//-----------------------------------------------------------------------------
	virtual StatContainerList_t* GetStatContainerList( void ) = 0;

private:

	//=============================================================================
	//
	// Templatized list of stats submitted
	//
	template < typename T >
	class CGameStatList : public IStatContainer, public CUtlVector< T* >
	{
	public:
		//-----------------------------------------------------------------------------
		// Get data ready to send to the SQL server
		//-----------------------------------------------------------------------------
		virtual void SendData( KeyValues *pKV )
		{
			//ASSERT( pKV != NULL );

			// Duplicate the master KeyValue for each stat instance
			for( int i=0; i < this->m_Size; ++i )
			{
				// Make a copy of the master key value and build the stat table
				KeyValues *pKVCopy = this->operator [](i)->m_bUseGlobalData ? pKV->MakeCopy() : new KeyValues( "" );
				this->operator [](i)->BuildGamestatDataTable( pKVCopy );
			}

			// Reset unique ID counter for the stat type
			UniqueStatID_t< T >::Reset();
		}

		//-----------------------------------------------------------------------------
		// Clear and delete every stat in this list
		//-----------------------------------------------------------------------------
		virtual void Clear( void )
		{
			this->PurgeAndDeleteElements();
		}

		//-----------------------------------------------------------------------------
		// Print out details about this lists memory usage
		//-----------------------------------------------------------------------------
		virtual void PrintMemoryUsage( void )
		{
			if( this->m_Size == 0 )
				return;

			// Compute the memory used as the size of type times the list count
			unsigned uMemoryUsed = this->m_Size * ( sizeof( T ) );

			Msg( "	%d\tbytes used by %s table\n", uMemoryUsed, T::GetStatTableName() );
		}
	};

	//-----------------------------------------------------------------------------
	// Templatized method to get a single instance of a stat list per data type.
	//-----------------------------------------------------------------------------
	template < typename T >
	CGameStatList< T >* GetStatTable( void )
	{
		static CGameStatList< T > *s_vecOfType = 0;
		if( s_vecOfType == 0 )
		{
			s_vecOfType = new CGameStatList< T >();
			GetStatContainerList()->AddToTail( s_vecOfType );
		}
		return s_vecOfType;
	}

};

struct BaseStatData
{
	BaseStatData( bool bUseGlobalData = true ) : m_bUseGlobalData( bUseGlobalData )
	{
		TimeSubmitted = GetSteamWorksSGameStatsUploader().GetTimeSinceEpoch();
	}

	bool	m_bUseGlobalData;
	uint64	TimeSubmitted;

};

extern ConVar sv_noroundstats;

#endif // CS_GAMESTATS_SHARED_H

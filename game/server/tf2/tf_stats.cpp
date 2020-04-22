//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: game stat gathering
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_stats.h"
#include "tf_shareddefs.h"
#include "tf_team.h"
#include "tf_player.h"
#include "utlbuffer.h"
#include "filesystem.h"
#include "igamesystem.h"
#include "textstatsmgr.h"
#include "info_act.h"

static ConVar tf_stats( "tf_stats", "0", 0, "Enable stat gathering for TF2." );

//-----------------------------------------------------------------------------
// Collect stats every N seconds
//-----------------------------------------------------------------------------
#define TF_STATS_COLLECTION_TIME	1

#define TF_STAT_FILE		"tf_stat_total"
#define TF_TEAM_STAT_FILE	"tf_stat_team"
#define TF_PLAYER_STAT_FILE "tf_stat_class"

static char s_pStatFile[MAX_PATH];
static char s_pTeamStatFile[MAX_PATH];
static char s_pPlayerStatFile[MAX_PATH];

//-----------------------------------------------------------------------------
// Strings assocaited with the stats
//-----------------------------------------------------------------------------
static const char *s_pStatStrings[TF_STAT_COUNT] = 
{
	"Ferry Control",			// TF_STAT_FERRY_CONTROL
	"Resource Chunks Spawned",	// TF_STAT_RESOURCE_CHUNKS_SPAWNED,
	"Resource Gems Spawned",	// TF_STAT_RESOURCE_PROCESSED_CHUNKS_SPAWNED,
	"Resource Chunks Retired",	// TF_STAT_RESOURCE_CHUNKS_RETIRED,
};

// These are the strings for the team stats above TFCLASS_CLASS_COUNT.
static const char *s_pNonClassTeamStatStrings[TF_TEAM_STAT_COUNT-TFCLASS_CLASS_COUNT] = 
{
	"Player Count",			// TF_TEAM_STAT_PLAYER_COUNT,
	"Resources Collected",	// TF_TEAM_STAT_RESOURCES_COLLECTED,
	"Resources Harvested",	// TF_TEAM_STAT_RESOURCES_HARVESTED,
	"Chunks Dropped",		// TF_TEAM_STAT_RESOURCE_CHUNKS_DROPPED,
	"Chunks Collected",		// TF_TEAM_STAT_RESOURCE_CHUNKS_COLLECTED,
	"Kill Count",			// TF_TEAM_STAT_KILL_COUNT,
	"Destroyed Objects",	// TF_TEAM_STAT_DESTROYED_OBJECT_COUNT,
	"Ferry Control Time",	// TF_TEAM_STAT_FERRY_CONTROL_TIME,
};

// These are initialized in the first call to GetTeamStatString().
static const char *s_pTeamStatStrings[TF_TEAM_STAT_COUNT];
static bool s_bTeamStatStringsInitted = false;

static const char *s_pPlayerStatStrings[TF_PLAYER_STAT_COUNT] = 
{
	"Player Count",			// TF_PLAYER_STAT_PLAYER_COUNT
	"Player Seconds",		// TF_PLAYER_STAT_PLAYER_SECONDS
	"Seconds At Least One Existed",	// TF_PLAYER_STAT_EXISTING_SECONDS

	"Resources Acquired", 	// TF_PLAYER_STAT_RESOURCES_ACQUIRED
	"Resources Acquired From Chunks", 	// TF_PLAYER_STAT_RESOURCES_ACQUIRED_FROM_CHUNKS
	"Resources Carried",	// TF_PLAYER_STAT_RESOURCES_CARRIED,
	"Resources Spent",		// TF_PLAYER_STAT_RESOURCES_SPENT,
	"Object Value",			// TF_PLAYER_STAT_CURRENT_OBJECT_VALUE
	"Objects Owned",		// TF_PLAYER_STAT_OBJECT_COUNT,
	
	"Kill Count",			// TF_PLAYER_STAT_KILL_COUNT,
	"Objects Destroyed",	// TF_PLAYER_STAT_DESTROYED_OBJECT_COUNT,
	"Health Given",			// TF_PLAYER_STAT_HEALTH_GIVEN,

	"Animation Idle Time",	// TF_PLAYER_STAT_ANIMATION_IDLE,
	"Animation Walk Time",	// TF_PLAYER_STAT_ANIMATION_WALKING,
	"Animation Run Time",	// TF_PLAYER_STAT_ANIMATION_RUNNING,
	"Animation Crouch Time",// TF_PLAYER_STAT_ANIMATION_CROUCHING,
	"Animation Jump Time",	// TF_PLAYER_STAT_ANIMATION_JUMPING,
	"Animation Other Time",	// TF_PLAYER_STAT_ANIMATION_OTHER,
};


static const char *GetStatString( int stat )
{
	if (stat < TF_STAT_FIRST_OBJECT_BUILT)
		return s_pStatStrings[stat];

	static char s_TempBuf[256];
	Q_snprintf( s_TempBuf, sizeof( s_TempBuf ), "%s Count Built", GetObjectInfo( stat - TF_STAT_FIRST_OBJECT_BUILT )->m_pClassName );
	return s_TempBuf;
}

static const char *GetTeamStatString( int stat )
{
	if ( !s_bTeamStatStringsInitted )
	{
		s_bTeamStatStringsInitted = true;

		// Go through and fill in the strings.
		for ( int i=0; i < TFCLASS_CLASS_COUNT; i++ )
			s_pTeamStatStrings[i] = GetTFClassInfo( i )->m_pClassName;

		for ( i=TFCLASS_CLASS_COUNT; i < TF_TEAM_STAT_COUNT; i++ )
			s_pTeamStatStrings[i] = s_pNonClassTeamStatStrings[i - TFCLASS_CLASS_COUNT];
	}
	
	return s_pTeamStatStrings[stat];
}

static const char *GetPlayerStatString( int stat )
{
	return s_pPlayerStatStrings[stat];
}


//-----------------------------------------------------------------------------
// Implementation of the TF stats class
//-----------------------------------------------------------------------------
class CTFStats : public CAutoGameSystem, public ITFStats
{
public:
	CTFStats();

	// Inherited from IAutoServerSystem
	virtual void LevelInitPreEntity();
	virtual void FrameUpdatePostEntityThink( );

	// Clear out the stats + their history
	void ResetStats();

	void IncrementStat( TFStatId_t stat, int nIncrement );
	void SetStat( TFStatId_t stat, int nAmount );

	void IncrementTeamStat( int nTeam, TFTeamStatId_t stat, int nIncrement );
	void SetTeamStat( int nTeam, TFTeamStatId_t stat, int nAmount );

	void IncrementPlayerStat( CBaseEntity *pPlayer, TFPlayerStatId_t stat, int nIncrement );
	void ClearPlayerStat( int nTeam, TFPlayerStatId_t stat );

	// We need to be ticked once a frame
	void FrameUpdate( );

private:
	struct Stat_t
	{
		int m_nCount;
	};

	typedef const char * (*StatNameFunc_t)( int stat );

	// Collects frame-based stats
	void CollectFrameStats( );
	void CollectStats( );

	int	GetStat( TFStatId_t stat ) const	{ return m_Stats[stat].m_nCount; }
	int	GetTeamStat( int nTeam, TFTeamStatId_t stat ) const	{ return m_TeamStats[nTeam][stat].m_nCount; }
	void WriteHeader( CUtlBuffer &buf, const char *pPrefix, int nCount, StatNameFunc_t func, bool bTerminate = true );
	void WriteStatLine( CUtlBuffer &buf, int nCount, Stat_t *pStats, bool bTerminate = true );
	void WriteAvgStatLine( CUtlBuffer &buf );
	void WriteStats();
	void WriteTeamStats();
	void WritePlayerStats( );
	void EraseFile( const char *pFileName );
	void AppendToFile( const char *pFileName, CUtlBuffer &buf );
	void ComputeFileNames();
	void ClearStats();

	// Compute class-based stats from the player stats
	int ComputeClassStats( int nTeam, TFClass classType, Stat_t *pStats );

	bool	m_bWrittenHeader;
	int		m_nLastWriteTime;
	Stat_t	m_Stats[TF_STAT_COUNT];
	Stat_t	m_TeamStats[MAX_TF_TEAMS][TF_TEAM_STAT_COUNT];
	Stat_t	m_ClassStats[MAX_TF_TEAMS][TFCLASS_CLASS_COUNT][TF_PLAYER_STAT_COUNT];
};


//-----------------------------------------------------------------------------
// Accessor method
//-----------------------------------------------------------------------------
static CTFStats s_TFStats;
ITFStats *TFStats()
{
	return &s_TFStats;
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CTFStats::CTFStats()
{
	ResetStats();
}

//-----------------------------------------------------------------------------
// Clear out the stats + their history
//-----------------------------------------------------------------------------
void CTFStats::ClearStats()
{
	int i;
	for (i = 0; i < TF_STAT_COUNT; ++i)
	{
		SetStat( (TFStatId_t)i, 0 );
	}

	for (i = 0; i < MAX_TF_TEAMS; ++i)
	{
		for (int j = 0; j < TF_TEAM_STAT_COUNT; ++j)
		{
			SetTeamStat(i, (TFTeamStatId_t)j, 0);
		}
	}

	for (int nTeam = 0; nTeam < MAX_TF_TEAMS; ++nTeam)
	{
		for (i = 0; i < TFCLASS_CLASS_COUNT; ++i)
		{
			for (int j = 0; j < TF_PLAYER_STAT_COUNT; ++j)
			{
				m_ClassStats[nTeam][i][(TFPlayerStatId_t)j].m_nCount = 0;
			}
		}
	}

}

//-----------------------------------------------------------------------------
// Clear out the stats + their history
//-----------------------------------------------------------------------------
void CTFStats::ResetStats()
{
	ClearStats();
	m_bWrittenHeader = false;
	m_nLastWriteTime = -9999;
}



//-----------------------------------------------------------------------------
// Inherited from IAutoServerSystem
//-----------------------------------------------------------------------------
void CTFStats::LevelInitPreEntity()
{
	ResetStats();
}


//-----------------------------------------------------------------------------
// Update stats...
//-----------------------------------------------------------------------------
void CTFStats::IncrementStat( TFStatId_t stat, int nIncrement )
{
	m_Stats[stat].m_nCount += nIncrement;
}

void CTFStats::SetStat( TFStatId_t stat, int nAmount )
{
	m_Stats[stat].m_nCount = nAmount;
}

void CTFStats::IncrementTeamStat( int nTeam, TFTeamStatId_t stat, int nIncrement )
{
	m_TeamStats[nTeam][stat].m_nCount += nIncrement;
}

void CTFStats::SetTeamStat( int nTeam, TFTeamStatId_t stat, int nAmount )
{
	m_TeamStats[nTeam][stat].m_nCount = nAmount;
}

void CTFStats::IncrementPlayerStat( CBaseEntity *pEntity, TFPlayerStatId_t stat, int nIncrement )
{
	if (!pEntity->IsPlayer())
		return;

	CBaseTFPlayer *pTFPlayer = static_cast<CBaseTFPlayer*>(pEntity);
	int nTeam = pTFPlayer->GetTeamNumber();
	CPlayerClass *pPlayerClass = pTFPlayer->GetPlayerClass();
	int nClass = pPlayerClass ? pPlayerClass->GetTFClass() : TFCLASS_UNDECIDED;

	m_ClassStats[nTeam][nClass][stat].m_nCount += nIncrement;
}

void CTFStats::ClearPlayerStat( int nTeam, TFPlayerStatId_t stat )
{
	for (int i = 0; i < TFCLASS_CLASS_COUNT; ++i)
	{
		m_ClassStats[nTeam][i][stat].m_nCount = 0;
	}
}


//-----------------------------------------------------------------------------
// We need to be ticked once a frame
//-----------------------------------------------------------------------------
void CTFStats::CollectFrameStats( )
{
	// This collects a bunch of polled stats so we don't have to pollute
	// a bunch of code
	for (int i = 0; i < MAX_TF_TEAMS; ++i)
	{
		CTFTeam *pTeam = GetGlobalTFTeam(i);
		for (int j = pTeam->GetNumPlayers(); --j >= 0; )
		{
			CBaseTFPlayer *pPlayer = static_cast<CBaseTFPlayer*>(pTeam->GetPlayer(j));
			if (!pPlayer)
				continue;

			int nTimeMS = 1000 * gpGlobals->frametime;

			switch( pPlayer->GetActivity() )
			{
			case ACT_IDLE:
				IncrementPlayerStat( pPlayer, TF_PLAYER_STAT_ANIMATION_IDLE, nTimeMS );
				break;

			case ACT_WALK:
				IncrementPlayerStat( pPlayer, TF_PLAYER_STAT_ANIMATION_WALKING, nTimeMS );
				break;

			case ACT_RUN:
				IncrementPlayerStat( pPlayer, TF_PLAYER_STAT_ANIMATION_RUNNING, nTimeMS );
				break;

			case ACT_JUMP:
				IncrementPlayerStat( pPlayer, TF_PLAYER_STAT_ANIMATION_JUMPING, nTimeMS );
				break;

			case ACT_CROUCHIDLE:
				IncrementPlayerStat( pPlayer, TF_PLAYER_STAT_ANIMATION_CROUCHING, nTimeMS );
				break;

			default:
				IncrementPlayerStat( pPlayer, TF_PLAYER_STAT_ANIMATION_OTHER, nTimeMS );
				break;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// We need to be ticked once a frame
//-----------------------------------------------------------------------------
void CTFStats::CollectStats( )
{
	// This collects a bunch of polled stats so we don't have to pollute
	// a bunch of code
	bool bAtLeastOnePlayer = false;
	for (int i = 0; i < MAX_TF_TEAMS; ++i)
	{
		CTFTeam *pTeam = GetGlobalTFTeam(i);

		for ( int iClass=0; iClass < TFCLASS_CLASS_COUNT; iClass++ )
		{
			SetTeamStat( i, (TFTeamStatId_t)iClass, pTeam->GetNumOfClass( (TFClass)iClass ) );
		}

		SetTeamStat( i, TF_TEAM_STAT_PLAYER_COUNT, pTeam->GetNumPlayers() );

		if ( GetStat( TF_STAT_FERRY_CONTROL ) == i )
		{
			IncrementTeamStat( i, TF_TEAM_STAT_FERRY_CONTROL_TIME, TF_STATS_COLLECTION_TIME ); 
		}

		ClearPlayerStat( i, TF_PLAYER_STAT_OBJECT_COUNT );
		ClearPlayerStat( i, TF_PLAYER_STAT_RESOURCES_CARRIED );
		ClearPlayerStat( i, TF_PLAYER_STAT_CURRENT_OBJECT_VALUE );
		ClearPlayerStat( i, TF_PLAYER_STAT_PLAYER_COUNT );

		bool bClassEncountered[TFCLASS_CLASS_COUNT];
		memset( bClassEncountered, 0, TFCLASS_CLASS_COUNT * sizeof(bool) );
		for (int j = pTeam->GetNumPlayers(); --j >= 0; )
		{
			bAtLeastOnePlayer = true;

			CBaseTFPlayer *pPlayer = static_cast<CBaseTFPlayer*>(pTeam->GetPlayer(j));
			IncrementPlayerStat( pPlayer, TF_PLAYER_STAT_OBJECT_COUNT, pPlayer->GetObjectCount() );
			IncrementPlayerStat( pPlayer, TF_PLAYER_STAT_RESOURCES_CARRIED, pPlayer->GetBankResources() );
			IncrementPlayerStat( pPlayer, TF_PLAYER_STAT_PLAYER_COUNT, 1 );
			IncrementPlayerStat( pPlayer, TF_PLAYER_STAT_PLAYER_SECONDS, 1 );

			CPlayerClass *pPlayerClass = pPlayer->GetPlayerClass();
			int nClass = pPlayerClass ? pPlayerClass->GetTFClass() : TFCLASS_UNDECIDED;

			if (!bClassEncountered[nClass])
			{
				IncrementPlayerStat( pPlayer, TF_PLAYER_STAT_EXISTING_SECONDS, 1 );
				bClassEncountered[nClass] = true;
			}

			// Count up the cost of all current objects..
			int nCost = 0;
			int pObjectCount[OBJ_LAST];
			memset( pObjectCount, 0, OBJ_LAST * sizeof(int) );
			for (int k = pPlayer->GetObjectCount(); --k >= 0; )
			{
				CBaseObject *pObject = pPlayer->GetObject(k);
				if (pObject)
				{
					int nType = pObject->GetType();
					nCost += CalculateObjectCost( nType, pObjectCount[nType], pPlayer->GetTeamNumber(), false );
					++pObjectCount[nType];
				}
			}
			IncrementPlayerStat( pPlayer, TF_PLAYER_STAT_CURRENT_OBJECT_VALUE, nCost );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
void CTFStats::ComputeFileNames()
{
	Q_snprintf( s_pStatFile, sizeof( s_pStatFile ), "%s.txt", TF_STAT_FILE );
	Q_snprintf( s_pTeamStatFile, sizeof( s_pTeamStatFile ), "%s.txt", TF_TEAM_STAT_FILE );
	Q_snprintf( s_pPlayerStatFile, sizeof( s_pPlayerStatFile ), "%s.txt", TF_PLAYER_STAT_FILE );
}


//-----------------------------------------------------------------------------
// File access.
//-----------------------------------------------------------------------------
void CTFStats::EraseFile( const char *pFileName )
{
	filesystem->RemoveFile( pFileName, "GAME" );
}

void CTFStats::AppendToFile( const char *pFileName, CUtlBuffer &buf )
{
	FileHandle_t fh = filesystem->Open( pFileName, "a", "LOGDIR" );
	if (fh != FILESYSTEM_INVALID_HANDLE)
	{
		filesystem->Write( buf.Base(), buf.TellPut(), fh );
		filesystem->Close( fh );
	}
}


//-----------------------------------------------------------------------------
// Write out a header.
//-----------------------------------------------------------------------------
void CTFStats::WriteHeader( CUtlBuffer &buf, const char *pPrefix, int nCount, StatNameFunc_t func, bool bTerminate )
{
	for (int i = 0; i < nCount-1; ++i)
	{
		buf.Printf("%s %s\t", pPrefix, func(i) );
	}

	buf.Printf( bTerminate ? "%s %s\n" : "%s %s\t", pPrefix, func(nCount-1) );
}

void CTFStats::WriteStatLine( CUtlBuffer &buf, int nCount, Stat_t *pStats, bool bTerminate )
{
	for (int i = 0; i < nCount-1; ++i)
	{
		buf.Printf("%d\t", pStats[i].m_nCount );
	}

	buf.Printf( bTerminate ? "%d\n" : "%d\t", pStats[nCount-1].m_nCount );
}

void CTFStats::WriteAvgStatLine( CUtlBuffer &buf )
{
	int nTeam, i, j;
	for ( nTeam = 0; nTeam < MAX_TF_TEAMS; ++nTeam )
	{
		for ( i = 0; i < TF_PLAYER_STAT_COUNT; ++i )
		{
			for ( j = 1; j < TFCLASS_CLASS_COUNT; ++j )
			{
				if (!GetTFClassInfo(j)->m_pCurrentlyActive)
					continue;

				buf.Printf("%d\t", m_ClassStats[nTeam][j][i].m_nCount);
			}
		}
	}

	// Blat out that last tab and replace with a \n
	buf.SeekPut( CUtlBuffer::SEEK_CURRENT, -1 );
	buf.Printf("\n");
}

//-----------------------------------------------------------------------------
// Write out total stats...
//-----------------------------------------------------------------------------
void CTFStats::WriteStats()
{
	CUtlBuffer buf( 0, 1024, CUtlBuffer::TEXT_BUFFER );

	if (!m_bWrittenHeader)
	{
		EraseFile( s_pStatFile );
		WriteHeader( buf, "", TF_STAT_COUNT, GetStatString );
	}

	WriteStatLine( buf, TF_STAT_COUNT, m_Stats );
	AppendToFile( s_pStatFile, buf );
}


//-----------------------------------------------------------------------------
// Write out total stats...
//-----------------------------------------------------------------------------
void CTFStats::WriteTeamStats()
{
	CUtlBuffer buf( 0, 1024, CUtlBuffer::TEXT_BUFFER );

	int i,j;
	if (!m_bWrittenHeader)
	{
		EraseFile( s_pTeamStatFile );
		for ( i = 0; i < TF_TEAM_STAT_COUNT; ++i )
		{
			for ( j = 0; j < MAX_TF_TEAMS; ++j )
			{
				buf.Printf("Team %d %s\t", j, GetTeamStatString(i) );
			}
		}
		// Blat out that last tab and replace with a \n
		buf.SeekPut( CUtlBuffer::SEEK_CURRENT, -1 );
		buf.Printf("\n");
	}

	for ( i = 0; i < TF_TEAM_STAT_COUNT; ++i )
	{
		for ( j = 0; j < MAX_TF_TEAMS; ++j )
		{
			buf.Printf("%d\t", m_TeamStats[j][i].m_nCount );
		}
	}

	// Blat out that last tab and replace with a \n
	buf.SeekPut( CUtlBuffer::SEEK_CURRENT, -1 );
	buf.Printf("\n");

	AppendToFile( s_pTeamStatFile, buf );
}


//-----------------------------------------------------------------------------
// Write out total stats...
//-----------------------------------------------------------------------------
void CTFStats::WritePlayerStats( )
{
	CUtlBuffer buf( 0, 1024, CUtlBuffer::TEXT_BUFFER );

	int i, j, nTeam;
	if (!m_bWrittenHeader)
	{
		EraseFile( s_pPlayerStatFile );
		for ( nTeam = 0; nTeam < MAX_TF_TEAMS; ++nTeam )
		{
			for ( i = 0; i < TF_PLAYER_STAT_COUNT; ++i )
			{
				for ( j = 1; j < TFCLASS_CLASS_COUNT; ++j )
				{
					if (!GetTFClassInfo(j)->m_pCurrentlyActive)
						continue;

					buf.Printf("Team %d %s %s\t", nTeam, GetTFClassInfo( j )->m_pClassName, GetPlayerStatString(i) );
				}
			}
		}

		// Blat out that last tab and replace with a \n
		buf.SeekPut( CUtlBuffer::SEEK_CURRENT, -1 );
		buf.Printf("\n");
	}

	WriteAvgStatLine( buf );

	AppendToFile( s_pPlayerStatFile, buf );
}


//-----------------------------------------------------------------------------
// We need to be ticked once a frame
//-----------------------------------------------------------------------------
void CTFStats::FrameUpdatePostEntityThink( )
{
	if (!tf_stats.GetBool())
		return;

	// Don't stat gather during waiting acts
	if ( CurrentActIsAWaitingAct() )
		return;

	CollectFrameStats();	

	// NOTE: We could keep track of the history here if we wanted for later
	// display when the map ends

	// Record the history every so often
	if (gpGlobals->curtime - m_nLastWriteTime < TF_STATS_COLLECTION_TIME)
		return;

	if (!m_bWrittenHeader)
	{
		ComputeFileNames();
	}

	CollectStats();	
	WriteStats();
	WriteTeamStats();
	WritePlayerStats();
	ClearStats();

	// By this point, we've written the header for each file
	m_bWrittenHeader = true;

	m_nLastWriteTime = gpGlobals->curtime;
}

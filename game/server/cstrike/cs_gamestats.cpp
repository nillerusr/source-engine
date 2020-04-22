//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CS game stats
//
// $NoKeywords: $
//=============================================================================//

// Some tricky business here - we don't want to include the precompiled header for the statreader
// and trying to #ifdef it out does funky things like ignoring the #endif. Define our header file
// separately and include it based on the switch

#include "cbase.h"

#include <tier0/platform.h>
#include "cs_gamerules.h"
#include "cs_gamestats.h"
#include "weapon_csbase.h"
#include "props.h"
#include "cs_achievement_constants.h"
#include "../../shared/cstrike/weapon_c4.h"

#include <time.h>
#include "filesystem.h"
#include "bot_util.h"
#include "cdll_int.h"
#include "steamworks_gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

float	g_flGameStatsUpdateTime = 0.0f;
short	g_iTerroristVictories[CS_NUM_LEVELS];
short	g_iCounterTVictories[CS_NUM_LEVELS];
short	g_iWeaponPurchases[WEAPON_MAX];

short	g_iAutoBuyPurchases = 0;
short	g_iReBuyPurchases = 0;
short	g_iAutoBuyM4A1Purchases = 0;
short	g_iAutoBuyAK47Purchases = 0;
short	g_iAutoBuyFamasPurchases = 0;
short	g_iAutoBuyGalilPurchases = 0;
short	g_iAutoBuyVestHelmPurchases = 0;
short	g_iAutoBuyVestPurchases = 0;

struct PropModelStats_t
{
	const char* szPropModelName;
	CSStatType_t statType;
} PropModelStatsTableInit[] =
{
	{ "models/props/cs_office/computer_caseb.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/computer_monitor.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/phone.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/projector.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/TV_plasma.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/computer_keyboard.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/radio.mdl", CSSTAT_PROPSBROKEN_OFFICERADIO },
	{ "models/props/cs_office/trash_can.mdl", CSSTAT_PROPSBROKEN_OFFICEJUNK },
	{ "models/props/cs_office/file_box.mdl", CSSTAT_PROPSBROKEN_OFFICEJUNK },
	{ "models/props_junk/watermelon01.mdl", CSSTAT_PROPSBROKEN_ITALY_MELON },
	// 	models/props/de_inferno/claypot01.mdl
	// 	models/props/de_inferno/claypot02.mdl
	//	models/props/de_dust/grainbasket01c.mdl
	//	models/props_junk/wood_crate001a.mdl 
	//	models/props/cs_office/file_box_p1.mdl 
};


struct ServerStats_t
{
	int achievementId;
	int statId;
	int roundRequirement;
	int matchRequirement;
	const char* mapFilter;

	bool IsMet(int roundStat, int matchStat)
	{
		return roundStat >= roundRequirement && matchStat >= matchRequirement;
	}
} ServerStatBasedAchievements[] =
{
	{ CSBreakWindows,			    CSSTAT_NUM_BROKEN_WINDOWS,		AchievementConsts::BreakWindowsInOfficeRound_Windows,	0,	"cs_office" },
	{ CSBreakProps,			        CSSTAT_PROPSBROKEN_ALL,			AchievementConsts::BreakPropsInRound_Props,				0,	NULL },
	{ CSUnstoppableForce,		    CSSTAT_KILLS,					AchievementConsts::UnstoppableForce_Kills,				0,	NULL },
	{ CSHeadshotsInRound,	        CSSTAT_KILLS_HEADSHOT,			AchievementConsts::HeadshotsInRound_Kills,				0,	NULL },
    { CSDominationOverkillsMatch,	CSSTAT_DOMINATION_OVERKILLS,	0,				                                        10,	NULL },
};

//=============================================================================
// HPE_BEGIN:
// [Forrest] Allow nemesis/revenge to be turned off for a server
//=============================================================================
static void SvNoNemesisChangeCallback( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( var.IsValid() && var.GetBool() )
	{
		// Clear all nemesis relationships.
		for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
		{
			CCSPlayer *pTemp = ToCSPlayer( UTIL_PlayerByIndex( i ) );
			if ( pTemp )
			{
				pTemp->RemoveNemesisRelationships();
			}
		}
	}
}

ConVar sv_nonemesis( "sv_nonemesis", "0", 0, "Disable nemesis and revenge.", SvNoNemesisChangeCallback );
//=============================================================================
// HPE_END
//=============================================================================

int GetCSLevelIndex( const char *pLevelName )
{
	for ( int i = 0; MapName_StatId_Table[i].statWinsId != CSSTAT_UNDEFINED; i ++ )
	{
		if ( Q_strcmp( pLevelName, MapName_StatId_Table[i].szMapName ) == 0 )
			return i;
	}

	return -1;
}

ConVar sv_debugroundstats( "sv_debugroundstats", "0", 0, "A temporary variable that will print extra information about stats upload which may be useful in debugging any problems." );

//=============================================================================
// HPE_BEGIN:
// [menglish] Addition of CCS_GameStats class
//=============================================================================
 
CCSGameStats CCS_GameStats;
CCSGameStats::StatContainerList_t* CCSGameStats::s_StatLists = new CCSGameStats::StatContainerList_t();

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  :  - 
//-----------------------------------------------------------------------------
CCSGameStats::CCSGameStats()
{
	gamestats = this;
	Clear();
	m_fDisseminationTimerLow = m_fDisseminationTimerHigh = 0.0f;

	// create table for mapping prop models to stats
	for ( int i = 0; i < ARRAYSIZE(PropModelStatsTableInit); ++i)
	{
		m_PropStatTable.Insert(PropModelStatsTableInit[i].szPropModelName, PropModelStatsTableInit[i].statType);
	}

	m_numberOfRoundsForDirectAverages = 0;
	m_numberOfTerroristEntriesForDirectAverages = 0;
	m_numberOfCounterTerroristEntriesForDirectAverages = 0;

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
// Input  :  - 
//-----------------------------------------------------------------------------
CCSGameStats::~CCSGameStats()
{
	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: Clear out game stats
// Input  :  - 
//-----------------------------------------------------------------------------
void CCSGameStats::Clear( void )
{
	m_PlayerSnipedPosition.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGameStats::Init( void )
{
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "break_prop" );
	ListenForGameEvent( "player_decal");
	ListenForGameEvent( "hegrenade_detonate");

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::PostInit( void )
{

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::LevelShutdownPreClearSteamAPIContext( void )
{
	// If we have any unsent round stats we better send them now or we'll lose them
	UploadRoundStats();

	GetSteamWorksSGameStatsUploader().EndSession();
}

extern double g_rowCommitTime;
extern double g_rowWriteTime;

void CCSGameStats::UploadRoundStats( void )
{
	CFastTimer totalTimer, purchasesAndDeathsStatTimer, weaponsStatBuildTimer, weaponsStatSubmitTimer, submitTimer, cleanupTimer;

	// Prevent uploading empty data
	if ( !m_bInRound )
		return;

	if ( sv_noroundstats.GetBool() )
	{
		if ( sv_debugroundstats.GetBool() )
		{
			Msg( "sv_noroundstats is enabled. Not uploading round stats.\n" );
		}

		m_MarketPurchases.PurgeAndDeleteElements();
		m_WeaponData.PurgeAndDeleteElements();
		m_DeathData.PurgeAndDeleteElements();
		return;
	}

	m_bInRound = false;

	KeyValues *pKV = new KeyValues( "basedata" );
	if ( !pKV )
		return;

	totalTimer.Start();
	purchasesAndDeathsStatTimer.Start();

	const char *pzMapName = gpGlobals->mapname.ToCStr();
	pKV->SetString( "MapID", pzMapName );

	for ( int k=0 ; k < m_MarketPurchases.Count() ; ++k )
		SubmitStat( m_MarketPurchases[ k ] );

	for ( int k=0 ; k < m_DeathData.Count() ; ++k )
		SubmitStat( m_DeathData[ k ] );

	purchasesAndDeathsStatTimer.End();
	weaponsStatBuildTimer.Start();

	// Now add the weapon stats that HPE collected
	for (int iWeapon = 0; iWeapon < WEAPON_MAX; ++iWeapon)
	{
		CCSWeaponInfo* pInfo = GetWeaponInfo( (CSWeaponID)iWeapon );
		if ( !pInfo )
			continue;

		const char* pWeaponName = pInfo->szClassName;
		if ( !pWeaponName || !pWeaponName[0] || ( m_weaponStats[iWeapon][0].shots == 0 && m_weaponStats[iWeapon][1].shots == 0 ) )
			continue;


		m_WeaponData.AddToTail( new SCSSWeaponData( pWeaponName, m_weaponStats[iWeapon][0] ) );

		// Now add the bot data if we're collecting detailled data
		if ( GetSteamWorksSGameStatsUploader().IsCollectingDetails() )
		{
			char pWeaponNameModified[64];
			V_snprintf( pWeaponNameModified, ARRAYSIZE(pWeaponNameModified), "%s_bot", pWeaponName );
			m_WeaponData.AddToTail( new SCSSWeaponData( pWeaponNameModified, m_weaponStats[iWeapon][1] ) );
		}
	}

	weaponsStatBuildTimer.End();
	weaponsStatSubmitTimer.Start();

	for ( int k=0 ; k < m_WeaponData.Count() ; ++k )
		SubmitStat( m_WeaponData[ k ] );

	weaponsStatSubmitTimer.End();

	// Perform the actual submission
	submitTimer.Start();
	SubmitGameStats( pKV );
	submitTimer.End();

	int iPurchases, iDeathData, iWeaponData;
	int listCount = s_StatLists->Count();
	iPurchases = m_MarketPurchases.Count();
	iDeathData = m_DeathData.Count();
	iWeaponData = m_WeaponData.Count();

	// Clear out the per round stats
	cleanupTimer.Start();
	m_MarketPurchases.Purge();
	m_WeaponData.Purge();
	m_DeathData.Purge();
	pKV->deleteThis();
	cleanupTimer.End();

	totalTimer.End();

	if ( sv_debugroundstats.GetBool() )
	{
		Msg( "**** ROUND STAT DEBUG ****\n" );
		Msg( "UploadRoundStats completed. %.3f msec. Breakdown:\n a: %.3f msec\n b: %.3f msec\n c: %.3f msec\n d: %.3f msec\n e: %.3f msec\n objects: %d %d %d %d\n commit: %.3fms\n write: %.3fms.\n\n",
			totalTimer.GetDuration().GetMillisecondsF(),
			purchasesAndDeathsStatTimer.GetDuration().GetMillisecondsF(),
			weaponsStatBuildTimer.GetDuration().GetMillisecondsF(),
			weaponsStatSubmitTimer.GetDuration().GetMillisecondsF(),
			submitTimer.GetDuration().GetMillisecondsF(),
			cleanupTimer.GetDuration().GetMillisecondsF(),
			iPurchases, iDeathData, iWeaponData, listCount,
			g_rowCommitTime, g_rowWriteTime);
	}
}

#ifdef _DEBUG
CON_COMMAND ( teststats, "Test command" )
{
	CFastTimer totalTimer;
	double uploadTime = 0.0f;
	g_rowCommitTime = 0.0f;
	g_rowWriteTime = 0.0f;


	for( int i = 0; i < 1000; i++ )
	{
		KeyValues *pKV = new KeyValues( "basedata" );
		if ( !pKV )
			return;

		pKV->SetName( "foobartest" );
		pKV->SetUint64( "test1", 1234 );
		pKV->SetUint64( "test2", 1234 );
		pKV->SetUint64( "test3", 1234 );
		pKV->SetUint64( "test4", 1234 );
		pKV->SetString( "test5", "TEST1234567890TEST1234567890TEST!");
		/*pKV->SetString( "test6", "TEST1234567890TEST1234567890TEST!");
		pKV->SetString( "test7", "TEST1234567890TEST1234567890TEST!");
		pKV->SetString( "test8", "TEST1234567890TEST1234567890TEST!");
		pKV->SetString( "test9", "TEST1234567890TEST1234567890TEST!");*/

		totalTimer.Start();
		GetSteamWorksSGameStatsUploader().AddStatsForUpload( pKV, args.ArgC() == 1 );
		totalTimer.End();

		uploadTime += totalTimer.GetDuration().GetMillisecondsF();
	}

	Msg( "teststats took %.3f msec   commit: %.3fms   write: %.3fms.\n", uploadTime, g_rowCommitTime, g_rowWriteTime );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::SubmitGameStats( KeyValues *pKV )
{
	int listCount = s_StatLists->Count();
	for( int i=0; i < listCount; ++i )
	{
		// Create a master key value that has stats everybody should share (map name, session ID, etc)
		(*s_StatLists)[i]->SendData(pKV);
		(*s_StatLists)[i]->Clear();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_ShotFired( CBasePlayer *pPlayer, CBaseCombatWeapon* pWeapon )
{
	Assert( pPlayer );
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );

	CWeaponCSBase* pCSWeapon = dynamic_cast< CWeaponCSBase * >(pWeapon);

    //=============================================================================
    // HPE_BEGIN:
    // [dwenger] adding tracking for weapon used fun fact
    //=============================================================================

    if ( pCSPlayer )
    {
        // [dwenger] Update the player's tracking of which weapon type they fired
        pCSPlayer->PlayerUsedFirearm( pWeapon );

        //=============================================================================
        // HPE_END
        //=============================================================================

	    IncrementStat( pCSPlayer, CSSTAT_SHOTS_FIRED, 1 );
	    // Increment the individual weapon
	    if( pCSWeapon )
	    {
			CSWeaponID weaponId = pCSWeapon->GetWeaponID();
		    for (int i = 0; WeaponName_StatId_Table[i].shotStatId != CSSTAT_UNDEFINED; ++i)
		    {
			    if ( WeaponName_StatId_Table[i].weaponId == weaponId )
			    {
				    IncrementStat( pCSPlayer, WeaponName_StatId_Table[i].shotStatId, 1 );
					break;
			    }
		    }

			int iType = pCSPlayer->IsBot();
			++m_weaponStats[weaponId][iType].shots;
	    }
    }
}

void CCSGameStats::Event_ShotHit( CBasePlayer *pPlayer, const CTakeDamageInfo &info )
{
	Assert( pPlayer );
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );

	IncrementStat( pCSPlayer, CSSTAT_SHOTS_HIT, 1 );

	CBaseEntity *pInflictor = info.GetInflictor();

	if ( pInflictor )
	{
		if ( pInflictor == pPlayer )
		{			
			if ( pPlayer->GetActiveWeapon() )
			{
				CWeaponCSBase* pCSWeapon = dynamic_cast< CWeaponCSBase * >(pPlayer->GetActiveWeapon());
				if (pCSWeapon)
				{
					CSWeaponID weaponId = pCSWeapon->GetWeaponID();
					for (int i = 0; WeaponName_StatId_Table[i].shotStatId != CSSTAT_UNDEFINED; ++i)
					{
						if ( WeaponName_StatId_Table[i].weaponId == weaponId )
						{
							IncrementStat( pCSPlayer, WeaponName_StatId_Table[i].hitStatId, 1 );
							break;
						}
					}
				}
			}
		}
	}
}
void CCSGameStats::Event_PlayerKilled( CBasePlayer *pPlayer, const CTakeDamageInfo &info )
{
	Assert( pPlayer );
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
	
	IncrementStat( pCSPlayer, CSSTAT_DEATHS, 1 );
}

void CCSGameStats::Event_PlayerSprayedDecal( CCSPlayer* pPlayer )
{
    IncrementStat( pPlayer, CSSTAT_DECAL_SPRAYS, 1 );
}

void CCSGameStats::Event_PlayerKilled_PreWeaponDrop( CBasePlayer *pPlayer, const CTakeDamageInfo &info )
{
	Assert( pPlayer );
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
	CCSPlayer *pAttacker = ToCSPlayer( info.GetAttacker() );
	bool victimZoomed = ( pCSPlayer->GetFOV() != pCSPlayer->GetDefaultFOV() );

	if (victimZoomed)
	{
		IncrementStat(pAttacker, CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER, 1);
	}


	//Check for knife fight
	if (pAttacker && pCSPlayer && pAttacker == info.GetInflictor() && pAttacker->GetTeamNumber() != pCSPlayer->GetTeamNumber())
	{
		CWeaponCSBase* attackerWeapon = pAttacker->GetActiveCSWeapon();
		CWeaponCSBase* victimWeapon = pCSPlayer->GetActiveCSWeapon();

		if (attackerWeapon && victimWeapon)
		{
			CSWeaponID attackerWeaponID = attackerWeapon->GetWeaponID();
			CSWeaponID victimWeaponID = victimWeapon->GetWeaponID();

			if (attackerWeaponID == WEAPON_KNIFE && victimWeaponID == WEAPON_KNIFE)
			{
				IncrementStat(pAttacker, CSSTAT_KILLS_KNIFE_FIGHT, 1);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_BombPlanted( CCSPlayer* pPlayer)
{
    IncrementStat( pPlayer, CSSTAT_NUM_BOMBS_PLANTED, 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_BombDefused( CCSPlayer* pPlayer)
{

    IncrementStat( pPlayer, CSSTAT_NUM_BOMBS_DEFUSED, 1 );
	IncrementStat( pPlayer, CSSTAT_OBJECTIVES_COMPLETED, 1 );
	if( pPlayer && pPlayer->HasDefuser() )
	{
		IncrementStat( pPlayer, CSSTAT_BOMBS_DEFUSED_WITHKIT, 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Increment terrorist team stat
//-----------------------------------------------------------------------------
void CCSGameStats::Event_BombExploded( CCSPlayer* pPlayer )
{
	IncrementStat( pPlayer, CSSTAT_OBJECTIVES_COMPLETED, 1 );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_HostageRescued( CCSPlayer* pPlayer)
{
    IncrementStat( pPlayer, CSSTAT_NUM_HOSTAGES_RESCUED, 1 );
}

//-----------------------------------------------------------------------------
// Purpose: Increment counter-terrorist team stat
//-----------------------------------------------------------------------------
void CCSGameStats::Event_AllHostagesRescued()
{
	IncrementTeamStat( TEAM_CT, CSSTAT_OBJECTIVES_COMPLETED, 1 );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_WindowShattered( CBasePlayer *pPlayer)
{
    Assert( pPlayer );
    CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );

    IncrementStat( pCSPlayer, CSSTAT_NUM_BROKEN_WINDOWS, 1 );
}


void CCSGameStats::Event_BreakProp( CCSPlayer* pPlayer, CBreakableProp *pProp )
{
	if (!pPlayer)
		return;

	DevMsg("Player %s broke a %s (%i)\n", pPlayer->GetPlayerName(), pProp->GetModelName().ToCStr(), pProp->entindex());

	int iIndex = m_PropStatTable.Find(pProp->GetModelName().ToCStr());
	if (m_PropStatTable.IsValidIndex(iIndex))
	{
		IncrementStat(pPlayer, m_PropStatTable[iIndex], 1);
	}
	IncrementStat(pPlayer, CSSTAT_PROPSBROKEN_ALL, 1);
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::UpdatePlayerRoundStats(int winner)
{	
    int mapIndex = GetCSLevelIndex(gpGlobals->mapname.ToCStr());
    CSStatType_t mapStatWinIndex  = CSSTAT_UNDEFINED, mapStatRoundIndex = CSSTAT_UNDEFINED;

    if ( mapIndex != -1 )
    {
        mapStatWinIndex = MapName_StatId_Table[mapIndex].statWinsId;
		mapStatRoundIndex = MapName_StatId_Table[mapIndex].statRoundsId;
    }

	// increment the team specific stats
	IncrementTeamStat( winner, CSSTAT_ROUNDS_WON, 1 );
	if ( mapStatWinIndex != CSSTAT_UNDEFINED )
	{
		IncrementTeamStat( winner, mapStatWinIndex, 1 );
	}
	if ( CSGameRules()->IsPistolRound() )
	{
		IncrementTeamStat( winner, CSSTAT_PISTOLROUNDS_WON, 1 );
	}
	IncrementTeamStat( TEAM_TERRORIST, CSSTAT_ROUNDS_PLAYED, 1 );
	IncrementTeamStat( TEAM_CT, CSSTAT_ROUNDS_PLAYED, 1 );
	IncrementTeamStat( TEAM_TERRORIST, mapStatRoundIndex, 1 );
	IncrementTeamStat( TEAM_CT, mapStatRoundIndex, 1 );

	for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayerIndex ) );
		if ( pPlayer && pPlayer->IsConnected() )
		{
			if ( winner == TEAM_CT )
			{
				IncrementStat( pPlayer, CSSTAT_CT_ROUNDS_WON, 1, true );
			}
			else if ( winner == TEAM_TERRORIST )
			{
				IncrementStat( pPlayer, CSSTAT_T_ROUNDS_WON, 1, true );
			}

			if ( winner == TEAM_CT || winner == TEAM_TERRORIST )
			{
				// Increment the win stats if this player is on the winning team
				if ( pPlayer->GetTeamNumber() == winner )
				{
					IncrementStat( pPlayer, CSSTAT_ROUNDS_WON, 1, true );

					if ( CSGameRules()->IsPistolRound() )
					{
						IncrementStat( pPlayer, CSSTAT_PISTOLROUNDS_WON, 1, true );
					}

					if ( mapStatWinIndex != CSSTAT_UNDEFINED )
					{
						IncrementStat( pPlayer, mapStatWinIndex, 1, true );
					}
				}

				IncrementStat( pPlayer, CSSTAT_ROUNDS_PLAYED, 1 );
				if ( mapStatWinIndex != CSSTAT_UNDEFINED )
				{
					IncrementStat( pPlayer, mapStatRoundIndex, 1, true );
				}

				// set the play time for the round
				IncrementStat( pPlayer, CSSTAT_PLAYTIME, (int)CSGameRules()->GetRoundElapsedTime(), true );
			}
		}
	}

	// send a stats update to all players
	for ( int iPlayerIndex = 1; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayerIndex ) );
		if ( pPlayer && pPlayer->IsConnected())
		{
			SendStatsToPlayer(pPlayer, CSSTAT_PRIORITY_ENDROUND);
		}
	}
}

void CCSGameStats::SendRollingStatsAveragesToAllPlayers()
{
	//The most stats we can send at once is the max message size minus the header, divided by the total size of each stat.
	const int maxStatsPerMessage = (MAX_USER_MSG_DATA - sizeof(CSStatType_t)) / (3 * sizeof(float));

	const int numMessagesNeeded = ((CSSTAT_MAX - CSSTAT_FIRST) / maxStatsPerMessage) + 1;

	for (int batchIndex = 0 ; batchIndex < numMessagesNeeded ; ++batchIndex)
	{
		int firstStatInThisBatch = (batchIndex * maxStatsPerMessage) + CSSTAT_FIRST;
		int lastMessageInThisBatch = firstStatInThisBatch + maxStatsPerMessage - 1;

		CRecipientFilter filter;
		filter.AddAllPlayers();
		UserMessageBegin( filter, "MatchStatsUpdate" );  

		WRITE_SHORT(firstStatInThisBatch);
		
		for ( int iStat = firstStatInThisBatch; iStat < CSSTAT_MAX && iStat <= lastMessageInThisBatch; ++iStat )
		{
			WRITE_FLOAT(m_rollingTStatAverages.m_fStat[iStat]);
			WRITE_FLOAT(m_rollingCTStatAverages.m_fStat[iStat]);
			WRITE_FLOAT(m_rollingPlayerStatAverages.m_fStat[iStat]);
		}

		MessageEnd();
	}
}


void CCSGameStats::SendDirectStatsAveragesToAllPlayers()
{
	//The most stats we can send at once is the max message size minus the header, divided by the total size of each stat.
	const int maxStatsPerMessage = (MAX_USER_MSG_DATA - sizeof(CSStatType_t)) / (3 * sizeof(float));

	const int numMessagesNeeded = ((CSSTAT_MAX - CSSTAT_FIRST) / maxStatsPerMessage) + 1;

	for (int batchIndex = 0 ; batchIndex < numMessagesNeeded ; ++batchIndex)
	{
		int firstStatInThisBatch = (batchIndex * maxStatsPerMessage) + CSSTAT_FIRST;
		int lastMessageInThisBatch = firstStatInThisBatch + maxStatsPerMessage - 1;

		CRecipientFilter filter;
		filter.AddAllPlayers();
		UserMessageBegin( filter, "MatchStatsUpdate" );  

		WRITE_SHORT(firstStatInThisBatch);

		for ( int iStat = firstStatInThisBatch; iStat < CSSTAT_MAX && iStat <= lastMessageInThisBatch; ++iStat )
		{
			WRITE_FLOAT(m_directTStatAverages.m_fStat[iStat]);
			WRITE_FLOAT(m_directCTStatAverages.m_fStat[iStat]);
			WRITE_FLOAT(m_directPlayerStatAverages.m_fStat[iStat]);
		}

		MessageEnd();
	}
}

void CCSGameStats::ComputeRollingStatAverages()
{
    int numPlayers = 0;
    int numCTs = 0;
    int numTs = 0;

    RoundStatsRollingAverage_t currentRoundTStatsAverage;
    RoundStatsRollingAverage_t currentRoundCTStatsAverage;
    RoundStatsRollingAverage_t currentRoundPlayerStatsAverage;

    currentRoundTStatsAverage.Reset();
    currentRoundCTStatsAverage.Reset();
    currentRoundPlayerStatsAverage.Reset();
    
    //for ( int iStat = CSSTAT_FIRST; iStat < CSSTAT_MAX; ++iStat )
    {
        for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
        {
            CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayerIndex ) );
            if ( pPlayer && pPlayer->IsConnected())
            {
                StatsCollection_t &roundStats = m_aPlayerStats[pPlayer->entindex()].statsCurrentRound;

                int teamNumber = pPlayer->GetTeamNumber();
                if (teamNumber == TEAM_CT)
                {
                    numCTs++;                
                    numPlayers++;
                    currentRoundCTStatsAverage += roundStats;
                    currentRoundPlayerStatsAverage += roundStats;
                }
                else if (teamNumber == TEAM_TERRORIST)
                {
                    numTs++;
                    numPlayers++;
                    currentRoundTStatsAverage += roundStats;
                    currentRoundPlayerStatsAverage += roundStats;
                }
            }
        }

        if (numTs > 0)
        {
            currentRoundTStatsAverage /= numTs;
        }

        if (numCTs > 0)
        {
            currentRoundCTStatsAverage /= numCTs;
        }

        if (numPlayers > 0)
        {
            currentRoundPlayerStatsAverage /= numPlayers;
        }

        m_rollingTStatAverages.RollDataSetIntoAverage(currentRoundTStatsAverage);
        m_rollingCTStatAverages.RollDataSetIntoAverage(currentRoundCTStatsAverage);
        m_rollingPlayerStatAverages.RollDataSetIntoAverage(currentRoundPlayerStatsAverage);
    }
}


void CCSGameStats::ComputeDirectStatAverages()
{		
	m_numberOfRoundsForDirectAverages++;

	m_directCTStatAverages.Reset();
	m_directTStatAverages.Reset();
	m_directPlayerStatAverages.Reset();


	for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayerIndex ) );
		if ( pPlayer && pPlayer->IsConnected())
		{
			StatsCollection_t &matchStats = m_aPlayerStats[pPlayer->entindex()].statsCurrentMatch;

			int teamNumber = pPlayer->GetTeamNumber();
			if (teamNumber == TEAM_CT)
			{
				m_numberOfCounterTerroristEntriesForDirectAverages++;                					
				m_directCTStatAverages += matchStats;
				m_directPlayerStatAverages += matchStats;
			}
			else if (teamNumber == TEAM_TERRORIST)
			{
				m_numberOfTerroristEntriesForDirectAverages++;				
				m_directTStatAverages += matchStats;
				m_directPlayerStatAverages += matchStats;
			}
		}
	}

	if (m_numberOfTerroristEntriesForDirectAverages > 0)
	{
		m_directTStatAverages /= m_numberOfTerroristEntriesForDirectAverages;
		m_directTStatAverages *= m_numberOfRoundsForDirectAverages;
	}

	if (m_numberOfCounterTerroristEntriesForDirectAverages > 0)
	{
		m_directCTStatAverages /= m_numberOfCounterTerroristEntriesForDirectAverages;
		m_directCTStatAverages *= m_numberOfRoundsForDirectAverages;
	}

	int numPlayers = m_numberOfCounterTerroristEntriesForDirectAverages + m_numberOfTerroristEntriesForDirectAverages;

	if (numPlayers > 0)
	{
		m_directPlayerStatAverages /= numPlayers;
		m_directPlayerStatAverages *= m_numberOfRoundsForDirectAverages;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Log accumulated weapon usage and performance data
//-----------------------------------------------------------------------------
void CCSGameStats::DumpMatchWeaponMetrics()
{
	//// generate a filename
	//time_t t = time( NULL );
	//struct tm *now = localtime( &t );
	//if ( !now )
	//	return;

	//int year = now->tm_year + 1900;
	//int month = now->tm_mon + 1;
	//int day = now->tm_mday;
	//int hour = now->tm_hour;
	//int minute = now->tm_min;
	//int second = now->tm_sec;

	//char filename[ 128 ];
	//Q_snprintf( filename, sizeof(filename), "wm_%4d%02d%02d_%02d%02d%02d_%s.csv", 
	//	year, month, day, hour, minute, second, gpGlobals->mapname.ToCStr());

	//FileHandle_t hLogFile = filesystem->Open( filename, "wt" );

	//if ( hLogFile == FILESYSTEM_INVALID_HANDLE )
	//	return;

	//filesystem->FPrintf(hLogFile, "%s\n", "WeaponId, Mode, Cost, Bullets, CycleTime, TotalShots, TotalHits, TotalDamage, TotalKills");

	//for (int iWeapon = 0; iWeapon < WEAPON_MAX; ++iWeapon)
	//{
	//	CCSWeaponInfo* pInfo = GetWeaponInfo( (CSWeaponID)iWeapon );
	//	if ( !pInfo )
	//		continue;

	//	const char* pWeaponName = pInfo->szClassName;
	//	if ( !pWeaponName )
	//		continue;

	//	if ( V_strncmp(pWeaponName, "weapon_", 7) == 0 )
	//		pWeaponName += 7;

	//	for ( int iMode = 0; iMode < WeaponMode_MAX; ++iMode)
	//	{
	//		filesystem->FPrintf(hLogFile, "%s, %d, %d, %d, %f, %d, %d, %d, %d\n", 
	//			pWeaponName,
	//			iMode,
	//			pInfo->GetWeaponPrice(),
	//			pInfo->m_iBullets,
	//			pInfo->m_flCycleTime,
	//			m_weaponStats[iWeapon][iMode].shots, 
	//			m_weaponStats[iWeapon][iMode].hits, 
	//			m_weaponStats[iWeapon][iMode].damage, 
	//			m_weaponStats[iWeapon][iMode].kills);
	//	}
	//}

	//filesystem->FPrintf(hLogFile, "\n");
	//filesystem->FPrintf(hLogFile, "weapon_accuracy_model, %d\n", weapon_accuracy_model.GetInt());
	//filesystem->FPrintf(hLogFile, "bot_difficulty, %d\n", cv_bot_difficulty.GetInt());

	//g_pFullFileSystem->Close(hLogFile);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_PlayerConnected( CBasePlayer *pPlayer )
{
	ResetPlayerStats( ToCSPlayer( pPlayer ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_PlayerDisconnected( CBasePlayer *pPlayer )
{
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
	if ( !pCSPlayer )
		return;

	ResetPlayerStats( pCSPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_PlayerKilledOther( CBasePlayer *pAttacker, CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	// This also gets called when the victim is a building.  That gets tracked separately as building destruction, don't count it here
	if ( !pVictim->IsPlayer() )
		return;

	CBaseEntity *pInflictor = info.GetInflictor();	

	CCSPlayer *pPlayerAttacker = ToCSPlayer( pAttacker );
	CCSPlayer *pPlayerVictim = ToCSPlayer( pVictim );	

    // keep track of how many times every player kills every other player
    TrackKillStats( pPlayerAttacker, pPlayerVictim );

	// Skip rest of stat reporting for friendly fire
	if ( pPlayerAttacker->GetTeam() == pVictim->GetTeam() )
		return;

	CSWeaponID weaponId = WEAPON_NONE;
	
	if ( pInflictor )
	{
		if ( pInflictor == pAttacker )
		{			
			if ( pAttacker->GetActiveWeapon() )
			{
				CBaseCombatWeapon* weapon = pAttacker->GetActiveWeapon();
				if (weapon)
				{
					CWeaponCSBase* pCSWeapon = static_cast<CWeaponCSBase*>(weapon);

					weaponId = pCSWeapon->GetWeaponID();

					CCSWeaponInfo *info = GetWeaponInfo(weaponId);

					if (info && info->m_iTeam != TEAM_UNASSIGNED && pAttacker->GetTeamNumber() != info->m_iTeam )
					{
						IncrementStat(pPlayerAttacker, CSSTAT_KILLS_ENEMY_WEAPON, 1);
					}
				}
			}
		}		
		else
		{
			//In the case of grenades, the inflictor is the spawned grenade entity.
			if ( V_strcmp(pInflictor->m_iClassname.ToCStr(), "hegrenade_projectile") == 0 )
				weaponId = WEAPON_HEGRENADE;
		}
	}

	// update weapon stats
	++m_weaponStats[weaponId][pPlayerAttacker->IsBot()].kills;

	for (int i = 0; WeaponName_StatId_Table[i].killStatId != CSSTAT_UNDEFINED; ++i)
	{
		if ( WeaponName_StatId_Table[i].weaponId == weaponId )
		{
			IncrementStat( pPlayerAttacker, WeaponName_StatId_Table[i].killStatId, 1 );
			break;
		}
	}

	if (pPlayerVictim && pPlayerVictim->IsBlind())
	{
		IncrementStat( pPlayerAttacker, CSSTAT_KILLS_ENEMY_BLINDED, 1 );
	}

	if (pPlayerVictim && pPlayerAttacker && pPlayerAttacker->IsBlindForAchievement())
	{
		IncrementStat( pPlayerAttacker, CSSTAT_KILLS_WHILE_BLINDED, 1 );
	}

	// [sbodenbender] check for deaths near planted bomb for funfact
	if (pPlayerVictim && pPlayerAttacker && pPlayerAttacker->GetTeamNumber() == TEAM_TERRORIST && CSGameRules()->m_bBombPlanted)
	{
		float bombCheckDistSq = AchievementConsts::KillEnemyNearBomb_MaxDistance * AchievementConsts::KillEnemyNearBomb_MaxDistance;
		for ( int i=0; i < g_PlantedC4s.Count(); i++ )
		{
			CPlantedC4 *pC4 = g_PlantedC4s[i];

			if ( pC4->IsBombActive() )
			{
				Vector bombPos = pC4->GetAbsOrigin();
				Vector victimToBomb = pPlayerVictim->GetAbsOrigin() - bombPos;
				Vector attackerToBomb = pPlayerAttacker->GetAbsOrigin() - bombPos;
				if (victimToBomb.LengthSqr() < bombCheckDistSq || attackerToBomb.LengthSqr() < bombCheckDistSq)
				{
					IncrementStat(pPlayerAttacker, CSSTAT_KILLS_WHILE_DEFENDING_BOMB, 1);
					break;	// you only get credit for one kill even if you happen to be by more than one bomb
				}
			}
		}		
	}

	//Increment stat if this is a headshot.
	if (info.GetDamageType() & DMG_HEADSHOT)
	{
		IncrementStat( pPlayerAttacker, CSSTAT_KILLS_HEADSHOT, 1 );
	}

	IncrementStat( pPlayerAttacker, CSSTAT_KILLS, 1 );

	// we don't have a simple way (yet) to check if the victim actually just achieved The Unstoppable Force, so we
	// award this achievement simply if they've met the requirements and would have received it.
	PlayerStats_t &victimStats = m_aPlayerStats[pVictim->entindex()];
	if (victimStats.statsCurrentRound[CSSTAT_KILLS] >= AchievementConsts::UnstoppableForce_Kills)
	{
		pPlayerAttacker->AwardAchievement(CSImmovableObject);
	}

	CCSGameRules::TeamPlayerCounts playerCounts[TEAM_MAXCOUNT];

	CSGameRules()->GetPlayerCounts(playerCounts);
	int iAttackerTeamNumber = pPlayerAttacker->GetTeamNumber() ;
	if (playerCounts[iAttackerTeamNumber].totalAlivePlayers == 1 && playerCounts[iAttackerTeamNumber].killedPlayers >= 2)
	{
		IncrementStat(pPlayerAttacker, CSSTAT_KILLS_WHILE_LAST_PLAYER_ALIVE, 1);
	}	

	//if they were damaged by more than one person that must mean that someone else did damage before the killer finished them off.
	if (pPlayerVictim->GetNumEnemyDamagers() > 1)
	{
		IncrementStat(pPlayerAttacker, CSSTAT_KILLS_ENEMY_WOUNDED, 1);
	}

	// Let's check for the "Happy Camper" achievement where we snipe two players while standing in the same spot.
	if ( pPlayerAttacker && !pPlayerAttacker->IsBot() && weaponId != WEAPON_NONE )
	{
		// Were we using a sniper rifle?
		bool bUsingSniper = (	weaponId == WEAPON_AWP ||
								weaponId == WEAPON_SCOUT ||
								weaponId == WEAPON_SG550 ||
								weaponId == WEAPON_G3SG1 );
			
		// If we're zoomed in
		if ( bUsingSniper && pPlayerAttacker->GetFOV() != pPlayerAttacker->GetDefaultFOV() )
		{
			// Get our position
			 Vector position = pPlayerAttacker->GetAbsOrigin();
			 int userid = pPlayerAttacker->GetUserID();

			 bool bFoundPlayerEntry = false;
			 FOR_EACH_LL( m_PlayerSnipedPosition, iElement )
			 {
				 sHappyCamperSnipePosition *pSnipePosition = &m_PlayerSnipedPosition[iElement];
					 
				 // We've found this player's entry. Next, check to see if they meet the achievement criteria
				 if ( userid == pSnipePosition->m_iUserID )
				 {
					 bFoundPlayerEntry = true;
					 Vector posDif = position - pSnipePosition->m_vPos;

					 // Give a small epsilon to account for floating point anomalies
					 if ( posDif.IsLengthLessThan( .01f) )
					 {
						 pPlayerAttacker->AwardAchievement( CSSnipeTwoFromSameSpot );
					 }

					 // Update their position
					 pSnipePosition->m_vPos = position;
					 break;
				 }
			 }

			 // No entry so add one
			 if ( !bFoundPlayerEntry )
			 {
				m_PlayerSnipedPosition.AddToTail( sHappyCamperSnipePosition( userid, position ) );
			 }
		}
	}
}


void CCSGameStats::CalculateOverkill(CCSPlayer* pAttacker, CCSPlayer* pVictim)
{
    //Count domination overkills - Do this before determining domination
    if (pAttacker->GetTeam() != pVictim->GetTeam())
    {
        if (pAttacker->IsPlayerDominated(pVictim->entindex()))
        {
            IncrementStat( pAttacker, CSSTAT_DOMINATION_OVERKILLS, 1 );
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: Steamworks Gamestats death tracking
//-----------------------------------------------------------------------------
void CCSGameStats::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	if ( !pVictim )
		return;

	m_DeathData.AddToTail( new SCSSDeathData( pVictim, info ) );
}

//-----------------------------------------------------------------------------
// Purpose: Stats event for giving damage to player
//-----------------------------------------------------------------------------
void CCSGameStats::Event_PlayerDamage( CBasePlayer *pBasePlayer, const CTakeDamageInfo &info )
{
	CCSPlayer *pAttacker = ToCSPlayer( info.GetAttacker() );
	if ( pAttacker && pAttacker->GetTeam() != pBasePlayer->GetTeam() )
	{
		CSWeaponMode weaponMode = Primary_Mode;
		IncrementStat( pAttacker, CSSTAT_DAMAGE, info.GetDamage() );
		
        if (pAttacker->m_bNightVisionOn)
        {
            IncrementStat( pAttacker, CSSTAT_NIGHTVISION_DAMAGE, info.GetDamage() );
		}

		const char *pWeaponName = NULL;

		if ( info.GetInflictor() == info.GetAttacker() )
		{
			if ( pAttacker->GetActiveWeapon() )
			{
				CWeaponCSBase* pCSWeapon = dynamic_cast< CWeaponCSBase * >(pAttacker->GetActiveWeapon());
				if (pCSWeapon)
				{
					pWeaponName = pCSWeapon->GetClassname();
					weaponMode = pCSWeapon->m_weaponMode;
				}
			}
		}
		// Need to determine the weapon name
		else
		{
			pWeaponName = info.GetInflictor()->GetClassname();
		}

		// Unknown weapon?!?
		if ( !pWeaponName )
			return;

		// Now update the damage this weapon has done
		CSWeaponID weaponId = AliasToWeaponID( GetTranslatedWeaponAlias( pWeaponName ) );
		for (int i = 0; WeaponName_StatId_Table[i].shotStatId != CSSTAT_UNDEFINED; ++i)
		{
			if ( weaponId == WeaponName_StatId_Table[i].weaponId )
			{
				int weaponId = WeaponName_StatId_Table[i].weaponId;
				int iType = pAttacker->IsBot();
				++m_weaponStats[weaponId][iType].hits;
				m_weaponStats[weaponId][iType].damage += info.GetDamage();
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stats event for giving money to player
//-----------------------------------------------------------------------------
void CCSGameStats::Event_MoneyEarned( CCSPlayer* pPlayer, int moneyEarned)
{
	if ( pPlayer && moneyEarned > 0)
	{
		IncrementStat(pPlayer, CSSTAT_MONEY_EARNED, moneyEarned);
	}
}

void CCSGameStats::Event_MoneySpent( CCSPlayer* pPlayer, int moneySpent, const char *pItemName )
{
	if ( pPlayer && moneySpent > 0)
	{
		IncrementStat(pPlayer, CSSTAT_MONEY_SPENT, moneySpent);
		if ( pItemName && !pPlayer->IsBot() )
		{
			CSteamID steamIDForBuyer;
			pPlayer->GetSteamID( &steamIDForBuyer );
			m_MarketPurchases.AddToTail( new SMarketPurchases( steamIDForBuyer.ConvertToUint64(), moneySpent, pItemName ) );
		}
	}
}

void CCSGameStats::Event_PlayerDonatedWeapon (CCSPlayer* pPlayer)
{
    if (pPlayer)
    {
        IncrementStat(pPlayer, CSSTAT_WEAPONS_DONATED, 1);
    }
}

void CCSGameStats::Event_MVPEarned( CCSPlayer* pPlayer )
{
	if (pPlayer)
	{
		IncrementStat(pPlayer, CSSTAT_MVPS, 1);
	}
}

void CCSGameStats::Event_KnifeUse( CCSPlayer* pPlayer, bool bStab, int iDamage )
{
	if (pPlayer)
	{
		int weaponId = WEAPON_KNIFE;
		int iType = pPlayer->IsBot();

		IncrementStat( pPlayer, CSSTAT_SHOTS_KNIFE, 1 );
		++m_weaponStats[weaponId][iType].shots;
		if ( iDamage )
		{
			IncrementStat( pPlayer, CSSTAT_HITS_KNIFE, 1 );
			++m_weaponStats[weaponId][iType].hits;

			IncrementStat( pPlayer, CSSTAT_DAMAGE_KNIFE, iDamage );
			m_weaponStats[weaponId][iType].damage += iDamage;
		}
	}
}
//-----------------------------------------------------------------------------
// Purpose: Event handler
//-----------------------------------------------------------------------------
void CCSGameStats::FireGameEvent( IGameEvent *event )
{
	const char *pEventName = event->GetName();

	if ( V_strcmp(pEventName, "round_start") == 0 )
	{
		m_PlayerSnipedPosition.Purge();
		m_bInRound = true;
	}
	else if ( V_strcmp(pEventName, "round_end") == 0 )
	{
		const int reason = event->GetInt( "reason" );

		if( reason == Game_Commencing )
		{
			ResetPlayerClassMatchStats();
		}
		else
		{
			UpdatePlayerRoundStats(event->GetInt("winner"));
			ComputeDirectStatAverages();
			SendDirectStatsAveragesToAllPlayers();

			UploadRoundStats();
			ResetWeaponStats();
		}
		return;
	}
	else if ( V_strcmp(pEventName, "break_prop") == 0 )
	{
		int userid = event->GetInt("userid", 0);
		int entindex = event->GetInt("entindex", 0);
 		CBreakableProp* pProp = static_cast<CBreakableProp*>(CBaseEntity::Instance(entindex));
 		Event_BreakProp(ToCSPlayer(UTIL_PlayerByUserId(userid)), pProp);
	}
	else if ( V_strcmp(pEventName, "player_decal") == 0 )
	{
		int userid = event->GetInt("userid", 0);
		Event_PlayerSprayedDecal(ToCSPlayer(UTIL_PlayerByUserId(userid)));
	}
	else if ( V_strcmp(pEventName, "hegrenade_detonate") == 0 )
	{
		int userid = event->GetInt("userid", 0);
		IncrementStat( ToCSPlayer(UTIL_PlayerByUserId(userid)), CSSTAT_SHOTS_HEGRENADE, 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return stats for the given player
//-----------------------------------------------------------------------------
const PlayerStats_t& CCSGameStats::FindPlayerStats( CBasePlayer *pPlayer ) const
{
	return m_aPlayerStats[pPlayer->entindex()];
}

//-----------------------------------------------------------------------------
// Purpose: Return stats for the given team
//-----------------------------------------------------------------------------
const StatsCollection_t& CCSGameStats::GetTeamStats( int iTeamIndex ) const
{
	int arrayIndex = iTeamIndex - FIRST_GAME_TEAM;
	Assert( arrayIndex >= 0 && arrayIndex < TEAM_MAXCOUNT - FIRST_GAME_TEAM );
	return m_aTeamStats[arrayIndex];
}

//-----------------------------------------------------------------------------
// Purpose: Resets the stats for each team
//-----------------------------------------------------------------------------
void CCSGameStats::ResetAllTeamStats()
{
	for ( int i = 0; i < ARRAYSIZE(m_aTeamStats); ++i )
	{
		m_aTeamStats[i].Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resets all stats (including round, match, accumulated and rolling averages
//-----------------------------------------------------------------------------
void CCSGameStats::ResetAllStats()
{
    for ( int i = 0; i < ARRAYSIZE( m_aPlayerStats ); i++ )
    {		
		m_aPlayerStats[i].statsDelta.Reset();
        m_aPlayerStats[i].statsCurrentRound.Reset();
        m_aPlayerStats[i].statsCurrentMatch.Reset();
        m_rollingCTStatAverages.Reset();
        m_rollingTStatAverages.Reset();
        m_rollingPlayerStatAverages.Reset();
		m_numberOfRoundsForDirectAverages = 0;
		m_numberOfTerroristEntriesForDirectAverages = 0;
		m_numberOfCounterTerroristEntriesForDirectAverages = 0;
    }
}


void CCSGameStats::ResetWeaponStats()
{
	V_memset(m_weaponStats, 0, sizeof(m_weaponStats));
}

void CCSGameStats::IncrementTeamStat( int iTeamIndex, int iStatIndex, int iAmount )
{
	int arrayIndex = iTeamIndex - TEAM_TERRORIST;
	Assert( iStatIndex >= 0 && iStatIndex < CSSTAT_MAX );
	if( arrayIndex >= 0 && arrayIndex < TEAM_MAXCOUNT - TEAM_TERRORIST )
	{
		m_aTeamStats[arrayIndex][iStatIndex] += iAmount;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resets all stats for this player
//-----------------------------------------------------------------------------
void CCSGameStats::ResetPlayerStats( CBasePlayer* pPlayer )
{
	PlayerStats_t &stats = m_aPlayerStats[pPlayer->entindex()];
	// reset the stats on this player
	stats.Reset();
	// reset the matrix of who killed whom with respect to this player
	ResetKillHistory( pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: Resets the kill history for this player
//-----------------------------------------------------------------------------
void CCSGameStats::ResetKillHistory( CBasePlayer* pPlayer )
{
	int iPlayerIndex = pPlayer->entindex();

	PlayerStats_t& statsPlayer = m_aPlayerStats[iPlayerIndex];

	// for every other player, set all all the kills with respect to this player to 0
	for ( int i = 0; i < ARRAYSIZE( m_aPlayerStats ); i++ )
	{
		//reset their record of us.
		PlayerStats_t &statsOther = m_aPlayerStats[i];
		statsOther.statsKills.iNumKilled[iPlayerIndex] = 0;
		statsOther.statsKills.iNumKilledBy[iPlayerIndex] = 0;
		statsOther.statsKills.iNumKilledByUnanswered[iPlayerIndex] = 0;

		//reset our record of them
		statsPlayer.statsKills.iNumKilled[i] = 0;
		statsPlayer.statsKills.iNumKilledBy[i] = 0;
		statsPlayer.statsKills.iNumKilledByUnanswered[i] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resets per-round stats for all players
//-----------------------------------------------------------------------------
void CCSGameStats::ResetRoundStats()
{
	for ( int i = 0; i < ARRAYSIZE( m_aPlayerStats ); i++ )
	{		
		m_aPlayerStats[i].statsCurrentRound.Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Increments specified stat for specified player by specified amount
//-----------------------------------------------------------------------------
void CCSGameStats::IncrementStat( CCSPlayer* pPlayer, CSStatType_t statId, int iDelta, bool bPlayerOnly /* = false */ )
{
    if ( pPlayer )
    {
	    PlayerStats_t &stats = m_aPlayerStats[pPlayer->entindex()];
	    stats.statsDelta[statId] += iDelta;
	    stats.statsCurrentRound[statId] += iDelta;
	    stats.statsCurrentMatch[statId] += iDelta;

		// increment team stat
		int teamIndex = pPlayer->GetTeamNumber() - FIRST_GAME_TEAM;
		if ( !bPlayerOnly && teamIndex >= 0 && teamIndex < ARRAYSIZE(m_aTeamStats) )
		{
			m_aTeamStats[teamIndex][statId] += iDelta;
		}

	    for (int i = 0; i < ARRAYSIZE(ServerStatBasedAchievements); ++i)
	    {
		    if (ServerStatBasedAchievements[i].statId == statId)
		    {
			    // skip this if there is a map filter and it doesn't match
			    if (ServerStatBasedAchievements[i].mapFilter != NULL && V_strcmp(gpGlobals->mapname.ToCStr(), ServerStatBasedAchievements[i].mapFilter) != 0)
				    continue;

			    bool bWasMet = ServerStatBasedAchievements[i].IsMet(stats.statsCurrentRound[statId] - iDelta, stats.statsCurrentMatch[statId] - iDelta);
			    bool bIsMet = ServerStatBasedAchievements[i].IsMet(stats.statsCurrentRound[statId], stats.statsCurrentMatch[statId]);
			    if (!bWasMet && bIsMet)
			    {
				    pPlayer->AwardAchievement(ServerStatBasedAchievements[i].achievementId);
                }
		    }
	    }
    }
}

//-----------------------------------------------------------------------------
// Purpose:  Sets the specified stat for specified player to the specified amount
//-----------------------------------------------------------------------------
void CCSGameStats::SetStat( CCSPlayer *pPlayer, CSStatType_t statId, int iValue )
{
	if (pPlayer)
	{
		int oldRoundValue, oldMatchValue;
		PlayerStats_t &stats = m_aPlayerStats[pPlayer->entindex()];

		oldRoundValue = stats.statsCurrentRound[statId];
		oldMatchValue = stats.statsCurrentMatch[statId];

		stats.statsDelta[statId] = iValue;
		stats.statsCurrentRound[statId] = iValue;
		stats.statsCurrentMatch[statId] = iValue;

		for (int i = 0; i < ARRAYSIZE(ServerStatBasedAchievements); ++i)
		{
			if (ServerStatBasedAchievements[i].statId == statId)
			{
				// skip this if there is a map filter and it doesn't match
				if (ServerStatBasedAchievements[i].mapFilter != NULL && V_strcmp(gpGlobals->mapname.ToCStr(), ServerStatBasedAchievements[i].mapFilter) != 0)
					continue;

				bool bWasMet = ServerStatBasedAchievements[i].IsMet(oldRoundValue, oldMatchValue);
				bool bIsMet = ServerStatBasedAchievements[i].IsMet(stats.statsCurrentRound[statId], stats.statsCurrentMatch[statId]);
				if (!bWasMet && bIsMet)
				{
					pPlayer->AwardAchievement(ServerStatBasedAchievements[i].achievementId);
				}
			}
		}
	}
}

void CCSGameStats::SendStatsToPlayer( CCSPlayer * pPlayer, int iMinStatPriority )
{
	ASSERT(CSSTAT_MAX < 255); // if we add more than 255 stats, we'll need to update this protocol
	if ( pPlayer && pPlayer->IsConnected())
	{				
		StatsCollection_t &deltaStats = m_aPlayerStats[pPlayer->entindex()].statsDelta;

		// check to see if we have any stats to actually send
		byte iStatsToSend = 0;
		for ( int iStat = CSSTAT_FIRST; iStat < CSSTAT_MAX; ++iStat )
		{
			ASSERT(CSStatProperty_Table[iStat].statId == iStat);
			if ( CSStatProperty_Table[iStat].statId != iStat )
			{
				Warning( "CSStatProperty_Table[iStat].statId != iStat, (%d)", CSStatProperty_Table[iStat].statId );
			}
			int iPriority = CSStatProperty_Table[iStat].flags & CSSTAT_PRIORITY_MASK;
			if (deltaStats[iStat] != 0 && iPriority >= iMinStatPriority)
			{
				++iStatsToSend;
			}
		}

		// nothing changed - bail out
		if ( !iStatsToSend )
			return;

		CSingleUserRecipientFilter filter( pPlayer );
		filter.MakeReliable();
		UserMessageBegin( filter, "PlayerStatsUpdate" );

		CRC32_t crc;
		CRC32_Init( &crc );

		// begin the CRC with a trivially hidden key value to discourage packet modification
		const uint32 key = 0x82DA9F4C;	// this key should match the key in cs_client_gamestats.cpp
		CRC32_ProcessBuffer( &crc, &key, sizeof(key));

		// if we make any change to the ordering of the stats or this message format, update this value
		const byte version = 0x01;
		CRC32_ProcessBuffer( &crc, &version, sizeof(version));
		WRITE_BYTE(version);

		CRC32_ProcessBuffer( &crc, &iStatsToSend, sizeof(iStatsToSend));
		WRITE_BYTE(iStatsToSend);

		for ( byte iStat = CSSTAT_FIRST; iStat < CSSTAT_MAX; ++iStat )
		{
			int iPriority = CSStatProperty_Table[iStat].flags & CSSTAT_PRIORITY_MASK;
			if (deltaStats[iStat] != 0 && iPriority >= iMinStatPriority)
			{
				CRC32_ProcessBuffer( &crc, &iStat, sizeof(iStat));
				WRITE_BYTE(iStat);
				Assert(deltaStats[iStat] <= 0x7FFF && deltaStats[iStat] > 0);	// make sure we aren't truncating bits
				short delta = deltaStats[iStat];
				CRC32_ProcessBuffer( &crc, &delta, sizeof(delta));
				WRITE_SHORT( deltaStats[iStat]);
				deltaStats[iStat] = 0;
				--iStatsToSend;
			}
		}

		Assert(iStatsToSend == 0);

		CRC32_Final( &crc );
		WRITE_LONG(crc);

		MessageEnd();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sends intermittent stats updates for stats that need to be updated during a round and/or life
//-----------------------------------------------------------------------------
void CCSGameStats::PreClientUpdate()
{
	int iMinStatPriority = -1;
	m_fDisseminationTimerHigh += gpGlobals->frametime;
	m_fDisseminationTimerLow += gpGlobals->frametime;

	if ( m_fDisseminationTimerHigh > cDisseminationTimeHigh)
	{
		iMinStatPriority = CSSTAT_PRIORITY_HIGH;
		m_fDisseminationTimerHigh = 0.0f;

		if ( m_fDisseminationTimerLow > cDisseminationTimeLow)
		{
			iMinStatPriority = CSSTAT_PRIORITY_LOW;
			m_fDisseminationTimerLow = 0.0f;
		}
	}
	else
		return;

	//The proper time has elapsed, now send the update to every player
	for ( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex(iPlayerIndex) );
		SendStatsToPlayer(pPlayer, iMinStatPriority);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates the stats of who has killed whom
//-----------------------------------------------------------------------------
void CCSGameStats::TrackKillStats( CCSPlayer *pAttacker, CCSPlayer *pVictim )
{
    int iPlayerIndexAttacker = pAttacker->entindex();
    int iPlayerIndexVictim = pVictim->entindex();

    PlayerStats_t &statsAttacker = m_aPlayerStats[iPlayerIndexAttacker];
    PlayerStats_t &statsVictim = m_aPlayerStats[iPlayerIndexVictim];

    statsVictim.statsKills.iNumKilledBy[iPlayerIndexAttacker]++;
    statsVictim.statsKills.iNumKilledByUnanswered[iPlayerIndexAttacker]++;
    statsAttacker.statsKills.iNumKilled[iPlayerIndexVictim]++;
    statsAttacker.statsKills.iNumKilledByUnanswered[iPlayerIndexVictim] = 0;    
}


//-----------------------------------------------------------------------------
// Purpose: Determines if attacker and victim have gotten domination or revenge
//-----------------------------------------------------------------------------
void CCSGameStats::CalcDominationAndRevenge( CCSPlayer *pAttacker, CCSPlayer *pVictim, int *piDeathFlags )
{
	//=============================================================================
	// HPE_BEGIN:
	// [Forrest] Allow nemesis/revenge to be turned off for a server
	//=============================================================================
	if ( sv_nonemesis.GetBool() )
	{
		return;
	}
	//=============================================================================
	// HPE_END
	//=============================================================================

	//If there is no attacker, there is no domination or revenge
	if( !pAttacker || !pVictim )
	{
		return;
	}
	if (pAttacker->GetTeam() == pVictim->GetTeam())
	{
		return;
	}
	int iPlayerIndexVictim = pVictim->entindex();
	PlayerStats_t &statsVictim = m_aPlayerStats[iPlayerIndexVictim];	
	// calculate # of unanswered kills between killer & victim
	// This is plus 1 as this function gets called before the stat is updated.  That is done so that the domination
	// and revenge will be calculated prior to the death message being sent to the clients
	int attackerEntityIndex = pAttacker->entindex();
	int iKillsUnanswered = statsVictim.statsKills.iNumKilledByUnanswered[attackerEntityIndex] + 1;	

	if ( CS_KILLS_FOR_DOMINATION == iKillsUnanswered )
	{
		// this is the Nth unanswered kill between killer and victim, killer is now dominating victim
		*piDeathFlags |= ( CS_DEATH_DOMINATION );
	}
	else if ( pVictim->IsPlayerDominated( pAttacker->entindex() ) )
	{
		// the killer killed someone who was dominating him, gains revenge
		*piDeathFlags |= ( CS_DEATH_REVENGE );
	}

    //Check the overkill on 1 player achievement
    if (iKillsUnanswered == CS_KILLS_FOR_DOMINATION + AchievementConsts::ExtendedDomination_AdditionalKills)
    {
        pAttacker->AwardAchievement(CSExtendedDomination);
    }

    if (iKillsUnanswered == CS_KILLS_FOR_DOMINATION)
    {			
        //this is the Nth unanswered kill between killer and victim, killer is now dominating victim        
        //set victim to be dominated by killer
        pAttacker->SetPlayerDominated( pVictim, true );

        //Check concurrent dominations achievement
        int numConcurrentDominations = 0;
        for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
        {
            CCSPlayer *pPlayer= ToCSPlayer( UTIL_PlayerByIndex( i ) );
            if (pPlayer && pAttacker->IsPlayerDominated(pPlayer->entindex()))
            {
                numConcurrentDominations++;
            }
        }
        if (numConcurrentDominations >= AchievementConsts::ConcurrentDominations_MinDominations)
        {
            pAttacker->AwardAchievement(CSConcurrentDominations);
        }

        
        // record stats
        Event_PlayerDominatedOther( pAttacker, pVictim );
    }
    else if ( pVictim->IsPlayerDominated( pAttacker->entindex() ) )
    {
        // the killer killed someone who was dominating him, gains revenge        
        // set victim to no longer be dominating the killer

        pVictim->SetPlayerDominated( pAttacker, false );
        // record stats
        Event_PlayerRevenge( pAttacker );
    }
}

void CCSGameStats::Event_PlayerDominatedOther( CCSPlayer *pAttacker, CCSPlayer* pVictim )
{
    IncrementStat( pAttacker, CSSTAT_DOMINATIONS, 1 );
}

void CCSGameStats::Event_PlayerRevenge( CCSPlayer *pAttacker )
{
    IncrementStat( pAttacker, CSSTAT_REVENGES, 1 );
}

void CCSGameStats::Event_PlayerAvengedTeammate( CCSPlayer* pAttacker, CCSPlayer* pAvengedPlayer )
{
    if (pAttacker && pAvengedPlayer)
    {
        IGameEvent *event = gameeventmanager->CreateEvent( "player_avenged_teammate" );

        if ( event )
        {
            event->SetInt( "avenger_id", pAttacker->GetUserID() );
            event->SetInt( "avenged_player_id", pAvengedPlayer->GetUserID() );
            gameeventmanager->FireEvent( event );
        }
    }
}

void CCSGameStats::Event_LevelInit()
{
	ResetAllTeamStats();
	ResetWeaponStats();
	CBaseGameStats::Event_LevelInit();
	GetSteamWorksSGameStatsUploader().StartSession();
}

void CCSGameStats::Event_LevelShutdown( float fElapsed )
{
	DumpMatchWeaponMetrics();
	CBaseGameStats::Event_LevelShutdown(fElapsed);
}

// Reset any per match info that resides in the player class
void CCSGameStats::ResetPlayerClassMatchStats()
{
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayer )
		{
			pPlayer->SetNumMVPs( 0 );
		}
	}
}

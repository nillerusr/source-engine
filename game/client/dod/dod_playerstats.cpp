//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "dod_playerstats.h"
#include "hud_macros.h"

const char *g_pszShortTeamNames[2] = 
{
	"US",
	"Ger"
};

const char *g_pszClassNames[NUM_DOD_PLAYERCLASSES] = 
{
	"Rifleman",
	"Assault",
	"Support",
	"Sniper",
	"MG",
	"Rocket"
};

const char *g_pszStatNames[DODSTAT_MAX] = 
{
	"iPlayTime",
	"iRoundsWon",
	"iRoundsLost",
	"iKills",
	"iDeaths",
	"iCaptures",
	"iBlocks",
	"iBombsPlanted",
	"iBombsDefused",
	"iDominations",
	"iRevenges",
	"iShotsHit",
	"iShotsFired",
	"iHeadshots"
};

int iValidWeaponStatBitMask = ( (1<<DODSTAT_KILLS ) | (1<<DODSTAT_SHOTS_HIT) | (1<<DODSTAT_SHOTS_FIRED) | (1<<DODSTAT_HEADSHOTS) );
int iValidPlayerStatBitMask = 0xFFFF & ~( (1<<DODSTAT_SHOTS_HIT) | (1<<DODSTAT_SHOTS_FIRED) | (1<<DODSTAT_HEADSHOTS) );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &msg - 
//-----------------------------------------------------------------------------
void __MsgFunc_DODPlayerStatsUpdate( bf_read &msg )
{
	g_DODPlayerStats.MsgFunc_DODPlayerStatsUpdate( msg );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDODPlayerStats::CDODPlayerStats() 
{
	m_flTimeNextForceUpload = 0;
}

//-----------------------------------------------------------------------------
// Purpose: called at init time after all systems are init'd.  We have to
//			do this in PostInit because the Steam app ID is not available earlier
//-----------------------------------------------------------------------------
void CDODPlayerStats::PostInit()
{
	SetNextForceUploadTime();

	ListenForGameEvent( "player_stats_updated" );
	ListenForGameEvent( "user_data_downloaded" );

	HOOK_MESSAGE( DODPlayerStatsUpdate );
}

//-----------------------------------------------------------------------------
// Purpose: called at level shutdown
//-----------------------------------------------------------------------------
void CDODPlayerStats::LevelShutdownPreEntity()
{
	// upload user stats to Steam on every map change or when we quit
	UploadStats();
}

//-----------------------------------------------------------------------------
// Purpose: called when the stats have changed in-game
//-----------------------------------------------------------------------------
void CDODPlayerStats::FireGameEvent( IGameEvent *event )
{
	const char *pEventName = event->GetName();

	if ( FStrEq( pEventName, "user_data_downloaded" ) )
	{
		// Store our stat data

		Assert( steamapicontext->SteamUserStats() );
		if ( !steamapicontext->SteamUserStats() )
			return; 

		CGameID gameID( engine->GetAppID() );

		// reset everything
		Q_memset( &m_PlayerStats, 0, sizeof(m_PlayerStats) );
		Q_memset( &m_WeaponStats, 0, sizeof(m_WeaponStats) );

		// read playerclass stats
		for ( int iTeam=0;iTeam<2;iTeam++ )
		{
			for ( int iClass=0;iClass<NUM_DOD_PLAYERCLASSES;iClass++ )
			{
				for ( int iStat=0;iStat<DODSTAT_MAX;iStat++ )
				{
					if ( iValidPlayerStatBitMask & (1<<iStat) )
					{
						char szStatName[256];
						int iData;

						Q_snprintf( szStatName, ARRAYSIZE( szStatName ), "%s.%s.%s", g_pszShortTeamNames[iTeam], g_pszClassNames[iClass], g_pszStatNames[iStat] );

						if ( steamapicontext->SteamUserStats()->GetStat( szStatName, &iData ) )
						{
							// use Steam's value
							m_PlayerStats[iTeam][iClass].m_iStat[iStat] = iData;
						}	
					}					
				}
			}
		}		

		// read weapon stats
		for ( int iWeapon=WEAPON_NONE+1;iWeapon<WEAPON_MAX;iWeapon++ )
		{
			for ( int iStat=0;iStat<DODSTAT_MAX;iStat++ )
			{
				if ( iValidWeaponStatBitMask & (1<<iStat) )
				{
					char szStatName[256];
					int iData;

					Q_snprintf( szStatName, ARRAYSIZE( szStatName ), "%s.%s", s_WeaponAliasInfo[iWeapon], g_pszStatNames[iStat] );

					if ( steamapicontext->SteamUserStats()->GetStat( szStatName, &iData ) )
					{
						// use Steam's value
						m_WeaponStats[iWeapon].m_iStat[iStat] = iData;
					}	
				}					
			}
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "player_stats_updated" );
		if ( event )
		{
			event->SetBool( "forceupload", false );
			gameeventmanager->FireEventClientSide( event );
		}
	}
}

void CDODPlayerStats::MsgFunc_DODPlayerStatsUpdate( bf_read &msg )
{
	dod_stat_accumulator_t playerStats;
	Q_memset( &playerStats, 0, sizeof(playerStats) );

	// get the fixed-size information
	int iClass = msg.ReadByte();
	int iTeam = msg.ReadByte();

	int iPlayerStatBits = msg.ReadLong();

	Assert( iClass >= 0 && iClass <= 5 );
	if ( iClass < 0 || iClass > 5 )
		return;

	// the bitfield indicates which stats are contained in the message.  Set the stats appropriately.
	int iStat = DODSTAT_FIRST;
	while ( iPlayerStatBits > 0 )
	{
		if ( iPlayerStatBits & 1 )
		{
			playerStats.m_iStat[iStat] = msg.ReadLong();
		}
		iPlayerStatBits >>= 1;
		iStat++;
	}

	int iNumWeapons = msg.ReadByte();
	int i;

	CUtlVector<weapon_stat_t> vecWeaponStats;

	for ( i=0;i<iNumWeapons;i++ )
	{
		weapon_stat_t weaponStat;
		Q_memset( &weaponStat, 0, sizeof(weaponStat) );

		weaponStat.iWeaponID = msg.ReadByte();

		vecWeaponStats.AddToTail( weaponStat );
	}

	for ( i=0;i<vecWeaponStats.Count();i++ )
	{
		int iWeaponStatMask = msg.ReadLong();

		int iStat = DODSTAT_FIRST;
		while ( iWeaponStatMask > 0 )
		{
			if ( iWeaponStatMask & 1 )
			{
				vecWeaponStats[i].stats.m_iStat[iStat] = msg.ReadLong();
			}
			iWeaponStatMask >>= 1;
			iStat++;
		}
	}

	// sanity check: the message should contain exactly the # of bytes we expect based on the bit field
	Assert( !msg.IsOverflowed() );
	Assert( 0 == msg.GetNumBytesLeft() );
	// if byte count isn't correct, bail out and don't use this data, rather than risk polluting player stats with garbage
	if ( msg.IsOverflowed() || ( 0 != msg.GetNumBytesLeft() ) )
		return;

	UpdateStats( iClass, iTeam, &playerStats, &vecWeaponStats );
}

//-----------------------------------------------------------------------------
// Purpose: Uploads stats for current Steam user to Steam
//-----------------------------------------------------------------------------
void CDODPlayerStats::UploadStats()
{
	// only upload if Steam is running
	if ( !steamapicontext->SteamUserStats() )
		return; 

	CGameID gameID( engine->GetAppID() );
	char szStatName[256];

	// write playerclass stats
	for ( int iTeam=0;iTeam<2;iTeam++ )
	{
		for ( int iClass=0;iClass<NUM_DOD_PLAYERCLASSES;iClass++ )
		{
			for ( int iStat=0;iStat<DODSTAT_MAX;iStat++ )
			{	
				if ( iValidPlayerStatBitMask & (1<<iStat) )
				{
					if ( m_PlayerStats[iTeam][iClass].m_bDirty[iStat] )
					{
						Q_snprintf( szStatName, ARRAYSIZE( szStatName ), "%s.%s.%s", g_pszShortTeamNames[iTeam], g_pszClassNames[iClass], g_pszStatNames[iStat] );

						steamapicontext->SteamUserStats()->SetStat( szStatName, m_PlayerStats[iTeam][iClass].m_iStat[iStat] );
					}
				}				
			}		
		}
	}		

	// write weapon stats
	for ( int iWeapon=WEAPON_NONE+1;iWeapon<WEAPON_MAX;iWeapon++ )
	{
		for ( int iStat=0;iStat<DODSTAT_MAX;iStat++ )
		{
			if ( m_WeaponStats[iWeapon].m_bDirty[iStat] )
			{
				if ( iValidWeaponStatBitMask & (1<<iStat) )
				{
					Q_snprintf( szStatName, ARRAYSIZE( szStatName ), "%s.%s", s_WeaponAliasInfo[iWeapon], g_pszStatNames[iStat] );

					steamapicontext->SteamUserStats()->SetStat( szStatName, m_WeaponStats[iWeapon].m_iStat[iStat] );
				}	
			}								
		}	
	}

	SetNextForceUploadTime();
}

void CDODPlayerStats::UpdateStats( int iPlayerClass, int iTeam, dod_stat_accumulator_t *playerStats, CUtlVector<weapon_stat_t> *vecWeaponStats )
{
	Assert( iPlayerClass >= 0 && iPlayerClass < NUM_DOD_PLAYERCLASSES );
	if ( iPlayerClass < 0 || iPlayerClass >= NUM_DOD_PLAYERCLASSES )
		return;

	// translate the team index into [0,1]
	int iTeamIndex = ( iTeam == TEAM_ALLIES ) ? 0 : 1;

	// upload this class' data

	// add this stat update to our stored stats
	for ( int iStat=0;iStat<DODSTAT_MAX;iStat++ )
	{
		if ( iValidPlayerStatBitMask & (1<<iStat) )
		{
			if ( playerStats->m_iStat[iStat] > 0 )
			{
				m_PlayerStats[iTeamIndex][iPlayerClass].m_iStat[iStat] += playerStats->m_iStat[iStat];
				m_PlayerStats[iTeamIndex][iPlayerClass].m_bDirty[iStat] = true;
			}
		}
	}		

	int iWeaponStatCount = vecWeaponStats->Count();

	for ( int i=0;i<iWeaponStatCount;i++ )
	{
		int iWeaponID = vecWeaponStats->Element(i).iWeaponID;

		for ( int iStat=0;iStat<DODSTAT_MAX;iStat++ )
		{
			if ( iValidWeaponStatBitMask & (1<<iStat) )
			{
				int iValue = vecWeaponStats->Element(i).stats.m_iStat[iStat];
				if ( iValue > 0 )
				{
					m_WeaponStats[iWeaponID].m_iStat[iStat] += iValue;
					m_WeaponStats[iWeaponID].m_bDirty[iStat] = true;
				}
			}					
		}
	}

	// if we haven't uploaded stats in a long time, upload them 
	if ( ( gpGlobals->curtime >= m_flTimeNextForceUpload ) )
	{
		UploadStats();
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets the next time to force a stats upload at
//-----------------------------------------------------------------------------
void CDODPlayerStats::SetNextForceUploadTime()
{
	// pick a time a while from now (an hour +/- 15 mins) to upload stats if we haven't gotten a map change by then
	m_flTimeNextForceUpload = gpGlobals->curtime + ( 60 * RandomInt( 45, 75 ) );
}

CDODPlayerStats g_DODPlayerStats;
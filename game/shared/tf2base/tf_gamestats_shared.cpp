//====== Copyright © 1996-2006, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#ifdef GAME_DLL
#include "gamestats.h"
#endif
#include "tf_gamestats_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
//#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  :  - 
//-----------------------------------------------------------------------------
TF_Gamestats_LevelStats_t::TF_Gamestats_LevelStats_t()
{
	m_bInitialized = false;
	m_flRoundStartTime = 0.0f;	
	m_Header.m_iRoundsPlayed = 0;
	m_Header.m_iTotalTime = 0;
	m_Header.m_iBlueWins = 0;
	m_Header.m_iRedWins = 0;
	m_Header.m_iStalemates = 0;
	m_Header.m_iBlueSuddenDeathWins = 0;
	m_Header.m_iRedSuddenDeathWins = 0;
	Q_memset( m_aClassStats, 0, sizeof( m_aClassStats ) );
	Q_memset( m_aWeaponStats, 0, sizeof( m_aWeaponStats ) );
	Q_memset( m_iPeakPlayerCount, 0, sizeof( m_iPeakPlayerCount ) );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
// Input  :  - 
//-----------------------------------------------------------------------------
TF_Gamestats_LevelStats_t::~TF_Gamestats_LevelStats_t()
{
	m_aPlayerDeaths.Purge();
	m_aPlayerDamage.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Copy constructor
// Input  :  - 
//-----------------------------------------------------------------------------
TF_Gamestats_LevelStats_t::TF_Gamestats_LevelStats_t( const TF_Gamestats_LevelStats_t &stats )
{
	m_bInitialized		= stats.m_bInitialized;
	m_flRoundStartTime	= stats.m_flRoundStartTime;
	m_Header			= stats.m_Header;
	m_aPlayerDeaths		= stats.m_aPlayerDeaths;
	m_aPlayerDamage		= stats.m_aPlayerDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszMapName - 
//			nIPAddr - 
//			nPort - 
//			flStartTime - 
//-----------------------------------------------------------------------------
void TF_Gamestats_LevelStats_t::Init( const char *pszMapName, int nIPAddr, short nPort, float flStartTime  )
{
	// Initialize.
	Q_strncpy( m_Header.m_szMapName, pszMapName, sizeof( m_Header.m_szMapName ) );
	m_Header.m_nIPAddr = nIPAddr;
	m_Header.m_nPort = nPort;
	
	// Start the level timer.
	m_flRoundStartTime = flStartTime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flEndTime - 
//-----------------------------------------------------------------------------
void TF_Gamestats_LevelStats_t::Shutdown( float flEndTime )
{
}

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
TFReportedStats_t::TFReportedStats_t()
{
	Clear();
	m_pCurrentGame = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
TFReportedStats_t::~TFReportedStats_t()
{
	if ( m_pCurrentGame )
	{
		delete m_pCurrentGame;
		m_pCurrentGame = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clears data
//-----------------------------------------------------------------------------
void TFReportedStats_t::Clear()
{
	m_pCurrentGame = NULL;
	m_dictMapStats.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szMapName - 
// Output : TF_Gamestats_LevelStats_t
//-----------------------------------------------------------------------------
TF_Gamestats_LevelStats_t *TFReportedStats_t::FindOrAddMapStats( const char *szMapName )
{
	int iMap = m_dictMapStats.Find( szMapName );
	if( iMap == m_dictMapStats.InvalidIndex() )
	{
		iMap = m_dictMapStats.Insert( szMapName );
	}	

	return &m_dictMapStats[iMap];
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: Saves data to buffer
//-----------------------------------------------------------------------------
void TFReportedStats_t::AppendCustomDataToSaveBuffer( CUtlBuffer &SaveBuffer )
{
	// save a version lump at beginning of file
	TF_Gamestats_Version_t versionLump;
	versionLump.m_iMagic = TF_GAMESTATS_MAGIC;
	versionLump.m_iVersion = TF_GAMESTATS_FILE_VERSION;
	CBaseGameStats::AppendLump( MAX_LUMP_COUNT, SaveBuffer, TFSTATS_LUMP_VERSION, 1, sizeof( versionLump ), &versionLump );

	// Save data per map.
	for ( int iMap =  m_dictMapStats.First(); iMap != m_dictMapStats.InvalidIndex(); iMap =  m_dictMapStats.Next( iMap ) )
	{
		// Get the current map.
		TF_Gamestats_LevelStats_t *pCurrentMap = &m_dictMapStats[iMap];
		Assert( pCurrentMap );

		// Write out the lumps.
		CBaseGameStats::AppendLump( MAX_LUMP_COUNT, SaveBuffer, TFSTATS_LUMP_MAPHEADER, 1, sizeof( TF_Gamestats_LevelStats_t::LevelHeader_t ), static_cast<void*>( &pCurrentMap->m_Header ) );
		CBaseGameStats::AppendLump( MAX_LUMP_COUNT, SaveBuffer, TFSTATS_LUMP_MAPDEATH, pCurrentMap->m_aPlayerDeaths.Count(), sizeof( TF_Gamestats_LevelStats_t::PlayerDeathsLump_t ), static_cast<void*>( pCurrentMap->m_aPlayerDeaths.Base() ) );
		CBaseGameStats::AppendLump( MAX_LUMP_COUNT, SaveBuffer, TFSTATS_LUMP_MAPDAMAGE, pCurrentMap->m_aPlayerDamage.Count(), sizeof( TF_Gamestats_LevelStats_t::PlayerDamageLump_t ), static_cast<void*>( pCurrentMap->m_aPlayerDamage.Base() ) );
		CBaseGameStats::AppendLump( MAX_LUMP_COUNT, SaveBuffer, TFSTATS_LUMP_CLASS, ARRAYSIZE( pCurrentMap->m_aClassStats ), sizeof( pCurrentMap->m_aClassStats[0] ), 
			static_cast<void*>( pCurrentMap->m_aClassStats ) );
		CBaseGameStats::AppendLump( MAX_LUMP_COUNT, SaveBuffer, TFSTATS_LUMP_WEAPON, ARRAYSIZE( pCurrentMap->m_aWeaponStats ), sizeof( pCurrentMap->m_aWeaponStats[0] ), 
			static_cast<void*>( pCurrentMap->m_aWeaponStats ) );
	}

	// Append an end tag to verify we've reached end of file and data was sane.  (Sometimes we receive stat files that start sane but become filled
	// with garbage partway through.)
	CBaseGameStats::AppendLump( MAX_LUMP_COUNT, SaveBuffer, TFSTATS_LUMP_ENDTAG, 1, sizeof( versionLump ), &versionLump );
}

//-----------------------------------------------------------------------------
// Purpose: Loads data from buffer
//-----------------------------------------------------------------------------
bool TFReportedStats_t::LoadCustomDataFromBuffer( CUtlBuffer &LoadBuffer )
{
	// read the version lump of beginning of file and verify version
	bool bGotEndTag = false;
	unsigned short iLump = 0;
	unsigned short iLumpCount = 0;
	if ( !CBaseGameStats::GetLumpHeader( MAX_LUMP_COUNT, LoadBuffer, iLump, iLumpCount ) )
		return false;
	if ( iLump != TFSTATS_LUMP_VERSION )
	{
		Msg( "Didn't find version header.  Expected lump type TFSTATS_LUMP_VERSION, got lump type %d.  Skipping file.\n", iLump );
		return false;
	}
	TF_Gamestats_Version_t versionLump;
	CBaseGameStats::LoadLump( LoadBuffer, iLumpCount, sizeof( versionLump ), &versionLump );
	if ( versionLump.m_iMagic != TF_GAMESTATS_MAGIC )
	{
		Msg( "Incorrect magic # in version header.  Expected %x, got %x.  Skipping file.\n", TF_GAMESTATS_MAGIC, versionLump.m_iMagic );
		return false;
	}
	if ( versionLump.m_iVersion != TF_GAMESTATS_FILE_VERSION )
	{
		Msg( "Mismatched file version.  Expected file version %d, got %d. Skipping file.\n", TF_GAMESTATS_FILE_VERSION, versionLump.m_iVersion  );
		return false;
	}

	// read all the lumps in the file
	while( CBaseGameStats::GetLumpHeader( MAX_LUMP_COUNT, LoadBuffer, iLump, iLumpCount ) )
	{
		switch ( iLump )
		{
		case TFSTATS_LUMP_MAPHEADER: 
			{
				TF_Gamestats_LevelStats_t::LevelHeader_t header;
				CBaseGameStats::LoadLump( LoadBuffer, iLumpCount, sizeof( TF_Gamestats_LevelStats_t::LevelHeader_t ), &header );

				// quick sanity check on some data -- we get some stat files that start out OK but are corrupted later in the file
				if ( ( header.m_iRoundsPlayed < 0 ) || ( header.m_iTotalTime < 0 ) || ( header.m_iRoundsPlayed > 1000 ) )
					return false;

				// if there's no interesting data, skip this file.  (Need to have server not send it in this case.)
				if ( header.m_iTotalTime == 0 )
					return false;

				m_pCurrentGame = FindOrAddMapStats( header.m_szMapName );
				if ( m_pCurrentGame )
				{
					m_pCurrentGame->m_Header = header;
				}
				break; 
			}
		case TFSTATS_LUMP_MAPDEATH:
			{
				CUtlVector<TF_Gamestats_LevelStats_t::PlayerDeathsLump_t> playerDeaths;

				playerDeaths.SetCount( iLumpCount );
				CBaseGameStats::LoadLump( LoadBuffer, iLumpCount, sizeof( TF_Gamestats_LevelStats_t::PlayerDeathsLump_t ), static_cast<void*>( playerDeaths.Base() ) );
				if ( m_pCurrentGame )
				{
					m_pCurrentGame->m_aPlayerDeaths = playerDeaths;
				}
				break;
			}
		case TFSTATS_LUMP_MAPDAMAGE:
			{
				CUtlVector<TF_Gamestats_LevelStats_t::PlayerDamageLump_t> playerDamage;

				playerDamage.SetCount( iLumpCount );
				CBaseGameStats::LoadLump( LoadBuffer, iLumpCount, sizeof( TF_Gamestats_LevelStats_t::PlayerDamageLump_t ), static_cast<void*>( playerDamage.Base() ) );
				if ( m_pCurrentGame )
				{
					m_pCurrentGame->m_aPlayerDamage = playerDamage;
				}
				break;
			}		
		case TFSTATS_LUMP_CLASS:
			{
				Assert( m_pCurrentGame );
				Assert ( iLumpCount == ARRAYSIZE( m_pCurrentGame->m_aClassStats ) );
				if ( iLumpCount == ARRAYSIZE( m_pCurrentGame->m_aClassStats ) )
				{
					CBaseGameStats::LoadLump( LoadBuffer, ARRAYSIZE( m_pCurrentGame->m_aClassStats ), sizeof( m_pCurrentGame->m_aClassStats[0] ), 
						m_pCurrentGame->m_aClassStats );

					// quick sanity check on some data -- we get some stat files that start out OK but are corrupted later in the file
					for ( int i = 0; i < ARRAYSIZE( m_pCurrentGame->m_aClassStats ); i++ )
					{
						TF_Gamestats_ClassStats_t &classStats = m_pCurrentGame->m_aClassStats[i];
						if ( ( classStats.iSpawns < 0 ) || ( classStats.iSpawns > 10000 ) || ( classStats.iTotalTime < 0 ) || ( classStats.iTotalTime > 36000 * 20 ) ||
							( classStats.iKills < 0 ) || ( classStats.iKills > 10000 ) )
						{
							return false;
						}
					}
				}
				else
				{
					// mismatched lump size, possibly from different build, don't know how it interpret it, just skip over it
					return false;
				}				
				break;
			}
		case TFSTATS_LUMP_WEAPON:
			{
				Assert( m_pCurrentGame );
				Assert ( iLumpCount == ARRAYSIZE( m_pCurrentGame->m_aWeaponStats ) );
				if ( iLumpCount == ARRAYSIZE( m_pCurrentGame->m_aWeaponStats ) )
				{
					CBaseGameStats::LoadLump( LoadBuffer, ARRAYSIZE( m_pCurrentGame->m_aWeaponStats ), sizeof( m_pCurrentGame->m_aWeaponStats[0] ), 
						m_pCurrentGame->m_aWeaponStats );

					// quick sanity check on some data -- we get some stat files that start out OK but are corrupted later in the file
					if ( ( m_pCurrentGame->m_aWeaponStats[TF_WEAPON_MEDIGUN].iShotsFired < 0 ) || ( m_pCurrentGame->m_aWeaponStats[TF_WEAPON_MEDIGUN].iShotsFired > 100000 )
						|| ( m_pCurrentGame->m_aWeaponStats[TF_WEAPON_FLAMETHROWER_ROCKET].iShotsFired != 0 ) ) // check that unused weapon has 0 shots
					{
						return false;
					}
					
				}				
				else
				{
					// mismatched lump size, possibly from different build, don't know how it interpret it, just skip over it
					return false;				
				}				
				break;
			}
		case TFSTATS_LUMP_ENDTAG:
			{
				// check that end tag is valid -- should be version lump again
				TF_Gamestats_Version_t versionLump;
				CBaseGameStats::LoadLump( LoadBuffer, iLumpCount, sizeof( versionLump ), &versionLump );
				if ( versionLump.m_iMagic != TF_GAMESTATS_MAGIC )
				{
					Msg( "Incorrect magic # in version header.  Expected %x, got %x.  Skipping file.\n", TF_GAMESTATS_MAGIC, versionLump.m_iMagic );
					return false;
				}
				if ( versionLump.m_iVersion != TF_GAMESTATS_FILE_VERSION )
				{
					Msg( "Mismatched file version.  Expected file version %d, got %d. Skipping file.\n", TF_GAMESTATS_FILE_VERSION, versionLump.m_iVersion  );
					return false;
				}
				bGotEndTag = true;
				break;
			}

		}
	}

	return bGotEndTag;
}
#endif

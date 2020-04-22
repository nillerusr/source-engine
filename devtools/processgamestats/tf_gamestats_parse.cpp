//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// 
//

#include "stdafx.h"
#include <stdio.h>
#include <process.h>
#include <string.h>
#include <windows.h>
#include <sys/stat.h>
#include <time.h>
#include "interface.h"
#include "imysqlwrapper.h"
#include "tier1/utlvector.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlstring.h"
#include "tier1/utldict.h"
#include "tier2/tier2.h"
#include "filesystem.h"

#include "cbase.h"
#include "gamestats.h"
class CBaseObject;
#include "tf/tf_gamestats.h"
#include "base_gamestats_parse.h"
#include "tier0/icommandline.h"

#include <string>

static TFReportedStats_t										g_reportedStats;

extern void v_escape_string (std::string& s);

//-----------------------------------------------------------------------------
// Weapons.
//-----------------------------------------------------------------------------
static const char *s_aWeaponNames[] =
{
	"NONE",
	"BAT",
	"BOTTLE", 
	"FIREAXE",
	"CLUB",
	"CROWBAR",
	"KNIFE",
	"MEDIKIT",
	"PIPE",
	"SHOVEL",
	"WRENCH",
	"BONESAW",
	"SHOTGUN_PRIMARY",
	"SHOTGUN_SECONDARY",
	"SNIPERRIFLE",
	"MINIGUN",
	"SMG",
	"SYRINGEGUN_MEDIC",
	"TRANQ",
	"ROCKETLAUNCHER",
	"GRENADELAUNCHER",
	"PIPEBOMBLAUNCHER",
	"FLAMETHROWER",
	"GRENADE_NORMAL",
	"GRENADE_CONCUSSION",
	"GRENADE_NAIL",
	"GRENADE_MIRV",
	"GRENADE_MIRV_DEMOMAN",
	"GRENADE_NAPALM",
	"GRENADE_GAS",
	"GRENADE_EMP",
	"GRENADE_CALTROP",
	"GRENADE_PIPEBOMB",
	"GRENADE_SMOKE_BOMB",
	"GRENADE_HEAL",
	"PISTOL",
	"REVOLVER",
	"NAILGUN",
	"PDA",
	"PDA_DEMOMAN",
	"PDA_ENGINEER",
	"PDA_SPY",
	"BUILDER",
	"MEDIGUN",
	"FLAG",
	"GRENADE_MIRVBOMB",
	"FLAMETHROWER_ROCKET",
	"GRENADE_DEMOMAN",
	"SENTRY_BULLET",
	"SENTRY_ROCKET",
	"DISPENSER",
	"INVIS",
};

//-----------------------------------------------------------------------------
// Classes
//-----------------------------------------------------------------------------
static const char *s_aClassNames[] =
{
	"UNDEFINED",
	"SCOUT",
	"SNIPER",
	"SOLDIER",
	"DEMOMAN",
	"MEDIC",
	"HEAVYWEAPONS",
	"PYRO",
	"SPY",
	"ENGINEER",
	"CIVILIAN",
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static bool LoadCustomDataFromBuffer( CUtlBuffer &LoadBuffer )
{
	g_reportedStats.Clear();
	return g_reportedStats.LoadCustomDataFromBuffer( LoadBuffer );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static const char *ClassIdToAlias( int iClass )
{
	if ( ( iClass >= ARRAYSIZE( s_aClassNames ) ) || ( iClass < 0 ) )
		return NULL;

	return s_aClassNames[iClass];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *WeaponIdToAlias( int iWeapon )
{
	if ( ( iWeapon >= ARRAYSIZE( s_aWeaponNames ) ) || ( iWeapon < 0 ) )
		return NULL;

	return s_aWeaponNames[iWeapon];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void DescribeTF2Stats()
{
#if 0	// not up to date w/latest stats code. 
	int iMap;
	for ( iMap = g_dictMapStats.First(); iMap != g_dictMapStats.InvalidIndex(); iMap = g_dictMapStats.Next( iMap ) )
	{
		// Get the current map.
		TF_Gamestats_LevelStats_t *pCurrentMap = &g_dictMapStats[iMap];

		Msg( " --- %s ------\n   %d deaths\n   %.2f seconds total playtime\n", pCurrentMap->m_Header.m_szMapName, 
			pCurrentMap->m_aPlayerDeaths.Count(), pCurrentMap->m_Header.m_flTime );
		for( int i = 0; i < pCurrentMap->m_aPlayerDeaths.Count(); i++ )
		{
			Msg( "   %s killed %s with %s at (%d,%d,%d), distance %d\n",
				ClassIdToAlias( pCurrentMap->m_aPlayerDeaths[ i ].iAttackClass ),
				ClassIdToAlias( pCurrentMap->m_aPlayerDeaths[ i ].iTargetClass ),
				WeaponIdToAlias( pCurrentMap->m_aPlayerDeaths[ i ].iWeapon ), 
				pCurrentMap->m_aPlayerDeaths[ i ].nPosition[ 0 ],
				pCurrentMap->m_aPlayerDeaths[ i ].nPosition[ 1 ],
				pCurrentMap->m_aPlayerDeaths[ i ].nPosition[ 2 ],
				pCurrentMap->m_aPlayerDeaths[ i ].iDistance );
		}
		Msg( "\n---------------------------------\n\n   %d damage records\n", pCurrentMap->m_aPlayerDamage.Count() );

		for( int i = 0; i < pCurrentMap->m_aPlayerDamage.Count(); i++ )
		{
			Msg( "   %.2f : %s at (%d,%d,%d) caused %d damage to %s with %s at (%d,%d,%d)%s%s\n",
				pCurrentMap->m_aPlayerDamage[ i ].fTime,
				ClassIdToAlias( pCurrentMap->m_aPlayerDamage[ i ].iAttackClass ),
				pCurrentMap->m_aPlayerDamage[ i ].nAttackerPosition[ 0 ],
				pCurrentMap->m_aPlayerDamage[ i ].nAttackerPosition[ 1 ],
				pCurrentMap->m_aPlayerDamage[ i ].nAttackerPosition[ 2 ],
				pCurrentMap->m_aPlayerDamage[ i ].iDamage,
				ClassIdToAlias( pCurrentMap->m_aPlayerDamage[ i ].iTargetClass ),
				WeaponIdToAlias( pCurrentMap->m_aPlayerDamage[ i ].iWeapon ), 
				pCurrentMap->m_aPlayerDamage[ i ].nTargetPosition[ 0 ],
				pCurrentMap->m_aPlayerDamage[ i ].nTargetPosition[ 1 ],
				pCurrentMap->m_aPlayerDamage[ i ].nTargetPosition[ 2 ],
				pCurrentMap->m_aPlayerDamage[ i ].iCrit ? ", CRIT!" : "",
				pCurrentMap->m_aPlayerDamage[ i ].iKill ? ", KILL" : "" );
		}

		Msg( "\n" );
	}
#endif
}
extern CUtlDict< int, unsigned short > g_mapOrder;
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void InsertTF2Data( bool bDeathOnly, char const *szStatsFileUserID, time_t fileTime, IMySQL *sql, int iStatsFileVersion )
{
	if ( !sql )
		return;

	std::string userid;
	userid = szStatsFileUserID;
	v_escape_string( userid );

	
	char szDate[128]="Now()";
	if ( fileTime > 0 )
	{
		tm * pTm = localtime( &fileTime );
		Q_snprintf( szDate, ARRAYSIZE( szDate ), "'%04d-%02d-%02d %02d:%02d:%02d'",
			pTm->tm_year + 1900, pTm->tm_mon + 1, pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec );
	}

	char q[4096];

	
	{
		int iMap;
		for ( iMap = g_reportedStats.m_dictMapStats.First(); iMap != g_reportedStats.m_dictMapStats.InvalidIndex(); iMap = g_reportedStats.m_dictMapStats.Next( iMap ) )
		{
			// Get the current map.
			TF_Gamestats_LevelStats_t *pCurrentMap = &g_reportedStats.m_dictMapStats[iMap];

#if 1
			int slot = g_mapOrder.Find( pCurrentMap->m_Header.m_szMapName );
			if ( slot == g_mapOrder.InvalidIndex() )
			{
				if ( Q_stricmp( pCurrentMap->m_Header.m_szMapName, "devtest" ) )
					continue;
			}
#endif

			std::string mapname;
			mapname = pCurrentMap->m_Header.m_szMapName;
			v_escape_string( mapname );

			std::string tag;
			tag = ""; // pCurrentMap->m_Tag.m_szTagText;
			v_escape_string( tag );

			int mapversion = 0;

			

			/*
			for( int i = 0; i < pCurrentMap->m_aPlayerDeaths.Count(); i++ )
			{
				Msg( "   %s killed %s with %s at (%d,%d,%d), distance %d\n",
					ClassIdToAlias( pCurrentMap->m_aPlayerDeaths[ i ].iAttackClass ),
					ClassIdToAlias( pCurrentMap->m_aPlayerDeaths[ i ].iTargetClass ),
					WeaponIdToAlias( pCurrentMap->m_aPlayerDeaths[ i ].iWeapon ), 
					pCurrentMap->m_aPlayerDeaths[ i ].nPosition[ 0 ],
					pCurrentMap->m_aPlayerDeaths[ i ].nPosition[ 1 ],
					pCurrentMap->m_aPlayerDeaths[ i ].nPosition[ 2 ],
					pCurrentMap->m_aPlayerDeaths[ i ].iDistance );
			}
			*/
			// Deal with deaths
			for ( int i = 0; i < pCurrentMap->m_aPlayerDeaths.Count(); ++i )
			{
				Q_snprintf( q, sizeof( q ), "REPLACE into %s_deaths (UserID,Tag,LastUpdate,MapName,MapVersion,DeathIndex,X,Y,Z) values (\"%s\",\"%s\",Now(),\"%-.20s\",%d,%d,%d,%d,%d);",
					"tf",
					userid.c_str(),
					tag.c_str(),
					mapname.c_str(),
					mapversion,
					i,
					pCurrentMap->m_aPlayerDeaths[ i ].nPosition[ 0 ],
					pCurrentMap->m_aPlayerDeaths[ i ].nPosition[ 1 ],
					pCurrentMap->m_aPlayerDeaths[ i ].nPosition[ 2 ] );

				int retcode = sql->Execute( q );
				if ( retcode != 0 )
				{
					printf( "Query %s failed\n", q );
					return;
				}
			}
		}
		if ( bDeathOnly )
			return;
	}

	int iMap;
	for ( iMap = g_reportedStats.m_dictMapStats.First(); iMap != g_reportedStats.m_dictMapStats.InvalidIndex(); 
		iMap = g_reportedStats.m_dictMapStats.Next( iMap ) )
	{
		// Get the current map.
		TF_Gamestats_LevelStats_t *pCurrentMap = &g_reportedStats.m_dictMapStats[iMap];
		std::string mapname = pCurrentMap->m_Header.m_szMapName;
		v_escape_string( mapname );

		// insert map data into database
		Q_snprintf( q, sizeof( q ), "INSERT into tf_mapdata (ServerID,TimeSubmitted,MapName,RoundsPlayed,TotalTime,BlueWins,RedWins,Stalemates,BlueSuddenDeathWins,RedSuddenDeathWins) "
			"values (\"%s\",%s,\"%s\",%d,%d,%d,%d,%d,%d,%d);",
			szStatsFileUserID,szDate, mapname.c_str(), pCurrentMap->m_Header.m_iRoundsPlayed, pCurrentMap->m_Header.m_iTotalTime, pCurrentMap->m_Header.m_iBlueWins,
			pCurrentMap->m_Header.m_iRedWins, pCurrentMap->m_Header.m_iStalemates, pCurrentMap->m_Header.m_iBlueSuddenDeathWins, pCurrentMap->m_Header.m_iRedSuddenDeathWins );

//		Msg( "%s\n", q );

		int retcode = sql->Execute( q );
		if ( retcode != 0 )
		{
			Msg( "Query %s failed\n", q );
			return;
		}

		// insert the class data
		for ( int i = TF_CLASS_UNDEFINED + 1; i < TF_CLASS_CIVILIAN; i++ )
		{
			TF_Gamestats_ClassStats_t &classStats = pCurrentMap->m_aClassStats[i];
			if  ( 0 == classStats.iSpawns )	// skip any classes that have had no spawns
				continue;

			Q_snprintf( q, sizeof( q ), "INSERT into tf_classdata (ServerID,MapName,TimeSubmitted,Class,Spawns,TotalTime,Score,Kills,Deaths,Assists,Captures) "
				"values (\"%s\",\"%s\",%s,%d,%d,%d,%d,%d,%d,%d,%d);",
				szStatsFileUserID, mapname.c_str(), szDate, i, classStats.iSpawns, classStats.iTotalTime, classStats.iScore, classStats.iKills, classStats.iDeaths,
				classStats.iAssists, classStats.iCaptures );

//			Msg( "%s\n", q );
			int retcode = sql->Execute( q );
			if ( retcode != 0 )
			{
				Msg( "Query %s failed\n", q );
				return;
			}
		}

		// insert the weapon data
		for ( int i = TF_WEAPON_NONE+1; i < TF_WEAPON_COUNT; i++ )
		{
			TF_Gamestats_WeaponStats_t &weaponStats = pCurrentMap->m_aWeaponStats[i];

			// skip any weapons that have no shots fired
			if  ( 0 == weaponStats.iShotsFired )	
				continue;

			Q_snprintf( q, sizeof( q ), "INSERT into tf_weapondata (ServerID, MapName, TimeSubmitted,WeaponID,ShotsFired,ShotsFiredCrit,ShotsHit,DamageTotal,"
				"HitsWithKnownDistance,DistanceTotal) "
				"values (\"%s\",\"%s\",%s,%d,%d,%d,%d,%d,%d,%llu)",
				szStatsFileUserID, mapname.c_str(), szDate, i, weaponStats.iShotsFired, weaponStats.iCritShotsFired, weaponStats.iHits, weaponStats.iTotalDamage,
				weaponStats.iHitsWithKnownDistance, weaponStats.iTotalDistance );

//			Msg( "%s\n", q );
			int retcode = sql->Execute( q );
			if ( retcode != 0 )
			{
				Msg( "Query %s failed\n", q );
				return;
			}
		}
	}
}

bool g_bDeathOnly = false;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int TF_ParseCustomGameStatsData( ParseContext_t *ctx )
{
	FILE *fp = fopen( ctx->file, "rb" );
	if ( !fp )
		return CUSTOMDATA_FAILED;

	CUtlBuffer statsBuffer;

	struct _stat sb;
	_stat( ctx->file, &sb );
	statsBuffer.Clear();
	statsBuffer.EnsureCapacity( sb.st_size );
	fread( statsBuffer.Base(), sb.st_size, 1, fp );
	statsBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, sb.st_size );
	fclose( fp );

	if ( memcmp( statsBuffer.Base(), "\"gamestats\"", 11 ) == 0 )
	{
		Msg( "Got new-style file format that we don't support, skipping\n" );
		return CUSTOMDATA_SUCCESS;
	}

	if ( memcmp( statsBuffer.Base(), "PERFDATA:", 9 ) == 0 )
	{
		ProcessPerfData( ctx->mysql, sb.st_mtime, "tf_perfdata", ( ( char * ) statsBuffer.Base() ) + 9 );
		return CUSTOMDATA_SUCCESS;
	}
	char shortname[ 128 ];
	Q_FileBase( ctx->file, shortname, sizeof( shortname ) );

	char szCurrentStatsFileUserID[17];
	int iCurrentStatsFileVersion;

	iCurrentStatsFileVersion = statsBuffer.GetShort();
	statsBuffer.Get( szCurrentStatsFileUserID, 16 );
	szCurrentStatsFileUserID[ sizeof( szCurrentStatsFileUserID ) - 1 ] = 0;

	unsigned int iCheckIfStandardDataSaved = statsBuffer.GetUnsignedInt();
	if( iCheckIfStandardDataSaved != GAMESTATS_STANDARD_NOT_SAVED )
	{
		// we don't care about the standard data, so why is it here?
		return CUSTOMDATA_FAILED;
	}

	// check for custom data
	bool bHasCustomData = (statsBuffer.TellPut() != statsBuffer.TellGet());
	if( !bHasCustomData )
	{
		// where's our data?
		return CUSTOMDATA_FAILED;
	}

	if ( !LoadCustomDataFromBuffer( statsBuffer ) )
		return CUSTOMDATA_FAILED;

	if( ctx->describeonly )
	{
		DescribeTF2Stats();
	}
	else
	{
		if ( CommandLine()->CheckParm( "-deathsonly" ) )
		{
			g_bDeathOnly = true;
		}

		InsertTF2Data( g_bDeathOnly, szCurrentStatsFileUserID, sb.st_mtime, ctx->mysql, iCurrentStatsFileVersion );
	}

	return CUSTOMDATA_SUCCESS;
}

//-----------------------------------------------------------------------------
// Purpose: Called after all new files have been parsed & imported
//-----------------------------------------------------------------------------
void TF_PostImport( IMySQL *sql )
{
#if 0	// now handled by PHP script
	if ( g_bDeathOnly )
		return;
	// All the new data files have been imported to SQL.  Now, do a rollup of the raw data into the rollup tables.

	// delete existing rollup for class data
	int retcode = sql->Execute( "delete from tf_classdata_rollup" );
	if ( 0 != retcode )
	{
		Msg( "Failed to delete from tf_classdata_rollup\n" );
		return;
	}
	// create new rollup for class data
	retcode = sql->Execute( "insert into tf_classdata_rollup (class,spawns,totaltime,score,kills,deaths,assists,captures) "
		"select class,sum(spawns),sum(totaltime),sum(score),sum(kills),sum(deaths),sum(assists),sum(captures) from tf_classdata group by class;" );
	if ( 0 != retcode )
	{
		Msg( "Failed to create class data rollup\n" );
		return;
	}

	// delete existing rollup for map data
	retcode = sql->Execute( "delete from tf_mapdata_rollup" );
	if ( 0 != retcode )
	{
		Msg( "Failed to delete from tf_mapdata_rollup\n" );
		return;
	}
	// create new rollup for map data
	retcode = sql->Execute( "insert into tf_mapdata_rollup (mapname,roundsplayed,totaltime,bluewins,redwins,stalemates) "
		"select mapname,sum(roundsplayed),sum(totaltime),sum(bluewins),sum(redwins),sum(stalemates) from tf_mapdata group by mapname;" );
	if ( 0 != retcode )
	{
		Msg( "Failed to create map data rollup\n" );
		return;
	}
#endif
}

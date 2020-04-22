//========= Copyright Valve Corporation, All rights reserved. ============//
#include "stdafx.h"
#include <stdio.h>
#include <process.h>
#include <string.h>
#include <windows.h>
#include <sys/stat.h>
#include <time.h>
#include <string>


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
#include "episodic/ep2_gamestats.h"
#include "base_gamestats_parse.h"

void v_escape_string( std::string& s );

// EP2 custom data blob parsing stuff

static Ep2LevelStats_t									*g_pCurrentMap;
static CUtlDict<Ep2LevelStats_t, unsigned short>		g_dictMapStats;

extern CUtlDict< int, unsigned short > g_mapOrder;

void DescribeData( BasicGameStats_t &stats, const char *szStatsFileUserID, int iStatsFileVersion );
void InsertData( CUtlDict< int, unsigned short >& mapOrder, IMySQL *sql, BasicGameStats_t &gs, const char *szStatsFileUserID, int iStatsFileVersion, const char *gamename, char const *tag = NULL );

struct CounterInfo_t
{
	int type;
	char const *counterName;
};

static CounterInfo_t g_IntCounterNames[ ] =
{
	{ Ep2LevelStats_t::COUNTER_CRATESSMASHED, "Crates Smashed" },
	{ Ep2LevelStats_t::COUNTER_OBJECTSPUNTED, "Objects Punted" },
	{ Ep2LevelStats_t::COUNTER_VEHICULARHOMICIDES, "Vehicular Homicides" },
	{ Ep2LevelStats_t::COUNTER_DISTANCE_INVEHICLE, "Distance in Vehicles (inches)" },
	{ Ep2LevelStats_t::COUNTER_DISTANCE_ONFOOT, "Distance on Foot (inches)" },
	{ Ep2LevelStats_t::COUNTER_DISTANCE_ONFOOTSPRINTING, "Distance on Foot Sprint (inches)" },
	{ Ep2LevelStats_t::COUNTER_FALLINGDEATHS, "Falling Death" },
	{ Ep2LevelStats_t::COUNTER_VEHICLE_OVERTURNED, "Flipped Vehicles" },
	{ Ep2LevelStats_t::COUNTER_LOADGAME_STILLALIVE, "Loadgame while player alive" },
	{ Ep2LevelStats_t::COUNTER_LOADS, "Number of saves (w/autosaves)" },
	{ Ep2LevelStats_t::COUNTER_SAVES, "Number of game loads" },
	{ Ep2LevelStats_t::COUNTER_GODMODES, "Times entered God mode" },
	{ Ep2LevelStats_t::COUNTER_NOCLIPS, "Times entered NoClip mode" },
};

static CounterInfo_t g_FloatCounterNames[ ] =
{
	{ Ep2LevelStats_t::COUNTER_DAMAGETAKEN, "Damage Taken" },
};

char const *SaveType( int type )
{
	switch ( type )
	{
	default:
	case Ep2LevelStats_t::SaveGameInfoRecord2_t::TYPE_UNKNOWN:
		break;
	case Ep2LevelStats_t::SaveGameInfoRecord2_t::TYPE_AUTOSAVE:
		return "Auto Save";
	case Ep2LevelStats_t::SaveGameInfoRecord2_t::TYPE_USERSAVE:
		return "User Save";
	}
	return "Unknown";
}

#define INCHES_TO_MILES ( 1.0 / ( 5280.0 * 12.0 ) )
#define INCHES_TO_FEET ( 1.0 / 12.0 )

void DescribeEp2Stats()
{
	int i;
	int iMap;
	for ( iMap =  g_dictMapStats.First(); iMap != g_dictMapStats.InvalidIndex(); iMap =  g_dictMapStats.Next( iMap ) )
	{
		// Get the current map.
		Ep2LevelStats_t *pCurrentMap = &g_dictMapStats[iMap];

		Msg( "map version[ %d ], user tag[ %s ]\n", pCurrentMap->m_Tag.m_nMapVersion, pCurrentMap->m_Tag.m_szTagText );

		Msg( " --- %s ------\n   %d deaths\n", pCurrentMap->m_Header.m_szMapName, pCurrentMap->m_aPlayerDeaths.Count() );
		for ( i = 0; i < pCurrentMap->m_aPlayerDeaths.Count(); ++i )
		{
			Msg( "   @ ( %d, %d, %d )\n",
				pCurrentMap->m_aPlayerDeaths[ i ].nPosition[ 0 ],
				pCurrentMap->m_aPlayerDeaths[ i ].nPosition[ 1 ],
				pCurrentMap->m_aPlayerDeaths[ i ].nPosition[ 2 ] );
		}

		Msg( "  -- Weapon Stats --\n" );

		CUtlDict< Ep2LevelStats_t::WeaponLump_t, int > &wdict = pCurrentMap->m_dictWeapons;
		for ( i = wdict.First(); i != wdict.InvalidIndex(); i = wdict.Next( i ) )
		{
			const Ep2LevelStats_t::WeaponLump_t &lump = wdict[ i ];
			double acc = 0.0f;
			if ( lump.m_nShots > 0 )
			{
				acc = 100.0f * (double)lump.m_nHits / (double)lump.m_nShots;
			}
			Msg( "   %32.32s:  shots %5d hits %5d damage %8.2f [accuracy %8.2f %%]\n", wdict.GetElementName( i ), lump.m_nShots, lump.m_nHits, lump.m_flDamageInflicted, acc );
		}
		Msg( "  -- NPC Stats -- \n" );

		CUtlDict< Ep2LevelStats_t::EntityDeathsLump_t, int > &dict = pCurrentMap->m_dictEntityDeaths;
		for ( i = dict.First(); i != dict.InvalidIndex(); i = dict.Next( i ) )
		{
			const Ep2LevelStats_t::EntityDeathsLump_t &lump = dict[ i ];
			Msg( "   %32.32s:  bodycount %5d killedplayer %3d\n", dict.GetElementName( i ), lump.m_nBodyCount, lump.m_nKilledPlayer );
		}

		for ( i = 0; i < Ep2LevelStats_t::NUM_INTCOUNTER_TYPES; ++i )
		{
			if ( i == Ep2LevelStats_t::COUNTER_DISTANCE_INVEHICLE ||
				i == Ep2LevelStats_t::COUNTER_DISTANCE_ONFOOT ||
				i == Ep2LevelStats_t::COUNTER_DISTANCE_ONFOOTSPRINTING ) 
			{
				Msg( "   %32.32s:  %8I64u [%8.3f feet] [%8.3f miles]\n", g_IntCounterNames[ i ].counterName, pCurrentMap->m_IntCounters[ i ], INCHES_TO_FEET * (double)pCurrentMap->m_IntCounters[ i ], INCHES_TO_MILES * (double)pCurrentMap->m_IntCounters[ i ] );
			}
			else
			{
				Msg( "   %32.32s:  %8I64u\n", g_IntCounterNames[ i ].counterName, pCurrentMap->m_IntCounters[ i ] );
			}
		}
		for ( i = 0; i < Ep2LevelStats_t::NUM_FLOATCOUNTER_TYPES; ++i )
		{
			Msg( "   %32.32s:  %8.2f\n", g_FloatCounterNames[ i ].counterName, pCurrentMap->m_FloatCounters[ i ] );
		}

		Ep2LevelStats_t::SaveGameInfo_t *sg = &pCurrentMap->m_SaveGameInfo;
		Msg( "  -- Save Game --\n" );
		time_t t = (time_t)sg->m_nCurrentSaveFileTime;
		struct tm *timestamp;
		timestamp = localtime( &t );
		if ( t == (time_t)0 )
		{
			Msg( "   No save file\n" );
		}
		else
		{
			Msg( "  Current save %s file time %s\n", sg->m_sCurrentSaveFile.String(), asctime( timestamp ) );
		}
		for ( i = 0; i < sg->m_Records.Count(); ++i )
		{
			const Ep2LevelStats_t::SaveGameInfoRecord2_t &rec = sg->m_Records[ i ];
			Msg( "  %3d deaths, saved at (%5d %5d %5d) with health %3d %s\n",
				rec.m_nNumDeaths, 
				(int)rec.m_nSavePos[ 0 ],
				(int)rec.m_nSavePos[ 1 ],
				(int)rec.m_nSavePos[ 2 ],
				(int)rec.m_nSaveHealth,
				SaveType( (int)rec.m_SaveType ) );
		}

		Msg( "  -- Generic Stats --\n" );

		CUtlDict< Ep2LevelStats_t::GenericStatsLump_t, int > &gdict = pCurrentMap->m_dictGeneric;
		for ( i = gdict.First(); i != gdict.InvalidIndex(); i = gdict.Next( i ) )
		{
			const Ep2LevelStats_t::GenericStatsLump_t &lump = gdict[ i ];
			Msg( "   %32.32s:  count %u total %f @[%d, %d, %d]\n", gdict.GetElementName( i ), lump.m_unCount, lump.m_flCurrentValue, lump.m_Pos[ 0 ], lump.m_Pos[ 1 ], lump.m_Pos[ 2 ] );
		}

		Msg( "\n" );
	}
}

void InsertCustomData( CUtlDict< int, unsigned short >& mapOrder, IMySQL *sql, const char *szStatsFileUserID, int iStatsFileVersion, const char *gamename )
{
	if ( !sql )
		return;

	char q[ 1024 ];
	char counternames[ 512 ];

	std::string userid;
	userid = szStatsFileUserID;
	v_escape_string( userid );

	// Deal with the maps
	int i;
	int iMap;
	for ( iMap = g_dictMapStats.First(); iMap != g_dictMapStats.InvalidIndex(); iMap =  g_dictMapStats.Next( iMap ) )
	{
		// Get the current map.
		Ep2LevelStats_t *pCurrentMap = &g_dictMapStats[iMap];

		char const *pszMapName = g_dictMapStats.GetElementName( iMap );
		std::string mapname;
		mapname = pszMapName;
		v_escape_string( mapname );

		std::string tag;
		tag = pCurrentMap->m_Tag.m_szTagText;
		v_escape_string( tag );

		int mapversion = pCurrentMap->m_Tag.m_nMapVersion;

#if 1
		int slot = mapOrder.Find( pszMapName );
		if ( slot == mapOrder.InvalidIndex() )
		{
			if ( Q_stricmp( pszMapName, "devtest" ) )
				continue;
		}
#endif

		// Deal with deaths
		for ( i = 0; i < pCurrentMap->m_aPlayerDeaths.Count(); ++i )
		{
			Q_snprintf( q, sizeof( q ), "REPLACE into %s_deaths (UserID,Tag,LastUpdate,MapName,MapVersion,DeathIndex,X,Y,Z) values (\"%s\",\"%s\",Now(),\"%-.20s\",%d,%d,%d,%d,%d);",
				gamename,
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

		// Deal with weapon data
		CUtlDict< Ep2LevelStats_t::WeaponLump_t, int > &wdict = pCurrentMap->m_dictWeapons;
		for ( i = wdict.First(); i != wdict.InvalidIndex(); i = wdict.Next( i ) )
		{
			const Ep2LevelStats_t::WeaponLump_t &lump = wdict[ i ];

			std::string wname = wdict.GetElementName( i );
			v_escape_string( wname );

			Q_snprintf( q, sizeof( q ), "REPLACE into %s_weapons (UserID,Tag,LastUpdate,MapName,MapVersion,Weapon,Shots,Hits,Damage) values (\"%s\",\"%s\",Now(),\"%-.20s\",%d,\"%-.32s\",%d,%d,%g);",
				gamename,
				userid.c_str(),
				tag.c_str(),
				mapname.c_str(),
				mapversion,
				wname.c_str(),
				lump.m_nShots, 
				lump.m_nHits, 
				lump.m_flDamageInflicted
				);

			int retcode = sql->Execute( q );
			if ( retcode != 0 )
			{
				printf( "Query %s failed\n", q );
				return;
			}
		}

		CUtlDict< Ep2LevelStats_t::EntityDeathsLump_t, int > &dict = pCurrentMap->m_dictEntityDeaths;
		for ( i = dict.First(); i != dict.InvalidIndex(); i = dict.Next( i ) )
		{
			const Ep2LevelStats_t::EntityDeathsLump_t &lump = dict[ i ];

			std::string ename = dict.GetElementName( i );
			v_escape_string( ename );

			Q_snprintf( q, sizeof( q ), "REPLACE into %s_entities (UserID,Tag,LastUpdate,MapName,MapVersion,Entity,BodyCount,KilledPlayer) values (\"%s\",\"%s\",Now(),\"%-.20s\",%d,\"%-.32s\",%d,%d);",
				gamename,
				userid.c_str(),
				tag.c_str(),
				mapname.c_str(),
				mapversion,
				ename.c_str(),
				lump.m_nBodyCount, 
				lump.m_nKilledPlayer
				);

			int retcode = sql->Execute( q );
			if ( retcode != 0 )
			{
				printf( "Query %s failed\n", q );
				return;
			}
		}

		// Counters
		Q_snprintf( counternames, sizeof( counternames ), 
				"CRATESSMASHED,"\
				"OBJECTSPUNTED,"\
				"VEHICULARHOMICIDES,"\
				"DISTANCE_INVEHICLE,"\
				"DISTANCE_ONFOOT,"\
				"DISTANCE_ONFOOTSPRINTING,"\
				"FALLINGDEATHS,"\
				"VEHICLE_OVERTURNED,"\
				"LOADGAME_STILLALIVE,"\
				"LOADS,"\
				"SAVES,"\
				"GODMODES,"\
				"NOCLIPS,"\
				"DAMAGETAKEN" );
		Q_snprintf( q, sizeof( q ), "REPLACE into %s_counters (UserID,Tag,LastUpdate,MapName,MapVersion,%s) values (\"%s\",\"%s\",Now(),\"%-.20s\",%d,"\
			"%u,"\
			"%u,"\
			"%u,"\
		    "%I64u,"\
			"%I64u,"\
			"%I64u,"\
			"%u,"\
			"%u,"\
			"%u,"\
			"%u,"\
			"%u,"\
			"%u,"\
			"%u,"\
			"%f);",
			gamename,
			counternames,
			userid.c_str(),
			tag.c_str(),
			mapname.c_str(),
			mapversion,
			(uint32)pCurrentMap->m_IntCounters[ Ep2LevelStats_t::COUNTER_CRATESSMASHED ],
			(uint32)pCurrentMap->m_IntCounters[ Ep2LevelStats_t::COUNTER_OBJECTSPUNTED ],
			(uint32)pCurrentMap->m_IntCounters[ Ep2LevelStats_t::COUNTER_VEHICULARHOMICIDES ],
			pCurrentMap->m_IntCounters[ Ep2LevelStats_t::COUNTER_DISTANCE_INVEHICLE ],
			pCurrentMap->m_IntCounters[ Ep2LevelStats_t::COUNTER_DISTANCE_ONFOOT ],
			pCurrentMap->m_IntCounters[ Ep2LevelStats_t::COUNTER_DISTANCE_ONFOOTSPRINTING ],
			(uint32)pCurrentMap->m_IntCounters[ Ep2LevelStats_t::COUNTER_FALLINGDEATHS ],
			(uint32)pCurrentMap->m_IntCounters[ Ep2LevelStats_t::COUNTER_VEHICLE_OVERTURNED ],
			(uint32)pCurrentMap->m_IntCounters[ Ep2LevelStats_t::COUNTER_LOADGAME_STILLALIVE ],
			(uint32)pCurrentMap->m_IntCounters[ Ep2LevelStats_t::COUNTER_LOADS ],
			(uint32)pCurrentMap->m_IntCounters[ Ep2LevelStats_t::COUNTER_SAVES ],
			(uint32)pCurrentMap->m_IntCounters[ Ep2LevelStats_t::COUNTER_GODMODES ],
			(uint32)pCurrentMap->m_IntCounters[ Ep2LevelStats_t::COUNTER_NOCLIPS ],
			(double)pCurrentMap->m_FloatCounters[ Ep2LevelStats_t::COUNTER_DAMAGETAKEN ]
			);

		int retcode = sql->Execute( q );
		if ( retcode != 0 )
		{
			printf( "Query %s failed\n", q );
			return;
		}

		Ep2LevelStats_t::SaveGameInfo_t *sg = &pCurrentMap->m_SaveGameInfo;
		/*
		Msg( "  -- Save Game --\n" );
		time_t t = (time_t)sg->m_nCurrentSaveFileTime;
		struct tm *timestamp;
		timestamp = localtime( &t );
		if ( t == (time_t)0 )
		{
			Msg( "   No save file\n" );
		}
		else
		{
			Msg( "  Current save %s file time %s\n", sg->m_sCurrentSaveFile.String(), asctime( timestamp ) );
		}
		*/
		for ( i = 0; i < sg->m_Records.Count(); ++i )
		{
			const Ep2LevelStats_t::SaveGameInfoRecord2_t &rec = sg->m_Records[ i ];

			Q_snprintf( q, sizeof( q ), "REPLACE into %s_saves (UserID,Tag,LastUpdate,MapName,MapVersion,FirstDeath,NumDeaths,X,Y,Z,Health,SaveType) values (\"%s\",\"%s\",Now(),\"%-.20s\",%d,%d,%d,%d,%d,%d,%d,%d);",
				gamename,
				userid.c_str(),
				tag.c_str(),
				mapname.c_str(),
				mapversion,
				rec.m_nFirstDeathIndex,
				rec.m_nNumDeaths, 
				(int)rec.m_nSavePos[ 0 ],
				(int)rec.m_nSavePos[ 1 ],
				(int)rec.m_nSavePos[ 2 ],
				(int)rec.m_nSaveHealth,
				(int)rec.m_SaveType
				);

			int retcode = sql->Execute( q );
			if ( retcode != 0 )
			{
				printf( "Query %s failed\n", q );
				return;
			}
		}


		// Deal with generic stats data
		CUtlDict< Ep2LevelStats_t::GenericStatsLump_t, int > &gdict = pCurrentMap->m_dictGeneric;
		for ( i = gdict.First(); i != gdict.InvalidIndex(); i = gdict.Next( i ) )
		{
			const Ep2LevelStats_t::GenericStatsLump_t &lump = gdict[ i ];

			std::string statname = gdict.GetElementName( i );
			v_escape_string( statname );

			Q_snprintf( q, sizeof( q ), "REPLACE into %s_generic (UserID,Tag,LastUpdate,MapName,MapVersion,StatName,Count,Value,X,Y,Z) values (\"%s\",\"%s\",Now(),\"%-.20s\",%d,\"%-.16s\",%d,%f,%d,%d,%d);",
				gamename,
				userid.c_str(),
				tag.c_str(),
				mapname.c_str(),
				mapversion,
				statname.c_str(),
				lump.m_unCount, 
				lump.m_flCurrentValue, 
				lump.m_Pos[ 0 ], 
				lump.m_Pos[ 1 ], 
				lump.m_Pos[ 2 ]
				);

			int retcode = sql->Execute( q );
			if ( retcode != 0 )
			{
				printf( "Query %s failed\n", q );
				return;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szMapName - 
// Output : Ep2LevelStats_t
//-----------------------------------------------------------------------------
Ep2LevelStats_t *FindOrAddMapStats( const char *szMapName )
{
	int iMap = g_dictMapStats.Find( szMapName );
	if( iMap == g_dictMapStats.InvalidIndex() )
	{
		iMap = g_dictMapStats.Insert( szMapName );
	}	

	return &g_dictMapStats[iMap];
}

int GetMaxLumpCount()
{
	return EP2_MAX_LUMP_COUNT;
}

void LoadCustomDataFromBuffer( CUtlBuffer &LoadBuffer, char *tag, size_t tagsize )
{
	tag[ 0 ] = 0;
	Ep2LevelStats_t::LoadData( g_dictMapStats, LoadBuffer );
	if ( g_dictMapStats.Count() > 0 )
	{
		Q_strncpy( tag, g_dictMapStats[ g_dictMapStats.First() ].m_Tag.m_szTagText, tagsize );
	}

	int i;
	int iMap;
	for ( iMap =  g_dictMapStats.First(); iMap != g_dictMapStats.InvalidIndex(); iMap =  g_dictMapStats.Next( iMap ) )
	{
		// Get the current map.
		Ep2LevelStats_t *pCurrentMap = &g_dictMapStats[iMap];
		CUtlDict< Ep2LevelStats_t::WeaponLump_t, int > &wdict = pCurrentMap->m_dictWeapons;
		for ( i = wdict.First(); i != wdict.InvalidIndex(); i = wdict.Next( i ) )
		{
			Ep2LevelStats_t::WeaponLump_t &lump = wdict[ i ];
			// Bogus #, some kind of damage scaling?
			if ( lump.m_flDamageInflicted > 50000 )
			{
				lump.m_flDamageInflicted = 0.0f;
				lump.m_nHits = 0;
				lump.m_nShots = 0;
			}
		}

		if ( (int64)pCurrentMap->m_IntCounters[ Ep2LevelStats_t::COUNTER_NOCLIPS ] < 0 )
		{
			pCurrentMap->m_IntCounters[ Ep2LevelStats_t::COUNTER_NOCLIPS ] = 0;
		}
	}
}

bool Ep2_ParseCurrentUserID( char const *pchDataFile, char *pchUserID, size_t bufsize, time_t &modifiedtime )
{
	FILE *fp = fopen( pchDataFile, "rb" );
	if ( fp )
	{
		CUtlBuffer statsBuffer;

		struct _stat sb;
		_stat( pchDataFile, &sb );

		// Msg( "Processing %s\n", ctx->file );
		int nBytesToRead = min( sb.st_size, sizeof( short ) + 16 );

		statsBuffer.Clear();
		statsBuffer.EnsureCapacity( nBytesToRead );
		fread( statsBuffer.Base(), nBytesToRead, 1, fp );
		statsBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, nBytesToRead );
		fclose( fp );

		// Skip the version
		statsBuffer.GetShort();
		statsBuffer.Get( pchUserID, 16 );
		pchUserID[ bufsize - 1 ] = 0;

		modifiedtime = sb.st_mtime;

		return statsBuffer.TellPut() == statsBuffer.TellGet();
	}

	return false;
}

int Ep2_ParseCustomGameStatsData( ParseContext_t *ctx )
{
	g_dictMapStats.Purge();

	FILE *fp = fopen( ctx->file, "rb" );
	if ( fp )
	{
		CUtlBuffer statsBuffer;

		struct _stat sb;
		_stat( ctx->file, &sb );

		// Msg( "Processing %s\n", ctx->file );

		statsBuffer.Clear();
		statsBuffer.EnsureCapacity( sb.st_size );
		fread( statsBuffer.Base(), sb.st_size, 1, fp );
		statsBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, sb.st_size );
		fclose( fp );

		char shortname[ 128 ];
		Q_FileBase( ctx->file, shortname, sizeof( shortname ) );

		char szCurrentStatsFileUserID[17];
		int iCurrentStatsFileVersion;

		iCurrentStatsFileVersion = statsBuffer.GetShort();
		statsBuffer.Get( szCurrentStatsFileUserID, 16 );
		szCurrentStatsFileUserID[ sizeof( szCurrentStatsFileUserID ) - 1 ] = 0;

		bool valid = true;
		BasicGameStats_t stats;

		char tag[ 32 ] = { 0 };

		unsigned int iCheckIfStandardDataSaved = statsBuffer.GetUnsignedInt();
		if( iCheckIfStandardDataSaved != GAMESTATS_STANDARD_NOT_SAVED )
		{
			//standard data was saved, rewind so the stats can read in time to completion
			statsBuffer.SeekGet( CUtlBuffer::SEEK_CURRENT, -((int)sizeof( unsigned int )) );
			
			valid = stats.ParseFromBuffer( statsBuffer, iCurrentStatsFileVersion );

			if ( ctx->describeonly )
			{
				DescribeData( stats, szCurrentStatsFileUserID, iCurrentStatsFileVersion );					
			}
		}

		//check for custom data
		bool bHasCustomData = (valid && (statsBuffer.TellPut() != statsBuffer.TellGet()));

		if( bHasCustomData )
		{
			LoadCustomDataFromBuffer( statsBuffer, tag, sizeof( tag ) );

			if( ctx->describeonly )
			{		
				DescribeEp2Stats();
			}
			else
			{				
				InsertCustomData( g_mapOrder, ctx->mysql, szCurrentStatsFileUserID, iCurrentStatsFileVersion, ctx->gamename );
			}
		}

		// Do base data after custom since we need the custom to parse out the "tag" used for this user
		if ( valid )
		{
			if ( !ctx->describeonly )
			{
				InsertData( g_mapOrder, ctx->mysql, stats, szCurrentStatsFileUserID, iCurrentStatsFileVersion, ctx->gamename, tag );
			}
		}
		else
		{
			++ctx->skipcount;
		}
	}
	return CUSTOMDATA_SUCCESS;
}


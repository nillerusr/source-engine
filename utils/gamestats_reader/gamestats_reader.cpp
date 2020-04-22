//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include "tier1/strtools.h"
#include <conio.h>
#include "tier1/utlbuffer.h"
#include "tier2/tier2.h"
#include "filesystem.h"
#include "imysqlwrapper.h"
#include "../../game/server/dod/dod_gamestats.h"
#include <time.h>


static void Pause( void )
{
	printf( "Hit a key to continue\n" );
	getch();
}

static void Usage()
{
	fprintf( stderr, "Usage: gamestats_reader <hostname> <database> <username> <password> <table> <directory to parse> ( -verbose )\n" );
	Pause();
	exit( -1 );
}

#define SQL_CMD_BUFSIZE		16000

char sqlCmd[SQL_CMD_BUFSIZE];
bool g_bFirstCmd;
bool g_bVerbose;

IMySQL *mysql;

char **g_argv;

void StartMYSQLInsert( void )
{
	g_bFirstCmd = true;
	sqlCmd[0] = '\0';

	Q_snprintf( sqlCmd, SQL_CMD_BUFSIZE, "INSERT INTO %s SET ", g_argv[5] );
}

void AddField( const char *field, const char *value )
{	
	char buf[128];

	if ( !g_bFirstCmd )
	{
		Q_strncat( sqlCmd, ", ", SQL_CMD_BUFSIZE, COPY_ALL_CHARACTERS );
	}

	Q_snprintf( buf, sizeof(buf), "%s=\"%s\"", field, value );
	Q_strncat( sqlCmd, buf, SQL_CMD_BUFSIZE, COPY_ALL_CHARACTERS );

	g_bFirstCmd = false;
}

void AddField( const char *field, const int value )
{	
	char buf[128];
	
	if ( !g_bFirstCmd )
	{
		Q_strncat( sqlCmd, ", ", SQL_CMD_BUFSIZE, COPY_ALL_CHARACTERS );
	}

	Q_snprintf( buf, sizeof(buf), "%s=%d", field, value );

#ifdef _DEBUG
	if ( Q_strlen(buf) + Q_strlen(sqlCmd) > SQL_CMD_BUFSIZE )
	{
		Assert( !"increase buf size\n" );
	}
#endif

	Q_strncat( sqlCmd, buf, SQL_CMD_BUFSIZE, COPY_ALL_CHARACTERS );

	g_bFirstCmd = false;
}

int CompleteMYSQLInsert( void )
{
	// terminate command and execute it
	Q_strncat( sqlCmd, "\n", SQL_CMD_BUFSIZE, COPY_ALL_CHARACTERS );

	if ( g_bVerbose )
	{
		printf( "%s", sqlCmd );
	}

	int retcode = mysql->Execute( sqlCmd );

	if ( retcode != 0 )
	{
		const char *asdf = mysql->GetLastError();
		printf( "Error: %s\n", asdf );
	}

	return retcode;
}

static const char *pszTeamNames[] = 
{
	"allies",
	"axis"
};

static const char *pszClassNames[] = 
{
	"rifleman",
	"assault",
	"support",
	"sniper",
	"mg",
	"rocket"
};

int iDistanceStatWeapons[DOD_NUM_DISTANCE_STAT_WEAPONS] = 
{
	WEAPON_COLT,
	WEAPON_P38,
	WEAPON_C96,
	WEAPON_GARAND,
	WEAPON_GARAND_ZOOMED,
	WEAPON_M1CARBINE,
	WEAPON_K98,
	WEAPON_K98_ZOOMED,
	WEAPON_SPRING,
	WEAPON_SPRING_ZOOMED,
	WEAPON_K98_SCOPED,
	WEAPON_K98_SCOPED_ZOOMED,
	WEAPON_THOMPSON,
	WEAPON_MP40,
	WEAPON_MP44,
	WEAPON_MP44_SEMIAUTO,
	WEAPON_BAR,
	WEAPON_BAR_SEMIAUTO,
	WEAPON_30CAL,
	WEAPON_30CAL_UNDEPLOYED,
	WEAPON_MG42,
	WEAPON_MG42_UNDEPLOYED,
};

// Send hit/shots only for the following weapons
int iNoDistStatWeapons[DOD_NUM_NODIST_STAT_WEAPONS] =
{
	WEAPON_AMERKNIFE,
	WEAPON_SPADE,
	WEAPON_BAZOOKA,
	WEAPON_PSCHRECK,
	WEAPON_FRAG_US,
	WEAPON_FRAG_GER,
	WEAPON_FRAG_US_LIVE,
	WEAPON_FRAG_GER_LIVE,
	WEAPON_RIFLEGREN_US,
	WEAPON_RIFLEGREN_GER,
	WEAPON_RIFLEGREN_US_LIVE,
	WEAPON_RIFLEGREN_GER_LIVE,
	WEAPON_THOMPSON_PUNCH,
	WEAPON_MP40_PUNCH,
};

const char * s_WeaponAliasInfo[] = 
{
	"none",	//	WEAPON_NONE = 0,

	//Melee
	"amerknife",	//WEAPON_AMERKNIFE,
	"spade",		//WEAPON_SPADE,

	//Pistols
	"colt",			//WEAPON_COLT,
	"p38",			//WEAPON_P38,
	"c96",			//WEAPON_C96

	//Rifles
	"garand",		//WEAPON_GARAND,
	"m1carbine",	//WEAPON_M1CARBINE,
	"k98",			//WEAPON_K98,

	//Sniper Rifles
	"spring",		//WEAPON_SPRING,
	"k98_scoped",	//WEAPON_K98_SCOPED,

	//SMG
	"thompson",		//WEAPON_THOMPSON,
	"mp40",			//WEAPON_MP40,
	"mp44",			//WEAPON_MP44,
	"bar",			//WEAPON_BAR,

	//Machine guns
	"30cal",		//WEAPON_30CAL,
	"mg42",			//WEAPON_MG42,

	//Rocket weapons
	"bazooka",		//WEAPON_BAZOOKA,
	"pschreck",		//WEAPON_PSCHRECK,

	//Grenades
	"frag_us",		//WEAPON_FRAG_US,
	"frag_ger",		//WEAPON_FRAG_GER,

	"frag_us_live",		//WEAPON_FRAG_US_LIVE
	"frag_ger_live",	//WEAPON_FRAG_GER_LIVE

	"smoke_us",			//WEAPON_SMOKE_US
	"smoke_ger",		//WEAPON_SMOKE_GER

	"riflegren_us",	//WEAPON_RIFLEGREN_US
	"riflegren_ger",	//WEAPON_RIFLEGREN_GER

	"riflegren_us_live",	//WEAPON_RIFLEGREN_US_LIVE
	"riflegren_ger_live",		//WEAPON_RIFLEGREN_GER_LIVE

	// not actually separate weapons, but defines used in stats recording
	"thompson_punch",		//WEAPON_THOMPSON_PUNCH
	"mp40_punch",			//WEAPON_MP40_PUNCH
	"garand_zoomed",		//WEAPON_GARAND_ZOOMED,	

	"k98_zoomed",			//WEAPON_K98_ZOOMED
	"spring_zoomed",		//WEAPON_SPRING_ZOOMED
	"k98_scoped_zoomed",	//WEAPON_K98_SCOPED_ZOOMED	

	"30cal_undeployed",		//WEAPON_30CAL_UNDEPLOYED,
	"mg42_undeployed",		//WEAPON_MG42_UNDEPLOYED,

	"bar_semiauto",			//WEAPON_BAR_SEMIAUTO,
	"mp44_semiauto",		//WEAPON_MP44_SEMIAUTO,

	0,		// end of list marker
};

void ParseFile( const char *fileName )
{
	FileHandle_t file = g_pFullFileSystem->Open( fileName, "rb" );

	if ( !file )
	{
		return;
	}

	dod_gamestats_t stats;
	g_pFullFileSystem->Read( &stats, sizeof( dod_gamestats_t ), file );

	if ( stats.header.iVersion != DOD_STATS_BLOB_VERSION || Q_stricmp( stats.header.szGameName, "dod" ) )
	{
		printf( "Error parsing file, bad header info: %s\n", fileName );
		return;
	}

	StartMYSQLInsert();

	AddField( "map", stats.header.szMapName );

	const time_t mapfiletime = g_pFullFileSystem->GetFileTime( fileName );

	struct tm *t = localtime( &mapfiletime );

	// YYYY-MM-DD HH:MM::SS 

	char filetimebuf[64];

	Q_snprintf( filetimebuf, sizeof(filetimebuf), "%04d-%02d-%02d %02d:%02d:%02d", 
		t->tm_year + 1900,
		t->tm_mon + 1,
		t->tm_mday,
		t->tm_hour,
		t->tm_min,
		t->tm_sec );

	AddField( "time", filetimebuf );

	AddField( "version", stats.header.iVersion );

	AddField( "ipaddr_0", stats.header.ipAddr[0] );
	AddField( "ipaddr_1", stats.header.ipAddr[1] );
	AddField( "ipaddr_2", stats.header.ipAddr[2] );
	AddField( "ipaddr_3", stats.header.ipAddr[3] );
	AddField( "port", stats.header.port );

	AddField( "minutes_map", stats.iMinutesPlayed );
	AddField( "wins_allies", stats.iNumAlliesWins );
	AddField( "wins_axis", stats.iNumAxisWins);
	AddField( "tickpoints_allies", stats.iAlliesTickPoints );
	AddField( "tickpoints_axis",  stats.iAxisTickPoints );

	char buf[128];

	for ( int cls=0;cls<6;cls++ )
	{
		Q_snprintf( buf, sizeof(buf), "minutes_allies_%s", pszClassNames[cls] );
		AddField( buf, stats.iMinutesPlayedPerClass_Allies[cls] );

		Q_snprintf( buf, sizeof(buf), "kills_allies_%s", pszClassNames[cls] );
		AddField( buf, stats.iKillsPerClass_Allies[cls] );

		Q_snprintf( buf, sizeof(buf), "defenses_allies_%s", pszClassNames[cls] );
		AddField( buf, stats.iDefensesPerClass_Allies[cls] );

		Q_snprintf( buf, sizeof(buf), "caps_allies_%s", pszClassNames[cls] );
		AddField( buf, stats.iCapsPerClass_Allies[cls] );

		Q_snprintf( buf, sizeof(buf), "spawns_allies_%s", pszClassNames[cls] );
		AddField( buf, stats.iSpawnsPerClass_Allies[cls] );

		Q_snprintf( buf, sizeof(buf), "classlimit_allies_%s", pszClassNames[cls] );
		AddField( buf, stats.iClassLimits_Allies[cls] );


		Q_snprintf( buf, sizeof(buf), "minutes_axis_%s", pszClassNames[cls] );
		AddField( buf, stats.iMinutesPlayedPerClass_Axis[cls] );

		Q_snprintf( buf, sizeof(buf), "kills_axis_%s", pszClassNames[cls] );
		AddField( buf, stats.iKillsPerClass_Axis[cls] );

		Q_snprintf( buf, sizeof(buf), "defenses_axis_%s", pszClassNames[cls] );
		AddField( buf, stats.iDefensesPerClass_Axis[cls] );

		Q_snprintf( buf, sizeof(buf), "caps_axis_%s", pszClassNames[cls] );
		AddField( buf, stats.iCapsPerClass_Axis[cls] );

		Q_snprintf( buf, sizeof(buf), "spawns_axis_%s", pszClassNames[cls] );
		AddField( buf, stats.iSpawnsPerClass_Axis[cls] );

		Q_snprintf( buf, sizeof(buf), "classlimit_axis_%s", pszClassNames[cls] );
		AddField( buf, stats.iClassLimits_Axis[cls] );
	}

	int i;

	for ( i=0;i<DOD_NUM_DISTANCE_STAT_WEAPONS;i++ )
	{
		int iWeapon = iDistanceStatWeapons[i];
		const char *pszWeapon = s_WeaponAliasInfo[iWeapon];

		Q_snprintf( buf, sizeof(buf), "weapon_shots_%s", pszWeapon );
		AddField( buf, stats.weaponStatsDistance[i].iNumAttacks );

		Q_snprintf( buf, sizeof(buf), "weapon_hits_%s", pszWeapon );
		AddField( buf, stats.weaponStatsDistance[i].iNumHits );

		for ( int j=0;j<DOD_NUM_WEAPON_DISTANCE_BUCKETS;j++ )
		{
			Q_snprintf( buf, sizeof(buf), "weapon_distance_%s_%d", pszWeapon, j );
			AddField( buf, stats.weaponStatsDistance[i].iDistanceBuckets[j] );
		}
	}

	for ( i=0;i<DOD_NUM_NODIST_STAT_WEAPONS;i++ )
	{
		int iWeapon = iNoDistStatWeapons[i];
		const char *pszWeapon = s_WeaponAliasInfo[iWeapon];

		Q_snprintf( buf, sizeof(buf), "weapon_shots_%s", pszWeapon );
		AddField( buf, stats.weaponStats[i].iNumAttacks );

		Q_snprintf( buf, sizeof(buf), "weapon_hits_%s", pszWeapon );
		AddField( buf, stats.weaponStats[i].iNumHits );
	}

	CompleteMYSQLInsert();

	g_pFullFileSystem->Close( file );
}

int main( int argc, char **argv )
{
	g_argv = argv;

	if( argc < 6 )
	{
		Usage();
	}

	if ( argc == 7 && !Q_stricmp( argv[6], "-verbose" ) )
	{
		g_bVerbose = true;
	}

	InitDefaultFileSystem();

	// Init MYSQL connection
	CSysModule *sql = Sys_LoadModule( "mysql_wrapper" );
	if ( sql )
	{
		CreateInterfaceFn factory = Sys_GetFactory( sql );
		if ( factory )
		{
			mysql = ( IMySQL * )factory( MYSQL_WRAPPER_VERSION_NAME, NULL );
			if ( mysql )
			{
				if ( mysql->InitMySQL( argv[ 2 ], argv[ 1 ], argv[ 3 ], argv[ 4 ] ) )
				{
					// insert rows

					const char *dir = argv[6];

					char searchString[MAX_PATH*2];
					Q_strncpy( searchString, dir, sizeof( searchString ) );
					Q_AppendSlash( searchString, sizeof( searchString ) );
					Q_strncat( searchString, "*.dat", sizeof( searchString ), COPY_ALL_CHARACTERS );

					int iNumFiles = 0;
					FileFindHandle_t findHandle = NULL;
					const char *filename = g_pFullFileSystem->FindFirst( searchString, &findHandle );
					while ( filename )
					{
						char fullFileName[MAX_PATH*2];

						Q_strncpy( fullFileName, dir, sizeof( fullFileName ) );
						Q_AppendSlash( fullFileName, sizeof( fullFileName ) );
						Q_strncat( fullFileName, filename, sizeof( fullFileName ), COPY_ALL_CHARACTERS );

						ParseFile( fullFileName );

						printf( "processing file: %s\n", fullFileName );
						iNumFiles++;

						filename = g_pFullFileSystem->FindNext(findHandle);
					}
					g_pFullFileSystem->FindClose(findHandle);	

					printf( "Completed: %d files processed from directory \"%s\"\n", iNumFiles, dir );

				}
				else
				{
					printf( "InitMySQL failed ( %s )\n", mysql->GetLastError() );
				}

				mysql->Release();
			}
			else
			{
				printf( "Unable to connect via mysql_wrapper\n");
			}
		}
		else
		{
			printf( "Unable to get factory from mysql_wrapper.dll, not updating access mysql table!!!" );
		}

		Sys_UnloadModule( sql );
	}
	else
	{
		printf( "Unable to load mysql_wrapper.dll, not updating access mysql table!!!" );
	}

	return 0;
}

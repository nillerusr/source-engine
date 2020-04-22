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
#include "cs_gamestats.h"
#include "base_gamestats_parse.h"

extern CUtlDict< int, unsigned short > g_mapOrder;

const char *pValidStatLevels[] =
{
	"cs_assault",
	"cs_compound",
	"cs_havana",
	"cs_italy",
	"cs_militia",
	"cs_office",
	"de_aztec",
	"de_cbble",
	"de_chateau",
	"de_dust2",
	"de_dust",
	"de_inferno",
	"de_nuke",
	"de_piranesi",
	"de_port",
	"de_prodigy",
	"de_tides",
	"de_train",
};

static const char * s_WeaponAliasInfo[] = 
{
	"none",		// WEAPON_NONE
	"p228",		// WEAPON_P228
	"glock",	// WEAPON_GLOCK				// old glock
	"scout",	// WEAPON_SCOUT
	"hegren",	// WEAPON_HEGRENADE
	"xm1014",	// WEAPON_XM1014			// auto shotgun
	"c4",		// WEAPON_C4
	"mac10",	// WEAPON_MAC10				// T only
	"aug",		// WEAPON_AUG
	"sgren",	// WEAPON_SMOKEGRENADE
	"elite",	// WEAPON_ELITE
	"fiveseven",// WEAPON_FIVESEVEN
	"ump45",	// WEAPON_UMP45
	"sg550",	// WEAPON_SG550				// auto-sniper
	"galil",	// WEAPON_GALIL
	"famas",	// WEAPON_FAMAS				// CT cheap m4a1
	"usp",		// WEAPON_USP
	"awp",		// WEAPON_AWP
	"mp5navy",	// WEAPON_MP5N 
	"m249",		// WEAPON_M249				// big machinegun
	"m3",		// WEAPON_M3 				// cheap shotgun
	"m4a1",		// WEAPON_M4A1
	"tmp",		// WEAPON_TMP
	"g3sg1",	// WEAPON_G3SG1				// T auto-sniper
	"flash",	// WEAPON_FLASHBANG
	"deagle",	// WEAPON_DEAGLE
	"sg552",	// WEAPON_SG552				// T aug equivalent
	"ak47",		// WEAPON_AK47
	"knife",	// WEAPON_KNIFE
	"p90",		// WEAPON_P90
	"shield",	// WEAPON_SHIELDGUN 
	"kevlar",
	"assaultsuit",
	"nightvision",
	NULL,		// WEAPON_NONE
};

void DescribeData( cs_gamestats_t &stats )
{
	Msg( "  Blob version:  %d\n", stats.header.iVersion );
	Msg( "  Server Uptime:  %d\n", stats.iMinutesPlayed );

	for ( int i = 0; i < CS_NUM_LEVELS; i++ )
	{
		Msg( "%s - Terrorists Wins: %d | Counter-Terrorists Wins: %d\n", pValidStatLevels[i], stats.iTerroristVictories[i], stats.iCounterTVictories[i] );
	}

	for ( int i = 0; i < WEAPON_MAX; i++ )
	{
		Msg( "%s was purchased %d time(s)\n", s_WeaponAliasInfo[i], stats.iBlackMarketPurchases[i] );
	}


	char q[ 512 ];

	Q_snprintf( q, sizeof( q ), "Auto-Buy = %d\n", stats.iAutoBuyPurchases );
	Msg( q );

	Q_snprintf( q, sizeof( q ), "Re-Buy = %d\n", stats.iReBuyPurchases );
	Msg( q );

	Q_snprintf( q, sizeof( q ), "Auto-Buy: M4A1 = %d\n", stats.iAutoBuyM4A1Purchases );
	Msg( q );

	Q_snprintf( q, sizeof( q ), "Auto-Buy: AK47 = %d\n", stats.iAutoBuyAK47Purchases );
	Msg( q );

	Q_snprintf( q, sizeof( q ), "Auto-Buy: Famas = %d\n", stats.iAutoBuyFamasPurchases );
	Msg( q );

	Q_snprintf( q, sizeof( q ), "Auto-Buy: Galil = %d\n", stats.iAutoBuyGalilPurchases );
	Msg( q );

	Q_snprintf( q, sizeof( q ), "Auto-Buy: Suit = %d\n", stats.iAutoBuyVestHelmPurchases );
	Msg( q );

	Q_snprintf( q, sizeof( q ), "Auto-Buy: Kev = %d\n", stats.iAutoBuyVestPurchases );
	Msg( q );
}

int CS_ParseCustomGameStatsData( ParseContext_t *ctx )
{
	if ( g_pFullFileSystem == NULL )
		return CUSTOMDATA_FAILED;

	FileHandle_t FileHandle = g_pFullFileSystem->Open( ctx->file, "rb" );

	if ( !FileHandle )
	{
		return CUSTOMDATA_FAILED;
	}

	if ( ctx->mysql == NULL && ctx->describeonly == false )
		return CUSTOMDATA_FAILED;

	char q[ 512 ];
	cs_gamestats_t stats;
	g_pFullFileSystem->Read( &stats, sizeof( cs_gamestats_t ), FileHandle );

    if ( Q_stricmp( stats.header.szGameName, "cstrike" ) )
		return CUSTOMDATA_FAILED;

	if ( stats.header.iVersion != CS_STATS_BLOB_VERSION )
	{
		Msg( "Error: Incorrect Blob Version! Got: %d - Expected: %d\n", stats.header.iVersion, CS_STATS_BLOB_VERSION );
		return CUSTOMDATA_FAILED;
	}

	if ( ctx->describeonly == true )
	{
		DescribeData( stats );
		return CUSTOMDATA_SUCCESS;
	}

	//Do maps first
	for ( int i = 0; i < CS_NUM_LEVELS; i++ )
	{
		Q_snprintf( q, sizeof( q ), "update maps set TerroristWins=TerroristWins+%d, CTWins=CTWins+%d where MapName = \"%s\";", stats.iTerroristVictories[i], stats.iCounterTVictories[i], pValidStatLevels[i]  );

		int retcode = ctx->mysql->Execute( q );
		if ( retcode != 0 )
		{
			printf( "Query:\n %s\n failed\n", q );
			return CUSTOMDATA_FAILED;
		}
	}

	//Now do all weapons
	for ( int i = 0; i < WEAPON_MAX; i++ )
	{
		int iWeaponID = i;

		//HACKHACK: Fix up incorrect data for the smoke grenades.
		if ( i == 0 && stats.iBlackMarketPurchases[i] != 0 )
		{
			iWeaponID = 9;
		}

		Q_snprintf( q, sizeof( q ), "update weapons set Count=Count+%d where WeaponID = %d;", stats.iBlackMarketPurchases[i], iWeaponID  );

		int retcode = ctx->mysql->Execute( q );
		if ( retcode != 0 )
		{
			printf( "Query:\n %s\n failed\n", q );
			return CUSTOMDATA_FAILED;
		}
	}


	Q_snprintf( q, sizeof( q ), "update autobuy set Count=Count+%d where Purchase = \"Auto-Buy\";", stats.iAutoBuyPurchases );
	ctx->mysql->Execute( q );

	Q_snprintf( q, sizeof( q ), "update autobuy set Count=Count+%d where Purchase = \"Re-Buy\";", stats.iReBuyPurchases );
	ctx->mysql->Execute( q );

	Q_snprintf( q, sizeof( q ), "update autobuy set Count=Count+%d where Purchase = \"Auto-Buy: M4A1\";", stats.iAutoBuyM4A1Purchases );
	ctx->mysql->Execute( q );

	Q_snprintf( q, sizeof( q ), "update autobuy set Count=Count+%d where Purchase = \"Auto-Buy: AK47\";", stats.iAutoBuyAK47Purchases );
	ctx->mysql->Execute( q );

	Q_snprintf( q, sizeof( q ), "update autobuy set Count=Count+%d where Purchase = \"Auto-Buy: Famas\";", stats.iAutoBuyFamasPurchases );
	ctx->mysql->Execute( q );

	Q_snprintf( q, sizeof( q ), "update autobuy set Count=Count+%d where Purchase = \"Auto-Buy: Galil\";", stats.iAutoBuyGalilPurchases );
	ctx->mysql->Execute( q );

	Q_snprintf( q, sizeof( q ), "update autobuy set Count=Count+%d where Purchase = \"Auto-Buy: Suit\";", stats.iAutoBuyVestHelmPurchases );
	ctx->mysql->Execute( q );

	Q_snprintf( q, sizeof( q ), "update autobuy set Count=Count+%d where Purchase = \"Auto-Buy: Kev\";", stats.iAutoBuyVestPurchases );
	ctx->mysql->Execute( q );



	return CUSTOMDATA_SUCCESS;
}

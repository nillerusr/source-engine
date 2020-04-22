//========= Copyright Valve Corporation, All rights reserved. ============//
// blackmarket.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <process.h>
#include <string.h>
#include <windows.h>
#include <sys/stat.h>


#include "interface.h"
#include "imysqlwrapper.h"
#include "mathlib.h"
#include "filesystem.h"
#include "filesystem_helpers.h"
#include "tier2/tier2.h"
#include "utlbuffer.h"

#include <ATLComTime.h>

const char *pDBName = "blackmarket_history";
const char *pHostName = "gamestats";
const char *pUserName = "root";
const char *pPassword = "r77GH34p";

CSysModule *sql = NULL;
CreateInterfaceFn factory = NULL;
IMySQL *mysql = NULL;

enum CSWeaponID
{
	WEAPON_NONE = 0,

	WEAPON_P228,
	WEAPON_GLOCK,
	WEAPON_SCOUT,
	WEAPON_HEGRENADE,
	WEAPON_XM1014,
	WEAPON_C4,
	WEAPON_MAC10,
	WEAPON_AUG,
	WEAPON_SMOKEGRENADE,
	WEAPON_ELITE,
	WEAPON_FIVESEVEN,
	WEAPON_UMP45,
	WEAPON_SG550,

	WEAPON_GALIL,
	WEAPON_FAMAS,
	WEAPON_USP,
	WEAPON_AWP,
	WEAPON_MP5NAVY,
	WEAPON_M249,
	WEAPON_M3,
	WEAPON_M4A1,
	WEAPON_TMP,
	WEAPON_G3SG1,
	WEAPON_FLASHBANG,
	WEAPON_DEAGLE,
	WEAPON_SG552,
	WEAPON_AK47,
	WEAPON_KNIFE,
	WEAPON_P90,

	WEAPON_SHIELDGUN,	// BOTPORT: Is this still needed?

	WEAPON_KEVLAR,
	WEAPON_ASSAULTSUIT,
	WEAPON_NVG,

	WEAPON_MAX,		// number of weapons weapon index
};

#define PRICE_BLOB_VERSION 1
#define PRICE_BLOB_NAME "weeklyprices.dat"
struct weeklyprice_t
{
	short iVersion;
	short iPreviousPrice[WEAPON_MAX];
	short iCurrentPrice[WEAPON_MAX];
};

int g_iCurrentWeaponPurchases[WEAPON_MAX];
int g_iNewWeaponPrices[WEAPON_MAX];
int g_iPriceDelta[WEAPON_MAX];
int g_iPurchaseDelta[WEAPON_MAX];
int	g_iCounter = 0;
int g_iResetCounter = 0;
bool g_bWeeklyUpdate = false;

#define PRICE_SCALE 0.01f

struct weapons_t
{
	int iWeaponType;
	int iDefaultPrice;
	int iID;
	int iPurchaseCount;
	int iCurrentPrice;
};

#define WEAPON_NOTHING 0
#define WEAPON_PISTOL 1
#define WEAPON_EVERYTHING 2

static weapons_t g_Weapons[] =
{
	{ WEAPON_NOTHING,	0, WEAPON_NONE, 0, 0, },
	{ WEAPON_PISTOL,	600, WEAPON_P228, 0, 0, },
	{ WEAPON_PISTOL,	400, WEAPON_GLOCK, 0, 0, },
	{ WEAPON_EVERYTHING, 2750, WEAPON_SCOUT, 0, 0, },
	{ WEAPON_EVERYTHING, 300, WEAPON_HEGRENADE, 0, 0, },
	{ WEAPON_EVERYTHING, 3000, WEAPON_XM1014, 0, 0, },
	{ WEAPON_NOTHING,	0, WEAPON_C4, 0, 0, },
	{ WEAPON_EVERYTHING, 1400, WEAPON_MAC10, 0, 0, },
	{ WEAPON_EVERYTHING, 3500, WEAPON_AUG, 0, 0, },
	{ WEAPON_EVERYTHING, 300, WEAPON_SMOKEGRENADE, 0, 0, },
	{ WEAPON_PISTOL,	800, WEAPON_ELITE, 0, 0, },
	{ WEAPON_PISTOL,	750, WEAPON_FIVESEVEN, 0, 0, },
	{ WEAPON_EVERYTHING, 1700, WEAPON_UMP45, 0, 0, },
	{ WEAPON_EVERYTHING, 4200, WEAPON_SG550, 0, 0, },
	{ WEAPON_EVERYTHING, 2000, WEAPON_GALIL, 0, 0, },
	{ WEAPON_EVERYTHING, 2250, WEAPON_FAMAS, 0, 0, },
	{ WEAPON_PISTOL,	500, WEAPON_USP, 0, 0, },
	{ WEAPON_EVERYTHING, 4750, WEAPON_AWP, 0, 0, },
	{ WEAPON_EVERYTHING, 1500, WEAPON_MP5NAVY, 0, 0, },
	{ WEAPON_EVERYTHING, 5750, WEAPON_M249, 0, 0, },
	{ WEAPON_EVERYTHING, 1700, WEAPON_M3, 0, 0, },
	{ WEAPON_EVERYTHING, 3100, WEAPON_M4A1, 0, 0, },
	{ WEAPON_EVERYTHING, 1250, WEAPON_TMP, 0, 0, },
	{ WEAPON_EVERYTHING, 5000, WEAPON_G3SG1, 0, 0, },
	{ WEAPON_EVERYTHING, 200, WEAPON_FLASHBANG, 0, 0, },
	{ WEAPON_PISTOL,	650, WEAPON_DEAGLE, 0, 0, },
	{ WEAPON_EVERYTHING, 3500, WEAPON_SG552, 0, 0, },
	{ WEAPON_EVERYTHING, 2500, WEAPON_AK47, 0, 0, },
	{ WEAPON_NOTHING, 0, WEAPON_KNIFE, 0, 0, },
	{ WEAPON_EVERYTHING, 2350, WEAPON_P90, 0, 0, },
	{ WEAPON_NOTHING, 0, WEAPON_SHIELDGUN, 0, 0, },
	{ WEAPON_EVERYTHING, 650, WEAPON_KEVLAR, 0, 0, },
	{ WEAPON_EVERYTHING,  1000, WEAPON_ASSAULTSUIT, 0, 0, },
	{ WEAPON_EVERYTHING, 1250, WEAPON_NVG, 0, 0, },
};

const char * s_WeaponAliasInfo[] = 
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
		"kev+helm",
		"ngvision",
		NULL,		// WEAPON_NONE
};


enum sorttype
{
	SORT_PURCHASE = 0,
	SORT_NEWPRICE,
	SORT_PRICEDELTA,
	SORT_PERCENTCHANGE,
};

void SortWeaponRanks( int iWeaponType, weapons_t *pWeaponsArray, int iSortType );

bool GetCurrentPurchaseCount( void )
{
	if ( mysql->InitMySQL( "gamestats_cstrike", pHostName, pUserName, pPassword ) )
	{
		Msg( "Successfully connected to database %s on host %s, user %s\n", "gamestats_cstrike", pHostName, pUserName );

		//Get purchase counts.
		mysql->Execute( "select * from weapons;" );
	
		bool bFoundNext = mysql->SeekToFirstRow();
		int iIndex = WEAPON_P228;

		while( bFoundNext && iIndex < WEAPON_MAX )
		{
			bFoundNext = mysql->NextRow();
			g_iCurrentWeaponPurchases[iIndex] = mysql->GetColumnValue_Int( mysql->GetColumnIndex( "Count" ) );
			g_Weapons[iIndex].iPurchaseCount = g_iCurrentWeaponPurchases[iIndex];
			iIndex++;
		}

		mysql->Execute( "select * from snapshot_counter;" );
		mysql->SeekToFirstRow();
		mysql->NextRow();

		//Get snapshot counter.
		g_iCounter = mysql->GetColumnValue_Int( mysql->GetColumnIndex( "counter" ) );

		mysql->Execute( "select * from reset_counter;" );
		mysql->SeekToFirstRow();
		mysql->NextRow();

		//Get reset snapshot counter.
		g_iResetCounter = mysql->GetColumnValue_Int( mysql->GetColumnIndex( "counter" ) );

		

		//Get current price and purchase count
		mysql->Execute( "select * from weapon_info;" );

		bFoundNext = mysql->SeekToFirstRow();
		iIndex = WEAPON_P228;

		while( bFoundNext && iIndex < WEAPON_MAX )
		{
			bFoundNext = mysql->NextRow();
			
			int iWeaponID = mysql->GetColumnValue_Int( mysql->GetColumnIndex( "WeaponID" ) );
			g_Weapons[iWeaponID].iCurrentPrice = mysql->GetColumnValue_Int( mysql->GetColumnIndex( "current_price" ) );

			if ( g_bWeeklyUpdate == true )
			{
				int iPreviousPurchases = mysql->GetColumnValue_Int( mysql->GetColumnIndex( "purchases_thisweek" ) );
				g_Weapons[iWeaponID].iPurchaseCount = g_iPurchaseDelta[iWeaponID] = iPreviousPurchases;
			}
			else
			{
				int iPreviousPurchases = mysql->GetColumnValue_Int( mysql->GetColumnIndex( "purchases" ) );
				int iPurchasesThisWeek = mysql->GetColumnValue_Int( mysql->GetColumnIndex( "purchases_thisweek" ) );

				g_iPurchaseDelta[iWeaponID] = g_iCurrentWeaponPurchases[iWeaponID] - iPreviousPurchases;

				g_Weapons[iWeaponID].iPurchaseCount =  g_iPurchaseDelta[iWeaponID] + iPurchasesThisWeek;
			}
			

			iIndex++;
		}


		if ( g_bWeeklyUpdate == true )
		{
			iIndex = WEAPON_P228;

			while( iIndex < WEAPON_MAX )
			{
				char szQueryString[512];

				Q_snprintf( szQueryString, 512, "select * from purchases_pricing where snapshot=\"%d_%d\"", g_iCounter, iIndex );

				//Get current price and purchase count
				mysql->Execute( szQueryString );
				mysql->SeekToFirstRow();
				mysql->NextRow();

				g_iPurchaseDelta[iIndex] = mysql->GetColumnValue_Int( mysql->GetColumnIndex( "purchases" ) );

				iIndex++;
			}
		}

		return true;
	}

	Msg( "mysql->InitMySQL( %s, %s, %s, [password]) failed\n", "gamestats_cstrike", pHostName, pUserName );

	return false;
}

int GetWeaponTypeCount( int iType )
{
	int iCount = 0;

	for ( int j = 0; j < WEAPON_MAX; j++ )
	{
		if ( g_Weapons[j].iWeaponType == iType )
		{
			iCount++;
		}
	}

	return iCount;
}

weapons_t g_PistolRanks[6];
weapons_t g_RifleAndEquipmentRanks[24];

void PrintNewPrices( void )
{
	int iTotalPistols = 0;
	int iTotalRiflesEquipment = 0;
	int iTotalCurrentPistols = 0;
	int iTotalCurrentRiflesEquipment = 0;

	for ( int j = 0; j < WEAPON_MAX; j++ )
	{
		//Msg( "%s - Default Price: %d - New Price: %d\n", s_WeaponAliasInfo[j], g_Weapons[j].iDefaultPrice, g_iNewWeaponPrices[j] );

		if ( g_Weapons[j].iWeaponType == WEAPON_PISTOL )
		{
			iTotalPistols += g_Weapons[j].iCurrentPrice;
			iTotalCurrentPistols += g_iNewWeaponPrices[j];
		}
		else if ( g_Weapons[j].iWeaponType == WEAPON_EVERYTHING )
		{
			iTotalRiflesEquipment += g_Weapons[j].iCurrentPrice;
			iTotalCurrentRiflesEquipment += g_iNewWeaponPrices[j];
		}
	}

	int iCount = GetWeaponTypeCount( WEAPON_PISTOL );

	SortWeaponRanks( WEAPON_PISTOL, g_PistolRanks, SORT_PERCENTCHANGE );

	Msg( "\n" );
	Msg( "Pistol Rankings:\n\n");

	for ( int i = 0; i < iCount; i++ )
	{
		Msg( "#%d: %s \t\t| Purchases: %d \t| Current Price: %d \t| New Price: %d \t| Price Change: %d\n", i+1, s_WeaponAliasInfo[g_PistolRanks[i].iID], g_Weapons[g_PistolRanks[i].iID].iPurchaseCount, g_PistolRanks[i].iCurrentPrice, g_iNewWeaponPrices[g_PistolRanks[i].iID], g_iPriceDelta[g_PistolRanks[i].iID] );
	}

	iCount = GetWeaponTypeCount( WEAPON_EVERYTHING );

	SortWeaponRanks( WEAPON_EVERYTHING, g_RifleAndEquipmentRanks, SORT_PERCENTCHANGE );

	Msg( "\n\n" );
	Msg( "Other Weapons and Equipment Rankings:\n\n");

	for ( int i = 0; i < iCount; i++ )
	{
		Msg( "#%d: %s \t\t| Purchases: %d \t| Current Price: %d \t| New Price: %d \t| Price Change: %d\n", i+1, s_WeaponAliasInfo[g_RifleAndEquipmentRanks[i].iID], g_Weapons[g_RifleAndEquipmentRanks[i].iID].iPurchaseCount, g_RifleAndEquipmentRanks[i].iCurrentPrice, g_iNewWeaponPrices[g_RifleAndEquipmentRanks[i].iID], g_iPriceDelta[g_RifleAndEquipmentRanks[i].iID] );
	}

	Msg( "\n" );
	Msg( "Total Pistol Baseline: %d\n", iTotalPistols );
	Msg( "Current Pistol Total: %d\n", iTotalCurrentPistols );
	Msg( "Total Rifles/Equipment Baseline: %d\n", iTotalRiflesEquipment );
	Msg( "Current Rifles/Equipment Total: %d\n", iTotalCurrentRiflesEquipment );

	Msg( "\n" );
}

float g_flTotalWeaponPurchasesPistols = 0;
float g_flTotalWeaponPurchasesRifleEquipment = 0;
float g_flTotalDollarsPistols = 1;
float g_flTotalDollarsRifleEquipment = 1;

void SortWeaponRanks( int iWeaponType, weapons_t *pWeaponsArray, int iSortType )
{
	int iCount = GetWeaponTypeCount( iWeaponType );

	int iTotalPurchases = g_flTotalWeaponPurchasesPistols;

	if ( iWeaponType == WEAPON_EVERYTHING )
	{
		iTotalPurchases = g_flTotalWeaponPurchasesRifleEquipment; 
	}

	bool bSorted = 1;

	while ( bSorted )
	{
		bSorted = false;

		for ( int i = 1; i < iCount; i++ )
		{
			float flSize1 = 0; // (pWeaponsArray[i].iPurchaseCount * PRICE_SCALE) * (pWeaponsArray[i].iCurrentPrice * PRICE_SCALE);
			float flSize2 = 0; //(pWeaponsArray[i-1].iPurchaseCount * PRICE_SCALE) * (pWeaponsArray[i-1].iCurrentPrice * PRICE_SCALE);

			if ( iSortType == SORT_PURCHASE )
			{
				flSize1 = pWeaponsArray[i].iPurchaseCount;
				flSize2 = pWeaponsArray[i-1].iPurchaseCount; 
			}
			else if ( iSortType == SORT_NEWPRICE )
			{
				flSize1 = g_iNewWeaponPrices[pWeaponsArray[i].iID];
				flSize2 = g_iNewWeaponPrices[pWeaponsArray[i-1].iID];
			}
			else if ( iSortType == SORT_PRICEDELTA )
			{
				flSize1 = g_iPriceDelta[pWeaponsArray[i].iID];
				flSize2 = g_iPriceDelta[pWeaponsArray[i-1].iID];
			}
			else if ( iSortType == SORT_PERCENTCHANGE )
			{
				flSize1 = (pWeaponsArray[i].iCurrentPrice / g_iNewWeaponPrices[pWeaponsArray[i].iID]) - 1;
				flSize2 =  (pWeaponsArray[i-1].iCurrentPrice / g_iNewWeaponPrices[pWeaponsArray[i-1].iID]) - 1;
			}
			if ( flSize1 > flSize2 )
			{
				weapons_t temp = pWeaponsArray[i];
				pWeaponsArray[i] = pWeaponsArray[i-1];
				pWeaponsArray[i-1] = temp;
				bSorted = true;
			}
		}
	}
}

void CalculateNewPrices( weapons_t *pWeaponsArray, int iWeaponType )
{
	int iTotalDollars = g_flTotalDollarsPistols;
	int iTotalPurchases = g_flTotalWeaponPurchasesPistols;
	int iTotalUp = 0;
	int iTotalDown = 0;

	if ( iWeaponType == WEAPON_EVERYTHING )
	{
		iTotalDollars = g_flTotalDollarsRifleEquipment;
		iTotalPurchases = g_flTotalWeaponPurchasesRifleEquipment; 
	}

	SortWeaponRanks( iWeaponType, pWeaponsArray, SORT_PURCHASE );

	int iCount = GetWeaponTypeCount( iWeaponType );
	float flPercentage = 0.0f;
	int iSign = 1;

	for ( int i = 0; i < iCount; i++ )
	{
		float flDollarAmount = ((float)pWeaponsArray[i].iCurrentPrice * PRICE_SCALE) * ((float)pWeaponsArray[i].iPurchaseCount * PRICE_SCALE);
		float flTest = flDollarAmount / (float)iTotalDollars;
		float flTest2 = (float)pWeaponsArray[i].iPurchaseCount / (float)iTotalPurchases;

		if ( iSign == 1 )
		{
			float flMod = (float)pWeaponsArray[i].iDefaultPrice / (float)pWeaponsArray[i].iCurrentPrice;

			g_iNewWeaponPrices[pWeaponsArray[i].iID] = ceil( pWeaponsArray[i].iCurrentPrice + ( ( pWeaponsArray[i].iCurrentPrice * flTest2 ) * flMod ) );
			iTotalUp += iTotalDollars * flTest;
		}
		else
		{
			float flMod = (float)pWeaponsArray[i].iCurrentPrice / (float)pWeaponsArray[i].iDefaultPrice;

			g_iNewWeaponPrices[pWeaponsArray[i].iID] = pWeaponsArray[i].iCurrentPrice - (( 1.0f / flTest2 )  * flMod );
			iTotalDown += iTotalDollars * flTest;
		}

		if ( g_iNewWeaponPrices[pWeaponsArray[i].iID] <= 0 )
		{
			g_iNewWeaponPrices[pWeaponsArray[i].iID] = 1;
		}

		g_iPriceDelta[pWeaponsArray[i].iID] = g_iNewWeaponPrices[pWeaponsArray[i].iID] - pWeaponsArray[i].iCurrentPrice;

		flPercentage = flPercentage + flTest;

		if ( flPercentage >= 0.5f )
		{
			iSign = -1;
		}
	}
}

void ProcessBlackMarket( void )
{
	int iPistol = 0;
	int iRifles = 0;

	for ( int i = 1; i < WEAPON_MAX; i++ )
	{
		if ( g_Weapons[i].iWeaponType == WEAPON_PISTOL )
		{
			g_flTotalDollarsPistols += ( (float)g_Weapons[i].iCurrentPrice * PRICE_SCALE) * ( (float)g_Weapons[i].iPurchaseCount * PRICE_SCALE);
			g_flTotalWeaponPurchasesPistols += g_Weapons[i].iPurchaseCount;

			g_PistolRanks[iPistol] = g_Weapons[i];
			g_PistolRanks[iPistol].iPurchaseCount = g_Weapons[i].iPurchaseCount; //g_iCurrentWeaponPurchases[i];
			iPistol++;
		}
		else if ( g_Weapons[i].iWeaponType == WEAPON_EVERYTHING )
		{
			g_flTotalDollarsRifleEquipment += ( (float)g_Weapons[i].iCurrentPrice * PRICE_SCALE ) * ( (float)g_Weapons[i].iPurchaseCount * PRICE_SCALE );
			g_flTotalWeaponPurchasesRifleEquipment += g_Weapons[i].iPurchaseCount;

			g_RifleAndEquipmentRanks[iRifles] = g_Weapons[i];
			g_RifleAndEquipmentRanks[iRifles].iPurchaseCount = g_Weapons[i].iPurchaseCount; //g_iCurrentWeaponPurchases[i];
			iRifles++;
		}
	}

	memset( g_iPriceDelta, 0, sizeof( g_iPriceDelta ) );

	CalculateNewPrices( g_PistolRanks, WEAPON_PISTOL );
	CalculateNewPrices( g_RifleAndEquipmentRanks, WEAPON_EVERYTHING );
}


//=================================
// Add the current snapshot to the db
//=================================
void AddSnapshotToDatabase( void )
{
	char szSnapshot[1024];

	//Only update the prices and close.
	if ( g_bWeeklyUpdate == true )
	{
		weeklyprice_t prices;
		CUtlBuffer buf;

		prices.iVersion = PRICE_BLOB_VERSION;

		for ( int i = 1; i < WEAPON_MAX; i ++ )
		{
			Q_snprintf( szSnapshot, sizeof( szSnapshot ), "update weapon_info set current_price=%d, purchases_thisweek=0 where WeaponID=%d", g_iNewWeaponPrices[i], i  );

			int retcode = mysql->Execute( szSnapshot );
			if ( retcode != 0 )
			{
				Msg( "Query:\n %s\n failed - Retcode: %d\n", szSnapshot, retcode );
			}

			prices.iPreviousPrice[i] = g_Weapons[i].iCurrentPrice;
			prices.iCurrentPrice[i] = g_iNewWeaponPrices[i];
		}
	

		buf.Put( &prices, sizeof( weeklyprice_t ) );

		if ( g_pFullFileSystem )
		{
			g_pFullFileSystem->WriteFile( PRICE_BLOB_NAME, NULL, buf );
		}

		Q_snprintf( szSnapshot, sizeof( szSnapshot ), "update reset_counter set counter=counter+1"  );

		int retcode = mysql->Execute( szSnapshot );
		if ( retcode != 0 )
		{
			Msg( "Query:\n %s\n failed - Retcode: %d\n", szSnapshot, retcode );
		}

		return;
	}

	g_iCounter++;
		
	for ( int i = 1; i < WEAPON_MAX; i ++ )
	{
		Q_snprintf( szSnapshot, sizeof( szSnapshot ), "Insert into purchases_pricing ( snapshot, dt2, weapon_id, purchases, current_price, projected_price, reset_counter ) values ( \"%d_%d\", NOW(), %d, %d, %d, %d, %d );",
					g_iCounter, i,
				   	i,
					g_iPurchaseDelta[i],
					g_Weapons[i].iCurrentPrice,
					g_iNewWeaponPrices[i], 
					g_iResetCounter );

		int retcode = mysql->Execute( szSnapshot );
		if ( retcode != 0 )
		{
			Msg( "Query:\n %s\n failed - Retcode: %d\n", szSnapshot, retcode );
		}
	}


	Q_snprintf( szSnapshot, sizeof( szSnapshot ), "update snapshot_counter set counter=%d", g_iCounter  );

	int retcode = mysql->Execute( szSnapshot );
	if ( retcode != 0 )
	{
		Msg( "Query:\n %s\n failed - Retcode: %d\n", szSnapshot, retcode );
	}

	for ( int i = 1; i < WEAPON_MAX; i ++ )
	{
		Q_snprintf( szSnapshot, sizeof( szSnapshot ), "update weapon_info set purchases=%d, purchases_thisweek=purchases_thisweek+%d where WeaponID=%d", g_iCurrentWeaponPurchases[i], g_iPurchaseDelta[i], i  );

		int retcode = mysql->Execute( szSnapshot );
		if ( retcode != 0 )
		{
			Msg( "Query:\n %s\n failed - Retcode: %d\n", szSnapshot, retcode );
		}
	}

	Msg( "Added new snapshot to database\n", szSnapshot, retcode );
	
}

int main(int argc, char* argv[])
{
	bool bSnapShot = false;

	InitDefaultFileSystem();

	if ( argc > 1 )
	{
		for ( int i = 0; i < argc; i++ )
		{
			if ( Q_stricmp( "-snapshot", argv[i] ) == 0 )
			{
				bSnapShot = true;
			}

			if( Q_stricmp( "-weeklyupdate", argv[i] ) == 0 )
			{
				g_bWeeklyUpdate = true;
			}
		}
	}

	// Now connect to steamweb and update the engineaccess table
	sql = Sys_LoadModule( "mysql_wrapper" );
	if ( sql )
	{
		factory = Sys_GetFactory( sql );
		if ( factory )
		{
			mysql = ( IMySQL * )factory( MYSQL_WRAPPER_VERSION_NAME, NULL );
			if ( mysql == NULL )
			{
				Msg( "Unable to get MYSQL_WRAPPER_VERSION_NAME(%s) from mysql_wrapper\n", MYSQL_WRAPPER_VERSION_NAME );
				exit( -1 );
			}
		}
		else
		{
			Msg( "Sys_GetFactory on mysql_wrapper failed\n" );
			exit( -1 );
		}
	}
	else
	{
		Msg( "Sys_LoadModule( mysql_wrapper ) failed\n" );
		exit( -1 );
	}

	if ( GetCurrentPurchaseCount() == false )
		exit( -1 );

	ProcessBlackMarket();

	if ( bSnapShot == true )
	{
		AddSnapshotToDatabase();
	}
	
	PrintNewPrices();

	if ( mysql )
	{
		mysql->Release();
	}

	return 0;
}


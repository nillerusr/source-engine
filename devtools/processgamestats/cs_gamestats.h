//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The cs game stats header
//
// $NoKeywords: $
//=============================================================================//

#define CS_STATS_BLOB_VERSION 3

extern const char *pValidStatLevels[];

#define CS_NUM_LEVELS 18
#define WEAPON_MAX 34

int GetCSLevelIndex( const char *pLevelName );

typedef struct
{
	char szGameName[8];
	byte iVersion;
	char szMapName[32];
	char ipAddr[4];
	short port;
	int serverid;
} gamestats_header_t;

typedef struct
{
	gamestats_header_t	header;
	short	iMinutesPlayed;

	short	iTerroristVictories[CS_NUM_LEVELS];
	short	iCounterTVictories[CS_NUM_LEVELS];
	short	iBlackMarketPurchases[WEAPON_MAX];

	short	iAutoBuyPurchases;
	short	iReBuyPurchases;
	short	iAutoBuyM4A1Purchases;
	short	iAutoBuyAK47Purchases;
	short	iAutoBuyFamasPurchases;
	short	iAutoBuyGalilPurchases;
	short	iAutoBuyVestHelmPurchases;
	short	iAutoBuyVestPurchases;

} cs_gamestats_t;

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef URLRETRIEVETHREAD_H
#define URLRETRIEVETHREAD_H

#include "KeyValues.h"
#include "cs_weapon_parse.h"

bool BlackMarket_DownloadPrices( void );

#define MED_BUFFER_SIZE 1024
#define SMALL_BUFFER_SIZE 255
#define MAX_DNS_NAME 255

#define PRICE_BLOB_VERSION 1
#define PRICE_BLOB_NAME "weeklyprices.dat"

struct weeklyprice_t
{
	short iVersion;
	short iPreviousPrice[WEAPON_MAX];
	short iCurrentPrice[WEAPON_MAX];
};

#endif // URLRETRIEVETHREAD_H

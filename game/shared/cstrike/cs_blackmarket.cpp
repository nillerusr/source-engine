//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "cs_gamerules.h"
#include "cs_blackmarket.h"
#include "weapon_csbase.h"
#include "filesystem.h"
#include <KeyValues.h>
#include "cs_gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern int g_iRoundCount;

#ifndef CLIENT_DLL
inline void CBlackMarketElement::NetworkStateChanged()
{
}


inline void CBlackMarketElement::NetworkStateChanged( void *pVar )
{
}

blackmarket_items_t blackmarket_items[] =
{
	{ "kevlar",	KEVLAR_PRICE },
	{ "assaultsuit",	ASSAULTSUIT_PRICE },
	{ "nightvision", NVG_PRICE },
};


CUtlVector<CBlackMarketElement> g_BlackMarket_WeaponsBought;

void TrackAutoBuyPurchases( const char *pWeaponName, CCSPlayer *pBuyer )
{
	if ( pBuyer->IsInAutoBuy() == false )
		return;

	if ( pBuyer->IsInAutoBuy() == true )
	{
		if ( Q_stristr( pWeaponName, "m4a1" ) )
		{
			g_iAutoBuyM4A1Purchases++;
		}
		else if ( Q_stristr( pWeaponName, "ak47" ) )
		{
			g_iAutoBuyAK47Purchases++;
		}
		else if ( Q_stristr( pWeaponName, "famas" ) )
		{
			g_iAutoBuyFamasPurchases++;
		}
		else if ( Q_stristr( pWeaponName, "galil" ) )
		{
			g_iAutoBuyGalilPurchases++;
		}
		else if ( Q_stristr( pWeaponName, "assault" ) )
		{
			g_iAutoBuyVestHelmPurchases++;
		}
		else if ( Q_stristr( pWeaponName, "kevlar" ) )
		{
			g_iAutoBuyVestPurchases++;
		}
	}
}

void BlackMarketAddWeapon( const char *pWeaponName, CCSPlayer *pBuyer )
{
	//Ignore bot purchases.
	if ( pBuyer && pBuyer->IsBot() )
		return;

	CSWeaponID iWeaponID = AliasToWeaponID( pWeaponName );

	TrackAutoBuyPurchases( pWeaponName, pBuyer );

	if ( g_BlackMarket_WeaponsBought.Count() > 0 )
	{
		for ( int i = 0; i < g_BlackMarket_WeaponsBought.Count(); i++ )
		{
			if ( g_BlackMarket_WeaponsBought[i].m_iWeaponID == iWeaponID )
			{
				g_BlackMarket_WeaponsBought[i].m_iTimesBought++;
				g_iWeaponPurchases[g_BlackMarket_WeaponsBought[i].m_iWeaponID]++;
				return;
			}
		}
	}

	CBlackMarketElement newweapon;

	newweapon.m_iWeaponID = iWeaponID;
	newweapon.m_iTimesBought = 1;
	newweapon.m_iPrice = 0;
	g_iWeaponPurchases[newweapon.m_iWeaponID] = 1;

	g_BlackMarket_WeaponsBought.AddToTail( newweapon );
}

int GetPistolCount( void )
{
	int iNumPistol = 0;

	for ( int j = 1; j < WEAPON_MAX; j++ )
	{
		CCSWeaponInfo *pWeaponInfo = GetWeaponInfo( (CSWeaponID)j );

		if ( pWeaponInfo )
		{
			if ( pWeaponInfo->m_WeaponType == WEAPONTYPE_PISTOL )
			{
				iNumPistol++;
			}
		}
	}

	return iNumPistol;
}

int GetRifleCount( void )
{
	int iNumRifle = 0;

	for ( int j = 1; j < WEAPON_MAX; j++ )
	{
		CCSWeaponInfo *pWeaponInfo = GetWeaponInfo( (CSWeaponID)j );

		if ( pWeaponInfo )
		{
			if ( pWeaponInfo->m_WeaponType != WEAPONTYPE_PISTOL )
			{
				iNumRifle++;
			}
		}
	}

	return iNumRifle + ARRAYSIZE( blackmarket_items );
}

#endif
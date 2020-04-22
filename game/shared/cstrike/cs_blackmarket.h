//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Holds defintion for game ammo types
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef CS_BLACKMARKET_H
#define CS_BLACKMARKET_H

#include "cs_weapon_parse.h"

#ifdef CLIENT_DLL
#include "c_cs_player.h"
#else
#include "cs_player.h"
#endif


#ifdef _WIN32
#pragma once
#endif

struct blackmarket_items_t
{
	const char		*pClassname;
	int				iDefaultPrice;
};

extern blackmarket_items_t blackmarket_items[];

void BlackMarketAddWeapon( const char *pWeaponName, CCSPlayer *pBuyer );


enum
{
	KEVLAR_PRICE = 650,
	HELMET_PRICE = 350,
	ASSAULTSUIT_PRICE = 1000,
	DEFUSEKIT_PRICE = 200,
	NVG_PRICE = 1250,
	SHIELD_PRICE = 2200
};

#ifndef CLIENT_DLL
class CBlackMarketElement
{
public:

	DECLARE_CLASS_NOBASE( CBlackMarketElement );

	// For CNetworkVars.
	void NetworkStateChanged();
	void NetworkStateChanged( void *pVar );

	CBlackMarketElement()
	{
		m_iPrice = 0;
		m_iTimesBought = 0;
		m_iWeaponID = 0;
	}

	CNetworkVar( int, m_iPrice );
	CNetworkVar( int, m_iWeaponID );

	int  m_iTimesBought;
};


#else

class C_BlackMarketElement
{
public:

	// This allows the datatables to access private members.
	ALLOW_DATATABLES_PRIVATE_ACCESS();

	int		m_iWeaponID;
	int		m_iPrice;
};

#define CBlackMarketElement C_BlackMarketElement
#endif

#endif // CS_BLACKMARKET_H
 
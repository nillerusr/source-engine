//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ORDER_BUILDSENTRYGUN_H
#define ORDER_BUILDSENTRYGUN_H
#ifdef _WIN32
#pragma once
#endif


#include "orders.h"


class CPlayerClassDefender;


class COrderBuildSentryGun : public COrder
{
public:
	DECLARE_CLASS( COrderBuildSentryGun, COrder );
	DECLARE_SERVERCLASS();

	// Create an order for the player.
	static bool		CreateOrder( CPlayerClassDefender *pClass );


// COrder overrides.
public:

	virtual bool	Update( void );
};


#endif // ORDER_BUILDSENTRYGUN_H

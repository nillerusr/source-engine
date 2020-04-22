//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ORDER_BUILDSHIELDWALL_H
#define ORDER_BUILDSHIELDWALL_H
#ifdef _WIN32
#pragma once
#endif


#include "orders.h"


class COrderBuildShieldWall : public COrder
{
public:
	DECLARE_CLASS( COrderBuildShieldWall, COrder );
	DECLARE_SERVERCLASS();

	// Create an order for the player.
	static bool		CreateOrder( CPlayerClass *pClass );


// COrder overrides.
public:

	virtual bool	Update( void );
};


#endif // ORDER_BUILDSHIELDWALL_H

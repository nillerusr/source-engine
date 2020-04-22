//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ORDER_RESOURCEPUMP_H
#define ORDER_RESOURCEPUMP_H
#ifdef _WIN32
#pragma once
#endif


#include "orders.h"


class CPlayerClass;


class COrderResourcePump : public COrder
{
public:
	DECLARE_CLASS( COrderResourcePump, COrder );
	DECLARE_SERVERCLASS();

	// Create an order for the player.
	static bool		CreateOrder( CPlayerClass *pClass );


// COrder overrides.
public:

	virtual bool Update( void );
};


#endif // ORDER_RESOURCEPUMP_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ORDER_RESUPPLY_H
#define ORDER_RESUPPLY_H
#ifdef _WIN32
#pragma once
#endif


#include "orders.h"


class CPlayerClass;


class COrderResupply : public COrder
{
public:
	DECLARE_CLASS( COrderResupply, COrder );
	DECLARE_SERVERCLASS();

	// Create an order for the player.
	static bool		CreateOrder( CPlayerClass *pClass );


// COrder overrides.
public:

	virtual bool	Update();
};


#endif // ORDER_RESUPPLY_H

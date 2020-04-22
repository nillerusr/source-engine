//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ORDER_BUILDMORTAR_H
#define ORDER_BUILDMORTAR_H
#ifdef _WIN32
#pragma once
#endif


#include "orders.h"


class COrderMortarAttack : public COrder
{
public:
	DECLARE_CLASS( COrderMortarAttack, COrder );
	DECLARE_SERVERCLASS();

	// Create an order for the player.
	static bool		CreateOrder( CPlayerClass *pClass );
};


#endif // ORDER_BUILDMORTAR_H

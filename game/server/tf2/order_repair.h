//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ORDER_REPAIR_H
#define ORDER_REPAIR_H
#ifdef _WIN32
#pragma once
#endif


#include "orders.h"


class CPlayerClass;
class CPlayerClassDefender;


class COrderRepair : public COrder
{
public:
	DECLARE_CLASS( COrderRepair, COrder );
	DECLARE_SERVERCLASS();

	// Create an order for the defender to fix friendly objects.
	static bool		CreateOrder_RepairFriendlyObjects( CPlayerClassDefender *pClass );

	// Create an order for anyone to repair their own objects.
	static bool		CreateOrder_RepairOwnObjects( CPlayerClass *pClass );


// COrder overrides.
public:

	virtual bool	Update( void );
};


#endif // ORDER_REPAIR_H

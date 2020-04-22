//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ORDER_HEAL_H
#define ORDER_HEAL_H
#ifdef _WIN32
#pragma once
#endif


#include "order_player.h"


class CPlayerClass;


class COrderHeal : public COrderPlayer
{
public:
	DECLARE_CLASS( COrderHeal, COrderPlayer );
	DECLARE_SERVERCLASS();

	// Create an order for the player.
	static bool		CreateOrder( CPlayerClass *pClass );


// COrder overrides.
public:

	virtual bool	Update();
	virtual bool	UpdateOnEvent( COrderEvent_Base *pEvent );
};


#endif // ORDER_HEAL_H

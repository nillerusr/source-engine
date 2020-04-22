//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ORDER_KILLMORTARGUY_H
#define ORDER_KILLMORTARGUY_H
#ifdef _WIN32
#pragma once
#endif


#include "order_player.h"


class CPlayerClass;


class COrderKillMortarGuy : public COrderPlayer
{
public:
	DECLARE_CLASS( COrderKillMortarGuy, COrderPlayer );
	DECLARE_SERVERCLASS();

	// Create an order for the player.
	static bool		CreateOrder( CPlayerClass *pClass );


// COrder overrides.
public:

	virtual bool	UpdateOnEvent( COrderEvent_Base *pEvent );
};


#endif // ORDER_KILLMORTARGUY_H

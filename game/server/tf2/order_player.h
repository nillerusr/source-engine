//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ORDER_PLAYER_H
#define ORDER_PLAYER_H
#ifdef _WIN32
#pragma once
#endif


#include "orders.h"


class COrderPlayer : public COrder
{
public:
	DECLARE_CLASS( COrderPlayer, COrder );
	DECLARE_SERVERCLASS();


// COrder overrides.
public:

	virtual bool	UpdateOnEvent( COrderEvent_Base *pEvent );
};


#endif // ORDER_PLAYER_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_ORDER_PLAYER_H
#define C_ORDER_PLAYER_H
#ifdef _WIN32
#pragma once
#endif


#include "c_order.h"


// Orders that point at players.
class C_OrderPlayer : public C_Order
{
public:
	DECLARE_CLASS( C_OrderPlayer, C_Order );
	DECLARE_CLIENTCLASS();


// C_Order overrides.
public:

	virtual void	GetTargetDescription( char *pDest, int bufferSize );
};


#endif // C_ORDER_PLAYER_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_ORDER_HEAL_H
#define C_ORDER_HEAL_H
#ifdef _WIN32
#pragma once
#endif


#include "c_order_player.h"


class C_OrderHeal : public C_OrderPlayer
{
public:
	DECLARE_CLASS( C_OrderHeal, C_OrderPlayer );
	DECLARE_CLIENTCLASS();


// C_Order overrides.
public:

	virtual void	GetDescription( char *pDest, int bufferSize );
};


#endif // C_ORDER_HEAL_H

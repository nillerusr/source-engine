//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_ORDER_ASSIST_H
#define C_ORDER_ASSIST_H
#ifdef _WIN32
#pragma once
#endif


#include "c_order_player.h"


class C_OrderAssist : public C_OrderPlayer
{
public:
	DECLARE_CLASS( C_OrderAssist, C_OrderPlayer );
	DECLARE_CLIENTCLASS();


// C_Order overrides.
public:

	virtual void	GetDescription( char *pDest, int bufferSize );
};


#endif // C_ORDER_ASSIST_H

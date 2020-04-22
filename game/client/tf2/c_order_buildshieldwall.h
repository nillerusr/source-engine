//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_ORDER_BUILDSHIELDWALL_H
#define C_ORDER_BUILDSHIELDWALL_H
#ifdef _WIN32
#pragma once
#endif


#include "c_order.h"


class C_OrderBuildShieldWall : public C_Order
{
public:
	DECLARE_CLASS( C_OrderBuildShieldWall, C_Order );
	DECLARE_CLIENTCLASS();


// C_Order overrides.
public:

	virtual void	GetDescription( char *pDest, int bufferSize );
};


#endif // C_ORDER_BUILDSHIELDWALL_H

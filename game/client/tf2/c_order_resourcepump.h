//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_ORDER_RESOURCEPUMP_H
#define C_ORDER_RESOURCEPUMP_H
#ifdef _WIN32
#pragma once
#endif


#include "c_order.h"


class C_OrderResourcePump : public C_Order
{
public:
	DECLARE_CLASS( C_OrderResourcePump, C_Order );
	DECLARE_CLIENTCLASS();

					C_OrderResourcePump();


// C_Order overrides.
public:

	virtual void	GetDescription( char *pDest, int bufferSize );
};


#endif // C_ORDER_RESOURCEPUMP_H

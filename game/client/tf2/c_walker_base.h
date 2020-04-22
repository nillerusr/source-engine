//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_WALKER_BASE_H
#define C_WALKER_BASE_H
#ifdef _WIN32
#pragma once
#endif


#include "basetfvehicle.h"
#include "client_class.h"


class C_WalkerBase : public C_BaseTFVehicle
{
public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( C_WalkerBase, C_BaseTFVehicle );

	C_WalkerBase() {}

	virtual void OnDataChanged( DataUpdateType_t updateType );

private:
	C_WalkerBase( const C_WalkerBase &other ) {}
};


#endif // C_WALKER_BASE_H

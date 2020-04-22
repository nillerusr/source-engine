//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_ORDER_ASSIST_H
#define TF_ORDER_ASSIST_H
#ifdef _WIN32
#pragma once
#endif


#include "order_player.h"


class CPlayerClass;


class COrderAssist : public COrderPlayer
{
public:
	DECLARE_CLASS( COrderAssist, COrderPlayer );
	DECLARE_SERVERCLASS();

	// Create an order for the player.
	static bool		CreateOrder( CPlayerClass *pClass );


// COrder overrides.
public:

	virtual bool	Update();
	virtual bool	UpdateOnEvent( COrderEvent_Base *pEvent );


private:
	
	enum
	{
		NUM_ASSIST_ENEMIES = 2
	};

	// The order goes away when the player who has the assist order shoots
	// one of these enemies.
	CHandle<CBaseEntity>	m_Enemies[NUM_ASSIST_ENEMIES];
};


#endif // TF_ORDER_ASSIST_H

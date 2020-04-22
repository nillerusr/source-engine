//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_ORDER_EVENTS_H
#define TF_ORDER_EVENTS_H
#ifdef _WIN32
#pragma once
#endif


#include "tf_player.h"


//-----------------------------------------------------------------------------
// ORDER EVENTS
//-----------------------------------------------------------------------------

typedef enum
{
	ORDER_EVENT_PLAYER_DISCONNECTED,	// COrderEvent_PlayerDisconnected
	ORDER_EVENT_PLAYER_KILLED,			// CorderEvent_PlayerKilled
	ORDER_EVENT_PLAYER_RESPAWNED,		// COrderEvent_PlayerRespawned
	ORDER_EVENT_OBJECT_DESTROYED,		// COrderEvent_ObjectDestroyed
	ORDER_EVENT_PLAYER_DAMAGED			// COrderEvent_PlayerDamaged
} OrderEventType;


abstract_class COrderEvent_Base
{
public:
	virtual OrderEventType	GetType() = 0;
};


// Fire a global order event. It goes to all orders so they can determine if 
// they want to react.
void GlobalOrderEvent( COrderEvent_Base *pOrder );


class COrderEvent_PlayerDisconnected : public COrderEvent_Base
{
public:
							COrderEvent_PlayerDisconnected( CBaseEntity *pPlayer )
							{
								m_pPlayer = pPlayer;
							}

	virtual OrderEventType	GetType()	{ return ORDER_EVENT_PLAYER_DISCONNECTED; }

	CBaseEntity		*m_pPlayer;
};


class COrderEvent_PlayerKilled : public COrderEvent_Base
{
public:
							COrderEvent_PlayerKilled( CBaseEntity *pPlayer )
							{
								m_pPlayer = pPlayer;
							}

	virtual OrderEventType	GetType()	{ return ORDER_EVENT_PLAYER_KILLED; }

	CBaseEntity		*m_pPlayer;
};


class COrderEvent_PlayerRespawned : public COrderEvent_Base
{
public:
							COrderEvent_PlayerRespawned( CBaseEntity *pPlayer )
							{
								m_pPlayer = pPlayer;
							}

	virtual OrderEventType	GetType()	{ return ORDER_EVENT_PLAYER_RESPAWNED; }

	CBaseEntity		*m_pPlayer;
};


class COrderEvent_ObjectDestroyed : public COrderEvent_Base
{
public:
							COrderEvent_ObjectDestroyed( CBaseEntity *pObj )
							{
								m_pObject = pObj;
							}

	virtual OrderEventType	GetType()	{ return ORDER_EVENT_OBJECT_DESTROYED; }

	CBaseEntity		*m_pObject;
};


class COrderEvent_PlayerDamaged : public COrderEvent_Base
{
public:
	virtual OrderEventType	GetType()	{ return ORDER_EVENT_PLAYER_DAMAGED; }

	CBaseEntity		*m_pPlayerDamaged;
	CTakeDamageInfo	m_TakeDamageInfo;
};


#endif // TF_ORDER_EVENTS_H

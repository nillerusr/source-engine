//========= Copyright (c) 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAME_EVENT_LISTENER_H
#define GAME_EVENT_LISTENER_H
#ifdef _WIN32
#pragma once
#endif

#include "igameevents.h"
extern IGameEventManager2 *gameeventmanager;

// A safer method than inheriting straight from IGameEventListener2.
// Avoids requiring the user to remove themselves as listeners in 
// their deconstructor, and sets the serverside variable based on
// our dll location.
class CGameEventListener : public IGameEventListener2
{
public:
	CGameEventListener() : m_bRegisteredForEvents(false)
	{
		m_nDebugID = EVENT_DEBUG_ID_INIT;
	}

	~CGameEventListener()
	{
		m_nDebugID = EVENT_DEBUG_ID_SHUTDOWN;
		StopListeningForAllEvents();
	}

	void ListenForGameEvent( const char *name )
	{
		m_bRegisteredForEvents = true;

#ifdef CLIENT_DLL
		bool bServerSide = false;
#else
		bool bServerSide = true;
#endif

		gameeventmanager->AddListener( this, name, bServerSide );
	}

	void StopListeningForAllEvents()
	{
		// remove me from list
		if ( m_bRegisteredForEvents )
		{
			gameeventmanager->RemoveListener( this );
			m_bRegisteredForEvents = false;
		}
	}

	// Intentionally abstract
	virtual void FireGameEvent( IGameEvent *event ) = 0;
	int m_nDebugID;
	virtual int GetEventDebugID( void )			{ return m_nDebugID; }

private:

	// Have we registered for any events?
	bool m_bRegisteredForEvents;
};

#endif

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: wrapper for GameEvent manager legacy support
//
// $NoKeywords: $
//
//=============================================================================//
// GameEventManager.cpp: implementation of the CGameEventManager class.
//
//////////////////////////////////////////////////////////////////////

#include "GameEventManager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CGameEventManagerOld  : public IGameEventManager
{

public:	// IGameEventManager wrapper

	int			LoadEventsFromFile( const char * filename )	{ return g_GameEventManager.LoadEventsFromFile( filename ); }
	KeyValues	*GetEvent(const char * name);
	void		Reset() { g_GameEventManager.Reset(); }

	bool AddListener( IGameEventListener * listener, const char * event, bool bIsServerSide	);
	bool AddListener( IGameEventListener * listener, bool bIsServerSide );
	void RemoveListener(IGameEventListener * listener);

	bool FireEvent( KeyValues * event );
	bool FireEventClientOnly( KeyValues * event );
	bool FireEventServerOnly( KeyValues * event );

	bool SerializeKeyValues( KeyValues *event, bf_write *buf, CGameEvent *eventtype = NULL );
	KeyValues * UnserializeKeyValue( bf_read *msg ); // create new KeyValues, must be deleted

protected:
	bool FireEventIntern( KeyValues * event, bool bServerSide, bool bClientSide );

};

static CGameEventManagerOld s_GameEventManagerOld;

// Expose CVEngineServer to the engine.

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGameEventManagerOld, IGameEventManager, INTERFACEVERSION_GAMEEVENTSMANAGER, s_GameEventManagerOld );


bool CGameEventManagerOld::AddListener( IGameEventListener * listener, const char * event, bool bIsServerSide	)
{
	CGameEventDescriptor *descriptor = g_GameEventManager.GetEventDescriptor( event );

	if ( !descriptor )
		return false;

	if ( bIsServerSide )
	{
		return g_GameEventManager.AddListener( listener, descriptor, CGameEventManager::SERVERSIDE_OLD );
	}
	else
	{
		return g_GameEventManager.AddListener( listener, descriptor, CGameEventManager::CLIENTSIDE_OLD );
	}
}

bool CGameEventManagerOld::AddListener( IGameEventListener * listener, bool bIsServerSide )
{
	if ( bIsServerSide )
	{
		return g_GameEventManager.AddListenerAll( listener, CGameEventManager::SERVERSIDE_OLD );
	}
	else
	{
		return g_GameEventManager.AddListenerAll( listener, CGameEventManager::CLIENTSIDE_OLD );
	}
}

void CGameEventManagerOld::RemoveListener(IGameEventListener * listener)
{
	g_GameEventManager.RemoveListenerOld( listener );
}

KeyValues *CGameEventManagerOld::GetEvent(const char * name)
{
	CGameEventDescriptor *event = g_GameEventManager.GetEventDescriptor( name );

	if ( !event )
		return NULL;

	return event->keys;
}

bool CGameEventManagerOld::FireEvent( KeyValues * event )
{
	return FireEventIntern( event, false, false );
}

bool CGameEventManagerOld::FireEventClientOnly( KeyValues * event )
{
	return FireEventIntern( event, false, true );
}

bool CGameEventManagerOld::FireEventServerOnly( KeyValues * event )
{
	return FireEventIntern( event, true, false );
}

bool CGameEventManagerOld::FireEventIntern( KeyValues *keys, bool bServerSideOnly, bool bClientSideOnly )
{
	if ( !keys )
		return false;

	CGameEvent *event = (CGameEvent*) g_GameEventManager.CreateEvent( keys->GetName() );

	if ( !event )
		return false;

	event->m_pDataKeys->deleteThis();
	event->m_pDataKeys = keys;

	if ( bClientSideOnly )
	{
		return g_GameEventManager.FireEventClientSide( event );
	}
	else
	{
		return g_GameEventManager.FireEvent( event, bServerSideOnly );
	}
}



bool CGameEventManagerOld::SerializeKeyValues( KeyValues* event, bf_write* buf, CGameEvent* eventtype )
{
	DevMsg("SerializeKeyValues:: not supported\n");
	return false;
}

KeyValues *CGameEventManagerOld::UnserializeKeyValue( bf_read *buf)
{
	DevMsg("UnserializeKeyValue:: not supported\n");
	return NULL;
}


//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: gameeventmanager.h: interface for the CGameEventManager class.
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//

#if !defined ( GAMEEVENTMANAGER_H )
#define GAMEEVENTMANAGER_H

#ifdef _WIN32
#pragma once
#endif 

#include <igameevents.h>
#include <utlvector.h>
#include <KeyValues.h>
#include <networkstringtabledefs.h>
#include <utlsymbol.h>

class SVC_GameEventList;
class CLC_ListenEvents;

class CGameEventCallback
{
public:
	void				*m_pCallback;		// callback pointer
	int					m_nListenerType;	// client or server side ?
};

class CGameEventDescriptor
{
public:
	CGameEventDescriptor()
	{
		name[0] = 0;
		eventid = -1;
		keys = NULL;
		local = false;
		reliable = true;
	}

public:
	char		name[MAX_EVENT_NAME_LENGTH];	// name of this event
	int			eventid;	// network index number, -1 = not networked
	KeyValues	*keys;		// KeyValue describing data types, if NULL only name 
	bool		local;		// local event, never tell clients about that
	bool		reliable;	// send this event as reliable message
    CUtlVector<CGameEventCallback*>	listeners;	// registered listeners
};

class CGameEvent : public IGameEvent
{
public:

	CGameEvent( CGameEventDescriptor *descriptor );
	virtual ~CGameEvent();

	const char *GetName() const;
	bool  IsEmpty(const char *keyName = NULL);
	bool  IsLocal() const;
	bool  IsReliable() const;

	bool  GetBool( const char *keyName = NULL, bool defaultValue = false );
	int   GetInt( const char *keyName = NULL, int defaultValue = 0 );
	float GetFloat( const char *keyName = NULL, float defaultValue = 0.0f );
	const char *GetString( const char *keyName = NULL, const char *defaultValue = "" );

	void SetBool( const char *keyName, bool value );
	void SetInt( const char *keyName, int value );
	void SetFloat( const char *keyName, float value );
	void SetString( const char *keyName, const char *value );
	
	CGameEventDescriptor	*m_pDescriptor;
	KeyValues				*m_pDataKeys;
};

class CGameEventManager : public IGameEventManager2
{
	friend class CGameEventManagerOld;

public:	// IGameEventManager functions

	enum
	{
		SERVERSIDE = 0,		// this is a server side listener, event logger etc
		CLIENTSIDE,			// this is a client side listenet, HUD element etc
		CLIENTSTUB,			// this is a serverside stub for a remote client listener (used by engine only)
		SERVERSIDE_OLD,		// legacy support for old server event listeners
		CLIENTSIDE_OLD,		// legecy support for old client event listeners
	};

	enum
	{
		TYPE_LOCAL = 0,	// not networked
		TYPE_STRING,	// zero terminated ASCII string
		TYPE_FLOAT,		// float 32 bit
		TYPE_LONG,		// signed int 32 bit
		TYPE_SHORT,		// signed int 16 bit
		TYPE_BYTE,		// unsigned int 8 bit
		TYPE_BOOL		// unsigned int 1 bit
	};

	CGameEventManager();
	virtual ~CGameEventManager();
	
	int	 LoadEventsFromFile( const char * filename );
	void Reset();
			
	bool AddListener( IGameEventListener2 *listener, const char *name, bool bServerSide );
	bool FindListener( IGameEventListener2 *listener, const char *name );
	void RemoveListener( IGameEventListener2 *listener);
		
	IGameEvent *CreateEvent( const char *name, bool bForce = false );
	IGameEvent *DuplicateEvent( IGameEvent *event);
	bool FireEvent( IGameEvent *event, bool bDontBroadcast = false );
	bool FireEventClientSide( IGameEvent *event );
	void FreeEvent( IGameEvent *event );

	bool SerializeEvent( IGameEvent *event, bf_write *buf );
	IGameEvent *UnserializeEvent( bf_read *buf );

public:
	bool Init();
	void Shutdown();
	void ReloadEventDefinitions();	// called by server on new map
	bool AddListener( void *listener, CGameEventDescriptor *descriptor, int nListenerType );

    CGameEventDescriptor *GetEventDescriptor( const char *name );
	CGameEventDescriptor *GetEventDescriptor( IGameEvent *event );
	CGameEventDescriptor *GetEventDescriptor( int eventid );

	void WriteEventList(SVC_GameEventList *msg);
	bool ParseEventList(SVC_GameEventList *msg);

	void WriteListenEventList(CLC_ListenEvents *msg);
	bool HasClientListenersChanged( bool bReset = true );
	void ConPrintEvent( IGameEvent *event);
	
	// legacy support 
	bool AddListenerAll( void *listener, int nListenerType );
	void RemoveListenerOld( void *listener);
	
	
protected:

	IGameEvent *CreateEvent( CGameEventDescriptor *descriptor );
	bool RegisterEvent( KeyValues * keys );
	void UnregisterEvent(int index);
	bool FireEventIntern( IGameEvent *event, bool bServerSide, bool bClientOnly );
	CGameEventCallback* FindEventListener( void* listener );
	
	CUtlVector<CGameEventDescriptor>	m_GameEvents;	// list of all known events
	CUtlVector<CGameEventCallback*>		m_Listeners;	// list of all registered listeners
	CUtlSymbolTable						m_EventFiles;	// list of all loaded event files
	CUtlVector<CUtlSymbol>				m_EventFileNames; 

	bool	m_bClientListenersChanged;	// true every time client changed listeners
};

extern CGameEventManager &g_GameEventManager;

#endif 

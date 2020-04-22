//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// GameEventManager.cpp: implementation of the CGameEventManager class.
//
//////////////////////////////////////////////////////////////////////

#include "GameEventManager.h"
#include "filesystem_engine.h"
#include "server.h"
#include "client.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static CGameEventManager s_GameEventManager;
CGameEventManager &g_GameEventManager = s_GameEventManager;

static const char *s_GameEnventTypeMap[] = 
{	"local",	// 0 : don't network this field
	"string",	// 1 : zero terminated ASCII string
	"float",	// 2 : float 32 bit
	"long",		// 3 : signed int 32 bit
	"short",	// 4 : signed int 16 bit
	"byte",		// 5 : unsigned int 8 bit
	"bool",		// 6 : unsigned int 1 bit
	NULL };

static ConVar net_showevents( "net_showevents", "0", FCVAR_CHEAT, "Dump game events to console (1=client only, 2=all)." );

// Expose CVEngineServer to the engine.

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGameEventManager, IGameEventManager2, INTERFACEVERSION_GAMEEVENTSMANAGER2, s_GameEventManager );

CGameEvent::CGameEvent( CGameEventDescriptor *descriptor )
{
	Assert( descriptor );
	m_pDescriptor = descriptor;
	m_pDataKeys = new KeyValues( descriptor->name );
}

CGameEvent::~CGameEvent()
{
	m_pDataKeys->deleteThis();
}

bool CGameEvent::GetBool( const char *keyName, bool defaultValue)
{
	return m_pDataKeys->GetInt( keyName, defaultValue ) != 0;
}

int CGameEvent::GetInt( const char *keyName, int defaultValue)
{
	return m_pDataKeys->GetInt( keyName, defaultValue );
}

float CGameEvent::GetFloat( const char *keyName, float defaultValue )
{
	return m_pDataKeys->GetFloat( keyName, defaultValue );
}

const char *CGameEvent::GetString( const char *keyName, const char *defaultValue )
{
	return m_pDataKeys->GetString( keyName, defaultValue );
}

void CGameEvent::SetBool( const char *keyName, bool value )
{
	m_pDataKeys->SetInt( keyName, value?1:0 );
}

void CGameEvent::SetInt( const char *keyName, int value )
{
	m_pDataKeys->SetInt( keyName, value );
}

void CGameEvent::SetFloat( const char *keyName, float value )
{
	m_pDataKeys->SetFloat( keyName, value );
}

void CGameEvent::SetString( const char *keyName, const char *value )
{
	m_pDataKeys->SetString( keyName, value );
}

bool CGameEvent::IsEmpty( const char *keyName )
{
	return m_pDataKeys->IsEmpty( keyName );
}

const char *CGameEvent::GetName() const
{
	return m_pDataKeys->GetName();
}

bool CGameEvent::IsLocal() const
{
	return m_pDescriptor->local;
}

bool CGameEvent::IsReliable() const
{
	return m_pDescriptor->reliable;
}

CGameEventManager::CGameEventManager()
{
	Reset();
}

CGameEventManager::~CGameEventManager()
{
	Reset();
}

bool CGameEventManager::Init()
{
	Reset();

	LoadEventsFromFile( "resource/serverevents.res" );

	return true;
}

void CGameEventManager::Shutdown()
{
	Reset();
}

void CGameEventManager::Reset()
{
	int number = m_GameEvents.Count();

	for (int i = 0; i<number; i++)
	{
		CGameEventDescriptor &e = m_GameEvents.Element( i );

		if ( e.keys )
		{
			e.keys->deleteThis(); // free the value keys
			e.keys = NULL;
		}
					
		e.listeners.Purge();	// remove listeners
	}

	m_GameEvents.Purge();
	m_Listeners.PurgeAndDeleteElements();
	m_EventFiles.RemoveAll();
	m_EventFileNames.RemoveAll();
	m_bClientListenersChanged = true;
	
	Assert( m_GameEvents.Count() == 0 );
}

bool CGameEventManager::HasClientListenersChanged( bool bReset /* = true  */)
{
	if ( !m_bClientListenersChanged )
		return false;

	if ( bReset )
		m_bClientListenersChanged = false;

	return true;
}

void CGameEventManager::WriteEventList(SVC_GameEventList *msg)
{
	// reset event ids to -1 first

	msg->m_nNumEvents = 0;

	for (int i=0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor &descriptor = m_GameEvents[i];

		if ( descriptor.local )
			continue;

		Assert( descriptor.eventid >= 0 && descriptor.eventid < MAX_EVENT_NUMBER );

		msg->m_DataOut.WriteUBitLong( descriptor.eventid, MAX_EVENT_BITS );
		msg->m_DataOut.WriteString( descriptor.name );
		
		KeyValues *key = descriptor.keys->GetFirstSubKey(); 

		while ( key )
		{
			int type = key->GetInt();

			if ( type != TYPE_LOCAL )
			{
				msg->m_DataOut.WriteUBitLong( type, 3 );
				msg->m_DataOut.WriteString( key->GetName() );
			}

			key = key->GetNextKey();
		}

		msg->m_DataOut.WriteUBitLong( TYPE_LOCAL, 3 ); // end marker
	
		msg->m_nNumEvents++;
	}
}

bool CGameEventManager::ParseEventList(SVC_GameEventList *msg)
{
	int i;

	// reset eventids to -1 first
	for ( i=0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor &descriptor = m_GameEvents[i];
		descriptor.eventid = -1;
	}

	// map server event IDs 
	for (i = 0; i<msg->m_nNumEvents; i++)
	{
		int id = msg->m_DataIn.ReadUBitLong( MAX_EVENT_BITS );
		char name[MAX_EVENT_NAME_LENGTH];
		msg->m_DataIn.ReadString( name, sizeof(name) );
        		
		CGameEventDescriptor *descriptor = GetEventDescriptor( name );

		if ( !descriptor )
		{
			// event unknown to client, skip data
			while ( msg->m_DataIn.ReadUBitLong( 3 ) )
				msg->m_DataIn.ReadString( name, sizeof(name) );

			continue;
		}

		// remove old definition list
		if ( descriptor->keys )
			descriptor->keys->deleteThis();

		descriptor->keys = new KeyValues("descriptor");

		int datatype = msg->m_DataIn.ReadUBitLong( 3 );

		while ( datatype != TYPE_LOCAL )
		{
			msg->m_DataIn.ReadString( name, sizeof(name) );
			descriptor->keys->SetInt( name, datatype );

			datatype = msg->m_DataIn.ReadUBitLong( 3 );
		}

		descriptor->eventid = id;
	}

	// force client to answer what events he listens to
	m_bClientListenersChanged = true;

	return true;
}
	
void CGameEventManager::WriteListenEventList(CLC_ListenEvents *msg)
{
	msg->m_EventArray.ClearAll();

	// and know tell the server what events we want to listen to
	for (int i=0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor &descriptor = m_GameEvents[i];

		bool bHasClientListener = false;

		for ( int j=0; j<descriptor.listeners.Count(); j++ )
		{
			CGameEventCallback *listener = descriptor.listeners[j];

			if ( listener->m_nListenerType == CGameEventManager::CLIENTSIDE ||
				 listener->m_nListenerType == CGameEventManager::CLIENTSIDE_OLD	)
			{
				// if we have a client side listener and server knows this event, add it
				bHasClientListener = true;
				break;
			}
		}

		if ( !bHasClientListener )
			continue;

		if ( descriptor.eventid == -1 )
		{
			DevMsg("Warning! Client listens to event '%s' unknown by server.\n", descriptor.name );
			continue;
		}

		msg->m_EventArray.Set( descriptor.eventid );
	}
}

IGameEvent *CGameEventManager::CreateEvent( CGameEventDescriptor *descriptor )
{
	return new CGameEvent ( descriptor );
}

IGameEvent *CGameEventManager::CreateEvent( const char *name, bool bForce )
{
	if ( !name || !name[0] )
		return NULL;

	CGameEventDescriptor *descriptor = GetEventDescriptor( name );

	// check if this event name is known
	if ( !descriptor )
	{
		DevMsg( "CreateEvent: event '%s' not registered.\n", name );
		return NULL;
	}

	// event is known but no one listen to it
	if ( descriptor->listeners.Count() == 0 && !bForce )
	{
		return NULL;
	}

	// create & return the new event 
	return new CGameEvent ( descriptor );
}

bool CGameEventManager::FireEvent( IGameEvent *event, bool bServerOnly )
{
	return FireEventIntern( event, bServerOnly, false );
}

bool CGameEventManager::FireEventClientSide( IGameEvent *event )
{
	return FireEventIntern( event, false, true );
}

IGameEvent *CGameEventManager::DuplicateEvent( IGameEvent *event )
{
	CGameEvent *gameEvent = dynamic_cast<CGameEvent*>(event);

	if ( !gameEvent )
		return NULL;

	// create new instance
	CGameEvent *newEvent = new CGameEvent ( gameEvent->m_pDescriptor );

	// free keys
	newEvent->m_pDataKeys->deleteThis();

	// and make copy
	newEvent->m_pDataKeys = gameEvent->m_pDataKeys->MakeCopy();

	return newEvent;
}

void CGameEventManager::ConPrintEvent( IGameEvent *event)
{
	CGameEventDescriptor *descriptor = GetEventDescriptor( event );

	if ( !descriptor )
		return;

	KeyValues *key = descriptor->keys->GetFirstSubKey(); 

	while ( key )
	{
		const char * keyName = key->GetName();

		int type = key->GetInt();

		switch ( type )
		{
		case TYPE_LOCAL : ConMsg( "- \"%s\" = \"%s\" (local)\n", keyName, event->GetString(keyName) ); break;
		case TYPE_STRING : ConMsg( "- \"%s\" = \"%s\"\n", keyName, event->GetString(keyName) ); break;
		case TYPE_FLOAT : ConMsg( "- \"%s\" = \"%.2f\"\n", keyName, event->GetFloat(keyName) ); break;
		default: ConMsg( "- \"%s\" = \"%i\"\n", keyName, event->GetInt(keyName) ); break;
		}
		key = key->GetNextKey();
	}
}

bool CGameEventManager::FireEventIntern( IGameEvent *event, bool bServerOnly, bool bClientOnly )
{
	if ( event == NULL )
		return false;

	Assert( !(bServerOnly && bClientOnly) ); // it can't be both

	VPROF_("CGameEventManager::FireEvent", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 
		bClientOnly ? BUDGETFLAG_CLIENT : ( bServerOnly ? BUDGETFLAG_SERVER : BUDGETFLAG_OTHER ) );

	CGameEventDescriptor *descriptor = GetEventDescriptor( event );

	if ( descriptor == NULL )
	{
		DevMsg( "FireEvent: event '%s' not registered.\n", event->GetName() );
		FreeEvent( event );
		return false;
	}

	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s (name: %s listeners: %d)", __FUNCTION__, tmDynamicString( TELEMETRY_LEVEL0, event->GetName() ), descriptor->listeners.Count() );

	// show game events in console
	if ( net_showevents.GetInt() > 0 )
	{
		if ( bClientOnly )
		{
			ConMsg( "Game event \"%s\", Tick %i:\n", descriptor->name, cl.GetClientTickCount() );
			ConPrintEvent( event );
		}
		else if ( net_showevents.GetInt() > 1 )
		{
			ConMsg( "Server event \"%s\", Tick %i:\n", descriptor->name, sv.GetTick() );
			ConPrintEvent( event );
		}
	}

	for ( int i = 0; i < descriptor->listeners.Count(); i++ )
	{
		CGameEventCallback *listener = descriptor->listeners.Element( i );

		Assert ( listener );

		// don't trigger server listners for clientside only events
		if ( ( listener->m_nListenerType == SERVERSIDE ||
			   listener->m_nListenerType == SERVERSIDE_OLD ) &&
			   bClientOnly  )
			continue;

        // don't trigger clientside events, if not explicit a clientside event
		if ( ( listener->m_nListenerType == CLIENTSIDE ||
			   listener->m_nListenerType == CLIENTSIDE_OLD ) &&  
			   !bClientOnly  )
			continue;

		// don't broadcast events if server side only
		if ( listener->m_nListenerType == CLIENTSTUB && (bServerOnly || bClientOnly) )
			continue;

		// TODO optimized the serialize event for clients, call only once and not per client

		// fire event in this listener module
		if ( listener->m_nListenerType == CLIENTSIDE_OLD ||
			 listener->m_nListenerType == SERVERSIDE_OLD )
		{
			tmZone( TELEMETRY_LEVEL1, TMZF_NONE, "FireGameEvent (i: %d, listenertype: %d (old))", i, listener->m_nListenerType );

			// legacy support for old system
			IGameEventListener *pCallback = static_cast<IGameEventListener*>(listener->m_pCallback);
			CGameEvent *pEvent = static_cast<CGameEvent*>(event);

			pCallback->FireGameEvent( pEvent->m_pDataKeys );
		}
		else
		{
			tmZone( TELEMETRY_LEVEL1, TMZF_NONE, "FireGameEvent (i: %d, listenertype: %d (new))", i, listener->m_nListenerType );

			// new system
			IGameEventListener2 *pCallback =  static_cast<IGameEventListener2*>(listener->m_pCallback);

			pCallback->FireGameEvent( event );
		}	 
	}

	// free event resources
	FreeEvent( event );

	return true;
}

bool CGameEventManager::SerializeEvent( IGameEvent *event, bf_write* buf )
{
	CGameEventDescriptor *descriptor = GetEventDescriptor( event );

	Assert( descriptor );

	buf->WriteUBitLong( descriptor->eventid, MAX_EVENT_BITS );

	// now iterate trough all fields described in gameevents.res and put them in the buffer

	KeyValues * key = descriptor->keys->GetFirstSubKey();

	if ( net_showevents.GetInt() > 2 )
	{
		DevMsg("Serializing event '%s' (%i):\n", descriptor->name, descriptor->eventid );
	}
	
	while ( key )
	{
		const char * keyName = key->GetName();

		int type = key->GetInt();

		if ( net_showevents.GetInt() > 2 )
		{
			DevMsg(" - %s (%i)\n", keyName, type );
		}

		//Make sure every key is used in the event
		// Assert( event->FindKey(keyName) && "GameEvent field not found in passed KeyValues" );

		// see s_GameEnventTypeMap for index
		switch ( type )
		{
			case TYPE_LOCAL : break; // don't network this guy
			case TYPE_STRING: buf->WriteString( event->GetString( keyName, "") ); break;
			case TYPE_FLOAT : buf->WriteFloat( event->GetFloat( keyName, 0.0f) ); break;
			case TYPE_LONG	: buf->WriteLong( event->GetInt( keyName, 0) ); break;
			case TYPE_SHORT	: buf->WriteShort( event->GetInt( keyName, 0) ); break;
			case TYPE_BYTE	: buf->WriteByte( event->GetInt( keyName, 0) ); break;
			case TYPE_BOOL	: buf->WriteOneBit( event->GetInt( keyName, 0) ); break;
			default: DevMsg(1, "CGameEventManager: unkown type %i for key '%s'.\n", type, key->GetName() ); break;
		}

		key = key->GetNextKey();
	}

	return !buf->IsOverflowed();
}

IGameEvent *CGameEventManager::UnserializeEvent( bf_read *buf)
{
	char databuf[MAX_EVENT_BYTES];

	// read event id

	int eventid = buf->ReadUBitLong( MAX_EVENT_BITS );

	// get event description
	CGameEventDescriptor *descriptor = GetEventDescriptor( eventid );

	if ( descriptor == NULL )
	{
		DevMsg( "CGameEventManager::UnserializeEvent:: unknown event id %i.\n", eventid );
		return NULL;
	}

	// create new event
	IGameEvent *event = CreateEvent( descriptor );

	if ( !event )
	{
		DevMsg( "CGameEventManager::UnserializeEvent:: failed to create event %s.\n", descriptor->name );
		return NULL;
	}

	KeyValues * key = descriptor->keys->GetFirstSubKey(); 

	while ( key )
	{
		const char * keyName = key->GetName();

		int type = key->GetInt();

		switch ( type )
		{
			case TYPE_LOCAL		: break; // ignore 
			case TYPE_STRING	: if ( buf->ReadString( databuf, sizeof(databuf) ) )
									event->SetString( keyName, databuf );
								  break;
			case TYPE_FLOAT		: event->SetFloat( keyName, buf->ReadFloat() ); break;
			case TYPE_LONG		: event->SetInt( keyName, buf->ReadLong() ); break;
			case TYPE_SHORT		: event->SetInt( keyName, buf->ReadShort() ); break;
			case TYPE_BYTE		: event->SetInt( keyName, buf->ReadByte() ); break;
			case TYPE_BOOL		: event->SetInt( keyName, buf->ReadOneBit() ); break;
			default: DevMsg(1, "CGameEventManager: unknown type %i for key '%s'.\n", type, key->GetName() ); break;
		}

		key = key->GetNextKey();
	}

	return event;
}

// returns true if this listener is listens to given event
bool CGameEventManager::FindListener( IGameEventListener2 *listener, const char *name )
{
	CGameEventDescriptor *pDescriptor = GetEventDescriptor( name );

	if ( !pDescriptor )
		return false; // event is unknown

	CGameEventCallback *pCallback = FindEventListener( listener );

	if ( !pCallback )
		return false; // listener is unknown

	// see if listener is in the list for this event
	return pDescriptor->listeners.IsValidIndex( pDescriptor->listeners.Find( pCallback ) );
}

CGameEventCallback* CGameEventManager::FindEventListener( void* pCallback )
{
	for (int i=0; i < m_Listeners.Count(); i++ )
	{
		CGameEventCallback *listener = m_Listeners.Element(i);

		if ( listener->m_pCallback == pCallback )
		{
			return listener;
		}
	}

	return NULL;
}

void CGameEventManager::RemoveListener(IGameEventListener2 *listener)
{
	CGameEventCallback *pCallback = FindEventListener( listener );
	
	if ( pCallback == NULL )
	{
		return;
	}

	// remove reference from events 
	for (int i=0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor &et = m_GameEvents.Element( i );
		et.listeners.FindAndRemove( pCallback );
	}

	// and from global list
	m_Listeners.FindAndRemove( pCallback );

	if ( pCallback->m_nListenerType == CLIENTSIDE )
	{
		m_bClientListenersChanged = true;
	}

	delete pCallback;
}

int CGameEventManager::LoadEventsFromFile( const char * filename )
{
	if ( UTL_INVAL_SYMBOL == m_EventFiles.Find( filename ) )
	{
		CUtlSymbol id = m_EventFiles.AddString( filename );
		m_EventFileNames.AddToTail( id );
	}

	KeyValues * key = new KeyValues(filename);
	KeyValues::AutoDelete autodelete_key( key );

	if  ( !key->LoadFromFile( g_pFileSystem, filename, "GAME" ) )
		return false;

	int count = 0;	// number new events

	KeyValues * subkey = key->GetFirstSubKey();

	while ( subkey )
	{
		if ( subkey->GetDataType() == KeyValues::TYPE_NONE )
		{
			RegisterEvent( subkey );
			count++;
		}

		subkey = subkey->GetNextKey();
	}

	if ( net_showevents.GetBool() )
		DevMsg( "Event System loaded %i events from file %s.\n", m_GameEvents.Count(), filename );

	return m_GameEvents.Count();
}

void CGameEventManager::ReloadEventDefinitions()
{
	for ( int i=0; i< m_EventFileNames.Count(); i++ )
	{
		const char *filename = m_EventFiles.String( m_EventFileNames[i] );
		LoadEventsFromFile( filename );
	}

	// we are the server, build string table now
	int number = m_GameEvents.Count();

	for (int j = 0; j<number; j++)
	{
		m_GameEvents[j].eventid = j;
	}
}

bool CGameEventManager::AddListener( IGameEventListener2 *listener, const char *event, bool bServerSide )
{
	if ( !event )
		return false;

	// look for the event descriptor
	CGameEventDescriptor *descriptor = GetEventDescriptor( event );

	if ( !descriptor )
	{
		DevMsg( "CGameEventManager::AddListener: event '%s' unknown.\n", event );
		return false;	// that should not happen
	}

	return AddListener( listener, descriptor, bServerSide ? SERVERSIDE : CLIENTSIDE );
}

bool CGameEventManager::AddListener( void *listener, CGameEventDescriptor *descriptor,  int nListenerType )
{
	if ( !listener || !descriptor )
		return false;	// bahh

	// check if we already know this listener
	CGameEventCallback *pCallback = FindEventListener( listener );

	if ( pCallback == NULL )
	{
		// add new callback 
		pCallback = new CGameEventCallback;
		m_Listeners.AddToTail( pCallback );

		pCallback->m_nListenerType = nListenerType;
		pCallback->m_pCallback = listener;
	}
	else
	{
		// make sure that it hasn't changed:
		Assert( pCallback->m_nListenerType == nListenerType	 );
		Assert( pCallback->m_pCallback == listener );
	}

	// add to event listeners list if not already in there
	if ( descriptor->listeners.Find( pCallback ) == descriptor->listeners.InvalidIndex() )
	{
		descriptor->listeners.AddToTail( pCallback );

		if ( nListenerType == CLIENTSIDE || nListenerType == CLIENTSIDE_OLD )
			m_bClientListenersChanged = true;
	}

	return true;
}


bool CGameEventManager::RegisterEvent( KeyValues * event)
{
	if ( event == NULL )
		return false;

	if ( m_GameEvents.Count() == MAX_EVENT_NUMBER )
	{
		DevMsg( "CGameEventManager: couldn't register event '%s', limit reached (%i).\n",
			event->GetName(), MAX_EVENT_NUMBER );
		return false;
	}

	CGameEventDescriptor *descriptor = GetEventDescriptor( event->GetName() );

	if ( !descriptor )
	{
		// event not known yet, create new one
		int index = m_GameEvents.AddToTail();
		descriptor =  &m_GameEvents.Element(index);

		AssertMsg2( V_strlen( event->GetName() ) <= MAX_EVENT_NAME_LENGTH, "Event named '%s' exceeds maximum name length %d", event->GetName(), MAX_EVENT_NAME_LENGTH );

		Q_strncpy( descriptor->name, event->GetName(), MAX_EVENT_NAME_LENGTH );	
	}
	else
	{
		// descriptor already know, but delete old definitions
		descriptor->keys->deleteThis();
	}

	// create new descriptor keys
	descriptor->keys = new KeyValues("descriptor");
	
	KeyValues *subkey = event->GetFirstSubKey();

	// interate through subkeys

	while ( subkey )
	{
		const char *keyName = subkey->GetName();

		// ok, check it's data type
		const char * type = subkey->GetString();

		if ( !Q_strcmp( "local", keyName) )
		{	
			descriptor->local = Q_atoi( type ) != 0;
		}
		else if ( !Q_strcmp( "reliable", keyName) )
		{
			descriptor->reliable = Q_atoi( type ) != 0;
		}
		else
		{
			int i;

			for (i = TYPE_LOCAL; i <= TYPE_BOOL; i++  )
			{
				if ( !Q_strcmp( type, s_GameEnventTypeMap[i]) )
				{
					// set data type
					descriptor->keys->SetInt( keyName, i );	// set data type
					break;
				}
			}

			if ( i > TYPE_BOOL )
			{
				descriptor->keys->SetInt( keyName, 0 );	// unknown
				DevMsg( "CGameEventManager:: unknown type '%s' for key '%s'.\n", type, subkey->GetName() );
			}
		}
		
		subkey = subkey->GetNextKey();
	}
	
	return true;
}

CGameEventDescriptor *CGameEventManager::GetEventDescriptor(IGameEvent *event)
{
	CGameEvent *gameevent = dynamic_cast<CGameEvent*>(event);

	if ( !gameevent )
		return NULL;

	return gameevent->m_pDescriptor;
}

CGameEventDescriptor *CGameEventManager::GetEventDescriptor(int eventid) // returns event name or NULL
{
	if ( eventid < 0 )
		return NULL;

	for ( int i = 0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor *descriptor = &m_GameEvents[i];

		if ( descriptor->eventid == eventid )
			return descriptor;
	}

	return NULL;
}

void CGameEventManager::FreeEvent( IGameEvent *event )
{
	if ( !event )
		return;

	delete event;
}

CGameEventDescriptor *CGameEventManager::GetEventDescriptor(const char * name)
{
	if ( !name || !name[0] )
		return NULL;

	for (int i=0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor *descriptor = &m_GameEvents[i];

		if ( Q_strcmp( descriptor->name, name ) == 0 )
			return descriptor;
	}

	return NULL;
}

bool CGameEventManager::AddListenerAll( void *listener, int nListenerType )
{
	if ( !listener )
		return false;

	for (int i=0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor *descriptor = &m_GameEvents[i];

		AddListener( listener, descriptor, nListenerType );
	}

	DevMsg("Warning! Game event listener registerd for all events. Use newer game event interface.\n");

	return true;
}

void CGameEventManager::RemoveListenerOld( void *listener)
{
	CGameEventCallback *pCallback = FindEventListener( listener );

	if ( pCallback == NULL )
	{
		DevMsg("RemoveListenerOld: couldn't find listener\n");
		return;
	}

	// remove reference from events 
	for (int i=0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor &et = m_GameEvents.Element( i );
		et.listeners.FindAndRemove( pCallback );
	}

	// and from global list
	m_Listeners.FindAndRemove( pCallback );

	if ( pCallback->m_nListenerType == CLIENTSIDE_OLD )
	{
		m_bClientListenersChanged = true;
	}

	delete pCallback;
}

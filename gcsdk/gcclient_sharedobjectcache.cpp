//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Extra functionality on top of CGCClientSharedObjectCache for GCClients
//
//=============================================================================

#include "stdafx.h"
#include <time.h>
#include "gcsdk/gcclient_sharedobjectcache.h"
#include "gcsdk_gcmessages.pb.h"
#include <typeinfo>

namespace GCSDK
{

//#define SOCDebug(...) Msg( __VA_ARGS__ )
#define SOCDebug(...) ((void)0)

//----------------------------------------------------------------------------
// Purpose: constructor
//----------------------------------------------------------------------------
CGCClientSharedObjectContext::CGCClientSharedObjectContext( const CSteamID & steamIDOwner )
	: m_steamIDOwner( steamIDOwner )
{

}


//----------------------------------------------------------------------------
// Purpose: Adds a new Listener to the cache. All objects in the cache will
//			be sent as create messages to the new Listener
//----------------------------------------------------------------------------
bool CGCClientSharedObjectContext::BAddListener( ISharedObjectListener *pListener )
{
	if( m_vecListeners.HasElement( pListener ) )
		return false;

	m_vecListeners.AddToTail( pListener );
	return true;
}


//----------------------------------------------------------------------------
// Purpose: Removes a Listener from the cache. All objects in the cache
//			will have destroy messages sent for them to the new Listener.
//----------------------------------------------------------------------------
bool CGCClientSharedObjectContext::BRemoveListener( ISharedObjectListener *pListener )
{
	return m_vecListeners.FindAndRemove( pListener );
}


//----------------------------------------------------------------------------
// Purpose: Send created/updated/destroyed calls on to all the listeners in the
//			context
//----------------------------------------------------------------------------
void CGCClientSharedObjectContext::SOCreated( const CSharedObject *pObject, ESOCacheEvent eEvent ) const
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );
	FOR_EACH_VEC( m_vecListeners, nListener )
	{
		m_vecListeners[nListener]->SOCreated( m_steamIDOwner, pObject, eEvent );
	}
}

void CGCClientSharedObjectContext::PreSOUpdate( ESOCacheEvent eEvent ) const
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );
	FOR_EACH_VEC( m_vecListeners, nListener )
	{
		m_vecListeners[nListener]->PreSOUpdate( m_steamIDOwner, eEvent );
	}
}

void CGCClientSharedObjectContext::SOUpdated( const CSharedObject *pObject, ESOCacheEvent eEvent ) const
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );
	FOR_EACH_VEC( m_vecListeners, nListener )
	{
		m_vecListeners[nListener]->SOUpdated( m_steamIDOwner, pObject, eEvent );
	}
}

void CGCClientSharedObjectContext::PostSOUpdate( ESOCacheEvent eEvent ) const
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );
	FOR_EACH_VEC( m_vecListeners, nListener )
	{
		m_vecListeners[nListener]->PostSOUpdate( m_steamIDOwner, eEvent );
	}
}

void CGCClientSharedObjectContext::SODestroyed( const CSharedObject *pObject, ESOCacheEvent eEvent ) const
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );
	FOR_EACH_VEC( m_vecListeners, nListener )
	{
		m_vecListeners[nListener]->SODestroyed( m_steamIDOwner, pObject, eEvent );
	}
}

void CGCClientSharedObjectContext::SOCacheSubscribed( const CSteamID & steamIDOwner, ESOCacheEvent eEvent ) const
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );
	FOR_EACH_VEC( m_vecListeners, nListener )
	{
		m_vecListeners[nListener]->SOCacheSubscribed( steamIDOwner, eEvent );
	}
}

void CGCClientSharedObjectContext::SOCacheUnsubscribed( const CSteamID & steamIDOwner, ESOCacheEvent eEvent ) const
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );
	FOR_EACH_VEC( m_vecListeners, nListener )
	{
		m_vecListeners[nListener]->SOCacheUnsubscribed( steamIDOwner, eEvent );
	}
}


//----------------------------------------------------------------------------
// Purpose: Constructor
//----------------------------------------------------------------------------
CGCClientSharedObjectTypeCache::CGCClientSharedObjectTypeCache( int nTypeID, const CGCClientSharedObjectContext & context )
	: m_context( context ), CSharedObjectTypeCache( nTypeID )
{

}


//----------------------------------------------------------------------------
// Purpose: Destructor
//----------------------------------------------------------------------------
CGCClientSharedObjectTypeCache::~CGCClientSharedObjectTypeCache()
{
}


//----------------------------------------------------------------------------
// Purpose: Parses a cache subscribed message.
//----------------------------------------------------------------------------
bool CGCClientSharedObjectTypeCache::BParseCacheSubscribedMsg( const CMsgSOCacheSubscribed_SubscribedType & msg, CUtlVector<CSharedObject*> &vecCreatedObjects, CUtlVector<CSharedObject*> &vecUpdatedObjects, CUtlVector<CSharedObject*> &vecObjectsToDestroy )
{
	CSharedObjectVec vecUntouchedObjects;
	for ( uint32 i = 0; i < GetCount(); i++ )
	{
		vecUntouchedObjects.AddToTail( GetObject( i ) );
	}

	for( uint16 usObject = 0; usObject < msg.object_data_size(); usObject++ )
	{
		bool bUpdatedExisting = false;
		CSharedObject *pObject = BCreateFromMsg( msg.object_data( usObject ).data(), msg.object_data( usObject ).size(), &bUpdatedExisting );
		if ( pObject == NULL)
		{
			Assert( pObject );
			return false;
		}

		// if an object was updated, remove it from the untouched list
		if ( bUpdatedExisting )
		{
			int index = vecUntouchedObjects.Find( pObject );
			if ( index != vecUntouchedObjects.InvalidIndex() )
			{
				vecUntouchedObjects[index] = NULL;
			}
			vecUpdatedObjects.AddToTail( pObject );
		}
		else
		{
			vecCreatedObjects.AddToTail( pObject );
		}
	}

	// all objects that weren't in the SubscribedMsg should be destroyed
	for ( int i = 0; i < vecUntouchedObjects.Count(); i++ )
	{
		if ( vecUntouchedObjects[i] == NULL )
			continue;

		CSharedObject *pObject = RemoveObject( *vecUntouchedObjects[i] );
		Assert( pObject );
		if( pObject )
			vecObjectsToDestroy.AddToTail( pObject );
	}

	return true;
}

void CGCClientSharedObjectTypeCache::RemoveAllObjects( CUtlVector<CSharedObject*> &vecObjects )
{

	// Go in reverse order to avoid O(n^2) shifting the items in the array
	for ( int i = GetCount() - 1; i >= 0; i-- )
	{
		CSharedObject *pObject = RemoveObjectByIndex( i );
		Assert( pObject );
		if ( pObject )
			vecObjects.AddToTail( pObject );
	}
}


//----------------------------------------------------------------------------
// Purpose: Processes a received create message for an object of this type on
//			the client/gameserver
//----------------------------------------------------------------------------
CSharedObject *CGCClientSharedObjectTypeCache::BCreateFromMsg( const void *pvData, uint32 unSize, bool *bUpdatedExisting )
{
	CUtlBuffer bufCreate( pvData, unSize, CUtlBuffer::READ_ONLY );
	CSharedObject *pNewObj = CSharedObject::Create( GetTypeID() );
	Assert( pNewObj );
	if( !pNewObj )
	{
		EmitError( SPEW_SHAREDOBJ, "Unable to create object of type %d\n", GetTypeID() );
		return NULL;
	}

	if( !pNewObj->BParseFromMessage( bufCreate ) )
	{
		delete pNewObj;
		return NULL;
	}

	// Existing object?
	CSharedObject *pObj = FindSharedObject( *pNewObj );
	if( pObj )
	{
		pObj->Copy( *pNewObj );
		delete pNewObj;
		if ( bUpdatedExisting )
		{
			*bUpdatedExisting = true;
		}
		return pObj;
	}

	// New object
	AddObject( pNewObj );
	if ( bUpdatedExisting )
	{
		*bUpdatedExisting = false;
	}
	return pNewObj;
}


//----------------------------------------------------------------------------
// Purpose: Processes a received destroy message for an object of this type on
//			the client/gameserver
//----------------------------------------------------------------------------
bool CGCClientSharedObjectTypeCache::BDestroyFromMsg( const void *pvData, uint32 unSize )
{
	CUtlBuffer bufDestroy( pvData, unSize, CUtlBuffer::READ_ONLY );
	CSharedObject *pIndexObj = CSharedObject::Create( GetTypeID() );
	if( !pIndexObj->BParseFromMessage( bufDestroy ) )
	{
		delete pIndexObj;
		return false;
	}

	CSharedObject *pObject = RemoveObject( *pIndexObj );
	if( pObject )
	{
		m_context.SODestroyed( pObject, eSOCacheEvent_Incremental );
		delete pObject;
	}

	delete pIndexObj;
	return true;
}


//----------------------------------------------------------------------------
// Purpose: Processes a received destroy message for an object of this type on
//			the client/gameserver
//----------------------------------------------------------------------------
bool CGCClientSharedObjectTypeCache::BUpdateFromMsg( const void *pvData, uint32 unSize )
{
	CUtlBuffer bufUpdate( pvData, unSize, CUtlBuffer::READ_ONLY );
	CSharedObject *pIndexObj = CSharedObject::Create( GetTypeID() );
	AssertMsg1( pIndexObj, "Unable to create index object of type %d", GetTypeID() );
	if( !pIndexObj )
		return false;
	if( !pIndexObj->BParseFromMessage( bufUpdate ) )
	{
		delete pIndexObj;
		return false;
	}

	CSharedObject *pObj = FindSharedObject( *pIndexObj );
	bool bRet = false;
	if( pObj )
	{
		bufUpdate.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );

		bRet = pObj->BUpdateFromNetwork( *pIndexObj );
		m_context.SOUpdated( pObj, eSOCacheEvent_Incremental );
	}

	delete pIndexObj;
	return bRet;
}


//----------------------------------------------------------------------------
// Purpose: Constructor
//----------------------------------------------------------------------------
CGCClientSharedObjectCache::CGCClientSharedObjectCache( const CSteamID & steamIDOwner ) 
	: m_context( steamIDOwner ),
	m_bInitialized( false ),
	m_bSubscribed( false )
{

}


//----------------------------------------------------------------------------
// Purpose: Destructor
//----------------------------------------------------------------------------
CGCClientSharedObjectCache::~CGCClientSharedObjectCache()
{
}


//----------------------------------------------------------------------------
// Purpose: Process an incoming create message on a client/gameserver.
//----------------------------------------------------------------------------
bool CGCClientSharedObjectCache::BParseCacheSubscribedMsg( const CMsgSOCacheSubscribed & msg )
{

	// Assume all type caches will be untouched
	CUtlVector<int> vecUntouchedTypes;
	for ( int i = FirstTypeCacheIndex(); i != InvalidTypeCacheIndex(); i = NextTypeCacheIndex( i ) )
	{
		CSharedObjectTypeCache *pTypeCache = GetTypeCacheByIndex( i );
		if ( pTypeCache )
		{
			vecUntouchedTypes.AddToTail( pTypeCache->GetTypeID() );
		}
	}

	// List of objects created, updated, and removed
	CUtlVector<CSharedObject*> vecCreatedObjects;
	CUtlVector<CSharedObject*> vecUpdatedObjects;
	CUtlVector<CSharedObject*> vecObjectsToDestroy;

	bool bResult = true;

	// Scan types in message
	for( uint16 usObject = 0; usObject < msg.objects_size(); usObject++ )
	{
		const CMsgSOCacheSubscribed_SubscribedType & msgType = msg.objects( usObject );

		// Find or create the type
		CGCClientSharedObjectTypeCache *pTypeCache = CreateTypeCache( msgType.type_id() );
		if ( pTypeCache )
		{
			int index = vecUntouchedTypes.Find( pTypeCache->GetTypeID() );
			if ( index != vecUntouchedTypes.InvalidIndex() )
			{
				vecUntouchedTypes[index] = -1;
			}
		}
		Assert( pTypeCache );
		if( !pTypeCache || !pTypeCache->BParseCacheSubscribedMsg( msgType, vecCreatedObjects, vecUpdatedObjects, vecObjectsToDestroy ) )
			bResult = false;
	}

	// any type caches that weren't in the SubscribedMsg should be cleared
	for ( int i = FirstTypeCacheIndex(); i != InvalidTypeCacheIndex(); i = NextTypeCacheIndex( i ) )
	{
		CGCClientSharedObjectTypeCache *pTypeCache = GetTypeCacheByIndex( i );
		if ( vecUntouchedTypes.Find( pTypeCache->GetTypeID() ) != vecUntouchedTypes.InvalidIndex() )
		{
			pTypeCache->RemoveAllObjects( vecObjectsToDestroy );
		}
	}

	// Which event is happening?
	ESOCacheEvent eNotificationEvent = eSOCacheEvent_Subscribed;
	if ( m_bSubscribed )
		eNotificationEvent = eSOCacheEvent_Resubscribed;

	// Set version, assuming we didn't have any problems.  If we hit any problems,
	// we want to force a refresh
	if ( bResult )
		SetVersion( msg.version() );

	// Mark that the cache has been initialized by the server
	m_bInitialized = true;
	m_bSubscribed = true;

	//
	// Send notifications
	//

	// Initial cache subscribed
	m_context.SOCacheSubscribed( GetOwner(), eNotificationEvent );

	// Deletions
	for ( int i = 0 ; i < vecObjectsToDestroy.Count() ; ++i )
	{
		m_context.SODestroyed( vecObjectsToDestroy[i], eNotificationEvent );
		delete vecObjectsToDestroy[i];
	}

	// Updates
	for ( int i = 0 ; i < vecUpdatedObjects.Count() ; ++i )
	{
		m_context.SOUpdated( vecUpdatedObjects[i], eNotificationEvent );
	}

	// Created
	for ( int i = 0 ; i < vecCreatedObjects.Count() ; ++i )
	{
		m_context.SOUpdated( vecCreatedObjects[i], eNotificationEvent );
	}

	// Return true if everything parsed OK, or false
	// if we had at least one failure
	return bResult;
}


//----------------------------------------------------------------------------
// Purpose: Process an incoming create message on a client/gameserver.
//----------------------------------------------------------------------------
void CGCClientSharedObjectCache::NotifyUnsubscribe()
{
	if ( m_bSubscribed )
	{
		m_bSubscribed = false;
		m_context.SOCacheUnsubscribed( GetOwner(), eSOCacheEvent_Unsubscribed );
	}
	else
	{
		AssertMsg( m_bSubscribed, "GC Sending us Unsubscribed message when we weren't subscribed" ); // Might not be a bug, but something worth checking
	}
}

//----------------------------------------------------------------------------
// Purpose: GC is telling us that the version we have is up-to-date,a nd we are subscribed
//----------------------------------------------------------------------------
void CGCClientSharedObjectCache::NotifyResubscribedUpToDate()
{
	if ( !m_bSubscribed )
	{
		Assert( m_bInitialized );
		m_bSubscribed = true;
		m_context.SOCacheSubscribed( GetOwner(), eSOCacheEvent_Subscribed );
	}
	else
	{
		AssertMsg( m_bSubscribed, "Got NotifyResubscribedUpToDate when we were already subscribed?" ); // Might not be a bug, but something worth checking
	}
}


//----------------------------------------------------------------------------
// Purpose: Process an incoming create message on a client/gameserver.
//----------------------------------------------------------------------------
bool CGCClientSharedObjectCache::BCreateFromMsg( int nTypeID, const void *pvData, uint32 unSize )
{
	// We should be subscribed
	if ( !m_bInitialized || !m_bSubscribed )
	{
		// Note: We can go down and come back up without the GC knowing this.
		// So this can happen
		//Assert( m_bInitialized );
		//Assert( m_bSubscribed );
		//EmitWarning( SPEW_SHAREDOBJ, 1, "Received SOCache incremental update for cache we were not subscribed to (object type %d)\n", nTypeID );
	}

	// Locate / create the type cache
	CGCClientSharedObjectTypeCache *pTypeCache = CreateTypeCache( nTypeID );

	// Create the message or update existing
	bool bUpdatedExisting = false;
	CSharedObject *pObject = pTypeCache->BCreateFromMsg( pvData, unSize, &bUpdatedExisting );
	if ( pObject == NULL )
		return false;

	// Send notifications to listeners
	if ( bUpdatedExisting )
	{
		// This can happen --- see comment at the top of this function
		//Assert( !bUpdatedExisting ); // shouldn't the GC know what it's already sent us?  This is weird
		m_context.SOUpdated( pObject, eSOCacheEvent_Incremental );
	}
	else
	{
		m_context.SOCreated( pObject, eSOCacheEvent_Incremental );
	}

	return true;
}


//----------------------------------------------------------------------------
// Purpose: Processes an incoming destroy message on a client/gameserver.
//----------------------------------------------------------------------------
bool CGCClientSharedObjectCache::BDestroyFromMsg( int nTypeID, const void *pvData, uint32 unSize )
{
	CGCClientSharedObjectTypeCache *pTypeCache = FindTypeCache( nTypeID );
	if( pTypeCache )
	{
		return pTypeCache->BDestroyFromMsg( pvData, unSize );
	}
	else
	{
		return false;
	}	
}

//----------------------------------------------------------------------------
// Purpose: Processes an incoming update message on a client/gameserver.
//----------------------------------------------------------------------------
bool CGCClientSharedObjectCache::BUpdateFromMsg( int nTypeID, const void *pvData, uint32 unSize )
{
	CGCClientSharedObjectTypeCache *pTypeCache = FindTypeCache( nTypeID );
	if( pTypeCache )
	{
		return pTypeCache->BUpdateFromMsg( pvData, unSize );
	}
	else
	{
		return false;
	}	
}

//----------------------------------------------------------------------------
// Purpose: Adds a listener object to be notified of object changes in this 
//			cache. The shared object cache does not own this object and will
//			not free it.
//----------------------------------------------------------------------------
void CGCClientSharedObjectCache::AddListener( ISharedObjectListener *pListener )
{
	Assert( pListener );
	if ( !m_context.BAddListener( pListener ) )
		return; // was already listening, no action needed

	SOCDebug( "[%s] Adding listener %s\n", GetOwner().Render(), typeid( *pListener ).name() );

	// If we're already subscribed, then immediately send notifications
	if( BIsSubscribed() )
	{
		pListener->SOCacheSubscribed( GetOwner(), eSOCacheEvent_ListenerAdded );
		for ( int i = FirstTypeCacheIndex(); i != InvalidTypeCacheIndex(); i = NextTypeCacheIndex( i ) )
		{
			CGCClientSharedObjectTypeCache *pTypeCache = GetTypeCacheByIndex( i );
			for ( uint32 j = 0 ; j < pTypeCache->GetCount() ; ++j )
			{
				CSharedObject *pObject = pTypeCache->GetObject( j );
				pListener->SOCreated( GetOwner(), pObject, eSOCacheEvent_ListenerAdded );
			}
		}
	}
}


//----------------------------------------------------------------------------
// Purpose: Removes a listener object from the list to be notified of changes
//			to this object cache.
//----------------------------------------------------------------------------
bool CGCClientSharedObjectCache::RemoveListener( ISharedObjectListener *pListener )
{
	Assert( pListener );
	if ( !m_context.BRemoveListener( pListener ) )
		return false; // wasn't already listening, nothing to do

	SOCDebug( "[%s] Removing listener %s\n", GetOwner().Render(), typeid( *pListener ).name() );

	// If we were subscribed, then the listener's last subscribe notification
	// was a "you are subscribed."  Send him an unsubscribed notification
	// so he doesn't think he's still subscribed.
	if( BIsSubscribed() )
	{
		pListener->SOCacheUnsubscribed( GetOwner(), eSOCacheEvent_ListenerRemoved );
	}

	return true;
}

}  // namespace GCSDK

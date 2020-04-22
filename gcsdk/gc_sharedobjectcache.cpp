//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Extra GC-specific code on top of CGCSharedObjectCache
//
//=============================================================================

#include "stdafx.h"
#include <time.h>
#include "gcsdk_gcmessages.pb.h"

#include "tier0/memdbgoff.h"

namespace GCSDK
{

//----------------------------------------------------------------------------
// Purpose: Container for the cached message, so we can track how many of these 
// things there are
//----------------------------------------------------------------------------
class CCachedSubscriptionMessage
{
	DECLARE_CLASS_MEMPOOL( CCachedSubscriptionMessage );
public:

	CCachedSubscriptionMessage()
		: message( k_ESOMsg_CacheSubscribed )
	{

	}

	~CCachedSubscriptionMessage()
	{

	}

	CProtoBufMsg<CMsgSOCacheSubscribed> message;

};

IMPLEMENT_CLASS_MEMPOOL( CCachedSubscriptionMessage, 10 * 1000, UTLMEMORYPOOL_GROW_SLOW );
}

#include "tier0/memdbgon.h" // needs to be the last include in the file

namespace GCSDK
{

//----------------------------------------------------------------------------
// Purpose: Default filter object  for subscriber messages whi
//----------------------------------------------------------------------------
class CDefaultSubscriberMessageFilter : public CISubscriberMessageFilter
{
public:
	CDefaultSubscriberMessageFilter()
	{

	}

	virtual bool BShouldSendAnyObjectsInCache( CGCSharedObjectTypeCache *pTypeCache, uint32 unFlags ) const
	{
		return ( CSharedObject::GetTypeFlags( pTypeCache->GetTypeID() ) & unFlags ) != 0;
	}

	virtual bool BShouldSendObject( CSharedObject *pSharedObject, uint32 unFlags ) const
	{
		return ( CSharedObject::GetTypeFlags( pSharedObject->GetTypeID() ) & unFlags ) != 0;
	}
};

//----------------------------------------------------------------------------
// Purpose: constructor
//----------------------------------------------------------------------------
CSharedObjectContext::CSharedObjectContext( const CSteamID & steamIDOwner )
	: m_steamIDOwner( steamIDOwner )
{
}

//----------------------------------------------------------------------------
// Purpose: Finds the steam ID within the vector
//----------------------------------------------------------------------------
int CSharedObjectContext::FindSubscriber( const CSteamID& steamID ) const
{
	FOR_EACH_VEC( m_vecSubscribers, nSubscriber )
	{
		if( steamID == m_vecSubscribers[ nSubscriber ].m_steamID )
			return nSubscriber;
	}
	return m_vecSubscribers.InvalidIndex();
}

//----------------------------------------------------------------------------
// Purpose: Adds a new subscriber to the cache. All objects in the cache will
//			be sent as create messages to the new subscriber
//			Returns false if it simply incremented the refcount of the subscribers of this cache
//----------------------------------------------------------------------------
bool CSharedObjectContext::BAddSubscriber( const CSteamID & steamID )
{
	MEM_ALLOC_CREDIT_CLASS();
	int iIdx = FindSubscriber( steamID );
	if ( iIdx != m_vecSubscribers.InvalidIndex() )
	{
		m_vecSubscribers[ iIdx ].m_nRefCount++;
		return false;
	}

	Subscriber_t Subscriber;
	Subscriber.m_steamID = steamID;
	Subscriber.m_nRefCount = 1;
	m_vecSubscribers.AddToTail( Subscriber );
	return true;
}


//----------------------------------------------------------------------------
// Purpose: Removes a subscriber from the cache. All objects in the cache
//			will have destroy messages sent for them to the new subscriber.
//----------------------------------------------------------------------------
bool CSharedObjectContext::BRemoveSubscriber( const CSteamID & steamID )
{
	int iIdx = FindSubscriber( steamID );
	if ( iIdx != m_vecSubscribers.InvalidIndex() )
	{
		m_vecSubscribers[ iIdx ].m_nRefCount--;

		if ( m_vecSubscribers[ iIdx ].m_nRefCount == 0 )
		{
			m_vecSubscribers.FastRemove( iIdx );
			return true;
		}
	}

	return false;
}


//----------------------------------------------------------------------------
// Purpose: Removes all subscribers from the context
//----------------------------------------------------------------------------
void CSharedObjectContext::RemoveAllSubscribers()
{
	m_vecSubscribers.RemoveAll();
}


//----------------------------------------------------------------------------
// Purpose: Constructor
//----------------------------------------------------------------------------
CGCSharedObjectTypeCache::CGCSharedObjectTypeCache( int nTypeID, const CSharedObjectContext & context )
	: m_context( context ), CSharedObjectTypeCache( nTypeID )
{

}


//----------------------------------------------------------------------------
// Purpose: Destructor
//----------------------------------------------------------------------------
CGCSharedObjectTypeCache::~CGCSharedObjectTypeCache()
{
	g_SharedObjectStats.TrackSharedObjectLifetime( GetTypeID(), -( ( int32 )GetCount() ) );
}

//----------------------------------------------------------------------------
// Purpose: Adds a shared object of the appropriate type to this type cache.
//----------------------------------------------------------------------------
void CGCSharedObjectTypeCache::EnsureCapacity( uint32 nItems )
{
	MEM_ALLOC_CREDIT_CLASS();
	Base::EnsureCapacity( nItems );
}


//----------------------------------------------------------------------------
// Purpose: Adds a shared object of the appropriate type to this type cache.
//----------------------------------------------------------------------------
bool CGCSharedObjectTypeCache::AddObject( CSharedObject *pObject )
{
	MEM_ALLOC_CREDIT_CLASS();
	Assert( GetTypeID() == pObject->GetTypeID() );
	if( CSharedObjectTypeCache::AddObject( pObject ) )
	{
		g_SharedObjectStats.TrackSharedObjectLifetime( GetTypeID(), 1 );
		return true;
	}

	return false;
}


//----------------------------------------------------------------------------
// Purpose: Destroys the object matching the one passed in. This could be the
//			same one or simply one with matching index fields.
//----------------------------------------------------------------------------
CSharedObject *CGCSharedObjectTypeCache::RemoveObject( const CSharedObject & soIndex )
{
	CSharedObject *pObj = CSharedObjectTypeCache::RemoveObject( soIndex );
	if( pObj )
	{
		g_SharedObjectStats.TrackSharedObjectLifetime( GetTypeID(), -1 );
	}

	return pObj;
}


//----------------------------------------------------------------------------
// Purpose: Populates a message with the data to create every object of this
//			type in the cache.
//----------------------------------------------------------------------------
void CGCSharedObjectTypeCache::BuildCacheSubscribedMsg( CMsgSOCacheSubscribed_SubscribedType *pMsgType, uint32 unFlags, const CISubscriberMessageFilter &filter )
{
	//MEM_ALLOC_CREDIT_( CFmtStr( "StartPlaying - Type BuildMsg - %s", CSharedObject::PchClassName( GetTypeID() ) ) );
	VPROF_BUDGET( CSharedObject::PchClassBuildCacheNodeName( GetTypeID() ), VPROF_BUDGETGROUP_STEAM );
	
	uint32 nNumSubscribed = 0;
	for( uint32 i=0; i<GetCount(); i++ )
	{
		CSharedObject *pObj = GetObject( i );

		if ( !filter.BShouldSendObject( pObj, unFlags ) )
			continue;

		if ( !pObj->BAddToMessage( pMsgType->add_object_data() ) )
		{
			EmitWarning( SPEW_SHAREDOBJ, SPEW_ALWAYS, "Failed to add create fields to message in create" );
		}
		else
		{
			nNumSubscribed++;
		}
	}

	//track this send
	g_SharedObjectStats.TrackSubscription( GetTypeID(), unFlags, nNumSubscribed );
}


//----------------------------------------------------------------------------
// Purpose: Constructor
//----------------------------------------------------------------------------
CGCSharedObjectCache::CGCSharedObjectCache( const CSteamID & steamIDOwner ) 
	: m_context( steamIDOwner ),
	m_bInWriteback( false ),
	m_unWritebackTime( 0 ),
	m_unLRUHandle( CUtlLinkedList< CSteamID, uint32 >::InvalidIndex() ),
	m_unCachedSubscriptionMsgFlags( 0 ),
	m_pCachedSubscriptionMsg( NULL ),
	m_bDetectVersionChanges( true )
#if WITH_SOCACHE_LRU_DEBUGGING
,	m_unDebugTag( 0 )
#endif // WITH_SOCACHE_LRU_DEBUGGING
{

}


//----------------------------------------------------------------------------
// Purpose: Destructor
//----------------------------------------------------------------------------
CGCSharedObjectCache::~CGCSharedObjectCache()
{
	delete m_pCachedSubscriptionMsg;
}


const CISubscriberMessageFilter &CGCSharedObjectCache::GetSubscriberMessageFilter()
{
	static CDefaultSubscriberMessageFilter sFilter;
	return sFilter;	
}

//----------------------------------------------------------------------------
// Purpose: Adds a shared object to the cache.
//----------------------------------------------------------------------------
bool CGCSharedObjectCache::AddObject( CSharedObject *pSharedObject )
{
	Assert( pSharedObject );
	if ( !GGCBase()->BIsSOCacheBeingLoaded( GetOwner() ) )
	{
		AssertMsg( GGCBase()->IsSteamIDLockedByCurJob( GetOwner() ), "Attempt to add an object to an unlocked SO cache!" );
	}

	if ( !CSharedObjectCache::AddObject( pSharedObject ) )
		return false;

	uint32 unTypeFlags = CSharedObject::GetTypeFlags( pSharedObject->GetTypeID() );
	if( BShouldSendToAnyClients( unTypeFlags ) )
	{
		DirtyNetworkObjectCreate( pSharedObject );
	}

	return true;
}

//----------------------------------------------------------------------------
// Purpose: Adds a shared object to the cache without dirtying the object or the cache
//----------------------------------------------------------------------------
bool CGCSharedObjectCache::AddObjectClean( CSharedObject *pSharedObject )
{
	CSharedObjectTypeCache *pTypeCache = CreateBaseTypeCache( pSharedObject->GetTypeID() );
	return pTypeCache->AddObjectClean( pSharedObject );
}

//----------------------------------------------------------------------------
// Purpose: Removes the object matching the one passed in from this cache, 
//          without destroying the actual object.
//----------------------------------------------------------------------------
CSharedObject *CGCSharedObjectCache::RemoveObject( const CSharedObject & soIndex )
{
	AssertMsg( GGCBase()->IsSteamIDLockedByCurJob( GetOwner() ), "Attempt to remove an object from an unlocked SO cache!" );

	CSharedObject *pObject = CSharedObjectCache::RemoveObject( soIndex );

	if ( pObject )
	{
		uint32 unTypeFlags = CSharedObject::GetTypeFlags( pObject->GetTypeID() );
		if( BShouldSendToAnyClients( unTypeFlags ) )
		{
			const CISubscriberMessageFilter &filter = GetSubscriberMessageFilter();
			const CUtlVector< CSharedObjectContext::Subscriber_t > &vecSubscribers = m_context.GetSubscribers();

			CUtlVector<CSteamID> vecRecipients( 0, vecSubscribers.Count() );
			FOR_EACH_VEC( vecSubscribers, nSub )
			{
				const CSteamID & steamID = vecSubscribers[nSub].m_steamID;
				uint32 unFlags = CalcSendFlags( steamID );
				if( filter.BShouldSendObject( pObject, unFlags ) )
				{
					VPROF_BUDGET( CSharedObject::PchClassCreateNodeName( pObject->GetTypeID() ), VPROF_BUDGETGROUP_STEAM );				
					vecRecipients.AddToTail( vecSubscribers[nSub].m_steamID );
				}
			}

			if ( vecRecipients.Count() > 0 )
			{
				VPROF_BUDGET( CSharedObject::PchClassCreateNodeName( pObject->GetTypeID() ), VPROF_BUDGETGROUP_STEAM );
#if 0
				// Kyle says: KFIXME: this is the more current way of doing things but we're missing some multi-broadcast
				//					  code that hasn't been integrated from Dota yet.
				pObject->BSendDestroyToSteamIDs( vecRecipients, m_context.GetOwner(), GetVersion() );
#else
				FOR_EACH_VEC( vecRecipients, i )
				{
					pObject->BSendDestroyToSteamID( vecRecipients[i], m_context.GetOwner(), GetVersion() );
				}
#endif
				g_SharedObjectStats.TrackSharedObjectSendDestroy( pObject->GetTypeID(), vecRecipients.Count() );
			}
		}

		m_networkDirtyObjs.Remove( pObject );
		if ( 0 == m_networkDirtyObjs.Count() )
		{
			m_networkDirtyObjs.Purge();
		}

		m_networkDirtyObjsCreate.Remove( pObject );
		if ( 0 == m_networkDirtyObjsCreate.Count() )
		{
			m_networkDirtyObjsCreate.Purge();
		}

		m_databaseDirtyList.FindAndRemove( pObject );
	}

	return pObject;
}


//----------------------------------------------------------------------------
// Purpose: Destroys the object matching the one passed in. This could be the
//			same one or simply one with matching index fields.
//----------------------------------------------------------------------------
bool CGCSharedObjectCache::BDestroyObject( const CSharedObject & soIndex, bool bRemoveFromDatabase )
{
	CSharedObject *pObject = RemoveObject( soIndex );

	if ( !pObject )
		return false;

	if ( bRemoveFromDatabase )
	{
		pObject->BYieldingRemoveFromDatabase();
	}

	delete pObject;
	return true;
}

//----------------------------------------------------------------------------
// Purpose: Adds a new subscriber to the cache. All objects in the cache will
//			be sent as create messages to the new subscriber
//----------------------------------------------------------------------------
void CGCSharedObjectCache::SetTradingPartner( const CSteamID &steamID )
{
	if ( m_steamIDTradingPartner.IsValid() )
	{
		RemoveSubscriber( m_steamIDTradingPartner );
	}
	m_steamIDTradingPartner = steamID;
	if ( steamID.IsValid() )
	{
		AddSubscriber( steamID );
	}	
}

//----------------------------------------------------------------------------
// Purpose: Adds a new subscriber to the cache. All objects in the cache will
//			be sent as create messages to the new subscriber
//----------------------------------------------------------------------------
void CGCSharedObjectCache::AddSubscriber( const CSteamID & steamID, bool bForceSendSubscriptionMsg )
{
	MEM_ALLOC_CREDIT_("StartPlaying - AddSubscriber" );
	VPROF_BUDGET( "CGCSharedObjectCache::AddSubscriber()", VPROF_BUDGETGROUP_STEAM );

	Assert( steamID.IsValid() );
	bool bAdded = m_context.BAddSubscriber( steamID );
	if ( bAdded || bForceSendSubscriptionMsg )
	{
		// ask the client if it really needs the contents of this cache, because we could potentially send a lot of stuff
		CProtoBufMsg<CMsgSOCacheSubscriptionCheck> msg( k_ESOMsg_CacheSubscriptionCheck );
		msg.Body().set_owner( GetOwner().ConvertToUint64() );
		msg.Body().set_version( GetVersion() );
		GGCBase()->BSendGCMsgToClient( steamID, msg );
	}
}

//----------------------------------------------------------------------------
// Purpose: 
//----------------------------------------------------------------------------
void CGCSharedObjectCache::SendSubscriberMessage( const CSteamID & steamID )
{
	Assert( steamID.IsValid() );
	if ( !m_context.BIsSubscribed( steamID ) )
		return;

	uint32 unFlags = CalcSendFlags( steamID );

	if ( unFlags == k_ESOFlag_SendToNobody )
		return;

	// cache off the *last* one
	if ( unFlags != m_unCachedSubscriptionMsgFlags )
	{
		ClearCachedSubscriptionMessage();
		m_pCachedSubscriptionMsg = BuildSubscriberMessage( unFlags );
		m_unCachedSubscriptionMsgFlags = unFlags;
	}
	GGCBase()->BSendGCMsgToClient( steamID, m_pCachedSubscriptionMsg->message );
}

//----------------------------------------------------------------------------
// Purpose: 
//----------------------------------------------------------------------------
void CGCSharedObjectCache::ClearCachedSubscriptionMessage()
{
	delete m_pCachedSubscriptionMsg;
	m_pCachedSubscriptionMsg = NULL;
	m_unCachedSubscriptionMsgFlags = 0;
}

//----------------------------------------------------------------------------
// Purpose: Builds a subscriber message that can be sent to multiple clients
//----------------------------------------------------------------------------
CCachedSubscriptionMessage * CGCSharedObjectCache::BuildSubscriberMessage( uint32 unFlags )
{
	MEM_ALLOC_CREDIT_( "BuildSubscriberMessage" );
	CCachedSubscriptionMessage *pCachedMsg = new CCachedSubscriptionMessage;

	CProtoBufMsg<CMsgSOCacheSubscribed> &msg = pCachedMsg->message;
	msg.Body().set_owner( GetOwner().ConvertToUint64() );
	msg.Body().set_version( GetVersion() );

	const CISubscriberMessageFilter &filter = GetSubscriberMessageFilter();

	for( int nTypeIndex = 0; nTypeIndex < GetTypeCacheCount(); nTypeIndex++ )
	{
		CGCSharedObjectTypeCache *pTypeCache = (CGCSharedObjectTypeCache *)GetTypeCacheByIndex( nTypeIndex );
		if(  !pTypeCache || !pTypeCache->GetCount() )
			continue;

		if ( !filter.BShouldSendAnyObjectsInCache( pTypeCache, unFlags ) )
			continue;

		CMsgSOCacheSubscribed_SubscribedType *pMsgType = msg.Body().add_objects();
		pMsgType->set_type_id( pTypeCache->GetTypeID() );
		pTypeCache->BuildCacheSubscribedMsg( pMsgType, unFlags, filter );
	}

	return pCachedMsg;
}


//----------------------------------------------------------------------------
// Purpose: Sends a message to the client to unsubscribe from this cache
//----------------------------------------------------------------------------
void CGCSharedObjectCache::SendUnsubscribeMessage( const CSteamID & steamID )
{

	// Don't send these while shutting down.  We'll use higher-level
	// connection-oriented messages instead
	if ( GGCBase()->GetIsShuttingDown() )
		return;

	CProtoBufMsg< CMsgSOCacheUnsubscribed > msg( k_ESOMsg_CacheUnsubscribed );
	msg.Body().set_owner( GetOwner().ConvertToUint64() );
	GGCBase()->BSendGCMsgToClient( steamID, msg );
}


//----------------------------------------------------------------------------
// Purpose: Removes a subscriber from the cache. All objects in the cache
//			will have destroy messages sent for them to the new subscriber.
//----------------------------------------------------------------------------
void CGCSharedObjectCache::RemoveSubscriber( const CSteamID & steamID )
{
	MEM_ALLOC_CREDIT_("CGCSharedObjectCache::RemoveSubscriber" );
	Assert( steamID.IsValid() );
	if( m_context.BRemoveSubscriber( steamID ) )
	{
		SendUnsubscribeMessage( steamID );
	}
}


//----------------------------------------------------------------------------
// Purpose: Removes all subscribers from the cache.
//----------------------------------------------------------------------------
void CGCSharedObjectCache::RemoveAllSubscribers()
{
	const CUtlVector< CSharedObjectContext::Subscriber_t > &vecSubscribers = m_context.GetSubscribers();
	FOR_EACH_VEC( vecSubscribers, i )
	{
		SendUnsubscribeMessage( vecSubscribers[i].m_steamID );
	}

	m_context.RemoveAllSubscribers();
}

//----------------------------------------------------------------------------
// Purpose: Check to see if a given shared object is in this SO cache.
//----------------------------------------------------------------------------
bool CGCSharedObjectCache::IsObjectCached( const CSharedObject *pObj, uint32 nTypeID ) const
{
	// pObj->GetTypeID() isn't called because the method is virtual,
	// and this code gets called from a destructor.
	CSharedObjectTypeCache *pTypeCache = FindBaseTypeCache( nTypeID );
	if ( pTypeCache == NULL )
	{
		return false;
	}
	for ( uint i = 0; i < pTypeCache->GetCount(); i++ )
	{
		if ( pTypeCache->GetObject( i ) == pObj )
		{
			return true;
		}
	}
	return false;
}

//----------------------------------------------------------------------------
// Purpose: Check to see if a given shared object this SO cache's dirty lists.
//----------------------------------------------------------------------------
bool CGCSharedObjectCache::IsObjectDirty( const CSharedObject *pConstObj ) const
{
	//the list types aren't really all that const correct
	CSharedObject* pObj = const_cast< CSharedObject* >( pConstObj );
	return m_databaseDirtyList.HasElement( pObj )
		|| m_networkDirtyObjsCreate.HasElement( pObj )
		|| m_networkDirtyObjs.HasElement( pObj );
}

//----------------------------------------------------------------------------
// Purpose: Remembers that this field and this object are dirty
//----------------------------------------------------------------------------
void CGCSharedObjectCache::DirtyNetworkObject( CSharedObject *pObj )
{
#ifdef DEBUG
	// Make sure the object is in the shared object cache.
	Assert( IsObjectCached( pObj, pObj->GetTypeID() ) );
#endif

	MarkDirty();

	// add it to our dirty list if it wasn't already dirty
	// if its the create queue, don't put it in the update queue
	UtlHashHandle_t netHandleCreate = m_networkDirtyObjsCreate.Find( pObj );
	if ( netHandleCreate == m_networkDirtyObjsCreate.InvalidHandle() )
	{
		m_networkDirtyObjs.Insert( pObj );
	}
}

//----------------------------------------------------------------------------
// Purpose: Remembers that this field and this object are dirty
//----------------------------------------------------------------------------
void CGCSharedObjectCache::DirtyNetworkObjectCreate( CSharedObject *pObj )
{
#ifdef DEBUG
	// Make sure the object is in the shared object cache.
	Assert( IsObjectCached( pObj, pObj->GetTypeID() ) );
#endif

	MarkDirty();
	// add it to our dirty list if it wasn't already dirty
	m_networkDirtyObjsCreate.Insert( pObj );
}

//----------------------------------------------------------------------------
// Purpose: Remembers that this field and this object are dirty
//----------------------------------------------------------------------------
void CGCSharedObjectCache::DirtyDatabaseObjectField( CSharedObject *pObj, int nFieldIndex )
{
#ifdef DEBUG
	// Make sure the object is in the shared object cache.
	Assert( IsObjectCached( pObj, pObj->GetTypeID() ) );
#endif

	// add it to our dirty list if it wasn't already dirty
	m_databaseDirtyList.DirtyObjectField( pObj, nFieldIndex );
}

//----------------------------------------------------------------------------
// Purpose: Remembers that this field and this object are dirty
//----------------------------------------------------------------------------
void CGCSharedObjectCache::DirtyObjectField( CSharedObject *pObj, int nFieldIndex )
{
	if ( pObj->BIsNetworked() )
	{
		DirtyNetworkObject( pObj );
	}
	if ( pObj->BIsDatabaseBacked() )
	{
		DirtyDatabaseObjectField( pObj, nFieldIndex );
	}
}

//----------------------------------------------------------------------------
// Purpose: Sends all network updates for this object and forgets it was dirty.
//			If the object was not dirty, do nothing.
//----------------------------------------------------------------------------
void CGCSharedObjectCache::SendNetworkUpdates( CSharedObject *pObj )
{
	UtlHashHandle_t netHandleCreate = m_networkDirtyObjsCreate.Find( pObj );
	if ( netHandleCreate != m_networkDirtyObjsCreate.InvalidHandle() )
	{
		SendNetworkCreateInternal( pObj );
		m_networkDirtyObjsCreate.Remove( pObj );
	}

	UtlHashHandle_t netHandle = m_networkDirtyObjs.Find( pObj );
	if ( netHandle != m_networkDirtyObjs.InvalidHandle() )
	{
		SendNetworkUpdateInternal( pObj );
		m_networkDirtyObjs.Remove( pObj );
	}
}

//----------------------------------------------------------------------------
// Purpose: Sends network updates for all network-dirty objects
//----------------------------------------------------------------------------
void CGCSharedObjectCache::SendAllNetworkUpdates()
{
	VPROF_BUDGET( "CGCSharedObjectCache::SendAllNetworkUpdates", VPROF_BUDGETGROUP_STEAM );

	// Create messages first
	if ( m_networkDirtyObjsCreate.Count() )
	{
		FOR_EACH_HASHTABLE( m_networkDirtyObjsCreate, i )
		{
			SendNetworkCreateInternal( m_networkDirtyObjsCreate.Key( i ) );
		}

		m_networkDirtyObjsCreate.RemoveAll();
	}

	// Check if anything is actually dirty first
	if ( !m_networkDirtyObjs.Count() )
		return;

	// build all object messages first

	CUtlVector< CMsgSOMultipleObjects_SingleObject > vecObjectMsgs( 0, m_networkDirtyObjs.Count() );
	CUtlVector< CSharedObject* > vecObjects( 0, m_networkDirtyObjs.Count() );

	{
		VPROF_BUDGET( "CGCSharedObjectCache::SendAllNetworkUpdates - build messages", VPROF_BUDGETGROUP_STEAM );
		FOR_EACH_HASHTABLE( m_networkDirtyObjs, i )
		{
			CSharedObject *pObj = m_networkDirtyObjs.Key( i );
			uint32 unFlags = CSharedObject::GetTypeFlags( pObj->GetTypeID() );
			if( !BShouldSendToAnyClients( unFlags ) )
				continue;

			// build single object msg
			vecObjects.AddToTail( pObj );
			int idx = vecObjectMsgs.AddToTail();		
			CMsgSOMultipleObjects_SingleObject &msg = vecObjectMsgs[idx];
			msg.set_type_id( pObj->GetTypeID() );
			pObj->BAddToMessage( msg.mutable_object_data() );
		}
	}
	m_networkDirtyObjs.RemoveAll();

	// Check if anything is still dirty. It's possible that everything dirty are not sent to clients
	if ( 0 == vecObjects.Count() )
		return;

	// now build uber msg for each subscriber
	{
		VPROF_BUDGET( "CGCSharedObjectCache::SendAllNetworkUpdates - send all messages", VPROF_BUDGETGROUP_STEAM );

		CUtlVector<int> vecObjsToSendToThisUser( 0, vecObjects.Count() );

		const CUtlVector< CSharedObjectContext::Subscriber_t > &vecSubscribers = m_context.GetSubscribers();
		const CISubscriberMessageFilter &filter = GetSubscriberMessageFilter();

		FOR_EACH_VEC( vecSubscribers, nSub )
		{
			vecObjsToSendToThisUser.RemoveAll();

			// Figure out which, if any, objects should be sent to this user
			uint32 unFlags = CalcSendFlags( vecSubscribers[nSub].m_steamID );
			FOR_EACH_VEC( vecObjectMsgs, idx )
			{
				CSharedObject *pObj = vecObjects[idx];
				if( filter.BShouldSendObject( pObj, unFlags ) )
				{
					vecObjsToSendToThisUser.AddToTail( idx );
				}
			}

			// If we have any objects to send then build the message and send it
			if ( vecObjsToSendToThisUser.Count() > 0 )
			{
				VPROF_BUDGET( "CGCSharedObjectCache::SendAllNetworkUpdates - Sending Msg", VPROF_BUDGETGROUP_STEAM );
				CProtoBufMsg< CMsgSOMultipleObjects > msg( k_ESOMsg_UpdateMultiple );
				msg.Body().set_owner( GetOwner().ConvertToUint64() );
				msg.Body().set_version( GetVersion() );

				FOR_EACH_VEC( vecObjsToSendToThisUser, idx )
				{
					const CMsgSOMultipleObjects_SingleObject &objectMsg = vecObjectMsgs[vecObjsToSendToThisUser[idx]];
					g_SharedObjectStats.TrackSharedObjectSend( objectMsg.type_id(), 1, objectMsg.object_data().size() );
					// KFIXME: Tracking for when multiplex changes integrated
					// g_SharedObjectStats.TrackSharedObjectSend( vecObjsToSendToThisUser[idx]->type_id(), mapFlagsToObjectsToSend[iMap]->m_vecRecipients.Count(), vecObjsToSendToThisUser[idx]->object_data().size() );
					CMsgSOMultipleObjects_SingleObject *pObjMsg = msg.Body().add_objects();
					*pObjMsg = objectMsg;
				}

				GGCBase()->BSendGCMsgToClient( vecSubscribers[nSub].m_steamID, msg );
			}
		}
	}
}

//----------------------------------------------------------------------------
// Purpose: Enqueues a flush instruction to Econ service for Web Inventory to update
//----------------------------------------------------------------------------
void CGCSharedObjectCache::FlushInventoryCache()
{
	const CSteamID& steamID = GetOwner();
	if ( steamID.IsValid() && steamID.BIndividualAccount() )
	{
		GGCBase()->FlushInventoryCache( steamID.GetAccountID() );
	}
}

//----------------------------------------------------------------------------
// Purpose: Adds the write to the transaction
//----------------------------------------------------------------------------
bool CGCSharedObjectCache::BYieldingAddWriteToTransaction( CSharedObject *pObj, CSQLAccess & sqlAccess )
{
	CSteamID cacheOwner = m_context.GetOwner();
	Assert( GGCBase()->IsSteamIDLockedByCurJob( cacheOwner ) );

	CCopyableUtlVector< int > dirtyFields;
	m_databaseDirtyList.GetDirtyFieldSetByObj( pObj, dirtyFields );
	if ( !pObj->BYieldingAddWriteToTransaction( sqlAccess, dirtyFields ) )
		return false;
	m_databaseDirtyList.FindAndRemove( pObj );

	// Add a listener to the transaction to re-add these to the cache should the transaction fail.  This stores off a
	// copy of the dirty object list, such that the caller can clear theirs now and take other actions - we'll merge
	// back in upon rollback.
	auto *pSelf = this;
	sqlAccess.AddRollbackListener( [pSelf, cacheOwner, pObj, dirtyFields] () \
	{
		Assert( GGCBase()->IsSteamIDLockedByCurJob( cacheOwner ) );
		FOR_EACH_VEC( dirtyFields, idx )
		{
			pSelf->DirtyDatabaseObjectField( pObj, dirtyFields[idx] );
		}

		// we don't need to worry about re-adding to writeback -- Callers must maintain a lock of the cache for the
		// duration, so we can't be processed for writeback unless the caller is writeback itself.
	} );

	return true;
}

//----------------------------------------------------------------------------
// Purpose: Sends network updates for all network-dirty objects
//----------------------------------------------------------------------------
void CGCSharedObjectCache::SendNetworkCreateInternal( CSharedObject * pObj )
{
	uint32 unFlags = CSharedObject::GetTypeFlags( pObj->GetTypeID() );
	if( !BShouldSendToAnyClients( unFlags ) )
		return;

	VPROF_BUDGET( CSharedObject::PchClassUpdateNodeName( pObj->GetTypeID() ), VPROF_BUDGETGROUP_STEAM );

	const CISubscriberMessageFilter &filter = GetSubscriberMessageFilter();
	const CUtlVector< CSharedObjectContext::Subscriber_t > &vecSubscribers = m_context.GetSubscribers();

	CUtlVector<CSteamID> vecRecipients( 0, vecSubscribers.Count() );
	FOR_EACH_VEC( vecSubscribers, nSub )
	{
		const CSteamID & steamID = vecSubscribers[nSub].m_steamID;
		Assert( steamID.IsValid() );
		uint32 unFlags = CalcSendFlags( steamID );
		if( filter.BShouldSendObject( pObj, unFlags ) )
		{
			VPROF_BUDGET( CSharedObject::PchClassCreateNodeName( pObj->GetTypeID() ), VPROF_BUDGETGROUP_STEAM );
			pObj->BSendCreateToSteamID( steamID, m_context.GetOwner(), GetVersion() );
			vecRecipients.AddToTail( steamID );
		}
	}

	if ( vecRecipients.Count() > 0 )
	{
		g_SharedObjectStats.TrackSharedObjectSendCreate( pObj->GetTypeID(), vecRecipients.Count() );
	}
}

//----------------------------------------------------------------------------
// Purpose: Sends network updates for all network-dirty objects
//----------------------------------------------------------------------------
void CGCSharedObjectCache::SendNetworkUpdateInternal( CSharedObject * pObj )
{
	uint32 unFlags = CSharedObject::GetTypeFlags( pObj->GetTypeID() );
	if( !BShouldSendToAnyClients( unFlags ) )
		return;

	VPROF_BUDGET( CSharedObject::PchClassUpdateNodeName( pObj->GetTypeID() ), VPROF_BUDGETGROUP_STEAM );

	CProtoBufMsg< CMsgSOSingleObject > msg( k_ESOMsg_Update );
	msg.Body().set_owner( GetOwner().ConvertToUint64() );
	msg.Body().set_type_id( pObj->GetTypeID() );
	msg.Body().set_version( GetVersion() );
	pObj->BAddToMessage( msg.Body().mutable_object_data() );

	const CISubscriberMessageFilter &filter = GetSubscriberMessageFilter();
	const CUtlVector< CSharedObjectContext::Subscriber_t > &vecSubscribers = m_context.GetSubscribers();

	FOR_EACH_VEC( vecSubscribers, nSub )
	{
		uint32 unFlags = CalcSendFlags( vecSubscribers[nSub].m_steamID );
		if( filter.BShouldSendObject( pObj, unFlags ) )
		{
			GGCBase()->BSendGCMsgToClient( vecSubscribers[nSub].m_steamID, msg );
		}
	}
}


//----------------------------------------------------------------------------
// Purpose: Saves the object to the database if it is dirty.
//----------------------------------------------------------------------------
void CGCSharedObjectCache::YieldingWriteToDatabase( CSharedObject *pObj )
{
	CUtlVector< int > dirtyFields;
	if( m_databaseDirtyList.GetDirtyFieldSetByObj( pObj, dirtyFields ) )
	{
		if ( pObj->BYieldingWriteToDatabase( dirtyFields ) )
		{
			m_databaseDirtyList.FindAndRemove( pObj );
		}
	}
}

//----------------------------------------------------------------------------
// Purpose: Adds all dirty objects to a transaction to save to the database
// Returns: The number of objects added to the transaction
//----------------------------------------------------------------------------
uint32 CGCSharedObjectCache::YieldingStageAllWrites( CSQLAccess & sqlAccess )
{
	int count = 0;
	int nDirtyObjects = m_databaseDirtyList.NumDirtyObjects();
	for( int i = 0; i < nDirtyObjects; ++i )
	{
		CUtlVector< int > dirtyFields;
		CSharedObject *pObj;
		if ( m_databaseDirtyList.GetDirtyFieldSetByIndex( i, &pObj, dirtyFields ) )
		{
			if ( pObj->BYieldingAddWriteToTransaction( sqlAccess, dirtyFields ) )
			{
				count++;
			}
		}
	}

	// Add a listener to flush our list if this transaction succeeds
	auto *pSelf = this;
	sqlAccess.AddCommitListener( [pSelf, nDirtyObjects] () \
	{
		// Changing the dirty list during a transaction is not allowed
		Assert( nDirtyObjects == pSelf->m_databaseDirtyList.NumDirtyObjects() );
		pSelf->m_databaseDirtyList.RemoveAll();
	} );

	return count;
}

//----------------------------------------------------------------------------
// Purpose: Sets the writeback flag and remembers the time it was first set
//			if appropriate.
//----------------------------------------------------------------------------
void CGCSharedObjectCache::SetInWriteback( bool bInWriteback )
{
	if( !m_bInWriteback && bInWriteback )
	{
		m_unWritebackTime = time( NULL );
	}
	m_bInWriteback = bInWriteback;
}

//----------------------------------------------------------------------------
// Purpose: 
//----------------------------------------------------------------------------
void CGCSharedObjectCache::MarkDirty()
{
	//see if we are detecting a version change when we weren't expecting one. This is often tied to loading or other events that should leave the version in a constant state
	//or runs the risk of creating unecessary sends
	AssertMsg( !m_bDetectVersionChanges, "Warning: Detected a change to the version of the SO cache when it was not expected. This is often done when an SO cache is being loaded, and can result in costly unnecessary updates of caches" );

	CSharedObjectCache::MarkDirty();
	ClearCachedSubscriptionMessage();
	m_ulVersion = GGCHost()->GenerateGID();
	GGCBase()->AddCacheToVersionChangedList( this );
}

//----------------------------------------------------------------------------
// Purpose: Determine the send flags for the given steam id
//----------------------------------------------------------------------------
uint32 CGCSharedObjectCache::CalcSendFlags( const CSteamID & steamID ) const
{
	if( m_context.GetOwner() == steamID )
		return k_ESOFlag_SendToOwner;

	if( steamID.BIndividualAccount() )
	{
		return k_ESOFlag_SendToOtherUsers;
	}
	else if( steamID.BGameServerAccount() )
	{
		return k_ESOFlag_SendToOtherGameservers | k_ESOFlag_SendToQuestObjectiveTrackers;
	}

	// What sort of an account is this?!
	Assert( steamID.IsValid() );

	return k_ESOFlag_SendToNobody;
}

//----------------------------------------------------------------------------
// Purpose: Given the flags, are we supposed to send anything to them
//----------------------------------------------------------------------------
bool CGCSharedObjectCache::BShouldSendToAnyClients( uint32 unFlags ) const
{
	return 0 != (unFlags & (k_ESOFlag_SendToOwner | k_ESOFlag_SendToOtherGameservers | k_ESOFlag_SendToOtherUsers) );
}


//----------------------------------------------------------------------------
// Purpose: Dumps all the objects in the type cache
//----------------------------------------------------------------------------
void CGCSharedObjectCache::Dump() const
{
	CSharedObjectCache::Dump();
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\t Subscribers (%d). Version (%llu):\n", m_context.GetSubscribers().Count(), m_ulVersion );
	FOR_EACH_VEC( m_context.GetSubscribers(), nSub )
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\t\t%s\n", m_context.GetSubscribers()[nSub].m_steamID.Render() );
	}
}

//----------------------------------------------------------------------------
// Purpose: 
//----------------------------------------------------------------------------
void CGCSharedObjectCache::DumpDirtyObjects() const
{
	if ( m_networkDirtyObjsCreate.Count() > 0 )
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\t Num network create dirty (%d):\n", m_networkDirtyObjsCreate.Count() );
		FOR_EACH_HASHTABLE( m_networkDirtyObjsCreate, i )
		{
			CSharedObject *pObj = m_networkDirtyObjsCreate.Key(i);
			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\t\t Object type id: %d\n", pObj->GetTypeID() );
		}
	}
	if ( m_networkDirtyObjs.Count() > 0 )
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\t Num network dirty (%d):\n", m_networkDirtyObjs.Count() );
		FOR_EACH_HASHTABLE( m_networkDirtyObjs, i )
		{
			CSharedObject *pObj = m_networkDirtyObjs.Key(i);
			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\t\t Object type id: %d\n", pObj->GetTypeID() );
		}
	}
	if ( m_databaseDirtyList.NumDirtyObjects() > 0 )
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\t Num database dirty (%d):\n", m_databaseDirtyList.NumDirtyObjects() );
		for( int i = 0; i < m_databaseDirtyList.NumDirtyObjects(); ++i )
		{
			CSharedObject *pObj = NULL;
			CUtlVector<int> fieldSet;
			m_databaseDirtyList.GetDirtyFieldSetByIndex( i, &pObj, fieldSet );
			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\t\t Object type id: %d\n", pObj->GetTypeID() );
		}
	}
}


//----------------------------------------------------------------------------
// Purpose: Claims all the memory for the cache
//----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CGCSharedObjectCache::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	CGCSharedObjectCache::Validate( validator, pchName );
	ValidateObj( m_networkDirtyObjs );
	ValidateObj( m_networkDirtyObjsCreate );
	ValidateObj( m_databaseDirtyList );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Interface for profiling SO cache stats
//-----------------------------------------------------------------------------

GC_CON_COMMAND( so_profile_on, "Starts the profiling of shared object stats" )
{
	//stop any previous one to make sure that our time is up to date
	g_SharedObjectStats.StopCollectingStats();
	g_SharedObjectStats.ResetStats();
	g_SharedObjectStats.StartCollectingStats();
}

GC_CON_COMMAND( so_profile_off, "Stops the profiling of shared object stats" )
{
	g_SharedObjectStats.StopCollectingStats();
}

GC_CON_COMMAND( so_profile_dump, "Displays the stats collected from profiling shared object stats" )
{
	g_SharedObjectStats.ReportCollectedStats();
}

}  // namespace GCSDK

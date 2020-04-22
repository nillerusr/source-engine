//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Additional shared object cache functionality for the GC
//
//=============================================================================

#ifndef GC_SHAREDOBJECTCACHE_H
#define GC_SHAREDOBJECTCACHE_H
#ifdef _WIN32
#pragma once
#endif

#include "sharedobjectcache.h"
#include "tier1/utlhashtable.h"
#include "gcdirtyfield.h"
#include "gcsdk/gcsystemaccess.h"

class CMsgSOCacheSubscribed_SubscribedType;

#include "tier0/memdbgon.h"

namespace GCSDK
{

class CCachedSubscriptionMessage;

//----------------------------------------------------------------------------
// Purpose: The part of a shared object cache that handles all objects of a 
//			single type.
//----------------------------------------------------------------------------
class CSharedObjectContext
{
public:

	//holds information about a subscriber to the context
	struct Subscriber_t
	{
		CSteamID	m_steamID;		//the steam ID of the subscriber
		int			m_nRefCount;	//the number of references to this subscription
	};

	CSharedObjectContext( const CSteamID & steamIDOwner );

	bool BAddSubscriber( const CSteamID & steamID );
	bool BRemoveSubscriber( const CSteamID & steamID );
	void RemoveAllSubscribers();
	bool BIsSubscribed( const CSteamID & steamID ) const			{ return FindSubscriber( steamID ) != m_vecSubscribers.InvalidIndex(); }
	
	const CUtlVector< Subscriber_t > &	GetSubscribers() const		{ return m_vecSubscribers; }
	const CSteamID &					GetOwner() const			{ return m_steamIDOwner; }

private:

	//finds a steam ID within the subscriber list, returns the index, invalid if it can't be found
	int FindSubscriber( const CSteamID& steamID ) const;

	CUtlVector<Subscriber_t>	m_vecSubscribers;
	CSteamID					m_steamIDOwner;
};

class CGCSharedObjectTypeCache;

enum ESOTypeFlags
{
	k_ESOFlag_SendToNobody					= 0,
	k_ESOFlag_SendToOwner					= 1 << 0,  // will go to the owner of the cache (user or gameserver), if he is subscribed
	k_ESOFlag_SendToOtherUsers				= 1 << 1,  // will go to subscribed users who are not the owner of the cache
	k_ESOFlag_SendToOtherGameservers		= 1 << 2,  // will go to subscribed gameservers who are not the owner of the cache
	k_ESOFlag_SendToQuestObjectiveTrackers	= 1 << 3,
	k_ESOFlag_LastFlag						= k_ESOFlag_SendToQuestObjectiveTrackers,
};

//----------------------------------------------------------------------------
// Purpose: Filter object used to determine whether a type cache's objects should
//	should be sent to subscribers and whether each object should be sent
//----------------------------------------------------------------------------
class CISubscriberMessageFilter
{
public:
	virtual bool BShouldSendAnyObjectsInCache( CGCSharedObjectTypeCache *pTypeCache, uint32 unFlags ) const = 0;
	virtual bool BShouldSendObject( CSharedObject *pSharedObject, uint32 unFlags ) const = 0;
};


//----------------------------------------------------------------------------
// Purpose: The part of a shared object cache that handles all objects of a 
//			single type.
//----------------------------------------------------------------------------
class CGCSharedObjectTypeCache : public CSharedObjectTypeCache
{
public:

	typedef CSharedObjectTypeCache Base;

	CGCSharedObjectTypeCache( int nTypeID, const CSharedObjectContext & context );
	virtual ~CGCSharedObjectTypeCache();

	virtual bool AddObject( CSharedObject *pObject );
	virtual CSharedObject *RemoveObject( const CSharedObject & soIndex );

	inline CSteamID GetOwner() const { return m_context.GetOwner(); }

	void BuildCacheSubscribedMsg( CMsgSOCacheSubscribed_SubscribedType *pMsgType, uint32 unFlags, const CISubscriberMessageFilter &filter );
	virtual void EnsureCapacity( uint32 nItems );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName );
#endif

private:

	const CSharedObjectContext & m_context;
};



//----------------------------------------------------------------------------
// Purpose: A cache of a bunch of shared objects of different types. This class
//			is shared between clients, gameservers, and the GC and is 
//			responsible for sending messages from the GC to cause object 
//			creation/destruction/updating on the clients/gameservers.
//----------------------------------------------------------------------------

class CGCSharedObjectCache : public CSharedObjectCache
{
public:
	CGCSharedObjectCache( const CSteamID & steamIDOwner = CSteamID() );
	virtual ~CGCSharedObjectCache();

	const CSteamID & GetOwner() const { return m_context.GetOwner(); }
	const CUtlVector< CSharedObjectContext::Subscriber_t > & GetSubscribers() const { return m_context.GetSubscribers(); }

	CGCSharedObjectTypeCache *FindTypeCache( int nClassID ) const { return (CGCSharedObjectTypeCache *)FindBaseTypeCache( nClassID ); }
	CGCSharedObjectTypeCache *CreateTypeCache( int nClassID ) { return (CGCSharedObjectTypeCache *)CreateBaseTypeCache( nClassID ); }

	virtual uint32 CalcSendFlags( const CSteamID &steamID ) const;
	virtual const CISubscriberMessageFilter &GetSubscriberMessageFilter();
	virtual bool AddObject( CSharedObject *pSharedObject );
	virtual bool AddObjectClean( CSharedObject *pSharedObject );

	bool BDestroyObject( const CSharedObject & soIndex, bool bRemoveFromDatabase );
	virtual CSharedObject *RemoveObject( const CSharedObject & soIndex );

	template< typename SOClass_t >
	bool BYieldingLoadSchObjects( IGCSQLResultSet *pResultSet, const CColumnSet & csRead, const SOClass_t & schDefaults );

	template< typename SOClass_t >
	bool BYieldingLoadSchSingleton( IGCSQLResultSet *pResultSet, const CColumnSet & csRead, const SOClass_t & schDefaults );

	template< typename SOClass_t, typename SchClass_t >
	bool BYieldingLoadProtoBufObjects( IGCSQLResultSet *pResultSet, const CColumnSet & csRead );

	template< typename SOClass_t, typename SchClass_t >
	bool BYieldingLoadProtoBufSingleton( IGCSQLResultSet *pResultSet, const CColumnSet & csRead, const SchClass_t & schDefaults );

	// @todo temporary for trading and item subscriptions (to be removed once we get cross-game trading)
	virtual void SetTradingPartner( const CSteamID &steamID );
	const CSteamID &GetTradingPartner() const { return m_steamIDTradingPartner; }

	void AddSubscriber( const CSteamID & steamID, bool bForceSendSubscriptionMsg = false );
	void RemoveSubscriber( const CSteamID & steamID );
	void RemoveAllSubscribers();
	void SendSubscriberMessage( const CSteamID & steamID );
	bool BIsSubscribed( const CSteamID & steamID ) { return m_context.BIsSubscribed( steamID ); }
	void ClearCachedSubscriptionMessage();

	bool BIsDatabaseDirty() const { return m_databaseDirtyList.NumDirtyObjects() > 0; }

	// This will mark the field as dirty for both network and database
	void DirtyObjectField( CSharedObject *pObj, int nFieldIndex );

	// Marks only dirty for network
	void DirtyNetworkObject( CSharedObject *pObj );
	void DirtyNetworkObjectCreate( CSharedObject *pObj );

	// Mark dirty for database
	void DirtyDatabaseObjectField( CSharedObject *pObj, int nFieldIndex );

	void SendNetworkUpdates( CSharedObject *pObj );

	// Add a specific object write to a transaction. The cache is expected to remain locked until this transaction is
	// closed.  If the transaction is rolled back, the object will be returned to the dirty list.
	bool BYieldingAddWriteToTransaction( CSharedObject *pObj, CSQLAccess & sqlAccess );

	// Add all pending object writes to a transaction.  The cache is expected to remain locked until this transaction is
	// closed.  If the transaction successfully commits, the dirty list will be flushed.
	//
	// This is intended for use in writeback -- no changes to the dirty objects list may occur until this transaction
	// closes.
	uint32 YieldingStageAllWrites( CSQLAccess & sqlAccess );

	void SendAllNetworkUpdates();
	void FlushInventoryCache();
	void YieldingWriteToDatabase( CSharedObject *pObj );

	void SetInWriteback( bool bInWriteback );
	bool GetInWriteback() const { return m_bInWriteback; }
	RTime32 GetWritebackTime() const { return m_unWritebackTime; }

	void SetLRUHandle( uint32 unLRUHandle ) { m_unLRUHandle = unLRUHandle; }
	uint32 GetLRUHandle() const { return m_unLRUHandle; }

	void Dump() const;
	void DumpDirtyObjects() const;
#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName );
#endif

	bool IsObjectCached( const CSharedObject *pObj ) const						{ return IsObjectCached( pObj, pObj->GetTypeID() ); }
	//the same as the above, but takes in the type since if called from a destructor, you can't use virtual functions
	bool IsObjectCached( const CSharedObject *pObj, uint32 nTypeID ) const;
	bool IsObjectDirty( const CSharedObject *pObj ) const;

	//called to mark that we are no longer loading
	void SetDetectVersionChanges( bool bState )		{ m_bDetectVersionChanges = bState; }

protected:

	virtual CSharedObjectTypeCache *AllocateTypeCache( int nClassID ) const OVERRIDE { return new CGCSharedObjectTypeCache( nClassID, m_context ); }

	virtual void MarkDirty();
	virtual bool BShouldSendToAnyClients( uint32 unFlags ) const;
	CCachedSubscriptionMessage *BuildSubscriberMessage( uint32 unFlags );

	CSteamID m_steamIDTradingPartner;

protected:
	void SendNetworkCreateInternal( CSharedObject * pObj );
	void SendNetworkUpdateInternal( CSharedObject * pObj );
	void SendUnsubscribeMessage( const CSteamID & steamID );

	//this is a flag that when set will cause any version changes to trigger an assert. This can be used during times like loading to ensure we don't have inappropriate version changes which
	//can cause inefficiencies
	bool	m_bDetectVersionChanges;

	CSharedObjectContext m_context;
	CUtlHashtable< CSharedObject * > m_networkDirtyObjs;
	CUtlHashtable< CSharedObject * > m_networkDirtyObjsCreate;
	CSharedObjectDirtyList m_databaseDirtyList;
	bool m_bInWriteback;
	RTime32 m_unWritebackTime;
	uint32 m_unLRUHandle;
	uint32 m_unCachedSubscriptionMsgFlags;
	CCachedSubscriptionMessage *m_pCachedSubscriptionMsg;
};




//----------------------------------------------------------------------------
// Purpose: Loads a list of CSchemaSharedObjects from a result list from a
//			query.
// Inputs:	pResultSet - The result set from the SQL query
//			schDefaults - A schema object that defines the values to set in
//				the new objects for fields that were not read in the query.
//				Typically this will be whatever fields were in the WHERE
//				clause of the query.
//			csRead - A columnSet defining the fields that were read in the query.
//----------------------------------------------------------------------------
template< typename SOClass_t >
bool CGCSharedObjectCache::BYieldingLoadSchObjects( IGCSQLResultSet *pResultSet, const CColumnSet & csRead, const SOClass_t & objDefaults )
{
	if ( NULL == pResultSet )
		return false;

	//don't bother creating a cache if we don't have objects to add into it
	if( pResultSet->GetRowCount() > 0 )
	{
		CGCSharedObjectTypeCache *pTypeCache = CreateTypeCache( SOClass_t::k_nTypeID );
		pTypeCache->EnsureCapacity( pResultSet->GetRowCount() );
		for( CSQLRecord record( 0, pResultSet ); record.IsValid(); record.NextRow() )
		{
			SOClass_t *pObj = new SOClass_t();
			pObj->Obj() = objDefaults.Obj();
			record.BWriteToRecord( &pObj->Obj(), csRead );
			pTypeCache->AddObjectClean( pObj );
		}
	}

	return true;
}

//----------------------------------------------------------------------------
// Purpose: Loads a single object of a type. If the object is not available,
//	a new object will be created at default values
// Inputs:	pResultSet - The result set from the SQL query
//			schDefaults - A schema object that defines the values to set in
//				the new objects for fields that were not read in the query.
//				Typically this will be whatever fields were in the WHERE
//				clause of the query.
//			csRead - A columnSet defining the fields that were read in the query.
//----------------------------------------------------------------------------
template< typename SOClass_t >
bool CGCSharedObjectCache::BYieldingLoadSchSingleton( IGCSQLResultSet *pResultSet, const CColumnSet & csRead, const SOClass_t & objDefaults )
{
	if ( NULL == pResultSet )
		return false;

	if ( pResultSet->GetRowCount() > 1 )
	{
		EmitError( SPEW_SHAREDOBJ, "Multiple rows passed to BYieldingLoadSchSingleton() on type %d\n", objDefaults.GetTypeID() );
		return false;
	}
	else if ( pResultSet->GetRowCount() == 1 )
	{
		return BYieldingLoadSchObjects<SOClass_t>( pResultSet, csRead, objDefaults );
	}
	else
	{
		// Create it if there wasn't one
		SOClass_t *pSchObj = new SOClass_t();
		pSchObj->Obj() = objDefaults.Obj();
		if( !pSchObj->BYieldingAddToDatabase() )
		{
			EmitError( SPEW_SHAREDOBJ, "Unable to add singleton type %d for %s\n", pSchObj->GetTypeID(), GetOwner().Render() );
			return false;
		}
		AddObjectClean( pSchObj );
		return true;
	}
}


//----------------------------------------------------------------------------
// Purpose: Loads a list of CProtoBufSharedObjects from a result list from a
//			query.
// Inputs:	pResultSet - The result set from the SQL query
//			schDefaults - A schema object that defines the values to set in
//				the new objects for fields that were not read in the query.
//				Typically this will be whatever fields were in the WHERE
//				clause of the query.
//			csRead - A columnSet defining the fields that were read in the query.
//----------------------------------------------------------------------------
template< typename SOClass_t, typename SchClass_t >
bool CGCSharedObjectCache::BYieldingLoadProtoBufObjects( IGCSQLResultSet *pResultSet, const CColumnSet & csRead )
{
	if ( NULL == pResultSet )
		return false;

	//don't bother creating a cache if we don't have objects to add into it
	if( pResultSet->GetRowCount() > 0 )
	{
		CGCSharedObjectTypeCache *pTypeCache = CreateTypeCache( SOClass_t::k_nTypeID );
		pTypeCache->EnsureCapacity( pResultSet->GetRowCount() );
		for( CSQLRecord record( 0, pResultSet ); record.IsValid(); record.NextRow() )
		{
			SchClass_t schRecord;
			record.BWriteToRecord( &schRecord, csRead );

			SOClass_t *pObj = new SOClass_t();
			pObj->ReadFromRecord( schRecord );
			pTypeCache->AddObjectClean( pObj );
		}
	}

	return true;
}


//----------------------------------------------------------------------------
// Purpose: Loads a single object of a type. If the object is not available,
//	a new object will be created at default values
// Inputs:	pResultSet - The result set from the SQL query
//			schDefaults - A schema object that defines the values to set in
//				the new objects for fields that were not read in the query.
//				Typically this will be whatever fields were in the WHERE
//				clause of the query.
//			csRead - A columnSet defining the fields that were read in the query.
//----------------------------------------------------------------------------
template< typename SOClass_t, typename SchClass_t >
bool CGCSharedObjectCache::BYieldingLoadProtoBufSingleton( IGCSQLResultSet *pResultSet, const CColumnSet & csRead, const SchClass_t & schDefaults )
{
	if ( NULL == pResultSet )
		return false;

	if ( pResultSet->GetRowCount() > 1 )
	{
		EmitError( SPEW_SHAREDOBJ, "Multiple rows passed to BYieldingLoadProtoBufSingleton() on type %d\n", SOClass_t::k_nTypeID );
		return false;
	}

	// load the duel summary
	SchClass_t schRead;
	CSQLRecord record( 0, pResultSet );
	if( record.IsValid() )
	{
		record.BWriteToRecord( &schRead, csRead );
	}
	else
	{
		CSQLAccess sqlAccess;
		if( !sqlAccess.BYieldingInsertRecord( const_cast<SchClass_t *>( &schDefaults ) ) )
			return false;
		schRead = schDefaults;
	}

	SOClass_t *pSharedObject = new SOClass_t();
	pSharedObject->ReadFromRecord( schRead );
	AddObjectClean( pSharedObject );

	return true;
}


} // namespace GCSDK

#include "tier0/memdbgoff.h"

#endif //GC_SHAREDOBJECTCACHE_H

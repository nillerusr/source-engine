//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base class for objects that are kept in synch between client and server
//
//=============================================================================

#ifndef SHAREDOBJECT_H
#define SHAREDOBJECT_H
#ifdef _WIN32
#pragma once
#endif

// ENABLE_SO_OVERWRITE_PARANOIA can be set to either 0 or 1. If enabled, it will add
// extra fields to every CSharedObject instance to try and detect overwrites at the
// cost of additional runtime memory.
#define ENABLE_SO_OVERWRITE_PARANOIA				0

// ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA can be set to either 0 or 1. If enabled, it
// will add extra fields to every CSharedObject instance to try and detect issues with
// constructions/destruction (ie., double-deletes, etc.), including reference counting.
#define ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA		(defined( STAGING_ONLY ))

#include "tier0/memdbgon.h"

namespace GCSDK
{

class CSQLAccess;
class CSharedObject;
typedef CSharedObject *(*SOCreationFunc_t)( );
class CSharedObjectCache;



//----------------------------------------------------------------------------
// Purpose: Abstract base class for objects that are shared between the GC and
//			a gameserver/client. These can also be stored in the database.
//----------------------------------------------------------------------------
class CSharedObject
{
	friend class CGCSharedObjectCache;
	friend class CSharedObjectCache;
public:

#ifdef GC
	virtual ~CSharedObject()
	{
#if ENABLE_SO_OVERWRITE_PARANOIA
		m_pThis = NULL;
#endif // ENABLE_SO_OVERWRITE_PARANOIA

#if ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
		AssertMsg2( m_nRefCount == 0, "Deleting shared object type %d with refcount %d", m_nSOTypeID, m_nRefCount );
		
		m_nSOTypeID = -1;
		m_nRefCount = -1;
#endif // ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
	}

#if ENABLE_SO_OVERWRITE_PARANOIA
	CSharedObject *m_pThis;
#endif // ENABLE_SO_OVERWRITE_PARANOIA

#if ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
	int m_nRefCount;
	mutable int m_nSOTypeID;
#endif // ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA

	void Check() const
	{
#if ENABLE_SO_OVERWRITE_PARANOIA
		Assert( m_pThis == this );
#endif // ENABLE_SO_OVERWRITE_PARANOIA

#if ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
		Assert( m_nRefCount >= 0 );

		if ( m_nSOTypeID == kSharedObject_UnassignedType )
		{
			m_nSOTypeID = GetTypeID();
		}

		Assert( m_nSOTypeID >= 0 );
#endif // ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
	}
#else
	virtual ~CSharedObject() {}
#endif

	virtual int GetTypeID() const = 0;
	virtual bool BParseFromMessage( const CUtlBuffer & buffer ) = 0;
	virtual bool BParseFromMessage( const std::string &buffer ) = 0;
	virtual bool BUpdateFromNetwork( const CSharedObject & objUpdate ) = 0;
	virtual bool BIsKeyLess( const CSharedObject & soRHS ) const = 0;
	virtual void Copy( const CSharedObject & soRHS ) = 0;
	virtual void Dump() const = 0;
	virtual bool BShouldDeleteByCache() const { return true; }
	virtual CUtlString GetDebugString() const { return PchClassName( GetTypeID() ); };

	bool BIsKeyEqual( const CSharedObject & soRHS ) const;

	static void RegisterFactory( int nTypeID, SOCreationFunc_t fnFactory, uint32 unFlags, const char *pchClassName );
	static CSharedObject *Create( int nTypeID );
	static uint32 GetTypeFlags( int nTypeID );
	static const char *PchClassName( int nTypeID );
	static const char *PchClassBuildCacheNodeName( int nTypeID );
	static const char *PchClassCreateNodeName( int nTypeID );
	static const char *PchClassUpdateNodeName( int nTypeID );

#ifdef GC
	virtual bool BIsNetworked() const { return true; }
	virtual bool BIsDatabaseBacked() const { return true; }
	virtual bool BYieldingAddToDatabase();
	virtual bool BYieldingWriteToDatabase( const CUtlVector< int > &fields );
	virtual bool BYieldingRemoveFromDatabase();

	virtual bool BYieldingAddInsertToTransaction( CSQLAccess & sqlAccess ) { return false; }
	virtual bool BYieldingAddWriteToTransaction( CSQLAccess & sqlAccess, const CUtlVector< int > &fields ) { return false; }
	virtual bool BYieldingAddRemoveToTransaction( CSQLAccess & sqlAccess ) { return false; }

	virtual bool BAddToMessage( CUtlBuffer & bufOutput ) const = 0;
	virtual bool BAddToMessage( std::string *pBuffer ) const = 0;
	virtual bool BAddDestroyToMessage( CUtlBuffer & bufDestroy ) const = 0;
	virtual bool BAddDestroyToMessage( std::string *pBuffer ) const = 0;

	virtual bool BParseFromMemcached( CUtlBuffer & buffer ) { return false; }
	virtual bool BAddToMemcached( CUtlBuffer & bufOutput ) const { return false; }

	bool BSendCreateToSteamID( const CSteamID & steamID, const CSteamID & ownerID, uint64 ulVersion ) const;
	bool BSendDestroyToSteamID( const CSteamID & steamID, const CSteamID & ownerID, uint64 ulVersion ) const;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName );
	static void ValidateStatics( CValidator & validator );
#endif

protected:
/*
	// Dirty bit modification. Do not call these directly on SharedObjects. Call them
	// on the cache that owns the object so they can be added/removed from the right lists.
	virtual void DirtyField( int nField ) = 0;
	virtual void MakeDatabaseClean() = 0;
	virtual void MakeNetworkClean() = 0;
*/
#endif // GC

#ifdef GC
	CSharedObject()
	{
#if ENABLE_SO_OVERWRITE_PARANOIA
		m_pThis = this;
#endif // ENABLE_SO_OVERWRITE_PARANOIA

#if ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
		m_nRefCount = 0;
		m_nSOTypeID = kSharedObject_UnassignedType;
#endif // ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
	}
#endif	

private:
#if ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
	enum { kSharedObject_UnassignedType = -999 };
#endif // ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA

	struct SharedObjectInfo_t
	{
		SOCreationFunc_t m_pFactoryFunction;
		uint32 m_unFlags;
		const char *m_pchClassName;
		CUtlString m_sBuildCacheSubNodeName;
		CUtlString m_sUpdateNodeName;
		CUtlString m_sCreateNodeName;
	};
	static CUtlMap<int, SharedObjectInfo_t> sm_mapFactories;

public:
	static const CUtlMap<int, SharedObjectInfo_t> & GetFactories() { return sm_mapFactories; }
};

typedef CUtlVectorFixedGrowable<CSharedObject *, 1> CSharedObjectVec;

#ifdef GC

//this class manages the stats for the shared objects, such as how many exist, how many have been sent, how many have been created, how many destroyed
class CSharedObjectStats
{
public:

	CSharedObjectStats();

	//called to register a shared object class given a type ID and name. This will return false if
	//a type of this ID is already registered
	void	RegisterSharedObjectType( int nTypeID, const char* pszTypeName );

	//called to track when a shared object is created/destroyed to update the running total
	void	TrackSharedObjectLifetime( int nTypeID, int32 nCount );

	//called when a shared object is created to track its creation
	void	TrackSharedObjectSendCreate( int nTypeID, uint32 nCount );
	//called when a shared object is destroyed to track counts
	void	TrackSharedObjectSendDestroy( int nTypeID, uint32 nCount );

	//tracks a new subscription
	void	TrackSubscription( int nTypeID, uint32 nFlags, uint32 nNumSubscriptions );

	//called when a shared object is sent
	void	TrackSharedObjectSend( int nTypeID, uint32 nNumClients, uint32 nMsgSize );

	//reset the stats for all the caches
	void	ResetStats();

	//called to start/stop collecting stats
	void	StartCollectingStats();
	void	StopCollectingStats();
	bool	IsCollectingStats() const					{ return m_bCollectingStats; }

	//called to report the currently collected stats
	void	ReportCollectedStats() const;

private:

	struct SOStats_t
	{
		SOStats_t();
		void		ResetStats();
		
		//the text name of this cache
		CUtlString	m_sName;
		//the number of outstanding SO cache objects of this type. This is not cleared
		uint32		m_nNumActive;
		//the type ID of this stat
		int			m_nTypeID;

		//the total number that have been created/destroyed. Active is difference between these
		uint32		m_nNumCreated;
		uint32		m_nNumDestroyed;
		//how many bytes we have sent (raw, and multiplexed to multiple subscribers)
		uint64		m_nRawBytesSent;
		uint64		m_nMultiplexedBytesSent;
		//the number of sends since our last clear
		uint32		m_nNumSends;
		//how many subscriptions we have gotten by various subscription types
		uint32		m_nNumSubOwner;
		uint32		m_nNumSubOtherUsers;
		uint32		m_nNumSubGameServer;
	};
	
	//sort function that controls the order that the SO stats are presented in the report
	static bool SortSOStatsSent( const SOStats_t* pLhs, const SOStats_t* pRhs );
	static bool SortSOStatsSubscribe( const SOStats_t* pLhs, const SOStats_t* pRhs );

	//to compact the memory space for wide ranging type IDs, the list of stats has two components, an array that can be looked up
	//via the type ID which maps to a stat index (or invalid index if no mapping is registered)
	static const uint16			knInvalidIndex = ( uint16 )-1;
	CUtlVector< uint16 >		m_vTypeToIndex;
	CUtlVector< SOStats_t >		m_Stats;
	
	//are we currently collecting stats or not?
	bool			m_bCollectingStats;
	//the time that we've been collecting
	CJobTime		m_CollectTime;
	uint64			m_nMicroSElapsed;
};

//global stat tracker
extern CSharedObjectStats g_SharedObjectStats;

#endif


//----------------------------------------------------------------------------
// Purpose: Templatized function to use as a factory method for 
//			CSharedObject subclasses
//----------------------------------------------------------------------------
template<typename SharedObjectSubclass_t>
CSharedObject *CreateSharedObjectSubclass()
{
	return new SharedObjectSubclass_t();
}

// Version that always asserts and returns NULL, for SOs that should not be auto-created this way
template<int nSharedObjectType>
CSharedObject *CreateSharedObjectSubclassProhibited()
{
	AssertMsg( false, "Attempting to auto-create object of type %d which does not allow SO-based creation", nSharedObjectType );
	return NULL;
}

#ifdef GC
#define REG_SHARED_OBJECT_SUBCLASS( derivedClass, flags ) GCSDK::CSharedObject::RegisterFactory( derivedClass::k_nTypeID, GCSDK::CreateSharedObjectSubclass<derivedClass>, (flags), #derivedClass )
// GC only -- sharedobjects that cannot be auto-created by the SO code, and might not have a specific derived class that
// publicly implements the interface
#define REG_SHARED_OBJECT_SUBCLASS_BY_ID_NOCREATE( strName, nTypeID, flags ) \
	GCSDK::CSharedObject::RegisterFactory( nTypeID, CreateSharedObjectSubclassProhibited<nTypeID>, (flags), strName )
#else
#define REG_SHARED_OBJECT_SUBCLASS( derivedClass ) GCSDK::CSharedObject::RegisterFactory( derivedClass::k_nTypeID, GCSDK::CreateSharedObjectSubclass<derivedClass>, 0, #derivedClass )
#endif

} // namespace GCSDK


#include "tier0/memdbgoff.h"

#endif //SHAREDOBJECT_H

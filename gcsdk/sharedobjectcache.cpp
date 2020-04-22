//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A cache of a bunch of CSharedObjects
//
//=============================================================================

#include "stdafx.h"
#include <time.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{

#ifdef GC
static GCConVar add_object_clean_do_has_element( "add_object_clean_do_has_element", "0", 0, "Enables AddObjectClean() checking that the cache doesn't already have this pointer" );
#endif

//----------------------------------------------------------------------------
// Purpose: Constructor
//----------------------------------------------------------------------------
CSharedObjectTypeCache::CSharedObjectTypeCache( int nTypeID )
: m_nTypeID( nTypeID )
{

}


//----------------------------------------------------------------------------
// Purpose: Destructor
//----------------------------------------------------------------------------
CSharedObjectTypeCache::~CSharedObjectTypeCache()
{
	for ( int i = 0; i < m_vecObjects.Count(); i++ )
	{
		// NULL the entry so that this SO isn't found during
		// cleanup assertion checking.
		CSharedObject *pObj = m_vecObjects[ i ];
		m_vecObjects[ i ] = NULL;

#ifdef GC
		if ( pObj->BShouldDeleteByCache() )
		{
#if ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
			--pObj->m_nRefCount;
			AssertMsg1( pObj->m_nRefCount == 0, "Destroying shared object %s that's still in use!", pObj->GetDebugString().String() );
#endif // ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
			delete pObj;
		}
#else
		delete pObj;		
#endif
	}
	m_vecObjects.Purge();
}

//----------------------------------------------------------------------------
// Purpose: Common shared add-to-cache code shared between AddObject() and
//			AddObjectClean().
//----------------------------------------------------------------------------
void CSharedObjectTypeCache::AddObjectInternal( CSharedObject *pObject )
{
	Assert( pObject );

	m_vecObjects.AddToTail( pObject );
#ifdef GC
#if ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
	AssertMsg1( pObject->m_nRefCount >= 0, "AddObjectInternal(): Invalid ref count for shared object %s", pObject->GetDebugString().String() );
	++pObject->m_nRefCount;
#endif // ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
#endif
}

//----------------------------------------------------------------------------
// Purpose: Adds a shared object of the appropriate type to this type cache.
//----------------------------------------------------------------------------
bool CSharedObjectTypeCache::AddObject( CSharedObject *pObject )
{
	Assert( pObject );
	Assert( m_nTypeID == pObject->GetTypeID() );
	if( m_vecObjects.HasElement( pObject ) )
		return false;

	AddObjectInternal( pObject );
	return true;
}

//----------------------------------------------------------------------------
// Purpose: Adds an object without dirtying. This is done when the object
//	is just being loaded from SQL or memcached, so it's safe not to do the
//	has element check.
//----------------------------------------------------------------------------
bool CSharedObjectTypeCache::AddObjectClean( CSharedObject *pObject )
{
	Assert( m_nTypeID == pObject->GetTypeID() );

#ifdef GC
	if ( add_object_clean_do_has_element.GetBool() )
	{
		Assert( !m_vecObjects.HasElement( pObject ) );
		if( m_vecObjects.HasElement( pObject ) )
			return false;
	}
#endif

	AddObjectInternal( pObject );
	return true;
}


//----------------------------------------------------------------------------
// Purpose: Destroys the object matching the one passed in. This could be the
//			same one or simply one with matching index fields.
//----------------------------------------------------------------------------
CSharedObject *CSharedObjectTypeCache::RemoveObject( const CSharedObject & soIndex )
{
	Assert( m_nTypeID == soIndex.GetTypeID() ); // This is probably harmless, but it's most likely a bug
	int nIndex = FindSharedObjectIndex( soIndex );
	if( m_vecObjects.IsValidIndex( nIndex ) )
	{
		return RemoveObjectByIndex( nIndex );
	}
	else
	{
		return NULL;
	}
}

CSharedObject *CSharedObjectTypeCache::RemoveObjectByIndex( uint32 nObj )
{
	CSharedObject *pObj = m_vecObjects[nObj];
#ifdef GC
#if ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
	AssertMsg1( pObj->m_nRefCount > 0, "Invalid ref count for shared object %s", pObj->GetDebugString().String() );
	--pObj->m_nRefCount;
#endif // ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
#endif // GC
	m_vecObjects.Remove( nObj );
	return pObj;
}

//----------------------------------------------------------------------------
// Purpose: Empties the object lists and deletes all elements
//----------------------------------------------------------------------------
void CSharedObjectTypeCache::DestroyAllObjects()
{
	for ( int i = 0; i < m_vecObjects.Count(); i++ )
	{
#ifdef GC
		if ( m_vecObjects[i]->BShouldDeleteByCache() )
		{
#if ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
			--m_vecObjects[i]->m_nRefCount;
			AssertMsg1( m_vecObjects[i]->m_nRefCount == 0, "Destroying shared object %s that's still in use!", m_vecObjects[i]->GetDebugString().String() );
#endif // ENABLE_SO_CONSTRUCT_DESTRUCT_PARANOIA
			delete m_vecObjects[i];
		}
#else
		delete m_vecObjects[i];
#endif
	}
	m_vecObjects.Purge();
}


//----------------------------------------------------------------------------
// Purpose: Empties the object lists but doesn't delete any of the objects
//----------------------------------------------------------------------------
void CSharedObjectTypeCache::RemoveAllObjectsWithoutDeleting()
{
	m_vecObjects.RemoveAll();
}


//----------------------------------------------------------------------------
// Purpose: Makes sure there's room in the object vector for the suggested
//			number of items
//----------------------------------------------------------------------------
void CSharedObjectTypeCache::EnsureCapacity( uint32 nItems )
{
	m_vecObjects.EnsureCapacity( nItems );
}


//----------------------------------------------------------------------------
// Purpose: Searches the object list for an object that matches the provided
//			object on its index fields.
//----------------------------------------------------------------------------
CSharedObject *CSharedObjectTypeCache::FindSharedObject( const CSharedObject & soIndex )
{
	int nIndex = FindSharedObjectIndex( soIndex );
	if( m_vecObjects.IsValidIndex( nIndex ) )
		return m_vecObjects[nIndex];
	else
		return NULL;
}


//----------------------------------------------------------------------------
// Purpose: Searches the object list for an object that matches the provided
//			object on its index fields.
//----------------------------------------------------------------------------
int CSharedObjectTypeCache::FindSharedObjectIndex( const CSharedObject & soIndex ) const
{
	FOR_EACH_VEC( m_vecObjects, nObj )
	{
		if( m_vecObjects[nObj]->BIsKeyEqual( soIndex ) )
			return nObj;
	}

	return -1;
}


//----------------------------------------------------------------------------
// Purpose: Dumps all the objects in the type cache
//----------------------------------------------------------------------------
void CSharedObjectTypeCache::Dump() const
{
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\tTypeCache for %d (%d objects):\n", GetTypeID(), m_vecObjects.Count() );
	FOR_EACH_VEC( m_vecObjects, nObj )
	{
		m_vecObjects[nObj]->Dump();
	}
}


//----------------------------------------------------------------------------
// Purpose: Claims all the memory for the cache and its objects
//----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CSharedObjectTypeCache::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_vecObjects );

	FOR_EACH_VEC( m_vecObjects, nIndex )
	{
		m_vecObjects[nIndex]->Validate( validator, "m_vecObjects[n]" );
	}
}
#endif


//----------------------------------------------------------------------------
// Purpose: Constructor
//----------------------------------------------------------------------------
CSharedObjectCache::CSharedObjectCache( ) 
: m_mapObjects( DefLessFunc(int) )
, m_ulVersion( 0 )
{

}


//----------------------------------------------------------------------------
// Purpose: Destructor
//----------------------------------------------------------------------------
CSharedObjectCache::~CSharedObjectCache()
{
	FOR_EACH_MAP( m_mapObjects, nTypeIndex )
	{
		delete m_mapObjects[nTypeIndex];
	}
	m_mapObjects.Purge();
}


//----------------------------------------------------------------------------
// Purpose: Returns the type cache for the specified type ID, returning NULL
//			if the cache didn't previously exist.
//----------------------------------------------------------------------------
CSharedObjectTypeCache *CSharedObjectCache::FindBaseTypeCache( int nClassID ) const
{
	int nIndex = m_mapObjects.Find( nClassID );
	CSharedObjectTypeCache *pTypeCache = NULL;
	if( m_mapObjects.IsValidIndex( nIndex ) )
	{
		pTypeCache = m_mapObjects[nIndex];
	}
	return pTypeCache;
}

//----------------------------------------------------------------------------
// Purpose: Returns the type cache for the specified type ID, creating a new
//			cache and returning it if one didn't previously exist. Never intended
//			to return NULL.
//----------------------------------------------------------------------------
CSharedObjectTypeCache *CSharedObjectCache::CreateBaseTypeCache( int nClassID )
{
	//see if we already have an existing one
	CSharedObjectTypeCache *pCache = FindBaseTypeCache( nClassID );
	if( pCache )
		return pCache;
	
	//nope, need to create one
	CSharedObjectTypeCache* pTypeCache = AllocateTypeCache( nClassID );
	m_mapObjects.Insert( nClassID, pTypeCache );
#if 0
	// Kyle says: this is the newer way of managing caches on Dota but we haven't
	//			  brought any of it over yet
	m_CacheObjects.AddToTail( pTypeCache );
	//sort this cache for faster access
	std::sort( m_CacheObjects.begin(), m_CacheObjects.end(), SortCacheByTypeID );
#endif
	return pTypeCache;
}


//----------------------------------------------------------------------------
// Purpose: Adds a shared object to the cache.
//----------------------------------------------------------------------------
bool CSharedObjectCache::AddObject( CSharedObject *pSharedObject )
{
	CSharedObjectTypeCache *pTypeCache = CreateBaseTypeCache( pSharedObject->GetTypeID() );
	if ( !pTypeCache->AddObject( pSharedObject ) )
		return false;

	MarkDirty();
	return true;
}


//----------------------------------------------------------------------------
// Purpose: Removes the object matching the one passed in from this cache, 
//          without destroying the actual object.
//----------------------------------------------------------------------------
CSharedObject *CSharedObjectCache::RemoveObject( const CSharedObject & soIndex )
{
	CSharedObjectTypeCache *pTypeCache = FindBaseTypeCache( soIndex.GetTypeID() );
	if( !pTypeCache )
		return NULL;

	MarkDirty();

	return pTypeCache->RemoveObject( soIndex );
}


//----------------------------------------------------------------------------
// Purpose: Empties the object lists but doesn't delete any of the objects
//----------------------------------------------------------------------------
void CSharedObjectCache::RemoveAllObjectsWithoutDeleting()
{
	FOR_EACH_MAP_FAST( m_mapObjects, nType )
	{
		m_mapObjects[nType]->RemoveAllObjectsWithoutDeleting();
	}
	MarkDirty();
}


//----------------------------------------------------------------------------
// Purpose: Searches the object list for an object that matches the provided
//			object on its index fields.
//----------------------------------------------------------------------------
CSharedObject *CSharedObjectCache::FindSharedObject( const CSharedObject & soIndex )
{
	CSharedObjectTypeCache *pTypeCache = FindBaseTypeCache( soIndex.GetTypeID() );
	if( pTypeCache )
		return pTypeCache->FindSharedObject( soIndex );
	else
		return NULL;
}

//----------------------------------------------------------------------------
// Purpose: Dumps all the objects in the type cache
//----------------------------------------------------------------------------
void CSharedObjectCache::Dump() const
{
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "SharedObjectCache for %s (%d types):\n", GetOwner().Render(), m_mapObjects.Count() );
	FOR_EACH_MAP( m_mapObjects, nTypeIndex )
	{
		m_mapObjects[nTypeIndex]->Dump();
	}
}


//----------------------------------------------------------------------------
// Purpose: Claims all the memory for the cache
//----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CSharedObjectCache::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	ValidateObj( m_mapObjects );
	FOR_EACH_MAP( m_mapObjects, nTypeIndex )
	{
		m_mapObjects[nTypeIndex]->Validate( validator, "m_mapObjects[n]" );
	}
}
#endif


}  // namespace GCSDK

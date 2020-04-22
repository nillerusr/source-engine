//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef GENERICPERSISTENTMANAGER_H
#define GENERICPERSISTENTMANAGER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/replayhandle.h"
#include "replay/ienginereplay.h"
#include "replay/replayutils.h"
#include "basethinker.h"
#include "utllinkedlist.h"
#include "utlstring.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "convar.h"
#include "replay/ireplayserializeable.h"
#include "replay/ireplaycontext.h"
#include "replay/shared_defs.h"
#include "replay_dbg.h"
#include "vprof.h"
#include "fmtstr.h"
#include "UtlSortVector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

extern IEngineReplay *g_pEngine;

//----------------------------------------------------------------------------------------

template< class T >
class CGenericPersistentManager	: public CBaseThinker
{
public:
						CGenericPersistentManager();
	virtual				~CGenericPersistentManager();

	virtual bool		Init( bool bLoad = true );
	virtual void		Shutdown();

	virtual T			*Create() = 0;	// Create an object
	T					*CreateAndGenerateHandle();	// Creates a new object and generates a unique handle

	void				Add( T *pNewObj );	// Commit the object - NOTE: The Create*() functions don't call Add() for you
	void				Remove( ReplayHandle_t hObj );	// Remove() will remove the object, remove any .dmx associated with the object on disk, and usually delete attached files (like .dems or movies, etc, depending on what the manager implementation is)
	void				Remove( T *pObj );
	void				RemoveFromIndex( int it );
	void				Clear();	// Remove all objects - NOTE: Doesn't save right away
	
	bool				WriteObjToFile( T *pObj, const char *pFilename );	// Write object data to an arbitrary file
	bool				Save();	// Saves any unsaved data immediately

	void				FlagIndexForFlush();	// Mark index as dirty
	void				FlagForFlush( T *pObj, bool bForceImmediate );	// Mark an object as dirty
	void				FlagForUnload( T *pObj );	// Unload as soon as possible

	T					*Find( ReplayHandle_t hHandle );
	int					FindIteratorFromHandle( ReplayHandle_t hHandle );

	int					Count() const;
	bool				IsDirty( T *pNewObj );

	virtual void		Think();	// IReplayThinker implementation - NOTE: not meant to be called directly - called from think manager
	virtual const char	*GetIndexPath() const;	// Should return path where index file lives

private:
	class CLessFunctor
	{
	public:
		bool Less( const T *pSrc1, const T *pSrc2, void *pContext )
		{
			return pSrc1->GetHandle() < pSrc2->GetHandle();
		}
	};

public:
	typedef CUtlSortVector< T *, CLessFunctor >	ObjContainer_t;
	ObjContainer_t		m_vecObjs;

protected:
	// For derived classes to implement:
	virtual IReplayContext	*GetReplayContext() const = 0;
	virtual const char	*GetRelativeIndexPath() const = 0;	// Should return relative (to replay/client or replay/server) path where index file lives - NOTE: Last char should be a slash
	virtual const char	*GetIndexFilename() const = 0;	// Should return just the name of the file, e.g. "replays.dmx"
	virtual const char	*GetDebugName() const = 0;
	virtual bool		ShouldDeleteObjects() const { return true; }	// TODO: Used by Clear() - I'm not convinced this is needed yet though.
	virtual int			GetVersion() const = 0;
	virtual bool		ShouldSerializeToIndividualFiles() const { return true; }
	virtual bool		ShouldSerializeIndexWithFullPath()  const { return false; }
	virtual bool		ShouldLoadObj( const T *pObj ) const { return true; }
	virtual void		OnObjLoaded( T *pObj ) {}

	virtual int			GetHandleBase() const	{ return 0; }	// Subclass can implement this to provide a base/minimum for handles
	virtual void		PreLoad() {}

	const char			*GetIndexFullFilename() const;		// Should return the full path to the main .dmx file
	bool				HaveDirtyObjects() const;
	bool				HaveObjsToUnload() const;
	bool				ReadObjFromFile( const char *pFile, T *&pOut, bool bForceLoad );
	bool				Load();

	virtual float		GetNextThinkTime() const;	// IReplayThinker implementation
	void				FlushThink();
	void				UnloadThink();
	void				CreateIndexDir();
	void				ReadObjFromKeyValues( KeyValues *pObjData );
	T*					ReadObjFromKeyValues( KeyValues *pObjData, bool bForceLoad );
	bool				ReadObjFromFile( const char *pFile );
	void				UpdateHandleSeed( ReplayHandle_t hNewHandle );

	typedef CUtlLinkedList< T *, int > ListContainer_t;

	ReplayHandle_t		m_nHandleSeed;
	int					m_nVersion;
	bool				m_bIndexDirty;
	ListContainer_t		m_lstDirtyObjs;
	ListContainer_t		m_lstObjsToUnload;
	float				m_flNextFlushTime;
	float				m_flNextUnloadTime;
};

//----------------------------------------------------------------------------------------

template< class T >
bool CGenericPersistentManager< T >::Init( bool bLoad/*=true*/ )
{
	// Make directory structure is in place
	CreateIndexDir();

	// Initialize handle seed to start at base
	m_nHandleSeed = GetHandleBase();

	return bLoad ? Load() : true;
}

template< class T >
void CGenericPersistentManager< T >::Shutdown()
{
	Save();
}

template< class T >
CGenericPersistentManager< T >::CGenericPersistentManager()
:	m_nHandleSeed( 0 ),
	m_nVersion( -1 ),
	m_bIndexDirty( false ),
	m_flNextFlushTime( 0.0f ),
	m_flNextUnloadTime( 0.0f )
{
}
	
template< class T >
CGenericPersistentManager< T >::~CGenericPersistentManager()
{
	Clear();
}

template< class T >
void CGenericPersistentManager< T >::Clear()
{
	if ( ShouldDeleteObjects() )
	{
		m_vecObjs.PurgeAndDeleteElements();
	}
	else
	{
		m_vecObjs.RemoveAll();
	}

	// NOTE: This list contains pointers to objects in m_vecObjs, so no destruction of elements here
	m_lstDirtyObjs.RemoveAll();
	m_lstObjsToUnload.RemoveAll();
}

template< class T >
int CGenericPersistentManager< T >::Count() const
{
	return m_vecObjs.Count();
}

template< class T >
void CGenericPersistentManager< T >::FlagIndexForFlush()
{
	m_bIndexDirty = true;

	IF_REPLAY_DBG2( Warning( "%f %s: Index flagged\n", g_pEngine->GetHostTime(), GetDebugName() ) );
}

template< class T >
void CGenericPersistentManager< T >::FlagForFlush( T *pObj, bool bForceImmediate )
{
	if ( !pObj )
	{
		AssertMsg( 0, "Trying to flag a NULL object for flush." );
		return;
	}

	// Add to dirty list if it's not already there
	if ( m_lstDirtyObjs.Find( pObj ) == m_lstDirtyObjs.InvalidIndex() )
	{
		m_lstDirtyObjs.AddToTail( pObj );
	}

	IF_REPLAY_DBG2( Warning( "%f %s: Obj %s flagged for flush\n", g_pEngine->GetHostTime(), GetDebugName(), pObj->GetDebugName() ) );

	// Force write now?
	if ( bForceImmediate )
	{
		Save();
	}
}

template< class T >
void CGenericPersistentManager< T >::FlagForUnload( T *pObj )
{
	AssertMsg(
		ShouldSerializeToIndividualFiles(),
		"This functionality should only be used for managers that write to individual files, i.e. NOT managers that maintain one monolithic index."
	);

	if ( !pObj )
	{
		AssertMsg( 0, "Trying to flag a NULL object for unload." );
		return;
	}

	if ( m_lstObjsToUnload.Find( pObj ) == m_lstObjsToUnload.InvalidIndex() )
	{
		m_lstObjsToUnload.AddToTail( pObj );
	}

	IF_REPLAY_DBG2( Warning( "%f %s: Obj %s flagged for unload\n", g_pEngine->GetHostTime(), GetDebugName(), pObj->GetDebugName() ) );
}

template< class T >
bool CGenericPersistentManager< T >::IsDirty( T *pNewObj )
{
	return m_lstDirtyObjs.Find( pNewObj ) != m_lstDirtyObjs.InvalidIndex();
}

template< class T >
void CGenericPersistentManager< T >::Add( T *pNewObj )
{
	IF_REPLAY_DBG2( Warning( "Adding object with handle %i\n", pNewObj->GetHandle() ) );
	Assert( m_vecObjs.Find( pNewObj ) == m_vecObjs.InvalidIndex() );
	m_vecObjs.Insert( pNewObj );
	FlagIndexForFlush();
	FlagForFlush( pNewObj, false );
}

template< class T >
void CGenericPersistentManager< T >::Remove( ReplayHandle_t hObj )
{
	int itObj = FindIteratorFromHandle( hObj );
	if ( itObj == m_vecObjs.InvalidIndex() )
	{
		AssertMsg( 0, "Attemting to remove an object which does not exist." );
		return;
	}

	RemoveFromIndex( itObj );
}

template< class T >
void CGenericPersistentManager< T >::Remove( T *pObj )
{
	const int it = m_vecObjs.Find( pObj );

	if ( it != m_vecObjs.InvalidIndex() )
	{
		RemoveFromIndex( it );
	}
}

template< class T >
void CGenericPersistentManager< T >::RemoveFromIndex( int it )
{
	T *pObj = m_vecObjs[ it ];	// NOTE: Constant speed since the implementation of
								// CUtlLinkedList indexes into an array

	// Remove file associated w/ this object if necessary
	if ( ShouldSerializeToIndividualFiles() )
	{
		CUtlString strFullFilename = pObj->GetFullFilename();
		bool bSimulateDelete = false;
#if _DEBUG
		extern ConVar replay_fileserver_simulate_delete;
		bSimulateDelete = replay_fileserver_simulate_delete.GetBool();
#endif
		if ( g_pFullFileSystem->FileExists( strFullFilename.Get() ) && !bSimulateDelete )
		{
			g_pFullFileSystem->RemoveFile( strFullFilename.Get() );
		}
	}

	Assert( !pObj->IsLocked() );

	// Let the object do stuff before it gets deleted
	pObj->OnDelete();

	// If the object is in the dirty list, remove it - NOTE: this is safe
	m_lstDirtyObjs.FindAndRemove( pObj );

	// The object should not be in the 'objects-to-unload' list
	AssertMsg( m_lstObjsToUnload.Find( pObj ) == m_lstObjsToUnload.InvalidIndex(), "The object being removed was also in the unload list - is this OK?  If so, code should be added to remove from that list as well." );

	// Remove the object
	m_vecObjs.Remove( it );

	// Free the object
	delete pObj;

	FlagIndexForFlush();
}

template< class T >
T *CGenericPersistentManager< T >::Find( ReplayHandle_t hHandle )
{
	FOR_EACH_VEC( m_vecObjs, i )
	{
		T *pCurObj = m_vecObjs[ i ];
		if ( hHandle == pCurObj->GetHandle() )
		{
			return pCurObj;
		}
	}

	return NULL;
}

template< class T >
int CGenericPersistentManager< T >::FindIteratorFromHandle( ReplayHandle_t hHandle )
{
	FOR_EACH_VEC( m_vecObjs, i )
	{
		T *pCurObj = m_vecObjs[ i ];
		if ( hHandle == pCurObj->GetHandle() )
		{
			return i;
		}
	}

	return m_vecObjs.InvalidIndex();
}

template< class T >
bool CGenericPersistentManager< T >::Load()
{
	bool bResult = true;

	Clear();
	PreLoad();

	const char *pFullFilename = GetIndexFullFilename();

	// Attempt to load from disk
	KeyValuesAD pRoot( pFullFilename );
	if ( pRoot->LoadFromFile( g_pFullFileSystem, pFullFilename ) )
	{
		// Get file format version
		m_nVersion = pRoot->GetInt( "version", -1 );
		if ( m_nVersion != GetVersion() )
		{
			Warning( "File (%s) has old format (%i).\n", pFullFilename, m_nVersion );
		}

		// Read from individual files?
		if ( ShouldSerializeToIndividualFiles() )
		{
			KeyValues *pFileIndex = pRoot->FindKey( "files" );
			if ( pFileIndex )
			{
				FOR_EACH_VALUE( pFileIndex, pValue )
				{
					const char *pName = pValue->GetName();
					if ( !ReadObjFromFile( pName ) )
					{
						Warning( "Failed to load data from file, \"%s\"\n", pName );
					}
				}
			}
			else
			{
				// Peek in directory and load files based on what's there
				CFmtStr fmtPath( "%s*.%s", GetIndexPath(), GENERIC_FILE_EXTENSION );
				FileFindHandle_t hFind;
				const char *pFilename = g_pFullFileSystem->FindFirst( fmtPath.Access(), &hFind );
				while ( pFilename )
				{
					// Ignore index file
					if ( V_stricmp( pFilename, GetIndexFilename() ) )
					{
						if ( !ReadObjFromFile( pFilename ) )
						{
							Warning( "Failed to load data from file, \"%s\"\n", pFilename );
						}
					}

					pFilename = g_pFullFileSystem->FindNext( hFind );
				}
			}
		}
		else
		{
			FOR_EACH_TRUE_SUBKEY( pRoot, pObjSubKey )
			{
				// Read data
				m_vecObjs.Insert( ReadObjFromKeyValues( pObjSubKey, false ) );
			}
		}

		// Let derived class do any per-object processing.
		FOR_EACH_VEC( m_vecObjs, i )
		{
			OnObjLoaded( m_vecObjs[ i ] );
		}
	}

	return bResult;
}

template< class T >
bool CGenericPersistentManager< T >::WriteObjToFile( T *pObj, const char *pFilename )
{
	// Create a keyvalues for the object
	KeyValuesAD pObjData( pObj->GetSubKeyTitle() );

	// Fill the keyvalues w/ data
	pObj->Write( pObjData );

	// Attempt to save the current object data to a separate file
	if ( !pObjData->SaveToFile( g_pFullFileSystem, pFilename ) )
	{
		Warning( "Failed to write file %s\n", pFilename );
		return false;
	}

	return true;
}

template< class T >
bool CGenericPersistentManager< T >::Save()
{
	IF_REPLAY_DBG2( Warning( "%f %s: Saving now...\n", g_pEngine->GetHostTime(), GetDebugName() ) );

	bool bResult = true;

	// Add subkey for movies
	KeyValuesAD pRoot( "root" );

	// Write format version
	pRoot->SetInt( "version", GetVersion() );
	
	// Write a file index instead of adding subkeys to the root?
	if ( ShouldSerializeToIndividualFiles() )
	{
		// Go through each object in the dirty list and write to a separate file
		FOR_EACH_LL( m_lstDirtyObjs, i )
		{
			T *pCurObj = m_lstDirtyObjs[ i ];

			// Write to the file
			bResult = bResult && WriteObjToFile( pCurObj, pCurObj->GetFullFilename() );
		}
	}

	// Write all objects to one monolithic file - writes all objects (ignores "dirtyness")
	else
	{
		FOR_EACH_VEC( m_vecObjs, i )
		{
			T *pCurObj = m_vecObjs[ i ];

			// Create a keyvalues for the object
			KeyValues *pCurObjData = new KeyValues( pCurObj->GetSubKeyTitle() );

			// Fill the keyvalues w/ data
			pCurObj->Write( pCurObjData );

			// Add as a subkey to the root keyvalues
			pRoot->AddSubKey( pCurObjData );
		}
	}

	// Clear the dirty list
	m_lstDirtyObjs.RemoveAll();

	// Write the index file if dirty
	if ( m_bIndexDirty )
	{
		return bResult && pRoot->SaveToFile( g_pFullFileSystem, GetIndexFullFilename() );
	}

	return bResult;
}

template< class T >
T *CGenericPersistentManager< T >::CreateAndGenerateHandle()
{
	T *pNewObj = Create();
	pNewObj->SetHandle( m_nHandleSeed++ );		Assert( Find( pNewObj->GetHandle() ) == NULL );
	FlagIndexForFlush();
	return pNewObj;
}

template< class T >
float CGenericPersistentManager< T >::GetNextThinkTime() const
{
	// Always think
	return 0.0f;
}

template< class T >
void CGenericPersistentManager< T >::Think()
{
	VPROF_BUDGET( "CGenericPersistentManager::Think", VPROF_BUDGETGROUP_REPLAY );

	CBaseThinker::Think();

	FlushThink();
	UnloadThink();
}

template< class T >
void CGenericPersistentManager< T >::FlushThink()
{
	const float flHostTime = g_pEngine->GetHostTime();
	bool bTimeToFlush = flHostTime >= m_flNextFlushTime;
	if ( !bTimeToFlush || ( !m_bIndexDirty && !HaveDirtyObjects() ) )
		return;

	// Flush now and clear dirty objects
	Save();

	// Reset
	m_bIndexDirty = false;

	// Setup next flush think
	extern ConVar replay_flushinterval;
	m_flNextFlushTime = flHostTime + replay_flushinterval.GetInt();
}

template< class T >
void CGenericPersistentManager< T >::UnloadThink()
{
	const float flHostTime = g_pEngine->GetHostTime();
	bool bTimeToUnload = flHostTime >= m_flNextUnloadTime;
	if ( !bTimeToUnload || !HaveObjsToUnload() )
		return;

	// Unload objects now
	FOR_EACH_LL( m_lstObjsToUnload, i )
	{
		T *pObj = m_lstObjsToUnload[ i ];
		
		// If the object has been marked as locked, don't unload it.
		if ( pObj->IsLocked() )
			continue;

		// If we're waiting to flush the file, don't unload it yet
		if ( IsDirty( pObj ) )
			continue;

		// Let the object do stuff before it gets deleted
		pObj->OnUnload();

		// Remove the object
		m_vecObjs.FindAndRemove( pObj );

		IF_REPLAY_DBG( Warning( "Unloading object %s\n", pObj->GetDebugName() ) );

		// Free the object
		delete pObj;
	}

	// Clear the list
	m_lstObjsToUnload.RemoveAll();

	// Think once a second
	m_flNextUnloadTime = flHostTime + 1.0f;
}

template< class T >
const char	*CGenericPersistentManager< T >::GetIndexPath() const
{
	return Replay_va( "%s%s", GetReplayContext()->GetBaseDir(), GetRelativeIndexPath() );
}

template< class T >
const char *CGenericPersistentManager< T >::GetIndexFullFilename() const		// Should return the full path to the main .dmx file
{
	return Replay_va( "%s%s", GetIndexPath(), GetIndexFilename() );
}

template< class T >
bool CGenericPersistentManager< T >::HaveDirtyObjects() const
{
	return m_lstDirtyObjs.Count() > 0;
}

template< class T >
bool CGenericPersistentManager< T >::HaveObjsToUnload() const
{
	return m_lstObjsToUnload.Count() > 0;
}

template< class T >
void CGenericPersistentManager< T >::CreateIndexDir()
{
	g_pFullFileSystem->CreateDirHierarchy( GetIndexPath(), "DEFAULT_WRITE_PATH" );
}

template< class T >
bool CGenericPersistentManager< T >::ReadObjFromFile( const char *pFile, T *&pOut, bool bForceLoad )
{
	// Use the full path and filename specified, or construct it if necessary
	CUtlString strFullFilename;
	if ( ShouldSerializeIndexWithFullPath() )
	{
		strFullFilename = pFile;
	}
	else
	{
		strFullFilename.Format( "%s%s", GetIndexPath(), pFile );
	}

	// Attempt to load the file
	KeyValuesAD pObjData( pFile );
	if ( !pObjData->LoadFromFile( g_pFullFileSystem, strFullFilename.Get() ) )
	{
		Warning( "Failed to load from file %s\n", strFullFilename.Get() );
		AssertMsg( 0, "Manager failed to load something..." );
		return false;
	}

	// Create and read a new object
	pOut = ReadObjFromKeyValues( pObjData, bForceLoad );
	if ( !pOut )
		return NULL;
	
	// Add the object to the manager
	m_vecObjs.Insert( pOut );

	return true;
}

template< class T >
bool CGenericPersistentManager< T >::ReadObjFromFile( const char *pFile )
{
	T *pNewObj;
	if ( !ReadObjFromFile( pFile, pNewObj, false ) )
		return false;

	return true;
}

template< class T >
T* CGenericPersistentManager< T >::ReadObjFromKeyValues( KeyValues *pObjData, bool bForceLoad )
{
	T *pNewObj = Create();			Assert( pNewObj );
	if ( !pNewObj )
		return NULL;

	// Attempt to read data for the object, and fail to load this particular object if the reader
	// says we should.
	if ( !pNewObj->Read( pObjData ) )
	{
		delete pNewObj;
		return NULL;
	}

	// This object OK to load?  Only check if bForceLoad is false.
	if ( !bForceLoad && !ShouldLoadObj( pNewObj ) )
	{
		delete pNewObj;
		return NULL;
	}

	// Sync up handle seed
	UpdateHandleSeed( pNewObj->GetHandle() );

	return pNewObj;
}

template< class T >
void CGenericPersistentManager< T >::UpdateHandleSeed( ReplayHandle_t hNewHandle )
{
	m_nHandleSeed = (ReplayHandle_t)( GetHandleBase() + MAX( (uint32)m_nHandleSeed, (uint32)hNewHandle ) + 1 );

#ifdef _DEBUG
	FOR_EACH_VEC( m_vecObjs, i )
	{
		AssertMsg( m_nHandleSeed != m_vecObjs[ i ]->GetHandle(), "Handle seed collision!" );
	}
#endif
}

//----------------------------------------------------------------------------------------

#define FOR_EACH_OBJ( _manager, _i )	FOR_EACH_VEC( _manager->m_vecObjs, _i )

//----------------------------------------------------------------------------------------

#endif	// GENERICPERSISTENTMANAGER_H

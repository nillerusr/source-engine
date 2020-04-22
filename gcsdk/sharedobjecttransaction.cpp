//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "stdafx.h"

#include "sharedobjecttransaction.h"

#include "sqlaccess/sqlaccess.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{	

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	class CTrustedHelper_OutputAndSetErrorState
	{
	public:
		CTrustedHelper_OutputAndSetErrorState( CSharedObjectTransactionEx& SharedObjectTransaction, const char *pszFunctionContext, const CSteamID& CacheOwnerSteamID, const CSharedObject *pObject )
			: m_SharedObjectTransaction( SharedObjectTransaction )
			, m_pszFunctionContext( pszFunctionContext )
			, m_CacheOwnerSteamID( CacheOwnerSteamID )
			, m_pObject( pObject )
		{
		}

		void operator()( const bool bExpResult, const char *pszExp ) const
		{
			if ( !bExpResult )
			{
				AssertMsg4( bExpResult, "Failed verification: %s (context '%s'; owner '%s'; object '%s')", pszExp, m_pszFunctionContext, m_CacheOwnerSteamID.Render(), m_pObject ? m_pObject->GetDebugString().String() : "[none]" );
				m_SharedObjectTransaction.SetErrorState();
			}
		}

	private:
		CSharedObjectTransactionEx& m_SharedObjectTransaction;
		const char *m_pszFunctionContext;
		const CSteamID& m_CacheOwnerSteamID;
		const CSharedObject *m_pObject;
	};

	#define CSOTVerifyBase( exp_, obj_ ) \
		CVerifyIfTrustedHelper( CTrustedHelper_OutputAndSetErrorState( *this, __FUNCTION__, m_pLockedSOCache->GetOwner(), obj_ ), (exp_), #exp_ ).GetResult()

	#define CSOTVerify( exp_ ) \
		CSOTVerifyBase( exp_, NULL )

	#define CSOTVerifyObj( exp_ ) \
		CSOTVerifyBase( exp_, pObject )

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	CSharedObjectTransactionEx::CSharedObjectTransactionEx( CGCSharedObjectCache *pLockedSOCache, const char *pszTransactionName )
		: m_pLockedSOCache( pLockedSOCache )
		, m_pSQLAccess( &m_sqlAccessInternal )
	{
		Assert( pszTransactionName );
		Assert( pszTransactionName[0] );
		Assert( m_pLockedSOCache );

		// We have to start a SQL transaction no matter what. If we don't do this, then
		// any code internally or externally that tries to add operations to the open transaction
		// will instead run immediately. We Verify() here because we know the only thing that
		// can fail beginning a new transaction is to already be in a transaction, and that
		// can't be the case because we just made this object.
		m_bTransactionBuildSuccess = m_pSQLAccess->BBeginTransaction( pszTransactionName );
		Verify( m_bTransactionBuildSuccess );

		// Grab another lock on our user that owns the cache we got passed in. This means that,
		// barring any maliscious or really terrible code happening on the outside, even if our
		// calling code unlocks *their* lock, we'll still have ours and can access the cache safely.
		Verify( GGCBase()->BLockSteamIDImmediate( m_pLockedSOCache->GetOwner() ) );

		// Because we just constructed a fresh object, we want to guarantee that our internal state
		// is consistent. This way, either we're guaranteed to fail construction or we know moving
		// forward that we started in a good place so anything that's wrong since then is some kind
		// of real programmer error.
		Verify( BIsValidInternalState() );
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	CSharedObjectTransactionEx::~CSharedObjectTransactionEx()
	{
		// If we fall off the stack and we haven't been submitted, manually rollback whatever work
		// we queued up in SQL and free up our local storage. This isn't an error.
		if ( m_pSQLAccess->BInTransaction() )
		{
			Rollback();
		}

		// We're finally done manipulating this user's cache. If we're practicing best
		// practices for locking, this will still leave us locked in our outer scope. If we're
		// not, at least we'll be safe.
		GGCBase()->UnlockSteamID( m_pLockedSOCache->GetOwner() );
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	bool CSharedObjectTransactionEx::BIsValidInternalState()
	{
		// Sanity check basic internal data.
		Assert( m_pSQLAccess );

		// If we've done something bad with our transaction (ie., committed from outside, or
		// tried to submit through this object) and we're still doing work, that's a case we
		// can't handle.
		if ( !CSOTVerify( m_pSQLAccess->BInTransaction() ) )
			return false;
		
		// Verify we have a cache and it's a cache we understand how to manipulate.
		if ( !CSOTVerify( m_pLockedSOCache ) || !CSOTVerify( m_pLockedSOCache->GetOwner().IsValid() ) || !CSOTVerify( m_pLockedSOCache->GetOwner().BIndividualAccount() ) )
			return false;

		// Transactions can yield when trying to commit, so we need to be running a job. We also want
		// to be paranoid and make sure that the lock is actively held by our current job.
		AssertRunningJob();

		if ( !CSOTVerify( GGCBase()->IsSteamIDLockedByJob( m_pLockedSOCache->GetOwner(), &GJobCur() ) ) )
			return false;

		// Internal state is such that we can at least try to perform operations, at least.
		return true;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	bool CSharedObjectTransactionEx::BIsValidInput( const CSharedObject *pObject )
	{
		if ( !BIsValidInternalState() )
			return false;

		if ( !CSOTVerifyObj( pObject ) )
			return false;

		// Make sure we have a valid type ID for this object. There aren't any objects that return negative
		// IDs, but if we pass in a deleted or bogus pointer, we'll probably crash here when we hit the vtable.
		if ( !CSOTVerifyObj( pObject->GetTypeID() > 0 ) )
			return false;
		
		return true;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	bool CSharedObjectTransactionEx::BTrackModifiedObjectInternal( CSharedObject *pObject, CSharedObject **out_ppWritableObject )
	{
		if ( !BIsValidInput( pObject ) )
			return false;

		if ( !CSOTVerifyObj( out_ppWritableObject ) )
			return false;

		// If an object is in the added list, we're going to do a full write so modifying it at this point adds nothing
		// new. We'll return the current only version of it as our writable object.
		{
			const CreateOrDestroyCommitInfo_t *pInfo = InternalFindCommitInfo( pObject, m_vecObjects_Added );
			if ( pInfo )
			{
				*out_ppWritableObject = pInfo->m_pObject;
				return true;
			}
		}
		
		// If an object is in the modified list, we've already made a copy that we can make modifications to, so we'll
		// return that copy.
		{
			const ModifyCommitInfo_t *pInfo = InternalFindCommitInfo( pObject, m_vecObjects_Modified );
			if ( pInfo )
			{
				*out_ppWritableObject = pInfo->m_pWriteableObject;
				return true;
			}
		}

		// We aren't already tracking this object. We're acting as if we've never seen it before, so first make sure that
		// we're in the cache we think we're in.
		if ( !CSOTVerify( m_pLockedSOCache->FindSharedObject( *pObject ) ) )
			return false;

		// Make a copy of our current state that we can make modifications to and track the association.
		*out_ppWritableObject = CSharedObject::Create( pObject->GetTypeID() );
		(*out_ppWritableObject)->Copy( *pObject );
		m_vecObjects_Modified.AddToTail( ModifyCommitInfo_t( *out_ppWritableObject, pObject ) );
		return true;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	bool CSharedObjectTransactionEx::BAddNewObjectInternal( CSharedObject *pObject )
	{
		// Make sure we pass basic sanity check measures for our inputs (ie., we have inputs, we have appropriate
		// locks to manipulate those inputs, etc.).
		if ( !BIsValidInput( pObject ) )
			return false;

		// Make sure this object isn't already in this cache. This can cause problems during rollback if we didn't
		// really add it as part of the transaction and remove it when a transaction fails.
		if ( !CSOTVerifyObj( m_pLockedSOCache->FindSharedObject( *pObject ) == NULL ) )
			return false;

		// Make sure this object isn't already in the list of objects we're adding as part of this transaction.
		// Having the object in the list multiple times is potentially harmless, but it probably indicates some
		// calling code is doing something we don't expect.
		if ( !CSOTVerifyObj( InternalFindCommitInfo( pObject, m_vecObjects_Added ) == NULL ) )
			return false;

		// Success.
		m_vecObjects_Added.AddToTail( CreateOrDestroyCommitInfo_t( pObject ) );
		return true;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	bool CSharedObjectTransactionEx::BRemoveObjectInternal( CSharedObject *pObject )
	{
		// Make sure we pass basic sanity check measures for our inputs (ie., we have inputs, we have appropriate
		// locks to manipulate those inputs, etc.).
		if ( !BIsValidInput( pObject ) )
			return false;

		// Make sure the object we're removing is in the cache we're trying to remove it from.
		if ( !CSOTVerifyObj( m_pLockedSOCache->FindSharedObject( *pObject ) ) )
			return false;

		// Look through our lists of objects that we're adding and objects that we're modifying. If we're
		// removing an object that we're adding in the same transaction through the same pointer, this will
		// result in a broken SO cache. Removing a modified object may or may not be safe but it's probably
		// indicative of a higher-level logic bug regardless.
		if ( !CSOTVerifyObj( InternalFindCommitInfo( pObject, m_vecObjects_Added ) == NULL ) )
			return false;

		if ( !CSOTVerifyObj( InternalFindCommitInfo( pObject, m_vecObjects_Modified ) == NULL ) )
			return false;

		// Success.
		m_vecObjects_Removed.AddToTail( CreateOrDestroyCommitInfo_t( pObject ) );
		return true;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	const CSharedObject *CSharedObjectTransactionEx::InternalFindSharedObject( CGCSharedObjectCache *pSOCache, const CSharedObject& soIndex ) const
	{
		FOR_EACH_VEC( m_vecObjects_Modified, i )
		{
			auto& info = m_vecObjects_Modified[i];
			if ( info.m_pObject->BIsKeyEqual( soIndex ) )
				return info.m_pObject;
		}

		FOR_EACH_VEC( m_vecObjects_Added, i )
		{
			auto& info = m_vecObjects_Added[i];
			if ( info.m_pObject->BIsKeyEqual( soIndex ) )
				return info.m_pObject;
		}

		return NULL;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	const char *CSharedObjectTransactionEx::InternalPreCommit()
	{
		// ...
		if ( !BIsValidInternalState() )
			return "invalid internal state";
		
		// ...
		if ( !CSOTVerify( m_pSQLAccess->BInTransaction() ) )
			return "transaction closed before commit";

		// Did we run into some error internally building this transaction? This doesn't assert because it
		// could be a SQL error or something else that doesn't necessarily indicate a problem with the way
		// we're using this class. It *probably* indicates the calling code has a problem, but it isn't
		// guaranteed so we don't assert.
		if ( !m_bTransactionBuildSuccess )
			return "error(s) building transaction";

		// Nothing wrong so far.
		return NULL;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	bool CSharedObjectTransactionEx::BYieldingCommit()
	{
		const char *pszPreCommitFailureDesc = InternalPreCommit();
		if ( pszPreCommitFailureDesc )
		{
			// We can't spit out information about which cache we're dealing with here because it's possible at
			// this point that the error we're displaying is "we don't have the cache anymore!" which means pulling
			// memory is probably unsafe.
			EmitError( SPEW_GC, "Failed to commit CSharedObjectTransaction '%s': %s!\n", GetInternalTransactionDesc(), pszPreCommitFailureDesc );

			Rollback();
			return false;
		}

		bool bNetworkRelevantObjectsChanged = false;
		bool bDBRelevantObjectsChanged = false;
		bool bGeneratedCommitSQL = true;

		// Add insert statements to SQL access instance for everything that needs to be written to the
		// database. Some types only exist in memory and have no database backing.
		FOR_EACH_VEC( m_vecObjects_Added, i )
		{
			CreateOrDestroyCommitInfo_t& info = m_vecObjects_Added[i];
			
			if ( info.m_pObject->BIsDatabaseBacked() )
			{
				bGeneratedCommitSQL &= info.m_pObject->BYieldingAddInsertToTransaction( *m_pSQLAccess );
				bDBRelevantObjectsChanged |= true;
			}

			bNetworkRelevantObjectsChanged |= info.m_pObject->BIsNetworked();
		}

		// For every object that we've changed state on, we've also been tracking information on which
		// fields we modified. Here we ask each of the modified objects (which aren't currently in the
		// SO cache, but are clones living outside of it) to queue up their updates to the SQL transaction.
		{
			CUtlVector< int > vecDirtyFields;

			FOR_EACH_VEC( m_vecObjects_Modified, i )
			{
				const ModifyCommitInfo_t& info = m_vecObjects_Modified[i];

				Assert( info.m_pObject );
				Assert( info.m_pWriteableObject );
				Assert( info.m_pObject != info.m_pWriteableObject );

				if ( info.m_pWriteableObject->BIsDatabaseBacked() )
				{
					vecDirtyFields.RemoveAll();
					m_SODirtyList.GetDirtyFieldSetByObj( info.m_pWriteableObject, vecDirtyFields );

					bGeneratedCommitSQL &= info.m_pWriteableObject->BYieldingAddWriteToTransaction( *m_pSQLAccess, vecDirtyFields );
					bDBRelevantObjectsChanged |= true;
				}

				bNetworkRelevantObjectsChanged |= info.m_pWriteableObject->BIsNetworked();

				AssertMsg4( info.m_pWriteableObject->BIsDatabaseBacked() == info.m_pObject->BIsDatabaseBacked(),
							"Disagreement over DB backing state between SOs '%s' and '%s' in transaction '%s' for user '%s'!",
							info.m_pObject->GetDebugString().String(),
							info.m_pWriteableObject->GetDebugString().String(),
							GetInternalTransactionDesc(),
							m_pLockedSOCache->GetOwner().Render() );
				AssertMsg4( info.m_pWriteableObject->BIsNetworked() == info.m_pObject->BIsNetworked(),
							"Disagreement over network state between SOs '%s' and '%s' in transaction '%s' for user '%s'!",
							info.m_pObject->GetDebugString().String(),
							info.m_pWriteableObject->GetDebugString().String(),
							GetInternalTransactionDesc(),
							m_pLockedSOCache->GetOwner().Render() );
			}
		}

		// Have each object that we'd like to remove queue up the SQL work necessary to do so.
		FOR_EACH_VEC( m_vecObjects_Removed, i )
		{
			CreateOrDestroyCommitInfo_t& info = m_vecObjects_Removed[i];

			if ( info.m_pObject->BIsDatabaseBacked() )
			{
				bGeneratedCommitSQL &= info.m_pObject->BYieldingAddRemoveToTransaction( *m_pSQLAccess );
				bDBRelevantObjectsChanged |= true;
			}

			// We don't have to update network state here as removes are sent immediately to the client
			// before we've even flushed our dirty updates.
		}

		// Our "did we generate the SQL to do this work in the DB" variable starts off true, so the only
		// way we'd expect it to be false here is if we attempted to do work above and ran into some errors.
		// If we don't have any SQL work to do at all, for example if we're only adding/modifying/removing
		// memory-only items, then "bGeneratedCommitSQL" will be true and "bDBRelevantObjectsChanged" will
		// be false.
		if ( !bGeneratedCommitSQL )
		{
			EmitError( SPEW_GC, "Failed to commit CSharedObjectTransaction '%s' for user '%s': failed to add inserts/writes.\n", GetInternalTransactionDesc(), m_pLockedSOCache->GetOwner().Render() );

			Rollback();
			return false;
		}

		// Try to commit DB transaction. We don't know for sure whether we're the only code that's adding commands
		// to this transaction, so we can't completely skip this if we didn't do any SQL work internally. We can
		// say "did we do anything internally?; if not, maybe we'll be empty".
		if ( !m_pSQLAccess->BCommitTransaction( !bDBRelevantObjectsChanged ) )
		{
			EmitError( SPEW_GC, "Failed to commit CSharedObjectTransaction '%s' for user '%s': SQL transaction failure.\n", GetInternalTransactionDesc(), m_pLockedSOCache->GetOwner().Render() );

			Rollback();
			return false;
		}

		// The database work committed successfully, so we update our memory state to match the state we just wrote
		// to the DB.
		FOR_EACH_VEC( m_vecObjects_Added, i )
		{
			CreateOrDestroyCommitInfo_t& info = m_vecObjects_Added[i];
			m_pLockedSOCache->AddObject( info.m_pObject );									// internally will assert if cache isn't locked
		}

		FOR_EACH_VEC( m_vecObjects_Modified, i )
		{
			ModifyCommitInfo_t& info = m_vecObjects_Modified[i];
			info.m_pObject->Copy( *info.m_pWriteableObject );								// stomp the version that's already in the cache with the properties from where we've been writing
			delete info.m_pWriteableObject;
		}

		FOR_EACH_VEC( m_vecObjects_Removed, i )
		{
			CreateOrDestroyCommitInfo_t &info = m_vecObjects_Removed[i];
			Verify( m_pLockedSOCache->BDestroyObject( *info.m_pObject, false ) );			// internally will assert if cache isn't locked
		}

		// Did we change anything that affected network state? If so, tell our cache to flush the dirty
		// state list.
		if ( bNetworkRelevantObjectsChanged )
		{
			// Our cache is responsible for sending all the network updates, including creates from the
			// AddObject() calls above, so now that our commit has succeeded we copy over our list of
			// dirty objects from inside this transaction.
			for ( const ModifyCommitInfo_t& info : m_vecObjects_Modified )
			{
				m_pLockedSOCache->DirtyNetworkObject( info.m_pObject );
			}

			m_pLockedSOCache->SendAllNetworkUpdates();
		}

		// Cleanup.
		m_vecObjects_Added.RemoveAll();
		m_vecObjects_Modified.RemoveAll();
		m_vecObjects_Removed.RemoveAll();
	
		return true;
	}

	void CSharedObjectTransactionEx::Rollback()
	{
		// Clean up any memory allocated to handle any database work that is outstanding but not yet
		// committed.
		m_pSQLAccess->RollbackTransaction();
		Assert( !m_pSQLAccess->BInTransaction() );

		// Clean up any in-memory changes that are currently outstanding.
		
		// We made new objects for our adds, so we need to free up that memory.
		FOR_EACH_VEC( m_vecObjects_Added, i )
		{
			delete m_vecObjects_Added[i].m_pObject;
		}
		m_vecObjects_Added.RemoveAll();

		// For our modifies, we haven't done any work on the versions that are in the cache, so all we have
		// to do is delete the temp memory that we allocated for a writable version.
		FOR_EACH_VEC( m_vecObjects_Modified, i )
		{
			ModifyCommitInfo_t& info = m_vecObjects_Modified[i];
			delete info.m_pWriteableObject;
		}
		m_vecObjects_Modified.RemoveAll();

		// We didn't actually do any in-memory work for our items that were set to be deleted, so we
		// can just free the memory for our tracking state.
		m_vecObjects_Removed.RemoveAll();
	}















	CSharedObjectTransaction::CSharedObjectTransaction( CSQLAccess &sqlAccess, const char *pName )
		: m_sqlAccess( sqlAccess )
	{
		if ( m_sqlAccess.BInTransaction() == false )
		{
			m_sqlAccess.BBeginTransaction( pName );
		}		
	}

	CSharedObjectTransaction::~CSharedObjectTransaction()
	{
		Rollback();
	}

	CSharedObjectTransaction::undoinfo_t *CSharedObjectTransaction::FindObjectInVector( const CSharedObject *pObject, CUtlVector<undoinfo_t> &vec ) const
	{
		FOR_EACH_VEC( vec, i )
		{
			if ( vec[i].pObject == pObject )
				return &vec[i];
		}

		return NULL;
	}

	bool CSharedObjectTransaction::AssertValidInput( const CGCSharedObjectCache *pSOCache, const CSharedObject *pObject, const char *pszContext )
	{
		Assert( pszContext );

		Assert( pSOCache );
		Assert( pObject );
		if ( pSOCache == NULL || pObject == NULL )
		{
			SetError( CFmtStr( "%s: attempt to manipulate invalid SO cache %s, object %s", pszContext, pSOCache ? pSOCache->GetOwner().Render() : "[none]", pObject ? pObject->GetDebugString().String() : "[none]" ) );
			return false;
		}

		const bool bSOCachedLocked = GGCBase()->IsSteamIDLockedByCurJob( pSOCache->GetOwner() );
		Assert( bSOCachedLocked );
		if ( !bSOCachedLocked )
		{
			SetError( CFmtStr( "%s: attempt to manipulate non-locked SO cache %s to add object %s", pszContext, pSOCache->GetOwner().Render(), pObject->GetDebugString().String() ) );
			return false;
		}

		return pSOCache != NULL && pObject != NULL && bSOCachedLocked;
	}

	void CSharedObjectTransaction::AddManagedObject( CGCSharedObjectCache *pSOCache, CSharedObject *pObject )
	{
		if ( !AssertValidInput( pSOCache, pObject, "AddManagedObject()" ) )
			return;

		// if an object is in the added list, we're going to do a full write so modifying it at this point adds nothing
		// new; if an object is in the modified list, we already tracked the initial state; either way we have no new
		// data to track here
		if ( FindObjectInVector( pObject, m_vecObjects_Added ) || FindObjectInVector( pObject, m_vecObjects_Modified ) )
			return;

		undoinfo_t info = { pObject, pSOCache, NULL };
		info.pOriginalCopy = CSharedObject::Create( pObject->GetTypeID() );
		info.pOriginalCopy->Copy( *pObject );
		m_vecObjects_Modified.AddToTail( info );
	}

	void CSharedObjectTransaction::AddNewObject( CGCSharedObjectCache *pSOCache, CSharedObject *pObject )
	{
		if ( !AssertValidInput( pSOCache, pObject, "AddNewObject()" ) )
			return;

		if ( FindObjectInVector( pObject, m_vecObjects_Added ) )
			return;

		undoinfo_t info = { pObject, pSOCache, NULL };
		m_vecObjects_Added.AddToTail( info );
	}

	void CSharedObjectTransaction::RemoveObject( CGCSharedObjectCache *pSOCache, CSharedObject *pObject )
	{
		if ( !AssertValidInput( pSOCache, pObject, "RemoveObject()" ) )
			return;

		// make sure the object we're removing is in the cache we're trying to remove it from.
		AssertMsg1( pSOCache->FindSharedObject( *pObject ), "Attempting to remove object '%s' from non-owning cache!", pObject->GetDebugString().Get() );

		// look through our lists of objects that we're adding and objects that we're modifying. If we're
		// removing an object that we're adding in the same transaction through the same pointer, this will
		// result in a broken SO cache. Removing a modified object may or may not be safe but it's probably
		// indicative of a higher-level logic bug regardless.
		if ( undoinfo_t *pInfo = FindObjectInVector( pObject, m_vecObjects_Added ) )
		{
			EmitError( SPEW_GC, "Attempting to add and remove the same object from the same CSharedObjectTransaction! Object: %s", pInfo->pObject->GetDebugString().Get() );
			Assert( !"Attempting to add and remove the same object from the same CSharedObjectTransaction!" );
			m_vecObjects_Added.FindAndFastRemove( *pInfo );
		}
		if ( undoinfo_t *pInfo = FindObjectInVector( pObject, m_vecObjects_Modified ) )
		{
			EmitError( SPEW_GC, "Attempting to modify and remove the same object from the same CSharedObjectTransaction! Object: %s", pInfo->pObject->GetDebugString().Get() );
			Assert( !"Attempting to modify and remove the same object from the same CSharedObjectTransaction!" );
			m_vecObjects_Modified.FindAndFastRemove( *pInfo );
		}

		// @note Tom Bui: the act of removing an item may change the object, so to roll that back,
		// we need the original version
		undoinfo_t info = { pObject, pSOCache, NULL };
		info.pOriginalCopy = CSharedObject::Create( pObject->GetTypeID() );
		info.pOriginalCopy->Copy( *pObject );
		m_vecObjects_Removed.AddToTail( info );

		if ( !pObject->BYieldingAddRemoveToTransaction( m_sqlAccess ) )
		{
			SetError( "RemoveObject(): BYieldingAddRemoveToTransaction() failed" );
			return;
		}
	}

	void CSharedObjectTransaction::ModifiedObject( CGCSharedObjectCache *pSOCache, CSharedObject *pObject, uint32 unFieldIdx )
	{
		if ( !AssertValidInput( pSOCache, pObject, "ModifiedObject()" ) )
			return;

		// look for an object in the transaction -- this might be a new object created for this
		// transaction or it might be an object we've already tagged for modification; we don't
		// use FindSharedObject() for this because we might not be in an SO cache yet

		// if we're in the add list, we aren't intended to be in a cache yet, and so we also don't
		// have to dirty any fields -- we don't exist for real yet so when we finalize this transaction,
		// effectively *everything* is dirty
		if ( FindObjectInVector( pObject, m_vecObjects_Added ) )
			return;

		// make sure the object we're removing is in the cache we think it is. This check has to happen
		// after the "is in the added list?" check above because we won't actually put items in the cache
		// for real until after the SQL transaction succeeds
		AssertMsg1( pSOCache->FindSharedObject( *pObject ), "Attempting to modify object '%s' in non-owning cache!", pObject->GetDebugString().Get() );

		undoinfo_t *pInfo = FindObjectInVector( pObject, m_vecObjects_Modified );
		if ( pInfo )
		{
			pInfo->pSOCache->DirtyObjectField( pObject, unFieldIdx );
			return;
		}

		Assert( !"Attempt to modify an unmanaged object in CSharedObjectTransaction!" );
		SetError( CFmtStr( "ModifiedObject(): attempt to modify an unmanaged object %s", pObject->GetDebugString().String() ) );
	}

	CSharedObject *CSharedObjectTransaction::FindSharedObject( CGCSharedObjectCache *pSOCache, const CSharedObject &soIndex )
	{
		// search in modified objects
		FOR_EACH_VEC( m_vecObjects_Modified, i )
		{
			undoinfo_t &info = m_vecObjects_Modified[i];
			if ( info.pSOCache == pSOCache && info.pObject->BIsKeyEqual( soIndex ) )
				return info.pObject;
		}

		// search in new objects
		FOR_EACH_VEC( m_vecObjects_Added, i )
		{
			undoinfo_t &info = m_vecObjects_Added[i];
			if ( info.pSOCache == pSOCache && info.pObject->BIsKeyEqual( soIndex ) )
				return info.pObject;
		}

		return NULL;
	}

	void CSharedObjectTransaction::Rollback()
	{
		if ( m_sqlAccess.BInTransaction() )
		{
			m_sqlAccess.RollbackTransaction();
		}
		Undo();
	}

	bool CSharedObjectTransaction::BYieldingCommit( bool bAllowEmpty )
	{
		const char *pszPreExistingError = GetError();
		if ( pszPreExistingError )
		{
			EmitError( SPEW_GC, "Failed to commit CSharedObjectTransaction '%s': %s.\n", PchName(), pszPreExistingError );

			Undo();
			return false;
		}

		Assert( m_sqlAccess.BInTransaction() );
		if ( !m_sqlAccess.BInTransaction() )
		{
			EmitError( SPEW_GC, "Failed to commit CSharedObjectTransaction '%s': transaction closed before commit!\n", PchName() );

			Undo();
			return false;
		}

		bool bSuccess = true;
		bool bDBRelevantObjectsChanged = false;

		// add insert statements to sql access
		FOR_EACH_VEC( m_vecObjects_Added, i )
		{
			undoinfo_t &info = m_vecObjects_Added[i];
			if ( info.pObject->BIsDatabaseBacked() )
			{
				bSuccess &= info.pObject->BYieldingAddInsertToTransaction( m_sqlAccess );
				bDBRelevantObjectsChanged = true;
			}
		}

		// add update statements to sql access
		FOR_EACH_VEC( m_vecObjects_Modified, i )
		{
			undoinfo_t &info = m_vecObjects_Modified[i];
			if ( info.pObject->BIsDatabaseBacked() )
			{
				bSuccess &= info.pSOCache->BYieldingAddWriteToTransaction( info.pObject, m_sqlAccess );
				bDBRelevantObjectsChanged = true;
			}
		}

		if ( bSuccess == false )
		{
			EmitError( SPEW_GC, "Failed to commit CSharedObjectTransaction '%s': failed to add inserts/writes.\n", PchName() );

			Undo();
			return false;
		}

		// try to commit db transaction.
		if ( m_sqlAccess.BCommitTransaction( bAllowEmpty && !bDBRelevantObjectsChanged ) == false )
		{
			EmitError( SPEW_GC, "Failed to commit CSharedObjectTransaction '%s': SQL transaction failure.\n", PchName() );

			Undo();
			return false;
		}

		// remove objects from SO cache
		FOR_EACH_VEC( m_vecObjects_Removed, i )
		{
			undoinfo_t &info = m_vecObjects_Removed[i];
			DbgVerify( info.pSOCache->BDestroyObject( *info.pObject, false ) );			// internally will assert if cache isn't locked
			delete info.pOriginalCopy;
		}
		m_vecObjects_Removed.RemoveAll();

		// add new objects to SO cache
		FOR_EACH_VEC( m_vecObjects_Added, i )
		{
			undoinfo_t &info = m_vecObjects_Added[i];
			info.pSOCache->AddObject( info.pObject );									// internally will assert if cache isn't locked
			Assert( info.pOriginalCopy == NULL );
		}

		// free up memory for original state of modified objects
		FOR_EACH_VEC( m_vecObjects_Modified, i )
		{
			undoinfo_t &info = m_vecObjects_Modified[i];
			info.pSOCache->DirtyNetworkObject( info.pObject );
			delete info.pOriginalCopy;
		}

		// send network updates for objects that were added or modified
		// this is OK to call more than once on a CGCSharedObjectCache, because internally
		// it keeps a list of things that were marked dirty
		FOR_EACH_VEC( m_vecObjects_Added, i )
		{
			m_vecObjects_Added[i].pSOCache->SendAllNetworkUpdates();
		}
		FOR_EACH_VEC( m_vecObjects_Modified, i )
		{
			m_vecObjects_Modified[i].pSOCache->SendAllNetworkUpdates();
		}

		m_vecObjects_Added.RemoveAll();
		m_vecObjects_Modified.RemoveAll();
	
		return true;
	}

	void CSharedObjectTransaction::Undo()
	{
		FOR_EACH_VEC( m_vecObjects_Added, i )
		{
			undoinfo_t &info = m_vecObjects_Added[i];
			delete info.pObject;
		}
		m_vecObjects_Added.RemoveAll();

		FOR_EACH_VEC( m_vecObjects_Removed, i )
		{
			undoinfo_t &info = m_vecObjects_Removed[i];
			AssertMsg1( GGCBase()->IsSteamIDLockedByCurJob( info.pSOCache->GetOwner() ), "Attempt to modify in-memory object '%s' during CSharedObjectTransaction removal rollback.", info.pObject->GetDebugString().Get() );
			info.pObject->Copy( *info.pOriginalCopy );
			delete info.pOriginalCopy;
		}
		m_vecObjects_Removed.RemoveAll();

		FOR_EACH_VEC( m_vecObjects_Modified, i )
		{
			undoinfo_t &info = m_vecObjects_Modified[i];
			AssertMsg1( GGCBase()->IsSteamIDLockedByCurJob( info.pSOCache->GetOwner() ), "Attempt to modify in-memory object '%s' during CSharedObjectTransaction modify rollback.", info.pObject->GetDebugString().Get() );
			info.pObject->Copy( *info.pOriginalCopy );
			delete info.pOriginalCopy;
		}
		m_vecObjects_Modified.RemoveAll();

		ClearError();
	}

	const char *CSharedObjectTransaction::PchName() const
	{
		return m_sqlAccess.PchTransactionName();
	}
};

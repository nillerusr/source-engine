//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base class for transactions that modify a CGCSharedObjectCache and the database
//
//=============================================================================

#ifndef SHAREDOBJECTTRANSACTION_H
#define SHAREDOBJECTTRANSACTION_H

#ifdef _WIN32
#pragma once
#endif

#include <functional>

namespace GCSDK
{

template < typename TSharedObject >
struct SharedObjectContainsAuditEntryType
{
	typedef char t_Yes[1];
	typedef char t_No[2];

	template < typename T >
    static t_Yes& Test( typename T::CAuditEntry * );
 
    template < typename T >
    static t_No& Test( ... );

	enum { kValue = sizeof( Test<TSharedObject>( NULL ) ) == sizeof( t_Yes ) };
};

//-----------------------------------------------------------------------------
// Purpose: Let's stop writing transactional code by hand everywhere!
//
// Core usage:
//
//		- make a new instance, either starting a new SQL transaction or hooking
//		  into an existing open transaction.
//
//		- call some combination of AddNewObject/RemoveObject to add newly-allocated
//		  objects or remove existing objects. If the types of objects being added/
//		  removed contain a linked audit data class, you're required to pass in
//		  a filled-out instance with the add/remove request.
//
//		- modify some existing objects. You can either call ModifyObject directly
//		  or call one of the helper functions/macros (ie., ModifyObjectSch).
//		  Whatever changes get made here won't be visible anywhere outside this
//		  transaction object until a commit succeeds.
//
//		- try to do a commit. If this succeeds, everything worked and memory has
//		  been updated to reflect DB changes (if any). If this fails, or if the
//		  transaction object gets destroyed with an open transaction, all queued
//		  SQL work will be dropped and no potential memory changes will happen.
//
// That was too long. What's a short version?:
//
//		{
//			CSharedObjectTransactionEx transaction( pSomeLockedSOCache, "Sample Tool Transaction" );
//			transaction.RemoveObject( pToolItem, CEconItem::CAuditEntry( ... ) );
//			transaction.RemoveObject( pToolTargetItem, CEconItem::CAuditEntry( ... ) );
//			transaction.AddNewObject( pNewResultItem, CEconItem::CAuditEntry( ... ) );
//			transaction.ModifyObjectSch( pEconGameAccount, unNumToolsUsed, pEconGameAccount->Obj().m_pEconGameAccount + 1 );
//			Verify( transaction.BYieldingCommit() );
//		}
//
// Guarantees this class makes:
//
//		- if the initial state is correct and client code isn't malicious, a
//		  lock on the SO cache passed in will be maintained for the lifetime
//		  of the class.
//
//		- externally, no changes will be visible on the GC or on cients until a
//		  commit attempt takes place. If the commit failures, no changes will
//		  take place in memory. If the commit succeeds, all memory changes will
//		  become visible "simultaneously" (no yields, but not atomic from a
//		  threading perspective.
//
//		- this class will do the minimum amount of work possible to guarantee
//		  correct behavior. If you don't touch any networked objects, no network
//		  updates will be sent. If you don't touch any DB-backed objects, no DB
//		  work will be done.
//-----------------------------------------------------------------------------
class CSharedObjectTransactionEx
{
public:
	CSharedObjectTransactionEx( CGCSharedObjectCache *pLockedSOCache, const char *pszTransactionName );

	~CSharedObjectTransactionEx();

	// AddNewObject:
	//		Add a new object to this user's SO cache. If the type of this object supports audit entries, you're
	//		required to pass in a CAuditEntry of the appropriate type (ie., CEconItem::CAuditEntry). If your type
	//		doesn't support audit entries, and you're really sure that not auditing in the correct behavior, you
	//		call the single-argument version below. In both cases, it's illegal to pass in an object that's
	//		a raw CSharedObject pointer because then this code doesn't know what type of audit data to look for.
	//
	//		It's possible to set up these functions using SFINAE so that the compiler will only see the appropriate
	//		version, but this way produces prettier error messages.

	// Add an object to this user's cache, conditional on the SQL transaction succeeding, including writing an
	// audit entry to SQL explaining where this object came from.
	template < typename TSharedObject >
	void AddNewObject( TSharedObject *pObject, const typename TSharedObject::CAuditEntry& audit )
	{
		COMPILE_TIME_ASSERT( (!AreTypesIdentical<TSharedObject, CSharedObject>::kValue) );
		COMPILE_TIME_ASSERT( SharedObjectContainsAuditEntryType<TSharedObject>::kValue );

		m_bTransactionBuildSuccess &= BAddNewObjectInternal( pObject );
		m_bTransactionBuildSuccess &= audit.BAddAuditEntryToTransaction( *m_pSQLAccess, pObject );
	}

	// Add an object to this user's cache, conditional on the SQL transaction succeeding, but don't write any
	// audit data. In general, this is probably the wrong thing to do and you want to be making sure this object
	// type supports writing audit data and then writing it.
	template < typename TSharedObject >
	void AddNewObject( TSharedObject *pObject )
	{
		COMPILE_TIME_ASSERT( (!AreTypesIdentical<TSharedObject, CSharedObject>::kValue) );
		COMPILE_TIME_ASSERT( !SharedObjectContainsAuditEntryType<TSharedObject>::kValue );

		m_bTransactionBuildSuccess &= BAddNewObjectInternal( pObject );
	}

	// This is a helper class purely to help VC will template type deduction. The real signature we want for the
	// base ModifyObject function is:
	//
	//		template < typename TSharedObject >
	//		void ModifyObject( TSharedObject *pObject, const std::function< bool( CSQLAccess&, CSharedObjectDirtyList&, TSharedObject * ) >& funcModifyAndAudit )
	//
	// ...but if we do do that, VC will complain that it doesn't know how to deduce the type for TSharedObject
	// because of the second parameter. Instead, we make it deduce the type based on the first parameter, and then
	// feed that type into this helper class to get the type for the second parameter.
	template < typename TSharedObject >
	struct CSharedObjectModifyAndAuditFunction
	{
		typedef std::function< bool( CSQLAccess&, CSharedObjectDirtyList&, TSharedObject * ) > ModifyFunctionType;
	};

	// ModifyObject: takes in 
	template < typename TSharedObject >
	void ModifyObject( TSharedObject *pObject, const typename CSharedObjectModifyAndAuditFunction<TSharedObject>::ModifyFunctionType& funcModifyAndAudit )
	{
		COMPILE_TIME_ASSERT( (!AreTypesIdentical<TSharedObject, CSharedObject>::kValue) );

		CSharedObject *pWritableObject = NULL;
		m_bTransactionBuildSuccess &= BTrackModifiedObjectInternal( pObject, &pWritableObject );
		AssertMsg( !m_bTransactionBuildSuccess || pWritableObject != NULL, "Cannot be tracking state for an object but not having a writable version!" );

		m_bTransactionBuildSuccess &= funcModifyAndAudit( *m_pSQLAccess, m_SODirtyList, assert_cast<TSharedObject *>( pWritableObject ) );
	}

	// ModifyObjectL() is a helper macro designed to behave like a function. It takes as parameters the original
	// object you want to modify and the code you want to run on it, expressed as a lambda.
	#define ModifyObjectL( obj_, modifyfunc_ ) \
		ModifyObject( obj_, [=] ( CSQLAccess& sqlAccess, CSharedObjectDirtyList& SODirtyList, decltype( obj_ ) pWritableObject ) -> bool { modifyfunc_ } )

	// ModifyObjectSch() is a helper macro designed to behave like a function. It takes as parameters the original
	// object you want to modify, an identifier for the field you want to change, and the new value you want to
	// change it to. Ex.:
	//
	//		transaction.ModifyObjectSch( pLockedSOCache->GetGameAccount(),		// type of internal object is used to look up field IDs
	//									 unNextHalloweenGiftTime,				// turns into "(obj).m_unNextHalloweenGiftTime" and "(type)::k_iField_unNextHalloweenGiftTime"
	//									 CRTime::RTime32DateAdd( CRTime::RTime32TimeCur(), tf_halloween_min_minutes_between_drops_per_player.GetInt(), k_ETimeUnitMinute ) );
	#define ModifyObjectSch( obj_, field_, newvalue_ ) \
		ModifyObjectL( obj_, \
			{ \
				pWritableObject->Obj().m_ ## field_ = (newvalue_); \
				SODirtyList.DirtyObjectField( pWritableObject, std::remove_reference< decltype( pWritableObject->Obj() ) >::type::k_iField_ ## field_ ); \
				return true; \
			} )

	// ModifyObjectProto() is a helper macro designed to behave like a function. It takes as parameters the original
	// object you want to modify, the field name in the message or the field you want to change, the identifier field
	// ID (will be identical except for case/underscores), and the new value. Ex.:
	//
	//		soTrans.ModifyObjectProto( pGameAccountClient,						// type of internal object is used to look up field IDs
	//								   preview_item_def,						// turns into "(obj)->set_preview_item_def"
	//								   PreviewItemDef,							// turns into "(type)::kPreviewItemDefFieldNumber"
	//								   0 );
	#define ModifyObjectProto( obj_, fieldfunc_, fieldname_, newvalue_ ) \
		ModifyObjectL( obj_, \
			{ \
				pWritableObject->Obj().set_ ## fieldfunc_( newvalue_ ); \
				SODirtyList.DirtyObjectField( pWritableObject, std::remove_reference< decltype( pWritableObject->Obj() ) >::type::k ## fieldname_ ## FieldNumber ); \
				return true; \
			} )

	// RemoveObject
	template < typename TSharedObject >
	void RemoveObject( TSharedObject *pObject, const typename TSharedObject::CAuditEntry& audit )
	{
		COMPILE_TIME_ASSERT( (!AreTypesIdentical<TSharedObject, CSharedObject>::kValue) );
		COMPILE_TIME_ASSERT( SharedObjectContainsAuditEntryType<TSharedObject>::kValue );

		m_bTransactionBuildSuccess &= BRemoveObjectInternal( pObject );
		m_bTransactionBuildSuccess &= audit.BAddAuditEntryToTransaction( *m_pSQLAccess, pObject );
	}

	template < typename TSharedObject >
	void RemoveObject( CSharedObject *pObject )
	{
		COMPILE_TIME_ASSERT( (!AreTypesIdentical<TSharedObject, CSharedObject>::kValue) );
		COMPILE_TIME_ASSERT( !SharedObjectContainsAuditEntryType<TSharedObject>::kValue );

		m_bTransactionBuildSuccess &= BRemoveObjectInternal( pObject );
	}

	template < class TSchType >
	void AddSQLRecord( const TSchType& sch )
	{
		// We can't test for all types here because there's no compile-time list. This is
		// mostly to demonstrate "seriously please don't  call this with types that have
		// CSharedObject wrappers".
		COMPILE_TIME_ASSERT( (!AreTypesIdentical<TSchType, CSchItem>::kValue) );
		COMPILE_TIME_ASSERT( (!AreTypesIdentical<TSchType, CSchItemAudit>::kValue) );
		COMPILE_TIME_ASSERT( (!AreTypesIdentical<TSchType, CSchGameAccount>::kValue) );

		// We're about to call a function with "Yielding" in the name and we aren't in a
		// function with "Yielding" in the name, but we *are* in a transaction so we know
		// we don't yield. We verify that assumption here.
		DO_NOT_YIELD_THIS_SCOPE();

		// Queue up the work for SQL as long as we're in a good state to do so.
		m_bTransactionBuildSuccess &= BIsValidInternalState()
									? m_pSQLAccess->BYieldingInsertRecord( &sch )
									: false;
	}

	template < class TSchType >
	void AddOrUpdateSQLRecord( TSchType& sch )
	{
		// We can't test for all types here because there's no compile-time list. This is
		// mostly to demonstrate "seriously please don't  call this with types that have
		// CSharedObject wrappers".
		COMPILE_TIME_ASSERT( (!AreTypesIdentical<TSchType, CSchItem>::kValue) );
		COMPILE_TIME_ASSERT( (!AreTypesIdentical<TSchType, CSchItemAudit>::kValue) );
		COMPILE_TIME_ASSERT( (!AreTypesIdentical<TSchType, CSchGameAccount>::kValue) );

		// We're about to call a function with "Yielding" in the name and we aren't in a
		// function with "Yielding" in the name, but we *are* in a transaction so we know
		// we don't yield. We verify that assumption here.
		DO_NOT_YIELD_THIS_SCOPE();

		// Queue up the work for SQL as long as we're in a good state to do so.
		m_bTransactionBuildSuccess &= BIsValidInternalState()
									? m_pSQLAccess->BYieldingInsertOrUpdateOnPK( &sch )
									: false;
	}

	// This is meant purely for interop with the SQL message queue and even then only as a temporary
	// measure until we have a CSQLTransaction object we can pass around instead.
	CSQLAccess& GetSQLTransactionForSQLMsgQueue() { return *m_pSQLAccess; }

	// slow! but fine for current uses
	template < class TSharedObject >
	const TSharedObject *FindTypedSharedObject( const CSharedObject &soIndex ) const
	{
		return assert_cast<TSharedObject *>( InternalFindSharedObject( pSOCache, soIndex ) );
	}

	// Take all the work we queued up for SQL and try to commit it to the DB. If that works, take all
	// of our memory changes and copy them over. From the outside, this will either move *all* memory
	// changes to our cache over at once, or not touch any in-memory structures at all.
	MUST_CHECK_RETURN bool BYieldingCommit();

	// Cancel this transaction completely -- this will empty the queue of whatever SQL work we may have
	// done and also free up the memory we used to track modified object state, etc. Once this function
	// gets called, it's illegal to call any other functions on this transaction object except the
	// destructor.
	void Rollback();

private:
	// State validation. Non-const because may set internal error state.
	bool BIsValidInternalState();
	bool BIsValidInput( const CSharedObject *pObject );

	const char *GetInternalTransactionDesc() const { return m_pSQLAccess->PchTransactionName(); }

	template < typename tCommitInfo >
	const tCommitInfo *InternalFindCommitInfo( const CSharedObject *pObject, const CUtlVector<tCommitInfo>& vec ) const
	{
		FOR_EACH_VEC( vec, i )
		{
			if ( vec[i].m_pObject == pObject )
				return &vec[i];
		}

		return NULL;
	}

	const CSharedObject *InternalFindSharedObject( CGCSharedObjectCache *pSOCache, const CSharedObject& soIndex ) const;

	// Will return NULL if pre-commit operations/checks were successful, or a pointer to a descriptive error string if
	// pre-commit failed.
	const char *InternalPreCommit();

	bool BAddNewObjectInternal( CSharedObject *pObject );
	bool BTrackModifiedObjectInternal( CSharedObject *pObject, CSharedObject **out_ppWritableObject );
	bool BRemoveObjectInternal( CSharedObject *pObject );

	friend class CTrustedHelper_OutputAndSetErrorState;
	void SetErrorState() { m_bTransactionBuildSuccess = false; }

private:
	CGCSharedObjectCache *m_pLockedSOCache;				// we don't do any locking ourself, but verify that the lock is held during construction/modification
	CSharedObjectDirtyList m_SODirtyList;

	CSQLAccess *m_pSQLAccess;							// our access to SQL -- may point to our inline instance or may point to an external object if we're hitching on an already-existing transaction
	bool m_bTransactionBuildSuccess;

	CSQLAccess m_sqlAccessInternal;

	struct CreateOrDestroyCommitInfo_t
	{
		CreateOrDestroyCommitInfo_t( CSharedObject *pObject ) : m_pObject( pObject ) { Assert( m_pObject ); }

		CSharedObject *m_pObject;
	};

	struct ModifyCommitInfo_t
	{
		ModifyCommitInfo_t( CSharedObject *pWriteableObject, CSharedObject *pOriginalCopy )
			: m_pWriteableObject( pWriteableObject )
			, m_pObject( pOriginalCopy )
		{
		}

		CSharedObject *m_pWriteableObject;		// scratch/memory-writable copy while transaction is open
		CSharedObject *m_pObject;				// original copy
	};

	CUtlVector<CreateOrDestroyCommitInfo_t> m_vecObjects_Added;
	CUtlVector<CreateOrDestroyCommitInfo_t> m_vecObjects_Removed;
	CUtlVector<ModifyCommitInfo_t> m_vecObjects_Modified;
};

class CSharedObjectTransaction
{
public:

	/**
	 * Constructor that will begin a transaction
	 * @param sqlAccess
	 * @param pName
	 */
	CSharedObjectTransaction( CSQLAccess &sqlAccess, const char *pName );

	/**
	 * Destructor
	 */
	~CSharedObjectTransaction();

	/**
	 * Adds an object that exists in the given CGCSharedObjectCache to be managed in this transaction.
	 * Call this before making any modifications to the object
	 * @param pSOCache the owner CGCSharedObjectCache
	 * @param pObject the object that will be modified
	 */
	void AddManagedObject( CGCSharedObjectCache *pSOCache, CSharedObject *pObject );

	/**
	 * Adds a brand new object to the given CGCSharedObjectCache
	 * @param pSOCache the owner CGCSharedObjectCache
	 * @param pObject the newly created object
	 */
	void AddNewObject( CGCSharedObjectCache *pSOCache, CSharedObject *pObject );

	/**
	 * Removes an existing object from the CGCSharedObjectCache
	 * @param pSOCache the owner CGCSharedObjectCache
	 * @param pObject the object to be removed from the CGCSharedObjectCache
	 */
	void RemoveObject( CGCSharedObjectCache *pSOCache, CSharedObject *pObject );

	/**
	 * Marks in the transaction that the object was modified.  The object must have been previously added via
	 * the AddManagedObject() call in order for the object to be marked dirty.  If the object is new to the
	 * CGCSharedObjectCache, then calling this will return false (which is not necessarily an error)
	 *
	 * @param pObject the object that will be modified
	 * @param unFieldIdx the field that was changed
	 */
	void ModifiedObject( CGCSharedObjectCache *pSOCache, CSharedObject *pObject, uint32 unFieldIdx );

	/**
	 * @param pSOCache
	 * @param soIndex
	 * @return the CSharedObject that matches either in the CGCSharedObjectCache or to be added
	 */
	template < class T >
	T *FindTypedSharedObject( CGCSharedObjectCache *pSOCache, const CSharedObject &soIndex )
	{
		return assert_cast<T *>( FindSharedObject( pSOCache, soIndex ) );
	}

	/**
	 * Rolls back any changes made to the objects in-memory and in the database
	 *
	 * This function should not be made virtual -- it's called from within the destructor.
	 */
	void Rollback();

	/**
	 * Commits any changes to the database and also to memory
	 * @return true if successful, false otherwise
	 */
	bool BYieldingCommit( bool bAllowEmpty = false );

	/**
	 * @return GCSDK::CSQLAccess associated with this transaction
	 */
	CSQLAccess &GetSQLAccess() { return m_sqlAccess; }

	/**
	 * Fetch name of transaction for debugging purposes
	 * @return the string passed to the constructor
	 */
	const char *PchName() const;

private:

	/**
	 * @param pSOCache
	 * @param soIndex
	 * @return the CSharedObject that matches either in the list of currently-modified objects or the list
	 *		   or of new objects we added; this will not search in the base SO cache
	 */
	CSharedObject *FindSharedObject( CGCSharedObjectCache *pSOCache, const CSharedObject &soIndex );

	/**
	 * Reverts all in-memory modifications and deletes all newly created objects.
	 *
	 * This function should not be made virtual -- it's called from within the destructor.
	 */
	void Undo();

	/**
	 * Set an error string to describe an error we encountered building this transaction. Setting this
	 * will cause the transaction to fail.
	 */
	void SetError( const char *pszNewError )
	{
		AssertMsg( pszNewError && ( pszNewError[0] != '\0' ), "Invalid NULL/empty error set in CSharedObjectTransaction::SetError()! This will have the effect of clearing the error state, which is unsupported." );
		m_sErrorDesc = pszNewError;
	}

	/**
	 * Clear our error string if we have one. This will allow transactions to succeed and so is only intended
	 * to be done when the transaction itself has either completed (no either to begin with) or emptied (clean
	 * slate).
	 */
	void ClearError()
	{
		Assert( m_vecObjects_Added.Count() == 0 );
		Assert( m_vecObjects_Removed.Count() == 0 );
		Assert( m_vecObjects_Modified.Count() == 0 );
		m_sErrorDesc.Clear();
	}

	/**
	 * Get access to the string describing what, if any, the last error we encountered was. Will return
	 * NULL if no errors have been encountered.
	 */
	const char *GetError() const
	{
		return m_sErrorDesc.IsEmpty() ? NULL : m_sErrorDesc.String();
	}

	struct undoinfo_t
	{
		CSharedObject *pObject;
		CGCSharedObjectCache *pSOCache;
		CSharedObject *pOriginalCopy;

		bool operator==( const undoinfo_t& other ) const { return other.pObject == pObject && other.pSOCache == pSOCache && other.pOriginalCopy == pOriginalCopy; }
	};

	// Wraps the common check to make sure these pointers are valid and that the cache is locked. This is non-const
	// because it can call SetError().
	bool AssertValidInput( const CGCSharedObjectCache *pSOCache, const CSharedObject *pObject, const char *pszContext );

	// Finds the object in the given vector (using simple pointer compare)
	undoinfo_t *FindObjectInVector( const CSharedObject *pObject, CUtlVector<undoinfo_t> &vec ) const;

	// variables
	CUtlVector< undoinfo_t > m_vecObjects_Added;
	CUtlVector< undoinfo_t > m_vecObjects_Removed;
	CUtlVector< undoinfo_t > m_vecObjects_Modified;
	CSQLAccess &m_sqlAccess;

	// internal error state
	CUtlString m_sErrorDesc;		// will be non-empty if we've encountered an error at some point building this transaction
}; // class CSharedObjectTransaction

}; // namespace GCSDK

#endif // SHAREDOBJECTTRANSACTION_H

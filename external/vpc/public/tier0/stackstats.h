//========= Copyright � 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: Tools for grabbing/dumping the stack at runtime
//
// $NoKeywords: $
//=============================================================================//

#ifndef TIER0_STACKSTATS_H
#define TIER0_STACKSTATS_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/stacktools.h"
#include "tier0/threadtools.h"

#if defined ENABLE_RUNTIME_STACK_TRANSLATION
#define ENABLE_STACK_STATS_GATHERING //uncomment to enable the gathering class
#endif

#if defined ENABLE_STACK_STATS_GATHERING
#include "tier0/valve_off.h"
#	include <map> //needed for CCallStackStatsGatherer
#	include <vector>
#include "tier0/valve_on.h"
#define CDefaultStatsGathererAllocator std::allocator
#else
template<class _Ty> class CNullStatsGathererAllocator { CNullStatsGathererAllocator( void ) { } };
#define CDefaultStatsGathererAllocator CNullStatsGathererAllocator
#endif


typedef size_t (*FN_DescribeStruct)( uint8 *, size_t );

class CCallStackStatsGatherer_Standardized_t;
struct CCallStackStatsGatherer_FunctionTable_t
{
	void (*pfn_GetDumpInfo)( void *, const char *&, size_t &, size_t &, void *&, size_t &, CCallStackStatsGatherer_Standardized_t *&, size_t & );
	void (*pfn_PushSubTree)( void *, const CCallStackStatsGatherer_Standardized_t &, const CCallStackStorage & );
	void (*pfn_PopSubTree)( void * );
	size_t (*pfn_DescribeCallStackStatStruct)( uint8 *, size_t );
	void (*pfn_SyncMutexes)( void *, bool );
	void *(*pfn_GetEntry)( void *, uint32 );
	void (*pfn_ApplyTreeAccessLock)( void *, bool );
	void (*pfn_LockEntry)( void *, uint32, bool );
};

//templatized classes are fun, can't really use a base pointer effectively, so we'll have a translator struct and store pointers to instances of the translator function
class CCallStackStatsGatherer_Standardized_t
{
public:
	CCallStackStatsGatherer_Standardized_t( void ) {};
	CCallStackStatsGatherer_Standardized_t( void *pThis, const CCallStackStatsGatherer_FunctionTable_t &FnTable ) : pGatherer( pThis ), pFunctionTable( &FnTable ) {};

	//go ahead and create some helper functions that are likely to be used by helper code
	void PushSubTree( const CCallStackStatsGatherer_Standardized_t &SubTree, const CCallStackStorage &PushStack = CCallStackStorage() ) const
	{
		pFunctionTable->pfn_PushSubTree( pGatherer, SubTree, PushStack );
	}

	inline void PopSubTree( void ) const
	{
		pFunctionTable->pfn_PopSubTree( pGatherer );
	}
	
	void *pGatherer;
	const CCallStackStatsGatherer_FunctionTable_t *pFunctionTable;
};

//Designed to be called by an instance of CCallStackStatsGatherer to dump it's data
PLATFORM_INTERFACE bool _CCallStackStatsGatherer_Internal_DumpStatsToFile( const char *szFileName, const CCallStackStatsGatherer_Standardized_t &StatsGatherer, bool bAllowMemoryAllocations );






template <class STATSTRUCT>
class CCallStackStatsGatherer_StructAccessor_Base
{
public:
	CCallStackStatsGatherer_StructAccessor_Base( const CCallStackStatsGatherer_Standardized_t &Gatherer, int iEntryIndex ) : m_Gatherer( Gatherer ), m_iEntryIndex( iEntryIndex ) {};
protected:
	uint32 m_iEntryIndex; //index of the stat entry we want in the vector. Stored as index as the vector base address can change.
	CCallStackStatsGatherer_Standardized_t m_Gatherer; //so we can lock the vector memory in place while manipulating the values
};


class CCallStackStatsGatherer_StatMutexBase
{
public:
	void LockEntry( uint32 iEntryIndex, bool bLock ) {} //true to increase lock refcount, false to decrease
};

template <uint32 SHAREDENTRYMUTEXES> //must be a power of 2
class CCallStackStatsGatherer_StatMutexPool
{
public:
	void LockEntry( uint32 iEntryIndex, bool bLock )
	{
#if defined( ENABLE_STACK_STATS_GATHERING )
		COMPILE_TIME_ASSERT( (SHAREDENTRYMUTEXES & (SHAREDENTRYMUTEXES - 1)) == 0 ); //must be a power of 2

		if( bLock )
		{
			m_IndividualEntryMutexes[iEntryIndex & (SHAREDENTRYMUTEXES - 1)].Lock();
		}
		else
		{
			m_IndividualEntryMutexes[iEntryIndex & (SHAREDENTRYMUTEXES - 1)].Unlock();
		}
#endif
	}
protected:
	CThreadFastMutex m_IndividualEntryMutexes[SHAREDENTRYMUTEXES];
};




//STATSTRUCT - The structure you'll use to track whatever it is you're tracking.
//CAPTUREDCALLSTACKLENGTH - The maximum length of your stack trace that we'll use to distinguish entries
//STACKACQUISITIONFUNCTION - The function to use to gather the current call stack. GetCallStack() is safe, GetCallStack_Fast() is faster, but has special requirements
//STATMUTEXHANDLER - If you want automatic mutex management for your individual entries, supply a handler here. 
//						You'll need to not only call GetEntry(), but lock/unlock the entry while accessing it.
//TEMPLATIZEDMEMORYALLOCATOR - We'll need to allocate memory, supply an allocator if you want to manage that
template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION = GetCallStack, typename STATMUTEXHANDLER = CCallStackStatsGatherer_StatMutexPool<4>, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR = CDefaultStatsGathererAllocator>
class CCallStackStatsGatherer : public STATMUTEXHANDLER
{
public:
#if !defined( ENABLE_STACK_STATS_GATHERING )
	CCallStackStatsGatherer( void )
	{
		for( size_t i = 0; i != CAPTUREDCALLSTACKLENGTH; ++i )
			m_SingleCallStack[i] = NULL;
	}
#endif

	CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT> GetEntry( void * const CallStack[CAPTUREDCALLSTACKLENGTH] ); //get the entry using some callstack grabbed a while ago. Assumes ALL invalid entries have been nullified
	CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT> GetEntry( void * const CallStack[CAPTUREDCALLSTACKLENGTH], uint32 iValidEntries ); //same as above, but does the work of nullifying invalid entries
	CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT> GetEntry( const CCallStackStorage &PushStack = CCallStackStorage( STACKACQUISITIONFUNCTION ) );
	CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT> GetEntry( uint32 iEntryIndex );

	//get the entry index for the caller's current call stack. Pre-nullified entries count as valid entries (and save re-nullification work)
	uint32 GetEntryIndex( void * const CallStack[CAPTUREDCALLSTACKLENGTH], uint32 iValidEntries ); //index is unchanging, safe to keep and use later (designed for exactly that purpose)
	uint32 GetEntryIndex( const CCallStackStorage &PushStack = CCallStackStorage( STACKACQUISITIONFUNCTION ) );

	

	typedef void *(&StackReference)[CAPTUREDCALLSTACKLENGTH];
	StackReference GetCallStackForIndex( uint32 iEntryIndex )
	{
#if defined( ENABLE_STACK_STATS_GATHERING )
		return m_StatEntries[iEntryIndex].m_CallStack;
#else
		return m_SingleCallStack;
#endif
	}

	void GetCallStackForIndex( uint32 iEntryIndex, void *CallStackOut[CAPTUREDCALLSTACKLENGTH] );

	size_t NumEntries( void ) const;

	bool DumpToFile( const char *szFileName, bool bAllowMemoryAllocations = true );

	static const CCallStackStatsGatherer_FunctionTable_t &GetFunctionTable( void );

	static void GetDumpInfo( void *pThis, const char *&szStructName, size_t &iCapturedStackLength, size_t &iEntrySizeWithStack, void *&pEntries, size_t &iEntryCount, CCallStackStatsGatherer_Standardized_t *&pSubTrees, size_t &iSubTreeCount );
	static void PushSubTree( void *pParent, const CCallStackStatsGatherer_Standardized_t &SubTree, const CCallStackStorage &PushStack );

	void PushSubTree( CCallStackStatsGatherer_Standardized_t &Parent, const CCallStackStorage &PushStack = CCallStackStorage( STACKACQUISITIONFUNCTION ) );

	static void PopSubTree( void *pParent );
	static void SyncMutexes( void *pParent, bool bLock ); //true for lock, false for unlock
	static void *GetEntry( void *pParent, uint32 iEntryIndex );
	static void ApplyTreeAccessLock( void *pParent, bool bLock );
	static void LockEntry( void *pParent, uint32 iEntryIndex, bool bLock );

	void Reset( void );
	

	CCallStackStatsGatherer_Standardized_t Standardized( void );
	operator CCallStackStatsGatherer_Standardized_t( void );

	const static FN_GetCallStack StackFunction; //publish the requested acquisition function so you can easily key all your stack gathering to the class instance
	const static size_t CapturedCallStackLength;
private:

#pragma pack(push)
#pragma pack(1)
	struct StackAndStats_t
	{
		void *m_CallStack[CAPTUREDCALLSTACKLENGTH];
		STATSTRUCT m_Stats;
	};
#pragma pack(pop)

	typedef CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR> ThisCast;
	

#if defined( ENABLE_STACK_STATS_GATHERING )

	struct IndexMapKey_t
	{
		IndexMapKey_t( void * const CallStack[CAPTUREDCALLSTACKLENGTH] )
		{
			m_Hash = 0;
			for( int i = 0; i < CAPTUREDCALLSTACKLENGTH; ++i )
			{
				m_CallStack[i] = CallStack[i];
				m_Hash += (uintp)CallStack[i];
			}
		}
		bool operator<( const IndexMapKey_t &key ) const
		{
			if( m_Hash != key.m_Hash )
				return m_Hash < key.m_Hash;

			//only here if there's a hash match. Do a full check. Extremely likely to be the exact same call stack. But not 100% guaranteed.
			for( int i = 0; i < CAPTUREDCALLSTACKLENGTH; ++i )
			{
				if( m_CallStack[i] == key.m_CallStack[i] )
				{
					continue;
				}

				return m_CallStack[i] < key.m_CallStack[i];
			}

			return false; //exact same call stack
		}

		uintp m_Hash;
		void *m_CallStack[CAPTUREDCALLSTACKLENGTH];
	};

	void KeepSubTree( CCallStackStatsGatherer_Standardized_t &SubTree )
	{
		AUTO_LOCK_FM( m_SubTreeMutex );
		for( StoredSubTreeVector_t::iterator treeIter = m_StoredSubTrees.begin(); treeIter != m_StoredSubTrees.end(); ++treeIter )
		{
			if( SubTree.pGatherer == treeIter->pGatherer )
				return;			
		}

		//Warning( "Storing subtree\n" );

		m_StoredSubTrees.push_back( SubTree );
	}

	uint32 PatchInSubTrees( void * const CallStackIn[CAPTUREDCALLSTACKLENGTH], void *CallStackOut[CAPTUREDCALLSTACKLENGTH], uint32 iValidEntries )
	{
		if( iValidEntries > CAPTUREDCALLSTACKLENGTH )
		{
			iValidEntries = CAPTUREDCALLSTACKLENGTH;
		}

		AUTO_LOCK_FM( m_SubTreeMutex );
		if( m_PushedSubTrees.size() == 0 )
		{
			memcpy( CallStackOut, CallStackIn, sizeof( void * ) * iValidEntries );
			return iValidEntries;
		}

		unsigned long iThreadID = ThreadGetCurrentId();
		PushedSubTreeVector_t::reverse_iterator treeIter;
		for( treeIter = m_PushedSubTrees.rbegin(); treeIter != m_PushedSubTrees.rend(); ++treeIter )
		{
			if( treeIter->iThreadID == iThreadID )
			{
				break;
			}
		}

		if( treeIter == m_PushedSubTrees.rend() )
		{
			memcpy( CallStackOut, CallStackIn, sizeof( void * ) * iValidEntries );
			return iValidEntries;
		}

		//char szTemp[4096];
		//TranslateStackInfo( CallStackIn, CAPTUREDCALLSTACKLENGTH, szTemp, sizeof( szTemp ), "\n\t" );		

		//Warning( "Attempting to link trees:\n=======================ONE=======================\n\t%s\n", szTemp );
		//TranslateStackInfo( treeIter->Stack, CAPTUREDCALLSTACKLENGTH, szTemp, sizeof( szTemp ), "\n\t" );
		//Warning( "=======================TWO=======================\n\t%s\n", szTemp );

		void *pMatchAddress = treeIter->Stack[1]; //while the first entry is where the actual push was made. The second entry is the first that is matchable in most cases

		uint32 i;
		for( i = 0; i < iValidEntries; ++i )
		{
			if( CallStackIn[i] == pMatchAddress )
			{
				//TranslateStackInfo( CallStackIn, i, szTemp, sizeof( szTemp ), "\n\t" );
				//Warning( "======================MATCH======================\n\t%s\n", szTemp );				

				CallStackOut[i] = treeIter->tree.pGatherer; //tag this entry as leading into the sub-tree
				KeepSubTree( treeIter->tree ); //store the sub-tree forever
				return i + 1;
			}
			CallStackOut[i] = CallStackIn[i];
		}

		return iValidEntries;

		//Warning( "=======================END=======================\n" );
	}

	struct StatIndex_t
	{
		StatIndex_t( void ) : m_Index((unsigned int)-1) {};
		unsigned int m_Index;
	};

	typedef std::vector<StackAndStats_t, TEMPLATIZEDMEMORYALLOCATOR<StackAndStats_t> > StatVector_t;
	typedef std::map< IndexMapKey_t, StatIndex_t, std::less<IndexMapKey_t>, TEMPLATIZEDMEMORYALLOCATOR<std::pair<const IndexMapKey_t, StatIndex_t> > > IndexMap_t;
	typedef typename IndexMap_t::iterator IndexMapIter_t;
	typedef typename IndexMap_t::value_type IndexMapEntry_t;

	StatVector_t m_StatEntries;
	IndexMap_t m_IndexMap;

	struct PushedSubTree_t
	{
		unsigned long iThreadID;
		CCallStackStatsGatherer_Standardized_t tree;
		void *Stack[CAPTUREDCALLSTACKLENGTH];
	};

	typedef std::vector<PushedSubTree_t, TEMPLATIZEDMEMORYALLOCATOR<PushedSubTree_t> > PushedSubTreeVector_t;
	PushedSubTreeVector_t m_PushedSubTrees;

	typedef std::vector<CCallStackStatsGatherer_Standardized_t, TEMPLATIZEDMEMORYALLOCATOR<CCallStackStatsGatherer_Standardized_t> > StoredSubTreeVector_t;
	StoredSubTreeVector_t m_StoredSubTrees;

	CThreadFastMutex m_IndexMapMutex;
	CThreadFastMutex m_SubTreeMutex;

	//only for locking the memory in place, locked for write when the entry addresses might change.
	//Locked for read when you've claimed you're manipulating a value.
	//You're on your own for making sure two threads don't access the same index simultaneously
	CThreadRWLock m_StatEntryLock;

#else //#if defined( ENABLE_STACK_STATS_GATHERING )	

	STATSTRUCT m_SingleEntry; //the class is disabled, we'll always return this same struct
	void *m_SingleCallStack[CAPTUREDCALLSTACKLENGTH];

	static size_t NULL_DescribeCallStackStatStruct( uint8 *pDescribeWriteBuffer, size_t iDescribeMaxLength ) { return 0; }

#endif //#if defined( ENABLE_STACK_STATS_GATHERING )
};

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
const FN_GetCallStack CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::StackFunction = STACKACQUISITIONFUNCTION;

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
const size_t CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::CapturedCallStackLength = CAPTUREDCALLSTACKLENGTH;

#if defined( ENABLE_STACK_STATS_GATHERING )

class CallStackStatStructDescFuncs;
PLATFORM_INTERFACE size_t _CCallStackStatsGatherer_Write_FieldDescriptions( CallStackStatStructDescFuncs *pFieldDescriptions, uint8 *pWriteBuffer, size_t iWriteBufferSize );
//PLATFORM_INTERFACE size_t _CCallStackStatsGatherer_Write_FieldMergeScript( CallStackStatStructDescFuncs *pFieldDescriptions, CallStackStatStructDescFuncs::MergeScript_Language scriptMergeLanguage, uint8 *pWriteBuffer, size_t iWriteBufferSize );

#define	DECLARE_CALLSTACKSTATSTRUCT() static const char *STATSTRUCTSTRINGNAME;\
										static size_t DescribeCallStackStatStruct( uint8 *pDescribeWriteBuffer, size_t iDescribeMaxLength );

#define BEGIN_STATSTRUCTDESCRIPTION( className ) const char *className::STATSTRUCTSTRINGNAME = #className;\
	size_t className::DescribeCallStackStatStruct( uint8 *pDescribeWriteBuffer, size_t iDescribeMaxLength ) {\
	size_t iWroteBytes = 0;
#define END_STATSTRUCTDESCRIPTION() return iWroteBytes; }


#define DECLARE_CALLSTACKSTATSTRUCT_FIELDDESCRIPTION() static CallStackStatStructDescFuncs *GetStatStructFieldDescriptions( void );

#define BEGIN_STATSTRUCTFIELDDESCRIPTION( className ) CallStackStatStructDescFuncs * className::GetStatStructFieldDescriptions( void ) {\
	typedef className ThisStruct;\
	CallStackStatStructDescFuncs *_pHeadLinkage = NULL;\
	CallStackStatStructDescFuncs **_pLinkageHelperVar = &_pHeadLinkage;

#define _DEFINE_STATSTRUCTFIELD_VARNAME( varName, fieldName, fieldStruct, fieldParmsInParentheses ) static fieldStruct varName##_desc##fieldParmsInParentheses;\
	varName##_desc.m_szFieldName = #fieldName;\
	varName##_desc.m_iFieldOffset = (size_t)(&((ThisStruct *)NULL)->fieldName);\
	varName##_desc.m_pNext = NULL;\
	*_pLinkageHelperVar = &varName##_desc;\
	_pLinkageHelperVar = &varName##_desc.m_pNext;

#define DEFINE_STATSTRUCTFIELD( fieldName, fieldStruct, fieldParmsInParentheses ) _DEFINE_STATSTRUCTFIELD_VARNAME( fieldName, fieldName, fieldStruct, fieldParmsInParentheses )
#define DEFINE_STATSTRUCTFIELD_ARRAYENTRY( arrayName, arrayIndex, fieldStruct, fieldParmsInParentheses ) _DEFINE_STATSTRUCTFIELD_VARNAME( arrayName##_##arrayIndex, arrayName##[##arrayIndex##], fieldStruct, fieldParmsInParentheses )

#define END_STATSTRUCTFIELDDESCRIPTION() static CallStackStatStructDescFuncs *s_pHeadStruct = _pHeadLinkage;\
	return s_pHeadStruct; }

#define WRITE_STATSTRUCT_FIELDDESCRIPTION() iWroteBytes += _CCallStackStatsGatherer_Write_FieldDescriptions( GetStatStructFieldDescriptions(), pDescribeWriteBuffer + iWroteBytes, iDescribeMaxLength - iWroteBytes );
//#define WRITE_STATSTRUCT_FIELDMERGESCRIPT( scriptMergeLanguage ) iWroteBytes += _CCallStackStatsGatherer_Write_FieldMergeScript( GetStatStructFieldDescriptions(), CallStackStatStructDescFuncs::scriptMergeLanguage, pDescribeWriteBuffer + iWroteBytes, iDescribeMaxLength - iWroteBytes );


#else //#if defined( ENABLE_STACK_STATS_GATHERING )

#define DECLARE_CALLSTACKSTATSTRUCT()
#define BEGIN_STATSTRUCTDESCRIPTION( className )
#define END_STATSTRUCTDESCRIPTION()

#define DECLARE_CALLSTACKSTATSTRUCT_FIELDDESCRIPTION()
#define BEGIN_STATSTRUCTFIELDDESCRIPTION( className )
#define DEFINE_STATSTRUCTFIELD( fieldName, fieldStruct, fieldParmsInParentheses )
#define END_STATSTRUCTFIELDDESCRIPTION()

#define WRITE_STATSTRUCT_FIELDDESCRIPTION()
//#define WRITE_STATSTRUCT_FIELDMERGESCRIPT( scriptMergeLanguage )

#endif //#if defined( ENABLE_STACK_STATS_GATHERING )




template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT> CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::GetEntry( void * const CallStack[CAPTUREDCALLSTACKLENGTH] ) //get the entry using some callstack grabbed a while ago. Assumes ALL invalid entries have been nullified
{
#if defined( ENABLE_STACK_STATS_GATHERING )
	return GetEntry( GetEntryIndex( CallStack ) );
#else
	return CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT>( Standardized(), 0 );
#endif
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT> CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::GetEntry( void * const CallStack[CAPTUREDCALLSTACKLENGTH], uint32 iValidEntries ) //same as above, but does the work of nullifying invalid entries
{
#if defined( ENABLE_STACK_STATS_GATHERING )
	void *CleanedCallStack[CAPTUREDCALLSTACKLENGTH];
	size_t i;
	for( i = 0; i < CAPTUREDCALLSTACKLENGTH; ++i )
	{
		CleanedCallStack[i] = CallStack[i];
	}

	for( ; i < CAPTUREDCALLSTACKLENGTH; ++i )
	{
		CleanedCallStack[i] = NULL;
	}
	return GetEntry( GetEntryIndex( CleanedCallStack ) );
#else
	return CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT>( Standardized(), 0 );
#endif
}



template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT> CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::GetEntry( const CCallStackStorage &PushStack )
{
#if defined( ENABLE_STACK_STATS_GATHERING )
	COMPILE_TIME_ASSERT( CAPTUREDCALLSTACKLENGTH <= ARRAYSIZE( PushStack.pStack ) );
	return GetEntry( GetEntryIndex( PushStack.pStack, PushStack.iValidEntries ) );
#else
	return CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT>( Standardized(), 0 );
#endif
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
uint32 CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::GetEntryIndex( const CCallStackStorage &PushStack )
{
#if defined( ENABLE_STACK_STATS_GATHERING )
	COMPILE_TIME_ASSERT( CAPTUREDCALLSTACKLENGTH <= ARRAYSIZE( PushStack.pStack ) );
	return GetEntryIndex( PushStack.pStack, PushStack.iValidEntries );
#else
	return 0;
#endif
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT> CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::GetEntry( uint32 iEntryIndex )
{
#if defined( ENABLE_STACK_STATS_GATHERING )
	return CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT>( Standardized(), iEntryIndex );
#else
	return CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT>( Standardized(), 0 );
#endif
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
uint32 CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::GetEntryIndex( void * const CallStack[CAPTUREDCALLSTACKLENGTH], uint32 iValidEntries ) //index is unchanging, safe to keep and use later (designed for exactly that purpose)
{
#if defined( ENABLE_STACK_STATS_GATHERING )
	AUTO_LOCK_FM( m_IndexMapMutex );
	std::pair<IndexMapIter_t, bool> indexMapIter;
	void *PatchedStack[CAPTUREDCALLSTACKLENGTH];

	//if we have a sub-tree. We'll be splicing it into the original call stack as if it were a return address. Then patching that when we interpret the results later
	//A stack with a sub-tree along the line is treated as distinctly different than one without a sub-tree
	iValidEntries = PatchInSubTrees( CallStack, PatchedStack, iValidEntries );

	Assert( iValidEntries <= CAPTUREDCALLSTACKLENGTH );

	for( int i = iValidEntries; i < CAPTUREDCALLSTACKLENGTH; ++i )
	{
		PatchedStack[i] = NULL;
	}

	indexMapIter = m_IndexMap.insert( IndexMapEntry_t( IndexMapKey_t( PatchedStack ), StatIndex_t() ) );

	if( indexMapIter.first->second.m_Index == -1 )
	{
		m_StatEntryLock.LockForWrite();
		indexMapIter.first->second.m_Index = (unsigned int)m_StatEntries.size();

		m_StatEntries.push_back( StackAndStats_t() );
		memcpy( m_StatEntries[indexMapIter.first->second.m_Index].m_CallStack, PatchedStack, sizeof( void * ) * CAPTUREDCALLSTACKLENGTH );
		m_StatEntryLock.UnlockWrite();
	}

	return indexMapIter.first->second.m_Index;
#else
	return 0;
#endif
}


template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
void CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::GetCallStackForIndex( uint32 iEntryIndex, void *CallStackOut[CAPTUREDCALLSTACKLENGTH] )
{
#if defined( ENABLE_STACK_STATS_GATHERING )
	m_StatEntryLock.LockForRead();
	for( size_t i = 0; i != CAPTUREDCALLSTACKLENGTH; ++i )
	{
		CallStackOut[i] = m_StatEntries[iEntryIndex].m_CallStack[i];
	}
	m_StatEntryLock.UnlockRead();
#else
	for( size_t i = 0; i != CAPTUREDCALLSTACKLENGTH; ++i )
		CallStackOut[i] = NULL;
#endif
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
size_t CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::NumEntries( void ) const
{
#if defined( ENABLE_STACK_STATS_GATHERING )
	return m_StatEntries.size();
#else
	return 0;
#endif
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
bool CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::DumpToFile( const char *szFileName, bool bAllowMemoryAllocations )
{
#if defined( ENABLE_STACK_STATS_GATHERING )
	CCallStackStatsGatherer_Standardized_t StandardThis = Standardized();
	SyncMutexes( this, true );
	bool bRetVal = _CCallStackStatsGatherer_Internal_DumpStatsToFile( szFileName, StandardThis, bAllowMemoryAllocations );
	SyncMutexes( this, false );
	return bRetVal;
#else
	return false;
#endif
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
const CCallStackStatsGatherer_FunctionTable_t &CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::GetFunctionTable( void )
{
	static CCallStackStatsGatherer_FunctionTable_t retVal = 
	{	GetDumpInfo,
		PushSubTree,
		PopSubTree,
#if defined( ENABLE_STACK_STATS_GATHERING )
		STATSTRUCT::DescribeCallStackStatStruct,
#else
		NULL_DescribeCallStackStatStruct,
#endif
		SyncMutexes,
		GetEntry,
		ApplyTreeAccessLock,
		LockEntry };

	return retVal;
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
void CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::GetDumpInfo( void *pThis, const char *&szStructName, size_t &iCapturedStackLength, size_t &iEntrySizeWithStack, void *&pEntries, size_t &iEntryCount, CCallStackStatsGatherer_Standardized_t *&pSubTrees, size_t &iSubTreeCount )
{
	ThisCast *pThisCast = (ThisCast *)pThis;
	iCapturedStackLength = CAPTUREDCALLSTACKLENGTH;
	iEntrySizeWithStack = sizeof( CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::StackAndStats_t );

#if defined( ENABLE_STACK_STATS_GATHERING )
	szStructName = STATSTRUCT::STATSTRUCTSTRINGNAME;
	iEntryCount = pThisCast->m_StatEntries.size();
	pEntries = iEntryCount > 0 ? &pThisCast->m_StatEntries[0] : NULL;
	iSubTreeCount = pThisCast->m_StoredSubTrees.size();
	pSubTrees = iSubTreeCount > 0 ? &pThisCast->m_StoredSubTrees[0] : NULL;
#else
	szStructName = "";
	iEntryCount = 0;
	pEntries = NULL;
	iSubTreeCount = 0;
	pSubTrees = NULL;
#endif
}


template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
void CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::PushSubTree( void *pParent, const CCallStackStatsGatherer_Standardized_t &SubTree, const CCallStackStorage &PushStack )
{
#if defined( ENABLE_STACK_STATS_GATHERING )
	ThisCast *pParentCast = (ThisCast *)pParent;
	PushedSubTree_t pushVal;
	pushVal.iThreadID = ThreadGetCurrentId();
	pushVal.tree = SubTree;

	memcpy( pushVal.Stack, PushStack.pStack, MIN( CAPTUREDCALLSTACKLENGTH, PushStack.iValidEntries ) * sizeof( void * ) );

	for( int i = PushStack.iValidEntries; i < CAPTUREDCALLSTACKLENGTH; ++i )
	{
		pushVal.Stack[i] = NULL;
	}

	pParentCast->m_SubTreeMutex.Lock();
	pParentCast->m_PushedSubTrees.push_back( pushVal );
	pParentCast->m_SubTreeMutex.Unlock();
#endif
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
void CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::PushSubTree( CCallStackStatsGatherer_Standardized_t &Parent, const CCallStackStorage &PushStack )
{
#if defined( ENABLE_STACK_STATS_GATHERING )
	CCallStackStatsGatherer_Standardized_t StandardThis = Standardized();
	Parent.PushSubTree( StandardThis, PushStack );
#endif
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
void CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::PopSubTree( void *pParent )
{
#if defined( ENABLE_STACK_STATS_GATHERING )
	ThisCast *pParentCast = (ThisCast *)pParent;
	pParentCast->m_SubTreeMutex.Lock();
	unsigned long iThreadID = ThreadGetCurrentId();
	
	for( PushedSubTreeVector_t::reverse_iterator treeIter = pParentCast->m_PushedSubTrees.rbegin(); treeIter != pParentCast->m_PushedSubTrees.rend(); ++treeIter )
	{
		if( treeIter->iThreadID == iThreadID )
		{
			++treeIter; //[24.4.1/1] &*(reverse_iterator(i)) == &*(i - 1)
			PushedSubTreeVector_t::iterator eraseIter = treeIter.base();
			pParentCast->m_PushedSubTrees.erase(eraseIter);
			break;
		}
	}

	pParentCast->m_SubTreeMutex.Unlock();
#endif
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
void CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::SyncMutexes( void *pParent, bool bLock ) //true for lock, false for unlock
{
#if defined( ENABLE_STACK_STATS_GATHERING )
	ThisCast *pParentCast = (ThisCast *)pParent;
	if( bLock )
	{
		pParentCast->m_IndexMapMutex.Lock();
		pParentCast->m_SubTreeMutex.Lock();

		for( StoredSubTreeVector_t::iterator treeIter = pParentCast->m_StoredSubTrees.begin(); treeIter != pParentCast->m_StoredSubTrees.end(); ++treeIter )
		{
			treeIter->pFunctionTable->pfn_SyncMutexes( treeIter->pGatherer, true );
		}
	}
	else
	{
		for( StoredSubTreeVector_t::iterator treeIter = pParentCast->m_StoredSubTrees.begin(); treeIter != pParentCast->m_StoredSubTrees.end(); ++treeIter )
		{
			treeIter->pFunctionTable->pfn_SyncMutexes( treeIter->pGatherer, false );
		}

		pParentCast->m_IndexMapMutex.Unlock();
		pParentCast->m_SubTreeMutex.Unlock();
	}
#endif
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
void *CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::GetEntry( void *pParent, uint32 iEntryIndex )
{
	ThisCast *pParentCast = (ThisCast *)pParent;
#if defined( ENABLE_STACK_STATS_GATHERING )	
	return &pParentCast->m_StatEntries[iEntryIndex].m_Stats;
#else
	return &pParentCast->m_SingleEntry;
#endif
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
void CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::ApplyTreeAccessLock( void *pParent, bool bLock )
{	
#if defined( ENABLE_STACK_STATS_GATHERING )
	ThisCast *pParentCast = (ThisCast *)pParent;
	if( bLock )
	{
		pParentCast->m_StatEntryLock.LockForRead();
	}
	else
	{
		pParentCast->m_StatEntryLock.UnlockRead();
	}
#endif
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
void CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::LockEntry( void *pParent, uint32 iEntryIndex, bool bLock )
{	
#if defined( ENABLE_STACK_STATS_GATHERING )
	ThisCast *pParentCast = (ThisCast *)pParent;
	pParentCast->STATMUTEXHANDLER::LockEntry( iEntryIndex, bLock );
#endif
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
void CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::Reset( void )
{
#if defined( ENABLE_STACK_STATS_GATHERING )
	m_StatEntryLock.LockForWrite();
	m_IndexMapMutex.Lock();
	m_SubTreeMutex.Lock();

	m_StatEntries.clear();
	m_IndexMap.clear();
	m_StoredSubTrees.clear();

	m_SubTreeMutex.Unlock();
	m_IndexMapMutex.Unlock();
	m_StatEntryLock.UnlockWrite();
#endif
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::operator CCallStackStatsGatherer_Standardized_t( void )
{
	return CCallStackStatsGatherer_Standardized_t( this, GetFunctionTable() );
}

template <class STATSTRUCT, size_t CAPTUREDCALLSTACKLENGTH, FN_GetCallStack STACKACQUISITIONFUNCTION, typename STATMUTEXHANDLER, template <typename T> class TEMPLATIZEDMEMORYALLOCATOR>
CCallStackStatsGatherer_Standardized_t CCallStackStatsGatherer<STATSTRUCT, CAPTUREDCALLSTACKLENGTH, STACKACQUISITIONFUNCTION, STATMUTEXHANDLER, TEMPLATIZEDMEMORYALLOCATOR>::Standardized( void )
{
	return CCallStackStatsGatherer_Standardized_t( this, GetFunctionTable() );
}


















class PLATFORM_CLASS CallStackStatStructDescFuncs
{
public:
	//description file format
	//1 byte version per entry. Assuming that the field will get more descriptive over time, reserving this now.
	virtual size_t DescribeField( uint8 *pDescribeWriteBuffer, size_t iDescribeMaxLength ) = 0;

	enum MergeScript_Language
	{
		SSMSL_Squirrel, //Only support squirrel for now, theoretically expandable to any vscript supported language
	};

#if 0 //embedded script handling not ready yet
	//this is expected to write a piece of script code into the body of a function that will merge two values of this type
	//The function will have two parameters, "mergeTo", and "mergeFrom". Both are tables with your field defined by name
	//So, an example to merge an integer count defined as "foo" in the stack struct would look like this "mergeTo.foo += mergeFrom.foo\n"
	virtual size_t DescribeMergeOperation( MergeScript_Language scriptLanguage, uint8 *pDescribeWriteBuffer, size_t iDescribeMaxLength ) = 0;
#endif

	//reserve your description field versions here to avoid stomping others
	enum DescribeFieldVersions_t
	{
		DFV_BasicStatStructFieldTypes_t, //Format: 1 byte BasicStatStructFieldTypes_t type, 4 byte field offset, null terminated string field name
	};

	const char *m_szFieldName;
	size_t m_iFieldOffset;
	CallStackStatStructDescFuncs *m_pNext; //needed for how the description macros are laid out. Couldn't figure out a way to store the static struct instances as a static array
};

enum StatStructDescription_LumpID
{
	SSDLID_UNKNOWN,
	SSDLID_STATICINTERPRETER,
	SSDLID_FIELDDESC,
	SSDLID_EMBEDDEDSCRIPT,


	SSDLID_COUNT,
	SSDLID_FORCE_UINT32 = 0xFFFFFFFF,
};

enum BasicStatStructFieldTypes_t
{
	BSSFT_UNKNOWN,
	BSSFT_BOOL,
	BSSFT_INT8,
	BSSFT_UINT8,
	BSSFT_INT16,
	BSSFT_UINT16,
	BSSFT_INT32,
	BSSFT_UINT32,
	BSSFT_INT64,
	BSSFT_UINT64,
	BSSFT_FLOAT,
	BSSFT_DOUBLE,
	BSSFT_VECTOR2D,
	BSSFT_VECTOR3D,
	BSSFT_VECTOR4D,

	BSSFT_COUNT,
};

#define BSSFT_INT (sizeof( int ) == sizeof( int32 ) ? BSSFT_INT32 : BSSFT_INT64)
#define BSSFT_UINT (sizeof( unsigned int ) == sizeof( uint32 ) ? BSSFT_UINT32 : BSSFT_UINT64)
#define BSSFT_SIZE_T (sizeof( size_t ) == sizeof( uint32 ) ? BSSFT_UINT32 : BSSFT_UINT64)

enum BasicStatStructFieldCombineMethods_t
{
	BSSFCM_UNKNOWN,
	BSSFCM_CUSTOM, //rely on some outside handler
	BSSFCM_ADD, //add the values
	//BSSFCM_SUBTRACT, //what would subtract even mean? which one from which?
	BSSFCM_MAX, //keep max value
	BSSFCM_MIN, //keep min value
	BSSFCM_AND, // &= , Non-integer behavior undefined
	BSSFCM_OR, // |= , Non-integer behavior undefined
	BSSFCM_XOR, // ^= , Non-integer behavior undefined
	/*BSSFCM_LIST, //keep a list of each value (probably complicated)*/

	BSSFCM_COUNT,
};

class PLATFORM_CLASS BasicStatStructFieldDesc : public CallStackStatStructDescFuncs
{
public:
	BasicStatStructFieldDesc( BasicStatStructFieldTypes_t type, BasicStatStructFieldCombineMethods_t combineMethod ) : m_Type(type), m_Combine(combineMethod) {};
	size_t DescribeField( uint8 *pDescribeWriteBuffer, size_t iDescribeMaxLength );
#if 0 //embedded script handling not ready yet
	size_t DescribeMergeOperation( MergeScript_Language scriptLanguage, uint8 *pDescribeWriteBuffer, size_t iDescribeMaxLength );
#endif

	BasicStatStructFieldTypes_t m_Type;
	BasicStatStructFieldCombineMethods_t m_Combine;	
};







//struct is locked in place while you're holding onto one of these. Get a base, then create one of these from it
template <class STATSTRUCT>
class CCallStackStatsGatherer_StructAccessor_AutoLock : CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT>
{
public:
	CCallStackStatsGatherer_StructAccessor_AutoLock( CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT> &copyFrom )
		: CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT>( copyFrom )
	{
		this->m_Gatherer.pFunctionTable->pfn_ApplyTreeAccessLock( this->m_Gatherer.pGatherer, true );
		this->m_Gatherer.pFunctionTable->pfn_LockEntry( this->m_Gatherer.pGatherer, this->m_iEntryIndex, true );
		this->m_pStruct = (STATSTRUCT *)this->m_Gatherer.pFunctionTable->pfn_GetEntry( this->m_Gatherer.pGatherer, this->m_iEntryIndex );
	}

	~CCallStackStatsGatherer_StructAccessor_AutoLock( void )
	{
		this->m_Gatherer.pFunctionTable->pfn_LockEntry( this->m_Gatherer.pGatherer, this->m_iEntryIndex, false );
		this->m_Gatherer.pFunctionTable->pfn_ApplyTreeAccessLock( this->m_Gatherer.pGatherer, false );
	}

	STATSTRUCT *operator->()
	{
		return this->m_pStruct;
	}

	STATSTRUCT *GetStruct( void ) //do not hold this pointer outside the lock period
	{
		return this->m_pStruct;
	}

protected:
	STATSTRUCT *m_pStruct;
};


//struct is locked in place only between Lock() and paired Unlock() calls. Get a base, then create one of these from it.
//It's safe to hold onto this for an extended period of time. The entry index is unchanging in the gatherer tree.
template <class STATSTRUCT>
class CCallStackStatsGatherer_StructAccessor_Manual : CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT>
{
public:
	CCallStackStatsGatherer_StructAccessor_Manual( CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT> &copyFrom )
		: CCallStackStatsGatherer_StructAccessor_Base<STATSTRUCT>( copyFrom ), m_pStruct( NULL )
	{ }

	STATSTRUCT *operator->()
	{
		return this->m_pStruct; //NULL while entry is not locked.
	}

	STATSTRUCT *GetStruct( void ) //do not hold this pointer outside the lock period
	{
		return this->m_pStruct; //NULL while entry is not locked.
	}

	void Lock( void )
	{
		this->m_Gatherer.pFunctionTable->pfn_ApplyTreeAccessLock( this->m_Gatherer.pGatherer, true );
		this->m_Gatherer.pFunctionTable->pfn_LockEntry( this->m_Gatherer.pGatherer, this->m_iEntryIndex, true );
		this->m_pStruct = (STATSTRUCT *)this->m_Gatherer.pFunctionTable->pfn_GetEntry( this->m_Gatherer.pGatherer, this->m_iEntryIndex );
	}

	void Unlock( void )
	{
		this->m_pStruct = NULL;
		this->m_Gatherer.pFunctionTable->pfn_LockEntry( this->m_Gatherer.pGatherer, this->m_iEntryIndex, false );
		this->m_Gatherer.pFunctionTable->pfn_ApplyTreeAccessLock( this->m_Gatherer.pGatherer, false );
	}

protected:
	STATSTRUCT *m_pStruct;
};





class CCallStackStats_PushSubTree_AutoPop
{
public:
	CCallStackStats_PushSubTree_AutoPop( const CCallStackStatsGatherer_Standardized_t &Parent, const CCallStackStatsGatherer_Standardized_t &Child, const CCallStackStorage &PushStack = CCallStackStorage() )
		: m_PopFrom( Parent )
	{
		Parent.pFunctionTable->pfn_PushSubTree( Parent.pGatherer, Child, PushStack );
	}
	~CCallStackStats_PushSubTree_AutoPop( void )
	{
		m_PopFrom.PopSubTree();
	}

	CCallStackStatsGatherer_Standardized_t m_PopFrom;
};


#endif //#ifndef TIER0_STACKTOOLS_H

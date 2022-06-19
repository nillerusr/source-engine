//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines a symbol table
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#pragma warning (disable:4514)

#include "utlsymbol.h"
#include "tier0/threadtools.h"
#include "stringpool.h"
#include "generichash.h"
#include "tier0/vprof.h"
#include <stddef.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define INVALID_STRING_INDEX CStringPoolIndex( 0xFFFF, 0xFFFF )

#define MIN_STRING_POOL_SIZE	2048

//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------

CUtlSymbolTableMT* CUtlSymbol::s_pSymbolTable = 0; 
bool CUtlSymbol::s_bAllowStaticSymbolTable = true;


//-----------------------------------------------------------------------------
// symbol methods
//-----------------------------------------------------------------------------

void CUtlSymbol::Initialize()
{
	// If this assert fails, then the module that this call is in has chosen to disallow
	// use of the static symbol table. Usually, it's to prevent confusion because it's easy
	// to accidentally use the global symbol table when you really want to use a specific one.
	Assert( s_bAllowStaticSymbolTable );

	// necessary to allow us to create global symbols
	static bool symbolsInitialized = false;
	if (!symbolsInitialized)
	{
		s_pSymbolTable = new CUtlSymbolTableMT;
		symbolsInitialized = true;
	}
}

void CUtlSymbol::LockTableForRead()
{
	Initialize();
	s_pSymbolTable->LockForRead();
}

void CUtlSymbol::UnlockTableForRead()
{
	s_pSymbolTable->UnlockForRead();
}

//-----------------------------------------------------------------------------
// Purpose: Singleton to delete table on exit from module
//-----------------------------------------------------------------------------
class CCleanupUtlSymbolTable
{
public:
	~CCleanupUtlSymbolTable()
	{
		delete CUtlSymbol::s_pSymbolTable;
		CUtlSymbol::s_pSymbolTable = NULL;
	}
};

static CCleanupUtlSymbolTable g_CleanupSymbolTable;

CUtlSymbolTableMT* CUtlSymbol::CurrTable()
{
	Initialize();
	return s_pSymbolTable; 
}


//-----------------------------------------------------------------------------
// string->symbol->string
//-----------------------------------------------------------------------------

CUtlSymbol::CUtlSymbol( const char* pStr )
{
	m_Id = CurrTable()->AddString( pStr );
}

const char* CUtlSymbol::String( ) const
{
	return CurrTable()->String(m_Id);
}

const char* CUtlSymbol::StringNoLock( ) const
{
	return CurrTable()->StringNoLock(m_Id);
}

void CUtlSymbol::DisableStaticSymbolTable()
{
	s_bAllowStaticSymbolTable = false;
}

//-----------------------------------------------------------------------------
// checks if the symbol matches a string
//-----------------------------------------------------------------------------

bool CUtlSymbol::operator==( const char* pStr ) const
{
	if (m_Id == UTL_INVAL_SYMBOL) 
		return false;
	return strcmp( String(), pStr ) == 0;
}



//-----------------------------------------------------------------------------
// symbol table stuff
//-----------------------------------------------------------------------------
inline const char* CUtlSymbolTable::DecoratedStringFromIndex( const CStringPoolIndex &index ) const
{
	Assert( index.m_iPool < m_StringPools.Count() );
	Assert( index.m_iOffset < m_StringPools[index.m_iPool]->m_TotalLen );

	// step over the hash decorating the beginning of the string
	return (&m_StringPools[index.m_iPool]->m_Data[index.m_iOffset]);
}

inline const char* CUtlSymbolTable::StringFromIndex( const CStringPoolIndex &index ) const
{
	// step over the hash decorating the beginning of the string
	return DecoratedStringFromIndex(index)+sizeof(hashDecoration_t);
}

// The first two bytes of each string in the pool are actually the hash for that string.
// Thus we compare hashes rather than entire strings for a significant perf benefit.
// However since there is a high rate of hash collision we must still compare strings
// if the hashes match.
bool CUtlSymbolTable::CLess::operator()( const CStringPoolIndex &i1, const CStringPoolIndex &i2 ) const
{
	// Need to do pointer math because CUtlSymbolTable is used in CUtlVectors, and hence
	// can be arbitrarily moved in memory on a realloc. Yes, this is portable. In reality,
	// right now at least, because m_LessFunc is the first member of CUtlRBTree, and m_Lookup
	// is the first member of CUtlSymbolTabke, this == pTable
	CUtlSymbolTable *pTable = (CUtlSymbolTable *)( (byte *)this - offsetof(CUtlSymbolTable::CTree, m_LessFunc) ) - offsetof(CUtlSymbolTable, m_Lookup );

#if 1 // using the hashes
	const char *str1, *str2;
	hashDecoration_t hash1, hash2;

	if (i1 == INVALID_STRING_INDEX)
	{
		str1 = pTable->m_pUserSearchString;
		hash1 = pTable->m_nUserSearchStringHash;
	}
	else
	{
		str1 = pTable->DecoratedStringFromIndex( i1 );
		hashDecoration_t storedHash = *reinterpret_cast<const hashDecoration_t *>(str1);
		str1 += sizeof(hashDecoration_t);
		AssertMsg2( storedHash == ( !pTable->m_bInsensitive ? HashString(str1) : HashStringCaseless(str1) ),
			"The stored hash (%d) for symbol %s is not correct.", storedHash, str1 );
		hash1 = storedHash;
	}

	if (i2 == INVALID_STRING_INDEX)
	{
		str2 = pTable->m_pUserSearchString;
		hash2 = pTable->m_nUserSearchStringHash;
	}
	else
	{
		str2 = pTable->DecoratedStringFromIndex( i2 );
		hashDecoration_t storedHash = *reinterpret_cast<const hashDecoration_t *>(str2);
		str2 += sizeof(hashDecoration_t);
		AssertMsg2( storedHash == ( !pTable->m_bInsensitive ? HashString(str2) : HashStringCaseless(str2) ),
			"The stored hash (%d) for symbol '%s' is not correct.", storedHash, str2 );
		hash2 = storedHash;
	}

	// compare the hashes
	if ( hash1 == hash2 )
	{
		if ( !str1 && str2 )
			return 1;
		if ( !str2 && str1 )
			return -1;
		if ( !str1 && !str2 )
			return 0;
		
		// if the hashes match compare the strings
		if ( !pTable->m_bInsensitive )
			return strcmp( str1, str2 ) < 0;
		else
			return V_stricmp( str1, str2 ) < 0;
	}
	else
	{
		return hash1 < hash2;
	}

#else // not using the hashes, just comparing strings
	const char* str1 = (i1 == INVALID_STRING_INDEX) ? pTable->m_pUserSearchString :
		pTable->StringFromIndex( i1 );
	const char* str2 = (i2 == INVALID_STRING_INDEX) ? pTable->m_pUserSearchString :
		pTable->StringFromIndex( i2 );

	if ( !str1 && str2 )
		return 1;
	if ( !str2 && str1 )
		return -1;
	if ( !str1 && !str2 )
		return 0;
	if ( !pTable->m_bInsensitive )
		return strcmp( str1, str2 ) < 0;
	else
		return strcmpi( str1, str2 ) < 0;
#endif
}


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CUtlSymbolTable::CUtlSymbolTable( int growSize, int initSize, bool caseInsensitive ) : 
	m_Lookup( growSize, initSize ), m_bInsensitive( caseInsensitive ), m_StringPools( 8 )
{
}

CUtlSymbolTable::~CUtlSymbolTable()
{
	// Release the stringpool string data
	RemoveAll();
}


CUtlSymbol CUtlSymbolTable::Find( const char* pString ) const
{	
	VPROF( "CUtlSymbol::Find" );
	if (!pString)
		return CUtlSymbol();
	
	// Store a special context used to help with insertion
	m_pUserSearchString = pString;
	m_nUserSearchStringHash = m_bInsensitive ? HashStringCaseless(pString) : HashString(pString) ;
	
	// Passing this special invalid symbol makes the comparison function
	// use the string passed in the context
	UtlSymId_t idx = m_Lookup.Find( INVALID_STRING_INDEX );

#ifdef _DEBUG
	m_pUserSearchString = NULL;
	m_nUserSearchStringHash = 0;
#endif

	return CUtlSymbol( idx );
}


int CUtlSymbolTable::FindPoolWithSpace( int len )	const
{
	for ( int i=0; i < m_StringPools.Count(); i++ )
	{
		StringPool_t *pPool = m_StringPools[i];

		if ( (pPool->m_TotalLen - pPool->m_SpaceUsed) >= len )
		{
			return i;
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Finds and/or creates a symbol based on the string
//-----------------------------------------------------------------------------

CUtlSymbol CUtlSymbolTable::AddString( const char* pString )
{
	VPROF("CUtlSymbol::AddString");
	if (!pString) 
		return CUtlSymbol( UTL_INVAL_SYMBOL );

	CUtlSymbol id = Find( pString );
	
	if (id.IsValid())
		return id;

	int lenString = strlen(pString) + 1; // length of just the string
	int lenDecorated = lenString + sizeof(hashDecoration_t); // and with its hash decoration
	// make sure that all strings are aligned on 2-byte boundaries so the hashes will read correctly
	COMPILE_TIME_ASSERT(sizeof(hashDecoration_t) == 2);
	lenDecorated = (lenDecorated + 1) & (~0x01); // round up to nearest multiple of 2

	// Find a pool with space for this string, or allocate a new one.
	int iPool = FindPoolWithSpace( lenDecorated );
	if ( iPool == -1 )
	{
		// Add a new pool.
		int newPoolSize = MAX( lenDecorated + sizeof( StringPool_t ), MIN_STRING_POOL_SIZE );
		StringPool_t *pPool = (StringPool_t*)malloc( newPoolSize );
		pPool->m_TotalLen = newPoolSize - sizeof( StringPool_t );
		pPool->m_SpaceUsed = 0;
		iPool = m_StringPools.AddToTail( pPool );
	}

	// Compute a hash
	hashDecoration_t hash = m_bInsensitive ? HashStringCaseless(pString) : HashString(pString) ;

	// Copy the string in.
	StringPool_t *pPool = m_StringPools[iPool];
	Assert( pPool->m_SpaceUsed < 0xFFFF );	// This should never happen, because if we had a string > 64k, it
											// would have been given its entire own pool.
	
	unsigned short iStringOffset = pPool->m_SpaceUsed;
	const char *startingAddr = &pPool->m_Data[pPool->m_SpaceUsed];

	// store the hash at the head of the string
	*((hashDecoration_t *)(startingAddr)) = hash;
	// and then the string's data
	memcpy( (void *)(startingAddr + sizeof(hashDecoration_t)), pString, lenString );
	pPool->m_SpaceUsed += lenDecorated;

	// insert the string into the vector.
	CStringPoolIndex index;
	index.m_iPool = iPool;
	index.m_iOffset = iStringOffset;

	MEM_ALLOC_CREDIT();
	UtlSymId_t idx = m_Lookup.Insert( index );
	return CUtlSymbol( idx );
}


//-----------------------------------------------------------------------------
// Look up the string associated with a particular symbol
//-----------------------------------------------------------------------------

const char* CUtlSymbolTable::String( CUtlSymbol id ) const
{
	if (!id.IsValid()) 
		return "";
	
	Assert( m_Lookup.IsValidIndex((UtlSymId_t)id) );
	return StringFromIndex( m_Lookup[id] );
}


//-----------------------------------------------------------------------------
// Remove all symbols in the table.
//-----------------------------------------------------------------------------

void CUtlSymbolTable::RemoveAll()
{
	m_Lookup.Purge();
	
	for ( int i=0; i < m_StringPools.Count(); i++ )
		free( m_StringPools[i] );

	m_StringPools.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
// Output : FileNameHandle_t
//-----------------------------------------------------------------------------
FileNameHandle_t CUtlFilenameSymbolTable::FindOrAddFileName( const char *pFileName )
{
	if ( !pFileName )
	{
		return NULL;
	}

	// find first
	FileNameHandle_t hFileName = FindFileName( pFileName );
	if ( hFileName )
	{
		return hFileName;
	}

	// Fix slashes+dotslashes and make lower case first..
	char fn[ MAX_PATH ];
	Q_strncpy( fn, pFileName, sizeof( fn ) );
	Q_RemoveDotSlashes( fn );
#ifdef _WIN32
	strlwr( fn );
#endif

	// Split the filename into constituent parts
	char basepath[ MAX_PATH ];
	Q_ExtractFilePath( fn, basepath, sizeof( basepath ) );
	char filename[ MAX_PATH ];
	Q_strncpy( filename, fn + Q_strlen( basepath ), sizeof( filename ) );

	// not found, lock and look again
	FileNameHandleInternal_t handle;
	m_lock.LockForWrite();
	handle.SetPath( m_PathStringPool.FindStringHandle( basepath ) );
	handle.SetFile( m_FileStringPool.FindStringHandle( filename ) );
	if ( handle.GetPath() && handle.GetFile() )
	{
		// found
		m_lock.UnlockWrite();
		return *( FileNameHandle_t * )( &handle );
	}

	// safely add it
	handle.SetPath( m_PathStringPool.ReferenceStringHandle( basepath ) );
	handle.SetFile( m_FileStringPool.ReferenceStringHandle( filename ) );
	m_lock.UnlockWrite();

	return *( FileNameHandle_t * )( &handle );
}

FileNameHandle_t CUtlFilenameSymbolTable::FindFileName( const char *pFileName )
{
	if ( !pFileName )
	{
		return NULL;
	}

	// Fix slashes+dotslashes and make lower case first..
	char fn[ MAX_PATH ];
	Q_strncpy( fn, pFileName, sizeof( fn ) );
	Q_RemoveDotSlashes( fn );
#ifdef _WIN32
	strlwr( fn );
#endif

	// Split the filename into constituent parts
	char basepath[ MAX_PATH ];
	Q_ExtractFilePath( fn, basepath, sizeof( basepath ) );
	char filename[ MAX_PATH ];
	Q_strncpy( filename, fn + Q_strlen( basepath ), sizeof( filename ) );

	FileNameHandleInternal_t handle;

	m_lock.LockForRead();
	handle.SetPath( m_PathStringPool.FindStringHandle( basepath ) );
	handle.SetFile( m_FileStringPool.FindStringHandle( filename ) );
	m_lock.UnlockRead();


	if ( ( handle.GetPath() == 0 )  || ( handle.GetFile() == 0 ) )
		return NULL;

	return *( FileNameHandle_t * )( &handle );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
// Output : const char
//-----------------------------------------------------------------------------
bool CUtlFilenameSymbolTable::String( const FileNameHandle_t& handle, char *buf, int buflen )
{
	buf[ 0 ] = 0;

	FileNameHandleInternal_t *internalFileHandle = ( FileNameHandleInternal_t * )&handle;
	if ( !internalFileHandle )
	{
		return false;
	}

	m_lock.LockForRead();
	const char *path = m_PathStringPool.HandleToString( internalFileHandle->GetPath() );
	const char *fn = m_FileStringPool.HandleToString( internalFileHandle->GetFile() );
	m_lock.UnlockRead();

	if ( !path || !fn )
	{
		return false;
	}

	Q_strncpy( buf, path, buflen );
	Q_strncat( buf, fn, buflen, COPY_ALL_CHARACTERS );

	return true;
}

void CUtlFilenameSymbolTable::RemoveAll()
{
	m_PathStringPool.FreeAll();
	m_FileStringPool.FreeAll();
}

void CUtlFilenameSymbolTable::SpewStrings()
{
	m_lock.LockForRead();
	m_PathStringPool.SpewStrings();
	m_FileStringPool.SpewStrings();
	m_lock.UnlockRead();
}

bool CUtlFilenameSymbolTable::SaveToBuffer( CUtlBuffer &buffer )
{
	m_lock.LockForRead();
	bool bResult = m_PathStringPool.SaveToBuffer( buffer );
	bResult = bResult && m_FileStringPool.SaveToBuffer( buffer );
	m_lock.UnlockRead();

	return bResult;
}

bool CUtlFilenameSymbolTable::RestoreFromBuffer( CUtlBuffer &buffer )
{
	m_lock.LockForWrite();
	bool bResult = m_PathStringPool.RestoreFromBuffer( buffer );
	bResult = bResult && m_FileStringPool.RestoreFromBuffer( buffer );
	m_lock.UnlockWrite();

	return bResult;
}

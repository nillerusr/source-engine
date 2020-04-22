//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <vstdlib/ikeyvaluessystem.h>
#include <keyvalues.h>
#include "tier1/mempool.h"
#include "utlsymbol.h"
#include "utlmap.h"
#include "tier0/threadtools.h"
#include "tier1/memstack.h"
#include "tier1/convar.h"

#ifdef _PS3
#include "ps3/ps3_core.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#ifdef NO_SBH // no need to pool if using tier0 small block heap
#define KEYVALUES_USE_POOL 1
#endif

//
// Defines platform-endian-specific macros:
// MEM_4BYTES_AS_0_AND_3BYTES :  present a 4 byte uint32 as a memory
//                               layout where first memory byte is zero
//                               and the other 3 bytes represent value
// MEM_4BYTES_FROM_0_AND_3BYTES: unpack from memory with first zero byte
//                               and 3 value bytes the original uint32 value
//
// used for efficiently reading/writing storing 3 byte values into memory
// region immediately following a null-byte-terminated string, essentially
// sharing the null-byte-terminator with the first memory byte
//
#if defined( PLAT_LITTLE_ENDIAN )
// Number in memory has lowest-byte in front, use shifts to make it zero
#define MEM_4BYTES_AS_0_AND_3BYTES( x4bytes ) ( ( (uint32) (x4bytes) ) << 8 )
#define MEM_4BYTES_FROM_0_AND_3BYTES( x03bytes ) ( ( (uint32) (x03bytes) ) >> 8 )
#endif
#if defined( PLAT_BIG_ENDIAN )
// Number in memory has highest-byte in front, use masking to make it zero
#define MEM_4BYTES_AS_0_AND_3BYTES( x4bytes ) ( ( (uint32) (x4bytes) ) & 0x00FFFFFF )
#define MEM_4BYTES_FROM_0_AND_3BYTES( x03bytes ) ( ( (uint32) (x03bytes) ) & 0x00FFFFFF )
#endif

//-----------------------------------------------------------------------------
// Purpose: Central storage point for KeyValues memory and symbols
//-----------------------------------------------------------------------------
class CKeyValuesSystem : public IKeyValuesSystem
{
public:
	CKeyValuesSystem();
	~CKeyValuesSystem();

	// registers the size of the KeyValues in the specified instance
	// so it can build a properly sized memory pool for the KeyValues objects
	// the sizes will usually never differ but this is for versioning safety
	void RegisterSizeofKeyValues(int size);

	// allocates/frees a KeyValues object from the shared mempool
	void *AllocKeyValuesMemory(int size);
	void FreeKeyValuesMemory(void *pMem);

	// symbol table access (used for key names)
	HKeySymbol GetSymbolForString( const char *name, bool bCreate );
	const char *GetStringForSymbol(HKeySymbol symbol);

	// returns the wide version of ansi, also does the lookup on #'d strings
	void GetLocalizedFromANSI( const char *ansi, wchar_t *outBuf, int unicodeBufferSizeInBytes);
	void GetANSIFromLocalized( const wchar_t *wchar, char *outBuf, int ansiBufferSizeInBytes );

	// for debugging, adds KeyValues record into global list so we can track memory leaks
	virtual void AddKeyValuesToMemoryLeakList(void *pMem, HKeySymbol name);
	virtual void RemoveKeyValuesFromMemoryLeakList(void *pMem);

	// set/get a value for keyvalues resolution symbol
	// e.g.: SetKeyValuesExpressionSymbol( "LOWVIOLENCE", true ) - enables [$LOWVIOLENCE]
	virtual void SetKeyValuesExpressionSymbol( const char *name, bool bValue );
	virtual bool GetKeyValuesExpressionSymbol( const char *name );

	// symbol table access from code with case-preserving requirements (used for key names)
	virtual HKeySymbol GetSymbolForStringCaseSensitive( HKeySymbol &hCaseInsensitiveSymbol, const char *name, bool bCreate = true );

private:
#ifdef KEYVALUES_USE_POOL
	CUtlMemoryPool *m_pMemPool;
#endif
	int m_iMaxKeyValuesSize;

	// string hash table
	/*
	Here's the way key values system data structures are laid out:
	hash table with 2047 hash buckets:
	[0] { hash_item_t }
	[1]
	[2]
	...
	each hash_item_t's stringIndex is an offset in m_Strings memory
	at that offset we store the actual null-terminated string followed
	by another 3 bytes for an alternative capitalization.
	These 3 trailing bytes are set to 0 if no alternative capitalization
	variants are present in the dictionary.
	These trailing 3 bytes are interpreted as stringIndex into m_Strings
	memory for the next	alternative capitalization

	Getting a string value by HKeySymbol : constant time access at the
	string memory represented by stringIndex

	Getting a symbol for a string value:
	1)	compute the hash
	2)	start walking the hash-bucket using special version of stricmp
		until a case insensitive match is found
	3a) for case-insensitive lookup return the found stringIndex
	3b) for case-sensitive lookup keep walking the list of alternative
		capitalizations using strcmp until exact case match is found
	*/
	CMemoryStack m_Strings;
	struct hash_item_t
	{
		int stringIndex;
		hash_item_t *next;
	};
	CUtlMemoryPool m_HashItemMemPool;
	CUtlVector<hash_item_t> m_HashTable;
	int CaseInsensitiveHash(const char *string, int iBounds);

	struct MemoryLeakTracker_t
	{
		int nameIndex;
		void *pMem;
	};
	static bool MemoryLeakTrackerLessFunc( const MemoryLeakTracker_t &lhs, const MemoryLeakTracker_t &rhs )
	{
		return lhs.pMem < rhs.pMem;
	}
	CUtlRBTree<MemoryLeakTracker_t, int> m_KeyValuesTrackingList;

	CUtlMap< HKeySymbol, bool > m_KvConditionalSymbolTable;

	CThreadFastMutex m_mutex;
};

// EXPOSE_SINGLE_INTERFACE(CKeyValuesSystem, IKeyValuesSystem, KEYVALUES_INTERFACE_VERSION);

//-----------------------------------------------------------------------------
// Instance singleton and expose interface to rest of code
//-----------------------------------------------------------------------------
static CKeyValuesSystem g_KeyValuesSystem;

IKeyValuesSystem *KeyValuesSystem()
{
	return &g_KeyValuesSystem;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CKeyValuesSystem::CKeyValuesSystem() :
	m_HashItemMemPool(sizeof(hash_item_t), 64, CUtlMemoryPool::GROW_FAST, "CKeyValuesSystem::m_HashItemMemPool"),
	m_KeyValuesTrackingList(0, 0, MemoryLeakTrackerLessFunc),
	m_KvConditionalSymbolTable( DefLessFunc( HKeySymbol ) )
{
	MEM_ALLOC_CREDIT();
	// initialize hash table
	m_HashTable.AddMultipleToTail(2047);
	for (int i = 0; i < m_HashTable.Count(); i++)
	{
		m_HashTable[i].stringIndex = 0;
		m_HashTable[i].next = NULL;
	}

	m_Strings.Init( "CKeyValuesSystem::m_Strings", 4*1024*1024, 64*1024, 0, 4 );
	// Make 0 stringIndex to never be returned, by allocating
	// and wasting minimal number of alignment bytes now:
	char *pszEmpty = ((char *)m_Strings.Alloc(1));
	*pszEmpty = 0;

#ifdef KEYVALUES_USE_POOL
	m_pMemPool = NULL;
#endif
	m_iMaxKeyValuesSize = sizeof(KeyValues);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CKeyValuesSystem::~CKeyValuesSystem()
{
#ifdef KEYVALUES_USE_POOL
#ifdef _DEBUG
	// display any memory leaks
	if (m_pMemPool && m_pMemPool->Count() > 0)
	{
		DevMsg("Leaked KeyValues blocks: %d\n", m_pMemPool->Count());
	}

	// iterate all the existing keyvalues displaying their names
	for (int i = 0; i < m_KeyValuesTrackingList.MaxElement(); i++)
	{
		if (m_KeyValuesTrackingList.IsValidIndex(i))
		{
			DevMsg("\tleaked KeyValues(%s)\n", &m_Strings[m_KeyValuesTrackingList[i].nameIndex]);
		}
	}
#endif

	delete m_pMemPool;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: registers the size of the KeyValues in the specified instance
//			so it can build a properly sized memory pool for the KeyValues objects
//			the sizes will usually never differ but this is for versioning safety
//-----------------------------------------------------------------------------
void CKeyValuesSystem::RegisterSizeofKeyValues(int size)
{
	if (size > m_iMaxKeyValuesSize)
	{
		m_iMaxKeyValuesSize = size;
	}
}

static void KVLeak( char const *fmt, ... )
{
	va_list argptr; 
    char data[1024];
    
    va_start(argptr, fmt);
    V_vsnprintf(data, sizeof( data ), fmt, argptr);
    va_end(argptr);

	Msg( data );
}

//-----------------------------------------------------------------------------
// Purpose: allocates a KeyValues object from the shared mempool
//-----------------------------------------------------------------------------
void *CKeyValuesSystem::AllocKeyValuesMemory(int size)
{
#ifdef KEYVALUES_USE_POOL
	// allocate, if we don't have one yet
	if (!m_pMemPool)
	{
		m_pMemPool = new CUtlMemoryPool(m_iMaxKeyValuesSize, 1024, CUtlMemoryPool::GROW_FAST, "CKeyValuesSystem::m_pMemPool" );
		m_pMemPool->SetErrorReportFunc( KVLeak );
	}

	return m_pMemPool->Alloc(size);
#else
	return malloc( size );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: frees a KeyValues object from the shared mempool
//-----------------------------------------------------------------------------
void CKeyValuesSystem::FreeKeyValuesMemory(void *pMem)
{
#ifdef KEYVALUES_USE_POOL
	m_pMemPool->Free(pMem);
#else
	free( pMem );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: symbol table access (used for key names)
//-----------------------------------------------------------------------------
HKeySymbol CKeyValuesSystem::GetSymbolForString( const char *name, bool bCreate )
{
	if ( !name )
	{
		return (-1);
	}

	AUTO_LOCK( m_mutex );
	MEM_ALLOC_CREDIT();

	int hash = CaseInsensitiveHash(name, m_HashTable.Count());
	int i = 0;
	hash_item_t *item = &m_HashTable[hash];
	while (1)
	{
		if (!stricmp(name, (char *)m_Strings.GetBase() + item->stringIndex ))
		{
			return (HKeySymbol)item->stringIndex;
		}

		i++;

		if (item->next == NULL)
		{
			if ( !bCreate )
			{
				// not found
				return -1;
			}

			// we're not in the table
			if (item->stringIndex != 0)
			{
				// first item is used, an new item
				item->next = (hash_item_t *)m_HashItemMemPool.Alloc(sizeof(hash_item_t));
				item = item->next;
			}

			// build up the new item
			item->next = NULL;
			int numStringBytes = strlen(name);
			char *pString = (char *)m_Strings.Alloc( numStringBytes + 1 + 3 );
			if ( !pString )
			{
				Error( "Out of keyvalue string space" );
				return -1;
			}
			item->stringIndex = pString - (char *)m_Strings.GetBase();
			V_memcpy( pString, name, numStringBytes );
			* reinterpret_cast< uint32 * >( pString + numStringBytes ) = 0;	// string null-terminator + 3 alternative spelling bytes
			return (HKeySymbol)item->stringIndex;
		}

		item = item->next;
	}

	// shouldn't be able to get here
	Assert(0);
	return (-1);
}

//-----------------------------------------------------------------------------
// Purpose: symbol table access (used for key names)
//-----------------------------------------------------------------------------
HKeySymbol CKeyValuesSystem::GetSymbolForStringCaseSensitive( HKeySymbol &hCaseInsensitiveSymbol, const char *name, bool bCreate )
{
	if ( !name )
	{
		return (-1);
	}

	AUTO_LOCK( m_mutex );
	MEM_ALLOC_CREDIT();

	int hash = CaseInsensitiveHash(name, m_HashTable.Count());
	int numNameStringBytes = -1;
	int i = 0;
	hash_item_t *item = &m_HashTable[hash];
	while (1)
	{
		char *pCompareString = (char *)m_Strings.GetBase() + item->stringIndex;
		int iResult = _V_stricmp_NegativeForUnequal( name, pCompareString );
		if ( iResult == 0 )
		{
			// strings are exactly equal matching every letter's case
			hCaseInsensitiveSymbol = (HKeySymbol)item->stringIndex;
			return (HKeySymbol)item->stringIndex;
		}
		else if ( iResult > 0 )
		{
			// strings are equal in a case-insensitive compare, but have different case for some letters
			// Need to walk the case-resolving chain
			numNameStringBytes = V_strlen( pCompareString );
			uint32 *pnCaseResolveIndex = reinterpret_cast< uint32 * >( pCompareString + numNameStringBytes );
			hCaseInsensitiveSymbol = (HKeySymbol)item->stringIndex;
			while ( int nAlternativeStringIndex = MEM_4BYTES_FROM_0_AND_3BYTES( *pnCaseResolveIndex ) )
			{
				pCompareString = (char *)m_Strings.GetBase() + nAlternativeStringIndex;
				int iResult = strcmp( name, pCompareString );
				if ( !iResult )
				{
					// found an exact match
					return (HKeySymbol)nAlternativeStringIndex;
				}
				// Keep traversing alternative case-resolving chain
				pnCaseResolveIndex = reinterpret_cast< uint32 * >( pCompareString + numNameStringBytes );
			}
			// Reached the end of alternative case-resolving chain, pnCaseResolveIndex is pointing at 0 bytes
			// indicating no further alternative stringIndex
			if ( !bCreate )
			{
				// If we aren't interested in creating the actual string index,
				// then return symbol with default capitalization
				// NOTE: this is not correct value, but it cannot be used to create a new value anyway,
				// only for locating a pre-existing value and lookups are case-insensitive
				return (HKeySymbol)item->stringIndex;
			}
			else
			{
				char *pString = (char *)m_Strings.Alloc( numNameStringBytes + 1 + 3 );
				if ( !pString )
				{
					Error( "Out of keyvalue string space" );
					return -1;
				}
				int nNewAlternativeStringIndex = pString - (char *)m_Strings.GetBase();
				V_memcpy( pString, name, numNameStringBytes );
				* reinterpret_cast< uint32 * >( pString + numNameStringBytes ) = 0;	// string null-terminator + 3 alternative spelling bytes
				*pnCaseResolveIndex = MEM_4BYTES_AS_0_AND_3BYTES( nNewAlternativeStringIndex );	// link previous spelling entry to the new entry
				return (HKeySymbol)nNewAlternativeStringIndex;
			}
		}

		i++;

		if (item->next == NULL)
		{
			if ( !bCreate )
			{
				// not found
				return -1;
			}

			// we're not in the table
			if (item->stringIndex != 0)
			{
				// first item is used, an new item
				item->next = (hash_item_t *)m_HashItemMemPool.Alloc(sizeof(hash_item_t));
				item = item->next;
			}

			// build up the new item
			item->next = NULL;
			int numStringBytes = strlen(name);
			char *pString = (char *)m_Strings.Alloc( numStringBytes + 1 + 3 );
			if ( !pString )
			{
				Error( "Out of keyvalue string space" );
				return -1;
			}
			item->stringIndex = pString - (char *)m_Strings.GetBase();
			V_memcpy( pString, name, numStringBytes );
			* reinterpret_cast< uint32 * >( pString + numStringBytes ) = 0;	// string null-terminator + 3 alternative spelling bytes
			hCaseInsensitiveSymbol = (HKeySymbol)item->stringIndex;
			return (HKeySymbol)item->stringIndex;
		}

		item = item->next;
	}

	// shouldn't be able to get here
	Assert(0);
	return (-1);
}

//-----------------------------------------------------------------------------
// Purpose: symbol table access
//-----------------------------------------------------------------------------
const char *CKeyValuesSystem::GetStringForSymbol(HKeySymbol symbol)
{
	if ( symbol == -1 )
	{
		return "";
	}
	return ((char *)m_Strings.GetBase() + (size_t)symbol);
}

//-----------------------------------------------------------------------------
// Purpose: adds KeyValues record into global list so we can track memory leaks
//-----------------------------------------------------------------------------
void CKeyValuesSystem::AddKeyValuesToMemoryLeakList(void *pMem, HKeySymbol name)
{
#ifdef _DEBUG
	// only track the memory leaks in debug builds
	MemoryLeakTracker_t item = { name, pMem };
	m_KeyValuesTrackingList.Insert(item);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: used to track memory leaks
//-----------------------------------------------------------------------------
void CKeyValuesSystem::RemoveKeyValuesFromMemoryLeakList(void *pMem)
{
#ifdef _DEBUG
	// only track the memory leaks in debug builds
	MemoryLeakTracker_t item = { 0, pMem };
	int index = m_KeyValuesTrackingList.Find(item);
	m_KeyValuesTrackingList.RemoveAt(index);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: generates a simple hash value for a string
//-----------------------------------------------------------------------------
int CKeyValuesSystem::CaseInsensitiveHash(const char *string, int iBounds)
{
	unsigned int hash = 0;

	for ( ; *string != 0; string++ )
	{
		if (*string >= 'A' && *string <= 'Z')
		{
			hash = (hash << 1) + (*string - 'A' + 'a');
		}
		else
		{
			hash = (hash << 1) + *string;
		}
	}
	  
	return hash % iBounds;
}

//-----------------------------------------------------------------------------
// Purpose: set/get a value for keyvalues resolution symbol
// e.g.: SetKeyValuesExpressionSymbol( "LOWVIOLENCE", true ) - enables [$LOWVIOLENCE]
//-----------------------------------------------------------------------------
void CKeyValuesSystem::SetKeyValuesExpressionSymbol( const char *name, bool bValue )
{
	if ( !name )
		return;

	if ( name[0] == '$' )
		++ name;

	HKeySymbol hSym = GetSymbolForString( name, true );	// find or create symbol
	
	{
		AUTO_LOCK( m_mutex );
		m_KvConditionalSymbolTable.InsertOrReplace( hSym, bValue );
	}
}

bool CKeyValuesSystem::GetKeyValuesExpressionSymbol( const char *name )
{
	if ( !name )
		return false;

	if ( name[0] == '$' )
		++ name;

	HKeySymbol hSym = GetSymbolForString( name, false );	// find or create symbol
	if ( hSym != -1 )
	{
		AUTO_LOCK( m_mutex );
		CUtlMap< HKeySymbol, bool >::IndexType_t idx = m_KvConditionalSymbolTable.Find( hSym );
		if ( idx != m_KvConditionalSymbolTable.InvalidIndex() )
		{
			// Found the symbol value in conditional symbol table
			return m_KvConditionalSymbolTable.Element( idx );
		}
	}

	//
	// Fallback conditionals
	//

	if ( !V_stricmp( name, "GAMECONSOLESPLITSCREEN" ) )
	{
#if defined( _GAMECONSOLE )
		return ( XBX_GetNumGameUsers() > 1 );
#else
		return false;
#endif
	}

	if ( !V_stricmp( name, "GAMECONSOLEGUEST" ) )
	{
#if defined( _GAMECONSOLE )
		return ( XBX_GetPrimaryUserIsGuest() != 0 );
#else
		return false;
#endif
	}

	if ( !V_stricmp( name, "ENGLISH" ) ||
		 !V_stricmp( name, "JAPANESE" ) ||
		 !V_stricmp( name, "GERMAN" ) ||
		 !V_stricmp( name, "FRENCH" ) ||
		 !V_stricmp( name, "SPANISH" ) ||
		 !V_stricmp( name, "ITALIAN" ) ||
		 !V_stricmp( name, "KOREAN" ) ||
		 !V_stricmp( name, "TCHINESE" ) ||
		 !V_stricmp( name, "PORTUGUESE" ) ||
		 !V_stricmp( name, "SCHINESE" ) ||
		 !V_stricmp( name, "POLISH" ) ||
		 !V_stricmp( name, "RUSSIAN" ) ||
		 !V_stricmp( name, "TURKISH" ) )
	{
		// the language symbols are true if we are in that language
		// english is assumed when no language is present
		const char *pLanguageString;
#ifdef _GAMECONSOLE
		pLanguageString = XBX_GetLanguageString();
#else
		static ConVarRef cl_language( "cl_language" );
		pLanguageString = cl_language.GetString();
#endif
		if ( !pLanguageString || !pLanguageString[0] )
		{
			pLanguageString = "english";
		}
		if ( !V_stricmp( name, pLanguageString ) )
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	// very expensive, back door for DLC updates
	if ( !V_strnicmp( name, "CVAR_", 5 ) )
	{
		ConVarRef cvRef( name + 5 );
		if ( cvRef.IsValid() )
			return cvRef.GetBool();
	}

	// purposely warn on these to prevent syntax errors
	// need to get these fixed asap, otherwise unintended false behavior
	Warning( "KV Conditional: Unknown symbol %s\n", name );
	return false;
}

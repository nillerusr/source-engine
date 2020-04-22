//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: unusedcontent.cpp : Defines the entry point for the console application.
//
//=============================================================================//
#include "cbase.h"
#include <stdio.h>
#include <windows.h>
#include <io.h>
#include <sys/stat.h>
#include "tier0/dbg.h"
#pragma warning( disable : 4018 )

#include "utlrbtree.h"
#include "utlvector.h"
#include "utldict.h"
#include "filesystem.h"
#include "FileSystem_Tools.h"
#include "FileSystem_Helpers.h"
#include "KeyValues.h"
#include "cmdlib.h"
#include "scriplib.h"
#include "tier0/icommandline.h"
#include "tier1/fmtstr.h"

bool uselogfile = false;
bool spewdeletions = false;
bool showreferencedfiles = false;
bool immediatedelete = false;
bool printwhitelist = false;
bool showmapfileusage = false;

static char modname[MAX_PATH];
static char g_szReslistDir[ MAX_PATH ] = "reslists/";

namespace UnusedContent
{

class CCleanupUtlSymbolTable;

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------

class CUtlSymbolTable;


//-----------------------------------------------------------------------------
// This is a symbol, which is a easier way of dealing with strings.
//-----------------------------------------------------------------------------

typedef unsigned int UtlSymId_t;

#define UC_UTL_INVAL_SYMBOL  ((UnusedContent::UtlSymId_t)~0)

class CUtlSymbol
{
public:
	// constructor, destructor
	CUtlSymbol() : m_Id(UTL_INVAL_SYMBOL) {}
	CUtlSymbol( UtlSymId_t id ) : m_Id(id) {}
	CUtlSymbol( char const* pStr );
	CUtlSymbol( CUtlSymbol const& sym ) : m_Id(sym.m_Id) {}
	
	// operator=
	CUtlSymbol& operator=( CUtlSymbol const& src ) { m_Id = src.m_Id; return *this; }
	
	// operator==
	bool operator==( CUtlSymbol const& src ) const { return m_Id == src.m_Id; }
	bool operator==( char const* pStr ) const;
	
	// Is valid?
	bool IsValid() const { return m_Id != UTL_INVAL_SYMBOL; }
	
	// Gets at the symbol
	operator UtlSymId_t const() const { return m_Id; }
	
	// Gets the string associated with the symbol
	char const* String( ) const;

	// Modules can choose to disable the static symbol table so to prevent accidental use of them.
	static void DisableStaticSymbolTable();
		
protected:
	UtlSymId_t   m_Id;
		
	// Initializes the symbol table
	static void Initialize();
	
	// returns the current symbol table
	static CUtlSymbolTable* CurrTable();
		
	// The standard global symbol table
	static CUtlSymbolTable* s_pSymbolTable; 

	static bool s_bAllowStaticSymbolTable;

	friend class UnusedContent::CCleanupUtlSymbolTable;
};


//-----------------------------------------------------------------------------
// CUtlSymbolTable:
// description:
//    This class defines a symbol table, which allows us to perform mappings
//    of strings to symbols and back. The symbol class itself contains
//    a static version of this class for creating global strings, but this
//    class can also be instanced to create local symbol tables.
//-----------------------------------------------------------------------------

class CUtlSymbolTable
{
public:
	// constructor, destructor
	CUtlSymbolTable( int growSize = 0, int initSize = 32, bool caseInsensitive = false );
	~CUtlSymbolTable();
	
	// Finds and/or creates a symbol based on the string
	CUtlSymbol AddString( char const* pString );

	// Finds the symbol for pString
	CUtlSymbol Find( char const* pString );
	
	// Look up the string associated with a particular symbol
	char const* String( CUtlSymbol id ) const;
	
	// Remove all symbols in the table.
	void  RemoveAll();

	int GetNumStrings( void ) const
	{
		return m_Lookup.Count();
	}

protected:
	class CStringPoolIndex
	{
	public:
		inline CStringPoolIndex()
		{
		}

		inline CStringPoolIndex( unsigned int iPool, unsigned int iOffset )
		{
			m_iPool = iPool;
			m_iOffset = iOffset;
		}

		inline bool operator==( const CStringPoolIndex &other )	const
		{
			return m_iPool == other.m_iPool && m_iOffset == other.m_iOffset;
		}

		unsigned int m_iPool;		// Index into m_StringPools.
		unsigned int m_iOffset;	// Index into the string pool.
	};

	// Stores the symbol lookup
	CUtlRBTree<CStringPoolIndex, unsigned int>	m_Lookup;

	typedef struct
	{	
		int m_TotalLen;		// How large is 
		int m_SpaceUsed;
		char m_Data[1];
	} StringPool_t;

	// stores the string data
	CUtlVector<StringPool_t*> m_StringPools;



private:

	int FindPoolWithSpace( int len ) const;

	const char* StringFromIndex( const CStringPoolIndex &index ) const;
		
	// Less function, for sorting strings
	static bool SymLess( CStringPoolIndex const& i1, CStringPoolIndex const& i2 );
	// case insensitive less function
	static bool SymLessi( CStringPoolIndex const& i1, CStringPoolIndex const& i2 );
};

//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Defines a symbol table
//
// $Header: $
// $NoKeywords: $
//=============================================================================

#pragma warning (disable:4514)

#define INVALID_STRING_INDEX CStringPoolIndex( 0xFFFF, 0xFFFF )

#define MIN_STRING_POOL_SIZE	2048

//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------

CUtlSymbolTable* CUtlSymbol::s_pSymbolTable = 0; 
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
		s_pSymbolTable = new CUtlSymbolTable;
		symbolsInitialized = true;
	}
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

CUtlSymbolTable* CUtlSymbol::CurrTable()
{
	Initialize();
	return s_pSymbolTable; 
}


//-----------------------------------------------------------------------------
// string->symbol->string
//-----------------------------------------------------------------------------

CUtlSymbol::CUtlSymbol( char const* pStr )
{
	m_Id = CurrTable()->AddString( pStr );
}

char const* CUtlSymbol::String( ) const
{
	return CurrTable()->String(m_Id);
}

void CUtlSymbol::DisableStaticSymbolTable()
{
	s_bAllowStaticSymbolTable = false;
}

//-----------------------------------------------------------------------------
// checks if the symbol matches a string
//-----------------------------------------------------------------------------

bool CUtlSymbol::operator==( char const* pStr ) const
{
	if (m_Id == UTL_INVAL_SYMBOL) 
		return false;
	return strcmp( String(), pStr ) == 0;
}



//-----------------------------------------------------------------------------
// symbol table stuff
//-----------------------------------------------------------------------------

struct LessCtx_t
{
	char const* m_pUserString;
	CUtlSymbolTable* m_pTable;
	
	LessCtx_t( ) : m_pUserString(0), m_pTable(0) {}
};

static LessCtx_t g_LessCtx;


inline const char* CUtlSymbolTable::StringFromIndex( const CStringPoolIndex &index ) const
{
	Assert( index.m_iPool < m_StringPools.Count() );
	Assert( index.m_iOffset < m_StringPools[index.m_iPool]->m_TotalLen );

	return &m_StringPools[index.m_iPool]->m_Data[index.m_iOffset];
}


bool CUtlSymbolTable::SymLess( CStringPoolIndex const& i1, CStringPoolIndex const& i2 )
{
	char const* str1 = (i1 == INVALID_STRING_INDEX) ? g_LessCtx.m_pUserString :
											g_LessCtx.m_pTable->StringFromIndex( i1 );
	char const* str2 = (i2 == INVALID_STRING_INDEX) ? g_LessCtx.m_pUserString :
											g_LessCtx.m_pTable->StringFromIndex( i2 );
	
	return strcmp( str1, str2 ) < 0;
}


bool CUtlSymbolTable::SymLessi( CStringPoolIndex const& i1, CStringPoolIndex const& i2 )
{
	char const* str1 = (i1 == INVALID_STRING_INDEX) ? g_LessCtx.m_pUserString :
											g_LessCtx.m_pTable->StringFromIndex( i1 );
	char const* str2 = (i2 == INVALID_STRING_INDEX) ? g_LessCtx.m_pUserString :
											g_LessCtx.m_pTable->StringFromIndex( i2 );
	
	return strcmpi( str1, str2 ) < 0;
}

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

CUtlSymbolTable::CUtlSymbolTable( int growSize, int initSize, bool caseInsensitive ) : 
	m_Lookup( growSize, initSize, caseInsensitive ? SymLessi : SymLess ), m_StringPools( 8 )
{
}

CUtlSymbolTable::~CUtlSymbolTable()
{
}


CUtlSymbol CUtlSymbolTable::Find( char const* pString )
{	
	if (!pString)
		return CUtlSymbol();
	
	// Store a special context used to help with insertion
	g_LessCtx.m_pUserString = pString;
	g_LessCtx.m_pTable = this;
	
	// Passing this special invalid symbol makes the comparison function
	// use the string passed in the context
	UtlSymId_t idx = m_Lookup.Find( INVALID_STRING_INDEX );
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

CUtlSymbol CUtlSymbolTable::AddString( char const* pString )
{
	if (!pString) 
		return CUtlSymbol( UTL_INVAL_SYMBOL );

	CUtlSymbol id = Find( pString );
	
	if (id.IsValid())
		return id;

	int len = strlen(pString) + 1;

	// Find a pool with space for this string, or allocate a new one.
	int iPool = FindPoolWithSpace( len );
	if ( iPool == -1 )
	{
		// Add a new pool.
		int newPoolSize = max( len, MIN_STRING_POOL_SIZE );
		StringPool_t *pPool = (StringPool_t*)malloc( sizeof( StringPool_t ) + newPoolSize - 1 );
		pPool->m_TotalLen = newPoolSize;
		pPool->m_SpaceUsed = 0;
		iPool = m_StringPools.AddToTail( pPool );
	}

	// Copy the string in.
	StringPool_t *pPool = m_StringPools[iPool];
	Assert( pPool->m_SpaceUsed < 0xFFFF );	// This should never happen, because if we had a string > 64k, it
											// would have been given its entire own pool.
	
	unsigned int iStringOffset = pPool->m_SpaceUsed;

	memcpy( &pPool->m_Data[pPool->m_SpaceUsed], pString, len );
	pPool->m_SpaceUsed += len;

	// didn't find, insert the string into the vector.
	CStringPoolIndex index;
	index.m_iPool = iPool;
	index.m_iOffset = iStringOffset;

	UtlSymId_t idx = m_Lookup.Insert( index );
	return CUtlSymbol( idx );
}


//-----------------------------------------------------------------------------
// Look up the string associated with a particular symbol
//-----------------------------------------------------------------------------

char const* CUtlSymbolTable::String( CUtlSymbol id ) const
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
	m_Lookup.RemoveAll();
	
	for ( int i=0; i < m_StringPools.Count(); i++ )
		free( m_StringPools[i] );

	m_StringPools.RemoveAll();
}

} // Namespace UnusedContent

struct AnalysisData
{
	UnusedContent::CUtlSymbolTable				symbols;
};

char *directories_to_check[] =
{
	"",
	"bin",
	"maps",
	"materials",
	"models",
	"scenes",
	"scripts",
	"sound",
	"hl2",
};

char *directories_to_ignore[] = // don't include these dirs in the others list
{
	"reslists",
	"reslists_temp",
	"logs",
	"media",
	"downloads",
	"save",
	"screenshots",
	"testscripts",
	"logos"
};


enum
{
	REFERENCED_NO = 0,
	REFERENCED_WHITELIST,
	REFERENCED_GAME
};

struct FileEntry
{
	FileEntry()
	{
		sym = UC_UTL_INVAL_SYMBOL;
		size = 0;
		referenced = REFERENCED_NO;
	}

	UnusedContent::CUtlSymbol		sym;
	unsigned int	size;
	int				referenced;
};

struct ReferencedFile
{
	ReferencedFile()
	{
		sym = UC_UTL_INVAL_SYMBOL;
	}

	ReferencedFile( const ReferencedFile& src )
	{
		sym = src.sym;
		maplist.RemoveAll();
		int c = src.maplist.Count();
		for ( int i = 0; i < c; ++i )
		{
			maplist.AddToTail( src.maplist[ i ] );
		}
	}

	ReferencedFile & operator =( const ReferencedFile& src )
	{
		if ( this == &src )
			return *this;

		sym = src.sym;
		maplist.RemoveAll();
		int c = src.maplist.Count();
		for ( int i = 0; i < c; ++i )
		{
			maplist.AddToTail( src.maplist[ i ] );
		}
		return *this;
	}

	UnusedContent::CUtlSymbol		sym;
	CUtlVector< UnusedContent::CUtlSymbol >	maplist;
};


static AnalysisData g_Analysis;

IFileSystem *filesystem = NULL;
static CUniformRandomStream g_Random;
IUniformRandomStream *random = &g_Random;

static bool spewed = false;

SpewRetval_t SpewFunc( SpewType_t type, char const *pMsg )
{	
	spewed = true;

	printf( "%s", pMsg );
	OutputDebugString( pMsg );
	
	if ( type == SPEW_ERROR )
	{
		printf( "\n" );
		OutputDebugString( "\n" );
		exit(-1);
	}

	return SPEW_CONTINUE;
}

char *va( const char *fmt, ... )
{
	static char string[ 8192 ];
	va_list va;
	va_start( va, fmt );
	vsprintf( string, fmt, va );
	va_end( va );
	return string;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : depth - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void vprint( int depth, const char *fmt, ... )
{
	char string[ 8192 ];
	va_list va;
	va_start( va, fmt );
	vsprintf( string, fmt, va );
	va_end( va );

	FILE *fp = NULL;

	if ( uselogfile )
	{
		fp = fopen( "log.txt", "ab" );
	}

	while ( depth-- > 0 )
	{
		printf( "  " );
		OutputDebugString( "  " );
		if ( fp )
		{
			fprintf( fp, "  " );
		}
	}

	::printf( "%s", string );
	OutputDebugString( string );

	if ( fp )
	{
		char *p = string;
		while ( *p )
		{
			if ( *p == '\n' )
			{
				fputc( '\r', fp );
			}
			fputc( *p, fp );
			p++;
		}
		fclose( fp );
	}
}

void logprint( char const *logfile, const char *fmt, ... )
{
	char string[ 8192 ];
	va_list va;
	va_start( va, fmt );
	vsprintf( string, fmt, va );
	va_end( va );

	FILE *fp = NULL;

	UnusedContent::CUtlSymbol sym = g_Analysis.symbols.Find( logfile );
	static CUtlRBTree< UnusedContent::CUtlSymbol, int >	previousfiles( 0, 0, DefLessFunc( UnusedContent::CUtlSymbol ) );
	if ( previousfiles.Find( sym ) == previousfiles.InvalidIndex() )
	{
		previousfiles.Insert( sym );
		fp = fopen( logfile, "wb" );
	}
	else
	{
		fp = fopen( logfile, "ab" );
	}

	if ( fp )
	{
		char *p = string;
		while ( *p )
		{
			if ( *p == '\n' )
			{
				fputc( '\r', fp );
			}
			fputc( *p, fp );
			p++;
		}
		fclose( fp );
	}
}

void Con_Printf( const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	vprintf( fmt, args );
	vsprintf( output, fmt, args );

	vprint( 0, output );
}

bool ShouldCheckDir( char const *dirname );
bool ShouldIgnoreDir( const char *dirname );

void BuildFileList_R( int depth, CUtlVector< FileEntry >& files, CUtlVector< FileEntry > * otherfiles, char const *dir, char const *wild, int skipchars )
{
	WIN32_FIND_DATA wfd;

	char directory[ 256 ];
	char filename[ 256 ];
	HANDLE ff;

	bool canrecurse = true;
	if ( !Q_stricmp( wild, "..." ) )
	{
		canrecurse = true;
		sprintf( directory, "%s%s%s", dir[0] == '\\' ? dir + 1 : dir, dir[0] != 0 ? "\\" : "", "*.*" );
	}
	else
	{
		sprintf( directory, "%s%s%s", dir, dir[0] != 0 ? "\\" : "", wild );
	}
	int dirlen = Q_strlen( dir );

	if ( ( ff = FindFirstFile( directory, &wfd ) ) == INVALID_HANDLE_VALUE )
		return;

	do
	{
		if ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			bool useOtherFiles = false;

			if ( wfd.cFileName[ 0 ] == '.' )
				continue;

			if ( depth == 0 && !ShouldCheckDir( wfd.cFileName ) && otherfiles )
			{
				if ( !ShouldIgnoreDir( wfd.cFileName ) )
				{
					useOtherFiles = true;
				}
			}

			if ( !canrecurse )
				continue;

			// Recurse down directory
			if ( dir[0] )
			{
				sprintf( filename, "%s\\%s", dir, wfd.cFileName );
			}
			else
			{
				sprintf( filename, "%s", wfd.cFileName );
			}
			BuildFileList_R( depth + 1, useOtherFiles ? *otherfiles: files, NULL, filename, wild, skipchars );
		}
		else
		{
			if (!stricmp(wfd.cFileName, "vssver.scc"))
				continue;

			char filename[ MAX_PATH ];
			if ( dirlen <= skipchars )
			{
				Q_snprintf( filename, sizeof( filename ), "%s", wfd.cFileName );
			}
			else
			{
				Q_snprintf( filename, sizeof( filename ), "%s\\%s", &dir[ skipchars ], wfd.cFileName );
			}
			_strlwr( filename );

			Q_FixSlashes( filename );

			UnusedContent::CUtlSymbol sym = g_Analysis.symbols.AddString( filename );

			FileEntry entry;
			entry.sym = sym;
			int size = g_pFileSystem->Size( filename );
			entry.size = size >= 0 ? (unsigned int)size : 0;

			files.AddToTail( entry );

			if ( !( files.Count() % 3000 ) )
			{
				vprint( 0, "...found %i files\n", files.Count() );
			}
		}
	} while ( FindNextFile( ff, &wfd ) );
}

void BuildFileList( int depth, CUtlVector< FileEntry >& files, CUtlVector< FileEntry > * otherfiles, char const *rootdir, int skipchars )
{
	files.RemoveAll();
	Assert( otherfiles );
	otherfiles->RemoveAll();
	BuildFileList_R( depth, files, otherfiles, rootdir, "...", skipchars );
}

void BuildFileListWildcard( int depth, CUtlVector< FileEntry >& files, char const *rootdir, char const *wildcard, int skipchars )
{
	files.RemoveAll();
	BuildFileList_R( depth, files, NULL, rootdir, wildcard, skipchars );
}


static CUtlVector< UnusedContent::CUtlSymbol > g_DirList;
static CUtlVector< UnusedContent::CUtlSymbol > g_IgnoreDir;

bool ShouldCheckDir( char const *dirname )
{
	int c = g_DirList.Count();
	for ( int i = 0; i < c; ++i )
	{
		char const *check = g_Analysis.symbols.String( g_DirList[ i ] );

		if ( !Q_stricmp( dirname, check ) )
			return true;
	}

	vprint( 1, "Skipping dir %s\n", dirname );
	return false;
}

bool ShouldIgnoreDir( const char *dirname )
{
	int c = g_IgnoreDir.Count();
	for ( int i = 0; i < c; ++i )
	{
		char const *check = g_Analysis.symbols.String( g_IgnoreDir[ i ] );

		if ( Q_stristr( dirname, "reslists" ) )
		{
			vprint( 1, "Ignoring dir %s\n", dirname );
			return true;
		}

		if ( !Q_stricmp( dirname, check ) )
		{
			vprint( 1, "Ignoring dir %s\n", dirname );
			return true;
		}
	}

	return false;
}

void AddCheckdir( char const *dirname )
{
	UnusedContent::CUtlSymbol sym = g_Analysis.symbols.AddString( dirname );
	g_DirList.AddToTail( sym );

	vprint( 1, "AddCheckdir[ \"%s\" ]\n", dirname );
}

void AddIgnoredir( const char *dirname )
{
	UnusedContent::CUtlSymbol sym = g_Analysis.symbols.AddString( dirname );
	g_IgnoreDir.AddToTail( sym );

	vprint( 1, "AddIgnoredir[ \"%s\" ]\n", dirname );
}


#define UNUSEDCONTENT_CFG	"unusedcontent.cfg"

void BuildCheckdirList()
{
	vprint( 0, "Checking for dirlist\n" );
	// Search for unusedcontent.cfg file
	if ( g_pFileSystem->FileExists( UNUSEDCONTENT_CFG, "GAME") )
	{
		KeyValues *kv = new KeyValues( UNUSEDCONTENT_CFG );
		if ( kv )
		{
			if ( kv->LoadFromFile( g_pFileSystem, UNUSEDCONTENT_CFG, "GAME" ) )
			{
				for ( KeyValues *sub = kv->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
				{
					if ( !Q_stricmp( sub->GetName(), "dir" ) )
					{
						AddCheckdir( sub->GetString() );
					}
					else if ( !Q_stricmp( sub->GetName(), "ignore" ) )
					{
						AddIgnoredir( sub->GetString() );
					}
					else
					{
						vprint( 1, "Unknown subkey '%s' in %s\n", sub->GetName(), UNUSEDCONTENT_CFG );
					}
				}
			}

			kv->deleteThis();
		}
	}
	else
	{
		int c = ARRAYSIZE( directories_to_check );
		int i;
		for ( i = 0; i < c; ++i )
		{
			AddCheckdir( directories_to_check[ i ] );
		}

		// add the list of dirs to ignore from the others lists
		c = ARRAYSIZE( directories_to_ignore );
		for ( i = 0; i < c; ++i )
		{
			AddIgnoredir( directories_to_ignore[ i ] );
		}
	}
}

static CUtlRBTree< UnusedContent::CUtlSymbol, int >	g_WhiteList( 0, 0, DefLessFunc( UnusedContent::CUtlSymbol ) );

#define WHITELIST_FILE	"whitelist.cfg"

static int wl_added = 0;
static int wl_removed = 0;

void AddToWhiteList( char const *path )
{
	vprint( 2, "+\t'%s'\n", path );

	char dir[ 512 ];
	Q_strncpy( dir, path, sizeof( dir ) );

	// Get the base filename from the path
	_strlwr( dir );
	Q_FixSlashes( dir );

	CUtlVector< FileEntry > files;

	char *lastslash = strrchr( dir, '\\' );
	if ( lastslash == 0 )
	{
		BuildFileListWildcard( 1, files, "", dir, 0 );
	}
	else
	{
		char *wild = lastslash + 1;
		*lastslash = 0;

		BuildFileListWildcard( 1, files, dir, wild, 0 );
	}

	int c = files.Count();
	for ( int i = 0; i < c; ++i )
	{
		UnusedContent::CUtlSymbol sym = files[ i ].sym;
		if ( g_WhiteList.Find( sym ) == g_WhiteList.InvalidIndex() )
		{
			g_WhiteList.Insert( sym );
			++wl_added;
		}
	}
}

void RemoveFromWhiteList( char const *path )
{
	vprint( 2, "-\t'%s'\n", path );

	char dir[ 512 ];
	Q_strncpy( dir, path, sizeof( dir ) );

	// Get the base filename from the path
	_strlwr( dir );
	Q_FixSlashes( dir );

	CUtlVector< FileEntry > files;

	char *lastslash = strrchr( dir, '\\' );
	if ( lastslash == 0 )
	{
		BuildFileListWildcard( 1, files, "", dir, 0 );
	}
	else
	{
		char *wild = lastslash + 1;
		*lastslash = 0;

		BuildFileListWildcard( 1, files, dir, wild, 0 );
	}

	int c = files.Count();
	for ( int i = 0; i < c; ++i )
	{
		UnusedContent::CUtlSymbol sym = files[ i ].sym;
		int idx = g_WhiteList.Find( sym );
		if ( idx != g_WhiteList.InvalidIndex() )
		{
			g_WhiteList.RemoveAt( idx );
			++wl_removed;
		}
	}
}

void BuildWhiteList()
{
	// Search for unusedcontent.cfg file
	if ( !g_pFileSystem->FileExists( WHITELIST_FILE ) )
	{
		vprint( 1, "Running with no whitelist.cfg file!!!\n" );
		return;
	}

	vprint( 1, "\nBuilding whitelist\n" );

	KeyValues *kv = new KeyValues( WHITELIST_FILE );
	if ( kv )
	{
		if ( kv->LoadFromFile( g_pFileSystem, WHITELIST_FILE, NULL ) )
		{
			for ( KeyValues *sub = kv->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
			{
				if ( !Q_stricmp( sub->GetName(), "add" ) )
				{
					AddToWhiteList( sub->GetString() );
				}
				else if ( !Q_stricmp( sub->GetName(), "remove" ) )
				{
					RemoveFromWhiteList( sub->GetString() );
				}
				else
				{
					vprint( 1, "Unknown subkey '%s' in %s\n", sub->GetName(), WHITELIST_FILE );
				}
			}
		}

		kv->deleteThis();
	}

	if ( verbose || printwhitelist )
	{
		vprint( 1, "Whitelist:\n\n" );


		for ( int i = g_WhiteList.FirstInorder(); 
			i != g_WhiteList.InvalidIndex(); 
			i = g_WhiteList.NextInorder( i ) )
		{
			UnusedContent::CUtlSymbol& sym = g_WhiteList[ i ];

			char const *resolved = g_Analysis.symbols.String( sym );
			vprint( 2, "  %s\n", resolved );
		}
	}

	// dump the whitelist file list anyway
	{
		filesystem->RemoveFile( "whitelist_files.txt", "GAME" );
		for ( int i = g_WhiteList.FirstInorder(); 
			i != g_WhiteList.InvalidIndex(); 
			i = g_WhiteList.NextInorder( i ) )
		{
			UnusedContent::CUtlSymbol& sym = g_WhiteList[ i ];
			char const *resolved = g_Analysis.symbols.String( sym );
			logprint( "whitelist_files.txt", "\"%s\"\n", resolved );
		}
	}


	vprint( 1, "Whitelist resolves to %d files (added %i/removed %i)\n\n", g_WhiteList.Count(), wl_added, wl_removed );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void printusage( void )
{
	vprint( 0, "usage:  unusedcontent maplistfile\n\
		\t Note that you must have generated the reslistsfile output via the engine first!!!\n\
		\t-d = spew command prompt deletion instructions to deletions.bat\n\
		\t-v = verbose output\n\
		\t-l = log to file log.txt\n\
		\t-r = print out all referenced files\n\
		\t-m = generate referenced.csv with map counts\n\
		\t-w = print out whitelist\n\
		\t-i = delete unused files immediately\n\
		\t-f <reslistdir> :  specify reslists folder, 'reslists' assumed by default\n\
		\t\tmaps/\n\
		\t\tmaterials/\n\
		\t\tmodels/\n\
		\t\tsounds/\n\
		\ne.g.:  unusedcontent -r maplist.txt\n" );

	// Exit app
	exit( 1 );
}

void ParseFilesFromResList( UnusedContent::CUtlSymbol & resfilesymbol, CUtlRBTree< ReferencedFile, int >& files, char const *resfile )
{
	int addedStrings = 0;
	int resourcesConsidered = 0;

	int offset = Q_strlen( gamedir );

	char basedir[MAX_PATH];
	Q_strncpy( basedir, gamedir, sizeof( basedir ) );
	if ( !Q_StripLastDir( basedir, sizeof( basedir ) ) )
		Error( "Can't get basedir from %s.", gamedir );

	FileHandle_t resfilehandle;
	resfilehandle = g_pFileSystem->Open( resfile, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE != resfilehandle )
	{
		// Read in the entire file
		int length = g_pFileSystem->Size(resfilehandle);
		if ( length > 0 )
		{
			char *pStart = (char *)new char[ length + 1 ];
			if ( pStart && ( length == g_pFileSystem->Read(pStart, length, resfilehandle) ) )
			{
				pStart[ length ] = 0;

				char *pFileList = pStart;

				char token[512];

				while ( 1 )
				{
					pFileList = ParseFile( pFileList, token, NULL );
					if ( !pFileList )
						break;

					if ( strlen( token ) > 0 )
					{
						char szFileName[ 256 ];
						Q_snprintf( szFileName, sizeof( szFileName ), "%s%s", basedir, token );
						_strlwr( szFileName );
						Q_FixSlashes( szFileName );
						while ( szFileName[ strlen( szFileName ) - 1 ] == '\n' ||
							szFileName[ strlen( szFileName ) - 1 ] == '\r' )
						{
							szFileName[ strlen( szFileName ) - 1 ] = 0;
						}

						if ( Q_strnicmp( szFileName, gamedir, offset ) )
							continue;

						char *pFile = szFileName + offset;
						++resourcesConsidered;

						ReferencedFile rf;
						rf.sym = g_Analysis.symbols.AddString( pFile );

						int idx = files.Find( rf );
						if ( idx == files.InvalidIndex() )
						{
							++addedStrings;
							rf.maplist.AddToTail( resfilesymbol );
							files.Insert( rf );
						}
						else
						{
							// 
							ReferencedFile & slot = files[ idx ];
							if ( slot.maplist.Find( resfilesymbol ) == slot.maplist.InvalidIndex() )
							{
								slot.maplist.AddToTail( resfilesymbol );
							}

						}
					}
				}

			}
			delete[] pStart;
		}

		g_pFileSystem->Close(resfilehandle);
	}

	int filesFound = addedStrings;

	vprint( 1, "Found %i new resources (%i total) in %s\n", filesFound, resourcesConsidered, resfile );
}

bool BuildReferencedFileList( CUtlVector< UnusedContent::CUtlSymbol >& resfiles, CUtlRBTree< ReferencedFile, int >& files, const char *resfile )
{

	// Load the reslist file
	FileHandle_t resfilehandle;
	resfilehandle = g_pFileSystem->Open( resfile, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE != resfilehandle )
	{
		// Read in and parse mapcycle.txt
		int length = g_pFileSystem->Size(resfilehandle);
		if ( length > 0 )
		{
			char *pStart = (char *)new char[ length + 1 ];
			if ( pStart && ( length == g_pFileSystem->Read(pStart, length, resfilehandle) )
			   )
			{
				pStart[ length ] = 0;

				char *pFileList = pStart;

				while ( 1 )
				{
					char szResList[ 256 ];

					pFileList = COM_Parse( pFileList );
					if ( strlen( com_token ) <= 0 )
						break;

					Q_snprintf(szResList, sizeof( szResList ), "%s%s.lst", g_szReslistDir, com_token );
					_strlwr( szResList );
					Q_FixSlashes( szResList );

					if ( !g_pFileSystem->FileExists( szResList ) )
					{
						vprint( 0, "Couldn't find %s\n", szResList );
						continue;
					}

					UnusedContent::CUtlSymbol sym = g_Analysis.symbols.AddString( szResList );
					resfiles.AddToTail( sym );

				}
			}
			delete[] pStart;
		}

		g_pFileSystem->Close(resfilehandle);
	}
	else
	{
		Error( "Unable to open reslist file %s\n", resfile );
		exit( -1 );
	}

	if ( g_pFileSystem->FileExists( CFmtStr( "%sall.lst", g_szReslistDir ) ) )
	{
		UnusedContent::CUtlSymbol sym = g_Analysis.symbols.AddString( CFmtStr( "%sall.lst", g_szReslistDir ) );
		resfiles.AddToTail( sym );
	}

	if ( g_pFileSystem->FileExists( CFmtStr( "%sengine.lst", g_szReslistDir ) ) )
	{
		UnusedContent::CUtlSymbol sym = g_Analysis.symbols.AddString( CFmtStr( "%sengine.lst", g_szReslistDir ) );
		resfiles.AddToTail( sym );
	}

	// Do we have any resfiles?
	if ( resfiles.Count() <= 0 )
	{
		vprint( 0, "%s didn't have any actual .lst files in the reslists folder, have you run the engine with %s\n", resfile,
			"-makereslists -usereslistfile maplist.txt" );
		return false;
	}

	vprint( 0, "Parsed %i reslist files\n", resfiles.Count() );

	// Now load in each res file
	int c = resfiles.Count();
	for ( int i = 0; i < c; ++i )
	{
		UnusedContent::CUtlSymbol& filename = resfiles[ i ];
		char fn[ 256 ];
		Q_strncpy( fn, g_Analysis.symbols.String( filename ), sizeof( fn ) );

		ParseFilesFromResList( filename, files, fn );
	}


	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CheckLogFile( void )
{
	if ( uselogfile )
	{
		_unlink( "log.txt" );
		vprint( 0, "    Outputting to log.txt\n" );
	}
}

void PrintHeader()
{
	vprint( 0, "Valve Software - unusedcontent.exe (%s)\n", __DATE__ );
	vprint( 0, "--- Compares reslists with actual game content tree to show unreferenced content and stats ---\n" );
}

static bool ReferencedFileLessFunc( const ReferencedFile &lhs, const ReferencedFile &rhs )
{ 
	char const *s1 = g_Analysis.symbols.String( lhs.sym );
	char const *s2 = g_Analysis.symbols.String( rhs.sym );

	return Q_stricmp( s1, s2 ) < 0;
}

static bool FileEntryLessFunc( const FileEntry &lhs, const FileEntry &rhs )
{ 
	char const *s1 = g_Analysis.symbols.String( lhs.sym );
	char const *s2 = g_Analysis.symbols.String( rhs.sym );

	return Q_stricmp( s1, s2 ) < 0;
}

static bool RefFileLessFunc( const ReferencedFile &lhs, const ReferencedFile &rhs )
{ 
	char const *s1 = g_Analysis.symbols.String( lhs.sym );
	char const *s2 = g_Analysis.symbols.String( rhs.sym );

	return Q_stricmp( s1, s2 ) < 0;
}

struct DirEntry
{
	DirEntry()
	{
		total = 0;
		unreferenced = 0;
		whitelist = 0;
	}

	double	total;
	double unreferenced;
	double whitelist;
};

void Correlate( CUtlRBTree< ReferencedFile, int >& referencedfiles, CUtlVector< FileEntry >& contentfiles, const char *modname )
{
	int i;
	int c = contentfiles.Count();
	
	double totalDiskSize = 0;
	double totalReferencedDiskSize = 0;
	double totalWhiteListDiskSize = 0;

	for ( i = 0; i < c; ++i )
	{
		totalDiskSize += contentfiles [ i ].size;
	}

	vprint( 0, "Content tree size on disk %s\n", Q_pretifymem( totalDiskSize, 3 ) );
	
	// Analysis is to walk tree and see which files on disk are referenced in the .lst files
	// Need a fast lookup from file symbol to referenced list
	CUtlRBTree< ReferencedFile, int >	tree( 0, 0, ReferencedFileLessFunc );
	c = referencedfiles.Count();
	for ( i = 0 ; i < c; ++i )
	{
		tree.Insert( referencedfiles[ i ] );
	}

	// Now walk the on disk file and see check off resources which are in referenced
	c = contentfiles.Count();
	int invalidindex = tree.InvalidIndex();
	unsigned int refcounted = 0;
	unsigned int whitelisted = 0;

	filesystem->RemoveFile( CFmtStr( "%swhitelist.lst", g_szReslistDir ), "GAME" );

	for ( i = 0; i < c; ++i )
	{
		FileEntry & entry = contentfiles[ i ];

		ReferencedFile foo;
		foo.sym = entry.sym;

		bool gameref = tree.Find( foo ) != invalidindex;
		char const *fn = g_Analysis.symbols.String( entry.sym );

		bool whitelist = g_WhiteList.Find( entry.sym ) != g_WhiteList.InvalidIndex();

		if ( gameref || whitelist )
		{
			entry.referenced = gameref ? REFERENCED_GAME : REFERENCED_WHITELIST;
			totalReferencedDiskSize += entry.size;
			if ( entry.referenced == REFERENCED_WHITELIST )
			{
				logprint( CFmtStr( "%swhitelist.lst", g_szReslistDir ), "\"%s\\%s\"\n", modname, fn );

				totalWhiteListDiskSize += entry.size;
				++whitelisted;
			}
			++refcounted;
		}
	}

	vprint( 0, "Found %i referenced (%i whitelist) files in tree, %s\n", refcounted, whitelisted, Q_pretifymem( totalReferencedDiskSize, 2 ) );
	vprint( 0, "%s appear unused\n", Q_pretifymem( totalDiskSize - totalReferencedDiskSize, 2 ) );

	// Now sort and dump the unreferenced ones..
	vprint( 0, "Sorting unreferenced files list...\n" );

	CUtlRBTree< FileEntry, int >	unreftree( 0, 0, FileEntryLessFunc );
	for ( i = 0; i < c; ++i )
	{
		FileEntry & entry = contentfiles[ i ];
		if ( entry.referenced != REFERENCED_NO )
			continue;

		unreftree.Insert( entry );
	}

	// Now walk the unref tree in order
	i = unreftree.FirstInorder();
	invalidindex = unreftree.InvalidIndex();
	int index = 0;
	while ( i != invalidindex )
	{
		FileEntry & entry = unreftree[ i ];

		if ( showreferencedfiles )
		{
			vprint( 1, "%6i %12s: %s\n", ++index, Q_pretifymem( entry.size, 2 ), g_Analysis.symbols.String( entry.sym ) );
		}
		
		i = unreftree.NextInorder( i );
	}

	if ( showmapfileusage )
	{
		vprint( 0, "Writing referenced.csv...\n" );

		// Now walk the list of referenced files and print out how many and which maps reference them
		i = tree.FirstInorder();
		invalidindex = tree.InvalidIndex();
		index = 0;
		while ( i != invalidindex )
		{
			ReferencedFile & entry = tree[ i ];

			char ext[ 32 ];
			Q_ExtractFileExtension( g_Analysis.symbols.String( entry.sym ), ext, sizeof( ext ) );

			logprint( "referenced.csv", "\"%s\",\"%s\",%d", g_Analysis.symbols.String( entry.sym ), ext, entry.maplist.Count() );

			int mapcount = entry.maplist.Count();
			for ( int j = 0 ; j < mapcount; ++j )
			{
				char basemap[ 128 ];
				Q_FileBase( g_Analysis.symbols.String( entry.maplist[ j ] ), basemap, sizeof( basemap ) );
				logprint( "referenced.csv", ",\"%s\"", basemap );
			}

			logprint( "referenced.csv", "\n" );
			
			i = tree.NextInorder( i );
		}
	}


	vprint( 0, "\nBuilding directory summary list...\n" );

	// Now build summaries by root branch off of gamedir (e.g., for sound, materials, models, etc.)
	CUtlDict< DirEntry, int > directories;
	invalidindex = directories.InvalidIndex();
	for ( i = 0; i < c; ++i )
	{
		FileEntry & entry = contentfiles[ i ];

		// Get the dir name
		char const *dirname = g_Analysis.symbols.String( entry.sym );

		const char *backslash = strstr( dirname, "\\" );

		char dir[ 256 ];
		if ( !backslash )
		{
			dir[0] = 0;
		}
		else
		{
			Q_strncpy( dir, dirname, backslash - dirname + 1);
		}

		
		int idx = directories.Find( dir );
		if ( idx == invalidindex )
		{
			DirEntry foo;
			idx = directories.Insert( dir, foo );
		}

		DirEntry & de = directories[ idx ];
		de.total += entry.size;
		if ( entry.referenced == REFERENCED_NO )
		{
			de.unreferenced += entry.size;
		}
		if ( entry.referenced == REFERENCED_WHITELIST )
		{
			de.whitelist += entry.size;
		}
	}

	if ( spewdeletions )
	{
		// Spew deletion commands to console
		if ( immediatedelete )
		{
			vprint( 0, "\n\nDeleting files...\n" );
		}
		else
		{
			vprint( 0, "\n\nGenerating deletions.bat\n" );
		}

		i = unreftree.FirstInorder();
		invalidindex = unreftree.InvalidIndex();
		float deletionSize = 0.0f;
		int deletionCount = 0;

		while ( i != invalidindex )
		{
			FileEntry & entry = unreftree[ i ];
			i = unreftree.NextInorder( i );

			// Don't delete stuff that's in the white list
			if ( g_WhiteList.Find( entry.sym ) != g_WhiteList.InvalidIndex() )
			{
				if ( verbose )
				{
					vprint( 0, "whitelist blocked deletion of %s\n", g_Analysis.symbols.String( entry.sym ) );
				}
				continue;
			}

			++deletionCount;
			deletionSize += entry.size;

			if ( immediatedelete ) 
			{
				if ( _chmod( g_Analysis.symbols.String( entry.sym ), _S_IWRITE ) == -1 )
				{
					vprint( 0, "Could not find file %s\n", g_Analysis.symbols.String( entry.sym ) );
				}
				if ( _unlink( g_Analysis.symbols.String( entry.sym ) ) == -1 )
				{
					vprint( 0, "Could not delete file %s\n", g_Analysis.symbols.String( entry.sym ) );
				}

				if ( deletionCount % 1000 == 0 )
				{
					vprint( 0, "...deleted %i files\n", deletionCount );
				}
			}
			else
			{
				logprint( "deletions.bat", "del \"%s\" /f\n",  g_Analysis.symbols.String( entry.sym ) );
			}
		}

		vprint( 0, "\nFile deletion (%d files, %s)\n\n", deletionCount, Q_pretifymem(deletionSize, 2) );
	}

	double grand_total = 0;
	double grand_total_unref = 0;
	double grand_total_white = 0;

	char totalstring[ 20 ];
	char unrefstring[ 20 ];
	char refstring[ 20 ];
	char whiteliststring[ 20 ];

	vprint( 0, "---------------------------------------- Summary ----------------------------------------\n" );

	vprint( 0, "% 15s               % 15s               % 15s               % 15s %12s\n",
		"Referenced",
		"WhiteListed",
		"Unreferenced",
		"Total",
		"Directory" );

	// Now walk the dictionary in order
	i = directories.First();
	while ( i != invalidindex )
	{
		DirEntry & de = directories[ i ];

		double remainder = de.total - de.unreferenced;

		float percent_unref = 0.0f;
		float percent_white = 0.0f;
		if ( de.total > 0 )
		{
			percent_unref = 100.0f * (float)de.unreferenced / (float)de.total;
			percent_white = 100.0f * (float)de.whitelist / (float)de.total;
		}

		Q_strncpy( totalstring, Q_pretifymem( de.total, 2 ), sizeof( totalstring ) );
		Q_strncpy( unrefstring, Q_pretifymem( de.unreferenced, 2 ), sizeof( unrefstring ) );
		Q_strncpy( refstring, Q_pretifymem( remainder, 2 ), sizeof( refstring ) );
		Q_strncpy( whiteliststring, Q_pretifymem( de.whitelist, 2 ), sizeof( whiteliststring ) );

		vprint( 0, "%15s (%8.3f%%)   %15s (%8.3f%%)   %15s (%8.3f%%)   %15s => dir: %s\n",
			refstring, 100.0f - percent_unref, whiteliststring, percent_white, unrefstring, percent_unref, totalstring, directories.GetElementName( i ) );

		grand_total += de.total;
		grand_total_unref += de.unreferenced;
		grand_total_white += de.whitelist;

		i = directories.Next( i );
	}

	Q_strncpy( totalstring, Q_pretifymem( grand_total, 2 ), sizeof( totalstring ) );
	Q_strncpy( unrefstring, Q_pretifymem( grand_total_unref, 2 ), sizeof( unrefstring ) );
	Q_strncpy( refstring, Q_pretifymem( grand_total - grand_total_unref, 2 ), sizeof( refstring ) );
	Q_strncpy( whiteliststring, Q_pretifymem( grand_total_white, 2 ), sizeof( whiteliststring ) );

	double percent_unref = 100.0 * grand_total_unref / grand_total;
	double percent_white = 100.0 * grand_total_white / grand_total;

	vprint( 0, "-----------------------------------------------------------------------------------------\n" );
	vprint( 0, "%15s (%8.3f%%)   %15s (%8.3f%%)   %15s (%8.3f%%)   %15s\n",
		refstring, 100.0f - percent_unref, whiteliststring, percent_white, unrefstring, percent_unref, totalstring );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : argc - 
//			argv[] - 
// Output : int
//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
{
	SpewOutputFunc( SpewFunc );
	SpewActivate( "unusedcontent", 2 );

	CommandLine()->CreateCmdLine( argc, argv );

	int i=1;
	for ( i ; i<argc ; i++)
	{
		if ( argv[ i ][ 0 ] == '-' )
		{
			switch( argv[ i ][ 1 ] )
			{
			case 'l':
				uselogfile = true;
				break;
			case 'v':
				verbose = true;
				break;
			case 'r':
				showreferencedfiles = true;
				break;
			case 'd':
				spewdeletions = true;
				break;
			case 'i':
				immediatedelete = true;
				break;
			case 'w':
				printwhitelist = true;
				break;
			case 'm':
				showmapfileusage = true;
				break;
			case 'g':
				// Just skip -game
				Assert( !Q_stricmp( argv[ i ], "-game" ) );
				++i;
				break;
			case 'f':
				// grab reslists folder
				{
					++i;
					Q_strncpy( g_szReslistDir, argv[ i ], sizeof( g_szReslistDir ) );
					Q_strlower( g_szReslistDir );
					Q_FixSlashes( g_szReslistDir );
					Q_AppendSlash( g_szReslistDir, sizeof( g_szReslistDir ) );
					
				}
				break;
			default:
				printusage();
				break;
			}
		}
	}

	if ( argc < 3 || ( i != argc ) )
	{
		PrintHeader();
		printusage();
		return 0;
	}

	CheckLogFile();

	PrintHeader();

	vprint( 0, "    Using reslist dir '%s'\n", g_szReslistDir );

	vprint( 0, "    Looking for extraneous content...\n" );

	char resfile[ 256 ];
	strcpy( resfile, argv[ i - 1 ] );

	vprint( 0, "    Comparing results of resfile (%s) with files under current directory...\n",	resfile );

	char workingdir[ 256 ];
	workingdir[0] = 0;
	Q_getwd( workingdir, sizeof( workingdir ) );

	// If they didn't specify -game on the command line, use VPROJECT.
	CmdLib_InitFileSystem( workingdir );

	filesystem = (IFileSystem *)(CmdLib_GetFileSystemFactory()( FILESYSTEM_INTERFACE_VERSION, NULL ));
	if ( !filesystem )
	{
		AssertMsg( 0, "Failed to create/get IFileSystem" );
		return 1;
	}

	g_pFullFileSystem->RemoveAllSearchPaths();
	g_pFullFileSystem->AddSearchPath(gamedir, "GAME");

	Q_strlower( gamedir );
	Q_FixSlashes( gamedir );

	//
	//ProcessMaterialsDirectory( vmtdir );

	// find out the mod dir name
	Q_strncpy( modname, gamedir, sizeof(modname) );
	modname[ strlen(modname) - 1] = 0;

	if ( strrchr( modname,  '\\' ) )
	{
		Q_strncpy( modname, strrchr( modname, '\\' ) + 1, sizeof(modname) );
	}
	else
	{
		Q_strncpy( modname, "", sizeof(modname) );
	}
	vprint( 1, "Mod Name:%s\n", modname);


	BuildCheckdirList();
	BuildWhiteList();

	vprint( 0, "Building aggregate file list from resfile output\n" );
	CUtlRBTree< ReferencedFile, int > referencedfiles( 0, 0, RefFileLessFunc );
	CUtlVector< UnusedContent::CUtlSymbol >	resfiles;

	BuildReferencedFileList( resfiles, referencedfiles, resfile );

	vprint( 0, "found %i files\n\n", referencedfiles.Count() );

	vprint( 0, "Building list of all game content files\n" );
	CUtlVector< FileEntry > contentfiles;
	CUtlVector< FileEntry > otherfiles;
	BuildFileList( 0, contentfiles, &otherfiles, "", 0 );
	vprint( 0, "found %i files in content tree\n\n", contentfiles.Count() );

	Correlate( referencedfiles, contentfiles, modname );

	// now output the files not referenced in the whitelist or general reslists
	filesystem->RemoveFile( CFmtStr( "%sunreferenced_files.lst", g_szReslistDir ), "GAME" );
	int c = otherfiles.Count();
	for ( i = 0; i < c; ++i )
	{
		FileEntry & entry = otherfiles[ i ];
		char const *name = g_Analysis.symbols.String( entry.sym );

		logprint( CFmtStr( "%sunreferenced_files.lst", g_szReslistDir ), "\"%s\\%s\"\n", modname, name );
	}

	// also include the files from deletions.bat, as we don't actually run that now
	c = contentfiles.Count();
	for ( i = 0; i < c; ++i )
	{
		FileEntry & entry = contentfiles[ i ];
		if ( entry.referenced != REFERENCED_NO )
			continue;

		char const *fn = g_Analysis.symbols.String( entry.sym );
		logprint( CFmtStr( "%sunreferenced_files.lst", g_szReslistDir ), "\"%s\\%s\"\n", modname, fn );
	}

	FileSystem_Term();

	return 0;
}
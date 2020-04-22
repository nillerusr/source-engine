//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#if defined( _WIN32 ) && !defined( _X360 )
#include <windows.h>		// for widechartomultibyte and multibytetowidechar
#elif defined(POSIX)
#include <wchar.h> // wcslen()
#define _alloca alloca
#define _wtoi(arg) wcstol(arg, NULL, 10)
#define _wtoi64(arg) wcstoll(arg, NULL, 10)
#endif

#include <keyvalues.h>
#include "filesystem.h"
#include <vstdlib/ikeyvaluessystem.h>

#include <color.h>
#include <stdlib.h>
#include <ctype.h>
#include "tier1/convar.h"
#include "tier0/dbg.h"
#include "tier0/mem.h"
#include "utlvector.h"
#include "utlbuffer.h"
#include "utlhash.h"
#include <vstdlib/vstrtools.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//////// VPROF? //////////////////
// For an example of how to mark up this file with VPROF nodes, see 
// changelist 702984. However, be aware that calls to FindKey and Init
// may occur outside of Vprof's usual hierarchy, which can cause strange
// duplicate KeyValues::FindKey nodes at the root level and other 
// confusing effects.
//////////////////////////////////

static char * s_LastFileLoadingFrom = "unknown"; // just needed for error messages

// Statics for the growable string table
int (*KeyValues::s_pfGetSymbolForString)( const char *name, bool bCreate ) = &KeyValues::GetSymbolForStringClassic;
const char *(*KeyValues::s_pfGetStringForSymbol)( int symbol ) = &KeyValues::GetStringForSymbolClassic;
CKeyValuesGrowableStringTable *KeyValues::s_pGrowableStringTable = NULL;

#define KEYVALUES_TOKEN_SIZE	1024
static char s_pTokenBuf[KEYVALUES_TOKEN_SIZE];

#define INTERNALWRITE( pData, len ) InternalWrite( filesystem, f, pBuf, pData, len )

#define MAKE_3_BYTES_FROM_1_AND_2( x1, x2 ) (( (( uint16 )x2) << 8 ) | (uint8)(x1))
#define SPLIT_3_BYTES_INTO_1_AND_2( x1, x2, x3 ) do { x1 = (uint8)(x3); x2 = (uint16)( (x3) >> 8 ); } while( 0 )

CExpressionEvaluator g_ExpressionEvaluator;

// a simple class to keep track of a stack of valid parsed symbols
const int MAX_ERROR_STACK = 64;
class CKeyValuesErrorStack
{
public:
	CKeyValuesErrorStack() : m_pFilename("NULL"), m_errorIndex(0), m_maxErrorIndex(0) {}

	void SetFilename( const char *pFilename )
	{
		m_pFilename = pFilename;
		m_maxErrorIndex = 0;
	}

	// entering a new keyvalues block, save state for errors
	// Not save symbols instead of pointers because the pointers can move!
	int Push( int symName )
	{
		if ( m_errorIndex < MAX_ERROR_STACK )
		{
			m_errorStack[m_errorIndex] = symName;
		}
		m_errorIndex++;
		m_maxErrorIndex = MAX( m_maxErrorIndex, (m_errorIndex-1) );
		return m_errorIndex-1;
	}

	// exiting block, error isn't in this block, remove.
	void Pop()
	{
		m_errorIndex--;
		Assert(m_errorIndex>=0);
	}

	// Allows you to keep the same stack level, but change the name as you parse peers
	void Reset( int stackLevel, int symName )
	{
		Assert( stackLevel >= 0 && stackLevel < m_errorIndex );
		m_errorStack[stackLevel] = symName;
	}

	// Hit an error, report it and the parsing stack for context
	void ReportError( const char *pError )
	{
		Warning( "KeyValues Error: %s in file %s\n", pError, m_pFilename );
		for ( int i = 0; i < m_maxErrorIndex; i++ )
		{
			if ( m_errorStack[i] != INVALID_KEY_SYMBOL )
			{
				if ( i < m_errorIndex )
				{
					Warning( "%s, ", KeyValuesSystem()->GetStringForSymbol(m_errorStack[i]) );
				}
				else
				{
					Warning( "(*%s*), ", KeyValuesSystem()->GetStringForSymbol(m_errorStack[i]) );
				}
			}
		}
		Warning( "\n" );
	}

private:
	int		m_errorStack[MAX_ERROR_STACK];
	const char *m_pFilename;
	int		m_errorIndex;
	int		m_maxErrorIndex;
} g_KeyValuesErrorStack;


// a simple helper that creates stack entries as it goes in & out of scope
class CKeyErrorContext
{
public:
	~CKeyErrorContext()
	{
		g_KeyValuesErrorStack.Pop();
	}
	explicit CKeyErrorContext( int symName )
	{
		Init( symName );
	}
	void Reset( int symName )
	{
		g_KeyValuesErrorStack.Reset( m_stackLevel, symName );
	}
private:
	void Init( int symName )
	{
		m_stackLevel = g_KeyValuesErrorStack.Push( symName );
	}

	int m_stackLevel;
};

// Uncomment this line to hit the ~CLeakTrack assert to see what's looking like it's leaking
// #define LEAKTRACK

#ifdef LEAKTRACK

class CLeakTrack
{
public:
	CLeakTrack()
	{
	}
	~CLeakTrack()
	{
		if ( keys.Count() != 0 )
		{
			Assert( 0 );
		}
	}

	struct kve
	{
		KeyValues *kv;
		char		name[ 256 ];
	};

	void AddKv( KeyValues *kv, char const *name )
	{
		kve k;
		V_strncpy( k.name, name ? name : "NULL", sizeof( k.name ) );
		k.kv = kv;

		keys.AddToTail( k );
	}

	void RemoveKv( KeyValues *kv )
	{
		int c = keys.Count();
		for ( int i = 0; i < c; i++ )
		{
			if ( keys[i].kv == kv )
			{
				keys.Remove( i );
				break;
			}
		}
	}

	CUtlVector< kve > keys;
};

static CLeakTrack track;

#define TRACK_KV_ADD( ptr, name )	track.AddKv( ptr, name )
#define TRACK_KV_REMOVE( ptr )		track.RemoveKv( ptr )

#else

#define TRACK_KV_ADD( ptr, name ) 
#define TRACK_KV_REMOVE( ptr )	

#endif


//-----------------------------------------------------------------------------
// Purpose: An arbitrarily growable string table for KeyValues key names. 
//	See the comment in the header for more info.
//-----------------------------------------------------------------------------
class CKeyValuesGrowableStringTable
{
public: 
	// Constructor
	CKeyValuesGrowableStringTable() :
	  m_vecStrings( 0, 512 * 1024 ),
	  m_hashLookup( 2048, 0, 0, m_Functor, m_Functor )
	{
		m_vecStrings.AddToTail( '\0' );
	}

	// Translates a string to an index
	int GetSymbolForString( const char *name, bool bCreate = true )
	{
		AUTO_LOCK( m_mutex );

		// Put the current details into our hash functor
		m_Functor.SetCurString( name );
		m_Functor.SetCurStringBase( (const char *)m_vecStrings.Base() );

		if ( bCreate )
		{
			bool bInserted = false;
			UtlHashHandle_t hElement = m_hashLookup.Insert( -1, &bInserted );
			if ( bInserted )
			{
				int iIndex = m_vecStrings.AddMultipleToTail( V_strlen( name ) + 1, name );
				m_hashLookup[ hElement ] = iIndex;
			}

			return m_hashLookup[ hElement ];
		}
		else
		{
			UtlHashHandle_t hElement = m_hashLookup.Find( -1 );
			if ( m_hashLookup.IsValidHandle( hElement ) )
				return m_hashLookup[ hElement ];
			else
				return -1;
		}
	}

	// Translates an index back to a string
	const char *GetStringForSymbol( int symbol )
	{
		return (const char *)m_vecStrings.Base() + symbol;
	}

private:
	
	// A class plugged into CUtlHash that allows us to change the behavior of the table
	// and store only the index in the table.
	class CLookupFunctor
	{
	public:
		CLookupFunctor() : m_pchCurString( NULL ), m_pchCurBase( NULL ) {}

		// Sets what we are currently inserting or looking for.
		void SetCurString( const char *pchCurString ) { m_pchCurString = pchCurString; }
		void SetCurStringBase( const char *pchCurBase ) { m_pchCurBase = pchCurBase; }

		// The compare function.
		bool operator()( int nLhs, int nRhs ) const
		{
			const char *pchLhs = nLhs > 0 ? m_pchCurBase + nLhs : m_pchCurString;
			const char *pchRhs = nRhs > 0 ? m_pchCurBase + nRhs : m_pchCurString;
			
			return ( 0 == V_stricmp( pchLhs, pchRhs ) );
		}

		// The hash function.
		unsigned int operator()( int nItem ) const
		{
			return HashStringCaseless( m_pchCurString );
		}

	private:
		const char *m_pchCurString;
		const char *m_pchCurBase;
	};

	CThreadFastMutex m_mutex;
	CLookupFunctor	m_Functor;
	CUtlHash<int, CLookupFunctor &, CLookupFunctor &> m_hashLookup;
	CUtlVector<char> m_vecStrings;
};


//-----------------------------------------------------------------------------
// Purpose: Sets whether the KeyValues system should use an arbitrarily growable
//	string table. See the comment in the header for more info.
//-----------------------------------------------------------------------------
void KeyValues::SetUseGrowableStringTable( bool bUseGrowableTable )
{
	if ( bUseGrowableTable )
	{
		s_pfGetStringForSymbol = &(KeyValues::GetStringForSymbolGrowable);
		s_pfGetSymbolForString = &(KeyValues::GetSymbolForStringGrowable);

		if ( NULL == s_pGrowableStringTable )
		{
			s_pGrowableStringTable = new CKeyValuesGrowableStringTable;
		}
	}
	else
	{
		s_pfGetStringForSymbol = &(KeyValues::GetStringForSymbolClassic);
		s_pfGetSymbolForString = &(KeyValues::GetSymbolForStringClassic);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Bodys of the function pointers used for interacting with the key
//	name string table
//-----------------------------------------------------------------------------
int KeyValues::GetSymbolForStringClassic( const char *name, bool bCreate )
{
	return KeyValuesSystem()->GetSymbolForString( name, bCreate );
}

const char *KeyValues::GetStringForSymbolClassic( int symbol )
{
	return KeyValuesSystem()->GetStringForSymbol( symbol );
}

int KeyValues::GetSymbolForStringGrowable( const char *name, bool bCreate )
{
	return s_pGrowableStringTable->GetSymbolForString( name, bCreate );
}

const char *KeyValues::GetStringForSymbolGrowable( int symbol )
{
	return s_pGrowableStringTable->GetStringForSymbol( symbol );
}



//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
KeyValues::KeyValues( const char *setName )
{
	TRACK_KV_ADD( this, setName );

	Init();
	SetName ( setName );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
KeyValues::KeyValues( const char *setName, const char *firstKey, const char *firstValue )
{
	TRACK_KV_ADD( this, setName );

	Init();
	SetName( setName );
	SetString( firstKey, firstValue );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
KeyValues::KeyValues( const char *setName, const char *firstKey, const wchar_t *firstValue )
{
	TRACK_KV_ADD( this, setName );

	Init();
	SetName( setName );
	SetWString( firstKey, firstValue );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
KeyValues::KeyValues( const char *setName, const char *firstKey, int firstValue )
{
	TRACK_KV_ADD( this, setName );

	Init();
	SetName( setName );
	SetInt( firstKey, firstValue );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
KeyValues::KeyValues( const char *setName, const char *firstKey, const char *firstValue, const char *secondKey, const char *secondValue )
{
	TRACK_KV_ADD( this, setName );

	Init();
	SetName( setName );
	SetString( firstKey, firstValue );
	SetString( secondKey, secondValue );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
KeyValues::KeyValues( const char *setName, const char *firstKey, int firstValue, const char *secondKey, int secondValue )
{
	TRACK_KV_ADD( this, setName );

	Init();
	SetName( setName );
	SetInt( firstKey, firstValue );
	SetInt( secondKey, secondValue );
}

//-----------------------------------------------------------------------------
// Purpose: Initialize member variables
//-----------------------------------------------------------------------------
void KeyValues::Init()
{
	m_iKeyName = 0;
	m_iKeyNameCaseSensitive1 = 0;
	m_iKeyNameCaseSensitive2 = 0;
	m_iDataType = TYPE_NONE;

	m_pSub = NULL;
	m_pPeer = NULL;
	m_pChain = NULL;

	m_sValue = NULL;
	m_wsValue = NULL;
	m_pValue = NULL;
	
	m_bHasEscapeSequences = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
KeyValues::~KeyValues()
{
	TRACK_KV_REMOVE( this );

	RemoveEverything();
}

//-----------------------------------------------------------------------------
// Purpose: remove everything
//-----------------------------------------------------------------------------
void KeyValues::RemoveEverything()
{
	KeyValues *dat;
	KeyValues *datNext = NULL;
	for ( dat = m_pSub; dat != NULL; dat = datNext )
	{
		datNext = dat->m_pPeer;
		dat->m_pPeer = NULL;
		delete dat;
	}

	for ( dat = m_pPeer; dat && dat != this; dat = datNext )
	{
		datNext = dat->m_pPeer;
		dat->m_pPeer = NULL;
		delete dat;
	}

	delete [] m_sValue;
	m_sValue = NULL;
	delete [] m_wsValue;
	m_wsValue = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *f - 
//-----------------------------------------------------------------------------

void KeyValues::RecursiveSaveToFile( CUtlBuffer& buf, int indentLevel )
{
	RecursiveSaveToFile( NULL, FILESYSTEM_INVALID_HANDLE, &buf, indentLevel );
}

//-----------------------------------------------------------------------------
// Adds a chain... if we don't find stuff in this keyvalue, we'll look
// in the one we're chained to.
//-----------------------------------------------------------------------------

void KeyValues::ChainKeyValue( KeyValues* pChain )
{
	m_pChain = pChain;
}

//-----------------------------------------------------------------------------
// Purpose: Get the name of the current key section
//-----------------------------------------------------------------------------
const char *KeyValues::GetName( void ) const
{
	return this ? KeyValuesSystem()->GetStringForSymbol( MAKE_3_BYTES_FROM_1_AND_2( m_iKeyNameCaseSensitive1, m_iKeyNameCaseSensitive2 ) ) : "";
}

//-----------------------------------------------------------------------------
// Purpose: Get the symbol name of the current key section
//-----------------------------------------------------------------------------
int KeyValues::GetNameSymbol() const
{
	return this ? m_iKeyName : INVALID_KEY_SYMBOL;
}

int KeyValues::GetNameSymbolCaseSensitive() const
{
	return this ? MAKE_3_BYTES_FROM_1_AND_2( m_iKeyNameCaseSensitive1, m_iKeyNameCaseSensitive2 ) : INVALID_KEY_SYMBOL;
}


//-----------------------------------------------------------------------------
// Purpose: Read a single token from buffer (0 terminated)
//-----------------------------------------------------------------------------
#pragma warning (disable:4706)
const char *KeyValues::ReadToken( CUtlBuffer &buf, bool &wasQuoted, bool &wasConditional )
{
	wasQuoted = false;
	wasConditional = false;

	if ( !buf.IsValid() )
		return NULL; 

	// eating white spaces and remarks loop
	while ( true )
	{
		buf.EatWhiteSpace();
		if ( !buf.IsValid() )
			return NULL;	// file ends after reading whitespaces

		// stop if it's not a comment; a new token starts here
		if ( !buf.EatCPPComment() )
			break;
	}

	const char *c = (const char*)buf.PeekGet( sizeof(char), 0 );
	if ( !c )
		return NULL;

	// read quoted strings specially
	if ( *c == '\"' )
	{
		wasQuoted = true;
		buf.GetDelimitedString( m_bHasEscapeSequences ? GetCStringCharConversion() : GetNoEscCharConversion(), 
			s_pTokenBuf, KEYVALUES_TOKEN_SIZE );
		return s_pTokenBuf;
	}

	if ( *c == '{' || *c == '}' )
	{
		// it's a control char, just add this one char and stop reading
		s_pTokenBuf[0] = *c;
		s_pTokenBuf[1] = 0;
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 1 );
		return s_pTokenBuf;
	}

	// read in the token until we hit a whitespace or a control character
	bool bReportedError = false;
	bool bConditionalStart = false;
	int nCount = 0;
	while ( c = (const char*)buf.PeekGet( sizeof(char), 0 ) )
	{
		// end of file
		if ( *c == 0 )
			break;

		// break if any control character appears in non quoted tokens
		if ( *c == '"' || *c == '{' || *c == '}' )
			break;

		if ( *c == '[' )
		{
			bConditionalStart = true;
		}

		if ( *c == ']' && bConditionalStart )
		{
			bConditionalStart = false;
			wasConditional = true;
		}

		// conditionals need to get tokenized as delimited by []
		// othwerwise break on whitespace
		if ( V_isspace(*c) && !bConditionalStart )
			break;

		if (nCount < (KEYVALUES_TOKEN_SIZE-1) )
		{
			s_pTokenBuf[nCount++] = *c;	// add char to buffer
		}
		else if ( !bReportedError )
		{
			bReportedError = true;
			g_KeyValuesErrorStack.ReportError(" ReadToken overflow" );
		}

		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 1 );
	}
	s_pTokenBuf[ nCount ] = 0;
	return s_pTokenBuf;
}
#pragma warning (default:4706)

	

//-----------------------------------------------------------------------------
// Purpose: if parser should translate escape sequences ( /n, /t etc), set to true
//-----------------------------------------------------------------------------
void KeyValues::UsesEscapeSequences(bool state)
{
	m_bHasEscapeSequences = state;
}


//-----------------------------------------------------------------------------
// Purpose: Load keyValues from disk
//-----------------------------------------------------------------------------
bool KeyValues::LoadFromFile( IBaseFileSystem *filesystem, const char *resourceName, const char *pathID, GetSymbolProc_t pfnEvaluateSymbolProc )
{
#if defined( _WIN32 )
	Assert( IsGameConsole() || ( _heapchk() == _HEAPOK ) );
#endif
	FileHandle_t f = filesystem->Open(resourceName, "rb", pathID);
	if ( !f )
		return false;

	s_LastFileLoadingFrom = (char*)resourceName;

	// load file into a null-terminated buffer
	int fileSize = filesystem->Size( f );
	unsigned bufSize = ((IFileSystem *)filesystem)->GetOptimalReadSize( f, fileSize + 2 );

	char *buffer = (char*)((IFileSystem *)filesystem)->AllocOptimalReadBuffer( f, bufSize );
	Assert( buffer );
	
	// read into local buffer
	bool bRetOK = ( ((IFileSystem *)filesystem)->ReadEx( buffer, bufSize, fileSize, f ) != 0 );

	filesystem->Close( f );	// close file after reading

	if ( bRetOK )
	{
		buffer[fileSize] = 0; // null terminate file as EOF
		buffer[fileSize+1] = 0; // double NULL terminating in case this is a unicode file

		CUtlBuffer utlBuffer;
		if ( IsGameConsole() && (unsigned int)((unsigned char *)buffer)[0] == KV_BINARY_POOLED_FORMAT )
		{
			// kv contents are compiled binary
			utlBuffer.SetExternalBuffer( buffer, bufSize, fileSize, CUtlBuffer::READ_ONLY );
		}
		else
		{
			// kv contents are text
			int nLen = V_strlen( buffer );
			utlBuffer.SetExternalBuffer( buffer, bufSize, nLen, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );
		}

		bRetOK = LoadFromBuffer( resourceName, utlBuffer, filesystem, pathID, pfnEvaluateSymbolProc );
	}

	((IFileSystem *)filesystem)->FreeOptimalReadBuffer( buffer );

	return bRetOK;
}

//-----------------------------------------------------------------------------
// Purpose: Save the keyvalues to disk
//			Creates the path to the file if it doesn't exist 
//-----------------------------------------------------------------------------
bool KeyValues::SaveToFile( IBaseFileSystem *filesystem, const char *resourceName, const char *pathID )
{
	// create a write file
	FileHandle_t f = filesystem->Open(resourceName, "wb", pathID);

	if ( f == FILESYSTEM_INVALID_HANDLE )
	{
		DevMsg( "KeyValues::SaveToFile: couldn't open file \"%s\" in path \"%s\".\n", 
			resourceName?resourceName:"NULL", pathID?pathID:"NULL" );
		return false;
	}

	RecursiveSaveToFile(filesystem, f, NULL, 0);
	filesystem->Close(f);

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Write out a set of indenting
//-----------------------------------------------------------------------------
void KeyValues::WriteIndents( IBaseFileSystem *filesystem, FileHandle_t f, CUtlBuffer *pBuf, int indentLevel )
{
	for ( int i = 0; i < indentLevel; i++ )
	{
		INTERNALWRITE( "\t", 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Write out a string where we convert the double quotes to backslash double quote
//-----------------------------------------------------------------------------
void KeyValues::WriteConvertedString( IBaseFileSystem *filesystem, FileHandle_t f, CUtlBuffer *pBuf, const char *pszString )
{
	// handle double quote chars within the string
	// the worst possible case is that the whole string is quotes
	int len = V_strlen(pszString);
	char *convertedString = (char *) alloca ((len + 1)  * sizeof(char) * 2);
	int j=0;
	for (int i=0; i <= len; i++)
	{
		if (pszString[i] == '\"')
		{
			convertedString[j] = '\\';
			j++;
		}
		else if ( m_bHasEscapeSequences && pszString[i] == '\\' )
		{
			convertedString[j] = '\\';
			j++;
		}
		convertedString[j] = pszString[i];
		j++;
	}		

	INTERNALWRITE(convertedString, strlen(convertedString));
}


void KeyValues::InternalWrite( IBaseFileSystem *filesystem, FileHandle_t f, CUtlBuffer *pBuf, const void *pData, int len )
{
	if ( filesystem )
	{
		filesystem->Write( pData, len, f );
	}

	if ( pBuf )
	{
		pBuf->Put( pData, len );
	}
} 


//-----------------------------------------------------------------------------
// Purpose: Save keyvalues from disk, if subkey values are detected, calls
//			itself to save those
//-----------------------------------------------------------------------------
void KeyValues::RecursiveSaveToFile( IBaseFileSystem *filesystem, FileHandle_t f, CUtlBuffer *pBuf, int indentLevel )
{
	// write header
	WriteIndents( filesystem, f, pBuf, indentLevel );
	INTERNALWRITE("\"", 1);
	WriteConvertedString(filesystem, f, pBuf, GetName());	
	INTERNALWRITE("\"\n", 2);
	WriteIndents( filesystem, f, pBuf, indentLevel );
	INTERNALWRITE("{\n", 2);

	// loop through all our keys writing them to disk
	for ( KeyValues *dat = m_pSub; dat != NULL; dat = dat->m_pPeer )
	{
		if ( dat->m_pSub )
		{
			dat->RecursiveSaveToFile( filesystem, f, pBuf, indentLevel + 1 );
		}
		else
		{
			// only write non-empty keys

			switch (dat->m_iDataType)
			{
			case TYPE_STRING:
				{
					if (dat->m_sValue && *(dat->m_sValue))
					{
						WriteIndents(filesystem, f, pBuf, indentLevel + 1);
						INTERNALWRITE("\"", 1);
						WriteConvertedString(filesystem, f, pBuf, dat->GetName());	
						INTERNALWRITE("\"\t\t\"", 4);

						WriteConvertedString(filesystem, f, pBuf, dat->m_sValue);	

						INTERNALWRITE("\"\n", 2);
					}
					break;
				}
			case TYPE_WSTRING:
				{
					if ( dat->m_wsValue )
					{
						static char buf[KEYVALUES_TOKEN_SIZE];
						// make sure we have enough space
						int result = V_UnicodeToUTF8( dat->m_wsValue, buf, KEYVALUES_TOKEN_SIZE);
						if (result)
						{
							WriteIndents(filesystem, f, pBuf, indentLevel + 1);
							INTERNALWRITE("\"", 1);
							INTERNALWRITE(dat->GetName(), V_strlen(dat->GetName()));
							INTERNALWRITE("\"\t\t\"", 4);

							WriteConvertedString(filesystem, f, pBuf, buf);

							INTERNALWRITE("\"\n", 2);
						}
					}
					break;
				}

			case TYPE_INT:
				{
					WriteIndents(filesystem, f, pBuf, indentLevel + 1);
					INTERNALWRITE("\"", 1);
					INTERNALWRITE(dat->GetName(), V_strlen(dat->GetName()));
					INTERNALWRITE("\"\t\t\"", 4);

					char buf[32];
					V_snprintf(buf, sizeof( buf ), "%d", dat->m_iValue);

					INTERNALWRITE(buf, V_strlen(buf));
					INTERNALWRITE("\"\n", 2);
					break;
				}

			case TYPE_UINT64:
				{
					WriteIndents(filesystem, f, pBuf, indentLevel + 1);
					INTERNALWRITE("\"", 1);
					INTERNALWRITE(dat->GetName(), V_strlen(dat->GetName()));
					INTERNALWRITE("\"\t\t\"", 4);

					char buf[32];
					// write "0x" + 16 char 0-padded hex encoded 64 bit value
					V_snprintf( buf, sizeof( buf ), "0x%016llX", *( (uint64 *)dat->m_sValue ) );

					INTERNALWRITE(buf, V_strlen(buf));
					INTERNALWRITE("\"\n", 2);
					break;
				}

			case TYPE_FLOAT:
				{
					WriteIndents(filesystem, f, pBuf, indentLevel + 1);
					INTERNALWRITE("\"", 1);
					INTERNALWRITE(dat->GetName(), V_strlen(dat->GetName()));
					INTERNALWRITE("\"\t\t\"", 4);

					char buf[48];
					V_snprintf(buf, sizeof( buf ), "%f", dat->m_flValue);

					INTERNALWRITE(buf, V_strlen(buf));
					INTERNALWRITE("\"\n", 2);
					break;
				}
			case TYPE_COLOR:
				DevMsg( "KeyValues::RecursiveSaveToFile: TODO, missing code for TYPE_COLOR.\n" );
				break;

			default:
				break;
			}
		}
	}

	// write tail
	WriteIndents(filesystem, f, pBuf, indentLevel);
	INTERNALWRITE("}\n", 2);
}

//-----------------------------------------------------------------------------
// Purpose: looks up a key by symbol name
//-----------------------------------------------------------------------------
KeyValues *KeyValues::FindKey(int keySymbol) const
{
	for (KeyValues *dat = this ? m_pSub : NULL; dat != NULL; dat = dat->m_pPeer)
	{
		if ( dat->m_iKeyName == (uint32) keySymbol )
			return dat;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Find a keyValue, create it if it is not found.
//			Set bCreate to true to create the key if it doesn't already exist 
//			(which ensures a valid pointer will be returned)
//-----------------------------------------------------------------------------
KeyValues *KeyValues::FindKey(const char *keyName, bool bCreate)
{
	// Validate NULL == this early out
	if ( !this )
	{
		Assert( !bCreate );
		return NULL;
	}

	// return the current key if a NULL subkey is asked for
	if (!keyName || !keyName[0])
		return this;

	// look for '/' characters deliminating sub fields
	char szBuf[256];
	const char *subStr = strchr(keyName, '/');
	const char *searchStr = keyName;

	// pull out the substring if it exists
	if (subStr)
	{
		int size = subStr - keyName;
		V_memcpy( szBuf, keyName, size );
		szBuf[size] = 0;
		searchStr = szBuf;
	}

	// lookup the symbol for the search string,
	// we do not need the case-sensitive symbol at this time
	// because if the key is found, then it will be found by case-insensitive lookup
	// if the key is not found and needs to be created we will pass the actual searchStr
	// and have the new KeyValues constructor get/create the case-sensitive symbol
	HKeySymbol iSearchStr = KeyValuesSystem()->GetSymbolForString( searchStr, bCreate );
	if ( iSearchStr == INVALID_KEY_SYMBOL )
	{
		// not found, couldn't possibly be in key value list
		return NULL;
	}

	KeyValues *lastItem = NULL;
	KeyValues *dat;
	// find the searchStr in the current peer list
	for (dat = m_pSub; dat != NULL; dat = dat->m_pPeer)
	{
		lastItem = dat;	// record the last item looked at (for if we need to append to the end of the list)

		// symbol compare
		if ( dat->m_iKeyName == ( uint32 ) iSearchStr )
		{
			break;
		}
	}

	if ( !dat && m_pChain )
	{
		dat = m_pChain->FindKey(keyName, false);
	}

	// make sure a key was found
	if (!dat)
	{
		if (bCreate)
		{
			// we need to create a new key
			dat = new KeyValues( searchStr );
//			Assert(dat != NULL);

			// insert new key at end of list
			if (lastItem)
			{
				lastItem->m_pPeer = dat;
			}
			else
			{
				m_pSub = dat;
			}
			dat->m_pPeer = NULL;

			// a key graduates to be a submsg as soon as it's m_pSub is set
			// this should be the only place m_pSub is set
			m_iDataType = TYPE_NONE;
		}
		else
		{
			return NULL;
		}
	}
	
	// if we've still got a subStr we need to keep looking deeper in the tree
	if ( subStr )
	{
		// recursively chain down through the paths in the string
		return dat->FindKey(subStr + 1, bCreate);
	}

	return dat;
}

//-----------------------------------------------------------------------------
// Purpose: Create a new key, with an autogenerated name.  
//			Name is guaranteed to be an integer, of value 1 higher than the highest 
//			other integer key name
//-----------------------------------------------------------------------------
KeyValues *KeyValues::CreateNewKey()
{
	int newID = 1;

	// search for any key with higher values
	for (KeyValues *dat = m_pSub; dat != NULL; dat = dat->m_pPeer)
	{
		// case-insensitive string compare
		int val = atoi(dat->GetName());
		if (newID <= val)
		{
			newID = val + 1;
		}
	}

	char buf[12];
	V_snprintf( buf, sizeof(buf), "%d", newID );

	return CreateKey( buf );
}


//-----------------------------------------------------------------------------
// Create a key
//-----------------------------------------------------------------------------
KeyValues* KeyValues::CreateKey( const char *keyName )
{
	// key wasn't found so just create a new one
	KeyValues* dat = new KeyValues( keyName );

	dat->UsesEscapeSequences( m_bHasEscapeSequences != 0 ); // use same format as parent does
	
	// add into subkey list
	AddSubKey( dat );

	return dat;
}


//-----------------------------------------------------------------------------
// Adds a subkey. Make sure the subkey isn't a child of some other keyvalues
//-----------------------------------------------------------------------------
void KeyValues::AddSubKey( KeyValues *pSubkey )
{
	// Make sure the subkey isn't a child of some other keyvalues
	Assert( pSubkey->m_pPeer == NULL );

	// add into subkey list
	if ( m_pSub == NULL )
	{
		m_pSub = pSubkey;
	}
	else
	{
		KeyValues *pTempDat = m_pSub;
		while ( pTempDat->GetNextKey() != NULL )
		{
			pTempDat = pTempDat->GetNextKey();
		}

		pTempDat->SetNextKey( pSubkey );
	}
}


	
//-----------------------------------------------------------------------------
// Purpose: Remove a subkey from the list
//-----------------------------------------------------------------------------
void KeyValues::RemoveSubKey(KeyValues *subKey)
{
	if (!subKey)
		return;

	// check the list pointer
	if (m_pSub == subKey)
	{
		m_pSub = subKey->m_pPeer;
	}
	else
	{
		// look through the list
		KeyValues *kv = m_pSub;
		while (kv->m_pPeer)
		{
			if (kv->m_pPeer == subKey)
			{
				kv->m_pPeer = subKey->m_pPeer;
				break;
			}
			
			kv = kv->m_pPeer;
		}
	}

	subKey->m_pPeer = NULL;
}

void KeyValues::InsertSubKey( int nIndex, KeyValues *pSubKey )
{
	// Sub key must be valid and not part of another chain
	Assert( pSubKey && pSubKey->m_pPeer == NULL );

	if ( nIndex == 0 )
	{
		pSubKey->m_pPeer = m_pSub;
		m_pSub = pSubKey;
		return;
	}
	else
	{
		int nCurrentIndex = 0;
		for ( KeyValues *pIter = GetFirstSubKey(); pIter != NULL; pIter = pIter->GetNextKey() )
		{
			++ nCurrentIndex;
			if ( nCurrentIndex == nIndex)
			{
				pSubKey->m_pPeer = pIter->m_pPeer;
				pIter->m_pPeer = pSubKey;
				return;
			}
		}
		// Index is out of range if we get here
		Assert( 0 );
		return;
	}
}

bool KeyValues::ContainsSubKey( KeyValues *pSubKey )
{
	for ( KeyValues *pIter = GetFirstSubKey(); pIter != NULL; pIter = pIter->GetNextKey() )
	{
		if ( pSubKey == pIter )
		{
			return true;
		}
	}
	return false;
}

void KeyValues::SwapSubKey( KeyValues *pExistingSubkey, KeyValues *pNewSubKey )
{
	Assert( pExistingSubkey != NULL && pNewSubKey != NULL );

	// Make sure the new sub key isn't a child of some other keyvalues
	Assert( pNewSubKey->m_pPeer == NULL );

	// Check the list pointer
	if ( m_pSub == pExistingSubkey )
	{
		pNewSubKey->m_pPeer = pExistingSubkey->m_pPeer;
		pExistingSubkey->m_pPeer = NULL;
		m_pSub = pNewSubKey;
	}
	else
	{
		// Look through the list
		KeyValues *kv = m_pSub;
		while ( kv->m_pPeer )
		{
			if ( kv->m_pPeer == pExistingSubkey )
			{
				pNewSubKey->m_pPeer = pExistingSubkey->m_pPeer;
				pExistingSubkey->m_pPeer = NULL;
				kv->m_pPeer = pNewSubKey;
				break;
			}

			kv = kv->m_pPeer;
		}
		// Existing sub key should always be found, otherwise it's a bug in the calling code.
		Assert( kv->m_pPeer != NULL );
	}
}

void KeyValues::ElideSubKey( KeyValues *pSubKey )
{
	// This pointer's "next" pointer needs to be fixed up when we elide the key
	KeyValues **ppPointerToFix = &m_pSub;
	for ( KeyValues *pKeyIter = m_pSub; pKeyIter != NULL; ppPointerToFix = &pKeyIter->m_pPeer, pKeyIter = pKeyIter->GetNextKey() )
	{
		if ( pKeyIter == pSubKey )
		{
			if ( pSubKey->m_pSub == NULL )
			{
				// No children, simply remove the key
				*ppPointerToFix = pSubKey->m_pPeer;
				pSubKey->deleteThis();
			}
			else
			{
				*ppPointerToFix = pSubKey->m_pSub;
				// Attach the remainder of this chain to the last child of pSubKey
				KeyValues *pChildIter = pSubKey->m_pSub;
				while ( pChildIter->m_pPeer != NULL )
				{
					pChildIter = pChildIter->m_pPeer;
				}
				// Now points to the last child of pSubKey
				pChildIter->m_pPeer = pSubKey->m_pPeer;
				// Detach the node to be elided
				pSubKey->m_pSub = NULL;
				pSubKey->m_pPeer = NULL;
				pSubKey->deleteThis();
			}
			return;		
		}
	}
	// Key not found; that's caller error.
	Assert( 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Return the first subkey in the list
//-----------------------------------------------------------------------------
KeyValues *KeyValues::GetFirstSubKey()
{
	return this ? m_pSub : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Return the next subkey
//-----------------------------------------------------------------------------
KeyValues *KeyValues::GetNextKey()
{
	return this ? m_pPeer : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Sets this key's peer to the KeyValues passed in
//-----------------------------------------------------------------------------
void KeyValues::SetNextKey( KeyValues *pDat )
{
	m_pPeer = pDat;
}


KeyValues* KeyValues::GetFirstTrueSubKey()
{
	KeyValues *pRet = this ? m_pSub : NULL;
	while ( pRet && pRet->m_iDataType != TYPE_NONE )
		pRet = pRet->m_pPeer;

	return pRet;
}

KeyValues* KeyValues::GetNextTrueSubKey()
{
	KeyValues *pRet = this ? m_pPeer : NULL;
	while ( pRet && pRet->m_iDataType != TYPE_NONE )
		pRet = pRet->m_pPeer;

	return pRet;
}

KeyValues* KeyValues::GetFirstValue()
{
	KeyValues *pRet = this ? m_pSub : NULL;
	while ( pRet && pRet->m_iDataType == TYPE_NONE )
		pRet = pRet->m_pPeer;

	return pRet;
}

KeyValues* KeyValues::GetNextValue()
{
	KeyValues *pRet = this ? m_pPeer : NULL;
	while ( pRet && pRet->m_iDataType == TYPE_NONE )
		pRet = pRet->m_pPeer;

	return pRet;
}


//-----------------------------------------------------------------------------
// Purpose: Get the integer value of a keyName. Default value is returned
//			if the keyName can't be found.
//-----------------------------------------------------------------------------
int KeyValues::GetInt( const char *keyName, int defaultValue )
{
	KeyValues *dat = FindKey( keyName, false );
	if ( dat )
	{
		switch ( dat->m_iDataType )
		{
		case TYPE_STRING:
			return atoi(dat->m_sValue);
		case TYPE_WSTRING:
			return _wtoi(dat->m_wsValue);
		case TYPE_FLOAT:
			return (int)dat->m_flValue;
		case TYPE_UINT64:
			// can't convert, since it would lose data
			Assert(0);
			return 0;
		case TYPE_INT:
		case TYPE_PTR:
		default:
			return dat->m_iValue;
		};
	}
	return defaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Get the integer value of a keyName. Default value is returned
//			if the keyName can't be found.
//-----------------------------------------------------------------------------
uint64 KeyValues::GetUint64( const char *keyName, uint64 defaultValue )
{
	KeyValues *dat = FindKey( keyName, false );
	if ( dat )
	{
		switch ( dat->m_iDataType )
		{
		case TYPE_STRING:
			{
				uint64 uiResult = 0ull;
				sscanf( dat->m_sValue, "%lld", &uiResult );
				return uiResult;
			}
		case TYPE_WSTRING:
			{
				uint64 uiResult = 0ull;
				swscanf( dat->m_wsValue, L"%lld", &uiResult );
				return uiResult;
			}
		case TYPE_FLOAT:
			return (int)dat->m_flValue;
		case TYPE_UINT64:
			return *((uint64 *)dat->m_sValue);
		case TYPE_INT:
		case TYPE_PTR:
		default:
			return dat->m_iValue;
		};
	}
	return defaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Get the pointer value of a keyName. Default value is returned
//			if the keyName can't be found.
//-----------------------------------------------------------------------------
void *KeyValues::GetPtr( const char *keyName, void *defaultValue )
{
	KeyValues *dat = FindKey( keyName, false );
	if ( dat )
	{
		switch ( dat->m_iDataType )
		{
		case TYPE_PTR:
			return dat->m_pValue;

		case TYPE_WSTRING:
		case TYPE_STRING:
		case TYPE_FLOAT:
		case TYPE_INT:
		case TYPE_UINT64:
		default:
			return NULL;
		};
	}
	return defaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Get the float value of a keyName. Default value is returned
//			if the keyName can't be found.
//-----------------------------------------------------------------------------
float KeyValues::GetFloat( const char *keyName, float defaultValue )
{
	KeyValues *dat = FindKey( keyName, false );
	if ( dat )
	{
		switch ( dat->m_iDataType )
		{
		case TYPE_STRING:
			return (float)atof(dat->m_sValue);
		case TYPE_WSTRING:
#ifdef WIN32
			return (float) _wtof(dat->m_wsValue);		// no wtof
#else
			return (float) wcstof( dat->m_wsValue, (wchar_t **)NULL ); 
#endif
			case TYPE_FLOAT:
			return dat->m_flValue;
		case TYPE_INT:
			return (float)dat->m_iValue;
		case TYPE_UINT64:
			return (float)(*((uint64 *)dat->m_sValue));
		case TYPE_PTR:
		default:
			return 0.0f;
		};
	}
	return defaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Get the string pointer of a keyName. Default value is returned
//			if the keyName can't be found.
//-----------------------------------------------------------------------------
const char *KeyValues::GetString( const char *keyName, const char *defaultValue )
{
	KeyValues *dat = FindKey( keyName, false );
	if ( dat )
	{
		// convert the data to string form then return it
		char buf[64];
		switch ( dat->m_iDataType )
		{
		case TYPE_FLOAT:
			V_snprintf( buf, sizeof( buf ), "%f", dat->m_flValue );
			SetString( keyName, buf );
			break;
		case TYPE_INT:
		case TYPE_PTR:
			V_snprintf( buf, sizeof( buf ), "%d", dat->m_iValue );
			SetString( keyName, buf );
			break;
		case TYPE_UINT64:
			V_snprintf( buf, sizeof( buf ), "%lld", *((uint64 *)(dat->m_sValue)) );
			SetString( keyName, buf );
			break;

		case TYPE_WSTRING:
		{
			// convert the string to char *, set it for future use, and return it
			char wideBuf[512];
			int result = V_UnicodeToUTF8(dat->m_wsValue, wideBuf, 512);
			if ( result )
			{
				// note: this will copy wideBuf
				SetString( keyName, wideBuf );
			}
			else
			{
				return defaultValue;
			}
			break;
		}
		case TYPE_STRING:
			break;
		default:
			return defaultValue;
		};
		
		return dat->m_sValue;
	}
	return defaultValue;
}


const wchar_t *KeyValues::GetWString( const char *keyName, const wchar_t *defaultValue)
{
	KeyValues *dat = FindKey( keyName, false );
	if ( dat )
	{
		wchar_t wbuf[64];
		switch ( dat->m_iDataType )
		{
		case TYPE_FLOAT:
			swprintf(wbuf, V_ARRAYSIZE(wbuf), L"%f", dat->m_flValue);
			SetWString( keyName, wbuf);
			break;
		case TYPE_INT:
		case TYPE_PTR:
			swprintf( wbuf, V_ARRAYSIZE(wbuf), L"%d", dat->m_iValue );
			SetWString( keyName, wbuf );
			break;
		case TYPE_UINT64:
			{
				swprintf( wbuf, V_ARRAYSIZE(wbuf), L"%lld", *((uint64 *)(dat->m_sValue)) );
				SetWString( keyName, wbuf );
			}
			break;

		case TYPE_WSTRING:
			break;
		case TYPE_STRING:
		{
			int bufSize = V_strlen(dat->m_sValue) + 1;
			wchar_t *pWBuf = new wchar_t[ bufSize ];
			int result = V_UTF8ToUnicode(dat->m_sValue, pWBuf, bufSize * sizeof( wchar_t ) );
			if ( result >= 0 ) // may be a zero length string
			{
				SetWString( keyName, pWBuf);
			}
			else
			{
				delete [] pWBuf;
				return defaultValue;
			}
			delete [] pWBuf;
			break;
		}
		default:
			return defaultValue;
		};
		
		return (const wchar_t* )dat->m_wsValue;
	}
	return defaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Gets a color
//-----------------------------------------------------------------------------
Color KeyValues::GetColor( const char *keyName , const Color& defaultColor )
{
	Color color = defaultColor;
	KeyValues *dat = FindKey( keyName , false );
	if ( dat )
	{
		if ( dat->m_iDataType == TYPE_COLOR )
		{
			color[0] = dat->m_Color[0];
			color[1] = dat->m_Color[1];
			color[2] = dat->m_Color[2];
			color[3] = dat->m_Color[3];
		}
		else if ( dat->m_iDataType == TYPE_FLOAT )
		{
			color[0] = (unsigned char)dat->m_flValue;
		}
		else if ( dat->m_iDataType == TYPE_INT )
		{
			color[0] = (unsigned char)dat->m_iValue;
		}
		else if ( dat->m_iDataType == TYPE_STRING )
		{
			// parse the colors out of the string
			float a, b, c, d;
			sscanf(dat->m_sValue, "%f %f %f %f", &a, &b, &c, &d);
			color[0] = (unsigned char)a;
			color[1] = (unsigned char)b;
			color[2] = (unsigned char)c;
			color[3] = (unsigned char)d;
		}
	}
	return color;
}

//-----------------------------------------------------------------------------
// Purpose: Sets a color
//-----------------------------------------------------------------------------
void KeyValues::SetColor( const char *keyName, Color value)
{
	KeyValues *dat = FindKey( keyName, true );

	if ( dat )
	{
		dat->m_iDataType = TYPE_COLOR;
		dat->m_Color[0] = value[0];
		dat->m_Color[1] = value[1];
		dat->m_Color[2] = value[2];
		dat->m_Color[3] = value[3];
	}
}

void KeyValues::SetStringValue( char const *strValue )
{
	// delete the old value
	delete [] m_sValue;
	// make sure we're not storing the WSTRING  - as we're converting over to STRING
	delete [] m_wsValue;
	m_wsValue = NULL;

	if (!strValue)
	{
		// ensure a valid value
		strValue = "";
	}

	// allocate memory for the new value and copy it in
	int len = V_strlen( strValue );
	m_sValue = new char[len + 1];
	V_memcpy( m_sValue, strValue, len+1 );

	m_iDataType = TYPE_STRING;
}

//-----------------------------------------------------------------------------
// Purpose: Set the string value of a keyName. 
//-----------------------------------------------------------------------------
void KeyValues::SetString( const char *keyName, const char *value )
{
	if ( KeyValues *dat = FindKey( keyName, true ) )
	{
		dat->SetStringValue( value );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the string value of a keyName. 
//-----------------------------------------------------------------------------
void KeyValues::SetWString( const char *keyName, const wchar_t *value )
{
	KeyValues *dat = FindKey( keyName, true );
	if ( dat )
	{
		// delete the old value
		delete [] dat->m_wsValue;
		// make sure we're not storing the STRING  - as we're converting over to WSTRING
		delete [] dat->m_sValue;
		dat->m_sValue = NULL;

		if (!value)
		{
			// ensure a valid value
			value = L"";
		}

		// allocate memory for the new value and copy it in
		int len = wcslen( value );
		dat->m_wsValue = new wchar_t[len + 1];
		V_memcpy( dat->m_wsValue, value, (len+1) * sizeof(wchar_t) );

		dat->m_iDataType = TYPE_WSTRING;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the integer value of a keyName. 
//-----------------------------------------------------------------------------
void KeyValues::SetInt( const char *keyName, int value )
{
	KeyValues *dat = FindKey( keyName, true );

	if ( dat )
	{
		dat->m_iValue = value;
		dat->m_iDataType = TYPE_INT;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the integer value of a keyName. 
//-----------------------------------------------------------------------------
void KeyValues::SetUint64( const char *keyName, uint64 value )
{
	KeyValues *dat = FindKey( keyName, true );

	if ( dat )
	{
		// delete the old value
		delete [] dat->m_sValue;
		// make sure we're not storing the WSTRING  - as we're converting over to STRING
		delete [] dat->m_wsValue;
		dat->m_wsValue = NULL;

		dat->m_sValue = new char[sizeof(uint64)];
		*((uint64 *)dat->m_sValue) = value;
		dat->m_iDataType = TYPE_UINT64;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the float value of a keyName. 
//-----------------------------------------------------------------------------
void KeyValues::SetFloat( const char *keyName, float value )
{
	KeyValues *dat = FindKey( keyName, true );

	if ( dat )
	{
		dat->m_flValue = value;
		dat->m_iDataType = TYPE_FLOAT;
	}
}

void KeyValues::SetName( const char * setName )
{
	HKeySymbol hCaseSensitiveKeyName = INVALID_KEY_SYMBOL, hCaseInsensitiveKeyName = INVALID_KEY_SYMBOL;
	hCaseSensitiveKeyName = KeyValuesSystem()->GetSymbolForStringCaseSensitive( hCaseInsensitiveKeyName, setName );
	
	m_iKeyName = hCaseInsensitiveKeyName;
	SPLIT_3_BYTES_INTO_1_AND_2( m_iKeyNameCaseSensitive1, m_iKeyNameCaseSensitive2, hCaseSensitiveKeyName );
}

//-----------------------------------------------------------------------------
// Purpose: Set the pointer value of a keyName. 
//-----------------------------------------------------------------------------
void KeyValues::SetPtr( const char *keyName, void *value )
{
	KeyValues *dat = FindKey( keyName, true );

	if ( dat )
	{
		dat->m_pValue = value;
		dat->m_iDataType = TYPE_PTR;
	}
}

void KeyValues::RecursiveCopyKeyValues( KeyValues& src )
{
	// garymcthack - need to check this code for possible buffer overruns.
	
	m_iKeyName = src.m_iKeyName;
	m_iKeyNameCaseSensitive1 = src.m_iKeyNameCaseSensitive1;
	m_iKeyNameCaseSensitive2 = src.m_iKeyNameCaseSensitive2;

	if( !src.m_pSub )
	{
		m_iDataType = src.m_iDataType;
		char buf[256];
		switch( src.m_iDataType )
		{
		case TYPE_NONE:
			break;
		case TYPE_STRING:
			if( src.m_sValue )
			{
				int len = V_strlen(src.m_sValue) + 1;
				m_sValue = new char[len];
				V_strncpy( m_sValue, src.m_sValue, len );
			}
			break;
		case TYPE_INT:
			{
				m_iValue = src.m_iValue;
				V_snprintf( buf,sizeof(buf), "%d", m_iValue );
				int len = V_strlen(buf) + 1;
				m_sValue = new char[len];
				V_strncpy( m_sValue, buf, len  );
			}
			break;
		case TYPE_FLOAT:
			{
				m_flValue = src.m_flValue;
				V_snprintf( buf,sizeof(buf), "%f", m_flValue );
				int len = V_strlen(buf) + 1;
				m_sValue = new char[len];
				V_strncpy( m_sValue, buf, len );
			}
			break;
		case TYPE_PTR:
			{
				m_pValue = src.m_pValue;
			}
			break;
		case TYPE_UINT64:
			{
				m_sValue = new char[sizeof(uint64)];
				V_memcpy( m_sValue, src.m_sValue, sizeof(uint64) );
			}
			break;
		case TYPE_COLOR:
			{
				m_Color[0] = src.m_Color[0];
				m_Color[1] = src.m_Color[1];
				m_Color[2] = src.m_Color[2];
				m_Color[3] = src.m_Color[3];
			}
			break;
			
		default:
			{
				// do nothing . .what the heck is this?
				Assert( 0 );
			}
			break;
		}

	}
#if 0
	KeyValues *pDst = this;
	for ( KeyValues *pSrc = src.m_pSub; pSrc; pSrc = pSrc->m_pPeer )
	{
		if ( pSrc->m_pSub )
		{
			pDst->m_pSub = new KeyValues( pSrc->m_pSub->getName() );
			pDst->m_pSub->RecursiveCopyKeyValues( *pSrc->m_pSub );
		}
		else
		{
			// copy non-empty keys
			if ( pSrc->m_sValue && *(pSrc->m_sValue) )
			{
				pDst->m_pPeer = new KeyValues( 
			}
		}
	}
#endif

	// Handle the immediate child
	if( src.m_pSub )
	{
		m_pSub = new KeyValues( NULL );
		m_pSub->RecursiveCopyKeyValues( *src.m_pSub );
	}

	// Handle the immediate peer
	if( src.m_pPeer )
	{
		m_pPeer = new KeyValues( NULL );
		m_pPeer->RecursiveCopyKeyValues( *src.m_pPeer );
	}
}

KeyValues& KeyValues::operator=( KeyValues& src )
{
	RemoveEverything();
	Init();	// reset all values
	RecursiveCopyKeyValues( src );
	return *this;
}


//-----------------------------------------------------------------------------
// Make a new copy of all subkeys, add them all to the passed-in keyvalues
//-----------------------------------------------------------------------------
void KeyValues::CopySubkeys( KeyValues *pParent ) const
{
	// recursively copy subkeys
	// Also maintain ordering....
	KeyValues *pPrev = NULL;
	for ( KeyValues *sub = m_pSub; sub != NULL; sub = sub->m_pPeer )
	{
		// take a copy of the subkey
		KeyValues *dat = sub->MakeCopy();
		 
		// add into subkey list
		if (pPrev)
		{
			pPrev->m_pPeer = dat;
		}
		else
		{
			pParent->m_pSub = dat;
		}
		dat->m_pPeer = NULL;
		pPrev = dat;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Makes a copy of the whole key-value pair set
//-----------------------------------------------------------------------------
KeyValues *KeyValues::MakeCopy( void ) const
{
	KeyValues *newKeyValue = new KeyValues(GetName());

	// copy data
	newKeyValue->m_iDataType = m_iDataType;
	switch ( m_iDataType )
	{
	case TYPE_STRING:
		{
			if ( m_sValue )
			{
				int len = V_strlen( m_sValue );
				Assert( !newKeyValue->m_sValue );
				newKeyValue->m_sValue = new char[len + 1];
				V_memcpy( newKeyValue->m_sValue, m_sValue, len+1 );
			}
		}
		break;
	case TYPE_WSTRING:
		{
			if ( m_wsValue )
			{
				int len = wcslen( m_wsValue );
				newKeyValue->m_wsValue = new wchar_t[len+1];
				V_memcpy( newKeyValue->m_wsValue, m_wsValue, (len+1)*sizeof(wchar_t));
			}
		}
		break;

	case TYPE_INT:
		newKeyValue->m_iValue = m_iValue;
		break;

	case TYPE_FLOAT:
		newKeyValue->m_flValue = m_flValue;
		break;

	case TYPE_PTR:
		newKeyValue->m_pValue = m_pValue;
		break;
		
	case TYPE_COLOR:
		newKeyValue->m_Color[0] = m_Color[0];
		newKeyValue->m_Color[1] = m_Color[1];
		newKeyValue->m_Color[2] = m_Color[2];
		newKeyValue->m_Color[3] = m_Color[3];
		break;

	case TYPE_UINT64:
		newKeyValue->m_sValue = new char[sizeof(uint64)];
		V_memcpy( newKeyValue->m_sValue, m_sValue, sizeof(uint64) );
		break;
	};

	// recursively copy subkeys
	CopySubkeys( newKeyValue );
	return newKeyValue;
}


//-----------------------------------------------------------------------------
// Purpose: Check if a keyName has no value assigned to it.
//-----------------------------------------------------------------------------
bool KeyValues::IsEmpty(const char *keyName)
{
	KeyValues *dat = FindKey(keyName, false);
	if (!dat)
		return true;

	if (dat->m_iDataType == TYPE_NONE && dat->m_pSub == NULL)
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Clear out all subkeys, and the current value
//-----------------------------------------------------------------------------
void KeyValues::Clear( void )
{
	delete m_pSub;
	m_pSub = NULL;
	m_iDataType = TYPE_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: Get the data type of the value stored in a keyName
//-----------------------------------------------------------------------------
KeyValues::types_t KeyValues::GetDataType(const char *keyName)
{
	KeyValues *dat = FindKey(keyName, false);
	if (dat)
		return (types_t)dat->m_iDataType;

	return TYPE_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: Deletion, ensures object gets deleted from correct heap
//-----------------------------------------------------------------------------
void KeyValues::deleteThis()
{
	delete this;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : includedKeys - 
//-----------------------------------------------------------------------------
void KeyValues::AppendIncludedKeys( CUtlVector< KeyValues * >& includedKeys )
{
	// Append any included keys, too...
	int includeCount = includedKeys.Count();
	int i;
	for ( i = 0; i < includeCount; i++ )
	{
		KeyValues *kv = includedKeys[ i ];
		Assert( kv );

		KeyValues *insertSpot = this;
		while ( insertSpot->GetNextKey() )
		{
			insertSpot = insertSpot->GetNextKey();
		}

		insertSpot->SetNextKey( kv );
	}
}

void KeyValues::ParseIncludedKeys( char const *resourceName, const char *filetoinclude, 
		IBaseFileSystem* pFileSystem, const char *pPathID, CUtlVector< KeyValues * >& includedKeys, GetSymbolProc_t pfnEvaluateSymbolProc )
{
	Assert( resourceName );
	Assert( filetoinclude );
	Assert( pFileSystem );
	
	// Load it...
	if ( !pFileSystem )
	{
		return;
	}

	// Get relative subdirectory
	char fullpath[ 512 ];
	V_strncpy( fullpath, resourceName, sizeof( fullpath ) );

	// Strip off characters back to start or first /
	bool done = false;
	int len = V_strlen( fullpath );
	while ( !done )
	{
		if ( len <= 0 )
		{
			break;
		}
		
		if ( fullpath[ len - 1 ] == '\\' || 
			 fullpath[ len - 1 ] == '/' )
		{
			break;
		}

		// zero it
		fullpath[ len - 1 ] = 0;
		--len;
	}

	// Append included file
	V_strncat( fullpath, filetoinclude, sizeof( fullpath ), COPY_ALL_CHARACTERS );

	KeyValues *newKV = new KeyValues( fullpath );

	// CUtlSymbol save = s_CurrentFileSymbol;	// did that had any use ???

	newKV->UsesEscapeSequences( m_bHasEscapeSequences != 0 );	// use same format as parent

	if ( newKV->LoadFromFile( pFileSystem, fullpath, pPathID, pfnEvaluateSymbolProc ) )
	{
		includedKeys.AddToTail( newKV );
	}
	else
	{
		DevMsg( "KeyValues::ParseIncludedKeys: Couldn't load included keyvalue file %s\n", fullpath );
		newKV->deleteThis();
	}

	// s_CurrentFileSymbol = save;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : baseKeys - 
//-----------------------------------------------------------------------------
void KeyValues::MergeBaseKeys( CUtlVector< KeyValues * >& baseKeys )
{
	int includeCount = baseKeys.Count();
	int i;
	for ( i = 0; i < includeCount; i++ )
	{
		KeyValues *kv = baseKeys[ i ];
		Assert( kv );

		RecursiveMergeKeyValues( kv );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : baseKV - keyvalues we're basing ourselves on
//-----------------------------------------------------------------------------
void KeyValues::RecursiveMergeKeyValues( KeyValues *baseKV )
{
	// Merge ourselves
	// we always want to keep our value, so nothing to do here

	// Now merge our children
	for ( KeyValues *baseChild = baseKV->m_pSub; baseChild != NULL; baseChild = baseChild->m_pPeer )
	{
		// for each child in base, see if we have a matching kv

		bool bFoundMatch = false;

		// If we have a child by the same name, merge those keys
		for ( KeyValues *newChild = m_pSub; newChild != NULL; newChild = newChild->m_pPeer )
		{
			if ( !V_strcmp( baseChild->GetName(), newChild->GetName() ) )
			{
				newChild->RecursiveMergeKeyValues( baseChild );
				bFoundMatch = true;
				break;
			}	
		}

		// If not merged, append this key
		if ( !bFoundMatch )
		{
			KeyValues *dat = baseChild->MakeCopy();
			Assert( dat );
			AddSubKey( dat );
		}
	}
}

//-----------------------------------------------------------------------------
// Returns whether a keyvalues conditional expression string evaluates to true or false
//-----------------------------------------------------------------------------
bool KeyValues::EvaluateConditional( const char *pExpressionString, GetSymbolProc_t pfnEvaluateSymbolProc )
{
	// evaluate the infix expression, calling the symbol proc to resolve each symbol's value
	bool bResult = false;
	bool bValid = g_ExpressionEvaluator.Evaluate( bResult, pExpressionString, pfnEvaluateSymbolProc );
	if ( !bValid )
	{
		g_KeyValuesErrorStack.ReportError( "KV Conditional Evaluation Error" );
	}

	return bResult;
}

// prevent two threads from entering this at the same time and trying to share the global error reporting and parse buffers
static CThreadFastMutex g_KVMutex;
//-----------------------------------------------------------------------------
// Read from a buffer...
//-----------------------------------------------------------------------------
bool KeyValues::LoadFromBuffer( char const *resourceName, CUtlBuffer &buf, IBaseFileSystem* pFileSystem, const char *pPathID, GetSymbolProc_t pfnEvaluateSymbolProc )
{
	AUTO_LOCK_FM( g_KVMutex );

	if ( IsGameConsole() )
	{
		// Let's not crash if the buffer is empty
		unsigned char *pData = buf.Size() > 0 ? (unsigned char *)buf.PeekGet() : NULL;
		if ( pData && (unsigned int)pData[0] == KV_BINARY_POOLED_FORMAT )
		{
			// skip past binary marker
			buf.GetUnsignedChar();
			// get the pool identifier, allows the fs to bind the expected string pool
			unsigned int poolKey = buf.GetUnsignedInt();
	
			RemoveEverything();
			Init();

			return ReadAsBinaryPooledFormat( buf, pFileSystem, poolKey, pfnEvaluateSymbolProc );
		}
	}

	KeyValues *pPreviousKey = NULL;
	KeyValues *pCurrentKey = this;
	CUtlVector< KeyValues * > includedKeys;
	CUtlVector< KeyValues * > baseKeys;
	bool wasQuoted;
	bool wasConditional;
	g_KeyValuesErrorStack.SetFilename( resourceName );	
	do 
	{
		bool bAccepted = true;

		// the first thing must be a key
		const char *s = ReadToken( buf, wasQuoted, wasConditional );
		if ( !buf.IsValid() || !s )
			break;

		if ( !wasQuoted && *s == '\0' )
		{
			// non quoted empty strings stop parsing
			// quoted empty strings are allowed to support unnnamed KV sections
			break;
		}

		if ( !V_stricmp( s, "#include" ) )	// special include macro (not a key name)
		{
			s = ReadToken( buf, wasQuoted, wasConditional );
			// Name of subfile to load is now in s

			if ( !s || *s == 0 )
			{
				g_KeyValuesErrorStack.ReportError("#include is NULL " );
			}
			else
			{
				ParseIncludedKeys( resourceName, s, pFileSystem, pPathID, includedKeys, pfnEvaluateSymbolProc );
			}

			continue;
		}
		else if ( !V_stricmp( s, "#base" ) )
		{
			s = ReadToken( buf, wasQuoted, wasConditional );
			// Name of subfile to load is now in s

			if ( !s || *s == 0 )
			{
				g_KeyValuesErrorStack.ReportError("#base is NULL " );
			}
			else
			{
				ParseIncludedKeys( resourceName, s, pFileSystem, pPathID, baseKeys, pfnEvaluateSymbolProc );
			}

			continue;
		}

		if ( !pCurrentKey )
		{
			pCurrentKey = new KeyValues( s );
			Assert( pCurrentKey );

			pCurrentKey->UsesEscapeSequences( m_bHasEscapeSequences != 0 ); // same format has parent use

			if ( pPreviousKey )
			{
				pPreviousKey->SetNextKey( pCurrentKey );
			}
		}
		else
		{
			pCurrentKey->SetName( s );
		}

		// get the '{'
		s = ReadToken( buf, wasQuoted, wasConditional );

		if ( wasConditional )
		{
			bAccepted = EvaluateConditional( s, pfnEvaluateSymbolProc );

			// Now get the '{'
			s = ReadToken( buf, wasQuoted, wasConditional );
		}

		if ( s && *s == '{' && !wasQuoted )
		{
			// header is valid so load the file
			pCurrentKey->RecursiveLoadFromBuffer( resourceName, buf, pfnEvaluateSymbolProc );
		}
		else
		{
			g_KeyValuesErrorStack.ReportError("LoadFromBuffer: missing {" );
		}

		if ( !bAccepted )
		{
			if ( pPreviousKey )
			{
				pPreviousKey->SetNextKey( NULL );
			}
			pCurrentKey->Clear();
		}
		else
		{
			pPreviousKey = pCurrentKey;
			pCurrentKey = NULL;
		}
	} while ( buf.IsValid() );

	AppendIncludedKeys( includedKeys );
	{
		// delete included keys!
		int i;
		for ( i = includedKeys.Count() - 1; i > 0; i-- )
		{
			KeyValues *kv = includedKeys[ i ];
			kv->deleteThis();
		}
	}

	MergeBaseKeys( baseKeys );
	{
		// delete base keys!
		int i;
		for ( i = baseKeys.Count() - 1; i >= 0; i-- )
		{
			KeyValues *kv = baseKeys[ i ];
			kv->deleteThis();
		}
	}

	g_KeyValuesErrorStack.SetFilename( "" );	

	return true;
}


//-----------------------------------------------------------------------------
// Read from a buffer...
//-----------------------------------------------------------------------------
bool KeyValues::LoadFromBuffer( char const *resourceName, const char *pBuffer, IBaseFileSystem* pFileSystem, const char *pPathID, GetSymbolProc_t pfnEvaluateSymbolProc )
{
	if ( !pBuffer )
		return true;

	if ( IsGameConsole() && (unsigned int)((unsigned char *)pBuffer)[0] == KV_BINARY_POOLED_FORMAT )
	{
		// bad, got a binary compiled KV file through an unexpected text path
		// not all paths support binary compiled kv, needs to get fixed
		// need to have caller supply buffer length (strlen not valid), this interface change was never plumbed
		Warning( "ERROR! Binary compiled KV '%s' in an unexpected handler\n", resourceName );
		Assert( 0 );
		return false;
	}

	int nLen = V_strlen( pBuffer );
	CUtlBuffer buf( pBuffer, nLen, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );
	// Translate Unicode files into UTF-8 before proceeding
	if ( nLen > 2 && (uint8)pBuffer[0] == 0xFF && (uint8)pBuffer[1] == 0xFE )
	{
		int nUTF8Len = V_UnicodeToUTF8( (wchar_t*)(pBuffer+2), NULL, 0 );
		char *pUTF8Buf = new char[nUTF8Len];
		V_UnicodeToUTF8( (wchar_t*)(pBuffer+2), pUTF8Buf, nUTF8Len );
		buf.AssumeMemory( pUTF8Buf, nUTF8Len, nUTF8Len, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );
	}
	return LoadFromBuffer( resourceName, buf, pFileSystem, pPathID, pfnEvaluateSymbolProc );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void KeyValues::RecursiveLoadFromBuffer( char const *resourceName, CUtlBuffer &buf, GetSymbolProc_t pfnEvaluateSymbolProc )
{
	CKeyErrorContext errorReport( GetNameSymbolCaseSensitive() );
	bool wasQuoted;
	bool wasConditional;
	// keep this out of the stack until a key is parsed
	CKeyErrorContext errorKey( INVALID_KEY_SYMBOL );
	while ( 1 )
	{
		bool bAccepted = true;

		// get the key name
		const char * name = ReadToken( buf, wasQuoted, wasConditional );

		if ( !name )	// EOF stop reading
		{
			g_KeyValuesErrorStack.ReportError("RecursiveLoadFromBuffer:  got EOF instead of keyname" );
			break;
		}

		if ( !*name ) // empty token, maybe "" or EOF
		{
			g_KeyValuesErrorStack.ReportError("RecursiveLoadFromBuffer:  got empty keyname" );
			break;
		}

		if ( *name == '}' && !wasQuoted )	// top level closed, stop reading
			break;

		// Always create the key; note that this could potentially
		// cause some duplication, but that's what we want sometimes
		KeyValues *dat = CreateKey( name );

		errorKey.Reset( dat->GetNameSymbolCaseSensitive() );

		// get the value
		const char * value = ReadToken( buf, wasQuoted, wasConditional );

		if ( wasConditional && value )
		{
			bAccepted = EvaluateConditional( value, pfnEvaluateSymbolProc );

			// get the real value
			value = ReadToken( buf, wasQuoted, wasConditional );
		}

		if ( !value )
		{
			g_KeyValuesErrorStack.ReportError("RecursiveLoadFromBuffer:  got NULL key" );
			break;
		}
		
		if ( *value == '}' && !wasQuoted )
		{
			g_KeyValuesErrorStack.ReportError("RecursiveLoadFromBuffer:  got } in key" );
			break;
		}

		if ( *value == '{' && !wasQuoted )
		{
			// this isn't a key, it's a section
			errorKey.Reset( INVALID_KEY_SYMBOL );
			// sub value list
			dat->RecursiveLoadFromBuffer( resourceName, buf, pfnEvaluateSymbolProc );
		}
		else 
		{
			if ( wasConditional )
			{
				g_KeyValuesErrorStack.ReportError("RecursiveLoadFromBuffer:  got conditional between key and value" );
				break;
			}
			
			if (dat->m_sValue)
			{
				delete[] dat->m_sValue;
				dat->m_sValue = NULL;
			}

			int len = V_strlen( value );

			// Here, let's determine if we got a float or an int....
			char* pIEnd;	// pos where int scan ended
			char* pFEnd;	// pos where float scan ended
			const char* pSEnd = value + len ; // pos where token ends

			int ival = strtol( value, &pIEnd, 10 );
			float fval = (float)strtod( value, &pFEnd );
			bool bOverflow = ( ival == LONG_MAX || ival == LONG_MIN ) && errno == ERANGE;
#ifdef POSIX
			// strtod supports hex representation in strings under posix but we DON'T
			// want that support in keyvalues, so undo it here if needed
			if ( len > 1 &&  tolower(value[1]) == 'x' )
			{
				fval = 0.0f;
				pFEnd = (char *)value;
			}
#endif
				
			if ( *value == 0 )
			{
				dat->m_iDataType = TYPE_STRING;	
			}
			else if ( ( 18 == len ) && ( value[0] == '0' ) && ( value[1] == 'x' ) )
			{
				// an 18-byte value prefixed with "0x" (followed by 16 hex digits) is an int64 value
				int64 retVal = 0;
				for( int i=2; i < 2 + 16; i++ )
				{
					char digit = value[i];
					if ( digit >= 'a' ) 
						digit -= 'a' - ( '9' + 1 );
					else
						if ( digit >= 'A' )
							digit -= 'A' - ( '9' + 1 );
					retVal = ( retVal * 16 ) + ( digit - '0' );
				}
				dat->m_sValue = new char[sizeof(uint64)];
				*((uint64 *)dat->m_sValue) = retVal;
				dat->m_iDataType = TYPE_UINT64;
			}
			else if ( (pFEnd > pIEnd) && (pFEnd == pSEnd) )
			{
				dat->m_flValue = fval; 
				dat->m_iDataType = TYPE_FLOAT;
			}
			else if (pIEnd == pSEnd && !bOverflow)
			{
				dat->m_iValue = ival; 
				dat->m_iDataType = TYPE_INT;
			}
			else
			{
				dat->m_iDataType = TYPE_STRING;
			}

			if (dat->m_iDataType == TYPE_STRING)
			{
				// copy in the string information
				dat->m_sValue = new char[len+1];
				V_memcpy( dat->m_sValue, value, len+1 );
			}

			// Look ahead one token for a conditional tag
			int prevPos = buf.TellGet();
			const char *peek = ReadToken( buf, wasQuoted, wasConditional );
			if ( wasConditional )
			{
				bAccepted = EvaluateConditional( peek, pfnEvaluateSymbolProc );
			}
			else
			{
				buf.SeekGet( CUtlBuffer::SEEK_HEAD, prevPos );
			}
		}

		if ( !bAccepted )
		{
			this->RemoveSubKey( dat );
			dat->deleteThis();
			dat = NULL;
		}
	}
}

// writes KeyValue as binary data to buffer
bool KeyValues::WriteAsBinary( CUtlBuffer &buffer ) const
{
	if ( buffer.IsText() ) // must be a binary buffer
		return false;

	if ( !buffer.IsValid() ) // must be valid, no overflows etc
		return false;

	// Write subkeys:
	
	// loop through all our peers
	for ( const KeyValues *dat = this; dat != NULL; dat = dat->m_pPeer )
	{
		// write type
		buffer.PutUnsignedChar( dat->m_iDataType );

		// write name
		buffer.PutString( dat->GetName() );

		// write type
		switch (dat->m_iDataType)
		{
		case TYPE_NONE:
			{
				dat->m_pSub->WriteAsBinary( buffer );
				break;
			}
		case TYPE_STRING:
			{
				if (dat->m_sValue && *(dat->m_sValue))
				{
					buffer.PutString( dat->m_sValue );
				}
				else
				{
					buffer.PutString( "" );
				}
				break;
			}
		case TYPE_WSTRING:
			{
				int nLength = dat->m_wsValue ? V_wcslen( dat->m_wsValue ) : 0;
				buffer.PutShort( nLength );
				for( int k = 0; k < nLength; ++ k )
				{
					buffer.PutShort( ( unsigned short ) dat->m_wsValue[k] );
				}
				break;
			}

		case TYPE_INT:
			{
				buffer.PutInt( dat->m_iValue );				
				break;
			}

		case TYPE_UINT64:
			{
				buffer.PutInt64( *((int64 *)dat->m_sValue) );
				break;
			}

		case TYPE_FLOAT:
			{
				buffer.PutFloat( dat->m_flValue );
				break;
			}
		case TYPE_COLOR:
			{
				buffer.PutUnsignedChar( dat->m_Color[0] );
				buffer.PutUnsignedChar( dat->m_Color[1] );
				buffer.PutUnsignedChar( dat->m_Color[2] );
				buffer.PutUnsignedChar( dat->m_Color[3] );
				break;
			}
		case TYPE_PTR:
			{
				buffer.PutUnsignedInt( (int)dat->m_pValue );
				break;
			}

		default:
			break;
		}
	}

	// write tail, marks end of peers
	buffer.PutUnsignedChar( TYPE_NUMTYPES ); 

	return buffer.IsValid();
}

// read KeyValues from binary buffer, returns true if parsing was successful
bool KeyValues::ReadAsBinary( CUtlBuffer &buffer )
{
	if ( buffer.IsText() ) // must be a binary buffer
		return false;

	if ( !buffer.IsValid() ) // must be valid, no overflows etc
		return false;

	RemoveEverything(); // remove current content
	Init();	// reset
	
	char		token[KEYVALUES_TOKEN_SIZE];
	KeyValues	*dat = this;
	types_t		type = (types_t)buffer.GetUnsignedChar();
	
	// loop through all our peers
	while ( true )
	{
		if ( type == TYPE_NUMTYPES )
			break; // no more peers

		dat->m_iDataType = type;

		buffer.GetString( token, KEYVALUES_TOKEN_SIZE-1 );
		token[KEYVALUES_TOKEN_SIZE-1] = 0;

		dat->SetName( token );
		
		switch ( type )
		{
		case TYPE_NONE:
			{
				dat->m_pSub = new KeyValues("");
				dat->m_pSub->ReadAsBinary( buffer );
				break;
			}
		case TYPE_STRING:
			{
				buffer.GetString( token, KEYVALUES_TOKEN_SIZE-1 );
				token[KEYVALUES_TOKEN_SIZE-1] = 0;

				int len = V_strlen( token );
				dat->m_sValue = new char[len + 1];
				V_memcpy( dat->m_sValue, token, len+1 );
								
				break;
			}
		case TYPE_WSTRING:
			{
				int nLength = buffer.GetShort();
				
				dat->m_wsValue = new wchar_t[nLength + 1];
				
				for( int k = 0; k < nLength; ++ k )
				{
					dat->m_wsValue[k] = buffer.GetShort();
				}
				dat->m_wsValue[ nLength ] = 0;
				break;
			}

		case TYPE_INT:
			{
				dat->m_iValue = buffer.GetInt();
				break;
			}

		case TYPE_UINT64:
			{
				dat->m_sValue = new char[sizeof(uint64)];
				*((uint64 *)dat->m_sValue) = buffer.GetInt64();
				break;
			}

		case TYPE_FLOAT:
			{
				dat->m_flValue = buffer.GetFloat();
				break;
			}
		case TYPE_COLOR:
			{
				dat->m_Color[0] = buffer.GetUnsignedChar();
				dat->m_Color[1] = buffer.GetUnsignedChar();
				dat->m_Color[2] = buffer.GetUnsignedChar();
				dat->m_Color[3] = buffer.GetUnsignedChar();
				break;
			}
		case TYPE_PTR:
			{
				dat->m_pValue = (void*)buffer.GetUnsignedInt();
				break;
			}

		default:
			break;
		}

		if ( !buffer.IsValid() ) // error occured
			return false;

		type = (types_t)buffer.GetUnsignedChar();

		if ( type == TYPE_NUMTYPES )
			break;

		// new peer follows
		dat->m_pPeer = new KeyValues("");
		dat = dat->m_pPeer;
	}

	return buffer.IsValid();
}

//-----------------------------------------------------------------------------
// Alternate dense binary format that pools all the strings, the xbox supports
// this during creation of each mod dir's zip processing of kv files.
//-----------------------------------------------------------------------------
bool KeyValues::ReadAsBinaryPooledFormat( CUtlBuffer &buffer, IBaseFileSystem *pFileSystem, unsigned int poolKey, GetSymbolProc_t pfnEvaluateSymbolProc )
{
	// xbox only support
	if ( !IsGameConsole() )
	{
		Assert( 0 );
		return false;
	}

	if ( buffer.IsText() ) // must be a binary buffer
		return false;

	if ( !buffer.IsValid() ) // must be valid, no overflows etc
		return false;
	
	char token[KEYVALUES_TOKEN_SIZE];
	KeyValues *dat = this;
	types_t type = (types_t)buffer.GetUnsignedChar();
	
	// loop through all our peers
	while ( true )
	{
		if ( type == TYPE_NUMTYPES )
			break; // no more peers

		dat->m_iDataType = type;

		unsigned int stringKey = buffer.GetUnsignedInt();
		if ( !((IFileSystem*)pFileSystem)->GetStringFromKVPool( poolKey, stringKey, token, sizeof( token ) ) )
			return false;
		dat->SetName( token );
		
		switch ( type )
		{
		case TYPE_NONE:
			{
				dat->m_pSub = new KeyValues( "" );
				if ( !dat->m_pSub->ReadAsBinaryPooledFormat( buffer, pFileSystem, poolKey, pfnEvaluateSymbolProc ) )
					return false;
				break;
			}

		case TYPE_STRING:
			{
				unsigned int stringKey = buffer.GetUnsignedInt();
				if ( !((IFileSystem*)pFileSystem)->GetStringFromKVPool( poolKey, stringKey, token, sizeof( token ) ) )
					return false;
				int len = V_strlen( token );
				dat->m_sValue = new char[len + 1];
				V_memcpy( dat->m_sValue, token, len+1 );			
				break;
			}

		case TYPE_WSTRING:
			{
				int nLength = buffer.GetShort();
				dat->m_wsValue = new wchar_t[nLength + 1];
				for ( int k = 0; k < nLength; ++k )
				{
					dat->m_wsValue[k] = buffer.GetShort();
				}
				dat->m_wsValue[nLength] = 0;
				break;
			}

		case TYPE_INT:
			{
				dat->m_iValue = buffer.GetInt();
				break;
			}

		case TYPE_UINT64:
			{
				dat->m_sValue = new char[sizeof(uint64)];
				*((uint64 *)dat->m_sValue) = buffer.GetInt64();
				break;
			}

		case TYPE_FLOAT:
			{
				dat->m_flValue = buffer.GetFloat();
				break;
			}

		case TYPE_COLOR:
			{
				dat->m_Color[0] = buffer.GetUnsignedChar();
				dat->m_Color[1] = buffer.GetUnsignedChar();
				dat->m_Color[2] = buffer.GetUnsignedChar();
				dat->m_Color[3] = buffer.GetUnsignedChar();
				break;
			}

		case TYPE_PTR:
			{
				dat->m_pValue = (void*)buffer.GetUnsignedInt();
				break;
			}

		case TYPE_COMPILED_INT_0:
			{
				// only for dense storage purposes, flip back to preferred internal format
				dat->m_iDataType = TYPE_INT;
				dat->m_iValue = 0;
				break;
			}

		case TYPE_COMPILED_INT_1:
			{
				// only for dense storage purposes, flip back to preferred internal format
				dat->m_iDataType = TYPE_INT;
				dat->m_iValue = 1;
				break;
			}

		case TYPE_COMPILED_INT_BYTE:
			{
				// only for dense storage purposes, flip back to preferred internal format
				dat->m_iDataType = TYPE_INT;
				dat->m_iValue = buffer.GetChar();
				break;
			}

		default:
			break;
		}

		if ( !buffer.IsValid() ) // error occured
			return false;

		if ( !buffer.GetBytesRemaining() )
			break;

		type = (types_t)buffer.GetUnsignedChar();
		if ( type == TYPE_NUMTYPES )
			break;

		// new peer follows
		dat->m_pPeer = new KeyValues("");
		dat = dat->m_pPeer;
	}

	return buffer.IsValid();
}

#include "tier0/memdbgoff.h"

//-----------------------------------------------------------------------------
// Purpose: memory allocator
//-----------------------------------------------------------------------------
void *KeyValues::operator new( size_t iAllocSize )
{
	MEM_ALLOC_CREDIT();
	return KeyValuesSystem()->AllocKeyValuesMemory(iAllocSize);
}

void *KeyValues::operator new( size_t iAllocSize, int nBlockUse, const char *pFileName, int nLine )
{
	MemAlloc_PushAllocDbgInfo( pFileName, nLine );
	void *p = KeyValuesSystem()->AllocKeyValuesMemory(iAllocSize);
	MemAlloc_PopAllocDbgInfo();
	return p;
}

#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: deallocator
//-----------------------------------------------------------------------------
void KeyValues::operator delete( void *pMem )
{
	KeyValuesSystem()->FreeKeyValuesMemory(pMem);
}

void KeyValues::operator delete( void *pMem, int nBlockUse, const char *pFileName, int nLine )
{
	KeyValuesSystem()->FreeKeyValuesMemory(pMem);
}

void KeyValues::UnpackIntoStructure( KeyValuesUnpackStructure const *pUnpackTable, void *pDest )
{
	uint8 *dest = ( uint8 * ) pDest;
	while( pUnpackTable->m_pKeyName )
	{
		uint8 * dest_field = dest + pUnpackTable->m_nFieldOffset;
		KeyValues * find_it = FindKey( pUnpackTable->m_pKeyName );
		switch( pUnpackTable->m_eDataType )
		{
			case UNPACK_TYPE_FLOAT:
			{
				float default_value = ( pUnpackTable->m_pKeyDefault ) ? atof( pUnpackTable->m_pKeyDefault ) : 0.0;
				*( ( float * ) dest_field ) = GetFloat( pUnpackTable->m_pKeyName, default_value );
				break;
			}
			break;

			case UNPACK_TYPE_VECTOR:
			{
				Vector *dest_v = ( Vector * ) dest_field;
				char const *src_string =
					GetString( pUnpackTable->m_pKeyName, pUnpackTable->m_pKeyDefault );
				if ( ( ! src_string ) ||
					 ( sscanf(src_string,"%f %f %f",
							  & ( dest_v->x ), & ( dest_v->y ), & ( dest_v->z )) != 3 ))
					dest_v->Init( 0, 0, 0 );
			}
			break;

			case UNPACK_TYPE_FOUR_FLOATS:
			{
				float *dest_f = ( float * ) dest_field;
				char const *src_string =
					GetString( pUnpackTable->m_pKeyName, pUnpackTable->m_pKeyDefault );
				if ( ( ! src_string ) ||
					 ( sscanf(src_string,"%f %f %f %f",
							  dest_f, dest_f + 1, dest_f + 2, dest_f + 3 )) != 4 )
					memset( dest_f, 0, 4 * sizeof( float ) );
			}
			break;

			case UNPACK_TYPE_TWO_FLOATS:
			{
				float *dest_f = ( float * ) dest_field;
				char const *src_string =
					GetString( pUnpackTable->m_pKeyName, pUnpackTable->m_pKeyDefault );
				if ( ( ! src_string ) ||
					 ( sscanf(src_string,"%f %f",
							  dest_f, dest_f + 1 )) != 2 )
					memset( dest_f, 0, 2 * sizeof( float ) );
			}
			break;

			case UNPACK_TYPE_STRING:
			{
				char *dest_s = ( char * ) dest_field;
				char const *pDefault = "";
				if ( pUnpackTable->m_pKeyDefault )
				{
					pDefault = pUnpackTable->m_pKeyDefault;
				}
				strncpy( dest_s,
						 GetString( pUnpackTable->m_pKeyName, pDefault ),
						 pUnpackTable->m_nFieldSize );
			}
			break;

			case UNPACK_TYPE_INT:
			{
				int *dest_i = ( int * ) dest_field;
				int default_int = 0;
				if ( pUnpackTable->m_pKeyDefault )
					default_int = atoi( pUnpackTable->m_pKeyDefault );
				*( dest_i ) = GetInt( pUnpackTable->m_pKeyName, default_int );
			}
			break;

			case UNPACK_TYPE_VECTOR_COLOR:
			{
				Vector *dest_v = ( Vector * ) dest_field;
				if ( find_it )
				{
					Color c = GetColor( pUnpackTable->m_pKeyName );
					dest_v->x = c.r();
					dest_v->y = c.g();
					dest_v->z = c.b();
				}
				else
				{
					if ( pUnpackTable->m_pKeyDefault )
						sscanf(pUnpackTable->m_pKeyDefault,"%f %f %f",
							   & ( dest_v->x ), & ( dest_v->y ), & ( dest_v->z ));
					else
						dest_v->Init( 0, 0, 0 );
				}
				*( dest_v ) *= ( 1.0 / 255 );
			}
		}
		pUnpackTable++;
	}
}

//-----------------------------------------------------------------------------
// Helper function for processing a keyvalue tree for console resolution support.
// Alters key/values for easier console video resolution support. 
// If running SD (640x480), the presence of "???_lodef" creates or slams "???".
// If running HD (1280x720), the presence of "???_hidef" creates or slams "???".
//-----------------------------------------------------------------------------
bool KeyValues::ProcessResolutionKeys( const char *pResString )
{	
	if ( !pResString )
	{
		// not for pc, console only
		return false;
	}

	KeyValues *pSubKey = GetFirstSubKey();
	if ( !pSubKey )
	{
		// not a block
		return false;
	}

	for ( ; pSubKey != NULL; pSubKey = pSubKey->GetNextKey() )
	{
		// recursively descend each sub block
		pSubKey->ProcessResolutionKeys( pResString );

		// check to see if our substring is present
		if ( V_stristr( pSubKey->GetName(), pResString ) != NULL )
		{
			char normalKeyName[128];
			V_strncpy( normalKeyName, pSubKey->GetName(), sizeof( normalKeyName ) );

			// substring must match exactly, otherwise keys like "_lodef" and "_lodef_wide" would clash.
			char *pString = V_stristr( normalKeyName, pResString );
			if ( pString && !V_stricmp( pString, pResString ) )
			{
				*pString = '\0';

				// find and delete the original key (if any)
				KeyValues *pKey = FindKey( normalKeyName );
				if ( pKey )
				{		
					// remove the key
					RemoveSubKey( pKey );
					pKey->deleteThis();
				}

				// rename the marked key
				pSubKey->SetName( normalKeyName );
			}
		}
	}

	return true;
}

//
// KeyValues merge operations
//

void KeyValues::MergeFrom( KeyValues *kvMerge, MergeKeyValuesOp_t eOp /* = MERGE_KV_ALL */ )
{
	if ( !this || !kvMerge )
		return;

	switch ( eOp )
	{
	case MERGE_KV_ALL:
		MergeFrom( kvMerge->FindKey( "update" ), MERGE_KV_UPDATE );
		MergeFrom( kvMerge->FindKey( "delete" ), MERGE_KV_DELETE );
		MergeFrom( kvMerge->FindKey( "borrow" ), MERGE_KV_BORROW );
		return;

	case MERGE_KV_UPDATE:
		{
			for ( KeyValues *sub = kvMerge->GetFirstTrueSubKey(); sub; sub = sub->GetNextTrueSubKey() )
			{
				char const *szName = sub->GetName();

				KeyValues *subStorage = this->FindKey( szName, false );
				if ( !subStorage )
				{
					AddSubKey( sub->MakeCopy() );
				}
				else
				{
					subStorage->MergeFrom( sub, eOp );
				}
			}
			for ( KeyValues *val = kvMerge->GetFirstValue(); val; val = val->GetNextValue() )
			{
				char const *szName = val->GetName();

				if ( KeyValues *valStorage = this->FindKey( szName, false ) )
				{
					this->RemoveSubKey( valStorage );
					valStorage->deleteThis();
				}
				this->AddSubKey( val->MakeCopy() );
			}
		}
		return;

	case MERGE_KV_BORROW:
		{
			for ( KeyValues *sub = kvMerge->GetFirstTrueSubKey(); sub; sub = sub->GetNextTrueSubKey() )
			{
				char const *szName = sub->GetName();

				KeyValues *subStorage = this->FindKey( szName, false );
				if ( !subStorage )
					continue;

				subStorage->MergeFrom( sub, eOp );
			}
			for ( KeyValues *val = kvMerge->GetFirstValue(); val; val = val->GetNextValue() )
			{
				char const *szName = val->GetName();

				if ( KeyValues *valStorage = this->FindKey( szName, false ) )
				{
					this->RemoveSubKey( valStorage );
					valStorage->deleteThis();
				}
				else
					continue;

				this->AddSubKey( val->MakeCopy() );
			}
		}
		return;

	case MERGE_KV_DELETE:
		{
			for ( KeyValues *sub = kvMerge->GetFirstTrueSubKey(); sub; sub = sub->GetNextTrueSubKey() )
			{
				char const *szName = sub->GetName();
				if ( KeyValues *subStorage = this->FindKey( szName, false ) )
				{
					subStorage->MergeFrom( sub, eOp );
				}
			}
			for ( KeyValues *val = kvMerge->GetFirstValue(); val; val = val->GetNextValue() )
			{
				char const *szName = val->GetName();

				if ( KeyValues *valStorage = this->FindKey( szName, false ) )
				{
					this->RemoveSubKey( valStorage );
					valStorage->deleteThis();
				}
			}
		}
		return;
	}
}


//
// KeyValues from string parsing
//

static char const * ParseStringToken( char const *szStringVal, char const **ppEndOfParse )
{
	// Eat whitespace
	while ( V_isspace( *szStringVal ) )
		++ szStringVal;

	char const *pszResult = szStringVal;

	while ( *szStringVal && !V_isspace( *szStringVal ) )
		++ szStringVal;

	if ( ppEndOfParse )
	{
		*ppEndOfParse = szStringVal;
	}

	return pszResult;
}

KeyValues * KeyValues::FromString( char const *szName, char const *szStringVal, char const **ppEndOfParse )
{
	if ( !szName )
		szName = "";

	if ( !szStringVal )
		szStringVal = "";

	KeyValues *kv = new KeyValues( szName );
	if ( !kv )
		return NULL;

	char chName[256] = {0};
	char chValue[256] = {0};

	for ( ; ; )
	{
		char const *szEnd;

		char const *szVarValue = NULL;
		char const *szVarName = ParseStringToken( szStringVal, &szEnd );
		if ( !*szVarName )
			break;
		if ( *szVarName == '}' )
		{
			szStringVal = szVarName + 1;
			break;
		}
		V_strncpy( chName, szVarName, MIN( sizeof( chName ), szEnd - szVarName + 1 ) );
		szVarName = chName;
		szStringVal = szEnd;

		if ( *szVarName == '{' )
		{
			szVarName = "";
			goto do_sub_key;
		}

		szVarValue = ParseStringToken( szStringVal, &szEnd );
		if ( *szVarValue == '}' )
		{
			szStringVal = szVarValue + 1;
			kv->SetString( szVarName, "" );
			break;
		}
		V_strncpy( chValue, szVarValue, MIN( sizeof( chValue ), szEnd - szVarValue + 1 ) );
		szVarValue = chValue;
		szStringVal = szEnd;

		if ( *szVarValue == '{' )
		{
			goto do_sub_key;
		}

		// Try to recognize some known types
		if ( char const *szInt = StringAfterPrefix( szVarValue, "#int#" ) )
		{
			kv->SetInt( szVarName, atoi( szInt ) );
		}
		else if ( !V_stricmp( szVarValue, "#empty#" ) )
		{
			kv->SetString( szVarName, "" );
		}
		else
		{
			kv->SetString( szVarName, szVarValue );
		}
		continue;

do_sub_key:
		{
			KeyValues *pSubKey = KeyValues::FromString( szVarName, szStringVal, &szEnd );
			if ( pSubKey )
			{
				kv->AddSubKey( pSubKey );
			}
			szStringVal = szEnd;
			continue;
		}
	}

	if ( ppEndOfParse )
	{
		*ppEndOfParse = szStringVal;
	}

	return kv;
}



//
// KeyValues dumping implementation
//
bool KeyValues::Dump( IKeyValuesDumpContext *pDump, int nIndentLevel /* = 0 */ )
{
	if ( !pDump->KvBeginKey( this, nIndentLevel ) )
		return false;
	
	// Dump values
	for ( KeyValues *val = this ? GetFirstValue() : NULL; val; val = val->GetNextValue() )
	{
		if ( !pDump->KvWriteValue( val, nIndentLevel + 1 ) )
			return false;
	}

	// Dump subkeys
	for ( KeyValues *sub = this ? GetFirstTrueSubKey() : NULL; sub; sub = sub->GetNextTrueSubKey() )
	{
		if ( !sub->Dump( pDump, nIndentLevel + 1 ) )
			return false;
	}

	return pDump->KvEndKey( this, nIndentLevel );
}

bool IKeyValuesDumpContextAsText::KvBeginKey( KeyValues *pKey, int nIndentLevel )
{
	if ( pKey )
	{
		return
			KvWriteIndent( nIndentLevel ) &&
			KvWriteText( pKey->GetName() ) &&
			KvWriteText( " {\n" );
	}
	else
	{
		return
			KvWriteIndent( nIndentLevel ) &&
			KvWriteText( "<< NULL >>\n" );
	}
}

bool IKeyValuesDumpContextAsText::KvWriteValue( KeyValues *val, int nIndentLevel )
{
	if ( !val )
	{
		return
			KvWriteIndent( nIndentLevel ) &&
			KvWriteText( "<< NULL >>\n" );
	}

	if ( !KvWriteIndent( nIndentLevel ) )
		return false;

	if ( !KvWriteText( val->GetName() ) )
		return false;

	if ( !KvWriteText( " " ) )
		return false;

	switch ( val->GetDataType() )
	{
	case KeyValues::TYPE_STRING:
		{
			if ( !KvWriteText( val->GetString() ) )
				return false;
		}
		break;

	case KeyValues::TYPE_INT:
		{
			int n = val->GetInt();
			char *chBuffer = ( char * ) stackalloc( 128 );
			V_snprintf( chBuffer, 128, "int( %d = 0x%X )", n, n );
			if ( !KvWriteText( chBuffer ) )
				return false;
		}
		break;
	
	case KeyValues::TYPE_FLOAT:
		{
			float fl = val->GetFloat();
			char *chBuffer = ( char * ) stackalloc( 128 );
			V_snprintf( chBuffer, 128, "float( %f )", fl );
			if ( !KvWriteText( chBuffer ) )
				return false;
		}
		break;

	case KeyValues::TYPE_PTR:
		{
			void *ptr = val->GetPtr();
			char *chBuffer = ( char * ) stackalloc( 128 );
			V_snprintf( chBuffer, 128, "ptr( 0x%p )", ptr );
			if ( !KvWriteText( chBuffer ) )
				return false;
		}
		break;

	case KeyValues::TYPE_WSTRING:
		{
			wchar_t const *wsz = val->GetWString();
			int nLen = V_wcslen( wsz );
			int numBytes = nLen*2 + 64;
			char *chBuffer = ( char * ) stackalloc( numBytes );
			V_snprintf( chBuffer, numBytes, "%ls [wstring, len = %d]", wsz, nLen );
			if ( !KvWriteText( chBuffer ) )
				return false;
		}
		break;

	case KeyValues::TYPE_UINT64:
		{
			uint64 n = val->GetUint64();
			char *chBuffer = ( char * ) stackalloc( 128 );
			V_snprintf( chBuffer, 128, "u64( %lld = 0x%llX )", n, n );
			if ( !KvWriteText( chBuffer ) )
				return false;
		}
		break;

	default:
		break;
#if 0	// this code was accidentally stubbed out by a mis-integration in CL722860; it hasn't been tested
		{
			int n = val->GetDataType();
			char *chBuffer = ( char * ) stackalloc( 128 );
			V_snprintf( chBuffer, 128, "??kvtype[%d]", n );
			if ( !KvWriteText( chBuffer ) )
				return false;
		}
		break;
#endif
	}

	return KvWriteText( "\n" );
}

bool IKeyValuesDumpContextAsText::KvEndKey( KeyValues *pKey, int nIndentLevel )
{
	if ( pKey )
	{
		return
			KvWriteIndent( nIndentLevel ) &&
			KvWriteText( "}\n" );
	}
	else
	{
		return true;
	}
}

bool IKeyValuesDumpContextAsText::KvWriteIndent( int nIndentLevel )
{
	int numIndentBytes = ( nIndentLevel * 2 + 1 );
	char *pchIndent = ( char * ) stackalloc( numIndentBytes );
	memset( pchIndent, ' ', numIndentBytes - 1 );
	pchIndent[ numIndentBytes - 1 ] = 0;
	return KvWriteText( pchIndent );
}


bool CKeyValuesDumpContextAsDevMsg::KvBeginKey( KeyValues *pKey, int nIndentLevel )
{
	static ConVarRef r_developer( "developer" );
	if ( r_developer.IsValid() && r_developer.GetInt() < m_nDeveloperLevel )
		// If "developer" is not the correct level, then avoid evaluating KeyValues tree early
		return false;
	else
		return IKeyValuesDumpContextAsText::KvBeginKey( pKey, nIndentLevel );
}

bool CKeyValuesDumpContextAsDevMsg::KvWriteText( char const *szText )
{
	if ( m_nDeveloperLevel > 0 )
	{
		DevMsg( "%s", szText );
	}
	else
	{
		Msg( "%s", szText );
	}
	return true;
}




//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// vmf_tweak.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "chunkfile.h"
#include "utllinkedlist.h"
#include <stdlib.h>
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlbuffer.h"
#include "tier1/KeyValues.h"
#include "filesystem.h"
#include "FileSystem_Tools.h"
#include "cmdlib.h"

#include <windows.h>

char const *g_pInFilename = NULL;

char *CopyString( char const *pStr );

unsigned long g_CurLoadOrder = 0;


//-----------------------------------------------------------------------------
// default spec function
//-----------------------------------------------------------------------------
SpewRetval_t VMFTweakSpewFunc( SpewType_t spewType, char const *pMsg )
{
	OutputDebugString( pMsg );
	printf( pMsg );
	switch( spewType )
	{
	case SPEW_MESSAGE:
	case SPEW_WARNING:
	case SPEW_LOG:
		return SPEW_CONTINUE;

	case SPEW_ASSERT:
	case SPEW_ERROR:
	default:
		return SPEW_DEBUGGER;
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
	static bool first = false;
	if ( first )
	{
		first = false;
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


class CChunkKeyBase
{
public:
	unsigned long m_LoadOrder;	// Which order it appeared in the file.
};


class CKeyValue : public CChunkKeyBase
{
public:
	void SetKey( const char *pStr )
	{
		delete [] m_pKey;
		m_pKey = CopyString( pStr );
	}

	void SetValue( const char *pStr )
	{
		delete [] m_pValue;
		m_pValue = CopyString( pStr );
	}
	

public:
	char	*m_pKey;
	char	*m_pValue;
};


class CChunk : public CChunkKeyBase
{
public:
	// Returns true if the chunk has the specified key and if its value
	// is equal to pValueStr (case-insensitive).
	bool			CompareKey( char const *pKeyName, char const *pValueStr );

	// Look for a key by name.
	CKeyValue*		FindKey( char const *pKeyName );
	CKeyValue*		FindKey( char const *pKeyName, const char *pValue );

	CChunk*			FindChunk( char const *pKeyName );

	// Find a key by name, and replace any occurences with the new name.
	void			RenameKey( const char *szOldName, const char *szNewName );

public:

	char										*m_pChunkName;
	CUtlLinkedList<CKeyValue*, unsigned short>	m_Keys;
	CUtlLinkedList<CChunk*, unsigned short>		m_Chunks;
};


CChunk* ParseChunk( char const *pChunkName, bool bOnlyOne );


CChunk		*g_pCurChunk = 0;
CChunkFile	*g_pChunkFile = 0;
int g_DotCounter = 0;



// --------------------------------------------------------------------------------- //
// CChunk implementation.
// --------------------------------------------------------------------------------- //

bool CChunk::CompareKey( char const *pKeyName, char const *pValueStr )
{
	CKeyValue *pKey = FindKey( pKeyName );

	if( pKey && stricmp( pKey->m_pValue, pValueStr ) == 0 )
		return true;
	else
		return false;
}	


CKeyValue* CChunk::FindKey( char const *pKeyName )
{
	for( unsigned short i=m_Keys.Head(); i != m_Keys.InvalidIndex(); i=m_Keys.Next(i) )
	{
		CKeyValue *pKey = m_Keys[i];

		if( stricmp( pKey->m_pKey, pKeyName ) == 0 )
			return pKey;
	}

	return NULL;
}


CChunk* CChunk::FindChunk( char const *pChunkName )
{
	for( unsigned short i=m_Chunks.Head(); i != m_Chunks.InvalidIndex(); i=m_Chunks.Next(i) )
	{
		CChunk *pChunk = m_Chunks[i];

		if( stricmp( pChunk->m_pChunkName, pChunkName ) == 0 )
			return pChunk;
	}

	return NULL;
}

CKeyValue* CChunk::FindKey( char const *pKeyName, const char *pValue )
{
	for( unsigned short i=m_Keys.Head(); i != m_Keys.InvalidIndex(); i=m_Keys.Next(i) )
	{
		CKeyValue *pKey = m_Keys[i];

		if( stricmp( pKey->m_pKey, pKeyName ) == 0 && stricmp( pKey->m_pValue, pValue ) == 0 )
			return pKey;
	}

	return NULL;
}


void CChunk::RenameKey( const char *szOldName, const char *szNewName )
{
	if ((!szOldName) || (!szNewName))
		return;

	CKeyValue *pKey = FindKey( szOldName );
	if ( pKey )
	{
		delete pKey->m_pKey;
		pKey->m_pKey = CopyString( szNewName );
	}
}


// --------------------------------------------------------------------------------- //
// Util functions.
// --------------------------------------------------------------------------------- //
char *CopyString( char const *pStr )
{
	char *pRet = new char[ strlen(pStr) + 1 ];
	strcpy( pRet, pStr );
	return pRet;
}

ChunkFileResult_t MyDefaultHandler( CChunkFile *pFile, void *pData, char const *pChunkName )
{
	CChunk *pChunk = ParseChunk( pChunkName, true );
	g_pCurChunk->m_Chunks.AddToTail( pChunk );
	return ChunkFile_Ok;
}


ChunkFileResult_t MyKeyHandler( const char *szKey, const char *szValue, void *pData )
{
	// Add the key to the current chunk.
	CKeyValue *pKey = new CKeyValue;
	pKey->m_LoadOrder = g_CurLoadOrder++;
	
	pKey->m_pKey   = CopyString( szKey );
	pKey->m_pValue = CopyString( szValue );

	g_pCurChunk->m_Keys.AddToTail( pKey );
	return ChunkFile_Ok;
}


CChunk* ParseChunk( char const *pChunkName, bool bOnlyOne )
{
	// Add the new chunk.
	CChunk *pChunk = new CChunk;
	pChunk->m_pChunkName = CopyString( pChunkName );
	pChunk->m_LoadOrder = g_CurLoadOrder++;

	// Parse it out..
	CChunk *pOldChunk = g_pCurChunk;
	g_pCurChunk = pChunk;

	//if( ++g_DotCounter % 16 == 0 )
	//	printf( "." );

	while( 1 )
	{
		if( g_pChunkFile->ReadChunk( MyKeyHandler ) != ChunkFile_Ok )
			break;
	
		if( bOnlyOne )
			break;
	}

	g_pCurChunk = pOldChunk;
	return pChunk;
}


CChunk* ReadChunkFile( char const *pInFilename )
{
	CChunkFile chunkFile;

	if( chunkFile.Open( pInFilename, ChunkFile_Read ) != ChunkFile_Ok )
	{
		printf( "Error opening chunk file %s for reading.\n", pInFilename );
		return NULL;
	}

	printf( "Reading.." );
	chunkFile.SetDefaultChunkHandler( MyDefaultHandler, 0 );
	g_pChunkFile = &chunkFile;
	
	CChunk *pRet = ParseChunk( "***ROOT***", false );
	printf( "\n\n" );

	return pRet;
}

class CChunkHolder
{
public:
	static bool SortChunkFn( const CChunkHolder &a, const CChunkHolder &b )
	{
		return a.m_LoadOrder < b.m_LoadOrder;
	}

	unsigned long m_LoadOrder;
	CKeyValue *m_pKey;
	CChunk *m_pChunk;
};


void WriteChunks_R( CChunkFile *pFile, CChunk *pChunk, bool bRoot )
{
	if( !bRoot )
	{
		pFile->BeginChunk( pChunk->m_pChunkName );
	}

	// Sort them..
	CUtlRBTree<CChunkHolder,int> sortedStuff( 0, 0, &CChunkHolder::SortChunkFn );
	
	// Write keys.
	for( unsigned short i=pChunk->m_Keys.Head(); i != pChunk->m_Keys.InvalidIndex(); i = pChunk->m_Keys.Next( i ) )
	{
		CChunkHolder holder;
		holder.m_pKey = pChunk->m_Keys[i];
		holder.m_LoadOrder = holder.m_pKey->m_LoadOrder;
		holder.m_pChunk = NULL;
		sortedStuff.Insert( holder );
	}

	// Write subchunks.
	for( int i=pChunk->m_Chunks.Head(); i != pChunk->m_Chunks.InvalidIndex(); i = pChunk->m_Chunks.Next( i ) )
	{
		CChunkHolder holder;
		holder.m_pChunk = pChunk->m_Chunks[i];
		holder.m_LoadOrder = holder.m_pChunk->m_LoadOrder;
		holder.m_pKey = NULL;
		sortedStuff.Insert( holder );
	}

	// Write stuff in sorted order.
	int i = sortedStuff.FirstInorder();
	if ( i != sortedStuff.InvalidIndex() )
	{
		while ( 1 )
		{
			CChunkHolder &h = sortedStuff[i];
			if ( h.m_pKey )
			{
				pFile->WriteKeyValue( h.m_pKey->m_pKey, h.m_pKey->m_pValue );
			}
			else
			{
				WriteChunks_R( pFile, h.m_pChunk, false );
			}
			if ( i == sortedStuff.LastInorder() )
				break;
			i = sortedStuff.NextInorder( i );
		}
	}

	if( !bRoot )
	{
		pFile->EndChunk();
	}
}


bool WriteChunkFile( char const *pOutFilename, CChunk *pRoot )
{
	CChunkFile chunkFile;

	if( chunkFile.Open( pOutFilename, ChunkFile_Write ) != ChunkFile_Ok )
	{
		printf( "Error opening chunk file %s for writing.\n", pOutFilename );
		return false;
	}

	printf( "Writing.." );
	WriteChunks_R( &chunkFile, pRoot, true );
	printf( "\n\n" );

	return true;
}


//
//
// EXAMPLE
//
//
void ScanRopeSlack( CChunk *pChunk )
{
	if( stricmp( pChunk->m_pChunkName, "entity" ) == 0 )
	{
		if( pChunk->CompareKey( "classname", "keyframe_rope" ) ||
			pChunk->CompareKey( "classname", "move_rope" ) )
		{
			CKeyValue *pKey = pChunk->FindKey( "slack" );
			if( pKey )
			{
				// Subtract 100 from all the Slack properties.
				float flCur = (float)atof( pKey->m_pValue );
				char str[256];
				sprintf( str, "%f", flCur + 100 );
				pKey->m_pValue = CopyString( str );
			}
		}
	}
}

int g_nLogicAutoReplacementsMade = 0;
void LogicAuto( CChunk *pChunk )
{
	if ( pChunk->CompareKey( "classname", "logic_auto" ) )
	{
		CChunk *pConnections = pChunk->FindChunk( "connections" );
		if ( !pConnections )
			return;
		
		bool bFound = false;
		for ( int i=0; i < pConnections->m_Keys.Count(); i++ )
		{
			CKeyValue *pTestKV = pConnections->m_Keys[i];
			if ( V_stristr( pTestKV->m_pValue, "tonemap" ) == pTestKV->m_pValue )
			{
				bFound = true;
				break;
			}
		}
		
		if ( !bFound )
			return;
			
		++g_nLogicAutoReplacementsMade;			 

		// Ok, this is a logic_auto with OnMapSpawn outputs in its connections.		
		CUtlLinkedList<CKeyValue*,int> newKeys;
		FOR_EACH_LL( pConnections->m_Keys, i )
		{
			CKeyValue *pTestKV = pConnections->m_Keys[i];
			if ( V_stricmp( pTestKV->m_pKey, "OnMapSpawn" ) == 0 )
			{
				if ( V_stristr( pTestKV->m_pValue, "tonemap" ) == pTestKV->m_pValue )
				{
					CKeyValue *pNewKV = new CKeyValue;
					pNewKV->SetKey( "OnMapTransition" );
					pNewKV->SetValue( pTestKV->m_pValue );
					newKeys.AddToTail( pNewKV );
				}
			}
		}
		
		FOR_EACH_LL( newKeys, i )
		{
			if ( !pConnections->FindKey( "OnMapTransition", newKeys[i]->m_pValue ) )
			{
				pConnections->m_Keys.AddToTail( newKeys[i] );
			}
		}
		
		// Fix spawnflags.
		CKeyValue *pFlags = pChunk->FindKey("spawnflags");
		if ( pFlags )
		{
			unsigned long curVal;
			sscanf( pFlags->m_pValue, "%lu", &curVal );
			if ( curVal & 1 )
			{
				--curVal;
				char str[512];
				sprintf( str, "%lu", curVal );
				pFlags->SetValue( str );
			}
		}
	}
}


void ScanChunks( CChunk *pChunk, void (*fn)(CChunk *) )
{
	fn( pChunk );

	// Recurse into the children.
	for( unsigned short i=pChunk->m_Chunks.Head(); i != pChunk->m_Chunks.InvalidIndex(); i = pChunk->m_Chunks.Next( i ) )
	{
		ScanChunks( pChunk->m_Chunks[i], fn );
	}
}

int main(int argc, char* argv[])
{
	CommandLine()->CreateCmdLine( argc, argv );
	SpewOutputFunc( VMFTweakSpewFunc );

	if( argc < 2 )
	{
		printf( "vmf_tweak <input file> [output file]\n" );
		return 1;
	}

	g_pInFilename = argv[1];
	char const *pOutFilename = argc >= 3 ? argv[2] : "";

	CChunk *pRoot = ReadChunkFile( g_pInFilename );
	if( !pRoot )
		return 2;

	//
	//
	//
	// This is where you can put code to modify the VMF.
	//
	//
	//

	// If they didn't specify -game on the command line, use VPROJECT.
	char workingdir[ 256 ];
	workingdir[0] = 0;
	Q_getwd( workingdir, sizeof(workingdir) );
	CmdLib_InitFileSystem( workingdir );
	ScanChunks( pRoot, LogicAuto );
	FileSystem_Term();

	Msg( "%s: %d logic_auto replacements made.\n", g_pInFilename, g_nLogicAutoReplacementsMade );

	if ( argc >= 3 )
	{
		if( !WriteChunkFile( pOutFilename, pRoot ) )
			return 3;
	}

	return 0;
}


//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// Defines the entry point for the application.
//
//===========================================================================//

#include "reslistgenerator.h"
#include "filesystem.h"
#include "tier1/utlrbtree.h"
#include "tier1/fmtstr.h"
#include "characterset.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include "tier1/utlbuffer.h"
#include "tier0/icommandline.h"
#include "tier1/KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool SaveResList( const CUtlRBTree< CUtlString, int > &list, char const *pchFileName, char const *pchSearchPath )
{
	FileHandle_t fh = g_pFullFileSystem->Open( pchFileName, "wt", pchSearchPath );
	if ( fh != FILESYSTEM_INVALID_HANDLE )
	{
		for ( int i = list.FirstInorder(); i != list.InvalidIndex(); i = list.NextInorder( i ) )
		{
			g_pFullFileSystem->Write( list[ i ].String(), Q_strlen( list[ i ].String() ), fh );
			g_pFullFileSystem->Write( "\n", 1, fh );
		}

		g_pFullFileSystem->Close( fh );
		return true;
	}
	return false;
}

void LoadResList( CUtlRBTree< CUtlString, int > &list, char const *pchFileName, char const *pchSearchPath )
{
	CUtlBuffer buffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( pchFileName, pchSearchPath, buffer ) )
	{
		// does not exist
		return;
	}

	characterset_t breakSet;
	CharacterSetBuild( &breakSet, "" );

	// parse reslist
	char szToken[ MAX_PATH ];
	for ( ;; )
	{
		int nTokenSize = buffer.ParseToken( &breakSet, szToken, sizeof( szToken ) );
		if ( nTokenSize <= 0 )
		{
			break;
		}

		Q_strlower( szToken );
		Q_FixSlashes( szToken );

		// Ensure filename has "quotes" around it
		CUtlString s;
		if ( szToken[ 0 ] == '\"' )
		{
			Assert( Q_strlen( szToken ) > 2 );
			Assert( szToken[ Q_strlen( szToken ) - 1 ] == '\"' );
			s = szToken;
		}
		else
		{
			s = CFmtStr( "\"%s\"", szToken );
		}

		int idx = list.Find( s );
		if ( idx == list.InvalidIndex() )
		{
			list.Insert( s );
		}
	}
}

static bool ReslistLogLessFunc( CUtlString const &pLHS, CUtlString const &pRHS )
{
	return CaselessStringLessThan( pLHS.Get(), pRHS.Get() );
}

void SortResList( char const *pchFileName, char const *pchSearchPath )
{
	CUtlRBTree< CUtlString, int > sorted( 0, 0, ReslistLogLessFunc );
	LoadResList( sorted, pchFileName, pchSearchPath );

	// Now write it back out
	SaveResList( sorted, pchFileName, pchSearchPath );
}

void MergeResLists( CUtlVector< CUtlString > &fileNames, char const *pchOutputFile, char const *pchSearchPath )
{
	CUtlRBTree< CUtlString, int > sorted( 0, 0, ReslistLogLessFunc );
	for ( int i = 0; i < fileNames.Count(); ++i )
	{
		LoadResList( sorted, fileNames[ i ].String(), pchSearchPath );
	}

	// Now write it back out
	SaveResList( sorted, pchOutputFile, pchSearchPath );
}

class CWorkItem
{
public:
	CWorkItem()
	{
	}

	CUtlString		m_sSubDir;
	CUtlString		m_sAddCommands;
};
class CResListGenerator: public IResListGenerator
{
public:
	enum
	{
		STATE_BUILDINGRESLISTS = 0,
		STATE_GENERATINGCACHES,
	};

	CResListGenerator();

	virtual void Init( char const *pchBaseDir, char const *pchGameDir );
	virtual bool IsActive();
	virtual void Shutdown();
	virtual void Collate();

	virtual void SetupCommandLine();
	virtual bool ShouldContinue();

private:

	bool InitCommandFile( char const *pchGameDir, char const *pchCommandFile );
	void LoadMapList( char const *pchGameDir, CUtlVector< CUtlString > &vecMaps, char const *pchMapFile );
	void CollateFiles( char const *pchResListFilename );

	bool		m_bInitialized;
	bool		m_bActive;
	CUtlString	m_sBaseDir;
	CUtlString  m_sGameDir;
	CUtlString	m_sFullGamePath;

	CUtlString	m_sFinalDir;
	CUtlString	m_sWorkingDir;
	CUtlString	m_sBaseCommandLine;
	CUtlString	m_sOriginalCommandLine;
	CUtlString	m_sInitialStartMap;

	int			m_nCurrentWorkItem;
	CUtlVector< CWorkItem >	m_WorkItems;

	CUtlVector< CUtlString > m_MapList;
	int			m_nCurrentState;
};

static CResListGenerator g_ResListGenerator;
IResListGenerator *reslistgenerator = &g_ResListGenerator;

CResListGenerator::CResListGenerator() :
	m_bInitialized( false ),
	m_bActive( false ),
	m_nCurrentWorkItem( 0 ),
	m_nCurrentState( STATE_BUILDINGRESLISTS )
{
	MEM_ALLOC_CREDIT();

	m_sFinalDir = "reslists";
	m_sWorkingDir = "reslists_work";
}

void CResListGenerator::CollateFiles( char const *pchResListFilename )
{
	CUtlVector< CUtlString > vecReslists;

	for ( int i = 0; i < m_WorkItems.Count(); ++i )
	{
		char fn[ MAX_PATH ];
		Q_snprintf( fn, sizeof( fn ), "%s\\%s\\%s\\%s", m_sFullGamePath.String(), m_sWorkingDir.String(), m_WorkItems[ i ].m_sSubDir.String(), pchResListFilename );
		vecReslists.AddToTail( fn );
	}

	MergeResLists( vecReslists, CFmtStr( "%s\\%s\\%s", m_sFullGamePath.String(), m_sFinalDir.String(), pchResListFilename ), "GAME" );
}

void CResListGenerator::Init( char const *pchBaseDir, char const *pchGameDir )
{
	if ( IsX360() )
	{
		// not used or supported
		return;
	}

	// Because we have to call this inside the first Apps "PreInit", we need only Init on the very first call
	if ( m_bInitialized )
	{
		return;
	}

	m_bInitialized = true;

	m_sBaseDir = pchBaseDir;
	m_sGameDir = pchGameDir;

	char path[MAX_PATH];
	Q_snprintf( path, sizeof(path), "%s/%s", m_sBaseDir.String(), m_sGameDir.String() );
	Q_FixSlashes( path );
	Q_strlower( path );
	m_sFullGamePath = path;

	const char *pchCommandFile = NULL;
	if ( CommandLine()->CheckParm( "-makereslists", &pchCommandFile ) && pchCommandFile )
	{
		// base path setup, now can get and parse command file
		InitCommandFile( path, pchCommandFile );
	}
}

void CResListGenerator::Shutdown()
{
	if ( !m_bActive )
		return;
}

bool CResListGenerator::IsActive()
{
	return m_bInitialized && m_bActive;
}

void CResListGenerator::Collate()
{
	char szDir[MAX_PATH];
	V_snprintf( szDir, sizeof( szDir ), "%s\\%s", m_sFullGamePath.String(), m_sFinalDir.String() );
	g_pFullFileSystem->CreateDirHierarchy( szDir, "GAME" );

	// Now create the collated/merged data
	CollateFiles( "all.lst" );
	CollateFiles( "engine.lst" );
	for ( int i = 0 ; i < m_MapList.Count(); ++i )
	{
		CollateFiles( CFmtStr( "%s.lst", m_MapList[ i ].String() ) );
	}
}

void CResListGenerator::SetupCommandLine()
{
	if ( !m_bActive )
		return;

	switch ( m_nCurrentState )
	{
	case STATE_BUILDINGRESLISTS:
		{
			Assert( m_nCurrentWorkItem < m_WorkItems.Count() );

			const CWorkItem &work = m_WorkItems[ m_nCurrentWorkItem ];

			// Clean the working dir
			char szWorkingDir[ 512 ];
			Q_snprintf( szWorkingDir, sizeof( szWorkingDir ), "%s\\%s", m_sWorkingDir.String(), work.m_sSubDir.String() );

			char szFullWorkingDir[MAX_PATH];
			V_snprintf( szFullWorkingDir, sizeof( szFullWorkingDir ), "%s\\%s", m_sFullGamePath.String(), szWorkingDir );
			g_pFullFileSystem->CreateDirHierarchy( szFullWorkingDir, "GAME" );

			// Preserve startmap
			char const *pszStartMap = NULL;
			CommandLine()->CheckParm( "-startmap", &pszStartMap );
			char szMap[ MAX_PATH ] = { 0 };
			if ( pszStartMap )
			{
				Q_strncpy( szMap, pszStartMap, sizeof( szMap ) );
			}

			// Prepare stuff
			// Reset command line based on current state
			char szCmd[ 512 ];
			Q_snprintf( szCmd, sizeof( szCmd ), "%s %s %s -reslistdir %s", m_sOriginalCommandLine.String(), m_sBaseCommandLine.String(), work.m_sAddCommands.String(), szWorkingDir );

			Warning( "Reslists:  Setting command line:\n'%s'\n", szCmd );

			CommandLine()->CreateCmdLine( szCmd );
			// Never rebuild caches by default, inly do it in STATE_GENERATINGCACHES
			CommandLine()->AppendParm( "-norebuildaudio", NULL );
			if ( szMap[ 0 ] )
			{
				CommandLine()->AppendParm( "-startmap", szMap );
			}
		}
		break;
	case STATE_GENERATINGCACHES:
		{
			Collate();

			// Prepare stuff
			// Reset command line based on current state
			char szCmd[ 512 ];
			Q_snprintf( szCmd, sizeof( szCmd ), "%s -reslistdir %s -rebuildaudio", m_sOriginalCommandLine.String(), m_sFinalDir.String());

			Warning( "Caches:  Setting command line:\n'%s'\n", szCmd );

			CommandLine()->CreateCmdLine( szCmd );
			
			CommandLine()->RemoveParm( "-norebuildaudio" );
			CommandLine()->RemoveParm( "-makereslists" );

			++m_nCurrentState;
		}
		break;
	}
}

bool CResListGenerator::ShouldContinue()
{
	if ( !m_bActive )
		return false;
	
	bool bContinueAdvancing = false;
	do
	{
		switch ( m_nCurrentState )
		{
		default:
			break;
		case STATE_BUILDINGRESLISTS:
			{ 
				CommandLine()->RemoveParm( "-startmap" );

				// Advance to next time
				++m_nCurrentWorkItem;

				if ( m_nCurrentWorkItem >= m_WorkItems.Count())
				{
					// Will stay in the loop
					++m_nCurrentState;
					bContinueAdvancing = true;
				}
				else
				{
					return true;
				}
			}
			break;
		case STATE_GENERATINGCACHES:
			{
				return true;
			}
			break;
		}
	} while ( bContinueAdvancing );
	
	return false;
}

void CResListGenerator::LoadMapList( char const *pchGameDir, CUtlVector< CUtlString > &vecMaps, char const *pchMapFile )
{
	char fullpath[ 512 ];
	Q_snprintf( fullpath, sizeof( fullpath ), "%s/%s", pchGameDir, pchMapFile );

	// Load them in
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	if ( g_pFullFileSystem->ReadFile( fullpath, "GAME", buf ) )
	{
		char szMap[ MAX_PATH ];
		while ( true )
		{
			buf.GetLine( szMap, sizeof( szMap ) );
			if ( !szMap[ 0 ] )
				break;

			// Strip trailing CR/LF chars
			int len = Q_strlen( szMap );
			while ( len >= 1 && ( szMap[ len - 1 ] == '\n' || szMap[ len - 1 ] == '\r' ) )
			{
				szMap[ len - 1 ] = 0;
				len = Q_strlen( szMap );
			}

			CUtlString newMap;
			newMap = szMap;
			vecMaps.AddToTail( newMap );
		}
	}
	else
	{
		Error( "Unable to maplist file %s\n", fullpath );
	}
}

bool CResListGenerator::InitCommandFile( char const *pchGameDir, char const *pchCommandFile )
{
	if ( *pchCommandFile == '+' ||
		 *pchCommandFile == '-' )
	{
		Msg( "falling back to legacy reslists system\n" );
		return false;
	}

	char fullpath[ 512 ];
	Q_snprintf( fullpath, sizeof( fullpath ), "%s/%s", pchGameDir, pchCommandFile );

	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( fullpath, "GAME", buf ) )
	{
		Error( "Unable to load '%s'\n", fullpath );
		return false;
	}

	KeyValues *kv = new KeyValues( "reslists" );
	if ( !kv->LoadFromBuffer( "reslists", (const char *)buf.Base() ) )
	{
		Error( "Unable to parse keyvalues from '%s'\n", fullpath );
		kv->deleteThis();
		return false;
	}

	CUtlString sMapListFile = kv->GetString( "maplist", "maplist.txt" );
	LoadMapList( pchGameDir, m_MapList, sMapListFile );
	if ( m_MapList.Count() <= 0 )
	{
		Error( "Maplist file '%s' empty or missing!!!\n", sMapListFile.String() );
		kv->deleteThis();
		return false;
	}

	char const *pszSolo = NULL;
	if ( CommandLine()->CheckParm( "+map", &pszSolo ) && pszSolo )
	{
		m_MapList.Purge();

		CUtlString newMap;
		newMap = pszSolo;
		m_MapList.AddToTail( newMap );
	}
	
	m_nCurrentWorkItem = CommandLine()->ParmValue( "-startstage", 0 );

	char const *pszStartMap = NULL;
	CommandLine()->CheckParm( "-startmap", &pszStartMap );
	if ( pszStartMap )
	{
		m_sInitialStartMap = pszStartMap;
	}

	CommandLine()->RemoveParm( "-startstage" );
	CommandLine()->RemoveParm( "-makereslists" );
	CommandLine()->RemoveParm( "-reslistdir" );
	CommandLine()->RemoveParm( "-norebuildaudio" );
	CommandLine()->RemoveParm( "-startmap" );

	m_sOriginalCommandLine = CommandLine()->GetCmdLine();

	// Add it back in for first map
	if ( pszStartMap )
	{
		CommandLine()->AppendParm( "-startmap", m_sInitialStartMap.String() );
	}

	m_sBaseCommandLine = kv->GetString( "basecommandline", "" );
	m_sFinalDir = kv->GetString( "finaldir", m_sFinalDir.String() );
	m_sWorkingDir = kv->GetString( "workdir", m_sWorkingDir.String() );

	int i = 0;
	do
	{
		char sz[ 32 ];
		Q_snprintf( sz, sizeof( sz ), "%i", i );
		KeyValues *subKey = kv->FindKey( sz, false );
		if ( !subKey )
			break;

		CWorkItem work;

		work.m_sSubDir = subKey->GetString( "subdir", "" );
		work.m_sAddCommands = subKey->GetString( "addcommands", "" );

		if ( work.m_sSubDir.Length() > 0 )
		{
			m_WorkItems.AddToTail( work );
		}
		else
		{
			Error( "%s: failed to specify 'subdir' for item %s\n", fullpath, sz );
		}

		++i;
	} while ( true );

	m_bActive = m_WorkItems.Count() > 0;
	
	m_nCurrentWorkItem = clamp( m_nCurrentWorkItem, 0, m_WorkItems.Count() - 1 );

	bool bCollate = CommandLine()->CheckParm( "-collate" ) ? true : false;
	if ( bCollate )
	{
		Collate();
		m_bActive = false;
		exit( -1 );
	}

	kv->deleteThis();

	/*
	if ( m_bActive )
	{
		// Wipe console log
		g_pFullFileSystem->RemoveFile( "console.log", "GAME" );
	}
	*/
	return m_bActive;
}



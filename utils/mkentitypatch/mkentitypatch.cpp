//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
//=============================================================================


// Valve includes
#include "appframework/tier2app.h"
#include "filesystem.h"
#include "icommandline.h"
#include "tier2/p4helpers.h"
#include "p4lib/ip4.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "bsplib.h"
#include "lumpfiles.h"
#include "filesystem_tools.h"
#include "cmdlib.h"

#ifdef _DEBUG
#include <windows.h>
#undef GetCurrentDirectory
#endif

//-----------------------------------------------------------------------------
// Standard spew functions
//-----------------------------------------------------------------------------
static SpewRetval_t SpewStdout( SpewType_t spewType, char const *pMsg )
{
	if ( !pMsg )
		return SPEW_CONTINUE;

#ifdef _DEBUG
	OutputDebugString( pMsg );
#endif

	printf( pMsg );
	fflush( stdout );

	return ( spewType == SPEW_ASSERT ) ? SPEW_DEBUGGER : SPEW_CONTINUE; 
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CMkEntityPatchApp : public CTier2SteamApp
{
	typedef CTier2SteamApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void Destroy() {}

	void PrintHelp( );

private:
};


DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT( CMkEntityPatchApp );

//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CMkEntityPatchApp::Create()
{
	SpewOutputFunc( SpewStdout );

	AppSystemInfo_t appSystems[] = 
	{
		{ "p4lib.dll",				P4_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	return AddSystems( appSystems );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CMkEntityPatchApp::PreInit( )
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );

	if ( !BaseClass::PreInit() )
		return false;

	if ( !g_pFullFileSystem )
	{
		Error( "// ERROR: sfmgen is missing a required interface!\n" );
		return false;
	}

	// Add paths...
	if ( !SetupSearchPaths( NULL, false, true ) )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Print help
//-----------------------------------------------------------------------------
void CMkEntityPatchApp::PrintHelp( )
{
	Msg( "Usage: mkentitypatch [-nop4] [-vproject <path to gameinfo.txt>] <in .bsp file>\n" );
	Msg( "\t-nop4\t: [Optional] Disables auto perforce checkout/add.\n" );
	Msg( "\t-vproject\t: [Optional] Specifies path to a gameinfo.txt file (which mod to build for).\n" );
	Msg( "\t Source .BSP file whose entity lump you wish to patch.\n" );
}


//-----------------------------------------------------------------------------
// Computes a full directory
//-----------------------------------------------------------------------------
static void ComputeFullPath( const char *pRelativeDir, char *pFullPath, int nBufLen )
{
	if ( !Q_IsAbsolutePath( pRelativeDir ) )
	{
		char pDir[MAX_PATH];
		if ( g_pFullFileSystem->GetCurrentDirectory( pDir, sizeof(pDir) ) )
		{
			Q_ComposeFileName( pDir, pRelativeDir, pFullPath, nBufLen );
		}
	}
	else
	{
		Q_strncpy( pFullPath, pRelativeDir, nBufLen );
	}

	Q_StripTrailingSlash( pFullPath );

	// Ensure the output directory exists
	g_pFullFileSystem->CreateDirHierarchy( pFullPath );
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
entity_t *FindEntity( KeyValues *pEntity )
{
	int nHammerId = pEntity->GetInt( "id", INT_MIN );
	if ( nHammerId != INT_MIN )
	{
		// First, look for hammerid
		for ( int i = 0; i < num_entities; ++i )
		{
			int nId = IntForKeyWithDefault( &entities[i], "hammerid", INT_MIN );
			if ( nId == nHammerId )
				return &entities[i];
		}
	}

	// Unfortunately, hammmerid appears to be a relatively new feature. Now, we must
	// look for target name
	int nMatch = -1;
	const char *pTargetName = pEntity->GetString( "targetname" );
	if ( pTargetName && pTargetName[0] )
	{
		// First, look for hammerid
		for ( int i = 0; i < num_entities; ++i )
		{
			const char *pMatchTargetName = ValueForKey( &entities[i], "targetname" );
			if ( !pMatchTargetName || !pMatchTargetName[0] )
				continue;
			if ( !V_stricmp( pTargetName, pMatchTargetName ) )
			{
				if ( nMatch >= 0 )
				{
					//Warning( "Encountered multiple entities that matched targetname %s!\n", pTargetName );
					//return false;
					nMatch = -1; // force a fallback to scanning classname and origin 
					break;
				}
				else
					nMatch = i;
			}
		}
	}

	if ( nMatch >= 0 )
		return &entities[nMatch];

	// No target name? Well, let's try classname and origin.
	const char *pClassName = pEntity->GetString( "classname" );
	if ( pClassName && pClassName[0] )
	{
		// First, look for hammerid
		for ( int i = 0; i < num_entities; ++i )
		{
			const char *pMatchClassName = ValueForKey( &entities[i], "classname" );
			if ( !pMatchClassName || !pMatchClassName[0] )
				continue;
			if ( V_stricmp( pClassName, pMatchClassName ) )
				continue;

			const char *pOrigin = "(na)";
			if ( V_stricmp( pClassName, "worldspawn" ) ) // allow worldspawn to match all
			{
				pOrigin = pEntity->GetString( "origin" );
				const char *pMatchOrigin = ValueForKey( &entities[i], "origin" );
				if ( !pMatchOrigin || !pMatchOrigin[0] )
					continue;
				if ( V_stricmp( pOrigin, pMatchOrigin ) )
					continue;
			}

			if ( nMatch >= 0 )
			{
				Warning( "Encountered multiple entities that matched classname %s, origin %s!\n", pClassName, pOrigin );
				return false;
			}
			nMatch = i;
		}
	}

	if ( nMatch >= 0 )
		return &entities[nMatch];

	return NULL;
}

bool InsertEntity( entity_t *pEntity, KeyValues *pEntityKeys )
{
	CUtlVector<KeyValues *> vecKVs;
	for ( KeyValues *pKey = pEntityKeys->GetFirstValue(); pKey; pKey = pKey->GetNextValue() )
	{
		vecKVs.AddToTail( pKey );
	}

	FOR_EACH_VEC_BACK( vecKVs, i )
	{
		epair_t *e = (epair_t*)malloc( sizeof(epair_t) );
		memset (e, 0, sizeof(epair_t));

		const char *pName = vecKVs[i]->GetName();
		if ( strlen(pName) >= MAX_KEY-1 )
		{
			Warning( "ParseEpar: token %s too long", pName );
			return false;
		}
		e->key = copystring(pName);

		const char *pValue = vecKVs[i]->GetString();
		if ( strlen(pValue) >= MAX_VALUE-1 )
		{
			Warning( "ParseEpar: token %s too long", pValue );
			return false;
		}
		e->value = copystring(pValue);

		// strip trailing spaces
		StripTrailing( e->key );
		StripTrailing( e->value );
		e->next = pEntity->epairs;
		pEntity->epairs = e;
	}

	// Flatten everything ( specifically, 'connection' keys, necessary to
	// make the patch file have the same format as the commentary files )
	for ( KeyValues *pKey = pEntityKeys->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey() )
	{
		InsertEntity( pEntity, pKey );
	}

	return true;
}

bool InsertEntity( KeyValues *pEntity )
{
	entity_t &entity = entities[ num_entities++ ];
	return InsertEntity( &entity, pEntity );
}

bool ReplaceEntity( KeyValues *pEntity )
{
	entity_t *pReplace = FindEntity( pEntity );
	if ( !pReplace )
	{
		Warning( "Tried to replace an entity %s, origin %s, but couldn't find the original!\n", pEntity->GetString( "classname" ), pEntity->GetString( "origin" ) );
		return false;
	}

	epair_t *pNext;
	for ( epair_t *e = pReplace->epairs; e; e = pNext )
	{
		pNext = e->next;
		free( e->key );
		free( e->value );
		free( e );
	}
	pReplace->epairs = NULL;

	return InsertEntity( pReplace, pEntity );
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
int CMkEntityPatchApp::Main()
{
	// Backward compat for bsplib
	g_pFileSystem = g_pFullFileSystem;

	// This bit of hackery allows us to access files on the harddrive
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD ); 

	if ( CommandLine()->CheckParm( "-h" ) || CommandLine()->CheckParm( "-help" ) || CommandLine()->ParmCount() == 1 )
	{
		PrintHelp();
		return 0;
	}

	// The file name is the last argument
	const char *pBSPFile = CommandLine()->GetParm( CommandLine()->ParmCount() - 1 );
	if ( !pBSPFile || pBSPFile[0] == 0 || pBSPFile[0] == '-' )
	{
		PrintHelp();
		return 0;
	}

	char pFullPath[MAX_PATH];
	ComputeFullPath( pBSPFile, pFullPath, sizeof(pFullPath) );

	char pBSPFileName[MAX_PATH];
	char pPatchFileName[MAX_PATH];
	char pOutputFileName[MAX_PATH];
	V_strcpy( pBSPFileName, pFullPath );
	V_strcpy( pPatchFileName, pFullPath );
	V_SetExtension( pBSPFileName, ".bsp", sizeof(pBSPFileName) );
	V_SetExtension( pPatchFileName, ".pat", sizeof(pPatchFileName) );
	GenerateLumpFileName( pFullPath, pOutputFileName, sizeof(pOutputFileName), LUMP_ENTITIES );

	if ( !g_pFullFileSystem->FileExists( pBSPFileName ) )
	{
		Warning( "BSP file %s doesn't exist!\n", pBSPFileName );
		return 0;
	}

	if ( !g_pFullFileSystem->FileExists( pPatchFileName ) )
	{
		Warning( "BSP patch file %s doesn't exist!\n", pPatchFileName );
		return 0;
	}

	KeyValues *pKeyValues = new KeyValues( "patch" );
	if ( !pKeyValues->LoadFromFile( g_pFullFileSystem, pPatchFileName ) )
	{
		Warning( "Error parsing patch file %s!\n", pPatchFileName );
		return 0;
	}

	LoadBSPFile( pFullPath );
	ParseEntities();

	for( int i = 0; i < num_entities; i++ )
	{
		entity_t *pCur = &entities[i];
		epair_t *pNext = NULL;
		epair_t *pPrev = NULL;
		for ( epair_t *e = pCur->epairs; e; e = pNext )
		{	
			pNext = e->next;
			e->next = pPrev;
			pPrev = e;
		}
		pCur->epairs = pPrev;
	}

	for ( KeyValues *pKey = pKeyValues->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey() )
	{
		const char *pKeyName = pKey->GetName();
		if ( !V_stricmp( pKeyName, "entity" ) )
		{
			if ( !InsertEntity( pKey ) )
				return 0;
		}
		else if ( !V_stricmp( pKeyName, "replace_entity" ) )
		{
			if ( !ReplaceEntity( pKey ) )
				return 0;
		}
	}

	// Do Perforce Stuff
	if ( CommandLine()->FindParm( "-nop4" ) )
	{
		g_p4factory->SetDummyMode( true );
	}

	g_p4factory->SetOpenFileChangeList( "Entity Patch Files" );

	CP4AutoAddFile p4AddBSP( pBSPFileName );
	CP4AutoAddFile p4AddPatch( pPatchFileName );
	CP4AutoEditAddFile p4AddOutput( pOutputFileName );

	UnparseEntities();
	WriteLumpToFile( pBSPFileName, LUMP_ENTITIES, 0, dentdata.Base(), dentdata.Count() );

	pKeyValues->deleteThis();

	return -1;
}
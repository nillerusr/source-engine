//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "quakedef.h" // for MAX_OSPATH
#include <stdlib.h>
#include <assert.h>
#include <malloc.h>
#include "filesystem.h"
#include "bitmap/tgawriter.h"
#include <tier2/tier2.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IFileSystem *g_pFileSystem = NULL;


void fs_whitelist_spew_flags_changefn( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	if ( g_pFileSystem )
	{
		ConVarRef var( pConVar );
		g_pFileSystem->SetWhitelistSpewFlags( var.GetInt() );
	}
}

#if defined( _DEBUG )
ConVar fs_whitelist_spew_flags( "fs_whitelist_spew_flags", "0", 0,
	"Set whitelist spew flags to a combination of these values:\n"
	"   0x0001 - list files as they are added to the CRC tracker\n"
	"   0x0002 - show files the filesystem is telling the engine to reload\n"
	"   0x0004 - show files the filesystem is NOT telling the engine to reload",
	fs_whitelist_spew_flags_changefn );
#endif

CON_COMMAND( path, "Show the engine filesystem path." )
{
	if( g_pFileSystem )
	{
		g_pFileSystem->PrintSearchPaths();
	}
}

CON_COMMAND( fs_printopenfiles, "Show all files currently opened by the engine." )
{
	if( g_pFileSystem )
	{
		g_pFileSystem->PrintOpenedFiles();
	}
}

CON_COMMAND( fs_warning_level, "Set the filesystem warning level." )
{
	if( args.ArgC() != 2 )
	{
		Warning( "\"fs_warning_level n\" where n is one of:\n" );
		Warning( "\t0:\tFILESYSTEM_WARNING_QUIET\n" );
		Warning( "\t1:\tFILESYSTEM_WARNING_REPORTUNCLOSED\n" );
		Warning( "\t2:\tFILESYSTEM_WARNING_REPORTUSAGE\n" );
		Warning( "\t3:\tFILESYSTEM_WARNING_REPORTALLACCESSES\n" );
		Warning( "\t4:\tFILESYSTEM_WARNING_REPORTALLACCESSES_READ\n" );
		Warning( "\t5:\tFILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE\n" );
		Warning( "\t6:\tFILESYSTEM_WARNING_REPORTALLACCESSES_ASYNC\n" );
		return;
	}

	int level = atoi( args[ 1 ] );
	switch( level )
	{
	case FILESYSTEM_WARNING_QUIET:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_QUIET\n" );
		break;
	case FILESYSTEM_WARNING_REPORTUNCLOSED:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTUNCLOSED\n" );
		break;
	case FILESYSTEM_WARNING_REPORTUSAGE:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTUSAGE\n" );
		break;
	case FILESYSTEM_WARNING_REPORTALLACCESSES:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTALLACCESSES\n" );
		break;
	case FILESYSTEM_WARNING_REPORTALLACCESSES_READ:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTALLACCESSES_READ\n" );
		break;
	case FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTALLACCESSES_READWRITE\n" );
		break;
	case FILESYSTEM_WARNING_REPORTALLACCESSES_ASYNC:
		Warning( "fs_warning_level = FILESYSTEM_WARNING_REPORTALLACCESSES_ASYNC\n" );
		break;

	default:
		Warning( "fs_warning_level = UNKNOWN!!!!!!!\n" );
		return;
		break;
	}
	g_pFileSystem->SetWarningLevel( ( FileWarningLevel_t )level );
}


//-----------------------------------------------------------------------------
// Purpose: Wrap Sys_LoadModule() with a filesystem GetLocalCopy() call to
//			ensure have the file to load when running Steam.
//-----------------------------------------------------------------------------
CSysModule *FileSystem_LoadModule(const char *path)
{
	if ( g_pFileSystem )
		return g_pFileSystem->LoadModule( path );
	else
		return Sys_LoadModule(path);
}

//-----------------------------------------------------------------------------
// Purpose: Provided for symmetry sake with FileSystem_LoadModule()...
//-----------------------------------------------------------------------------
void FileSystem_UnloadModule(CSysModule *pModule)
{
	Sys_UnloadModule(pModule);
}


void FileSystem_SetWhitelistSpewFlags()
{
#if defined( _DEBUG )
	if ( !g_pFileSystem )
	{
		Assert( !"FileSystem_InitSpewFlags - no filesystem." );
		return;
	}
	
	g_pFileSystem->SetWhitelistSpewFlags( fs_whitelist_spew_flags.GetInt() );
#endif
}

	

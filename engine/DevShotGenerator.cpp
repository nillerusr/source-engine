//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "DevShotGenerator.h"
#include "cmd.h"
#include "fmtstr.h"
#include "host.h"
#include "tier0/icommandline.h"
#include <tier0/dbg.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PAUSE_FRAMES_BETWEEN_MAPS	300
#define PAUSE_TIME_BETWEEN_MAPS		2.0f

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DevShotGenerator_Usage()
{
	// 
	Msg( "-makedevshots usage:\n" );
	Msg( "  [ -usedevshotsfile filename ] -- get map list from specified file, default is to build for maps/*.bsp\n" );
	Msg( "  [ -startmap mapname ] -- restart generation at specified map (after crash, implies resume)\n" );
	Msg( "  [ -condebug ] -- prepend console.log entries with mapname or engine if not in a map\n" );
	Msg( "  [ +map mapname ] -- generate devshots for specified map and exit after that map\n" );
}

void DevShotGenerator_Init()
{
	// check for devshot generation
	if ( CommandLine()->FindParm("-makedevshots") )
	{
		bool usemaplistfile = false;
		if ( CommandLine()->FindParm("-usedevshotsfile") )
		{
			usemaplistfile = true;
		}
		DevShotGenerator().EnableDevShotGeneration( usemaplistfile );
	}
}

void DevShotGenerator_Shutdown()
{
	DevShotGenerator().Shutdown();
}

void DevShotGenerator_BuildMapList()
{
	DevShotGenerator().BuildMapList();
}

CDevShotGenerator g_DevShotGenerator;
CDevShotGenerator &DevShotGenerator()
{
	return g_DevShotGenerator;
}

void CL_DevShots_NextMap()
{
	DevShotGenerator().NextMap();
}

static ConCommand devshots_nextmap( "devshots_nextmap", CL_DevShots_NextMap, "Used by the devshots system to go to the next map in the devshots maplist." );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDevShotGenerator::CDevShotGenerator() 
{
	m_bUsingMapList = false;
	m_bDevShotsEnabled = false;
	m_iCurrentMap = 0;
}

void CDevShotGenerator::BuildMapList()
{
	if ( !m_bDevShotsEnabled )
		return;

	DevShotGenerator_Usage();

	// Get the maplist file, if any
	const char *pMapFile = NULL;
	CommandLine()->CheckParm( "-usedevshotsfile", &pMapFile );

	// Build the map list
	if ( !BuildGeneralMapList( &m_Maps, CommandLine()->FindParm("-usedevshotsfile") != 0, pMapFile, "devshots", &m_iCurrentMap ) )
	{
		m_bDevShotsEnabled = false;
	}
}

void CDevShotGenerator::NextMap()
{
	if ( !m_bDevShotsEnabled )
		return;

	//Msg("DEVSHOTS: Nextmap command received.\n");

	if (m_Maps.IsValidIndex(m_iCurrentMap))
	{
		//Msg("DEVSHOTS: Switching to %s (%d).\n", m_Maps[m_iCurrentMap].name, m_iCurrentMap );
		CFmtStr str("map %s\n", m_Maps[m_iCurrentMap].name);
		Cbuf_AddText( str.Access() );

		++m_iCurrentMap;
	}
	else
	{
		//Msg("DEVSHOTS: Finished on map %d.\n", m_iCurrentMap);

		// no more levels, just quit
		Cbuf_AddText( "quit\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: initializes the object to enable dev shot generation
//-----------------------------------------------------------------------------
void CDevShotGenerator::EnableDevShotGeneration( bool usemaplistfile )
{
	m_bUsingMapList = usemaplistfile;
	m_bDevShotsEnabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: starts the first map
//-----------------------------------------------------------------------------
void CDevShotGenerator::StartDevShotGeneration()
{
	BuildMapList();

	CFmtStr str("map %s\n", m_Maps[m_iCurrentMap].name);
	Cbuf_AddText( str.Access() );
	++m_iCurrentMap;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDevShotGenerator::Shutdown()
{
}


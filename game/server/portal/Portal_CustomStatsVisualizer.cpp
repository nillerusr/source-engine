//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "filesystem.h"
#include "portal_gamestats.h"
#include "util_shared.h"
#include "prop_portal_shared.h"
#include "utlstring.h"


#ifdef PORTAL_GAMESTATS_VERBOSE //this shouldn't be in release versions, and most loading/saving code is only enabled with this defined

static ConVar portalstats_visdeaths( "portalstats_visdeaths", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
static ConVar portalstats_visjumps( "portalstats_visjumps", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
static ConVar portalstats_visusages( "portalstats_visusages", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
static ConVar portalstats_visstucks( "portalstats_visstucks", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
static ConVar portalstats_visplacement( "portalstats_visplacement", "1", FCVAR_REPLICATED | FCVAR_CHEAT );

class CPortalGameStatsVisualizer : public CPortalGameStats, public CAutoGameSystemPerFrame
{
public:
	float m_fRefreshTimer;
	void FrameUpdatePostEntityThink();
	void LevelInitPreEntity();
	void PrepareLevelData( void );

	CUtlVector<QAngle> m_PortalPlacementAngles;

	CPortalGameStatsVisualizer( void )
		: m_fRefreshTimer( 0.0f )
	{ };
};
CPortalGameStatsVisualizer s_GameStatsVisualizer;

void CPortalGameStatsVisualizer::LevelInitPreEntity()
{
	m_fRefreshTimer = gpGlobals->curtime;
	PrepareLevelData();
}

void CPortalGameStatsVisualizer::PrepareLevelData( void )
{
	m_pCurrentMapStats = FindOrAddMapStats( STRING( gpGlobals->mapname ) );

	m_PortalPlacementAngles.SetSize( m_pCurrentMapStats->m_pPlacements->Count() );

	for( int i = m_pCurrentMapStats->m_pPlacements->Count(); --i >= 0; )
	{
		Portal_Gamestats_LevelStats_t::PortalPlacement_t &PlacementStat = m_pCurrentMapStats->m_pPlacements->Element( i );

		trace_t tr;

		// Trace to see where the portal hit
		Vector vDirection = PlacementStat.ptPlacementPosition - PlacementStat.ptPlayerFiredFrom;

		UTIL_TraceLine( PlacementStat.ptPlayerFiredFrom, PlacementStat.ptPlayerFiredFrom + (vDirection * 100000.0f), MASK_SHOT_PORTAL, NULL, &tr );

		Vector vUp( 0.0f, 0.0f, 1.0f );
		if( ( tr.plane.normal.x > -0.001f && tr.plane.normal.x < 0.001f ) && ( tr.plane.normal.y > -0.001f && tr.plane.normal.y < 0.001f ) )
		{
			//plane is a level floor/ceiling
			vUp = vDirection;
		}

		VectorAngles( tr.plane.normal, vUp, m_PortalPlacementAngles[i] );
	}
}

#define PORTALSTATSVISUALIZER_REFRESHTIME 1.0f
#define PORTALSTATSVISUALIZER_DISPLAYTIME (PORTALSTATSVISUALIZER_REFRESHTIME + 0.05f)

void CPortalGameStatsVisualizer::FrameUpdatePostEntityThink()
{
	if( m_fRefreshTimer > gpGlobals->curtime )
		return;

	//refresh now
	m_fRefreshTimer = gpGlobals->curtime + PORTALSTATSVISUALIZER_REFRESHTIME;

	static const Vector vPlayerBoxMins( -16.0f, -16.0f, 0.0f ), vPlayerBoxMaxs( 16.0f, 16.0f, 72.0f ), vPlayerTextOffset( 0.0f, 0.0f, 36.0f );
	static const Vector vSmallBoxMins( -7.5f, -7.5f, -7.5f ), vSmallBoxMaxs( 7.5f, 7.5f, 7.5f );

	if( portalstats_visdeaths.GetBool() )
	{
		for( int i = m_pCurrentMapStats->m_pDeaths->Count(); --i >= 0; )
		{
			Portal_Gamestats_LevelStats_t::PlayerDeaths_t &DeathStat = m_pCurrentMapStats->m_pDeaths->Element( i );
			NDebugOverlay::Box( DeathStat.ptPositionOfDeath, vPlayerBoxMins, vPlayerBoxMaxs, 255, 0, 0, 100, PORTALSTATSVISUALIZER_DISPLAYTIME );
			NDebugOverlay::Text( DeathStat.ptPositionOfDeath + vPlayerTextOffset, DeathStat.szAttackerClassName, true, PORTALSTATSVISUALIZER_DISPLAYTIME );
		}
	}

	if( portalstats_visjumps.GetBool() )
	{
		for( int i = m_pCurrentMapStats->m_pJumps->Count(); --i >= 0; )
		{
			Portal_Gamestats_LevelStats_t::JumpEvent_t &JumpStat = m_pCurrentMapStats->m_pJumps->Element( i );

			NDebugOverlay::Box( JumpStat.ptPlayerPositionAtJumpStart, vPlayerBoxMins, vPlayerBoxMaxs, 0, 255, 0, 100, PORTALSTATSVISUALIZER_DISPLAYTIME );
			NDebugOverlay::Line( JumpStat.ptPlayerPositionAtJumpStart + vPlayerTextOffset, JumpStat.ptPlayerPositionAtJumpStart + vPlayerTextOffset + (JumpStat.vPlayerVelocityAtJumpStart), 0, 255, 0, false, PORTALSTATSVISUALIZER_REFRESHTIME );
		}
	}

	if( portalstats_visusages.GetBool() )
	{
		for( int i = m_pCurrentMapStats->m_pUseEvents->Count(); --i >= 0; )
		{
			Portal_Gamestats_LevelStats_t::PlayerUse_t &UseEvent = m_pCurrentMapStats->m_pUseEvents->Element( i );
			
			NDebugOverlay::Box( UseEvent.ptTraceStart, vSmallBoxMins, vSmallBoxMaxs, 0, 0, 255, 100, PORTALSTATSVISUALIZER_DISPLAYTIME );
			NDebugOverlay::Line( UseEvent.ptTraceStart, UseEvent.ptTraceStart + (UseEvent.vTraceDelta * 100.0f), 0, 0, 255, false, PORTALSTATSVISUALIZER_DISPLAYTIME );
			NDebugOverlay::Text( UseEvent.ptTraceStart, UseEvent.szUseEntityClassName, true, PORTALSTATSVISUALIZER_DISPLAYTIME );
		}
	}

	if( portalstats_visstucks.GetBool() )
	{
		for( int i = m_pCurrentMapStats->m_pStuckSpots->Count(); --i >= 0; )
		{
			Portal_Gamestats_LevelStats_t::StuckEvent_t &StuckEvent = m_pCurrentMapStats->m_pStuckSpots->Element( i );

			NDebugOverlay::Box( StuckEvent.ptPlayerPosition, vPlayerBoxMins, vPlayerBoxMaxs, 255, 0, 255, 100, PORTALSTATSVISUALIZER_DISPLAYTIME );
		}
	}

	if( portalstats_visplacement.GetBool() )
	{
		Assert( m_pCurrentMapStats->m_pPlacements->Count() == m_PortalPlacementAngles.Count() );

		for( int i = m_pCurrentMapStats->m_pPlacements->Count(); --i >= 0; )
		{
			Portal_Gamestats_LevelStats_t::PortalPlacement_t &PlacementStat = m_pCurrentMapStats->m_pPlacements->Element( i );
			
			unsigned char brightnessval;
			if( PlacementStat.iSuccessCode == 0 ) 
				 brightnessval = 255;//worked
			else
				 brightnessval = 64;//failed
				
			NDebugOverlay::BoxAngles( PlacementStat.ptPlacementPosition, CProp_Portal_Shared::vLocalMins, CProp_Portal_Shared::vLocalMaxs, m_PortalPlacementAngles[i], 0, brightnessval, brightnessval, 100, PORTALSTATSVISUALIZER_DISPLAYTIME );
			NDebugOverlay::Box( PlacementStat.ptPlayerFiredFrom, vSmallBoxMins, vSmallBoxMaxs, 0, brightnessval, brightnessval, 100, PORTALSTATSVISUALIZER_DISPLAYTIME );
			NDebugOverlay::Line( PlacementStat.ptPlayerFiredFrom, PlacementStat.ptPlacementPosition, 0, brightnessval, brightnessval, false, PORTALSTATSVISUALIZER_DISPLAYTIME );
		}
	}
}


static CUtlVector<CUtlString> s_PortalStatsDataFileList;
static void PortalStats_UpdateFileList( void )
{
	s_PortalStatsDataFileList.RemoveAll();
	FileFindHandle_t fileHandle;
	const char *pszFileName = filesystem->FindFirst( "customstats/*.dat", &fileHandle );

	while( pszFileName )
	{
		// Skip it if it's a directory
		if( !filesystem->FindIsDirectory( fileHandle ) )
		{
			char szRelativeFileName[_MAX_PATH];
			Q_snprintf( szRelativeFileName, sizeof( szRelativeFileName ), "customstats/%s", pszFileName );

			// Only load files from the current mod's directory hierarchy
			if( filesystem->FileExists( szRelativeFileName, "MOD" ) )
			{
				int index = s_PortalStatsDataFileList.AddToTail();
				s_PortalStatsDataFileList[index].Set( pszFileName );
			}
		}

		pszFileName = filesystem->FindNext( fileHandle );
	}

	filesystem->FindClose( fileHandle );
}

static int PortalStats_LoadFile_f_CompletionFunc( char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	PortalStats_UpdateFileList();

	int iRetCount = MIN( s_PortalStatsDataFileList.Count(), COMMAND_COMPLETION_MAXITEMS );
	for( int i = 0; i != iRetCount; ++i )
	{
		Q_snprintf( commands[i], COMMAND_COMPLETION_ITEM_LENGTH, "%s %s", partial, s_PortalStatsDataFileList[i].Get() );
	}	

	return iRetCount;
}

static char s_szCurrentlyLoadedStatsFile[256] = "";

static void PortalStats_LoadFileHelper( const char *szFileName )
{
	CUtlBuffer statsfileContents;
	char szRelativeName[256];

	Q_snprintf( szRelativeName, sizeof( szRelativeName ), "customstats/%s", szFileName );

	if( filesystem->ReadFile( szRelativeName, "MOD", statsfileContents ) )
	{
		Q_strncpy( s_szCurrentlyLoadedStatsFile, szFileName, sizeof( s_szCurrentlyLoadedStatsFile ) );

		s_GameStatsVisualizer.Clear();
		s_GameStatsVisualizer.LoadCustomDataFromBuffer( statsfileContents );
		s_GameStatsVisualizer.PrepareLevelData();
		s_GameStatsVisualizer.m_fRefreshTimer = 0.0f; //start drawing new data right away
	}
}

static void PortalStats_LoadFile_f( const CCommand &args )
{
	if( args.ArgC() > 1 )
	{
		PortalStats_LoadFileHelper( args[1] );
	}
}

static void PortalStats_LoadNextFile_f( void )
{
	PortalStats_UpdateFileList();
	if( s_PortalStatsDataFileList.Count() == 0 )
		return;

	int i;
	for( i = s_PortalStatsDataFileList.Count(); --i >= 0; )
	{
		if( Q_stricmp( s_PortalStatsDataFileList[i].Get(), s_szCurrentlyLoadedStatsFile ) == 0 )
			break;
	}

	if( i < 0 )
	{
		//currently loaded file not found, just load the first
		PortalStats_LoadFileHelper( s_PortalStatsDataFileList[0] );
	}
	else
	{
		++i;
		if( i == s_PortalStatsDataFileList.Count() )
		{
			DevMsg( "PortalStats_LoadNextFile looping to first file in directory.\n" );
			NDebugOverlay::ScreenText( 0.3f, 0.5f, "PortalStats_LoadNextFile looping to first file in directory.", 255, 255, 255, 255, 2.0f );
			i = 0;
		}
		
		PortalStats_LoadFileHelper( s_PortalStatsDataFileList[i] );
	}
}

static void PortalStats_LoadPrevFile_f( void )
{
	PortalStats_UpdateFileList();
	if( s_PortalStatsDataFileList.Count() == 0 )
		return;

	int i;
	for( i = s_PortalStatsDataFileList.Count(); --i >= 0; )
	{
		if( Q_stricmp( s_PortalStatsDataFileList[i].Get(), s_szCurrentlyLoadedStatsFile ) == 0 )
			break;
	}

	if( i < 0 )
	{
		//currently loaded file not found, just load the first
		PortalStats_LoadFileHelper( s_PortalStatsDataFileList[0] );
	}
	else
	{
		--i;
		if( i < 0 )
		{
			i = s_PortalStatsDataFileList.Count() - 1;
			DevMsg( "PortalStats_LoadPrevFile looping to last file in directory.\n" );
			NDebugOverlay::ScreenText( 0.3f, 0.5f, "PortalStats_LoadPrevFile looping to last file in directory.", 255, 255, 255, 255, 2.0f );
		}

		PortalStats_LoadFileHelper( s_PortalStatsDataFileList[i] );
	}
}

//static ConCommand Portal_LoadCustomStatsFile("portal_loadcustomstatsfile", Portal_LoadCustomStatsFile_f, "Load a custom stats file for visualizing.", FCVAR_DONTRECORD, Portal_LoadCustomStatsFile_f_CompletionFunc );
static ConCommand PortalStats_LoadFile("portalstats_loadfile", PortalStats_LoadFile_f, "Load a custom stats file for visualizing.", FCVAR_DONTRECORD, PortalStats_LoadFile_f_CompletionFunc );
static ConCommand PortalStats_LoadNextFile("portalstats_loadnextfile", PortalStats_LoadNextFile_f, "Load the next custom stats file for visualizing.", FCVAR_DONTRECORD );
static ConCommand PortalStats_LoadPrevFile("portalstats_loadprevfile", PortalStats_LoadPrevFile_f, "Load the next custom stats file for visualizing.", FCVAR_DONTRECORD );











#endif //#ifdef PORTAL_GAMESTATS_VERBOSE


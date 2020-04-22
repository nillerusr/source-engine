//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cl_performancemanager.h"
#include "cl_replaymanager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

#define PERFORMANCE_INDEX_VERSION	0

//----------------------------------------------------------------------------------------

CReplayPerformanceManager::CReplayPerformanceManager()
{
}

CReplayPerformanceManager::~CReplayPerformanceManager()
{
}

void CReplayPerformanceManager::Init()
{
	g_pFullFileSystem->CreateDirHierarchy( Replay_va( "%s%s", CL_GetBasePath(), SUBDIR_PERFORMANCES ) );
}

void CReplayPerformanceManager::DeletePerformance( CReplayPerformance *pPerformance )
{
	// Delete the performance file
	const char *pFullFilename = pPerformance->GetFullPerformanceFilename();
	g_pFullFileSystem->RemoveFile( pFullFilename );

	// Remove from replay list
	CReplay *pOwnerReplay = pPerformance->m_pReplay;
	pOwnerReplay->m_vecPerformances.FindAndRemove( pPerformance ); // This can fail if the replay doesn't own the performance yet - which is no problem.

	// Free
	delete pPerformance;

	CL_GetReplayManager()->FlagReplayForFlush( pOwnerReplay, true );
}

const char *CReplayPerformanceManager::GetRelativePath() const
{
	return Replay_va( "%s%s%c", CL_GetRelativeBasePath(), SUBDIR_PERFORMANCES, CORRECT_PATH_SEPARATOR );
}

const char *CReplayPerformanceManager::GetFullPath() const
{
	return Replay_va( "%s%c%s", g_pEngine->GetGameDir(), CORRECT_PATH_SEPARATOR, GetRelativePath() );
}

CReplayPerformance *CReplayPerformanceManager::CreatePerformance( CReplay *pReplay )
{
	return new CReplayPerformance( pReplay );
}

const char *CReplayPerformanceManager::GeneratePerformanceFilename( CReplay *pReplay )
{
	static char s_szBaseFilename[ MAX_OSPATH ];
	char szIdealBaseFilename[ MAX_OSPATH ];
	V_strcpy_safe( szIdealBaseFilename, Replay_va( "replay_%i_edit", (int)pReplay->GetHandle() ) );
	Replay_GetFirstAvailableFilename( s_szBaseFilename, sizeof( s_szBaseFilename ), szIdealBaseFilename, "." GENERIC_FILE_EXTENSION,
		CL_GetPerformanceManager()->GetRelativePath(), pReplay->GetPerformanceCount() );
	return s_szBaseFilename;
}

//----------------------------------------------------------------------------------------

//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "shared_replaycontext.h"
#include "replay/shared_defs.h"
#include "replay/replayutils.h"
#include "baserecordingsession.h"
#include "baserecordingsessionblock.h"
#include "baserecordingsessionmanager.h"
#include "baserecordingsessionblockmanager.h"
#include "thinkmanager.h"
#include "filesystem.h"
#include "errorsystem.h"

#undef Yield
#include "vstdlib/jobthread.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CSharedReplayContext::CSharedReplayContext( IReplayContext *pOwnerContext )
:	m_pOwnerContext( pOwnerContext ),
	m_pRecordingSessionManager( NULL ),
	m_pRecordingSessionBlockManager( NULL ),
	m_pErrorSystem( NULL ),
	m_pThreadPool( NULL ),
	m_bInit( false )
{
}

CSharedReplayContext::~CSharedReplayContext()
{
	delete m_pRecordingSessionManager;
	delete m_pRecordingSessionBlockManager;
	delete m_pErrorSystem;
	delete m_pThreadPool;
}

bool CSharedReplayContext::Init( CreateInterfaceFn fnFactory )
{
	m_strRelativeBasePath.Format(
		"%s%c%s%c",
		SUBDIR_REPLAY,
		CORRECT_PATH_SEPARATOR,
		m_strSubDir.Get(),
		CORRECT_PATH_SEPARATOR
	);

	m_strBasePath.Format(
		"%s%c%s",
		g_pEngine->GetGameDir(),
		CORRECT_PATH_SEPARATOR,
		m_strRelativeBasePath.Get()
	);

	// Owning context should have initialized these by now
	// NOTE: Session manager init must come after block manager init since session manager
	// assumes all blocks have been loaded.
	// 
	m_pRecordingSessionBlockManager->Init();
	m_pRecordingSessionManager->Init();

	if ( !InitThreadPool() )
		return false;

	m_bInit = true;

	return true;
}

bool CSharedReplayContext::InitThreadPool()
{
	// Create thread pool
	Log( "Replay: Creating thread pool..." );
	IThreadPool *pThreadPool = CreateThreadPool();
	if ( !pThreadPool )
	{
		Log( "failed!\n" );
		return false;
	}
	Log( "succeeded.\n" );

	// Jon says: The client only really needs a single "ReplayContext" thread, so that the replay editor can write
	//  data asynchronously. The game server does in fact require 4 threads, and can be configured to use more
	//	via the replay_max_publish_threads convar.
	int nMaxThreads = 1;

	if ( g_pEngine->IsDedicated() )
	{
		// Use the convar for max threads on servers
		extern ConVar replay_max_publish_threads;
		nMaxThreads = replay_max_publish_threads.GetInt();
	}

	// Start thread pool
	Log( "Replay: Starting thread pool with %i threads...", nMaxThreads );
	if ( !pThreadPool->Start( ThreadPoolStartParams_t( true, nMaxThreads ), "ReplayContext" ) )
	{
		Log( "failed!\n" );
		return false;
	}
	Log( "succeeded.\n" );

	m_pThreadPool = pThreadPool;

	return true;
}

void CSharedReplayContext::Shutdown()
{
	m_pRecordingSessionBlockManager->Shutdown();
	m_pRecordingSessionManager->Shutdown();
	m_pThreadPool->Stop();
}

void CSharedReplayContext::Think()
{
}

const char *CSharedReplayContext::GetRelativeBaseDir() const
{
	return m_strRelativeBasePath.Get();
}

const char *CSharedReplayContext::GetBaseDir() const
{
	return m_strBasePath.Get();
}

const char *CSharedReplayContext::GetReplaySubDir() const
{
	return m_strSubDir.Get();
}

void CSharedReplayContext::EnsureDirHierarchy()
{
	g_pFullFileSystem->CreateDirHierarchy( m_strBasePath.Get() );
}

//----------------------------------------------------------------------------------------

bool RunJobToCompletion( IThreadPool *pThreadPool, CJob *pJob )
{
	pThreadPool->AddJob( pJob );
	pJob->WaitForFinish();
	return pJob->Executed();
}

//----------------------------------------------------------------------------------------

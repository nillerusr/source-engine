//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef CL_REPLAYCONTEXT_H
#define CL_REPLAYCONTEXT_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "shared_replaycontext.h"
#include "replay/iclientreplaycontext.h"
#include "igameevents.h"
#include "cl_recordingsessionmanager.h"
#include "cl_replaymoviemanager.h"
#include "cl_recordingsessionblockmanager.h"
#include "cl_performancemanager.h"
#include "cl_performancecontroller.h"
#include "errorsystem.h"

//----------------------------------------------------------------------------------------

class IReplayMovieRenderer;
class CScreenshotManager;
class CReplayManager;
class CReplayMovieManager;
class CClientRecordingSessionManager;
class CReplayPerformanceManager;
class CHttpDownloader;
class CSessionBlockDownloader;
class CClientRecordingSession;
class CPerformanceController;
class CRenderQueue;

//----------------------------------------------------------------------------------------

class CClientReplayContext : public IClientReplayContext,
							 public IErrorReporter
{
public:
	LINK_TO_SHARED_REPLAYCONTEXT_IMP();

	CClientReplayContext();
	~CClientReplayContext();

	virtual bool				Init( CreateInterfaceFn fnFactory );
	virtual void				Shutdown();

	virtual void				Think();	// Called by engine

	bool						ReconstructReplayIfNecessary( CReplay *pReplay );
	void						DisableReplayOnClient( bool bDisable );
	bool						IsClientSideReplayDisabled() const		{ return m_bClientSideReplayDisabled; }

	//
	// IClientReplayContext
	//
	virtual CReplay				*GetReplay( ReplayHandle_t hReplay );
	virtual IReplayManager		*GetReplayManager();
	virtual IReplayMovieRenderer	*GetMovieRenderer();
	virtual IReplayMovieManager	*GetMovieManager();
	virtual IReplayScreenshotManager	*GetScreenshotManager();
	virtual IReplayPerformanceManager	*GetPerformanceManager();
	virtual IReplayPerformanceController *GetPerformanceController();
	virtual IReplayRenderQueue			*GetRenderQueue();
	virtual void				SetMovieRenderer( IReplayMovieRenderer *pMovieRenderer );
	virtual void				OnSignonStateFull();
	virtual void				OnClientSideDisconnect();
	virtual void				PlayReplay( ReplayHandle_t hReplay, int iPerformance, bool bPlaySound );
	virtual void				OnPlayerSpawn();
	virtual void				OnPlayerClassChanged();
	virtual void				GetPlaybackTimes( float &flOutTime, float &flOutLength, const CReplay *pReplay, const CReplayPerformance *pPerformance );
	virtual uint64				GetServerSessionId( ReplayHandle_t hReplay );
	virtual void				CleanupUnneededBlocks();

	//
	// IErrorReporter
	//
	virtual void				ReportErrorsToUser( wchar_t *pErrorText );

	void						TestDownloader( const char *pURL );

	CReplayManager				*m_pReplayManager;
	CScreenshotManager			*m_pScreenshotManager;
	IReplayMovieRenderer		*m_pMovieRenderer;
	CReplayMovieManager			*m_pMovieManager;
	CReplayPerformanceManager	*m_pPerformanceManager;
	CPerformanceController		*m_pPerformanceController;
	CSessionBlockDownloader		*m_pSessionBlockDownloader;
	CRenderQueue				*m_pRenderQueue;
	
	CHttpDownloader				*m_pTestDownloader;

private:
	void						DebugThink();
	void						ReplayThink();

	bool						m_bClientSideReplayDisabled;
};

//----------------------------------------------------------------------------------------

extern CClientReplayContext *g_pClientReplayContextInternal;

//----------------------------------------------------------------------------------------

//
// Helpers
//
inline const char *CL_GetBasePath()
{
	return g_pClientReplayContextInternal->m_pShared->m_strBasePath;
}

inline const char *CL_GetRelativeBasePath()
{
	return g_pClientReplayContextInternal->m_pShared->m_strRelativeBasePath.Get();
}

inline CReplayManager *CL_GetReplayManager()
{
	return g_pClientReplayContextInternal->m_pReplayManager;
}

inline CClientRecordingSessionBlockManager *CL_GetRecordingSessionBlockManager()
{
	return static_cast< CClientRecordingSessionBlockManager * >( g_pClientReplayContextInternal->GetRecordingSessionBlockManager() );
}

inline CScreenshotManager *CL_GetScreenshotManager()
{
	return g_pClientReplayContextInternal->m_pScreenshotManager;
}

inline IReplayMovieRenderer *CL_GetMovieRenderer()
{
	return g_pClientReplayContextInternal->m_pMovieRenderer;
}

inline CReplayMovieManager *CL_GetMovieManager()
{
	return g_pClientReplayContextInternal->m_pMovieManager;
}

inline const char *CL_GetReplayBaseDir()
{
	return g_pClientReplayContextInternal->m_pShared->m_strBasePath;
}

inline CErrorSystem *CL_GetErrorSystem()
{
	return g_pClientReplayContextInternal->m_pShared->m_pErrorSystem;
}

inline CSessionBlockDownloader *CL_GetSessionBlockDownloader()
{
	return g_pClientReplayContextInternal->m_pSessionBlockDownloader;
}

inline CReplayPerformanceManager *CL_GetPerformanceManager()
{
	return g_pClientReplayContextInternal->m_pPerformanceManager;
}

inline CPerformanceController *CL_GetPerformanceController()
{
	return g_pClientReplayContextInternal->m_pPerformanceController;
}

inline IThreadPool *CL_GetThreadPool()
{
	return g_pClientReplayContextInternal->m_pShared->m_pThreadPool;
}

inline CRenderQueue *CL_GetRenderQueue()
{
	return g_pClientReplayContextInternal->m_pRenderQueue;
}

//----------------------------------------------------------------------------------------

CClientRecordingSessionManager *CL_GetRecordingSessionManager();
CClientRecordingSession *CL_GetRecordingSessionInProgress();

//----------------------------------------------------------------------------------------

#endif	// CL_REPLAYCONTEXT_H

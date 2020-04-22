//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef SV_REPLAYCONTEXT_H
#define SV_REPLAYCONTEXT_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "shared_replaycontext.h"
#include "replay/iserverreplaycontext.h"
#include "sv_recordingsessionmanager.h"
#include "sv_recordingsessionblockmanager.h"
#include "errorsystem.h"

//----------------------------------------------------------------------------------------

class CSessionRecorder;
class CBaseRecordingSessionBlock;
class IRecordingSessionManager;
class IThreadPool;
class CFileserverCleaner;

//----------------------------------------------------------------------------------------

class CServerReplayContext : public IServerReplayContext,
							 public IErrorReporter
{
public:
	LINK_TO_SHARED_REPLAYCONTEXT_IMP();

	CServerReplayContext();
	~CServerReplayContext();

	virtual bool			Init( CreateInterfaceFn fnFactory );
	virtual void			Shutdown();

	virtual void			Think();	// Called by engine

	virtual void			OnPublishFailed();
	void					DoSanityCheckNow();

	void					UpdateFileserverIPFromHostname( const char *pHostname );
	void					UpdateFileserverProxyIPFromHostname( const char *pHostname );

	//
	// IErrorReporter
	//
	virtual void			ReportErrorsToUser( wchar_t *pErrorText );

	//
	// IServerReplayContext
	//
	virtual void			FlagForConVarSanityCheck();
	virtual IGameEvent		*CreateReplaySessionInfoEvent();
	virtual	IReplaySessionRecorder	*GetSessionRecorder();
	virtual const char		*GetLocalFileServerPath() const;
	virtual void			CreateSessionOnClient( int nClientSlot );

	const char				*GetServerSubDirName() const;

	CSessionRecorder		*m_pSessionRecorder;
	CFileserverCleaner		*m_pFileserverCleaner;

	char					m_szFileserverIP[16];		// Fileserver's IP, cached any time "replay_fileserver_offload_hostname" is modified.
	char					m_szFileserverProxyIP[16];	// Proxy's IP, cached any time "replay_fileserver_offload_proxy_host" is modified.

private:
	void					CleanTmpDir();
	void					ConVarSanityThink();

	float					m_flConVarSanityCheckTime; 
	bool					m_bShouldAbortRecording;
};

//----------------------------------------------------------------------------------------

extern CServerReplayContext *g_pServerReplayContext;

//----------------------------------------------------------------------------------------

inline CServerRecordingSessionManager *SV_GetRecordingSessionManager()
{
	return static_cast< CServerRecordingSessionManager * >( g_pServerReplayContext->GetRecordingSessionManager() );
}

inline CServerRecordingSessionBlockManager *SV_GetRecordingSessionBlockManager()
{
	return static_cast< CServerRecordingSessionBlockManager * >( g_pServerReplayContext->GetRecordingSessionBlockManager() );
}

inline CSessionRecorder *SV_GetSessionRecorder()
{
	return g_pServerReplayContext->m_pSessionRecorder;
}

inline CFileserverCleaner *SV_GetFileserverCleaner()
{
	return g_pServerReplayContext->m_pFileserverCleaner;
}

inline const char *SV_GetBasePath()
{
	return g_pServerReplayContext->m_pShared->m_strBasePath;
}

inline IThreadPool *SV_GetThreadPool()
{
	return g_pServerReplayContext->m_pShared->m_pThreadPool;
}

inline char const *SV_GetFileserverIP()
{
	return g_pServerReplayContext->m_szFileserverIP;
}

inline char const *SV_GetFileserverProxyIP()
{
	return g_pServerReplayContext->m_szFileserverProxyIP;
}

CServerRecordingSession *SV_GetRecordingSessionInProgress();
const char	*SV_GetTmpDir();	// Get "replay/server/tmp/"
bool		SV_IsOffloadingEnabled();

class CJob;
bool		SV_RunJobToCompletion( CJob *pJob );	// NOTE: Adds to thread pool first

//----------------------------------------------------------------------------------------

#endif	// SV_REPLAYCONTEXT_H

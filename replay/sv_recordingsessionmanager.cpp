//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "sv_recordingsessionmanager.h"
#include "baserecordingsessionblock.h"
#include "sv_replaycontext.h"
#include "sv_recordingsession.h"
#include "replaysystem.h"
#include "KeyValues.h"
#include "replay/replayutils.h"
#include "filesystem.h"
#include "iserver.h"
#include "sv_filepublish.h"
#include <time.h>
#include "vprof.h"
#include "sv_fileservercleanup.h"
#include "sv_sessionrecorder.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

#define VERSION_SERVERRECORDINGSESSIONMANAGER	0

//----------------------------------------------------------------------------------------

CServerRecordingSessionManager::CServerRecordingSessionManager( IReplayContext *pContext )
:	CBaseRecordingSessionManager( pContext ),
	m_flNextScheduledCleanup( 0.0f ),
	m_bOffload( false )
{
}

CServerRecordingSessionManager::~CServerRecordingSessionManager()
{
}

const char *CServerRecordingSessionManager::GetNewSessionName() const
{
	// Setup filename for the session
	tm today; VCRHook_LocalTime( &today );
	return Replay_va(
		"%04i%02i%02i-%02i%02i%02i-%s",
		1900 + today.tm_year, today.tm_mon+1, today.tm_mday, 
		today.tm_hour, today.tm_min, today.tm_sec,
		g_pEngine->GetGameServer()->GetMapName()
	);
}

void CServerRecordingSessionManager::Think()
{
	VPROF_BUDGET( "CServerRecordingSessionManager::Think", VPROF_BUDGETGROUP_REPLAY );

	BaseClass::Think();
}

CBaseRecordingSession *CServerRecordingSessionManager::Create()
{
	return new CServerRecordingSession( m_pContext );
}

int CServerRecordingSessionManager::GetVersion() const
{
	return VERSION_SERVERRECORDINGSESSIONMANAGER;
}

IReplayContext *CServerRecordingSessionManager::GetReplayContext() const
{
	return g_pServerReplayContext;
}

bool CServerRecordingSessionManager::CanDeleteSession( ReplayHandle_t hSession ) const
{
	const CBaseRecordingSession *pSession = FindSession( hSession );	AssertMsg( pSession, "The session should always be valid here!" );
	return !pSession->IsLocked();
}

void CServerRecordingSessionManager::OnAllSessionsDeleted()
{
	SV_GetFileserverCleaner()->DoCleanAsynchronous();
}

CBaseRecordingSession *CServerRecordingSessionManager::OnSessionStart( int nCurrentRecordingStartTick, const char *pSessionName )
{
	CBaseRecordingSession *pResult = BaseClass::OnSessionStart( nCurrentRecordingStartTick, pSessionName );

	// Cache offload state
	m_bOffload = false;

	return pResult;
}

void CServerRecordingSessionManager::OnSessionEnd()
{
	BaseClass::OnSessionEnd();

	extern ConVar replay_fileserver_autocleanup;
	if ( replay_fileserver_autocleanup.GetBool() )
	{
		// Cleanup expired sessions/blocks now
		SV_DoFileserverCleanup( false, g_pBlockSpewer );
	}
}

//----------------------------------------------------------------------------------------

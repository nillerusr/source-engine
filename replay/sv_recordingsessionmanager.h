//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef SV_PUBLISHMANAGER_H
#define SV_PUBLISHMANAGER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "baserecordingsessionmanager.h"

//----------------------------------------------------------------------------------------

class CServerRecordingSession;

//----------------------------------------------------------------------------------------

//
// Manages and serializes all replay recording session data on the server
//
class CServerRecordingSessionManager : public CBaseRecordingSessionManager
{
	typedef CBaseRecordingSessionManager BaseClass;

public:
	CServerRecordingSessionManager( IReplayContext *pContext );
	~CServerRecordingSessionManager();

	void					Think();

	const char				*GetNewSessionName() const;

	virtual CBaseRecordingSession *OnSessionStart( int nCurrentRecordingStartTick, const char *pSessionName );
	virtual void			OnSessionEnd();

	void					EnableCleanupOnSessionEnd( bool bState );

	// Offload session data to external fileserver?  Cached once per session based on replay_fileserver_offload_enable.
	bool					ShouldOffload() const		{ return m_bOffload; }

protected:
	//
	// CGenericPersistentManager
	//
	virtual CBaseRecordingSession	*Create();
	virtual int				GetVersion() const;
	virtual bool			ShouldSerializeIndexWithFullPath() { return false; } // On the server, write one file per session
	virtual IReplayContext	*GetReplayContext() const;

	virtual bool			CanDeleteSession( ReplayHandle_t hSession ) const;
	virtual bool			ShouldUnloadSessions() const { return true; }
	virtual void			OnAllSessionsDeleted();

private:
	float					m_flNextScheduledCleanup;
	bool					m_bOffload;
};

//----------------------------------------------------------------------------------------

#endif // SV_PUBLISHMANAGER_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef CL_RECORDINGSESSIONMANAGER_H
#define CL_RECORDINGSESSIONMANAGER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "baserecordingsessionmanager.h"
#include "igameevents.h"
#include "replaysystem.h"
#include "replay/shared_defs.h"

//----------------------------------------------------------------------------------------

//
// Manages and serializes all replay recording session data on the client
//
class CClientRecordingSessionManager : public CBaseRecordingSessionManager,
									   public IGameEventListener2
{
	typedef CBaseRecordingSessionManager BaseClass;

public:
	CClientRecordingSessionManager( IReplayContext *pContext );
	~CClientRecordingSessionManager();

	virtual bool			Init();

	void					CleanupUnneededBlocks();

	virtual CBaseRecordingSession *OnSessionStart( int nCurrentRecordingStartTick, const char *pSessionName );
	virtual void			OnSessionEnd();

	void					ClearServerRecordingState()		{ m_ServerRecordingState.Clear(); }
	void					OnReplayDeleted( CReplay *pReplay );
	void					OnReplaysLoaded();

	//
	// CGenericPersistentManager
	//
	virtual CBaseRecordingSession *Create();

	struct ServerRecordingState_t
	{
		ServerRecordingState_t() { Clear(); }
		void Clear() { m_strSessionName = ""; m_nDumpInterval = m_nCurrentBlock = m_nStartTick = 0; }

		CUtlString		m_strSessionName;		// Name of current recording session
		int				m_nDumpInterval;		// The interval at which the server is dumping partial replays
		int				m_nCurrentBlock;		// Current session block being written to on the server (approximation - may be ahead but never behind)
		int				m_nStartTick;			// The tick on the server when the session began - used to calculate an adjusted spawn tick on the client

		bool IsValid()
		{
			return !m_strSessionName.IsEmpty() &&
				   m_nDumpInterval >= MIN_SERVER_DUMP_INTERVAL &&
				   m_nDumpInterval <= MAX_SERVER_DUMP_INTERVAL;
		}
	}
	m_ServerRecordingState;

private:
	//
	// CGenericPersistentManager
	//
	virtual int				GetVersion() const;
	virtual void			Think();
	virtual IReplayContext	*GetReplayContext() const;
	virtual void			OnObjLoaded( CBaseRecordingSession *pSession );

	//
	// IRecordingSessionManager
	//
	virtual const char		*GetNewSessionName() const;
	virtual void			FireGameEvent( IGameEvent *pEvent );

	void					AddEventsForListen();
	void					DownloadThink();

	float					m_flNextBlockUpdateTime;
	float					m_flNextPossibleDownloadTime;

	int						m_nNumSessionBlockDownloaders;	// TODO: Manage the number of session block downloaders
};

//----------------------------------------------------------------------------------------


#endif // CL_RECORDINGSESSIONMANAGER_H

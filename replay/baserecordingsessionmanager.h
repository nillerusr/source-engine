//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef BASERECORDINGSESSIONMANAGER_H
#define BASERECORDINGSESSIONMANAGER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "tier0/platform.h"
#include "utlstring.h"
#include "utllinkedlist.h"
#include "replay/replayhandle.h"
#include "replay/irecordingsessionmanager.h"
#include "genericpersistentmanager.h"

//----------------------------------------------------------------------------------------

class CBaseRecordingSession;
class CBaseRecordingSessionBlock;
class KeyValues;
class IReplayContext;

//----------------------------------------------------------------------------------------

//
// Manages and serializes all replay recording session data - instanced on both the client
// and server via CClientRecordingSessionManager and CServerRecordingSessionManager.
//
class CBaseRecordingSessionManager : public CGenericPersistentManager< CBaseRecordingSession >,
									 public IRecordingSessionManager
{
	typedef CGenericPersistentManager< CBaseRecordingSession > BaseClass;

public:
	CBaseRecordingSessionManager( IReplayContext *pContext );
	virtual ~CBaseRecordingSessionManager();

	virtual bool			Init();

	virtual CBaseRecordingSession *OnSessionStart( int nCurrentRecordingStartTick, const char *pSessionName );
	virtual void			OnSessionEnd();

	void					DeleteSession( ReplayHandle_t hSession, bool bForce );

	const char				*GetCurrentSessionName() const;
	int						GetCurrentSessionBlockIndex() const;

	CBaseRecordingSession	*FindSessionByName( const char *pSessionName );

	// This is here so that server-side cleanup can be done for a ditched session.  See calling code for details.
	void					DoSessionCleanup();

	//
	// IRecordingSessionManager
	//
	virtual CBaseRecordingSession	*FindSession( ReplayHandle_t hSession );
	virtual const CBaseRecordingSession	*FindSession( ReplayHandle_t hSession ) const;
	virtual void			FlagSessionForFlush( CBaseRecordingSession *pSession, bool bForceImmediate );
	virtual int				GetServerStartTickForSession( ReplayHandle_t hSession );

	// Get the recording session in progress
	CBaseRecordingSession	*GetRecordingSessionInProgress()	{ return m_pRecordingSession; }

	bool					LastSessionDitched() const	{ return m_bLastSessionDitched; }

protected:
	//
	// CGenericPersistentManager
	//
	virtual const char		*GetIndexFilename() const	{ return "sessions." GENERIC_FILE_EXTENSION; }
	virtual const char		*GetRelativeIndexPath() const;
	virtual const char		*GetDebugName() const			{ return "session manager"; }
	virtual void			Think();

	//
	// CBaseThinker
	//
	virtual float			GetNextThinkTime() const;

	IReplayContext			*m_pContext;
	CBaseRecordingSession	*m_pRecordingSession;	// Currently recording session, or NULL if not recording
	bool					m_bLastSessionDitched;

	virtual bool			CanDeleteSession( ReplayHandle_t hSession ) const	{ return true; }
	virtual bool			ShouldUnloadSessions() const { return false; }

	virtual void			OnAllSessionsDeleted() {}

private:
	void					DeleteSessionThink();
	void					MarkSessionForDelete( ReplayHandle_t hSession );

	CUtlLinkedList< ReplayHandle_t, int >	m_lstSessionsToDelete;
};

//----------------------------------------------------------------------------------------


#endif // BASERECORDINGSESSIONMANAGER_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef SV_SESSIONRECORDER_H
#define SV_SESSIONRECORDER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "basethinker.h"
#include "utllinkedlist.h"
#include "replay/ireplaysessionrecorder.h"
#include "replay/replayhandle.h"
#include "sv_filepublish.h"

//----------------------------------------------------------------------------------------

class IGameEvent;
class CServerRecordingSessionBlock;
class CServerRecordingSessionManager;
class CBaseRecordingSessionBlockManager;
class CSessionPublishManager;
class IDemoBuffer;
class CServerRecordingSession;

//----------------------------------------------------------------------------------------

class CSessionRecorder : public CBaseThinker,
						 public IReplaySessionRecorder
{
public:
	CSessionRecorder();
	~CSessionRecorder();

	bool				Init();
	void				AbortCurrentSessionRecording();

	int					GetCurrentRecordingStartTick() const	{ return m_nCurrentRecordingStartTick; }

	void				UpdateSessionLocks();	// Looks at publish managers and unlocks sessions as needed

	//
	// IReplaySessionRecorder
	//
	virtual void		StartRecording();	// Will waiting for recording to stop, then will begin recording
	virtual void		StopRecording( bool bAborting );
	virtual void		SetCurrentRecordingStartTick( int nStartTick );

	// Finish any publish jobs synchronously
	void				PublishAllSynchronous();

	bool				RecordingAborted() const	{ return m_bRecordingAborted; }

private:
	//
	// CBaseThinker
	//
	float				GetNextThinkTime() const;
	void				Think();

	void				PublishThink();

	CSessionPublishManager	*GetCurrentPublishManager() const;	// Get the publish manager for the currently recording session
	void				CreateAndAddNewPublishManager( CServerRecordingSession *pSession );

	int					m_nCurrentRecordingStartTick;
	bool				m_bRecordingAborted;

	typedef CUtlLinkedList< CSessionPublishManager *, int > PublishManagerContainer_t;
	PublishManagerContainer_t	m_lstPublishManagers;
};

//----------------------------------------------------------------------------------------

#endif	// SV_SESSIONRECORDER_H

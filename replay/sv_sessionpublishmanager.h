//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef SV_SESSIONPUBLISHMANAGER_H
#define SV_SESSIONPUBLISHMANAGER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/replayhandle.h"

//----------------------------------------------------------------------------------------

class CServerRecordingSession;
class CSessionBlockPublisher;
class CSessionInfoPublisher;

//----------------------------------------------------------------------------------------

//
// CSessionPublishManager takes care of all the publishing for a particular session.
// For asynchronous publishing of block and session info data, publishing can
// sometimes overlap between rounds, as is sometimes the case for FTP.
//
// A CSessionPublishManager instance are created by passing in the in-progress recording
// session into the constructor.
//
// CSessionRecorder maintains a list of CSessionPublishManager instances and cleans
// them up once all publishing for their corresponding session is completed.
//

class CSessionPublishManager
{
public:
	CSessionPublishManager( CServerRecordingSession *pSession );
	~CSessionPublishManager();

	void				Think();	// Called explicitly

	// Finish any publish jobs synchronously
	void				PublishAllSynchronous();

	// Have all publish job completed?
	bool				IsDone() const;

	// This will flag this publish manager as recording
	void				OnStartRecording();

	// This will write out and publish any final session block
	void				OnStopRecord( bool bAborting );

	// Get the handle for the associated session
	ReplayHandle_t		GetSessionHandle() const;

	// Unlock the associated session - this should only be called if IsDone() returns true.
	void				UnlockSession();

	// Abort publishing
	void				AbortPublish();

#ifdef _DEBUG
	void				Validate();
#endif

private:
	CServerRecordingSession	*m_pSession;
	CSessionBlockPublisher	*m_pBlockPublisher;
	CSessionInfoPublisher	*m_pSessionInfoPublisher;
};

//----------------------------------------------------------------------------------------

#endif	// SV_SESSIONPUBLISHMANAGER_H

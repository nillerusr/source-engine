//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef SV_SESSIONINFOPUBLISHER_H
#define SV_SESSIONINFOPUBLISHER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "utlbuffer.h"
#include "sv_filepublish.h"
#include "replay/replaytime.h"
#include "sessioninfoheader.h"

//----------------------------------------------------------------------------------------

class CServerRecordingSession;
class IFilePublisher;

//----------------------------------------------------------------------------------------

class CSessionInfoPublisher : public IPublishCallbackHandler
{
public:
	CSessionInfoPublisher( CServerRecordingSession *pSession );
	~CSessionInfoPublisher();

	void			Publish();
	void			PublishAllSynchronous();

	bool			IsPublishingSessionInfo() const	{ return m_flSessionInfoPublishTime != 0.0f || m_pFilePublisher; }
	bool			IsDone() const;

	void			OnStopRecord( bool bAborting );
	void			AbortPublish();
	void			Think();	// Called explicitly

private:
	//
	// IPublishCallback
	//
	virtual void	OnPublishComplete( const IFilePublisher *pPublisher, void *pUserData );
	virtual void	OnPublishAborted( const IFilePublisher *pPublisher );
	virtual void	AdjustHeader( const IFilePublisher *pPublisher, void *pHeaderData );

	void			RefreshSessionInfoBlockData( CUtlBuffer &buf );

	IFilePublisher	*m_pFilePublisher;
	float			m_flSessionInfoPublishTime;

	SessionInfoHeader_t m_SessionInfoHeader;
	int				m_itLastCompletedBlockWrittenToBuffer;	// The last block written to the buffer that isn't going to change state
	CUtlBuffer		m_bufSessionInfo;	// Growable buffer for the session info block data (does not include header)

	CServerRecordingSession	*m_pSession;

	bool			m_bShouldPublish;	// True if we should publish - can be true while already publishing.  Means another publish is needed.
};

//----------------------------------------------------------------------------------------

#endif // SV_SESSIONINFOPUBLISHER_H

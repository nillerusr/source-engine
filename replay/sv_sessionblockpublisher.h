//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef SV_SESSIONBLOCKPUBLISHER_H
#define SV_SESSIONBLOCKPUBLISHER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "sv_filepublish.h"
#include "replay/replayhandle.h"

//----------------------------------------------------------------------------------------

class CServerRecordingSession;
class CServerRecordingSessionBlock;
class CSessionInfoPublisher;
class IDemoBuffer;

//----------------------------------------------------------------------------------------

class CSessionBlockPublisher : public IPublishCallbackHandler
{
public:
	CSessionBlockPublisher( CServerRecordingSession *pSession, CSessionInfoPublisher *pSessionInfoPublisher );
	~CSessionBlockPublisher();

	void				Think();	// Called explicitly

	// Finish any publish jobs synchronously
	void				PublishAllSynchronous();

	// Abort any publishing
	void				AbortPublish();

	// Have all publish job completed?
	bool				IsDone() const;

	// This will flag this publish manager as recording
	void				OnStartRecording();

	// This will write out and publish any final session block
	void				OnStopRecord( bool bAborting );

	// Get the handle for the associated session
	ReplayHandle_t		GetSessionHandle() const;

#ifdef _DEBUG
	void				Validate();
#endif

private:
	//
	// IPublishCallback
	//
	virtual void		OnPublishComplete( const IFilePublisher *pPublisher, void *pUserData );
	virtual void		OnPublishAborted( const IFilePublisher *pPublisher );
	virtual void		AdjustHeader( const IFilePublisher *pPublisher, void *pHeaderData ) {}

	void				PublishBlock( CServerRecordingSessionBlock *pBlock );

	void				WriteAndPublishSessionBlock();
	void				PublishThink();
	void				WriteSessionBlockThink();
	void				SessionLockThink();
	void				GatherBlockData( uint8 *pSessionBuffer, int nSessionBufferSize, CServerRecordingSessionBlock *pBlock,
										 unsigned char **ppBlockData, int *pBlockSize );
	CServerRecordingSessionBlock	*FindBlockFromPublisher( const IFilePublisher *pPublisher );

	float				m_flLastBlockWriteTime;
	int					m_nDumpInterval;
	CUtlLinkedList< CServerRecordingSessionBlock *, int > m_lstPublishingBlocks;

	CServerRecordingSession	*m_pSession;
	CSessionInfoPublisher	*m_pSessionInfoPublisher;
};

//----------------------------------------------------------------------------------------

#endif	// SV_SESSIONBLOCKPUBLISHER_H

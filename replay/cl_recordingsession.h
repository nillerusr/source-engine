//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef CL_RECORDINGSESSION_H
#define CL_RECORDINGSESSION_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "baserecordingsession.h"
#include "basethinker.h"

//----------------------------------------------------------------------------------------

class CReplay;
class CSessionInfoDownloader;
class CClientRecordingSessionBlock;

//----------------------------------------------------------------------------------------

class CClientRecordingSession : public CBaseRecordingSession,
								public CBaseThinker
{
	typedef CBaseRecordingSession BaseClass;
public:
	CClientRecordingSession( IReplayContext *pContext );
	~CClientRecordingSession();

	void					DeleteBlocks();
	void					UpdateAllBlocksDownloaded();		// Sets the all-blocks-downloaded flag if this session is no longer recording and all blocks have been downloaded
	void					EnsureDownloadingEnabled();

	//
	// CGenericPersistentManager
	//
	bool					Read( KeyValues *pIn );
	void					Write( KeyValues *pOut );

	void					AdjustLastBlockToDownload( int iNewLastBlockToDownload );	// When the client predicts more blocks than the server is ever going to write (if the round ends and the player just died, for example), this allows the session info downloader to adjust the max # of blocks to download - also does adjustments for any offending replays
	int						UpdateLastBlockToDownload();	// Calling this will implicitly cause new blocks to be downloaded for the given session
	void					SyncSessionBlocks();	// Creates session info downloader and fires off the download - creates and syncs blocks as necessary
	void					OnReplayDeleted( CReplay *pReplay );
	void					UpdateReplayStatuses( CClientRecordingSessionBlock *pBlock );	// Called after a given block is downloaded/failed
	void					CacheReplay( CReplay *pReplay );

	int						GetLastBlockToDownload() const		{ return m_iLastBlockToDownload; }
	bool					ShouldSyncBlocksWithServer() const;	// Returns true if the number of blocks for this session is out of sync with m_iLastBlockToDownload
	bool					HasSessionInfoDownloader() const	{ return m_pSessionInfoDownloader != NULL; }
	int						GetGreatestConsecutiveBlockDownloaded() const	{ return m_iGreatestConsecutiveBlockDownloaded; }
	void					OnDownloadTimeout();	// Called if blocks were expected to appear or become downloadable but never showed up or updated.
	void					RefreshLastUpdateTime();
	bool					TimedOut() const					{ return m_bTimedOut; }
	float					GetLastUpdateTime() const			{ return m_flLastUpdateTime; }
	bool					AllReplaysReconstructed() const;

	//
	// CBaseRecordingSession
	//
	virtual void			PopulateWithRecordingData( int nCurrentRecordingStartTick );
	virtual bool			ShouldDitchSession() const;
	virtual void			OnDelete();

	uint64					GetServerSessionID() const			{ return m_uServerSessionID; }

private:
	//
	// CBaseThinker
	//
	virtual float			GetNextThinkTime() const;
	virtual void			Think();

	void					UpdateGreatestConsecutiveBlockDownloaded();
	void					ShowDownloadFailedMessage( const CReplay *pReplay );

	float					m_flLastUpdateTime;		// The last time a block was added or updated during session info download (see CSessionInfoDownloader::OnDownloadComplete())
	int						m_iLastBlockToDownload;	// The last block we should download for the given session (in terms of reconstruction index)
	int						m_iGreatestConsecutiveBlockDownloaded;	// The greatest consecutive block downloaded (in terms of reconstruction index)
	CSessionInfoDownloader	*m_pSessionInfoDownloader;
	int						m_nSessionInfoDownloadAttempts;
	bool					m_bTimedOut;			// We can't test time-out state based on m_flLastUpdateTime - the session has to be put into the timed-out state explicitly by
													// the session info downloader.  "Time out" in this context means that nothing updated in the session info file for more than
													// 90 seconds or whatever.
	bool					m_bAllBlocksDownloaded;

	CUtlLinkedList< CReplay *, int >	m_lstReplays;	// List of replays that refer to this session

	uint64					m_uServerSessionID;		// A globally unique ID for the round
};

//----------------------------------------------------------------------------------------

inline CClientRecordingSession *CL_CastSession( IReplaySerializeable *pSession )
{
	return static_cast< CClientRecordingSession * >( pSession );
}

//----------------------------------------------------------------------------------------

#endif // CL_RECORDINGSESSION_H

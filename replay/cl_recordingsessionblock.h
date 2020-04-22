//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef CL_RECORDINGSESSIONBLOCK_H
#define CL_RECORDINGSESSIONBLOCK_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "baserecordingsessionblock.h"
#include "engine/http.h"

//----------------------------------------------------------------------------------------

class CClientRecordingSessionBlock : public CBaseRecordingSessionBlock
{
	typedef CBaseRecordingSessionBlock BaseClass;

public:
	CClientRecordingSessionBlock( IReplayContext *pContext );

	bool			NeedsUpdate() const;
	bool			ShouldDownloadNow() const;
	bool			DownloadedSuccessfully() const;

	int				GetNumDownloadAttempts() const		{ return m_nDownloadAttempts; }

	virtual bool	Read( KeyValues *pIn );
	virtual void	Write( KeyValues *pOut );
	virtual void	OnDelete();

	// Resets the download status to be "ready for download" if the # of download attempts
	// is under 3.  Returns false if reset failed, otherwise true.
	bool			AttemptToResetForDownload();

	// Checks data against the block's md5 digest
	bool			ValidateData( const void *pData, int nSize, unsigned char *pOutHash = NULL ) const;

	enum DownloadStatus_t
	{
		DOWNLOADSTATUS_ABORTED,			// Download was aborted for some reason
		DOWNLOADSTATUS_ERROR,			// Refer to m_nError for more detail
		DOWNLOADSTATUS_WAITING,			// Waiting for the file to be ready on the server
		DOWNLOADSTATUS_READYTODOWNLOAD,	// File is ready to be downloaded
		DOWNLOADSTATUS_CONNECTING,		// Connecting to file server
		DOWNLOADSTATUS_DOWNLOADING,		// Currently downloading
		DOWNLOADSTATUS_DOWNLOADED,		// Successfully downloaded file
		
		MAX_DOWNLOADSTATUS
	};

	// Persistent:
	DownloadStatus_t	m_nDownloadStatus;
	uint32				m_uBytesDownloaded;
	bool				m_bDataInvalid;		// Hash didn't match data?
	HTTPError_t			m_nHttpError;

private:
	// Non-persistent:
	int					m_nDownloadAttempts;		// Should be modified via AttemptToResetForDownload()
};

//----------------------------------------------------------------------------------------

inline CClientRecordingSessionBlock *CL_CastBlock( IReplaySerializeable *pBlock )
{
	return static_cast< CClientRecordingSessionBlock * >( pBlock );
}

inline const CClientRecordingSessionBlock *CL_CastBlock( const IReplaySerializeable *pBlock )
{
	return static_cast< const CClientRecordingSessionBlock * >( pBlock );
}

//----------------------------------------------------------------------------------------

#endif // CL_RECORDINGSESSIONBLOCK_H
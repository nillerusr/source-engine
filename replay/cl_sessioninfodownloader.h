//========= Copyright Valve Corporation, All rights reserved. ============//
//
//----------------------------------------------------------------------------------------

#ifndef SESSIONINFODOWNLOADER_H
#define SESSIONINFODOWNLOADER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "basethinker.h"
#include "cl_downloader.h"

//----------------------------------------------------------------------------------------

class CHttpDownloader;
class CBaseRecordingSession;

//----------------------------------------------------------------------------------------

class CSessionInfoDownloader : public CBaseThinker,
							   public IDownloadHandler
{
public:
	CSessionInfoDownloader();
	~CSessionInfoDownloader();

	void					CleanupDownloader();

	void					DownloadSessionInfoAndUpdateBlocks( CBaseRecordingSession *pSession );

	bool					IsDone() const		{ return m_bDone; }
	bool					CanDelete() const	{ return m_pDownloader == NULL; }

	enum ServerSessionInfoError_t
	{
		ERROR_NONE,							// No error
		ERROR_NOT_ENOUGH_DATA,				// The session info file wasn't even big enough to read a header
		ERROR_BAD_NUM_BLOCKS,				// The "nb" field either didn't exist or was invalid - there should always been at least one block by the time we're downloading
		ERROR_REPLAY_NOT_FOUND,				// The server index was downloaded but the replay was not found
		ERROR_INVALID_REPLAY_STATUS,		// The server index was downloaded and the replay was found, but it had an invalid status
		ERROR_INVALID_ORDER,				// The server index was downloaded and the replay was found, but it had an invalid reconstruction order (-1)
		ERROR_NO_SESSION_NAME,				// No session name for entry
		ERROR_UNKNOWN_SESSION,				// The session info file points to a session (via its name) that the client doesn't know about
		ERROR_DOWNLOAD_FAILED,				// The session file failed to download
		ERROR_BLOCK_READ_FAILED,			// Failed to read a block - most likely an overflow
		ERROR_COULD_NOT_CREATE_COMPRESSOR,	// Could not create the ICompressor to decompress the payload
		ERROR_INVALID_UNCOMPRESSED_SIZE,	// Uncompressed size was not large enough to read at least one block
		ERROR_PAYLOAD_DECOMPRESS_FAILED,	// Decompression of the payload failed
		ERROR_PAYLOAD_HASH_FAILED,			// Used MD5 digest from header on payload and failed
	};

	ServerSessionInfoError_t	m_nError;
	HTTPError_t					m_nHttpError;

private:
	//
	// CBaseThinker
	//
	float					GetNextThinkTime() const;
	void					Think();

	//
	// IDownloadHandler
	//
	virtual void			OnConnecting( CHttpDownloader *pDownloader ) {}
	virtual void			OnFetch( CHttpDownloader *pDownloader ) {}
	virtual void			OnDownloadComplete( CHttpDownloader *pDownloader, const unsigned char *pData );

	const char				*GetErrorString( int nError, HTTPError_t nHttpError ) const;

	const CBaseRecordingSession	*m_pSession;
	CHttpDownloader			*m_pDownloader;
	bool					m_bDone;
	float					m_flLastDownloadTime;
};

//----------------------------------------------------------------------------------------

#endif // SESSIONINFODOWNLOADER_H

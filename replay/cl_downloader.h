//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef CL_DOWNLOADER_H
#define CL_DOWNLOADER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "platform.h"
#include "engine/http.h"
#include "interface.h"
#include "tier0/threadtools.h"

//----------------------------------------------------------------------------------------

struct RequestContext_t;
class CHttpDownloader;
class KeyValues;

//----------------------------------------------------------------------------------------

class IDownloadHandler
{
public:
	virtual void		OnConnecting( CHttpDownloader *pDownloader ) = 0;
	virtual void		OnFetch( CHttpDownloader *pDownloader ) = 0;

	// Called when the download is done successfully, errors out, or is aborted.
	// NOTE: pDownloader should NOT be deleted from within OnDownloadComplete().
	// pData contains the downloaded data for processing.
	virtual void		OnDownloadComplete( CHttpDownloader *pDownloader, const unsigned char *pData ) = 0;
};

//----------------------------------------------------------------------------------------

//
// Generic downloader class - downloads a single file on its own thread, and maintains
// state for that data.
//
// TODO: Derive from CBaseThinker and remove explicit calls to Think() - will make this
// class less bug-prone (easy to forget to call Think()).
//
class CHttpDownloader
{
public:
	// Pass in a callback 
	CHttpDownloader( IDownloadHandler *pHandler = NULL );
	~CHttpDownloader();

	//
	// Download the file at the given URL (HTTP/HTTPS support only)
	// pGamePath - Game path where we should put the file - can be NULL if we don't
	//    want to save to the file to disk
	// pUserData - Passed back to IDownloadHandler, if one has been set - can be NULL
	// pBytesDownloaded - If non-NULL, # of bytes downloaded written.
	// Returns true on success.
	//
	bool				BeginDownload( const char *pURL, const char *pGamePath = NULL,
									   void *pUserData = NULL, uint32 *pBytesDownloaded = NULL );

	//
	// Abort the download (if there is one), wait for the download to shutdown and
	// do cleanup.
	//
	void				AbortDownloadAndCleanup();

	inline bool			IsDone() const			{ return m_bDone; }	// Download done?
	inline bool			CanDelete() const		{ return m_pThreadState == NULL; }	// Can free?
	inline HTTPStatus_t	GetStatus() const		{ return m_nHttpStatus; }
	inline HTTPError_t	GetError() const		{ return m_nHttpError; }
	inline void			*GetUserData() const	{ return m_pUserData; }
	inline uint32		GetBytesDownloaded() const	{ return m_uBytesDownloaded; }
	inline uint32		GetSize() const			{ return m_uSize; }	// File size in bytes - NOTE: Not valid until the download is complete, aborted, or errored out
	inline const char	*GetURL() const			{ return m_szURL; }

	void				Think();

	KeyValues			*GetOgsRow( int nErrorCounter ) const;

	static const char	*GetHttpErrorToken( HTTPError_t nError );

private:
	bool				CleanupThreadIfDone();
	void				InvokeHandler();

	RequestContext_t	*m_pThreadState;
	float				m_flNextThinkTime;
	bool				m_bDone;
	HTTPError_t			m_nHttpError;
	HTTPStatus_t		m_nHttpStatus;
	uint32				m_uBytesDownloaded;
	uint32				*m_pBytesDownloaded;	// Passed into BeginDownload()
	uint32				m_uSize;
	IDownloadHandler	*m_pHandler;
	void				*m_pUserData;
	char				m_szURL[512];

	// Use this to make sure that AbortDownloadAndCleanup isn't executed simultaneously
	// by two threads. This was causing use-after-free crashes during shutdown.
	CThreadMutex		m_lock;
};

//----------------------------------------------------------------------------------------

#endif // CL_DOWNLOADER_H

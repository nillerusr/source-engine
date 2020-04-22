//========= Copyright Valve Corporation, All rights reserved. ============//
//
//----------------------------------------------------------------------------------------

#ifndef CL_SESSIONBLOCKDOWNLOADER_H
#define CL_SESSIONBLOCKDOWNLOADER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "utllinkedlist.h"
#include "cl_downloader.h"
#include "basethinker.h"
#include "replay/replayhandle.h"

//----------------------------------------------------------------------------------------

class CClientRecordingSession;

//----------------------------------------------------------------------------------------

//
// Downloads multiple session blocks concurrently and updates their status.
//
class CSessionBlockDownloader : public CBaseThinker,
								public IDownloadHandler
{
public:
	CSessionBlockDownloader();

	void				Shutdown();
	void				Think();

	// If pSession is NULL, all remaining downloads will affected.
	void				AbortDownloadsAndCleanup( CClientRecordingSession *pSession );

private:
	bool				AtMaxConcurrentDownloads() const;

	//
	// CBaseThinker
	//
	virtual float		GetNextThinkTime() const;

	//
	// IDownloadHandler
	//
	virtual void		OnConnecting( CHttpDownloader *pDownloader );
	virtual void		OnFetch( CHttpDownloader *pDownloader );
	virtual void		OnDownloadComplete( CHttpDownloader *pDownloader, const unsigned char *pData );

	static int			sm_nNumCurrentDownloads;

	CUtlLinkedList< CHttpDownloader	*, int > m_lstDownloaders;
	int					m_nMaxBlock;

	friend class CErrorSystem;
};

//----------------------------------------------------------------------------------------

#endif // CL_SESSIONBLOCKDOWNLOADER_H

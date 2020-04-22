//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef CL_SCREENSHOTMANAGER_H
#define CL_SCREENSHOTMANAGER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "basethinker.h"
#include "replay/ireplayscreenshotmanager.h"
#include "replay/screenshot.h"

//----------------------------------------------------------------------------------------

class CReplay;

//----------------------------------------------------------------------------------------

class CScreenshotManager : public CBaseThinker,
						   public IReplayScreenshotManager
{
public:
					CScreenshotManager();
					~CScreenshotManager();

	bool			Init();

	bool			ShouldCaptureScreenshot();
	void			DoCaptureScreenshot();

	void			SetScreenshotReplay( ReplayHandle_t hReplay );
	ReplayHandle_t	GetScreenshotReplay() const	{ return m_hScreenshotReplay; }

	//
	// IReplayScreenshotManager
	//
	virtual void	CaptureScreenshot( CaptureScreenshotParams_t& params );
	virtual void	GetUnpaddedScreenshotSize( int &nOutWidth, int &nOutHeight );
	virtual void	DeleteScreenshotsForReplay( CReplay *pReplay );

private:
	//
	// CBaseThinker
	//
	void			Think();
	float			GetNextThinkTime() const;

	float			m_flScreenshotCaptureTime;
	CaptureScreenshotParams_t	m_screenshotParams;	// Params for next scheduled screenshot
	int				m_aScreenshotWidths[3][2];	// [ 16:9, 16:10, 4:3 ][ lo res, hi res ]

	ReplayHandle_t	m_hScreenshotReplay;	// Destination replay for any screenshots taken - we always write to the "pending"
											// replay for the most part, but if we want to take a screenshot after the local
											// player is dead, we need to use a handle rather than using m_pPendingReplay directly,
											// since it becomes NULL when the player dies.
	float			m_flLastScreenshotTime;
	int				m_nPrevScreenDims[2];	// Screenshot dimensions, used to determine if we should update the screenshot cache on the client
};

//----------------------------------------------------------------------------------------

#endif	// CL_SCREENSHOTMANAGER_H

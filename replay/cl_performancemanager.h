//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef REPLAYPERFORMANCEMANAGER_H
#define REPLAYPERFORMANCEMANAGER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/ireplayperformancemanager.h"
#include "replay/performance.h"

//----------------------------------------------------------------------------------------

class KeyValues;
class CReplay;
class IReplayPerformancePlaybackController;

//----------------------------------------------------------------------------------------

class CReplayPerformanceManager : public IReplayPerformanceManager
{
public:
	CReplayPerformanceManager();
	~CReplayPerformanceManager();

	void						Init();

	//
	// IReplayPerformanceManager
	//
	virtual const char			*GetRelativePath() const;
	virtual const char			*GetFullPath() const;
	virtual CReplayPerformance	*CreatePerformance( CReplay *pReplay );
	virtual void				DeletePerformance( CReplayPerformance *pPerformance );
	virtual const char			*GeneratePerformanceFilename( CReplay *pReplay );
};

//----------------------------------------------------------------------------------------

#endif // REPLAYPERFORMANCEMANAGER_H

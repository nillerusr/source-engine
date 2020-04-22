//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef REPLAYRENDERQUEUE_H
#define REPLAYRENDERQUEUE_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/ireplayrenderqueue.h"
#include "utlvector.h"

//----------------------------------------------------------------------------------------

class CRenderQueue : public IReplayRenderQueue
{
public:
	CRenderQueue();
	~CRenderQueue();

	virtual void	Add( ReplayHandle_t hReplay, int iPerformance );
	virtual void	Remove( ReplayHandle_t hReplay, int iPerformance );
	virtual void	Clear();

	virtual int		GetCount() const;
	virtual bool	GetEntryData( int iIndex, ReplayHandle_t *pHandleOut, int *pPerformanceOut ) const;
	virtual bool	IsInQueue( ReplayHandle_t hReplay, int iPerformance ) const;

private:
	struct RenderInfo_t
	{
		ReplayHandle_t	m_hReplay;
		int				m_iPerformance;
	};

	RenderInfo_t *Find( ReplayHandle_t hReplay, int iPerformance );
	const RenderInfo_t *Find( ReplayHandle_t hReplay, int iPerformance ) const;

	CUtlVector< RenderInfo_t * > m_vecQueue;
};

//----------------------------------------------------------------------------------------

#endif // REPLAYRENDERQUEUE_H

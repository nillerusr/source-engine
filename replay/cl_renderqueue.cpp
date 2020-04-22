//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cl_renderqueue.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CRenderQueue::CRenderQueue()
{
}

CRenderQueue::~CRenderQueue()
{
	Clear();
}

void CRenderQueue::Add( ReplayHandle_t hReplay, int iPerformance )
{
	RenderInfo_t *pEntry = new RenderInfo_t;
	pEntry->m_hReplay = hReplay;
	pEntry->m_iPerformance = iPerformance;
	m_vecQueue.AddToTail( pEntry );
}

void CRenderQueue::Remove( ReplayHandle_t hReplay, int iPerformance )
{
	RenderInfo_t *pEntry = Find( hReplay, iPerformance );
	if ( pEntry )
	{
		m_vecQueue.FindAndRemove( pEntry );
		delete pEntry;
	}
}

void CRenderQueue::Clear()
{
	m_vecQueue.PurgeAndDeleteElements();
}

int CRenderQueue::GetCount() const
{
	return m_vecQueue.Count();
}

bool CRenderQueue::GetEntryData( int iIndex, ReplayHandle_t *pHandleOut, int *pPerformanceOut ) const
{
	if ( iIndex < 0 || iIndex >= GetCount() )
	{
		AssertMsg( 0, "Request for replay render queue data is out of bounds!" );
		Warning( "Request for replay render queue data is out of bounds!" );
		return false;
	}

	if ( !pHandleOut || !pPerformanceOut )
	{
		AssertMsg( 0, "Bad parameters" );
		return false;
	}

	const RenderInfo_t *pEntry = m_vecQueue[ iIndex ];
	*pHandleOut = pEntry->m_hReplay;
	*pPerformanceOut = pEntry->m_iPerformance;

	return true;
}

bool CRenderQueue::IsInQueue( ReplayHandle_t hReplay, int iPerformance ) const
{
	return Find( hReplay, iPerformance ) != NULL;
}

CRenderQueue::RenderInfo_t *CRenderQueue::Find( ReplayHandle_t hReplay, int iPerformance )
{
	FOR_EACH_VEC( m_vecQueue, i )
	{
		RenderInfo_t *pEntry = m_vecQueue[ i ];
		if ( pEntry->m_hReplay == hReplay && pEntry->m_iPerformance == iPerformance )
			return pEntry;
	}
	return NULL;
}

const CRenderQueue::RenderInfo_t *CRenderQueue::Find( ReplayHandle_t hReplay, int iPerformance ) const
{
	return const_cast< CRenderQueue * >( this )->Find( hReplay, iPerformance );
}

//----------------------------------------------------------------------------------------


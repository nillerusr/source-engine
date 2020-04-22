//========= Copyright ©, Valve LLC, All rights reserved. ======================
//
// Purpose: Defines a buffer pool used to group small allocations
//
//=============================================================================


#include "stdafx.h"
#include "bufferpool.h"

using namespace GCSDK;

CUtlVector<CBufferPool *> CBufferPool::sm_vecBufferPools;


//----------------------------------------------------------------------------
// Purpose: Constructor
//----------------------------------------------------------------------------
CBufferPool::CBufferPool( const char *pchName, const GCConVar &cvMaxSizeMB, const GCConVar &cvInitBufferSize, int nFlags )
	:	m_nBuffersInUse( 0 )
	,	m_nBuffersTotal( 0 )
	,	m_nHighWatermark( 0 )
	,	m_cubFree( 0 )
	,	m_sName( pchName )
	,	m_cvMaxSizeMB( cvMaxSizeMB )
	,	m_cvInitBufferSize( cvInitBufferSize )
	,	m_nFlags( nFlags )
{
	sm_vecBufferPools.AddToTail( this );
}


//----------------------------------------------------------------------------
// Purpose: Destructor
//----------------------------------------------------------------------------
CBufferPool::~CBufferPool()
{
	m_vecFreeBuffers.PurgeAndDeleteElements();
	sm_vecBufferPools.FindAndRemove( this );
}


//----------------------------------------------------------------------------
// Purpose: Gives a bind param buffer to a query
//----------------------------------------------------------------------------
CUtlBuffer *CBufferPool::GetBuffer()
{
	m_nBuffersInUse++;

	if ( m_vecFreeBuffers.Count() > 0 )
	{
		CUtlBuffer *pBuffer = m_vecFreeBuffers.Tail();
		m_vecFreeBuffers.Remove( m_vecFreeBuffers.Count() - 1 );
		m_cubFree -= pBuffer->Size();

		return pBuffer;
	}
	else
	{
		m_nBuffersTotal++;
		m_nHighWatermark = max( m_nBuffersTotal, m_nHighWatermark );
		return new CUtlBuffer( 0, m_cvInitBufferSize.GetInt(), m_nFlags );
	}
}


//----------------------------------------------------------------------------
// Purpose: Returns a bind param buffer when a query is done with it
//----------------------------------------------------------------------------
void CBufferPool::ReturnBuffer( CUtlBuffer *pBuffer )
{
	m_nBuffersInUse--;

	if ( ( int64 )( m_cubFree + pBuffer->Size() ) <= ( m_cvMaxSizeMB.GetInt() * k_nMegabyte ) )
	{
		pBuffer->Clear();
		m_cubFree += pBuffer->Size();
		m_vecFreeBuffers.AddToTail( pBuffer );
	}
	else
	{
		m_nBuffersTotal--;
		delete pBuffer;
	}
}


//----------------------------------------------------------------------------
// Purpose: Print statistics to the console
//----------------------------------------------------------------------------
void CBufferPool::DumpPools()
{
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Reusable Buffer Pools:\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, 
		"  Pool                                Buffers         Free  Max Buffers  Total Mem (est)         Free Mem\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, 
		"  ------------------------------ ------------ ------------ ------------ ---------------- ----------------\n" );

	FOR_EACH_VEC( sm_vecBufferPools, i )
	{
		CBufferPool *pPool = sm_vecBufferPools[i];

		int nTotalEst = 0;
		if ( pPool->m_vecFreeBuffers.Count() > 0 )
		{
			nTotalEst = pPool->m_cubFree / pPool->m_vecFreeBuffers.Count() * pPool->m_nBuffersTotal;
		}

		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "  %-30s %12d %12d %12d %16s %16s\n",
			pPool->m_sName.Get(),
			pPool->m_nBuffersTotal,
			pPool->m_vecFreeBuffers.Count(),
			pPool->m_nHighWatermark,
			Q_pretifymem( nTotalEst, 0, true ),
			Q_pretifymem( pPool->m_cubFree, 0, true )
		);
	}

	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\n" );
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#include "pch_materialsystem.h"

#define MATSYS_INTERNAL

#include "occlusionquerymgr.h"
#include "imaterialsysteminternal.h"
#include "imatrendercontextinternal.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static COcclusionQueryMgr s_OcclusionQueryMgr;
COcclusionQueryMgr *g_pOcclusionQueryMgr = &s_OcclusionQueryMgr;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
COcclusionQueryMgr::COcclusionQueryMgr()
{
	m_nFrameCount = 0;
}


//-----------------------------------------------------------------------------
// Allocate and delete query objects.
//-----------------------------------------------------------------------------
OcclusionQueryObjectHandle_t COcclusionQueryMgr::CreateOcclusionQueryObject( )
{
	m_Mutex.Lock();
	int h = m_OcclusionQueryObjects.AddToTail();
	m_Mutex.Unlock();
	return (OcclusionQueryObjectHandle_t)h;
}

void COcclusionQueryMgr::OnCreateOcclusionQueryObject( OcclusionQueryObjectHandle_t h )
{
	for ( int i = 0; i < COUNT_OCCLUSION_QUERY_STACK; i++)
	{
		m_OcclusionQueryObjects[(int)h].m_QueryHandle[i] = g_pShaderAPI->CreateOcclusionQueryObject( );
	}
}

// Flushes an outstanding query
// HEY - Be very careful using this method - it causes a full pipeline flush/stall!
void COcclusionQueryMgr::FlushQuery( OcclusionQueryObjectHandle_t hOcclusionQuery, int nIndex )
{
	// Flush out any previous queries
	int h = (int)hOcclusionQuery;
	if ( m_OcclusionQueryObjects[h].m_bHasBeenIssued[nIndex] )
	{
		ShaderAPIOcclusionQuery_t hQuery = m_OcclusionQueryObjects[h].m_QueryHandle[nIndex];
		
		while ( OCCLUSION_QUERY_RESULT_PENDING == g_pShaderAPI->OcclusionQuery_GetNumPixelsRendered( hQuery, true ) )
			continue;
	}
}

void COcclusionQueryMgr::DestroyOcclusionQueryObject( OcclusionQueryObjectHandle_t hOcclusionQuery )
{
	int h = (int)hOcclusionQuery;
	Assert( m_OcclusionQueryObjects.IsValidIndex( h ) );
	if ( m_OcclusionQueryObjects.IsValidIndex( h ) )
	{
		for ( int i = 0; i < COUNT_OCCLUSION_QUERY_STACK; i++)
		{
			if ( m_OcclusionQueryObjects[h].m_QueryHandle[i] != INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE )
			{
				g_pShaderAPI->DestroyOcclusionQueryObject( m_OcclusionQueryObjects[h].m_QueryHandle[i] );
			}
		}
		m_Mutex.Lock();
		m_OcclusionQueryObjects.Remove( h );
		m_Mutex.Unlock();
	}
}


//-----------------------------------------------------------------------------
// Advance frame
//-----------------------------------------------------------------------------
void COcclusionQueryMgr::AdvanceFrame()
{
	++m_nFrameCount;
}


//-----------------------------------------------------------------------------
// Alt-tab support
// NOTE: This doesn't queue anything up
//-----------------------------------------------------------------------------
void COcclusionQueryMgr::AllocOcclusionQueryObjects( void )
{
	FOR_EACH_LL( m_OcclusionQueryObjects, iterator )
	{
		for ( int i = 0; i < COUNT_OCCLUSION_QUERY_STACK; i++)
		{
			m_OcclusionQueryObjects[iterator].m_QueryHandle[i] = g_pShaderAPI->CreateOcclusionQueryObject();
			m_OcclusionQueryObjects[iterator].m_bHasBeenIssued[i] = false;		// any in-flight queries are never returning
		}
	}
}

void COcclusionQueryMgr::FreeOcclusionQueryObjects( void )
{
	FOR_EACH_LL( m_OcclusionQueryObjects, iterator )
	{
		for ( int i = 0; i < COUNT_OCCLUSION_QUERY_STACK; i++)
		{
			if ( m_OcclusionQueryObjects[iterator].m_QueryHandle[i] != INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE )
			{
				g_pShaderAPI->DestroyOcclusionQueryObject( m_OcclusionQueryObjects[iterator].m_QueryHandle[i] );
				m_OcclusionQueryObjects[iterator].m_QueryHandle[i] = INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE;
				m_OcclusionQueryObjects[iterator].m_bHasBeenIssued[i] = false;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Used to make the handle think it's never had a successful query before
//-----------------------------------------------------------------------------
void COcclusionQueryMgr::ResetOcclusionQueryObject( OcclusionQueryObjectHandle_t hOcclusionQuery )
{
	int h = (int)hOcclusionQuery;
	Assert( m_OcclusionQueryObjects.IsValidIndex( h ) );
	if ( m_OcclusionQueryObjects.IsValidIndex( h ) )
	{
		// Forget we've issued any previous queries - there's no need to flush them.
		for ( int i = 0; i < COUNT_OCCLUSION_QUERY_STACK; i++)
		{
			m_OcclusionQueryObjects[h].m_bHasBeenIssued[i] = false;
		}

		m_OcclusionQueryObjects[h].m_LastResult = -1;
		m_OcclusionQueryObjects[h].m_nFrameIssued = -1;
	}
}


//-----------------------------------------------------------------------------
// Bracket drawing with begin and end so that we can get counts next frame.
//-----------------------------------------------------------------------------
void COcclusionQueryMgr::BeginOcclusionQueryDrawing( OcclusionQueryObjectHandle_t hOcclusionQuery )
{
	int h = (int)hOcclusionQuery;
	Assert( m_OcclusionQueryObjects.IsValidIndex( h ) );
	if ( m_OcclusionQueryObjects.IsValidIndex( h ) )
	{
		int nCurrent = m_OcclusionQueryObjects[h].m_nCurrentIssue;
		ShaderAPIOcclusionQuery_t hQuery = m_OcclusionQueryObjects[h].m_QueryHandle[nCurrent];
		if ( hQuery != INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE )
		{
			// If it's been issued, but we haven't gotten a result when we polled last time,
			// try polling one last time, since we can't poll again after we issue again.
			if ( m_OcclusionQueryObjects[h].m_bHasBeenIssued[nCurrent] )
			{
				int nPixels = g_pShaderAPI->OcclusionQuery_GetNumPixelsRendered( hQuery, false );
				if ( ( nPixels == OCCLUSION_QUERY_RESULT_PENDING ) && ( m_OcclusionQueryObjects[h].m_nFrameIssued == m_nFrameCount ) )
				{
					static int s_nWarnCount = 0;
					if ( s_nWarnCount++ < 5 )
					{
						DevWarning( "blocking issue in occlusion queries! Grab brian!\n" );
					}
				}
				while( !OCCLUSION_QUERY_FINISHED( nPixels ) ) 
				{
					// We're going to reuse this query, so issue a flush to force the query results to come back.
					nPixels = g_pShaderAPI->OcclusionQuery_GetNumPixelsRendered( hQuery, true );
				}
				if ( nPixels >= 0 )
				{
					m_OcclusionQueryObjects[h].m_LastResult = nPixels;
				}
				m_OcclusionQueryObjects[h].m_bHasBeenIssued[nCurrent] = false;
			}
			g_pShaderAPI->BeginOcclusionQueryDrawing( hQuery );
		}
	}
}


void COcclusionQueryMgr::EndOcclusionQueryDrawing( OcclusionQueryObjectHandle_t hOcclusionQuery )
{
	int h = (int)hOcclusionQuery;
	Assert( m_OcclusionQueryObjects.IsValidIndex( h ) );
	if ( m_OcclusionQueryObjects.IsValidIndex( h ) )
	{
		int nCurrent = m_OcclusionQueryObjects[h].m_nCurrentIssue;
		ShaderAPIOcclusionQuery_t hQuery = m_OcclusionQueryObjects[h].m_QueryHandle[nCurrent];
		if ( hQuery != INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE )
		{
			g_pShaderAPI->EndOcclusionQueryDrawing( hQuery );

			m_OcclusionQueryObjects[h].m_bHasBeenIssued[nCurrent] = true;
			m_OcclusionQueryObjects[h].m_nFrameIssued = m_nFrameCount;

			nCurrent = ( nCurrent + 1 ) % COUNT_OCCLUSION_QUERY_STACK;
			m_OcclusionQueryObjects[h].m_nCurrentIssue = nCurrent;
		}
	}
}


//-----------------------------------------------------------------------------
// Get the number of pixels rendered between begin and end on an earlier frame.
// Calling this in the same frame is a huge perf hit!
//-----------------------------------------------------------------------------
void COcclusionQueryMgr::OcclusionQuery_IssueNumPixelsRenderedQuery( OcclusionQueryObjectHandle_t hOcclusionQuery )
{
	int h = (int)hOcclusionQuery;
	Assert( m_OcclusionQueryObjects.IsValidIndex( h ) );
	if ( m_OcclusionQueryObjects.IsValidIndex( h ) )
	{
		for( int i = 0; i < COUNT_OCCLUSION_QUERY_STACK; i++ )
		{
			int nIndex = ( m_OcclusionQueryObjects[h].m_nCurrentIssue + i ) % COUNT_OCCLUSION_QUERY_STACK;
			ShaderAPIOcclusionQuery_t hQuery = m_OcclusionQueryObjects[h].m_QueryHandle[nIndex];
			if ( hQuery != INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE && m_OcclusionQueryObjects[h].m_bHasBeenIssued[nIndex] )
			{
				int nPixels = g_pShaderAPI->OcclusionQuery_GetNumPixelsRendered( hQuery );
				if ( nPixels == OCCLUSION_QUERY_RESULT_ERROR )
				{
					// In GL mode, it's possible for queries to fail (say when mat_queue_mode is toggled). In this case, just clear m_bHasBeenIssued and forget we ever issued this query.
					m_OcclusionQueryObjects[h].m_bHasBeenIssued[nIndex] = false;
				}
				else if ( nPixels >= 0 )
				{
					m_OcclusionQueryObjects[h].m_LastResult = nPixels;
					m_OcclusionQueryObjects[h].m_bHasBeenIssued[nIndex] = false;
				}
			}
		}
	}
}

int COcclusionQueryMgr::OcclusionQuery_GetNumPixelsRendered( OcclusionQueryObjectHandle_t h, bool bDoQuery )
{
	if ( bDoQuery )
	{
		OcclusionQuery_IssueNumPixelsRenderedQuery( h );
	}

	int nPixels = m_OcclusionQueryObjects[(int)h].m_LastResult;
	return nPixels;
}

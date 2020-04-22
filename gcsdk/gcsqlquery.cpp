//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "stdafx.h"

#include "tier0/memdbgoff.h"

namespace GCSDK
{
IMPLEMENT_CLASS_MEMPOOL( CGCSQLQuery, 1000, UTLMEMORYPOOL_GROW_SLOW );
IMPLEMENT_CLASS_MEMPOOL( CGCSQLQueryGroup, 1000, UTLMEMORYPOOL_GROW_SLOW );
}

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{
//----------------------------------------------------------------------------
// Purpose: Constructor. 
//----------------------------------------------------------------------------
// create queries on the heap with Alloc
CGCSQLQuery::CGCSQLQuery()
{
	m_pBufParams = GetBufferPool().GetBuffer();
}

//----------------------------------------------------------------------------
// Purpose: Destructor. 
//----------------------------------------------------------------------------
CGCSQLQuery::~CGCSQLQuery()
{
	// free all the data
	ClearParams();
	GetBufferPool().ReturnBuffer( m_pBufParams );
}


//----------------------------------------------------------------------------
// Purpose: Gets a singleton buffer pool for bind params
//----------------------------------------------------------------------------
static GCConVar sql_bind_param_max_pool_size_mb( "sql_bind_param_max_pool_size_mb", "10", "Maximum size in bytes of the SQL Bind Param buffer pool" );
static GCConVar sql_bind_param_init_buffer_size( "sql_bind_param_init_buffer_size", "16384", "Initial buffer size for buffers in the SQL Bind Param buffer pool" );
/*static*/ CBufferPool &CGCSQLQuery::GetBufferPool()
{
	static CBufferPool s_bufferPool( "SQL bind params", sql_bind_param_max_pool_size_mb, sql_bind_param_init_buffer_size );
	return s_bufferPool;
}


//----------------------------------------------------------------------------
// Purpose: Allocates a bit of memory for the parameter and adds it to the list
//----------------------------------------------------------------------------
void CGCSQLQuery::AddBindParamRaw( EGCSQLType eType, const byte *pubData, uint32 cubData )
{
	Assert( m_vecParams.Count() < k_cMaxBindParams );
	if( m_vecParams.Count() >= k_cMaxBindParams )
		return;

	GCSQLBindParam_t &param = m_vecParams[ m_vecParams.AddToTail() ];
	param.m_eType = eType;
	param.m_cubData = cubData;

	if ( cubData )
	{
		param.m_nOffset = m_pBufParams->TellPut();
		m_pBufParams->Put( pubData, cubData );
	}
	else 
	{
		param.m_nOffset = 0;
	}
}


//----------------------------------------------------------------------------
// Purpose: frees all the bind params
//----------------------------------------------------------------------------
void CGCSQLQuery::ClearParams()
{
	m_vecParams.RemoveAll();
	m_pBufParams->Clear();
}


//----------------------------------------------------------------------------
// Purpose: Constructor. 
//----------------------------------------------------------------------------
// create queries on the heap with Alloc
CGCSQLQueryGroup::CGCSQLQueryGroup()
: m_pResults( NULL ) 
{

}


//----------------------------------------------------------------------------
// Purpose: Sets the result pointer
//----------------------------------------------------------------------------
CGCSQLQueryGroup::~CGCSQLQueryGroup()
{
	// free all the data
	Clear();

	if( m_pResults )
		m_pResults->Destroy();
}


//----------------------------------------------------------------------------
// Purpose: Adds a new query to the group
//----------------------------------------------------------------------------
void CGCSQLQueryGroup::AddQuery( CGCSQLQuery *pQuery )
{
	m_vecQueries.AddToTail( pQuery );
}


//----------------------------------------------------------------------------
// Purpose: Sets the name (file and line) for the group
//----------------------------------------------------------------------------
void CGCSQLQueryGroup::SetName( const char *sName )
{
	m_sName = sName;
}


//----------------------------------------------------------------------------
// Purpose: clears all the queries in the query group and resets its name
//----------------------------------------------------------------------------
void CGCSQLQueryGroup::Clear()
{
	m_vecQueries.PurgeAndDeleteElements();
	m_sName = "";
}


//----------------------------------------------------------------------------
// Purpose: Sets the result pointer
//----------------------------------------------------------------------------
void CGCSQLQueryGroup::SetResults( IGCSQLResultSetList *pResults ) 
{ 
	if( m_pResults )
		m_pResults->Destroy();
	m_pResults = pResults; 
}

} // namespace GCSDK

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================


#include "mdllib_utils.h"


//////////////////////////////////////////////////////////////////////////
//
// CInsertionTracker implementation
//
//////////////////////////////////////////////////////////////////////////


void CInsertionTracker::InsertBytes( void *pos, int length )
{
	if ( length <= 0 )
		return;

	Assert( m_map.InvalidIndex() == m_map.Find( ( byte * ) pos ) );
	m_map.InsertOrReplace( ( byte * ) pos, length );
}

int CInsertionTracker::GetNumBytesInserted() const
{
	int iInserted = 0;

	for ( Map::IndexType_t idx = m_map.FirstInorder();
		idx != m_map.InvalidIndex(); idx = m_map.NextInorder( idx ) )
	{
		int numBytes = m_map.Element( idx );
		iInserted += numBytes;
	}

	return iInserted;
}

void CInsertionTracker::Finalize()
{
	// Iterate the map and find all the adjacent removal data blocks
	// TODO:
}

void CInsertionTracker::MemMove( void *ptrBase, int &length ) const
{
	int numBytesInsertReq = GetNumBytesInserted();
	byte *pbBlockEnd = BYTE_OFF_PTR( ptrBase, length );
	length += numBytesInsertReq;

	for ( Map::IndexType_t idx = m_map.LastInorder();
		idx != m_map.InvalidIndex(); idx = m_map.PrevInorder( idx ) )
	{
		byte *ptr = m_map.Key( idx );
		int numBytes = m_map.Element( idx );

		// Move [ptr, pbBlockEnd) ->> + numBytesInsertReq
		memmove( BYTE_OFF_PTR( ptr, numBytesInsertReq ), ptr, BYTE_DIFF_PTR( ptr, pbBlockEnd ) );

		// Inserted data
		memset( BYTE_OFF_PTR( ptr, numBytesInsertReq - numBytes ), 0, numBytes );

		numBytesInsertReq -= numBytes;
		pbBlockEnd = ptr;
	}
}

int CInsertionTracker::ComputeOffset( void *ptrBase, int off ) const
{
	void *ptrNewBase = ComputePointer( ptrBase );
	void *ptrNewData = ComputePointer( BYTE_OFF_PTR( ptrBase, off ) );
	return BYTE_DIFF_PTR( ptrNewBase, ptrNewData );
}

void * CInsertionTracker::ComputePointer( void *ptrNothingInserted ) const
{
	int iInserted = 0;

	// Iterate the map and find all the data that would be inserted before the given pointer
	for ( Map::IndexType_t idx = m_map.FirstInorder();
		idx != m_map.InvalidIndex(); idx = m_map.NextInorder( idx ) )
	{
		if ( m_map.Key( idx ) < ptrNothingInserted )
			iInserted += m_map.Element( idx );
		else
			break;
	}

	return BYTE_OFF_PTR( ptrNothingInserted, iInserted );
}



//////////////////////////////////////////////////////////////////////////
//
// CRemoveTracker implementation
//
//////////////////////////////////////////////////////////////////////////


void CRemoveTracker::RemoveBytes( void *pos, int length )
{
	if ( length <= 0 )
		return;

	// -- hint
	if ( m_map.Count() )
	{
		if ( m_hint.ptr < pos )
		{
			if ( BYTE_OFF_PTR( m_hint.ptr, m_hint.len ) == pos )
			{
				m_hint.len += length;
				m_map.Element( m_hint.idx ) = m_hint.len;
				return;
			}
		}
		else if ( m_hint.ptr > pos )
		{
			if ( BYTE_OFF_PTR( pos, length ) == m_hint.ptr )
			{
				m_hint.len += length;
				m_hint.ptr = BYTE_OFF_PTR( m_hint.ptr, - length );
				m_map.Key( m_hint.idx ) = m_hint.ptr;
				m_map.Element( m_hint.idx ) = m_hint.len;
				return;
			}
		}
	}
	// -- end hint

	// Insert new
	Assert( m_map.InvalidIndex() == m_map.Find( ( byte * ) pos ) );
	Map::IndexType_t idx = m_map.InsertOrReplace( ( byte * ) pos, length );
	
	// New hint
	m_hint.idx = idx;
	m_hint.ptr = ( byte * ) pos;
	m_hint.len = length;
}

int CRemoveTracker::GetNumBytesRemoved() const
{
	int iRemoved = 0;

	for ( Map::IndexType_t idx = m_map.FirstInorder();
		idx != m_map.InvalidIndex(); idx = m_map.NextInorder( idx ) )
	{
		int numBytes = m_map.Element( idx );
		iRemoved += numBytes;
	}

	return iRemoved;
}

void CRemoveTracker::Finalize()
{
	// Iterate the map and find all the adjacent removal data blocks
	// TODO:
}

void CRemoveTracker::MemMove( void *ptrBase, int &length ) const
{
	int iRemoved = 0;

	for ( Map::IndexType_t idx = m_map.FirstInorder();
		idx != m_map.InvalidIndex(); idx = m_map.NextInorder( idx ) )
	{
		byte *ptr = m_map.Key( idx );
		byte *ptrDest = BYTE_OFF_PTR( ptr, - iRemoved );
		int numBytes = m_map.Element( idx );
		memmove( ptrDest, BYTE_OFF_PTR( ptrDest, numBytes ), BYTE_DIFF_PTR( BYTE_OFF_PTR( ptr, numBytes ), BYTE_OFF_PTR( ptrBase, length ) ) );
		iRemoved += numBytes;
	}

	length -= iRemoved;
}

int CRemoveTracker::ComputeOffset( void *ptrBase, int off ) const
{
	void *ptrNewBase = ComputePointer( ptrBase );
	void *ptrNewData = ComputePointer( BYTE_OFF_PTR( ptrBase, off ) );
	return BYTE_DIFF_PTR( ptrNewBase, ptrNewData );
}

void * CRemoveTracker::ComputePointer( void *ptrNothingRemoved ) const
{
	int iRemoved = 0;

	// Iterate the map and find all the data that would be removed before the given pointer
	for ( Map::IndexType_t idx = m_map.FirstInorder();
		idx != m_map.InvalidIndex(); idx = m_map.NextInorder( idx ) )
	{
		if ( m_map.Key( idx ) < ptrNothingRemoved )
			iRemoved += m_map.Element( idx );
		else
			break;
	}

	return BYTE_OFF_PTR( ptrNothingRemoved, - iRemoved );
}



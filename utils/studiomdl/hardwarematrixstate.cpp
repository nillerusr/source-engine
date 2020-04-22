//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <windows.h>
#include "HardwareMatrixState.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "studio.h"
#include "studiomdl.h"

CHardwareMatrixState::CHardwareMatrixState()
{
	m_LRUCounter = 0;
	m_NumMatrices = 0;
	m_matrixState = NULL;
	m_savedMatrixState = NULL;
}

void CHardwareMatrixState::Init( int numHardwareMatrices )
{
	m_NumMatrices = numHardwareMatrices;
	delete [] m_matrixState;
	m_matrixState = new MatrixState_t[m_NumMatrices];
	Assert( m_matrixState );
	delete [] m_savedMatrixState;
	m_savedMatrixState = new MatrixState_t[m_NumMatrices];
	Assert( m_savedMatrixState );
	m_LRUCounter = 0;
	m_AllocatedMatrices = 0;

	int i;
	for( i = 0; i < m_NumMatrices; i++ )
	{
		m_matrixState[i].allocated = false;
	}
}

bool CHardwareMatrixState::AllocateMatrix( int globalMatrixID )
{
	int i;

	if( IsMatrixAllocated( globalMatrixID ) )
	{
		return true;
	}

	for( i = 0; i < m_NumMatrices; i++ )
	{
		if( !m_matrixState[i].allocated )
		{
			m_matrixState[i].globalMatrixID = globalMatrixID;
			m_matrixState[i].allocated = true;
			m_matrixState[i].lastUsageID = m_LRUCounter++;
			++m_AllocatedMatrices;
			DumpState();
			return true;
		}
	}
	DumpState();
	return false;
}

int CHardwareMatrixState::FindLocalLRUIndex( void )
{
	int oldestLRUCounter = INT_MAX;
	int i;
	int oldestID = 0;

	for( i = 0; i < m_NumMatrices; i++ )
	{
		if( !m_matrixState[i].allocated )
		{
			continue;
		}
		if( m_matrixState[i].lastUsageID < oldestLRUCounter )
		{
			oldestLRUCounter = m_matrixState[i].lastUsageID;
			oldestID = i;
		}
	}

	Assert( oldestLRUCounter != INT_MAX );
	return oldestID;
}

void CHardwareMatrixState::DeallocateLRU( void )
{
	int id;

	id = FindLocalLRUIndex();
	m_matrixState[id].allocated = false;
	--m_AllocatedMatrices;
}

void CHardwareMatrixState::DeallocateLRU( int n )
{
	int i;

	for( i = 0; i < n; i++ )
	{
		DeallocateLRU();
	}
}

bool CHardwareMatrixState::IsMatrixAllocated( int globalMatrixID ) const
{
	int i;

	for( i = 0; i < m_NumMatrices; i++ )
	{
		if( m_matrixState[i].globalMatrixID == globalMatrixID && 
			m_matrixState[i].allocated )
		{
			return true;
		}
	}
	return false;
}

void CHardwareMatrixState::DeallocateAll()
{
	int i;

	DumpState();
	for( i = 0; i < m_NumMatrices; i++ )
	{
		m_matrixState[i].allocated = false;
		m_matrixState[i].globalMatrixID = INT_MAX;
		m_matrixState[i].lastUsageID = INT_MAX;
	}
	m_AllocatedMatrices = 0;
	DumpState();
}

void CHardwareMatrixState::SaveState( void )
{
	int i;

	for( i = 0; i < m_NumMatrices; i++ )
	{
		m_savedMatrixState[i] = m_matrixState[i];
	}
}

void CHardwareMatrixState::RestoreState( void )
{
	int i;

	for( i = 0; i < m_NumMatrices; i++ )
	{
		m_matrixState[i] = m_savedMatrixState[i];
	}
}

int CHardwareMatrixState::AllocatedMatrixCount() const
{
	return m_AllocatedMatrices;
}

int CHardwareMatrixState::FreeMatrixCount() const
{
	return m_NumMatrices - m_AllocatedMatrices;
}

int CHardwareMatrixState::GetNthBoneGlobalID( int n ) const
{
	int i;
	int m = 0;

	for( i = 0; i < m_NumMatrices; i++ )
	{
		if( m_matrixState[i].allocated )
		{
			if( n == m )
			{
				return m_matrixState[i].globalMatrixID;
			}
			m++;
		}
	}
	Assert( 0 );
	MdlError( "GetNthBoneGlobalID() Failure\n" );
	return 0;
}

void CHardwareMatrixState::DumpState( void )
{
	int i;
	static char buf[256];

//#ifndef _DEBUG
	return;
//#endif
	
	OutputDebugString( "DumpState\n:" );
	for( i = 0; i < m_NumMatrices; i++ )
	{
		if( m_matrixState[i].allocated )
		{
			sprintf( buf, "%d: allocated: %s lastUsageID: %d globalMatrixID: %d\n",
				i, 
				m_matrixState[i].allocated ? "true " : "false",
				m_matrixState[i].lastUsageID,
				m_matrixState[i].globalMatrixID );
			OutputDebugString( buf );
		}
	}
}

int CHardwareMatrixState::FindHardwareMatrix( int globalMatrixID )
{
	int i;

	for( i = 0; i < m_NumMatrices; i++ )
	{
		if( m_matrixState[i].globalMatrixID == globalMatrixID )
		{
			return i;
		}
	}

	Assert( 0 );
	MdlError( "barfing in FindHardwareMatrix\n" );

	return 0;
}


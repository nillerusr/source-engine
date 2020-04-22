//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdlib.h>
#include <stdio.h>
#include "HardwareVertexCache.h"

CHardwareVertexCache::CHardwareVertexCache()
{
	m_Fifo = NULL;
	m_Size = 0;
	Flush();
}

void CHardwareVertexCache::Init( int size )
{
	m_Size = size;
	m_Fifo = new int[size];
	Flush();
}

void CHardwareVertexCache::Flush( void )
{
	m_HeadIndex = 0;
	m_NumEntries = 0;
}

bool CHardwareVertexCache::IsPresent( int index )
{
	int i;
//	printf( "testing if %d is present\n", index );
	for( i = 0; i < m_NumEntries; i++ )
	{
		if( m_Fifo[( m_HeadIndex + i ) % m_Size] == index )
		{
//			printf( "yes!\n" );
			return true;
		}
	}
//	printf( "no!\n" );
//	Print();
	return false;
}

void CHardwareVertexCache::Insert( int index )
{
//	printf( "Inserting: %d\n", index );
	m_Fifo[( m_HeadIndex + m_NumEntries ) % m_Size] = index;
	if( m_NumEntries == m_Size )
	{
		m_HeadIndex = ( m_HeadIndex + 1 ) % m_Size;
	}
	else
	{
		m_NumEntries++;
	}
//	Print();
}

void CHardwareVertexCache::Print( void )
{
	int i;
	for( i = 0; i < m_NumEntries; i++ )
	{
		printf( "fifo entry %d: %d\n", i, ( int )m_Fifo[( m_HeadIndex + i ) % m_Size] );
	}
}

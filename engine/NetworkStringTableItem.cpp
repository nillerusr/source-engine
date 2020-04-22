//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "quakedef.h"
#include "networkstringtableitem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNetworkStringTableItem::CNetworkStringTableItem( void )
{
	m_pUserData = NULL;
	m_nUserDataLength = 0;
	m_nTickChanged = 0;

#ifndef SHARED_NET_STRING_TABLES
	m_nTickCreated = 0;
	m_pChangeList = NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNetworkStringTableItem::~CNetworkStringTableItem( void )
{
#ifndef SHARED_NET_STRING_TABLES
	if ( m_pChangeList )
	{
		// free changelist and elements

		for ( int i=0; i < m_pChangeList->Count(); i++ )
		{
			itemchange_s item = m_pChangeList->Element( i );

			if ( item.data )
				delete[] item.data;
		}

		delete m_pChangeList; // destructor calls Purge()

		m_pUserData = NULL;
	}
#endif
		
	if ( m_pUserData )
	{
		delete[] m_pUserData;
	}
}

#ifndef SHARED_NET_STRING_TABLES
void CNetworkStringTableItem::EnableChangeHistory( void )
{
	if ( m_pChangeList )
		return; // already enabled

	m_pChangeList = new CUtlVector<itemchange_s>;

	Assert ( m_pChangeList );
}

void CNetworkStringTableItem::UpdateChangeList( int tick, int length, const void *userData )
{
	int count = m_pChangeList->Count();
	itemchange_s item;

	if ( count > 0 )
	{	
		// check if different from last change in list
		item = m_pChangeList->Element( count-1 );

		if ( !item.data && !userData )
			return; // both NULL data

		if ( item.length == length )
		{
			if ( item.data && userData )
			{
				if ( Q_memcmp( (void*)userData, (void*)item.data, length ) == 0 )
				{
					return; // no data or size change
				}
			}
		}

		if ( item.tick == tick )
		{
			// two changes within same tick frame, remove last change from list
			if ( item.data )
			{
				delete[] item.data;
			}

			m_pChangeList->Remove( count-1 );
		}
				
	}

	item.tick = tick;

	// add new user data and time stamp

	if ( userData && length )
	{
		item.data = new unsigned char[length];
		item.length = length;
		Q_memcpy( item.data, userData, length );
	}
	else
	{
		item.data = NULL;
		item.length = 0;
	}

	m_pChangeList->AddToTail( item );
}

int CNetworkStringTableItem::RestoreTick( int tick )
{
	Assert( m_pChangeList->Count()>0 );

	int index = 1;

	itemchange_s *item = &m_pChangeList->Element( 0 );

	while ( index < m_pChangeList->Count() )
	{
		itemchange_s *nextitem = &m_pChangeList->Element( index );

		if ( nextitem->tick > tick )
		{
			break;
		}

		item = nextitem;
		index++;
	}

	if ( item->tick > tick )
	{
		// this string was created after tick, so ignore it right now
		m_pUserData = NULL;
		m_nUserDataLength = 0;
		m_nTickChanged = 0;
	}
	else
	{
		// restore user data for this string
		m_pUserData = item->data;
		m_nUserDataLength = item->length;
		m_nTickChanged = item->tick;
	}

	return m_nTickChanged;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *string - 
//-----------------------------------------------------------------------------
bool CNetworkStringTableItem::SetUserData( int tick, int length, const void *userData )
{

#ifndef SHARED_NET_STRING_TABLES
	if ( m_pChangeList )
	{
		UpdateChangeList( tick, length, userData );
		return false;
	}
	Assert ( m_nTickCreated > 0 && m_nTickCreated <= tick );
#endif

	Assert ( m_nTickChanged > 0 && m_nTickChanged <= tick );
	Assert ( length < CNetworkStringTableItem::MAX_USERDATA_SIZE );

	// no old or new data
	if ( !userData && !m_pUserData )
		return false;

	if ( m_pUserData && 
		length == m_nUserDataLength &&
		!Q_memcmp( m_pUserData, (void*)userData, length ) )
	{
		return false; // old & new data are equal
	}

	if ( m_pUserData )
		delete[] m_pUserData;

	m_nUserDataLength = length;

	if ( length > 0 )
	{
		m_pUserData = new unsigned char[ length ];
		Q_memcpy( m_pUserData, userData, length );
	}
	else
	{
		m_pUserData = NULL; 
	}

	m_nTickChanged = tick;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : stringNumber - 
// Output : const void
//-----------------------------------------------------------------------------
const void *CNetworkStringTableItem::GetUserData( int *length )
{
	if ( length )
		*length = m_nUserDataLength;

	return ( const void * )m_pUserData;
}







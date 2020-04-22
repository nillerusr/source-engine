//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A network message that sends chat messages
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "networkmessages.h"
#include "networkmanager.h"
#include "tier1/bitbuf.h"


//-----------------------------------------------------------------------------
// Chat network message
//-----------------------------------------------------------------------------
REGISTER_NETWORK_MESSAGE( CNetworkMessage_Chat );

CNetworkMessage_Chat::CNetworkMessage_Chat()
{ 
	SetReliable( false ); 
}

bool CNetworkMessage_Chat::WriteToBuffer( bf_write &buffer )
{
	int nLen = m_Message.Length();
	buffer.WriteShort( nLen );
	if ( nLen )
	{
		buffer.WriteBytes( m_Message.Get(), nLen );
	}
	return !buffer.IsOverflowed();
}

bool CNetworkMessage_Chat::ReadFromBuffer( bf_read &buffer )
{
	int nLen = buffer.ReadShort();
	if ( nLen )
	{
		m_Message.SetLength( nLen + 1 );
		char *pDest = m_Message.Get();
		buffer.ReadBytes( pDest, nLen );
		pDest[ nLen ] = 0;
	}
	else
	{
		m_Message.Set( NULL );
	}
	return !buffer.IsOverflowed();
}


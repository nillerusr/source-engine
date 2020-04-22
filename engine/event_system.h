//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#if !defined( EVENT_SYSTEM_H )
#define EVENT_SYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "event_flags.h"
#include "common.h"
#include "enginesingleuserfilter.h"


class SendTable;
class ClientClass;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEventInfo
{
public:
	enum
	{
		EVENT_INDEX_BITS = 8,
		EVENT_DATA_LEN_BITS = 11,
		MAX_EVENT_DATA = 192,  // ( 1<<8 bits == 256, but only using 192 below )
	};

	inline CEventInfo()
	{
		classID = 0;
		fire_delay = 0.0f;
		bits = 0;
		flags = 0;
		pSendTable = NULL;
		pClientClass = NULL;
		pData = NULL;
	}

	~CEventInfo()
	{
		if ( pData )
		{
			delete pData;
		}
	}

	CEventInfo( const CEventInfo& src )
	{
		classID = src.classID;
		fire_delay = src.fire_delay;
		bits = src.bits;
		flags = src.flags;
		pSendTable = src.pSendTable;
		pClientClass = src.pClientClass;
		filter.AddPlayersFromFilter( &src.filter );
				
		if ( src.pData )
		{
			int size = Bits2Bytes( src.bits );
			pData = new byte[size];
			Q_memcpy( pData, src.pData, size );
		}
		else
		{
			pData = NULL;
		}

	}

	// 0 implies not in use
	short classID;
	
	// If non-zero, the delay time when the event should be fired ( fixed up on the client )
	float fire_delay;

	// send table pointer or NULL if send as full update
	const SendTable *pSendTable;
	const ClientClass *pClientClass;
	
	// Length of data bits
	int		bits;
	// Raw event data
	byte	*pData;
	// CLIENT ONLY Reliable or not, etc.
	int		flags;
	
	// clients that see that event
	CEngineRecipientFilter filter;
};


#endif // EVENT_SYSTEM_H

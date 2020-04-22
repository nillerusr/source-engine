//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "client_pch.h"
#include "dt_recv_eng.h"
#include "client_class.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar cl_showevents	( "cl_showevents", "0", FCVAR_CHEAT, "Print event firing info in the console" );

//-----------------------------------------------------------------------------
// Purpose: Show descriptive info about an event in the numbered console area
// Input  : slot - 
//			*eventname - 
//-----------------------------------------------------------------------------
void CL_DescribeEvent( int slot, CEventInfo *event, const char *eventname )
{
	int idx = (slot & 31);

	if ( !cl_showevents.GetInt() )
		return;

	if ( !eventname )
		return;

	con_nprint_t n;
	n.index = idx;
	n.fixed_width_font = true;
	n.time_to_live = 4.0f;
	n.color[0] = 0.8;
	n.color[1] = 0.8;
	n.color[2] = 1.0;

	Con_NXPrintf( &n, "%02i %6.3ff %20s %03i bytes", slot, cl.GetTime(), eventname, Bits2Bytes( event->bits ) );

	if ( cl_showevents.GetInt() == 2 )
	{
		DevMsg( "%02i %6.3ff %20s %03i bytes\n", slot, cl.GetTime(), eventname, Bits2Bytes( event->bits ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Decode raw event data into underlying class structure using the specified data table
// Input  : *RawData - 
//			*pToData - 
//			*pRecvTable - 
//-----------------------------------------------------------------------------
void CL_ParseEventDelta( void *RawData, void *pToData, RecvTable *pRecvTable, unsigned int uReadBufferSize )
{
	// Make sure we have a decoder
	assert(pRecvTable->m_pDecoder);

	// Only so much data allowed
	bf_read fromBuf( "CL_ParseEventDelta->fromBuf", RawData, uReadBufferSize );

	// First, decode all properties as zeros since temp ents are delta'd from zeros.
	RecvTable_DecodeZeros( pRecvTable, pToData, -1 );

	// Now decode the data from the network on top of that.
	RecvTable_Decode( pRecvTable, pToData, &fromBuf, -1 );

	// Make sure the server, etc. didn't try to send too much
	assert(!fromBuf.IsOverflowed());
}

//-----------------------------------------------------------------------------
// Purpose: Once per frame, walk the client's event slots and look for any events
//  that are ready for playing.
//-----------------------------------------------------------------------------
void CL_FireEvents( void )
{
	VPROF("CL_FireEvents");
	if ( !cl.IsActive() )
	{
		cl.events.RemoveAll();
		return;
	}

	int i, next;
	for ( i = cl.events.Head(); i != cl.events.InvalidIndex(); i = next )
	{
		next = cl.events.Next( i );

		CEventInfo *ei = &cl.events[ i ];
		if ( ei->classID == 0 )
		{
			cl.events.Remove( i );
			continue;
		}

		// Delayed event!
		if ( ei->fire_delay && ( ei->fire_delay > cl.GetTime() ) )
			continue;

		bool success = false;

		// Get the receive table if it exists
		Assert( ei->pClientClass );
				
		// Get pointer to the event.
		if( ei->pClientClass->m_pCreateEventFn )
		{
			IClientNetworkable *pCE = ei->pClientClass->m_pCreateEventFn();
			if(pCE)
			{
				// Prepare to copy in the data
				pCE->PreDataUpdate( DATA_UPDATE_CREATED );

				// Decode data into client event object
				unsigned int buffer_size = PAD_NUMBER( Bits2Bytes( ei->bits ), 4 );
				CL_ParseEventDelta( ei->pData, pCE->GetDataTableBasePtr(), ei->pClientClass->m_pRecvTable, buffer_size );

				// Fire the event!!!
				pCE->PostDataUpdate( DATA_UPDATE_CREATED );

				// Spew to debug area if needed
				CL_DescribeEvent( i, ei, ei->pClientClass->m_pNetworkName );

				success = true;
			}
		}

		if ( !success )
		{
			ConDMsg( "Failed to execute event for classId %i\n", ei->classID - 1 );
		}

		cl.events.Remove( i );
	}
}



//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "CompletionEvent.h"
#include "winlite.h"

//-----------------------------------------------------------------------------
// Purpose: creates an event
//-----------------------------------------------------------------------------
EventHandle_t Event_CreateEvent()
{
	return (EventHandle_t)::CreateEvent(NULL, false, false, NULL);
}

//-----------------------------------------------------------------------------
// Purpose: sets the current thread to wait for either the event to be signalled, or the timeout to occur
//-----------------------------------------------------------------------------
void Event_WaitForEvent(EventHandle_t event, unsigned long timeoutMilliseconds)
{
	::WaitForSingleObject((HANDLE)event, timeoutMilliseconds);
}

//-----------------------------------------------------------------------------
// Purpose: signals an event to Activate
//			Releases one thread waiting on the event.  
//			If the event has no threads waiting on it, the next thread to wait on it will be let right through
//-----------------------------------------------------------------------------
void Event_SignalEvent(EventHandle_t event)
{
	::SetEvent((HANDLE)event);
}



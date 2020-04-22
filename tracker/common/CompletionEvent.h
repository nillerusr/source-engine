//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Wraps windows WaitForSingleEvent() calls
//
// $NoKeywords: $
//=============================================================================

#ifndef COMPLETIONEVENT_H
#define COMPLETIONEVENT_H
#ifdef _WIN32
#pragma once
#endif

// handle to an event
typedef unsigned long EventHandle_t;

// creates an event
EventHandle_t Event_CreateEvent();

// sets the current thread to wait for either the event to be signalled, or the timeout to occur
void Event_WaitForEvent(EventHandle_t event, unsigned long timeoutMilliseconds);

// set the timeout to this for it to never time out
#define TIMEOUT_INFINITE 0xFFFFFFFF

// signals an event to Activate
// Releases one thread waiting on the event.  
// If the event has no threads waiting on it, the next thread to wait on it will be let right through
void Event_SignalEvent(EventHandle_t event);


#endif // COMPLETIONEVENT_H

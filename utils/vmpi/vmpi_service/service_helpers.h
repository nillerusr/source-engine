//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SERVICE_HELPERS_H
#define SERVICE_HELPERS_H
#ifdef _WIN32
#pragma once
#endif


// Call this if you want to use the ExitEarly() and ShouldExit() helpers.
void ServiceHelpers_Init();

// Start this app in the service control manager.
//
// The service will run in a thread. If the service starts successfully, then
// it will call pFn and pass in pParam. Inside there, you should loop until
// ShouldServiceExit() returns true.
bool ServiceHelpers_StartService( const char *pServiceName, void (*pFn)( void *pParam ), void *pParam );


// Call this to exit the service early. This will make ShouldServiceExit() return true,
// and your main thread function should pick it up and exit.
//
// NOTE: this can be used even if the service isn't running as long as you call ServiceHelpers_Init().
void ServiceHelpers_ExitEarly();

// Your thread loop should call this each time around. If this function returns true,
// then your thread function should return, causing the service to exit.
//
// NOTE: this can be used even if the service isn't running as long as you call ServiceHelpers_Init().
bool ServiceHelpers_ShouldExit();


// This function wants a better home.
char* GetLastErrorString();


#endif // SERVICE_HELPERS_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "service_helpers.h"


static CRITICAL_SECTION g_CtrlHandlerMutex;

static void (*g_pInternalServiceFn)( void *pParam ) = NULL;
static void *g_pInternalServiceParam = NULL;

static volatile bool g_bShouldExit = false;


SERVICE_STATUS          MyServiceStatus; 
SERVICE_STATUS_HANDLE   MyServiceStatusHandle = NULL;


void WINAPI MyServiceCtrlHandler( DWORD Opcode )
{ 
    DWORD status; 
 
    switch(Opcode) 
    { 
        case SERVICE_CONTROL_STOP: 
			// Do whatever it takes to stop here. 
			ServiceHelpers_ExitEarly();

            MyServiceStatus.dwWin32ExitCode = 0; 
            MyServiceStatus.dwCurrentState  = SERVICE_STOPPED; 
 
            if ( !SetServiceStatus( MyServiceStatusHandle, &MyServiceStatus) )
            { 
                status = GetLastError(); 
                Msg( "[MY_SERVICE] SetServiceStatus error %ld\n", status ); 
            } 
 
            Msg( "[MY_SERVICE] Leaving MyService \n" ); 
            return; 
 
        case SERVICE_CONTROL_INTERROGATE: 
			// Fall through to send current status. 
            break; 
 
        default: 
            Msg("[MY_SERVICE] Unrecognized opcode %ld\n", Opcode ); 
    } 
 
    // Send current status. 
    if ( !SetServiceStatus( MyServiceStatusHandle, &MyServiceStatus ) )
    { 
        status = GetLastError(); 
        Msg( "[MY_SERVICE] SetServiceStatus error %ld\n", status ); 
    } 
}


void WINAPI MyServiceStart( DWORD argc, LPTSTR *argv ) 
{ 
    DWORD status; 

    MyServiceStatus.dwServiceType        = SERVICE_WIN32; 
    MyServiceStatus.dwCurrentState       = SERVICE_START_PENDING; 
    MyServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP; 
    MyServiceStatus.dwWin32ExitCode      = 0; 
    MyServiceStatus.dwServiceSpecificExitCode = 0; 
    MyServiceStatus.dwCheckPoint         = 0; 
    MyServiceStatus.dwWaitHint           = 0; 
 
    MyServiceStatusHandle = RegisterServiceCtrlHandler( "MyService", MyServiceCtrlHandler ); 
    if ( MyServiceStatusHandle == (SERVICE_STATUS_HANDLE)0 ) 
    { 
        Msg("[MY_SERVICE] RegisterServiceCtrlHandler failed %d\n", GetLastError() );
        return; 
    } 

    // Initialization complete - report running status. 
    MyServiceStatus.dwCurrentState       = SERVICE_RUNNING; 
 
    if ( !SetServiceStatus( MyServiceStatusHandle, &MyServiceStatus ) )
    { 
        status = GetLastError(); 
        Msg( "[MY_SERVICE] SetServiceStatus error %ld\n", status );
    } 


	// Run the app's main in-thread loop.    
	g_pInternalServiceFn( g_pInternalServiceParam );


	// Tell the SCM that we're stopped.        
	MyServiceStatus.dwCurrentState       = SERVICE_STOPPED; 
    MyServiceStatus.dwWin32ExitCode      = NO_ERROR;
    MyServiceStatus.dwServiceSpecificExitCode = 0; 
    SetServiceStatus( MyServiceStatusHandle, &MyServiceStatus );


    // This is where the service does its work. 
    Msg( "[MY_SERVICE] Returning the Main Thread \n" ); 
}


void ServiceHelpers_Init()
{
	InitializeCriticalSection( &g_CtrlHandlerMutex );
}


bool ServiceHelpers_StartService( const char *pServiceName, void (*pFn)( void *pParam ), void *pParam )
{
	// Ok, just run the service.
	const SERVICE_TABLE_ENTRY DispatchTable[2] = 
	{
		{ (char*)pServiceName, MyServiceStart },
		{ NULL, NULL }
	};

	g_pInternalServiceFn = pFn;
	g_pInternalServiceParam = pParam;

	if ( StartServiceCtrlDispatcher( DispatchTable ) ) 
	{
		return true;
	}
	else
	{ 											    
		Msg( "StartServiceCtrlDispatcher error = '%s'\n", GetLastErrorString() ); 
		return false;
	}
}


void ServiceHelpers_ExitEarly()
{
	EnterCriticalSection( &g_CtrlHandlerMutex );
	g_bShouldExit = true;
	LeaveCriticalSection( &g_CtrlHandlerMutex );
}


bool ServiceHelpers_ShouldExit()
{
	EnterCriticalSection( &g_CtrlHandlerMutex );
	bool bRet = g_bShouldExit;
	LeaveCriticalSection( &g_CtrlHandlerMutex );

	return bRet;
}


char* GetLastErrorString()
{
	static char err[2048];
	
	LPVOID lpMsgBuf;
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
	);

	strncpy( err, (char*)lpMsgBuf, sizeof( err ) );
	LocalFree( lpMsgBuf );

	err[ sizeof( err ) - 1 ] = 0;

	return err;
}



//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// ETW (Event Tracing for Windows) profiling helpers.
// This allows easy insertion of Generic Event markers into ETW/xperf tracing
// which then aids in analyzing the traces and finding performance problems.
//
//===============================================================================

#include "pch_tier0.h"
#include "tier0/etwprof.h"
#include <memory>

#ifdef	ETW_MARKS_ENABLED

// After building the DLL if it has never been registered on this machine or
// if the providers have changed you need to go:
//    xcopy /y %vgame%\bin\tier0.dll %temp%
//    wevtutil um %vgame%\..\src\tier0\ValveETWProvider.man
//    wevtutil im %vgame%\..\src\tier0\ValveETWProvider.man

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// These are defined in evntrace.h but you need a Vista+ Windows
// SDK to have them available, so I define them here.
#define EVENT_CONTROL_CODE_DISABLE_PROVIDER 0
#define EVENT_CONTROL_CODE_ENABLE_PROVIDER  1
#define EVENT_CONTROL_CODE_CAPTURE_STATE    2

// EVNTAPI is used in evntprov.h which is included by ValveETWProviderEvents.h
// We define EVNTAPI without the DECLSPEC_IMPORT specifier so that
// we can implement these functions locally instead of using the import library,
// and can therefore still run on Windows XP.
#define EVNTAPI __stdcall
// Include the event register/write/unregister macros compiled from the manifest file.
// Note that this includes evntprov.h which requires a Vista+ Windows SDK
// which we don't currently have, so evntprov.h is checked in.
#include "ValveETWProviderEvents.h"

// Typedefs for use with GetProcAddress
typedef ULONG (__stdcall *tEventRegister)( LPCGUID ProviderId, PENABLECALLBACK EnableCallback, PVOID CallbackContext, PREGHANDLE RegHandle);
typedef ULONG (__stdcall *tEventWrite)( REGHANDLE RegHandle, PCEVENT_DESCRIPTOR EventDescriptor, ULONG UserDataCount, PEVENT_DATA_DESCRIPTOR UserData);
typedef ULONG (__stdcall *tEventUnregister)( REGHANDLE RegHandle );

// Helper class to dynamically load Advapi32.dll, find the ETW functions, 
// register the providers if possible, and get the performance counter frequency.
class CETWRegister
{
public:
	CETWRegister()
	{
		QueryPerformanceFrequency( &m_frequency );

		// Find Advapi32.dll. This should always succeed.
		HMODULE pAdvapiDLL = LoadLibraryW( L"Advapi32.dll" );
		if ( pAdvapiDLL )
		{
			// Try to find the ETW functions. This will fail on XP.
			m_pEventRegister = ( tEventRegister )GetProcAddress( pAdvapiDLL, "EventRegister" );
			m_pEventWrite = ( tEventWrite )GetProcAddress( pAdvapiDLL, "EventWrite" );
			m_pEventUnregister = ( tEventUnregister )GetProcAddress( pAdvapiDLL, "EventUnregister" );

			// Register two ETW providers. If registration fails then the event logging calls will fail.
			// On XP these calls will do nothing.
			// On Vista and above, if these providers have been enabled by xperf or logman then
			// the VALVE_FRAMERATE_Context and VALVE_MAIN_Context globals will be modified
			// like this:
			//     MatchAnyKeyword: 0xffffffffffffffff
			//     IsEnabled: 1
			//     Level: 255
			// In other words, fully enabled.

			EventRegisterValve_FrameRate();
			EventRegisterValve_ServerFrameRate();
			EventRegisterValve_Main();
			EventRegisterValve_Input();
			EventRegisterValve_Network();

			// Emit the thread ID for the main thread. This also indicates that
			// the main provider is initialized.
			EventWriteThread_ID( GetCurrentThreadId(), "Main thread" );
			// Emit an input system event so we know that it is active.
			EventWriteKey_down( "Valve input provider initialized.", 0, 0 );
		}
	}
	~CETWRegister()
	{
		// Unregister our providers.
		EventUnregisterValve_Network();
		EventUnregisterValve_Input();
		EventUnregisterValve_Main();
		EventUnregisterValve_ServerFrameRate();
		EventUnregisterValve_FrameRate();
	}

	tEventRegister m_pEventRegister;
	tEventWrite m_pEventWrite;
	tEventUnregister m_pEventUnregister;

	// QPC frequency
	LARGE_INTEGER m_frequency;

} g_ETWRegister;

// Redirector function for EventRegister. Called by macros in ValveETWProviderEvents.h
ULONG EVNTAPI EventRegister( LPCGUID ProviderId, PENABLECALLBACK EnableCallback, PVOID CallbackContext, PREGHANDLE RegHandle )
{
	if ( g_ETWRegister.m_pEventRegister )
		return g_ETWRegister.m_pEventRegister( ProviderId, EnableCallback, CallbackContext, RegHandle );

	return 0;
}

// Redirector function for EventWrite. Called by macros in ValveETWProviderEvents.h
ULONG EVNTAPI EventWrite( REGHANDLE RegHandle, PCEVENT_DESCRIPTOR EventDescriptor, ULONG UserDataCount, PEVENT_DATA_DESCRIPTOR UserData )
{
	if ( g_ETWRegister.m_pEventWrite )
		return g_ETWRegister.m_pEventWrite( RegHandle, EventDescriptor, UserDataCount, UserData );
	return 0;
}

// Redirector function for EventUnregister. Called by macros in ValveETWProviderEvents.h
ULONG EVNTAPI EventUnregister( REGHANDLE RegHandle )
{
	if ( g_ETWRegister.m_pEventUnregister )
		return g_ETWRegister.m_pEventUnregister( RegHandle );
	return 0;
}

// Call QueryPerformanceCounter
static int64 GetQPCTime()
{
	LARGE_INTEGER time;

	QueryPerformanceCounter( &time );
	return time.QuadPart;
}

// Convert a QueryPerformanceCounter delta into milliseconds
static float QPCToMS( int64 nDelta )
{
	// Convert from a QPC delta to seconds.
	float flSeconds = ( float )( nDelta / double( g_ETWRegister.m_frequency.QuadPart ) );

	// Convert from seconds to milliseconds
	return flSeconds * 1000;
}

// Public functions for emitting ETW events.

int64 ETWMark( const char *pMessage )
{
	int64 nTime = GetQPCTime();
	EventWriteMark( pMessage );
	return nTime;
}

int64 ETWMarkPrintf( const char *pMessage, ... )
{
	// If we are running on Windows XP or if our providers have not been enabled
	// (by xperf or other) then this will be false and we can early out.
	// Be sure to check the appropriate context for the event. This is only
	// worth checking if there is some cost beyond the EventWrite that we can
	// avoid -- the redirectors in this file guarantee that EventWrite is always
	// safe to call.
	if ( !VALVE_MAIN_Context.IsEnabled )
	{
		return 0;
	}

	char buffer[1000];
	va_list args;
	va_start( args, pMessage );
	vsprintf_s( buffer, pMessage, args );
	va_end( args );

	int64 nTime = GetQPCTime();
	EventWriteMark( buffer );
	return nTime;
}

void ETWMark1F( const char *pMessage, float data1 )
{
	EventWriteMark1F( pMessage, data1 );
}

void ETWMark2F( const char *pMessage, float data1, float data2 )
{
	EventWriteMark2F( pMessage, data1, data2 );
}

void ETWMark3F( const char *pMessage, float data1, float data2, float data3 )
{
	EventWriteMark3F( pMessage, data1, data2, data3 );
}

void ETWMark4F( const char *pMessage, float data1, float data2, float data3, float data4 )
{
	EventWriteMark4F( pMessage, data1, data2, data3, data4 );
}

void ETWMark1I( const char *pMessage, int data1 )
{
	EventWriteMark1I( pMessage, data1 );
}

void ETWMark2I( const char *pMessage, int data1, int data2 )
{
	EventWriteMark2I( pMessage, data1, data2 );
}

void ETWMark3I( const char *pMessage, int data1, int data2, int data3 )
{
	EventWriteMark3I( pMessage, data1, data2, data3 );
}

void ETWMark4I( const char *pMessage, int data1, int data2, int data3, int data4 )
{
	EventWriteMark4I( pMessage, data1, data2, data3, data4 );
}

void ETWMark1S( const char *pMessage, const char* data1 )
{
	EventWriteMark1S( pMessage, data1 );
}

void ETWMark2S( const char *pMessage, const char* data1, const char* data2 )
{
	EventWriteMark2S( pMessage, data1, data2 );
}

// Track the depth of ETW Begin/End pairs. This needs to be per-thread
// if we start emitting marks on multiple threads. Using __declspec(thread)
// has some problems on Windows XP, but since these ETW functions only work
// on Vista+ that doesn't matter.
static __declspec( thread ) int s_nDepth;

int64 ETWBegin( const char *pMessage )
{
	// If we are running on Windows XP or if our providers have not been enabled
	// (by xperf or other) then this will be false and we can early out.
	// Be sure to check the appropriate context for the event. This is only
	// worth checking if there is some cost beyond the EventWrite that we can
	// avoid -- the redirectors in this file guarantee that EventWrite is always
	// safe to call.
	// In this case we also avoid the potentially unreliable TLS implementation
	// (for dynamically loaded DLLs) on Windows XP.
	if ( !VALVE_MAIN_Context.IsEnabled )
	{
		return 0;
	}

	int64 nTime = GetQPCTime();
	EventWriteStart( pMessage, s_nDepth++ );
	return nTime;
}

int64 ETWEnd( const char *pMessage, int64 nStartTime )
{
	// If we are running on Windows XP or if our providers have not been enabled
	// (by xperf or other) then this will be false and we can early out.
	// Be sure to check the appropriate context for the event. This is only
	// worth checking if there is some cost beyond the EventWrite that we can
	// avoid -- the redirectors in this file guarantee that EventWrite is always
	// safe to call.
	// In this case we also avoid the potentially unreliable TLS implementation
	// (for dynamically loaded DLLs) on Windows XP.
	if ( !VALVE_MAIN_Context.IsEnabled )
	{
		return 0;
	}

	int64 nTime = GetQPCTime();
	EventWriteStop( pMessage, --s_nDepth, QPCToMS( nTime - nStartTime ) );
	return nTime;
}

static int s_nRenderFrameCount;

int ETWGetRenderFrameNumber()
{
	return s_nRenderFrameCount;
}

// Insert a render frame marker using the Valve-FrameRate provider. Automatically
// count the frame number and frame time. Since the frame count and elapsed time
// are tracked without paying attention to the bIsServerProcess flag the results
// will be 'unexpected' if bIsServerProcess changes value within a process.
void ETWRenderFrameMark( bool bIsServerProcess )
{
	static int64 s_lastFrameTime;

	int64 nCurrentFrameTime = GetQPCTime();
	float flElapsedFrameTime = 0.0f;
	if ( s_nRenderFrameCount )
	{
		flElapsedFrameTime = QPCToMS( nCurrentFrameTime - s_lastFrameTime );
	}

	if ( bIsServerProcess )
	{
		EventWriteServerRenderFrameMark( s_nRenderFrameCount, flElapsedFrameTime );
	}
	else
	{
		EventWriteRenderFrameMark( s_nRenderFrameCount, flElapsedFrameTime );
	}

	++s_nRenderFrameCount;
	s_lastFrameTime = nCurrentFrameTime;
}

// Insert a simulation frame marker using the Valve-FrameRate provider. Automatically
// count the frame number and frame time. Since the frame count and elapsed time
// are tracked without paying attention to the bIsServerProcess flag the results
// will be 'unexpected' if bIsServerProcess changes value within a process.
void ETWSimFrameMark( bool bIsServerProcess )
{
	static int s_nFrameCount;
	static int64 s_lastFrameTime;

	int64 nCurrentFrameTime = GetQPCTime();
	float flElapsedFrameTime = 0.0f;
	if ( s_nFrameCount )
	{
		flElapsedFrameTime = QPCToMS( nCurrentFrameTime - s_lastFrameTime );
	}

	if ( bIsServerProcess )
	{
		EventWriteServerSimFrameMark( s_nFrameCount, flElapsedFrameTime );
	}
	else
	{
		EventWriteSimFrameMark( s_nFrameCount, flElapsedFrameTime );
	}

	++s_nFrameCount;
	s_lastFrameTime = nCurrentFrameTime;
}

void ETWMouseDown( int whichButton, int x, int y )
{
	EventWriteMouse_down( whichButton, x, y );
}

void ETWMouseUp( int whichButton, int x, int y )
{
	EventWriteMouse_up( whichButton, x, y );
}

void ETWKeyDown( int nScanCode, int nVirtualCode, const char *pChar )
{
	EventWriteKey_down( pChar, nScanCode, nVirtualCode );
}

void ETWSendPacket( const char *pTo, int nWireSize, int nOutSequenceNR, int nOutSequenceNrAck )
{
	static int s_nCumulativeWireSize;
	s_nCumulativeWireSize += nWireSize;

	EventWriteSendPacket( pTo, nWireSize, nOutSequenceNR, nOutSequenceNrAck, s_nCumulativeWireSize );
}

void ETWThrottled()
{
	EventWriteThrottled();
}

void ETWReadPacket( const char *pFrom, int nWireSize, int nInSequenceNR, int nOutSequenceNRAck )
{
	static int s_nCumulativeWireSize;
	s_nCumulativeWireSize += nWireSize;

	EventWriteReadPacket( pFrom, nWireSize, nInSequenceNR, nOutSequenceNRAck, s_nCumulativeWireSize );
}

#endif // ETW_MARKS_ENABLED

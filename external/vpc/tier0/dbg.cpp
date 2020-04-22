//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "tier0/platform.h"

#if defined( PLATFORM_WINDOWS_PC )
#define WIN_32_LEAN_AND_MEAN
#include <windows.h>				// Currently needed for IsBadReadPtr and IsBadWritePtr
#pragma comment(lib,"user32.lib")	// For MessageBox
#endif

#include "tier0/minidump.h"
#include "tier0/stacktools.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "color.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "tier0/icommandline.h"
#include <math.h>

#if defined( _X360 )
#include "xbox/xbox_console.h"
#endif

#include "tier0/etwprof.h"

#ifndef STEAM
#define PvRealloc realloc
#define PvAlloc malloc
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )
#pragma optimize( "g", off ) //variable argument functions seem to screw up stack walking unless this optimization is disabled
#pragma warning( disable: 4748 )  // Turn off the warning telling us that optimizations are off if /GS is on
#endif

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_LOADING, "LOADING" );

//-----------------------------------------------------------------------------
// Stack attachment management
//-----------------------------------------------------------------------------
#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )

static bool s_bCallStacksWithAllWarnings = false; //if true, attach a call stack to every SPEW_WARNING message. Warning()/DevWarning()/...
static int s_iWarningMaxCallStackLength = 5;
#define AutomaticWarningCallStackLength() (s_bCallStacksWithAllWarnings ? s_iWarningMaxCallStackLength : 0)

void _Warning_AlwaysSpewCallStack_Enable( bool bEnable )
{
	s_bCallStacksWithAllWarnings = bEnable;
}

void _Warning_AlwaysSpewCallStack_Length( int iMaxCallStackLength )
{
	s_iWarningMaxCallStackLength = iMaxCallStackLength;
}

static bool s_bCallStacksWithAllErrors = false; //if true, attach a call stack to every SPEW_ERROR message. Mostly just Error()
static int s_iErrorMaxCallStackLength = 20; //default to higher output with an error since we're quitting anyways
#define AutomaticErrorCallStackLength() (s_bCallStacksWithAllErrors ? s_iErrorMaxCallStackLength : 0)

void _Error_AlwaysSpewCallStack_Enable( bool bEnable )
{
	s_bCallStacksWithAllErrors = bEnable;
}

void _Error_AlwaysSpewCallStack_Length( int iMaxCallStackLength )
{
	s_iErrorMaxCallStackLength = iMaxCallStackLength;
}

#else //#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )

#define AutomaticWarningCallStackLength() 0
#define AutomaticErrorCallStackLength() 0

void _Warning_AlwaysSpewCallStack_Enable( bool bEnable )
{
}

void _Warning_AlwaysSpewCallStack_Length( int iMaxCallStackLength )
{
}

void _Error_AlwaysSpewCallStack_Enable( bool bEnable )
{
}

void _Error_AlwaysSpewCallStack_Length( int iMaxCallStackLength )
{
}

#endif //#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )

void _ExitOnFatalAssert( const tchar* pFile, int line )
{
	Log_Msg( LOG_ASSERT, _T("Fatal assert failed: %s, line %d.  Application exiting.\n"), pFile, line );

	// only write out minidumps if we're not in the debugger
	if ( !Plat_IsInDebugSession() )
	{
		WriteMiniDump();
	}

	Log_Msg( LOG_DEVELOPER, _T("_ExitOnFatalAssert\n") );
	Plat_ExitProcess( EXIT_FAILURE );
}


//-----------------------------------------------------------------------------
// Templates to assist in validating pointers:
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE void _AssertValidReadPtr( void* ptr, int count/* = 1*/ )
{
#if defined( _WIN32 ) && !defined( _X360 )
	Assert( !IsBadReadPtr( ptr, count ) );
#else
	Assert( !count || ptr );
#endif
}

PLATFORM_INTERFACE void _AssertValidWritePtr( void* ptr, int count/* = 1*/ )
{
#if defined( _WIN32 ) && !defined( _X360 )
	Assert( !IsBadWritePtr( ptr, count ) );
#else
	Assert( !count || ptr );
#endif
}

PLATFORM_INTERFACE void _AssertValidReadWritePtr( void* ptr, int count/* = 1*/ )
{
#if defined( _WIN32 ) && !defined( _X360 )
	Assert(!( IsBadWritePtr(ptr, count) || IsBadReadPtr(ptr,count)));
#else
	Assert( !count || ptr );
#endif
}

PLATFORM_INTERFACE void _AssertValidStringPtr( const tchar* ptr, int maxchar/* = 0xFFFFFF */ )
{
#if defined( _WIN32 ) && !defined( _X360 )
	#ifdef TCHAR_IS_CHAR
		Assert( !IsBadStringPtr( ptr, maxchar ) );
	#else
		Assert( !IsBadStringPtrW( ptr, maxchar ) );
	#endif
#else
	Assert( ptr );
#endif
}

void AppendCallStackToLogMessage( tchar *formattedMessage, int iMessageLength, int iAppendCallStackLength )
{
#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )
#	if defined( TCHAR_IS_CHAR ) //I'm horrible with unicode and I don't plan on testing this with wide characters just yet
		if( iAppendCallStackLength > 0 )
		{
			int iExistingMessageLength = (int)strlen( formattedMessage ); //no V_strlen in tier 0, plus we're only compiling this for windows and 360. Seems safe
			formattedMessage += iExistingMessageLength;
			iMessageLength -= iExistingMessageLength;

			if( iMessageLength <= 32 )
				return; //no room for anything useful

			//append directly to the spew message
			if( (iExistingMessageLength > 0) && (formattedMessage[-1] == '\n') )
			{
				--formattedMessage;
				++iMessageLength;
			}

			//append preface
			int iAppendedLength = _snprintf( formattedMessage, iMessageLength, _T("\nCall Stack:\n\t") );
							
			void **CallStackBuffer = (void **)stackalloc( iAppendCallStackLength * sizeof( void * ) );
			int iCount = GetCallStack( CallStackBuffer, iAppendCallStackLength, 2 );
			if( TranslateStackInfo( CallStackBuffer, iCount, formattedMessage + iAppendedLength, iMessageLength - iAppendedLength, _T("\n\t") ) == 0 )
			{
				//failure
				formattedMessage[0] = '\0'; //this is pointing at where we wrote "\nCall Stack:\n\t"
			}
			else
			{
				iAppendedLength += (int)strlen( formattedMessage + iAppendedLength ); //no V_strlen in tier 0, plus we're only compiling this for windows and 360. Seems safe

				if( iAppendedLength < iMessageLength )
				{
					formattedMessage[iAppendedLength] = '\n'; //Add another newline.
					++iAppendedLength;

					formattedMessage[iAppendedLength] = '\0';
				}
			}
		}
#	else
		AssertMsg( false, "Fixme" );
#	endif
#endif
}

// Forward declare for internal use only.
CLoggingSystem *GetGlobalLoggingSystem();

#define Log_LegacyHelperColor_Stack( Channel, Severity, Color, MessageFormat, AppendCallStackLength ) \
	do \
{ \
	CLoggingSystem *pLoggingSystem = GetGlobalLoggingSystem(); \
	if ( pLoggingSystem->IsChannelEnabled( Channel, Severity ) ) \
{ \
	tchar formattedMessage[MAX_LOGGING_MESSAGE_LENGTH]; \
	va_list args; \
	va_start( args, MessageFormat ); \
	Tier0Internal_vsntprintf( formattedMessage, MAX_LOGGING_MESSAGE_LENGTH, MessageFormat, args ); \
	va_end( args ); \
	AppendCallStackToLogMessage( formattedMessage, MAX_LOGGING_MESSAGE_LENGTH, AppendCallStackLength ); \
	pLoggingSystem->LogDirect( Channel, Severity, Color, formattedMessage ); \
} \
} while( 0 )

#define Log_LegacyHelperColor( Channel, Severity, Color, MessageFormat ) Log_LegacyHelperColor_Stack( Channel, Severity, Color, MessageFormat, 0 )

#define Log_LegacyHelper_Stack( Channel, Severity, MessageFormat, AppendCallStackLength ) Log_LegacyHelperColor_Stack( Channel, Severity, pLoggingSystem->GetChannelColor( Channel ), MessageFormat, AppendCallStackLength )
#define Log_LegacyHelper( Channel, Severity, MessageFormat ) Log_LegacyHelperColor( Channel, Severity, pLoggingSystem->GetChannelColor( Channel ), MessageFormat )


void Msg( const tchar* pMsgFormat, ... )
{
	Log_LegacyHelper( LOG_GENERAL, LS_MESSAGE, pMsgFormat );
}

void Warning( const tchar *pMsgFormat, ... )
{
	Log_LegacyHelper_Stack( LOG_GENERAL, LS_WARNING, pMsgFormat, AutomaticWarningCallStackLength() );
}

void Warning_SpewCallStack( int iMaxCallStackLength, const tchar *pMsgFormat, ... )
{
	Log_LegacyHelper_Stack( LOG_GENERAL, LS_WARNING, pMsgFormat, iMaxCallStackLength );
}


void Error( const tchar *pMsgFormat, ... )
{
	Log_LegacyHelper_Stack( LOG_GENERAL, LS_ERROR, pMsgFormat, AutomaticErrorCallStackLength() );
}

void Error_SpewCallStack( int iMaxCallStackLength, const tchar *pMsgFormat, ... )
{
	Log_LegacyHelper_Stack( LOG_GENERAL, LS_ERROR, pMsgFormat, iMaxCallStackLength );
}


//-----------------------------------------------------------------------------
// A couple of super-common dynamic spew messages, here for convenience 
// These looked at the "developer" group, print if it's level 1 or higher 
//-----------------------------------------------------------------------------
void DevMsg( int level, const tchar* pMsgFormat, ... )
{
	LoggingChannelID_t channel = level >= 2 ? LOG_DEVELOPER_VERBOSE : LOG_DEVELOPER;
	Log_LegacyHelper( channel, LS_MESSAGE, pMsgFormat );
}


void DevWarning( int level, const tchar *pMsgFormat, ... )
{
	LoggingChannelID_t channel = level >= 2 ? LOG_DEVELOPER_VERBOSE : LOG_DEVELOPER;
	Log_LegacyHelper( channel, LS_WARNING, pMsgFormat );
}

void DevMsg( const tchar *pMsgFormat, ... )
{
	Log_LegacyHelper( LOG_DEVELOPER, LS_MESSAGE, pMsgFormat );
}

void DevWarning( const tchar *pMsgFormat, ... )
{
	Log_LegacyHelper( LOG_DEVELOPER, LS_WARNING, pMsgFormat );
}

void ConColorMsg( const Color& clr, const tchar* pMsgFormat, ... )
{
	Log_LegacyHelperColor( LOG_CONSOLE, LS_MESSAGE, clr, pMsgFormat );
}

void ConMsg( const tchar *pMsgFormat, ... )
{
	Log_LegacyHelper( LOG_CONSOLE, LS_MESSAGE, pMsgFormat );
}

void ConDMsg( const tchar *pMsgFormat, ... )
{
	Log_LegacyHelper( LOG_DEVELOPER_CONSOLE, LS_MESSAGE, pMsgFormat );
}

// If we don't have a function from math.h, then it doesn't link certain floating-point
// functions in and printfs with %f cause runtime errors in the C libraries.
PLATFORM_INTERFACE float CrackSmokingCompiler( float a )
{
	return (float)fabs( a );
}

void* Plat_SimpleLog( const tchar* file, int line )
{
	FILE* f = _tfopen( _T("simple.log"), _T("at+") );
	_ftprintf( f, _T("%s:%i\n"), file, line );
	fclose( f );

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: For debugging startup times, etc.
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void COM_TimestampedLog( char const *fmt, ... )
{
	static float s_LastStamp = 0.0;
	static bool s_bShouldLog = false;
	static bool s_bShouldLogToConsole = false;
	static bool s_bShouldLogToETW = false;
	static bool s_bChecked = false;
	static bool	s_bFirstWrite = false;

	if ( !s_bChecked )
	{
		s_bShouldLog = ( CommandLine()->CheckParm( "-profile" ) ) ? true : false;
		s_bShouldLogToConsole = ( CommandLine()->ParmValue( "-profile", 0.0f ) != 0.0f ) ? true : false;

		s_bShouldLogToETW = ( CommandLine()->CheckParm( "-etwprofile" ) ) ? true : false;
		if ( s_bShouldLogToETW )
		{
			s_bShouldLog = true;
		}

		s_bChecked = true;
	}
	if ( !s_bShouldLog )
	{
		return;
	}

	char string[1024];
	va_list argptr;
	va_start( argptr, fmt );
	Tier0Internal_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	float curStamp = Plat_FloatTime();

#if defined( _X360 )
	XBX_rTimeStampLog( curStamp, string );
#elif defined( _PS3 )
	Log_Warning( LOG_LOADING, "%8.4f / %8.4f:  %s\n", curStamp, curStamp - s_LastStamp, string );
#endif

	if ( IsPC() )
	{
		// If ETW profiling is enabled then do it only.
		if ( s_bShouldLogToETW )
		{
			ETWMark( string );
		}
		else
		{
			if ( !s_bFirstWrite )
			{
				unlink( "timestamped.log" );
				s_bFirstWrite = true;
			}

			FILE* fp = fopen( "timestamped.log", "at+" );
			fprintf( fp, "%8.4f / %8.4f:  %s\n", curStamp, curStamp - s_LastStamp, string );
			fclose( fp );
		}

		if ( s_bShouldLogToConsole )
		{
			Msg( "%8.4f / %8.4f:  %s\n", curStamp, curStamp - s_LastStamp, string );
		}
	}

	s_LastStamp = curStamp;
}

#ifdef IS_WINDOWS_PC

class CHardwareBreakPoint
{
public:

	enum EOpCode
	{
		BRK_SET = 0,
		BRK_UNSET,
	};

	CHardwareBreakPoint()
	{
		m_eOperation = BRK_SET;
		m_pvAddress = 0;
		m_hThread = 0;
		m_hThreadEvent = 0;
		m_nRegister = 0;
		m_bSuccess = false;
	}

	const void				*m_pvAddress;
	HANDLE					m_hThread;
	EHardwareBreakpointType m_eType;
	EHardwareBreakpointSize m_eSize;
	HANDLE					m_hThreadEvent;
	int						m_nRegister;
	EOpCode					m_eOperation;
	bool					m_bSuccess;

	static void SetBits( DWORD_PTR& dw, int lowBit, int bits, int newValue );
	static DWORD WINAPI ThreadProc( LPVOID lpParameter );
};

void CHardwareBreakPoint::SetBits( DWORD_PTR& dw, int lowBit, int bits, int newValue )
{
	DWORD_PTR mask = (1 << bits) - 1; 
	dw = (dw & ~(mask << lowBit)) | (newValue << lowBit);
}

DWORD WINAPI CHardwareBreakPoint::ThreadProc( LPVOID lpParameter )
{
	CHardwareBreakPoint *h = reinterpret_cast< CHardwareBreakPoint * >( lpParameter );
	SuspendThread( h->m_hThread );

	// Get current context
	CONTEXT ct = {0};
	ct.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	GetThreadContext(h->m_hThread,&ct);

	int FlagBit = 0;

	bool Dr0Busy = false;
	bool Dr1Busy = false;
	bool Dr2Busy = false;
	bool Dr3Busy = false;
	if (ct.Dr7 & 1)
		Dr0Busy = true;
	if (ct.Dr7 & 4)
		Dr1Busy = true;
	if (ct.Dr7 & 16)
		Dr2Busy = true;
	if (ct.Dr7 & 64)
		Dr3Busy = true;

	if ( h->m_eOperation == CHardwareBreakPoint::BRK_UNSET )
	{
		// Remove
		if (h->m_nRegister == 0)
		{
			FlagBit = 0;
			ct.Dr0 = 0;
			Dr0Busy = false;
		}
		if (h->m_nRegister == 1)
		{
			FlagBit = 2;
			ct.Dr1 = 0;
			Dr1Busy = false;
		}
		if (h->m_nRegister == 2)
		{
			FlagBit = 4;
			ct.Dr2 = 0;
			Dr2Busy = false;
		}
		if (h->m_nRegister == 3)
		{
			FlagBit = 6;
			ct.Dr3 = 0;
			Dr3Busy = false;
		}
		ct.Dr7 &= ~(1 << FlagBit);
	}
	else
	{
		if (!Dr0Busy)
		{
			h->m_nRegister = 0;
			ct.Dr0 = (DWORD_PTR)h->m_pvAddress;
			Dr0Busy = true;
		}
		else if (!Dr1Busy)
		{
			h->m_nRegister = 1;
			ct.Dr1 = (DWORD_PTR)h->m_pvAddress;
			Dr1Busy = true;
		}
		else if (!Dr2Busy)
		{
			h->m_nRegister = 2;
			ct.Dr2 = (DWORD_PTR)h->m_pvAddress;
			Dr2Busy = true;
		}
		else if (!Dr3Busy)
		{
			h->m_nRegister = 3;
			ct.Dr3 = (DWORD_PTR)h->m_pvAddress;
			Dr3Busy = true;
		}
		else
		{
			h->m_bSuccess = false;
			ResumeThread(h->m_hThread);
			SetEvent(h->m_hThreadEvent);
			return 0;
		}

		ct.Dr6 = 0;
		int st = 0;
		if (h->m_eType == BREAKPOINT_EXECUTE)
			st = 0;
		if (h->m_eType == BREAKPOINT_READWRITE)
			st = 3;
		if (h->m_eType == BREAKPOINT_WRITE)
			st = 1;

		int le = 0;
		if (h->m_eSize == BREAKPOINT_SIZE_1)
			le = 0;
		if (h->m_eSize == BREAKPOINT_SIZE_2)
			le = 1;
		if (h->m_eSize == BREAKPOINT_SIZE_4)
			le = 3;
		if (h->m_eSize == BREAKPOINT_SIZE_8)
			le = 2;

		SetBits( ct.Dr7, 16 + h->m_nRegister*4, 2, st );
		SetBits( ct.Dr7, 18 + h->m_nRegister*4, 2, le );
		SetBits( ct.Dr7, h->m_nRegister*2,1,1);
	}

	ct.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	SetThreadContext(h->m_hThread,&ct);

	ResumeThread( h->m_hThread );
	h->m_bSuccess = true;
	SetEvent( h->m_hThreadEvent );
	return 0;
}

HardwareBreakpointHandle_t SetHardwareBreakpoint( EHardwareBreakpointType eType, EHardwareBreakpointSize eSize, const void *pvLocation )
{
	CHardwareBreakPoint *h = new CHardwareBreakPoint();
	h->m_pvAddress = pvLocation;
	h->m_eSize = eSize;
	h->m_eType = eType;
	HANDLE hThread = GetCurrentThread();
	h->m_hThread = hThread;

	if ( hThread == GetCurrentThread() )
	{
		DWORD nThreadId = GetCurrentThreadId();
		h->m_hThread = OpenThread( THREAD_ALL_ACCESS, 0, nThreadId );
	}

	h->m_hThreadEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	h->m_eOperation = CHardwareBreakPoint::BRK_SET; // Set Break
	CreateThread( 0, 0, CHardwareBreakPoint::ThreadProc, (LPVOID)h, 0, 0 );
	WaitForSingleObject( h->m_hThreadEvent,INFINITE );
	CloseHandle( h->m_hThreadEvent );
	h->m_hThreadEvent = 0;
	if ( hThread == GetCurrentThread() )
	{
		CloseHandle( h->m_hThread );
	}
	h->m_hThread = hThread;
	if ( !h->m_bSuccess )
	{
		delete h;
		return (HardwareBreakpointHandle_t)0;
	}
	return (HardwareBreakpointHandle_t)h;
}

bool ClearHardwareBreakpoint( HardwareBreakpointHandle_t handle )
{
	CHardwareBreakPoint *h = reinterpret_cast< CHardwareBreakPoint* >( handle );
	if ( !h )
	{
		return false;
	}

	bool bOpened = false;
	if ( h->m_hThread == GetCurrentThread() )
	{
		DWORD nThreadId = GetCurrentThreadId();
		h->m_hThread = OpenThread( THREAD_ALL_ACCESS, 0, nThreadId );
		bOpened = true;
	}

	h->m_hThreadEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	h->m_eOperation = CHardwareBreakPoint::BRK_UNSET; // Remove Break
	CreateThread( 0,0,CHardwareBreakPoint::ThreadProc, (LPVOID)h, 0,0 );
	WaitForSingleObject( h->m_hThreadEvent, INFINITE );
	CloseHandle( h->m_hThreadEvent );
	h->m_hThreadEvent = 0;
	if ( bOpened )
	{
		CloseHandle( h->m_hThread );
	}
	delete h;
	return true;
}

#endif // IS_WINDOWS_PC


//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "pch_tier0.h"

#include "tier0/minidump.h"
#include "tier0/platform.h"

#if defined( _WIN32 ) && !defined( _X360 )

#if _MSC_VER >= 1300
#include "tier0/valve_off.h"
#define WIN_32_LEAN_AND_MEAN
#include <windows.h>

#include <dbghelp.h>

#include <time.h>

// MiniDumpWriteDump() function declaration (so we can just get the function directly from windows)
typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)
	(
	HANDLE hProcess, 
	DWORD dwPid, 
	HANDLE hFile, 
	MINIDUMP_TYPE DumpType,
	CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
	CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
	CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
	);


// counter used to make sure minidump names are unique
static int g_nMinidumpsWritten = 0;

// process-wide prefix to use for minidumps
static tchar g_rgchMinidumpFilenamePrefix[MAX_PATH];

// Process-wide comment to put into minidumps
static char g_rgchMinidumpComment[2048];

//-----------------------------------------------------------------------------
// Purpose: Creates a new file and dumps the exception info into it
// Input  : uStructuredExceptionCode	- windows exception code, unused.
//			pExceptionInfo				- call stack.
//			minidumpType				- type of minidump to write.
//			ptchMinidumpFileNameBuffer	- if not-NULL points to a writable tchar buffer
//										  of length at least _MAX_PATH to contain the name
//										  of the written minidump file on return.
//-----------------------------------------------------------------------------
bool WriteMiniDumpUsingExceptionInfo( 
	unsigned int uStructuredExceptionCode, 
	_EXCEPTION_POINTERS * pExceptionInfo, 
	int minidumpType,
	const char *pszFilenameSuffix,
	tchar *ptchMinidumpFileNameBuffer /* = NULL */
	)
{
	if ( ptchMinidumpFileNameBuffer )
	{
		*ptchMinidumpFileNameBuffer = tchar( 0 );
	}

	// get the function pointer directly so that we don't have to include the .lib, and that
	// we can easily change it to using our own dll when this code is used on win98/ME/2K machines
	HMODULE hDbgHelpDll = ::LoadLibrary( "DbgHelp.dll" );
	if ( !hDbgHelpDll )
		return false;

	bool bReturnValue = false;
	MINIDUMPWRITEDUMP pfnMiniDumpWrite = (MINIDUMPWRITEDUMP) ::GetProcAddress( hDbgHelpDll, "MiniDumpWriteDump" );

	if ( pfnMiniDumpWrite )
	{
		// create a unique filename for the minidump based on the current time and module name
		time_t currTime = ::time( NULL );
		struct tm * pTime = ::localtime( &currTime );
		++g_nMinidumpsWritten;

		// If they didn't set a dump prefix, then set one for them using the module name
		if ( g_rgchMinidumpFilenamePrefix[0] == TCHAR(0) )
		{
			tchar rgchModuleName[MAX_PATH];
			#ifdef TCHAR_IS_WCHAR
				::GetModuleFileNameW( NULL, rgchModuleName, sizeof(rgchModuleName) / sizeof(tchar) );
			#else
				::GetModuleFileName( NULL, rgchModuleName, sizeof(rgchModuleName) / sizeof(tchar) );
			#endif

			// strip off the rest of the path from the .exe name
			tchar *pch = _tcsrchr( rgchModuleName, '.' );
			if ( pch )
			{
				*pch = 0;
			}
			pch = _tcsrchr( rgchModuleName, '\\' );
			if ( pch )
			{
				// move past the last slash
				pch++;
			}
			else
			{
				pch = _T("unknown");
			}
			strcpy( g_rgchMinidumpFilenamePrefix, pch );
		}

		
		// can't use the normal string functions since we're in tier0
		tchar rgchFileName[MAX_PATH];
		_sntprintf( rgchFileName, sizeof(rgchFileName) / sizeof(tchar),
			_T("%s_%d%02d%02d_%02d%02d%02d_%d%hs%hs.mdmp"),
			g_rgchMinidumpFilenamePrefix,
			pTime->tm_year + 1900,	/* Year less 2000 */
			pTime->tm_mon + 1,		/* month (0 - 11 : 0 = January) */
			pTime->tm_mday,			/* day of month (1 - 31) */
			pTime->tm_hour,			/* hour (0 - 23) */
			pTime->tm_min,		    /* minutes (0 - 59) */
			pTime->tm_sec,		    /* seconds (0 - 59) */
			g_nMinidumpsWritten,	// ensures the filename is unique
			( pszFilenameSuffix != NULL ) ? "_" : "",
			( pszFilenameSuffix != NULL ) ? pszFilenameSuffix : ""
			);
		// Ensure null-termination.
		rgchFileName[ Q_ARRAYSIZE(rgchFileName) - 1 ] = 0;

		// Create directory, if our dump filename had a directory in it
		for ( char *pSlash = rgchFileName ; *pSlash != '\0' ; ++pSlash )
		{
			char c = *pSlash;
			if ( c == '/' || c == '\\' )
			{
				*pSlash = '\0';
				::CreateDirectory( rgchFileName, NULL );
				*pSlash = c;
			}
		}

		BOOL bMinidumpResult = FALSE;
#ifdef TCHAR_IS_WCHAR
		HANDLE hFile = ::CreateFileW( rgchFileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
#else
		HANDLE hFile = ::CreateFile( rgchFileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
#endif

		if ( hFile )
		{
			// dump the exception information into the file
			_MINIDUMP_EXCEPTION_INFORMATION	ExInfo;
			ExInfo.ThreadId	= ::GetCurrentThreadId();
			ExInfo.ExceptionPointers = pExceptionInfo;
			ExInfo.ClientPointers = FALSE;

			// Do we have a comment?
			MINIDUMP_USER_STREAM_INFORMATION StreamInformationHeader;
			MINIDUMP_USER_STREAM UserStreams[1];
			memset( &StreamInformationHeader, 0, sizeof(StreamInformationHeader) );
			StreamInformationHeader.UserStreamArray = UserStreams;

			if ( g_rgchMinidumpComment[0] != '\0' )
			{
				MINIDUMP_USER_STREAM *pCommentStream = &UserStreams[StreamInformationHeader.UserStreamCount++];
				pCommentStream->Type = CommentStreamA;
				pCommentStream->Buffer = g_rgchMinidumpComment;
				pCommentStream->BufferSize = (ULONG)strlen(g_rgchMinidumpComment)+1;
			}

			bMinidumpResult = (*pfnMiniDumpWrite)( ::GetCurrentProcess(), ::GetCurrentProcessId(), hFile, (MINIDUMP_TYPE)minidumpType, &ExInfo, &StreamInformationHeader, NULL );
			::CloseHandle( hFile );

			// Clear comment for next time
			g_rgchMinidumpComment[0] = '\0';

			if ( bMinidumpResult )
			{
				bReturnValue = true;

				if ( ptchMinidumpFileNameBuffer )
				{
					// Copy the file name from "pSrc = rgchFileName" into "pTgt = ptchMinidumpFileNameBuffer"
					tchar *pTgt = ptchMinidumpFileNameBuffer;
					tchar const *pSrc = rgchFileName;
					while ( ( *( pTgt ++ ) = *( pSrc ++ ) ) != tchar( 0 ) )
						continue;
				}
			}

			// fall through to trying again
		}
		
		// mark any failed minidump writes by renaming them
		if ( !bMinidumpResult )
		{
			tchar rgchFailedFileName[_MAX_PATH];
			_sntprintf( rgchFailedFileName, sizeof(rgchFailedFileName) / sizeof(tchar), "(failed)%s", rgchFileName );
			// Ensure null-termination.
			rgchFailedFileName[ Q_ARRAYSIZE(rgchFailedFileName) - 1 ] = 0;
			rename( rgchFileName, rgchFailedFileName );
		}
	}

	::FreeLibrary( hDbgHelpDll );

	// call the log flush function if one is registered to try to flush any logs
	//CallFlushLogFunc();

	return bReturnValue;
}


void InternalWriteMiniDumpUsingExceptionInfo( unsigned int uStructuredExceptionCode, _EXCEPTION_POINTERS * pExceptionInfo, const char *pszFilenameSuffix )
{
	// If this is is a real crash (not an assert or one we purposefully triggered), then try to write a full dump
	// only do this on our GC (currently GC is 64-bit, so we can use a #define rather than some run-time switch
#ifdef _WIN64
	if ( uStructuredExceptionCode != EXCEPTION_BREAKPOINT )
	{
		if ( WriteMiniDumpUsingExceptionInfo( uStructuredExceptionCode, pExceptionInfo, MiniDumpWithFullMemory, pszFilenameSuffix ) )
		{
			return;
		}
	}
#endif

	// First try to write it with all the indirectly referenced memory (ie: a large file).
	// If that doesn't work, then write a smaller one.
	int iType = MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory;
	if ( !WriteMiniDumpUsingExceptionInfo( uStructuredExceptionCode, pExceptionInfo, (MINIDUMP_TYPE)iType, pszFilenameSuffix ) )
	{
		iType = MiniDumpWithDataSegs;
		WriteMiniDumpUsingExceptionInfo( uStructuredExceptionCode, pExceptionInfo, (MINIDUMP_TYPE)iType, pszFilenameSuffix );
	}
}

// minidump function to use
static FnMiniDump g_pfnWriteMiniDump = InternalWriteMiniDumpUsingExceptionInfo;

//-----------------------------------------------------------------------------
// Purpose: Set a function to call which will write our minidump, overriding
//			the default function
// Input  : pfn -		Pointer to minidump function to set
// Output :				Previously set function
//-----------------------------------------------------------------------------
FnMiniDump SetMiniDumpFunction( FnMiniDump pfn )
{
	FnMiniDump pfnTemp = g_pfnWriteMiniDump;
	g_pfnWriteMiniDump = pfn;
	return pfnTemp;
}


//-----------------------------------------------------------------------------
// Unhandled exceptions
//-----------------------------------------------------------------------------
static FnMiniDump g_UnhandledExceptionFunction;
static LONG STDCALL ValveUnhandledExceptionFilter( _EXCEPTION_POINTERS* pExceptionInfo )
{
	uint uStructuredExceptionCode = pExceptionInfo->ExceptionRecord->ExceptionCode;
	g_UnhandledExceptionFunction( uStructuredExceptionCode, pExceptionInfo, 0 );
	return EXCEPTION_CONTINUE_SEARCH;
}

void MinidumpSetUnhandledExceptionFunction( FnMiniDump pfn )
{
	g_UnhandledExceptionFunction = pfn;
	SetUnhandledExceptionFilter( ValveUnhandledExceptionFilter );
}


//-----------------------------------------------------------------------------
// Purpose: set prefix to use for filenames
//-----------------------------------------------------------------------------
void SetMinidumpFilenamePrefix( const char *pszPrefix )
{
	#ifdef TCHAR_IS_WCHAR
		mbstowcs( g_rgchMinidumpFilenamePrefix, pszPrefix, sizeof(g_rgchMinidumpFilenamePrefix) / sizeof(g_rgchMinidumpFilenamePrefix[0]) - 1 );
	#else
		strncpy( g_rgchMinidumpFilenamePrefix, pszPrefix, sizeof(g_rgchMinidumpFilenamePrefix) / sizeof(g_rgchMinidumpFilenamePrefix[0]) - 1 );
	#endif
}

//-----------------------------------------------------------------------------
// Purpose: set comment to put into minidumps
//-----------------------------------------------------------------------------
void SetMinidumpComment( const char *pszComment )
{
	if ( pszComment == NULL )
		pszComment = "";
	strncpy( g_rgchMinidumpComment, pszComment, sizeof(g_rgchMinidumpComment) - 1 );
}

//-----------------------------------------------------------------------------
// Purpose: writes out a minidump from the current process
//-----------------------------------------------------------------------------
void WriteMiniDump( const char *pszFilenameSuffix )
{
	// throw an exception so we can catch it and get the stack info
	__try
	{
		::RaiseException
			(
			EXCEPTION_BREAKPOINT,		// dwExceptionCode
			EXCEPTION_NONCONTINUABLE,	// dwExceptionFlags
			0,							// nNumberOfArguments,
			NULL						// const ULONG_PTR* lpArguments
			);

		// Never get here (non-continuable exception)
	}
	// Write the minidump from inside the filter (GetExceptionInformation() is only 
	// valid in the filter)
	__except ( g_pfnWriteMiniDump( EXCEPTION_BREAKPOINT, GetExceptionInformation(), pszFilenameSuffix ), EXCEPTION_EXECUTE_HANDLER )
	{
	}
}

DBG_OVERLOAD bool g_bInException = false;

//-----------------------------------------------------------------------------
// Purpose: Catches and writes out any exception throw by the specified function.
// Input:	pfn -			Function to call within protective exception block
//			pv -			Void pointer to pass that function
//-----------------------------------------------------------------------------
void CatchAndWriteMiniDump( FnWMain pfn, int argc, tchar *argv[] )
{
	CatchAndWriteMiniDumpEx( pfn, argc, argv, k_ECatchAndWriteMiniDumpAbort );
}

// message types
enum ECatchAndWriteFunctionType
{
	k_eSCatchAndWriteFunctionTypeInvalid		= 0,
	k_eSCatchAndWriteFunctionTypeWMain			= 1,   // typedef void (*FnWMain)( int , tchar *[] );
	k_eSCatchAndWriteFunctionTypeWMainIntReg	= 2,   // typedef int (*FnWMainIntRet)( int , tchar *[] );
	k_eSCatchAndWriteFunctionTypeVoidPtr		= 3,   // typedef void (*FnVoidPtrFn)( void * );
};

struct CatchAndWriteContext_t
{
	ECatchAndWriteFunctionType  m_eType;
	void						*m_pfn;
	int							*m_pargc;
	tchar						***m_pargv;
	void						**m_ppv;
	ECatchAndWriteMinidumpAction m_eAction;

	void						Set( ECatchAndWriteFunctionType eType, ECatchAndWriteMinidumpAction eAction, void *pfn, int *pargc, tchar **pargv[], void **ppv )
	{
		m_eType = eType;
		m_eAction = eAction;
		m_pfn = pfn;
		m_pargc = pargc;
		m_pargv = pargv;
		m_ppv = ppv;

		ErrorIfNot( m_pfn, ( "CatchAndWriteContext_t::Set w/o a function pointer!" ) );
	}

	int							Invoke()
	{
		switch ( m_eType )
		{
		default:
		case k_eSCatchAndWriteFunctionTypeInvalid:
			break;
		case k_eSCatchAndWriteFunctionTypeWMain:
			ErrorIfNot( m_pargc && m_pargv, ( "CatchAndWriteContext_t::Invoke with bogus argc/argv" ) );
			((FnWMain)m_pfn)( *m_pargc, *m_pargv );
			break;
		case k_eSCatchAndWriteFunctionTypeWMainIntReg:
			ErrorIfNot( m_pargc && m_pargv, ( "CatchAndWriteContext_t::Invoke with bogus argc/argv" ) );
			return ((FnWMainIntRet)m_pfn)( *m_pargc, *m_pargv );
		case k_eSCatchAndWriteFunctionTypeVoidPtr:
			ErrorIfNot( m_ppv, ( "CatchAndWriteContext_t::Invoke with bogus void *ptr" ) );
			((FnVoidPtrFn)m_pfn)( *m_ppv );
			break;
		}

		return 0;
	}
};

//-----------------------------------------------------------------------------
// Purpose: Catches and writes out any exception throw by the specified function
// Input:	pfn -			Function to call within protective exception block
//			pv -			Void pointer to pass that function
//			eAction -		Specifies what to do if it catches an exception
//-----------------------------------------------------------------------------
#if defined(_PS3)

int CatchAndWriteMiniDump_Impl( CatchAndWriteContext_t &ctx )
{
	// we dont handle minidumps on ps3
	return ctx.Invoke();
}

#else

static const char *GetExceptionCodeName( unsigned long code )
{
	switch ( code )
	{
		case EXCEPTION_ACCESS_VIOLATION: return "accessviolation";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "arrayboundsexceeded";
		case EXCEPTION_BREAKPOINT: return "breakpoint";
		case EXCEPTION_DATATYPE_MISALIGNMENT: return "datatypemisalignment";
		case EXCEPTION_FLT_DENORMAL_OPERAND: return "fltdenormaloperand";
		case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "fltdividebyzero";
		case EXCEPTION_FLT_INEXACT_RESULT: return "fltinexactresult";
		case EXCEPTION_FLT_INVALID_OPERATION: return "fltinvalidoperation";
		case EXCEPTION_FLT_OVERFLOW: return "fltoverflow";
		case EXCEPTION_FLT_STACK_CHECK: return "fltstackcheck";
		case EXCEPTION_FLT_UNDERFLOW: return "fltunderflow";
		case EXCEPTION_INT_DIVIDE_BY_ZERO: return "intdividebyzero";
		case EXCEPTION_INT_OVERFLOW: return "intoverflow";
		case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "noncontinuableexception";
		case EXCEPTION_PRIV_INSTRUCTION: return "privinstruction";
		case EXCEPTION_SINGLE_STEP: return "singlestep";
	}

	// Unknown exception
	return "crash";
}

int CatchAndWriteMiniDump_Impl( CatchAndWriteContext_t &ctx )
{
	// Sorry, this is the only action currently implemented!
	Assert( ctx.m_eAction == k_ECatchAndWriteMiniDumpAbort );

	if ( Plat_IsInDebugSession() )
	{
		// don't mask exceptions when running in the debugger
		return ctx.Invoke();
	}	
	
//	g_DumpHelper.Init();

// Win32 code gets to use a special handler
#if defined( _WIN32 )
	__try
	{
		return ctx.Invoke();
	}
	__except ( g_pfnWriteMiniDump( GetExceptionCode(), GetExceptionInformation(), GetExceptionCodeName( GetExceptionCode() ) ), EXCEPTION_EXECUTE_HANDLER )
	{
		TerminateProcess( GetCurrentProcess(), EXIT_FAILURE ); // die, die RIGHT NOW! (don't call exit() so destructors will not get run)
	}

	// if we get here, we definitely are not in an exception handler
	g_bInException = false;
	
	return 0;
#else
//	if ( ctx.m_pargv != 0 )
//	{
//		g_DumpHelper.ComputeExeNameFromArgv0( (*ctx.m_pargv)[ 0 ] );
//	}
//
//	ICrashHandler *handler = g_DumpHelper.GetHandlerAPI();
//	CCrashHandlerScope scope( handler, g_DumpHelper.GetProduct(), g_DumpHelper.GetVersion(), g_DumpHelper.GetBuildID(), false );
//	if ( handler )
//		handler->SetSteamID( g_DumpHelper.GetSteamID() );
	
	return ctx.Invoke();
#endif
}

#endif // _PS3

//-----------------------------------------------------------------------------
// Purpose: Catches and writes out any exception throw by the specified function
// Input:	pfn -			Function to call within protective exception block
//			pv -			Void pointer to pass that function
//			eAction -		Specifies what to do if it catches an exception
//-----------------------------------------------------------------------------
void CatchAndWriteMiniDumpEx( FnWMain pfn, int argc, tchar *argv[], ECatchAndWriteMinidumpAction eAction )
{
	CatchAndWriteContext_t ctx;
	ctx.Set( k_eSCatchAndWriteFunctionTypeWMain, eAction, (void *)pfn, &argc, &argv, NULL );
	CatchAndWriteMiniDump_Impl( ctx );
}

//-----------------------------------------------------------------------------
// Purpose: Catches and writes out any exception throw by the specified function
// Input:	pfn -			Function to call within protective exception block
//			pv -			Void pointer to pass that function
//			eAction -		Specifies what to do if it catches an exception
//-----------------------------------------------------------------------------
int CatchAndWriteMiniDumpExReturnsInt( FnWMainIntRet pfn, int argc, tchar *argv[], ECatchAndWriteMinidumpAction eAction )
{
	CatchAndWriteContext_t ctx;
	ctx.Set( k_eSCatchAndWriteFunctionTypeWMainIntReg, eAction, (void *)pfn, &argc, &argv, NULL );
	return CatchAndWriteMiniDump_Impl( ctx );
}



//-----------------------------------------------------------------------------
// Purpose: Catches and writes out any exception throw by the specified function
// Input:	pfn -			Function to call within protective exception block
//			pv -			Void pointer to pass that function
//			eAction -		Specifies what to do if it catches an exception
//-----------------------------------------------------------------------------
void CatchAndWriteMiniDumpExForVoidPtrFn( FnVoidPtrFn pfn, void *pv, ECatchAndWriteMinidumpAction eAction )
{
	CatchAndWriteContext_t ctx;
	ctx.Set( k_eSCatchAndWriteFunctionTypeVoidPtr, eAction, (void *)pfn, NULL, NULL, &pv );
	CatchAndWriteMiniDump_Impl( ctx );
}

//-----------------------------------------------------------------------------
// Purpose: Catches and writes out any exception throw by the specified function
// Input:	pfn -			Function to call within protective exception block
//			pv -			Void pointer to pass that function
//			bExitQuietly -	If true (for client) just exit after mindump, with no visible error for user
//							If false, re-throws.
//-----------------------------------------------------------------------------
void CatchAndWriteMiniDumpForVoidPtrFn( FnVoidPtrFn pfn, void *pv, bool bExitQuietly )
{
	return CatchAndWriteMiniDumpExForVoidPtrFn( pfn, pv, bExitQuietly ? k_ECatchAndWriteMiniDumpAbort : k_ECatchAndWriteMiniDumpReThrow );
}


/*
Call this function to ensure that your program actually crashes when it crashes.

Oh my god.

When 64-bit Windows came out it turns out that it wasn't possible to throw
and catch exceptions from user-mode, through kernel-mode, and back to user
mode. Therefore, for crashes that happen in kernel callbacks such as Window
procs Microsoft had to decide either to always crash when an exception
is thrown (including an SEH such as an access violation) or else always silently
swallow the exception.

They chose badly.

Therefore, for the last five or so years, programs on 64-bit Windows have been
silently swallowing *some* exceptions. As a concrete example, consider this code:

	case WM_PAINT:
		{
			hdc = BeginPaint(hWnd, &ps);
			char* p = new char;
			*(int*)0 = 0;
			delete p;
			EndPaint(hWnd, &ps);
		}
		break;

It's in a WindowProc handling a paint message so it will generally be called from
kernel mode. Therefore the crash in the middle of it is, by default, 'handled' for
us. The "delete p;" and EndPaint() never happen. This means that the process is
left in an indeterminate state. It also means that our error reporting never sees
the exception. It is effectively as though there is a __try/__except handler at the
kernel boundary and any crashes cause the stack to be unwound (without destructors
being run) to the kernel boundary where execution continues.

Charming.

The fix is to use the Get/SetProcessUserModeExceptionPolicy API to tell Windows
that we don't want to struggle on after crashing.

For more scary details see this article. It actually suggests using the compatibility
manifest, but that does not appear to work.
http://blog.paulbetts.org/index.php/2010/07/20/the-case-of-the-disappearing-onload-exception-user-mode-callback-exceptions-in-x64/
*/
void EnableCrashingOnCrashes()
{
	typedef BOOL (WINAPI *tGetProcessUserModeExceptionPolicy)(LPDWORD lpFlags);
	typedef BOOL (WINAPI *tSetProcessUserModeExceptionPolicy)(DWORD dwFlags);
	#define PROCESS_CALLBACK_FILTER_ENABLED     0x1

	HMODULE kernel32 = LoadLibraryA("kernel32.dll");
	tGetProcessUserModeExceptionPolicy pGetProcessUserModeExceptionPolicy = (tGetProcessUserModeExceptionPolicy)GetProcAddress(kernel32, "GetProcessUserModeExceptionPolicy");
	tSetProcessUserModeExceptionPolicy pSetProcessUserModeExceptionPolicy = (tSetProcessUserModeExceptionPolicy)GetProcAddress(kernel32, "SetProcessUserModeExceptionPolicy");
	if (pGetProcessUserModeExceptionPolicy && pSetProcessUserModeExceptionPolicy)
	{
		DWORD dwFlags;
		if (pGetProcessUserModeExceptionPolicy(&dwFlags))
		{
			pSetProcessUserModeExceptionPolicy(dwFlags & ~PROCESS_CALLBACK_FILTER_ENABLED); // turn off bit 1
		}
	}
}

#else

PLATFORM_INTERFACE void WriteMiniDump( const char *pszFilenameSuffix )
{
}

PLATFORM_INTERFACE void CatchAndWriteMiniDump( FnWMain pfn, int argc, tchar *argv[] )
{
	pfn( argc, argv );
}

#endif
#elif defined(_X360 )
PLATFORM_INTERFACE void WriteMiniDump( const char *pszFilenameSuffix )
{
	DmCrashDump(false);
}

#else // !_WIN32
#include "tier0/minidump.h"

PLATFORM_INTERFACE void WriteMiniDump( const char *pszFilenameSuffix )
{
}

PLATFORM_INTERFACE void CatchAndWriteMiniDump( FnWMain pfn, int argc, tchar *argv[] )
{
	pfn( argc, argv );
}

#endif 

// User minidump stream info comment strings.
//
// Single header string of 512 bytes set via MinidumpUserStreamInfoSetHeader.
static char g_UserStreamInfoHeader[ 512 ];
// Array of 32 round robin 128 byte strings set via MinidumpUserStreamInfoAppend.
static char g_UserStreamInfo[ 64 ][ 128 ];
static int g_UserStreamInfoIndex = 0;

// Set the single g_UserStreamInfoHeader string.
void MinidumpUserStreamInfoSetHeader( const char *pFormat, ... )
{
	va_list marker;

	va_start( marker, pFormat );
	_vsnprintf( g_UserStreamInfoHeader, ARRAYSIZE( g_UserStreamInfoHeader ), pFormat, marker );
	g_UserStreamInfoHeader[ ARRAYSIZE( g_UserStreamInfoHeader ) - 1 ] = 0;
	va_end( marker );
}

// Set the next comment in the g_UserStreamInfo array.
void MinidumpUserStreamInfoAppend( const char *pFormat, ... )
{
	va_list marker;
	char *pData = g_UserStreamInfo[ g_UserStreamInfoIndex ];
	const int DataSize = ARRAYSIZE( g_UserStreamInfo[ g_UserStreamInfoIndex ] );

	// Add tick count just so we have a general idea of when this event happened.
	_snprintf( pData, DataSize, "[%x]", Plat_MSTime() );
	pData[ DataSize - 1 ] = 0;
	size_t HeaderLen = strlen( pData );

	va_start( marker, pFormat );
	_vsnprintf( pData + HeaderLen, DataSize - HeaderLen, pFormat, marker );
	pData[ DataSize - 1 ] = 0;
	va_end( marker );

	// Bump up index, and go back to 0 if we've hit the end.
	g_UserStreamInfoIndex++;
	if( g_UserStreamInfoIndex >= ARRAYSIZE( g_UserStreamInfo ) )
	{
		g_UserStreamInfoIndex = 0;
	}
}

// Retrieve the string given the Index.
//	Index 0: header string
//	Index 1+: comment string
//	Returns NULL when you've reached the end of the comment string array
//  Empty strings ("\0") can be returned if comment hasn't been set
const char *MinidumpUserStreamInfoGet( int Index )
{
	if( ( Index < 0 ) || ( Index >= (ARRAYSIZE( g_UserStreamInfo ) + 1) ) ) //+1 because we map 0 to the header
		return NULL;

	if( Index == 0 )
		return g_UserStreamInfoHeader;

	Index = ( (Index + (ARRAYSIZE( g_UserStreamInfo ) - 1)) + //subtract 1 in a way that circularly wraps. Since 0 maps to the header, the comment indices are 1 based
		g_UserStreamInfoIndex ) //start with our oldest comment
		% ARRAYSIZE( g_UserStreamInfo ); //circular buffer wrapping

	return g_UserStreamInfo[ Index ];
}



//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Xbox console link
//
//=====================================================================================//

#include "xbox/xbox_console.h"
#include "xbox/xbox_vxconsole.h"
#include "tier0/threadtools.h"
#include "tier0/tslist.h"
#include "tier0/ICommandLine.h"
#include "tier0/memdbgon.h"

// all redirecting funneled here, stop redirecting in this module only
#undef OutputDebugStringA

#pragma comment( lib, "xbdm.lib" )

struct DebugString_t
{
	unsigned int	color;
	char			*pString;
};

#define XBX_DBGCOMMANDPREFIX	"XCMD"
#define XBX_DBGRESPONSEPREFIX	"XACK"
#define XBX_DBGPRINTPREFIX		"XPRT"
#define XBX_DBGCOLORPREFIX		"XCLR"

#define XBX_MAX_RCMDLENGTH		256
#define XBX_MAX_MESSAGE			2048

CThreadFastMutex				g_xbx_dbgChannelMutex;
CThreadFastMutex				g_xbx_dbgCommandHandlerMutex;
static char						g_xbx_dbgRemoteBuf[XBX_MAX_RCMDLENGTH];
static HANDLE					g_xbx_dbgValidEvent;
static HANDLE					g_xbx_dbgCmdCompleteEvent;
bool							g_xbx_bUseVXConsoleOutput = true;
bool							g_xbx_bDoSyncOutput;
static ThreadHandle_t			g_xbx_hDebugThread;
CTSQueue<DebugString_t>			g_xbx_DebugStringQueue;
extern CInterlockedInt			g_xbx_numProfileCounters;
extern unsigned int				g_xbx_profileCounters[];
extern char						g_xbx_profileName[];
int								g_xbx_freeMemory;

_inline bool XBX_NoXBDM()			{ return false; }

static CXboxConsole XboxConsole;

DLL_EXPORT IXboxConsole *GetConsoleInterface()
{
	return &XboxConsole;
}

//-----------------------------------------------------------------------------
//	Low level string output.
//	Input string should be stack based, can get clobbered.
//-----------------------------------------------------------------------------
static void OutputStringToDevice( unsigned int color, char *pString, bool bRemoteValid )
{
	if ( !bRemoteValid )
	{
		// local debug only
		OutputDebugStringA( pString );
		return;
	}

	// remote debug valid
	// non pure colors don't translate well - find closest pure hue
	unsigned int bestColor = color;
	int r = ( bestColor & 0xFF );
	int g = ( bestColor >> 8 ) & 0xFF;
	int b = ( bestColor >> 16 ) & 0xFF;
	if ( ( r && r != 255 ) || ( g && g != 255 ) || ( b && b != 255 ) )
	{
		int r0, g0, b0;
		unsigned int minDist = 0xFFFFFFFF;
		for ( int i=0; i<8; i++ )
		{
			r0 = g0 = b0 = 0;
			if ( i&4 )
				r0 = 255;
			if ( i&2 )
				g0 = 255;
			if ( i&1 )
				b0 = 255;
			unsigned int d = ( r-r0 )*( r-r0 ) + ( g-g0 )*( g-g0 ) + ( b-b0 )*( b-b0 );
			if ( minDist > d )
			{
				minDist = d;
				bestColor = XMAKECOLOR( r0, g0, b0 );
			}
		}
	}

	// create color string
	char colorString[16];
	sprintf( colorString, XBX_DBGCOLORPREFIX "[%8.8x]", bestColor );

	// chunk line out, for each cr
	char strBuffer[XBX_MAX_RCMDLENGTH];
	char *pStart = pString;
	char *pEnd = pStart + strlen( pStart );
	char *pNext;
	while ( pStart < pEnd )
	{
		pNext = strchr( pStart, '\n' );
		if ( !pNext )
			pNext = pEnd;
		else
			*pNext = '\0';

		int length = _snprintf( strBuffer, XBX_MAX_RCMDLENGTH, "%s!%s%s", XBX_DBGPRINTPREFIX, colorString, pStart );
		if ( length == -1 )
		{
			strBuffer[sizeof( strBuffer )-1] = '\0';
		}
		// Send the string
		DmSendNotificationString( strBuffer );

		// advance past cr
		pStart = pNext+1;
	}
}

//-----------------------------------------------------------------------------
//	XBX_IsConsoleConnected
//
//-----------------------------------------------------------------------------
bool CXboxConsole::IsConsoleConnected()
{
	bool bConnected;
 
	if ( g_xbx_dbgValidEvent == NULL )
	{
		// init was never called
		return false;
	}

	AUTO_LOCK_FM( g_xbx_dbgChannelMutex );

	bConnected = ( WaitForSingleObject( g_xbx_dbgValidEvent, 0 ) == WAIT_OBJECT_0 );

	return bConnected;
}

//-----------------------------------------------------------------------------
//	Output string to listening console. Queues output for slave thread.
//	Needs to be lightweight.
//-----------------------------------------------------------------------------
void CXboxConsole::DebugString( unsigned int color, const char* pFormat, ... )
{
	if ( XBX_NoXBDM() )
		return;

	va_list	args;
	char	szStringBuffer[XBX_MAX_MESSAGE];
	int		length;

	// resolve string
	va_start( args, pFormat );
	length = _vsnprintf( szStringBuffer, sizeof( szStringBuffer ), pFormat, args );
	if ( length == -1 )
	{
		szStringBuffer[sizeof( szStringBuffer ) - 1] = '\0';
	}
	va_end( args );

	if ( !g_xbx_bDoSyncOutput )
	{
		// queue string for delayed output
		DebugString_t debugString;
		debugString.color = color;
		debugString.pString = strdup( szStringBuffer );
		g_xbx_DebugStringQueue.PushItem( debugString );
	}
	else
	{
		bool bRemoteValid = g_xbx_bUseVXConsoleOutput && XBX_IsConsoleConnected();
		OutputStringToDevice( color, szStringBuffer, bRemoteValid );
	}
}

//-----------------------------------------------------------------------------
//	Waits for debug queue to drain.
//-----------------------------------------------------------------------------
void CXboxConsole::FlushDebugOutput()
{
	while ( g_xbx_DebugStringQueue.Count() != 0 )
	{
		Sleep( 1 );
	}
}

//-----------------------------------------------------------------------------
//	_xdbg_strlen
//
//	Critical section safe.
//-----------------------------------------------------------------------------
int _xdbg_strlen( const CHAR* str )
{
    const CHAR* strEnd = str;

    while( *strEnd )
        strEnd++;

    return strEnd - str;
}

//-----------------------------------------------------------------------------
//	_xdbg_tolower
//
//	Critical section safe.
//-----------------------------------------------------------------------------
inline CHAR _xdbg_tolower( CHAR ch )
{
    if( ch >= 'A' && ch <= 'Z' )
        return ch - ( 'A' - 'a' );
    else
        return ch;
}

//-----------------------------------------------------------------------------
//	_xdbg_strnicmp
//
//	Critical section safe.
//-----------------------------------------------------------------------------
BOOL _xdbg_strnicmp( const CHAR* str1, const CHAR* str2, int n )
{
    while ( ( _xdbg_tolower( *str1 ) == _xdbg_tolower( *str2 ) ) && *str1 && n > 0 )
    {
        --n;
        ++str1;
        ++str2;
    }
    return ( !n || _xdbg_tolower( *str1 ) == _xdbg_tolower( *str2 ) );
}

//-----------------------------------------------------------------------------
//	_xdbg_strcpy
//
//	Critical section safe.
//-----------------------------------------------------------------------------
VOID _xdbg_strcpy( CHAR* strDest, const CHAR* strSrc )
{
    while ( ( *strDest++ = *strSrc++ ) != 0 );
}

//-----------------------------------------------------------------------------
//	_xdbg_strcpyn
//
//	Critical section safe.
//-----------------------------------------------------------------------------
VOID _xdbg_strcpyn( CHAR* strDest, const CHAR* strSrc, int numChars )
{
	while ( numChars>0 && ( *strDest++ = *strSrc++ ) != 0 )
		numChars--;
}

//-----------------------------------------------------------------------------
//	_xdbg_gettoken
//
//	Critical section safe.
//-----------------------------------------------------------------------------
void _xdbg_gettoken( CHAR** tokenStream, CHAR* token, int tokenSize )
{
	int		c;
	int		len;
	CHAR*	data;

	len = 0;

	// skip prefix whitespace
	data = *tokenStream;
	while ( ( c = *data ) <= ' ' ) 
	{
		if ( !c )
			goto cleanUp;
		data++;
	}

	// parse a token
	do
	{
		if ( len < tokenSize )
			token[len++] = c;

		data++;
		c = *data;
	} while ( c > ' ' );

	if ( len >= tokenSize ) 
		len = 0;

cleanUp:
	token[len]   = '\0';
	*tokenStream = data;
}

//-----------------------------------------------------------------------------
//	_xdbg_tokenize
//
//	Critical section safe.
//-----------------------------------------------------------------------------
int _xdbg_tokenize( CHAR* tokenStream, CHAR** tokens, int maxTokens )
{
	char	token[64];

	// tokenize stream into seperate tokens
	int	numTokens = 0;
	while ( 1 )
	{
		tokens[numTokens++] = tokenStream;
		if ( numTokens >= maxTokens )
			break;

		_xdbg_gettoken( &tokenStream, token, sizeof( token ) );
		if ( !tokenStream[0] || !token[0] )
			break;

		*tokenStream = '\0';
		tokenStream++;
	}
	return ( numTokens );
}

//-----------------------------------------------------------------------------
//	_xdbg_findtoken
//
//	Critical section safe. Returns -1 if not found
//-----------------------------------------------------------------------------
int _xdbg_findtoken( CHAR** tokens, int numTokens, CHAR* token )
{
	int	i;
	int	len;

	len = _xdbg_strlen( token );
	for ( i=0; i<numTokens; i++ )
	{
		if ( _xdbg_strnicmp( tokens[i], token, len ) )
			return i;
	}
	
	// not found
	return -1;
}

//-----------------------------------------------------------------------------
//	_DebugCommandHandler
//
//-----------------------------------------------------------------------------
HRESULT __stdcall _DebugCommandHandler( const CHAR* strCommand, CHAR* strResponse, DWORD dwResponseLen, PDM_CMDCONT pdmcc )
{
	CHAR			buff[256];
	CHAR*			args[8];
	int				numArgs;

	AUTO_LOCK_FM( g_xbx_dbgCommandHandlerMutex );

    // skip over the command prefix and the exclamation mark
    strCommand += _xdbg_strlen( XBX_DBGCOMMANDPREFIX ) + 1;

	if ( strCommand[0] == '\0' )
	{
		// just a ping
		goto cleanUp;
	}

	// get command and optional arguments
	_xdbg_strcpyn( buff, strCommand, sizeof( buff ) );
	numArgs = _xdbg_tokenize( buff, args, sizeof( args )/sizeof( CHAR* ) );

    if ( _xdbg_strnicmp( args[0], "__connect__", 11 ) )
    {
		if ( numArgs > 1 && atoi( args[1] ) == VXCONSOLE_PROTOCOL_VERSION )
		{
			// initial connect - respond that we're connected
			_xdbg_strcpyn( strResponse, XBX_DBGRESPONSEPREFIX "Connected To Application.", dwResponseLen );
			SetEvent( g_xbx_dbgValidEvent );

			// notify convar system to send its commands
			// allows vxconsole to re-connect during game
			_xdbg_strcpy( g_xbx_dbgRemoteBuf, "getcvars" );
			XBX_QueueEvent( XEV_REMOTECMD, ( int )g_xbx_dbgRemoteBuf, 0, 0 );
		}
		else
		{
			_xdbg_strcpyn( strResponse, XBX_DBGRESPONSEPREFIX "Rejecting Connection: Wrong Protocol Version.", dwResponseLen );
		}
		goto cleanUp;
    }

	if ( _xdbg_strnicmp( args[0], "__disconnect__", 14 ) )
    {
		// respond that we're disconnected
        _xdbg_strcpyn( strResponse, XBX_DBGRESPONSEPREFIX "Disconnected.", dwResponseLen );
		ResetEvent( g_xbx_dbgValidEvent );
		goto cleanUp;
    }

    if ( _xdbg_strnicmp( args[0], "__complete__", 12 ) )
    {
		// remote server has finished command - respond to acknowledge
		_xdbg_strcpyn( strResponse, XBX_DBGRESPONSEPREFIX "OK", dwResponseLen );

		// set the complete event - allows expected synchronous calling mechanism
		SetEvent( g_xbx_dbgCmdCompleteEvent );
        goto cleanUp;
    }

    if ( _xdbg_strnicmp( args[0], "__memory__", 10 ) )
    {		
		// get a current stat of available memory
		MEMORYSTATUS stat;
		GlobalMemoryStatus( &stat );
		g_xbx_freeMemory = stat.dwAvailPhys;

		if ( _xdbg_findtoken( args, numArgs, "quiet" ) > 0 )
		{
			_xdbg_strcpyn( strResponse, XBX_DBGRESPONSEPREFIX "OK", dwResponseLen );
		}
		else
		{
			// 32 MB is reserved and fixed by OS, so not reporting
			_snprintf( strResponse, dwResponseLen, XBX_DBGRESPONSEPREFIX "Available: %.2f MB, Used: %.2f MB, Free: %.2f MB", 
				stat.dwTotalPhys/( 1024.0f*1024.0f ) - 32.0f,
				( stat.dwTotalPhys - stat.dwAvailPhys )/( 1024.0f*1024.0f ) - 32.0f, 
				stat.dwAvailPhys/( 1024.0f*1024.0f ) );
		}
		goto cleanUp;
	}

	if ( g_xbx_dbgRemoteBuf[0] )
	{
		// previous command still pending
		_xdbg_strcpyn( strResponse, XBX_DBGRESPONSEPREFIX "Cannot execute: Previous command still pending", dwResponseLen );
	}
	else
	{
		// add the command to the event queue to be processed by main app
		_xdbg_strcpy( g_xbx_dbgRemoteBuf, strCommand );
		XBX_QueueEvent( XEV_REMOTECMD, ( int )g_xbx_dbgRemoteBuf, 0, 0 );
		_xdbg_strcpyn( strResponse, XBX_DBGRESPONSEPREFIX "OK", dwResponseLen );
	}

cleanUp:
    return XBDM_NOERR;
}

//-----------------------------------------------------------------------------
//	XBX_SendRemoteCommand
//
//-----------------------------------------------------------------------------
void CXboxConsole::SendRemoteCommand( const char *pCommand, bool async )
{
	char cmdString[XBX_MAX_RCMDLENGTH];

	if ( XBX_NoXBDM() || !IsConsoleConnected() )
		return;

	AUTO_LOCK_FM( g_xbx_dbgChannelMutex );

	_snprintf( cmdString, sizeof( cmdString ), "%s!%s", XBX_DBGCOMMANDPREFIX, pCommand );
	HRESULT hr = DmSendNotificationString( cmdString );
	if ( FAILED( hr ) )
	{
		XBX_Error( "XBX_SendRemoteCommand: failed on %s", cmdString );
	}

	// wait for command completion
	if ( !async )
	{
		DWORD timeout;
		if ( !strnicmp( pCommand, "Assert()", 8 ) )
		{
			// the assert is waiting for user to make selection
			timeout = INFINITE;
		}
		else
		{
			// no vxconsole operation should take this long
			timeout = 15000;
		}

		if ( WaitForSingleObject( g_xbx_dbgCmdCompleteEvent, timeout ) == WAIT_TIMEOUT )
		{
			// we have no choice but to dump core
			DmCrashDump( false );
		}
	}
}

//-----------------------------------------------------------------------------
//	Handle delayed VXConsole transactions
//
//-----------------------------------------------------------------------------
static unsigned _DebugThreadFunc( void *pParam )
{
	while ( 1 )
	{
		Sleep( 10 );

		if ( !g_xbx_DebugStringQueue.Count() && !g_xbx_numProfileCounters && !g_xbx_freeMemory )
		{
			continue;
		}

		if ( g_xbx_numProfileCounters )
		{
			// build and send asynchronously
			char dbgCommand[XBX_MAX_RCMDLENGTH];
			_snprintf( dbgCommand, sizeof( dbgCommand ), "SetProfileData() %s 0x%8.8x", g_xbx_profileName, g_xbx_profileCounters );
			XBX_SendRemoteCommand( dbgCommand, true );

			// mark as sent
			g_xbx_numProfileCounters = 0;
		}

		if ( g_xbx_freeMemory )
		{
			// build and send asynchronously
			char dbgCommand[XBX_MAX_RCMDLENGTH];
			_snprintf( dbgCommand, sizeof( dbgCommand ), "FreeMemory() 0x%8.8x", g_xbx_freeMemory );
			XBX_SendRemoteCommand( dbgCommand, true );

			// mark as sent
			g_xbx_freeMemory = 0;
		}

		bool bRemoteValid = g_xbx_bUseVXConsoleOutput && XBX_IsConsoleConnected();
		while ( 1 )
		{
			DebugString_t debugString;
			if ( !g_xbx_DebugStringQueue.PopItem( &debugString ) )
			{
				break;
			}

			OutputStringToDevice( debugString.color, debugString.pString, bRemoteValid );
			free( debugString.pString );
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
//	XBX_InitConsoleMonitor
//
//-----------------------------------------------------------------------------
void CXboxConsole::InitConsoleMonitor( bool bWaitForConnect )
{
	if ( XBX_NoXBDM() )
		return;

	// create our events
	g_xbx_dbgValidEvent = CreateEvent( XBOX_DONTCARE, TRUE, FALSE, NULL );
	g_xbx_dbgCmdCompleteEvent = CreateEvent( XBOX_DONTCARE, FALSE, FALSE, NULL );

	// register our command handler with the debug monitor
	HRESULT hr = DmRegisterCommandProcessor( XBX_DBGCOMMANDPREFIX, _DebugCommandHandler );
	if ( FAILED( hr ) )
	{
		XBX_Error( "XBX_InitConsoleMonitor: failed to register command processor" );
	}

	// user can have output bypass slave thread
	g_xbx_bDoSyncOutput = CommandLine()->FindParm( "-syncoutput" ) != 0;

	// create a slave thread to do delayed VXConsole transactions
	ThreadId_t threadID;
	g_xbx_hDebugThread = CreateSimpleThread( _DebugThreadFunc, NULL, &threadID, 16*1024 ); 
	ThreadSetDebugName( threadID, "DebugThread" );
	ThreadSetAffinity( g_xbx_hDebugThread, XBOX_PROCESSOR_5 );

	if ( bWaitForConnect )
	{
		XBX_DebugString( XBX_CLR_DEFAULT, "Waiting For VXConsole Connection...\n" );
		WaitForSingleObject( g_xbx_dbgValidEvent, INFINITE );
	}
}

//-----------------------------------------------------------------------------
//	Sends a disconnect signal to possibly attached VXConsole.
//-----------------------------------------------------------------------------
void CXboxConsole::DisconnectConsoleMonitor()
{
	if ( XBX_NoXBDM() )
		return;

	// caller is trying to safely stop vxconsole traffic, disconnect must be synchronous
	XBX_SendRemoteCommand( "Disconnect()", false );
}

bool CXboxConsole::GetXboxName( char *pName, unsigned *pLength )
{
	return ( DmGetXboxName( pName, (DWORD *)pLength ) == XBDM_NOERR );
}

void CXboxConsole::CrashDump( bool b )
{
	DmCrashDump(b);
}

//-----------------------------------------------------------------------------
// Walk to a specific module and dump size info
//-----------------------------------------------------------------------------
int CXboxConsole::DumpModuleSize( const char *pName )
{
	HRESULT error;
	PDM_WALK_MODULES pWalkMod = NULL;
	DMN_MODLOAD modLoad;
	int size = 0;

	// iterate and find match
	do 
	{
		error = DmWalkLoadedModules( &pWalkMod, &modLoad );
		if ( XBDM_NOERR == error && !stricmp( modLoad.Name, pName ) )
		{
			Msg( "0x%8.8x, %5.2f MB, %s\n", modLoad.BaseAddress, modLoad.Size/( 1024.0f*1024.0f ), modLoad.Name );
			size = modLoad.Size;
			error = XBDM_ENDOFLIST;
		}
	} 
	while ( XBDM_NOERR == error );
	DmCloseLoadedModules( pWalkMod );

	if ( error != XBDM_ENDOFLIST )
	{
		Warning( "DmWalkLoadedModules() failed.\n" );
	}

	return size;
}

//-----------------------------------------------------------------------------
// 360 spew sizes of dll modules
//-----------------------------------------------------------------------------
char const* HACK_stristr( char const* pStr, char const* pSearch ) // hack because moved code from above vstdlib
{
	AssertValidStringPtr(pStr);
	AssertValidStringPtr(pSearch);

	if (!pStr || !pSearch) 
		return 0;

	char const* pLetter = pStr;

	// Check the entire string
	while (*pLetter != 0)
	{
		// Skip over non-matches
		if (tolower((unsigned char)*pLetter) == tolower((unsigned char)*pSearch))
		{
			// Check for match
			char const* pMatch = pLetter + 1;
			char const* pTest = pSearch + 1;
			while (*pTest != 0)
			{
				// We've run off the end; don't bother.
				if (*pMatch == 0)
					return 0;

				if (tolower((unsigned char)*pMatch) != tolower((unsigned char)*pTest))
					break;

				++pMatch;
				++pTest;
			}

			// Found a match!
			if (*pTest == 0)
				return pLetter;
		}

		++pLetter;
	}

	return 0;
}

void CXboxConsole::DumpDllInfo( const char *pBasePath )
{
	// Directories containing dlls
	static char *dllDirs[] = 
	{
		"bin",
		"hl2\\bin",
		"tf\\bin",
		"portal\\bin",
		"episodic\\bin"
	};

	char binPath[MAX_PATH];
	char dllPath[MAX_PATH];
	char searchPath[MAX_PATH];

	HMODULE hModule;
	WIN32_FIND_DATA wfd;
	HANDLE hFind;

	Msg( "Dumping Module Sizes...\n" );

	for ( int i = 0; i < ARRAYSIZE( dllDirs ); ++i )
	{
		int totalSize = 0;

		_snprintf( binPath, sizeof( binPath ), "%s\\%s", pBasePath, dllDirs[i] );
		_snprintf( searchPath, sizeof( binPath ), "%s\\*.dll", binPath );

		// show the directory we're searching
		Msg( "\nDirectory: %s\n\n", binPath );

		// Start the find and check for failure.
		hFind = FindFirstFile( searchPath, &wfd );
		if ( INVALID_HANDLE_VALUE == hFind )
		{
			Warning( "No Files Found.\n" );
		}
		else
		{
			// Load and unload each dll individually.
			do
			{
				if ( !HACK_stristr( wfd.cFileName, "_360.dll" ) )
				{
					// exclude explicit pc dlls
					// FindFirstFile does not support a spec mask of *_360.dll on the Xbox HDD
					continue;
				}

				_snprintf( dllPath, sizeof( dllPath ), "%s\\%s", binPath, wfd.cFileName );
				hModule = LoadLibrary( dllPath );
				if ( hModule )
				{
					totalSize += DumpModuleSize( wfd.cFileName );
					FreeLibrary( hModule );
				}
				else
				{
					Warning( "Failed to load: %s\n", dllPath );
				}
			} 
			while( FindNextFile( hFind, &wfd ) );

			FindClose( hFind );

			Msg( "Total Size: %.2f MB\n", totalSize/( 1024.0f*1024.0f ) ); 
		}
	}
}

void CXboxConsole::OutputDebugString( const char *p )
{
	::OutputDebugStringA( p );
}

bool CXboxConsole::IsDebuggerPresent()
{
	return ( DmIsDebuggerPresent() != 0 );
}

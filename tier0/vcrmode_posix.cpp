//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "tier0/threadtools.h"

#define PROTECTED_THINGS_DISABLE
#include "tier0/vcrmode.h"
#include "tier0/dbg.h"
#include "extendedtrace.h"

// FIXME: We totally have a bad tier dependency here
#include "inputsystem/InputEnums.h"

#define VCRFILE_VERSION		2

#define VCR_RuntimeAssert(x)	VCR_RuntimeAssertFn(x, #x)
#define PvAlloc malloc

bool		g_bExpectingWindowProcCalls = false;

IVCRHelpers	*g_pHelpers = 0;

FILE		*g_pVCRFile = NULL;
VCRMode_t		g_VCRMode = VCR_Disabled;

VCRMode_t		g_OldVCRMode = (VCRMode_t)-1;		// Stored temporarily between SetEnabled(0)/SetEnabled(1) blocks.
int			g_iCurEvent = 0;

int			g_CurFilePos = 0;				// So it knows when we're done playing back.
int			g_FileLen = 0;					

VCREvent	g_LastReadEvent = (VCREvent)-1;	// Last VCR_ReadEvent() call.

int			g_bVCREnabled = 0;


// ---------------------------------------------------------------------- //
// Internal functions.
// ---------------------------------------------------------------------- //
static void VCR_Error( const char *pFormat, ... )
{
#if defined( _DEBUG )
	DebuggerBreak();
#endif
	char str[256];
	va_list marker;
	va_start( marker, pFormat );
	_snprintf( str, sizeof( str ), pFormat, marker );
	va_end( marker );

	g_pHelpers->ErrorMessage( str );
	VCREnd();
}

static void VCR_RuntimeAssertFn(int bAssert, char const *pStr)
{
	if(!bAssert)
	{
		VCR_Error( "*** VCR ASSERT FAILED: %s ***\n", pStr );
	}
}

static void VCR_Read(void *pDest, int size)
{
	if(!g_pVCRFile)
	{
		memset(pDest, 0, size);
		return;
	}

	fread(pDest, 1, size, g_pVCRFile);
	
	g_CurFilePos += size;
	
	VCR_RuntimeAssert(g_CurFilePos <= g_FileLen);
	
	if(g_CurFilePos >= g_FileLen)
	{
		VCREnd();
	}
}

template<class T>
static void VCR_ReadVal(T &val)
{
	VCR_Read(&val, sizeof(val));
}

static void VCR_Write(void const *pSrc, int size)
{
	fwrite(pSrc, 1, size, g_pVCRFile);
	fflush(g_pVCRFile);
}

template<class T>
static void VCR_WriteVal(T &val)
{
	VCR_Write(&val, sizeof(val));
}


// Hook from ExtendedTrace.cpp
bool g_bTraceRead = false;
void OutputDebugStringFormat( const char *pMsg, ... )
{
	char msg[4096];
	va_list marker;
	va_start( marker, pMsg );
	_vsnprintf( msg, sizeof( msg )-1, pMsg, marker );
	va_end( marker );
	int len = strlen( msg );
	
	if ( g_bTraceRead )
	{
		char tempData[4096];
		int tempLen;
		VCR_ReadVal( tempLen );
		VCR_RuntimeAssert( tempLen <= sizeof( tempData ) );
		VCR_Read( tempData, tempLen );
		tempData[tempLen] = 0;
		fprintf( stderr, "FILE: " );
		fprintf( stderr, "%s", tempData );

		VCR_RuntimeAssert( memcmp( msg, tempData, len ) == 0 );
	}
	else
	{
		VCR_WriteVal( len );
		VCR_Write( msg, len );
	}
}


static VCREvent VCR_ReadEvent()
{
	g_bTraceRead = true;
	//STACKTRACE();

	char event;
	VCR_Read(&event, 1);
	g_LastReadEvent = (VCREvent)event;

	return (VCREvent)event;
}


static void VCR_WriteEvent(VCREvent event)
{
	g_bTraceRead = false;
	//STACKTRACE();

	// Write a stack trace.
	char cEvent = (char)event;
	VCR_Write(&cEvent, 1);
}

static void VCR_IncrementEvent()
{
	++g_iCurEvent;
}

static void VCR_Event(VCREvent type)
{
	if(g_VCRMode == VCR_Disabled)
		return;

	VCR_IncrementEvent();
	if(g_VCRMode == VCR_Record)
	{
		VCR_WriteEvent(type);
	}
	else
	{
		VCREvent currentEvent = VCR_ReadEvent();
		VCR_RuntimeAssert( currentEvent == type );
	}
}


// ---------------------------------------------------------------------- //
// VCR trace interface.
// ---------------------------------------------------------------------- //

class CVCRTrace : public IVCRTrace
{
public:
	virtual VCREvent	ReadEvent()
	{
		return VCR_ReadEvent();
	}

	virtual void		Read( void *pDest, int size )
	{
		VCR_Read( pDest, size );
	}
};

static CVCRTrace g_VCRTrace;


// ---------------------------------------------------------------------- //
// VCR interface.
// ---------------------------------------------------------------------- //

static int VCR_Start( char const *pFilename, bool bRecord, IVCRHelpers *pHelpers )
{
	unsigned long version;

	g_pHelpers = pHelpers;
	
	VCREnd();

	EXTENDEDTRACEINITIALIZE( "/tmp/hl2" );

	g_OldVCRMode = (VCRMode_t)-1;
	if(bRecord)
	{
		g_pVCRFile = fopen( pFilename, "wb" );
		if( g_pVCRFile )
		{
			// Write the version.
			version = VCRFILE_VERSION;
			VCR_Write(&version, sizeof(version));

			g_VCRMode = VCR_Record;
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		g_pVCRFile = fopen( pFilename, "rb" );
		if( g_pVCRFile )
		{
			// Get the file length.
			fseek(g_pVCRFile, 0, SEEK_END);
			g_FileLen = ftell(g_pVCRFile);
			fseek(g_pVCRFile, 0, SEEK_SET);
			g_CurFilePos = 0;

			// Verify the file version.
			VCR_Read(&version, sizeof(version));
			if(version != VCRFILE_VERSION)
			{
				assert(!"VCR_Start: invalid file version");
				VCREnd();
				return FALSE;
			}
			
			g_VCRMode = VCR_Playback;
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
}


static void VCR_End()
{
	if(g_pVCRFile)
	{
		fclose(g_pVCRFile);
		g_pVCRFile = NULL;
	}

	g_VCRMode = VCR_Disabled;
	EXTENDEDTRACEUNINITIALIZE();
}


static IVCRTrace* VCR_GetVCRTraceInterface()
{
	return &g_VCRTrace;
}


static VCRMode_t VCR_GetMode()
{
	return g_VCRMode;
}


static void VCR_SetEnabled(int bEnabled)
{
	if(bEnabled)
	{
		VCR_RuntimeAssert(g_OldVCRMode != (VCRMode_t)-1);
		g_VCRMode = g_OldVCRMode;
		g_OldVCRMode = (VCRMode_t)-1;
	}
	else
	{
		VCR_RuntimeAssert(g_OldVCRMode == (VCRMode_t)-1);
		g_OldVCRMode = g_VCRMode;
		g_VCRMode = VCR_Disabled;
	}
}


static void VCR_SyncToken(char const *pToken)
{
	unsigned char len;

	VCR_Event(VCREvent_SyncToken);

	if(g_VCRMode == VCR_Record)
	{
		int intLen = strlen( pToken );
		assert( intLen <= 255 );

		len = (unsigned char)intLen;
		
		VCR_Write(&len, 1);
		VCR_Write(pToken, len);
	}
	else if(g_VCRMode == VCR_Playback)
	{
		char test[256];

		VCR_Read(&len, 1);
		VCR_Read(test, len);
		
		VCR_RuntimeAssert( len == (unsigned char)strlen(pToken) );
		VCR_RuntimeAssert( memcmp(pToken, test, len) == 0 );
	}
}


static double VCR_Hook_Sys_FloatTime(double time)
{
	VCR_Event(VCREvent_Sys_FloatTime);

	if(g_VCRMode == VCR_Record)
	{
		VCR_Write(&time, sizeof(time));
	}
	else if(g_VCRMode == VCR_Playback)
	{
		VCR_Read(&time, sizeof(time));
	}

	return time;
}



static int VCR_Hook_PeekMessage(
	struct tagMSG *msg, 
	void *hWnd, 
	unsigned int wMsgFilterMin, 
	unsigned int wMsgFilterMax, 
	unsigned int wRemoveMsg
	)
{
	Assert( "VCR_Hook_PeekMessage unsupported" );
	return 0;
}


void VCR_Hook_RecordGameMsg( const InputEvent_t& event )
{
	if ( g_VCRMode == VCR_Record )
	{
		VCR_Event( VCREvent_GameMsg );
		
		char val = 1;
		VCR_WriteVal( val );
		VCR_WriteVal( event.m_nType );
		VCR_WriteVal( event.m_nData );
		VCR_WriteVal( event.m_nData2 );
		VCR_WriteVal( event.m_nData3 );
	}
}


void VCR_Hook_RecordEndGameMsg()
{
	if ( g_VCRMode == VCR_Record )
	{
		VCR_Event( VCREvent_GameMsg );
		char val = 0;
		VCR_WriteVal( val );	// record that there are no more messages.
	}
}


bool VCR_Hook_PlaybackGameMsg( InputEvent_t* pEvent  )
{
	if ( g_VCRMode == VCR_Playback )
	{
		VCR_Event( VCREvent_GameMsg );
		
		char bMsg;
		VCR_ReadVal( bMsg );
		if ( bMsg )
		{
			VCR_ReadVal( pEvent->m_nType );
			VCR_ReadVal( pEvent->m_nData );
			VCR_ReadVal( pEvent->m_nData2 );
			VCR_ReadVal( pEvent->m_nData3 );
			return true;
		}
	}
	
	return false;
}



static void VCR_Hook_GetCursorPos(struct tagPOINT *pt)
{
	Assert( "VCR_Hook_GetCursorPos unsupported" );
}


static void VCR_Hook_ScreenToClient(void *hWnd, struct tagPOINT *pt)
{
	Assert( "VCR_Hook_GetCursorPos unsupported" );
}


static int VCR_Hook_recvfrom(int s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
	VCR_Event(VCREvent_recvfrom);

	int ret;
	if ( g_VCRMode == VCR_Playback )
	{
		// Get the result from our file.
		VCR_Read(&ret, sizeof(ret));
		if(ret == -1)
		{
			int err;
			VCR_ReadVal(err);
			errno = err;
		}
		else
		{
			VCR_Read( buf, ret );

			char bFrom;
			VCR_ReadVal( bFrom );
			if ( bFrom )
			{
				VCR_Read( from, *fromlen );
			}
		}
	}
	else
	{
		ret = recvfrom(s, buf, len, flags, from, (socklen_t *)(fromlen));

		if ( g_VCRMode == VCR_Record )
		{
			// Record the result.
			VCR_Write(&ret, sizeof(ret));
			if(ret == -1)
			{
				VCR_WriteVal(errno);
			}
			else
			{
				VCR_Write( buf, ret );
				
				char bFrom = !!from;
				VCR_WriteVal( bFrom );
				if ( bFrom )
					VCR_Write( from, *fromlen );
			}
		}
	}

	return ret;
}


static void VCR_Hook_Cmd_Exec(char **f)
{
	VCR_Event(VCREvent_Cmd_Exec);

	if(g_VCRMode == VCR_Playback)
	{
		int len;

		VCR_Read(&len, sizeof(len));
		if(len == -1)
		{
			*f = NULL;
		}
		else
		{
			*f = (char*)PvAlloc(len);
			VCR_Read(*f, len);
		}
	}
	else if(g_VCRMode == VCR_Record)
	{
		int len;
		char *str = *f;

		if(str)
		{
			len = strlen(str)+1;
			VCR_Write(&len, sizeof(len));
			VCR_Write(str, len);
		}
		else
		{
			len = -1;
			VCR_Write(&len, sizeof(len));
		}
	}
}

#define MAX_LINUX_CMDLINE 512
static char linuxCmdline[ MAX_LINUX_CMDLINE +7 ]; // room for -steam

const char * BuildCmdLine( int argc, char **argv, bool fAddSteam )
{
	int len;
	int i;

	for (len = 0, i = 0; i < argc; i++)
	{
		len += strlen(argv[i]);
	}

	if ( len > MAX_LINUX_CMDLINE )
	{
		printf( "command line too long, %i max\n", MAX_LINUX_CMDLINE );
		exit(-1);
		return "";
	}

	linuxCmdline[0] = '\0';
	for ( i = 0; i < argc; i++ )
	{
		if ( i > 0 )
		{
			strcat( linuxCmdline, " " );
		}
		strcat( linuxCmdline, argv[ i ] );
	}

	if( fAddSteam )
	{
		strcat( linuxCmdline, " -steam" );
	}

	return linuxCmdline;
}

char *GetCommandLine()
{
	return linuxCmdline;
}


static char* VCR_Hook_GetCommandLine()
{
	VCR_Event(VCREvent_CmdLine);

	int len;
	char *ret;

	if(g_VCRMode == VCR_Playback)
	{
		VCR_Read(&len, sizeof(len));
		ret = new char[len];
		VCR_Read(ret, len);
	}
	else
	{
		ret = GetCommandLine();

		if(g_VCRMode == VCR_Record)
		{
			len = strlen(ret) + 1;
			VCR_WriteVal(len);
			VCR_Write(ret, len);
		}
	}
	
	return ret;
}


static long VCR_Hook_RegOpenKeyEx( void *hKey, const char *lpSubKey, unsigned long ulOptions, unsigned long samDesired, void *pHKey )
{
	Assert( "VCR_Hook_RegOpenKeyEx unsupported" );
	return 0;
}


static long VCR_Hook_RegSetValueEx(void *hKey, char const *lpValueName, unsigned long Reserved, unsigned long dwType, unsigned char const *lpData, unsigned long cbData)
{
	Assert( "VCR_Hook_RegSetValueEx unsupported" );
	return 0;
}


static long VCR_Hook_RegQueryValueEx(void *hKey, char const *lpValueName, unsigned long *lpReserved, unsigned long *lpType, unsigned char *lpData, unsigned long *lpcbData)
{
	Assert( "VCR_Hook_RegQueryValueEx unsupported" );
	return 0;
}


static long VCR_Hook_RegCreateKeyEx(void *hKey, char const *lpSubKey, unsigned long Reserved, char *lpClass, unsigned long dwOptions, 
	unsigned long samDesired, void *lpSecurityAttributes, void *phkResult, unsigned long *lpdwDisposition)
{
	Assert( "VCR_Hook_RegCreateKeyEx unsupported" );
	return 0;
}


static void VCR_Hook_RegCloseKey(void *hKey)
{
	Assert( "VCR_Hook_RegCloseKey unsupported" );
}


int VCR_Hook_GetNumberOfConsoleInputEvents( void *hInput, unsigned long *pNumEvents )
{
	VCR_Event( VCREvent_GetNumberOfConsoleInputEvents );

	char ret;
	if ( g_VCRMode == VCR_Playback )
	{
		VCR_ReadVal( ret );
		VCR_ReadVal( *pNumEvents );
	}
	else
	{
		ret = 1;

		if ( g_VCRMode == VCR_Record )
		{
			VCR_WriteVal( ret );
			VCR_WriteVal( *pNumEvents );
		}
	}

	return ret;
}


int	VCR_Hook_ReadConsoleInput( void *hInput, void *pRecs, int nMaxRecs, unsigned long *pNumRead )
{
	Assert( "VCR_Hook_ReadConsoleInput unsupported" );
	return 0;
}


void VCR_Hook_LocalTime( struct tm *today )
{
	// We just provide a wrapper on this function so we can protect access to time() everywhere.
	time_t ltime;
	time( &ltime );
	tm *pTime = localtime( &ltime );
	memcpy( today, pTime, sizeof( *today ) );
}


short VCR_Hook_GetKeyState( int nVirtKey )
{
	Assert( "VCREvent_GetKeyState unsupported" );
	return 0;
}

void VCR_GenericRecord( const char *pEventName, const void *pData, int len )
{
        VCR_Event( VCREvent_Generic );

        if ( g_VCRMode != VCR_Record )
                Error( "VCR_GenericRecord( %s ): not recording a VCR file", pEventName );

        // Write the event name (or 255 if none).
        int nameLen = 255;
        if ( pEventName )
        {
                nameLen = strlen( pEventName ) + 1;
                if ( nameLen >= 255 )
                {
                        VCR_Error( "VCR_GenericRecord( %s ): nameLen too long (%d)", pEventName, nameLen );
                        return;
                }
        }
        unsigned char ucNameLen = (unsigned char)nameLen;
        VCR_WriteVal( ucNameLen );
        VCR_Write( pEventName, ucNameLen );

        // Write the data.
        VCR_WriteVal( len );
        VCR_Write( pData, len );
}


int VCR_GenericPlayback( const char *pEventName, void *pOutData, int maxLen, bool bForceSameLen )
{
        VCR_Event( VCREvent_Generic );

        if ( g_VCRMode != VCR_Playback )
                Error( "VCR_Playback( %s ): not playing back a VCR file", pEventName );

        unsigned char nameLen;
        VCR_ReadVal( nameLen );
        if ( nameLen != 255 )
        {
                char testName[512];
                VCR_Read( testName, nameLen );
                if ( strcmp( pEventName, testName ) != 0 )
                {
                        VCR_Error( "VCR_GenericPlayback( %s ) - event name does not match '%s'", pEventName, testName );
                        return 0;
                }
        }

        int dataLen;
        VCR_ReadVal( dataLen );
        if ( dataLen > maxLen )
        {
                VCR_Error( "VCR_GenericPlayback( %s ) - generic data too long (greater than maxLen: %d)", pEventName, maxLen );
                return 0;
        }
        else if ( bForceSameLen && dataLen != maxLen )
        {
                VCR_Error( "VCR_GenericPlayback( %s ) - data size in file (%d) different than desired (%d)", pEventName, dataLen, maxLen );
                return 0;
        }

        VCR_Read( pOutData, dataLen );
        return dataLen;
}


void VCR_GenericValue( const char *pEventName, void *pData, int maxLen )
{
        if ( g_VCRMode == VCR_Record )
                VCR_GenericRecord( pEventName, pData, maxLen );
        else if ( g_VCRMode == VCR_Playback )
                VCR_GenericPlayback( pEventName, pData, maxLen, true );
}


static int VCR_Hook_recv(int s, char *buf, int len, int flags)
{
        VCR_Event(VCREvent_recv);

        int ret;
        if ( g_VCRMode == VCR_Playback )
        {
                // Get the result from our file.
                VCR_Read(&ret, sizeof(ret));
                if(ret == -1)
                {
                        int err;
                        VCR_ReadVal(err);
                        errno = err;
                }
                else
                {
                        VCR_Read( buf, ret );
                }
        }
        else
        {
                ret = recv( s, buf, len, flags );

                if ( g_VCRMode == VCR_Record )
                {
                        // Record the result.
                        VCR_Write(&ret, sizeof(ret));
                        if(ret == -1)
                        {
                                VCR_WriteVal(errno);
                        }
                        else
                        {
                                VCR_Write( buf, ret );
                        }
                }
        }

        return ret;
}

static int VCR_Hook_send(int s, const char *buf, int len, int flags)
{
        VCR_Event(VCREvent_send);

        int ret;
        if ( g_VCRMode == VCR_Playback )
        {
                // Get the result from our file.
                VCR_Read(&ret, sizeof(ret));
                if(ret == -1)
                {
                        int err;
                        VCR_ReadVal(err);
			errno = err;
                }
        }
        else
        {
                ret = send( s, buf, len, flags );

                if ( g_VCRMode == VCR_Record )
                {
                        // Record the result.
                        VCR_Write(&ret, sizeof(ret));
                        if(ret == -1)
                        {
                                VCR_WriteVal(errno);
                        }
                }
        }

        return ret;
}



double VCR_GetPercentCompleted()
{
	if ( g_VCRMode == VCR_Playback )
	{
		return (double)g_CurFilePos / g_FileLen;
	}
	else
	{
		return 0;
	}
}

void* VCR_CreateThread( 
					   void *lpThreadAttributes,
					   unsigned long dwStackSize,
					   void *lpStartAddress,
					   void *lpParameter,
					   unsigned long dwCreationFlags,
					   unsigned long *lpThreadID )
{	
	return CreateSimpleThread( (ThreadFunc_t)lpStartAddress, lpParameter, lpThreadID, 0 );
}
					   


unsigned long VCR_WaitForSingleObject(
									  void *handle,
									  unsigned long dwMilliseconds )
{
	return -1;
}

unsigned long VCR_WaitForMultipleObjects( uint32 nHandles, const void **pHandles, int bWaitAll, uint32 timeout )
{
	return -1;
}

void VCR_EnterCriticalSection( void *pInputCS )
{
}


void VCR_Hook_Time( long *today )
{
	// We just provide a wrapper on this function so we can protect access to time() everywhere.
	// NOTE: For 64-bit systems we should eventually get a function that takes a time_t, but we should have
	//       until about 2038 to do that before we overflow a long.
	time_t curTime;
	time( &curTime );
			
	VCR_Event( VCREvent_Time );
	if ( g_VCRMode == VCR_Playback )
	{
		VCR_Read( &curTime, sizeof( curTime ) );
	}
	else if ( g_VCRMode == VCR_Record )
	{
		VCR_Write( &curTime, sizeof( curTime ) );
	}
	
	*today = (long)curTime;
}



void VCR_GenericString( const char *pEventName, const char *pString )
{
}


void VCR_GenericValueVerify( const tchar *pEventName, const void *pData, int maxLen )
{
}



// ---------------------------------------------------------------------- //
// The global VCR interface.
// ---------------------------------------------------------------------- //

VCR_t g_VCR =
{
	VCR_Start,
	VCR_End,
	VCR_GetVCRTraceInterface,
	VCR_GetMode,
	VCR_SetEnabled,
	VCR_SyncToken,
	VCR_Hook_Sys_FloatTime,
	VCR_Hook_PeekMessage,
	VCR_Hook_RecordGameMsg,
	VCR_Hook_RecordEndGameMsg,
	VCR_Hook_PlaybackGameMsg,
	VCR_Hook_recvfrom,
	VCR_Hook_GetCursorPos,
	VCR_Hook_ScreenToClient,
	VCR_Hook_Cmd_Exec,
	VCR_Hook_GetCommandLine,
	VCR_Hook_RegOpenKeyEx,
	VCR_Hook_RegSetValueEx,
	VCR_Hook_RegQueryValueEx,
	VCR_Hook_RegCreateKeyEx,
	VCR_Hook_RegCloseKey,
	VCR_Hook_GetNumberOfConsoleInputEvents,
	VCR_Hook_ReadConsoleInput,
	VCR_Hook_LocalTime,
	VCR_Hook_GetKeyState,
	VCR_Hook_recv,
        VCR_Hook_send,
        VCR_GenericRecord,
        VCR_GenericPlayback,
        VCR_GenericValue,

	VCR_GetPercentCompleted,
	VCR_CreateThread,
	VCR_WaitForSingleObject,
	VCR_EnterCriticalSection,
	VCR_Hook_Time,
	VCR_GenericString,
	VCR_GenericValueVerify,
	VCR_WaitForMultipleObjects,
	
};

VCR_t *g_pVCR = &g_VCR;



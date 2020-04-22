//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: VCR mode records a client's game and allows you to 
//			play it back and reproduce it exactly. When playing it back, nothing
//			is simulated on the server, but all server packets are recorded.
//
//			Most of the VCR mode functionality is accomplished through hooks
//			called at various points in the engine.
//
// $NoKeywords: $
//=============================================================================

#include "xbox/xbox_platform.h"
#include "xbox/xbox_win32stubs.h"
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "tier0/vcrmode.h"
#include "tier0/dbg.h"
												
#define VCRFILE_VERSION		2
#define VCR_RuntimeAssert(x)	VCR_RuntimeAssertFn(x, #x)
												
// ---------------------------------------------------------------------- //
// Internal functions.
// ---------------------------------------------------------------------- //

static void VCR_Error( const char *pFormat, ... )
{
}

static void VCR_RuntimeAssertFn(int bAssert, char const *pStr)
{
}

static void VCR_Read(void *pDest, int size)
{
}

template<class T>
static void VCR_ReadVal(T &val)
{
	VCR_Read(&val, sizeof(val));
}

static void VCR_Write(void const *pSrc, int size)
{
}

template<class T>
static void VCR_WriteVal(T &val)
{
}

// Hook from ExtendedTrace.cpp
bool g_bTraceRead = false;
void OutputDebugStringFormat( const char *pMsg, ... )
{
}

static VCREvent VCR_ReadEvent()
{
	return (VCREvent)-1;
}

static void VCR_WriteEvent(VCREvent event)
{
}

static void VCR_IncrementEvent()
{
}

static void VCR_Event(VCREvent type)
{
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
	return 0;
}

static void VCR_End()
{
}

static IVCRTrace* VCR_GetVCRTraceInterface()
{
	return 0;
}

static VCRMode VCR_GetMode()
{
	return VCR_Disabled;
}

static void VCR_SetEnabled(int bEnabled)
{
}

static void VCR_SyncToken(char const *pToken)
{
}

static double VCR_Hook_Sys_FloatTime(double time)
{
	return 0;
}

static int VCR_Hook_PeekMessage(
	struct tagMSG *msg, 
	void *hWnd, 
	unsigned int wMsgFilterMin, 
	unsigned int wMsgFilterMax, 
	unsigned int wRemoveMsg
	)
{
	return 0;
}

void VCR_Hook_RecordGameMsg( unsigned int uMsg, unsigned int wParam, long lParam )
{
}

void VCR_Hook_RecordEndGameMsg()
{
}

bool VCR_Hook_PlaybackGameMsg( unsigned int &uMsg, unsigned int &wParam, long &lParam )
{
	return 0;
}

static void VCR_Hook_GetCursorPos(struct tagPOINT *pt)
{
}

static void VCR_Hook_ScreenToClient(void *hWnd, struct tagPOINT *pt)
{
}

static int VCR_Hook_recvfrom(int s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
	return 0;
}

static int VCR_Hook_recv(int s, char *buf, int len, int flags)
{
	return 0;
}

static int VCR_Hook_send(int s, const char *buf, int len, int flags)
{
	return 0;
}

static void VCR_Hook_Cmd_Exec(char **f)
{
}

static char* VCR_Hook_GetCommandLine()
{
	return GetCommandLine();
}

static long VCR_Hook_RegOpenKeyEx( void *hKey, const char *lpSubKey, unsigned long ulOptions, unsigned long samDesired, void *pHKey )
{
	return 0;
}

static long VCR_Hook_RegSetValueEx(void *hKey, char const *lpValueName, unsigned long Reserved, unsigned long dwType, unsigned char const *lpData, unsigned long cbData)
{
	return 0;
}

static long VCR_Hook_RegQueryValueEx(void *hKey, char const *lpValueName, unsigned long *lpReserved, unsigned long *lpType, unsigned char *lpData, unsigned long *lpcbData)
{
	return 0;
}

static long VCR_Hook_RegCreateKeyEx(void *hKey, char const *lpSubKey, unsigned long Reserved, char *lpClass, unsigned long dwOptions, 
	unsigned long samDesired, void *lpSecurityAttributes, void *phkResult, unsigned long *lpdwDisposition)
{
	return 0;
}

static void VCR_Hook_RegCloseKey(void *hKey)
{
}

int VCR_Hook_GetNumberOfConsoleInputEvents( void *hInput, unsigned long *pNumEvents )
{
	return 0;
}

int	VCR_Hook_ReadConsoleInput( void *hInput, void *pRecs, int nMaxRecs, unsigned long *pNumRead )
{
	return 0;
}

void VCR_Hook_LocalTime( struct tm *today )
{
}

short VCR_Hook_GetKeyState( int nVirtKey )
{
	return 0;
}

void VCR_GenericRecord( const char *pEventName, const void *pData, int len )
{
}	

int VCR_GenericPlayback( const char *pEventName, void *pOutData, int maxLen, bool bForceSameLen )
{
	return 0;
}

void VCR_GenericValue( const char *pEventName, void *pData, int maxLen )
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
	VCR_GenericValue
};

VCR_t *g_pVCR = &g_VCR;



//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Xbox
//
//=====================================================================================//

#include "pch_tier0.h"
#include "xbox/xbox_console.h"
#include "xbox/xbox_win32stubs.h"
#include "xbox/xbox_launch.h"
#include "tier0/memdbgon.h"

#define XBX_MAX_EVENTS		32

xevent_t		g_xbx_eventQueue[XBX_MAX_EVENTS];
int				g_xbx_eventHead;
int				g_xbx_eventTail;
DWORD			g_iStorageDeviceId = XBX_INVALID_STORAGE_ID;
DWORD			g_iPrimaryUserId = XBX_INVALID_USER_ID;
DWORD			g_InvitedUserId = XBX_INVALID_USER_ID;
HANDLE			g_hListenHandle = INVALID_HANDLE_VALUE;
ULONG64			g_ListenCategories = 0;
XNKID			g_InviteSessionId = { 0, 0, 0, 0, 0, 0, 0, 0 };

//-----------------------------------------------------------------------------
//	Convert an Xbox notification to a custom windows message
//-----------------------------------------------------------------------------
static int NotificationToWindowsMessage( DWORD id )
{
	switch( id )
	{
	case XN_SYS_UI:							return WM_SYS_UI;						
	case XN_SYS_SIGNINCHANGED:				return WM_SYS_SIGNINCHANGED;				
	case XN_SYS_STORAGEDEVICESCHANGED:		return WM_SYS_STORAGEDEVICESCHANGED;		
	case XN_SYS_PROFILESETTINGCHANGED:		return WM_SYS_PROFILESETTINGCHANGED;		
	case XN_SYS_MUTELISTCHANGED:			return WM_SYS_MUTELISTCHANGED;				
	case XN_SYS_INPUTDEVICESCHANGED:		return WM_SYS_INPUTDEVICESCHANGED;			
	case XN_SYS_INPUTDEVICECONFIGCHANGED:	return WM_SYS_INPUTDEVICECONFIGCHANGED;		
	case XN_LIVE_CONNECTIONCHANGED:			return WM_LIVE_CONNECTIONCHANGED;			
	case XN_LIVE_INVITE_ACCEPTED:			return WM_LIVE_INVITE_ACCEPTED;				
	case XN_LIVE_LINK_STATE_CHANGED:		return WM_LIVE_LINK_STATE_CHANGED;			
	case XN_LIVE_CONTENT_INSTALLED:			return WM_LIVE_CONTENT_INSTALLED;			
	case XN_LIVE_MEMBERSHIP_PURCHASED:		return WM_LIVE_MEMBERSHIP_PURCHASED;		
	case XN_LIVE_VOICECHAT_AWAY:			return WM_LIVE_VOICECHAT_AWAY;				
	case XN_LIVE_PRESENCE_CHANGED:			return WM_LIVE_PRESENCE_CHANGED;			
	case XN_FRIENDS_PRESENCE_CHANGED:		return WM_FRIENDS_PRESENCE_CHANGED;			
	case XN_FRIENDS_FRIEND_ADDED:			return WM_FRIENDS_FRIEND_ADDED;				
	case XN_FRIENDS_FRIEND_REMOVED:			return WM_FRIENDS_FRIEND_REMOVED;			
	//deprecated in Jun08 XDK: case XN_CUSTOM_GAMEBANNERPRESSED:		return WM_CUSTOM_GAMEBANNERPRESSED;	
	case XN_CUSTOM_ACTIONPRESSED:			return WM_CUSTOM_ACTIONPRESSED;				
	case XN_XMP_STATECHANGED:				return WM_XMP_STATECHANGED;					
	case XN_XMP_PLAYBACKBEHAVIORCHANGED:	return WM_XMP_PLAYBACKBEHAVIORCHANGED;		
	case XN_XMP_PLAYBACKCONTROLLERCHANGED:	return WM_XMP_PLAYBACKCONTROLLERCHANGED;
	default:
		Warning( "Unrecognized notification id %d\n", id );
		return 0;
	}
}

//-----------------------------------------------------------------------------
//	XBX_Error
//
//-----------------------------------------------------------------------------
void XBX_Error( const char* format, ... )
{
	va_list args;
	char	message[XBX_MAX_MESSAGE];

	va_start( args, format );
	_vsnprintf( message, sizeof( message ), format, args );
	va_end( args );

	message[sizeof( message )-1] = '\0';

	XBX_DebugString( XMAKECOLOR(255,0,0), message );
	XBX_FlushDebugOutput();

	DebugBreak();

	static volatile int doReturn;

	while ( !doReturn );
}

//-----------------------------------------------------------------------------
//	XBX_OutputDebugStringA
//
//	Replaces 'OutputDebugString' to send through debugging channel
//-----------------------------------------------------------------------------
void XBX_OutputDebugStringA( LPCSTR lpOutputString )
{
	XBX_DebugString( XMAKECOLOR(0,0,0), lpOutputString );
}

//-----------------------------------------------------------------------------
//	XBX_GetEvent
//
//-----------------------------------------------------------------------------
static xevent_t* XBX_GetEvent(void)
{
	xevent_t* evPtr;

	if ( g_xbx_eventHead == g_xbx_eventTail )
	{
		// empty
		return NULL;
	}

	evPtr = &g_xbx_eventQueue[g_xbx_eventHead & (XBX_MAX_EVENTS-1)];

	// advance to next event
	g_xbx_eventHead++;

	return evPtr;
}

//-----------------------------------------------------------------------------
//	XBX_FlushEvents
//
//-----------------------------------------------------------------------------
static void XBX_FlushEvents(void)
{
	g_xbx_eventHead = 0;
	g_xbx_eventTail = 0;
}

//-----------------------------------------------------------------------------
//	XBX_ProcessXCommand
//
//-----------------------------------------------------------------------------
static void XBX_ProcessXCommand( const char *pCommand )
{
	// remote command
	// pass it game via windows message
	HWND hWnd = GetFocus();
	WNDPROC windowProc = ( WNDPROC)GetWindowLong( hWnd, GWL_WNDPROC );
	if ( windowProc )
	{
		windowProc( hWnd, WM_XREMOTECOMMAND, 0, (LPARAM)pCommand );
	}
}

//-----------------------------------------------------------------------------
//	XBX_ProcessListenerNotification
//
//-----------------------------------------------------------------------------
static void XBX_ProcessListenerNotification( int notification, int parameter )
{
	// pass it game via windows message
	HWND hWnd = GetFocus();
	WNDPROC windowProc = ( WNDPROC)GetWindowLong( hWnd, GWL_WNDPROC );
	if ( windowProc )
	{
		windowProc( hWnd, notification, 0, (LPARAM)parameter );
	}
}

//-----------------------------------------------------------------------------
//	XBX_QueueEvent
//
//-----------------------------------------------------------------------------
void XBX_QueueEvent(xevent_e event, int arg1, int arg2, int arg3)
{
	xevent_t*	evPtr;

	evPtr = &g_xbx_eventQueue[g_xbx_eventTail & (XBX_MAX_EVENTS-1)];
	evPtr->event = event;
	evPtr->arg1 = arg1;
	evPtr->arg2 = arg2;
	evPtr->arg3 = arg3;

	// next slot, queue never fills just overwrite older events
	g_xbx_eventTail++;
}

//-----------------------------------------------------------------------------
//	XBX_ProcessEvents
//
//	Assumed one per frame only!
//-----------------------------------------------------------------------------
void XBX_ProcessEvents(void)
{
	xevent_t	*evPtr;

	DWORD id;
	ULONG parameter;
 	while ( XNotifyGetNext( g_hListenHandle, 0, &id, &parameter ) )
 	{
 		// Special handling
 		switch( id )
 		{
 		case XN_SYS_STORAGEDEVICESCHANGED:
 			{
 				// Have we selected a storage device?
 				DWORD storageID = XBX_GetStorageDeviceId();
 				if ( storageID == XBX_INVALID_STORAGE_ID || storageID == XBX_STORAGE_DECLINED )
 					break;
 
 				// Validate the selected storage device
 				XDEVICE_DATA deviceData;
 				DWORD ret = XContentGetDeviceData( storageID, &deviceData );
 				if ( ret != ERROR_SUCCESS )
 				{
 					// Device was removed
 					XBX_SetStorageDeviceId( XBX_INVALID_STORAGE_ID );
 					XBX_QueueEvent( XEV_LISTENER_NOTIFICATION, NotificationToWindowsMessage( id ), 0, 0 );
 				}
 				break;
 			}
 
		default:
 			XBX_QueueEvent( XEV_LISTENER_NOTIFICATION, NotificationToWindowsMessage( id ), parameter, 0 );
 			break;
 		}
 	}

	// pump event queue
	while ( 1 )
	{
		evPtr = XBX_GetEvent();
		if ( !evPtr )
			break;

		switch ( evPtr->event )
		{
			case XEV_REMOTECMD:
				XBX_ProcessXCommand( (char *)evPtr->arg1 );
				// clear to mark as absorbed
				((char *)evPtr->arg1)[0] = '\0';
				break;

			case XEV_LISTENER_NOTIFICATION:
				XBX_ProcessListenerNotification( evPtr->arg1, evPtr->arg2 );
				break;
		}
	}
}

//-----------------------------------------------------------------------------
//	XBX_NotifyCreateListener
//
//	Add notification categories to the listener object
//-----------------------------------------------------------------------------
bool XBX_NotifyCreateListener( ULONG64 categories )
{
	if ( categories != 0 )
	{
		categories |= g_ListenCategories;
	}

	g_hListenHandle = XNotifyCreateListener( categories );
	if ( g_hListenHandle == NULL || g_hListenHandle == INVALID_HANDLE_VALUE )
	{
		return false;
	}

	g_ListenCategories = categories;
	return true;
}

//-----------------------------------------------------------------------------
//	XBX_GetLanguageString
//
//	Returns the xbox language setting as a string
//-----------------------------------------------------------------------------
const char* XBX_GetLanguageString( void )
{
	switch( XGetLanguage() )
	{
	case XC_LANGUAGE_FRENCH:
		return "french";
	case XC_LANGUAGE_GERMAN:
		return "german";
	}

	return "english";
}

bool XBX_IsLocalized( void )
{
	switch( XGetLanguage() )
	{
	case XC_LANGUAGE_FRENCH:
	case XC_LANGUAGE_GERMAN:
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
//	XBX_GetStorageDeviceId
//
//	Returns the xbox storage device ID
//-----------------------------------------------------------------------------
DWORD XBX_GetStorageDeviceId( void )
{
	return g_iStorageDeviceId;
}

//-----------------------------------------------------------------------------
//	XBX_SetStorageDeviceId
//
//	Sets the xbox storage device ID
//-----------------------------------------------------------------------------
void XBX_SetStorageDeviceId( DWORD id )
{
	g_iStorageDeviceId = id;
}

//-----------------------------------------------------------------------------
//	XBX_GetPrimaryUserId
//
//	Returns the active user ID
//-----------------------------------------------------------------------------
DWORD XBX_GetPrimaryUserId( void )
{
	return g_iPrimaryUserId;
}

//-----------------------------------------------------------------------------
//	XBX_SetPrimaryUserId
//
//	Sets the active user ID
//-----------------------------------------------------------------------------
void XBX_SetPrimaryUserId( DWORD idx )
{
	g_iPrimaryUserId = idx;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the stored session ID for a cross-game invite
//-----------------------------------------------------------------------------
XNKID XBX_GetInviteSessionId( void )
{
	return g_InviteSessionId;
}

//-----------------------------------------------------------------------------
// Purpose: Store a session ID for an invitation
//-----------------------------------------------------------------------------
void XBX_SetInviteSessionId( XNKID nSessionId )
{
	g_InviteSessionId = nSessionId;
}

//-----------------------------------------------------------------------------
// Purpose: Get the Id of the user who received an invite
//-----------------------------------------------------------------------------

DWORD XBX_GetInvitedUserId( void )
{
	return g_InvitedUserId;
}

//-----------------------------------------------------------------------------
// Purpose: Set the Id of the user who received an invite
//-----------------------------------------------------------------------------
void XBX_SetInvitedUserId( DWORD nUserId )
{
	g_InvitedUserId = nUserId;
}

static CXboxLaunch g_XBoxLaunch;
CXboxLaunch *XboxLaunch()
{
	return &g_XBoxLaunch;
}

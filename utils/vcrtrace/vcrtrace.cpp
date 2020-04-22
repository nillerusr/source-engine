//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// vcrtrace.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <time.h>
#include "tier0/vcrmode.h"
#include "tier1/utlvector.h"



IVCRTrace *g_pTrace = 0;


class CVCRHelpers : public IVCRHelpers
{
public:
	virtual void	ErrorMessage( const char *pMsg )
	{
		Msg("ERROR: %s\n", pMsg);
	}

	virtual void*	GetMainWindow()
	{
		return 0;
	}
};

static CVCRHelpers g_VCRHelpers;


void ErrFunction( char const *pMsg )
{
	Msg( "%s", pMsg );
}


typedef struct
{
	int			m_Message;
	char const	*m_pName;
} WMessage;
#define DEFINE_WMESSAGE(x) x, #x


// Message table for windows messages.
WMessage g_WMessages[] =
{
	DEFINE_WMESSAGE(WM_NOTIFY),
	DEFINE_WMESSAGE(WM_INPUTLANGCHANGEREQUEST),
	DEFINE_WMESSAGE(WM_INPUTLANGCHANGE),
	DEFINE_WMESSAGE(WM_TCARD),
	DEFINE_WMESSAGE(WM_HELP),
	DEFINE_WMESSAGE(WM_USERCHANGED),
	DEFINE_WMESSAGE(WM_NOTIFYFORMAT),
	DEFINE_WMESSAGE(NFR_ANSI),
	DEFINE_WMESSAGE(NFR_UNICODE),
	DEFINE_WMESSAGE(NF_QUERY),
	DEFINE_WMESSAGE(NF_REQUERY),
	DEFINE_WMESSAGE(WM_CONTEXTMENU),
	DEFINE_WMESSAGE(WM_STYLECHANGING),
	DEFINE_WMESSAGE(WM_STYLECHANGED),
	DEFINE_WMESSAGE(WM_DISPLAYCHANGE),
	DEFINE_WMESSAGE(WM_GETICON),
	DEFINE_WMESSAGE(WM_SETICON),
	DEFINE_WMESSAGE(WM_NCCREATE),
	DEFINE_WMESSAGE(WM_NCDESTROY),
	DEFINE_WMESSAGE(WM_NCCALCSIZE),
	DEFINE_WMESSAGE(WM_NCHITTEST),
	DEFINE_WMESSAGE(WM_NCPAINT),
	DEFINE_WMESSAGE(WM_NCACTIVATE),
	DEFINE_WMESSAGE(WM_GETDLGCODE),
	DEFINE_WMESSAGE(WM_SYNCPAINT),
	DEFINE_WMESSAGE(WM_NCMOUSEMOVE),
	DEFINE_WMESSAGE(WM_NCLBUTTONDOWN),
	DEFINE_WMESSAGE(WM_NCLBUTTONUP),
	DEFINE_WMESSAGE(WM_NCLBUTTONDBLCLK),
	DEFINE_WMESSAGE(WM_NCRBUTTONDOWN),
	DEFINE_WMESSAGE(WM_NCRBUTTONUP),
	DEFINE_WMESSAGE(WM_NCRBUTTONDBLCLK),
	DEFINE_WMESSAGE(WM_NCMBUTTONDOWN),
	DEFINE_WMESSAGE(WM_NCMBUTTONUP),
	DEFINE_WMESSAGE(WM_NCMBUTTONDBLCLK),
	DEFINE_WMESSAGE(WM_KEYFIRST),
	DEFINE_WMESSAGE(WM_KEYDOWN),
	DEFINE_WMESSAGE(WM_KEYUP),
	DEFINE_WMESSAGE(WM_CHAR),
	DEFINE_WMESSAGE(WM_DEADCHAR),
	DEFINE_WMESSAGE(WM_SYSKEYDOWN),
	DEFINE_WMESSAGE(WM_SYSKEYUP),
	DEFINE_WMESSAGE(WM_SYSCHAR),
	DEFINE_WMESSAGE(WM_SYSDEADCHAR),
	DEFINE_WMESSAGE(WM_KEYLAST),
	DEFINE_WMESSAGE(WM_IME_STARTCOMPOSITION),
	DEFINE_WMESSAGE(WM_IME_ENDCOMPOSITION),
	DEFINE_WMESSAGE(WM_IME_COMPOSITION),
	DEFINE_WMESSAGE(WM_IME_KEYLAST),
	DEFINE_WMESSAGE(WM_INITDIALOG),
	DEFINE_WMESSAGE(WM_COMMAND),
	DEFINE_WMESSAGE(WM_SYSCOMMAND),
	DEFINE_WMESSAGE(WM_TIMER),
	DEFINE_WMESSAGE(WM_HSCROLL),
	DEFINE_WMESSAGE(WM_VSCROLL),
	DEFINE_WMESSAGE(WM_INITMENU),
	DEFINE_WMESSAGE(WM_INITMENUPOPUP),
	DEFINE_WMESSAGE(WM_MENUSELECT),
	DEFINE_WMESSAGE(WM_MENUCHAR),
	DEFINE_WMESSAGE(WM_ENTERIDLE),
	DEFINE_WMESSAGE(WM_CTLCOLORMSGBOX),
	DEFINE_WMESSAGE(WM_CTLCOLOREDIT),
	DEFINE_WMESSAGE(WM_CTLCOLORLISTBOX),
	DEFINE_WMESSAGE(WM_CTLCOLORBTN),
	DEFINE_WMESSAGE(WM_CTLCOLORDLG),
	DEFINE_WMESSAGE(WM_CTLCOLORSCROLLBAR),
	DEFINE_WMESSAGE(WM_CTLCOLORSTATIC),
	DEFINE_WMESSAGE(WM_MOUSEMOVE),
	DEFINE_WMESSAGE(WM_LBUTTONDOWN),
	DEFINE_WMESSAGE(WM_LBUTTONUP),
	DEFINE_WMESSAGE(WM_LBUTTONDBLCLK),
	DEFINE_WMESSAGE(WM_RBUTTONDOWN),
	DEFINE_WMESSAGE(WM_RBUTTONUP),
	DEFINE_WMESSAGE(WM_RBUTTONDBLCLK),
	DEFINE_WMESSAGE(WM_MBUTTONDOWN),
	DEFINE_WMESSAGE(WM_MBUTTONUP),
	DEFINE_WMESSAGE(WM_MBUTTONDBLCLK),
	DEFINE_WMESSAGE(WM_MOUSELAST),
	DEFINE_WMESSAGE(WM_PARENTNOTIFY),
	DEFINE_WMESSAGE(WM_ENTERMENULOOP),
	DEFINE_WMESSAGE(WM_EXITMENULOOP                 )
};

// Message table for winsock messages.
WMessage g_WinsockMessages[] = 
{
	DEFINE_WMESSAGE(WSAEWOULDBLOCK),
	DEFINE_WMESSAGE(WSAEINPROGRESS),
	DEFINE_WMESSAGE(WSAEALREADY),
	DEFINE_WMESSAGE(WSAENOTSOCK),
	DEFINE_WMESSAGE(WSAEDESTADDRREQ),
	DEFINE_WMESSAGE(WSAEMSGSIZE),
	DEFINE_WMESSAGE(WSAEPROTOTYPE),
	DEFINE_WMESSAGE(WSAENOPROTOOPT),
	DEFINE_WMESSAGE(WSAEPROTONOSUPPORT),
	DEFINE_WMESSAGE(WSAESOCKTNOSUPPORT),
	DEFINE_WMESSAGE(WSAEOPNOTSUPP),
	DEFINE_WMESSAGE(WSAEPFNOSUPPORT),
	DEFINE_WMESSAGE(WSAEAFNOSUPPORT),
	DEFINE_WMESSAGE(WSAEADDRINUSE),
	DEFINE_WMESSAGE(WSAEADDRNOTAVAIL),
	DEFINE_WMESSAGE(WSAENETDOWN),
	DEFINE_WMESSAGE(WSAENETUNREACH),
	DEFINE_WMESSAGE(WSAENETRESET),
	DEFINE_WMESSAGE(WSAECONNABORTED),
	DEFINE_WMESSAGE(WSAECONNRESET),
	DEFINE_WMESSAGE(WSAENOBUFS),
	DEFINE_WMESSAGE(WSAEISCONN),
	DEFINE_WMESSAGE(WSAENOTCONN),
	DEFINE_WMESSAGE(WSAESHUTDOWN),
	DEFINE_WMESSAGE(WSAETOOMANYREFS),
	DEFINE_WMESSAGE(WSAETIMEDOUT),
	DEFINE_WMESSAGE(WSAECONNREFUSED),
	DEFINE_WMESSAGE(WSAELOOP),
	DEFINE_WMESSAGE(WSAENAMETOOLONG),
	DEFINE_WMESSAGE(WSAEHOSTDOWN),
	DEFINE_WMESSAGE(WSAEHOSTUNREACH),
	DEFINE_WMESSAGE(WSAENOTEMPTY),
	DEFINE_WMESSAGE(WSAEPROCLIM),
	DEFINE_WMESSAGE(WSAEUSERS),
	DEFINE_WMESSAGE(WSAEDQUOT),
	DEFINE_WMESSAGE(WSAESTALE),
	DEFINE_WMESSAGE(WSAEREMOTE),
	DEFINE_WMESSAGE(WSAEDISCON),
	DEFINE_WMESSAGE(WSASYSNOTREADY),
	DEFINE_WMESSAGE(WSAVERNOTSUPPORTED),
	DEFINE_WMESSAGE(WSANOTINITIALISED)
};


static char const* GetWMessageName(WMessage *pMessages, int nMessages, int msg)
{
	static char str[128];
	int i;

	for(i=0; i < nMessages; i++)
	{
		if(pMessages[i].m_Message == msg)
			return pMessages[i].m_pName;
	}

	sprintf(str, "%d", msg);
	return str;
}

FILE *g_fpTrace = NULL;
bool g_bEndOfFile = false;

template<class T>
static void VCRTrace_ReadVal(T &val)
{
	if ( fread( &val, 1, sizeof( val ), g_fpTrace ) != sizeof( val ) )
		g_bEndOfFile = true;
}

static void VCRTrace_Read( void *pOut, int size )
{
	if ( fread( pOut, 1, size, g_fpTrace ) != (size_t)size )
		g_bEndOfFile = true;
}

void ReadAndPrintShortString( const char *pFormat )
{
	unsigned short len;
	VCRTrace_ReadVal( len );

	static CUtlVector<char> tempData;
	if ( tempData.Count() < len )
		tempData.SetSize( len );

	VCRTrace_Read( tempData.Base(), len );
	if ( len > 0 && ( tempData[len-2] == '\n' || tempData[len-2] == '\r' ) )
		tempData[len-2] = 0;

	Msg( pFormat, tempData.Base() );
}

static void VCR_TraceEvents()
{
	int iEvent = 0;

	g_pTrace = g_pVCR->GetVCRTraceInterface();
	
	while ( !g_bEndOfFile )
	{
		++iEvent;

		unsigned char eventCode;
		VCRTrace_Read( &eventCode, sizeof( eventCode ) );

		unsigned short iThread = 0;
		if ( eventCode & 0x80 )
		{
			VCRTrace_Read( &iThread, sizeof( iThread ) );
			eventCode &= ~0x80;
		}

		Msg( "%5d (thread %d): ", iEvent, iThread );

		VCREvent event = (VCREvent)eventCode;
		switch(event)
		{
			case VCREvent_Sys_FloatTime:
			{
				double ret;
				VCRTrace_Read(&ret, sizeof(ret));
				Msg("Sys_FloatTime: %f\n", ret);
			}
			break;
			
			case VCREvent_recvfrom:
			{
				int ret;
				char buf[8192];
				
				VCRTrace_Read(&ret, sizeof(ret));

				Assert(ret < (int)sizeof(buf));
				if(ret == SOCKET_ERROR)
				{
					int err;
					VCRTrace_ReadVal(err);

					Msg("recvfrom: %d (error %s)\n", ret, GetWMessageName(g_WinsockMessages, sizeof(g_WinsockMessages)/sizeof(g_WinsockMessages[0]), err));
				}
				else
				{
					Msg("recvfrom: %d\n", ret);
					VCRTrace_Read(buf, ret);

					char bFrom;
					VCRTrace_ReadVal( bFrom );
					if ( bFrom )
					{
						sockaddr_in from;
						VCRTrace_Read( &from, sizeof( from ) );
					}
				}
			}
			break;

			case VCREvent_SyncToken:
			{
				char len;
				char buf[256];

				VCRTrace_Read(&len, 1);
				VCRTrace_Read(buf, len);
				buf[len] = 0;

				Msg("SyncToken: %s\n", buf);
			}
			break;

			case VCREvent_GetCursorPos:
			{
				POINT pt;
				VCRTrace_ReadVal(pt);
				Msg("GetCursorPos: (%d, %d)\n", pt.x, pt.y);
			}
			break;

			case VCREvent_SetCursorPos:
			{
				int x, y;
				VCRTrace_Read(&x, sizeof(x));
				VCRTrace_Read(&y, sizeof(y));
				Msg("SetCursorPos: (%d, %d)\n", x, y);
			}
			break;

			case VCREvent_ScreenToClient:
			{
				POINT pt;
				VCRTrace_ReadVal(pt);
				Msg("ScreenToClient: (%d, %d)\n", pt.x, pt.y);
			}
			break;

			case VCREvent_Cmd_Exec:
			{
				int len;
				char *f;

				VCRTrace_Read(&len, sizeof(len));
				
				if(len != -1)
				{
					f = (char*)malloc(len);
					VCRTrace_Read(f, len);
				}
				
				Msg("Cmd_Exec: %d\n", len);
			}
			break;

			case VCREvent_CmdLine:
			{
				int len;
				char str[8192];

				VCRTrace_Read(&len, sizeof(len));
				Assert(len < sizeof(str));
				VCRTrace_Read(str, len);

				Msg("CmdLine: %s\n", str);
			}
			break;

			case VCREvent_RegOpenKeyEx:
			{
				long ret;
				VCRTrace_ReadVal(ret);
				Msg("RegOpenKeyEx: %d\n", ret);
			}
			break;

			case VCREvent_RegSetValueEx:
			{
				long ret;
				VCRTrace_ReadVal(ret);
				Msg("RegSetValueEx: %d\n", ret);
			}
			break;

			case VCREvent_RegQueryValueEx:
			{
				long ret;
				unsigned long type, cbData;
				char dummy;

				VCRTrace_ReadVal(ret);
				VCRTrace_ReadVal(type);
				VCRTrace_ReadVal(cbData);
				while(cbData)
				{
					VCRTrace_ReadVal(dummy);
					cbData--;
				}

				Msg("RegQueryValueEx\n");
			}
			break;

			case VCREvent_RegCreateKeyEx:
			{
				long ret;
				VCRTrace_ReadVal(ret);
				Msg("RegCreateKeyEx: %d\n", ret);
			}
			break;

			case VCREvent_RegCloseKey:
			{
				Msg("VCREvent_RegCloseKey\n");
			}
			break;
			
			case VCREvent_PeekMessage:
			{
				MSG msg;
				int valid;

				VCRTrace_Read(&valid, sizeof(valid));
				if(valid)
				{
					VCRTrace_Read(&msg, sizeof(MSG));
				}

				Msg("PeekMessage - msg: %s, wParam: %x, lParam: %x\n", GetWMessageName(g_WMessages, sizeof(g_WMessages)/sizeof(g_WMessages[0]), msg.message), msg.wParam, msg.lParam);
			}
			break;

			case VCREvent_GameMsg:
			{
				char valid;
				VCRTrace_Read( &valid, sizeof(valid) );

				if ( valid )
				{
					UINT uMsg;
					WPARAM wParam;
					LPARAM lParam;
					VCRTrace_Read( &uMsg, sizeof(uMsg) );
					VCRTrace_Read( &wParam, sizeof(wParam) );
					VCRTrace_Read( &lParam, sizeof(lParam) );
					Msg( "GameMsg - msg: %s, wParam: %x, lParam: %x\n", GetWMessageName(g_WMessages, sizeof(g_WMessages)/sizeof(g_WMessages[0]), uMsg), wParam, lParam );
				}
				else
				{
					Msg("GameMsg - <end>\n" );
				}
			}
			break;

			case VCREvent_GetNumberOfConsoleInputEvents:
			{
				char val;
				unsigned long nEvents;
				
				VCRTrace_ReadVal( val );
				VCRTrace_ReadVal( nEvents );

				Msg( "GetNumberOfConsoleInputEvents (returned %d, nEvents = %d)\n", val, nEvents );
			}
			break;

			case VCREvent_ReadConsoleInput:
			{
				char val;
				unsigned long nRead;
				INPUT_RECORD recs[1024];

				VCRTrace_ReadVal( val );
				if ( val )
				{
					VCRTrace_ReadVal( nRead );
					VCRTrace_Read( recs, nRead * sizeof( INPUT_RECORD ) );
				}
				else
				{
					nRead = 0;
				}

				Msg( "ReadConsoleInput (returned %d, nRead = %d)\n", val, nRead );
			}
			break;

			case VCREvent_GetKeyState:
			{
				short ret;
				VCRTrace_ReadVal( ret );
				Msg( "VCREvent_GetKeyState: %d\n", ret );
			}
			break;

			case VCREvent_recv:
			{
				int ret;
				
				// Get the result from our file.
				VCRTrace_ReadVal( ret );
				if ( ret == SOCKET_ERROR )
				{
					int err;
					VCRTrace_ReadVal( err );
					Msg( "VCREvent_recv - SOCKET_ERROR - %d\n", err );
				}
				else
				{
					CUtlVector<char> dummyData;
					dummyData.SetSize( ret );
					VCRTrace_Read( dummyData.Base(), ret );
					Msg( "VCREvent_recv - size %d", ret );
				}
			}
			break;

			case VCREvent_send:
			{
				int ret;

				// Get the result from our file.
				VCRTrace_ReadVal( ret );										   
				if ( ret == SOCKET_ERROR )
				{
					int err;
					VCRTrace_ReadVal(err);
					Msg( "VCREvent_send - SOCKET_ERROR - %d\n", err );
				}
				else
				{
					Msg( "VCREvent_send - %d\n", ret );
				}
			}
			break;

			case VCREvent_Generic:
			{
				unsigned char nameLen;
				VCRTrace_ReadVal( nameLen );
				char testName[512] = "(none)";

				if ( nameLen != 255 )
				{
					VCRTrace_Read( testName, nameLen );
				}

				int dataLen;
				VCRTrace_ReadVal( dataLen );

				CUtlVector<char> tempData;
				tempData.SetSize( dataLen );
				VCRTrace_Read( tempData.Base(), dataLen );

				Msg( "VCREvent_Generic (name: %s, len: %d)\n", testName, dataLen );
			}
			break;

			case VCREvent_CreateThread:
			{
				Msg( "VCREvent_CreateThread\n" );
			}
			break;

			case VCREvent_WaitForSingleObject:
			{
				char val;
				VCRTrace_ReadVal( val );
				
				Msg( "VCREvent_WaitForSingleObject " );
				if ( val == 1 )
					Msg( "(WAIT_OBJECT_0)\n" );
				else if ( val == 2 )
					Msg( "(WAIT_ABANDONED)\n" );
				else
					Msg( "(WAIT_TIMEOUT)\n" );
			}
			break;

			case VCREvent_EnterCriticalSection:
			{
				Msg( "VCREvent_EnterCriticalSection\n" );
			}
			break;

			case VCREvent_LocalTime:
			{
				tm today;
				VCRTrace_ReadVal( today );
				Msg( "VCREvent_LocalTime\n" );
			}
			break;

			case VCREvent_Time:
			{
				long today;
				VCRTrace_ReadVal( today );
				Msg( "VCREvent_Time\n" );
			}
			break;

			case VCREvent_GenericString:
			{
				Msg( "VCREvent_GenericString: " );

				ReadAndPrintShortString( "[%s]  " );
				ReadAndPrintShortString( "%s" );
				Msg( "\n" );
			}
			break;

			default:
			{
				Msg( "***ERROR*** VCR_TraceEvent: invalid event" );
				return;
			}
		}
	}
}


SpewRetval_t MySpewOutput( SpewType_t spewType, tchar const *pMsg )
{
	printf( "%s", pMsg );
	OutputDebugString( pMsg );

	if ( spewType == SPEW_ASSERT )
		return SPEW_DEBUGGER;
	
	else if ( spewType == SPEW_ERROR )
		return SPEW_ABORT;
	
	else
		return SPEW_CONTINUE;
}


int main(int argc, char* argv[])
{
	if(argc <= 1)
	{
		Msg("vcrtrace <vcr filename>\n");
		return 1;
	}

	SpewOutputFunc( MySpewOutput );

	g_fpTrace = fopen( argv[1], "rb" );
	if ( !g_fpTrace )
	{
		Msg("Invalid or mission VCR file '%s'\n", argv[1]);
		return 1;
	}

	unsigned long version;
	VCRTrace_ReadVal( version );
	if ( version != VCRFILE_VERSION )
	{
		Msg( "Invalid VCR file version (is %d, but we need %d).\n", version, VCRFILE_VERSION );
		return 1;
	}

	VCR_TraceEvents();
	return 0;
}


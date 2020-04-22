//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Material editor
//=============================================================================

#include <windows.h>
#include "appframework/appframework.h"
#include "networksystem/inetworksystem.h"
#include "networksystem/inetworkmessage.h"
#include "bitbuf.h"
#include "filesystem.h"
#include "filesystem_init.h"
#include "tier0/icommandline.h"
#include "vstdlib/cvar.h"


//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------
IFileSystem *g_pFileSystem;
INetworkSystem *g_pNetworkSystem;


//-----------------------------------------------------------------------------
// Purpose: Warning/Msg call back through this API
// Input  : type - 
//			*pMsg - 
// Output : SpewRetval_t
//-----------------------------------------------------------------------------
SpewRetval_t SpewFunc( SpewType_t type, char const *pMsg )
{	
	OutputDebugString( pMsg );
	if ( type == SPEW_ASSERT )
	{
		DebuggerBreak();
	}
	return SPEW_CONTINUE;
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CNetworkTestApp : public CSteamAppSystemGroup
{
	typedef CSteamAppSystemGroup BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void PostShutdown( );
	virtual void Destroy();
	virtual const char *GetAppName() { return "NetworkTest"; }
	virtual bool AppUsesReadPixels() { return false; }

private:
	// Window management
	bool CreateAppWindow( char const *pTitle, bool bWindowed, int w, int h );

	// Sets up the game path
	bool SetupSearchPaths();

	HWND m_HWnd;
};

DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CNetworkTestApp );


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CNetworkTestApp::Create()
{
	SpewOutputFunc( SpewFunc );

	// Add in the cvar factory
	AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
	AddSystem( cvarModule, CVAR_INTERFACE_VERSION );
	
	AppSystemInfo_t appSystems[] = 
	{
		{ "networksystem.dll",		NETWORKSYSTEM_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	if ( !AddSystems( appSystems ) ) 
		return false;

	g_pFileSystem = (IFileSystem*)FindSystem( FILESYSTEM_INTERFACE_VERSION );
	g_pNetworkSystem = (INetworkSystem*)FindSystem( NETWORKSYSTEM_INTERFACE_VERSION );

	if (!g_pFileSystem || !g_pNetworkSystem )
		return false;

	return true;
}

void CNetworkTestApp::Destroy()
{
	g_pFileSystem = NULL;
	g_pNetworkSystem = NULL;
}


//-----------------------------------------------------------------------------
// Window management
//-----------------------------------------------------------------------------
bool CNetworkTestApp::CreateAppWindow( char const *pTitle, bool bWindowed, int w, int h )
{
	WNDCLASSEX		wc;
	memset( &wc, 0, sizeof( wc ) );
	wc.cbSize		 = sizeof( wc );
    wc.style         = CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc   = DefWindowProc;
    wc.hInstance     = (HINSTANCE)GetAppInstance();
    wc.lpszClassName = "Valve001";
	wc.hIcon		 = NULL; //LoadIcon( s_HInstance, MAKEINTRESOURCE( IDI_LAUNCHER ) );
	wc.hIconSm		 = wc.hIcon;

    RegisterClassEx( &wc );

	// Note, it's hidden
	DWORD style = WS_POPUP | WS_CLIPSIBLINGS;
	
	if ( bWindowed )
	{
		// Give it a frame
		style |= WS_OVERLAPPEDWINDOW;
		style &= ~WS_THICKFRAME;
	}

	// Never a max box
	style &= ~WS_MAXIMIZEBOX;

	RECT windowRect;
	windowRect.top		= 0;
	windowRect.left		= 0;
	windowRect.right	= w;
	windowRect.bottom	= h;

	// Compute rect needed for that size client area based on window style
	AdjustWindowRectEx(&windowRect, style, FALSE, 0);

	// Create the window
	m_HWnd = CreateWindow( wc.lpszClassName, pTitle, style, 0, 0, 
		windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, 
		NULL, NULL, (HINSTANCE)GetAppInstance(), NULL );

	if (!m_HWnd)
		return false;

    int     CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;

	// In VCR modes, keep it in the upper left so mouse coordinates are always relative to the window.
	SetWindowPos (m_HWnd, NULL, CenterX, CenterY, 0, 0,
				  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);

	return true;
}


//-----------------------------------------------------------------------------
// Sets up the game path
//-----------------------------------------------------------------------------
bool CNetworkTestApp::SetupSearchPaths()
{
	CFSSteamSetupInfo steamInfo;
	steamInfo.m_pDirectoryName = NULL;
	steamInfo.m_bOnlyUseDirectoryName = false;
	steamInfo.m_bToolsMode = true;
	steamInfo.m_bSetSteamDLLPath = true;
	steamInfo.m_bSteam = g_pFileSystem->IsSteam();
	if ( FileSystem_SetupSteamEnvironment( steamInfo ) != FS_OK )
		return false;

	CFSMountContentInfo fsInfo;
	fsInfo.m_pFileSystem = g_pFileSystem;
	fsInfo.m_bToolsMode = true;
	fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;

	if ( FileSystem_MountContent( fsInfo ) != FS_OK )
		return false;

	// Finally, load the search paths for the "GAME" path.
	CFSSearchPathsInit searchPathsInit;
	searchPathsInit.m_pDirectoryName = steamInfo.m_GameInfoPath;
	searchPathsInit.m_pFileSystem = g_pFileSystem;
	if ( FileSystem_LoadSearchPaths( searchPathsInit ) != FS_OK )
		return false;

	g_pFileSystem->AddSearchPath( steamInfo.m_GameInfoPath, "SKIN", PATH_ADD_TO_HEAD );

	char platform[MAX_PATH];
	Q_strncpy( platform, steamInfo.m_GameInfoPath, MAX_PATH );
	Q_StripTrailingSlash( platform );
	Q_strncat( platform, "/../platform", MAX_PATH, MAX_PATH );

	g_pFileSystem->AddSearchPath( platform, "PLATFORM" );

	return true;
}


//-----------------------------------------------------------------------------
// PreInit, PostShutdown
//-----------------------------------------------------------------------------
bool CNetworkTestApp::PreInit( )
{
	// Add paths...
	if ( !SetupSearchPaths() )
		return false;

	const char *pArg;
	int iWidth = 1024;
	int iHeight = 768;
	bool bWindowed = (CommandLine()->CheckParm( "-fullscreen" ) == NULL);
	if (CommandLine()->CheckParm( "-width", &pArg ))
	{
		iWidth = atoi( pArg );
	}
	if (CommandLine()->CheckParm( "-height", &pArg ))
	{
		iHeight = atoi( pArg );
	}

	if (!CreateAppWindow( "NetworkTest", bWindowed, iWidth, iHeight ))
		return false;

	return true;
}

void CNetworkTestApp::PostShutdown( )
{
}


//-----------------------------------------------------------------------------
// Network message ids
//-----------------------------------------------------------------------------
enum
{
	TEST_GROUP = NETWORKSYSTEM_FIRST_GROUP,
};

enum
{
	TEST_MESSAGE_1 = 0,
};



//-----------------------------------------------------------------------------
// Test network message
//-----------------------------------------------------------------------------
class CTestNetworkMessage : public CNetworkMessage
{
public:
	CTestNetworkMessage() { SetReliable( false ); }
	CTestNetworkMessage( int nValue ) : m_Data( nValue ) { SetReliable( false ); }

	DECLARE_BASE_MESSAGE( TEST_GROUP, TEST_MESSAGE_1, "Test Message 1" )

	bool Process();

	int m_Data;
};

bool CTestNetworkMessage::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteShort( m_Data );
	return !buffer.IsOverflowed();
}

bool CTestNetworkMessage::ReadFromBuffer( bf_read &buffer )
{
	m_Data = buffer.ReadShort();
	return !buffer.IsOverflowed();
}

bool CTestNetworkMessage::Process()
{
	Msg( "Received test message %d\n", m_Data );
	return true;
}


//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CNetworkTestApp::Main()
{
	// Network messages must be registered before the server or client is started
	g_pNetworkSystem->RegisterMessage( new CTestNetworkMessage() );

	int nRetVal = 0;
	if ( !g_pNetworkSystem->StartServer( ) )
		return 0;

	if ( !g_pNetworkSystem->StartClient( ) )
		goto shutdownServer;

	// Set the channel up for receiving
	INetChannel *pChan = g_pNetworkSystem->ConnectClientToServer( "localhost", 27001 );
	if ( !pChan )
		goto shutdownClient;

	INetChannel *pServerChan = NULL;

	{
		while( true )
		{
			// Helps avoid a buffer overflow
			Sleep( 1 );

			// Send a message out 
			if ( pChan->GetConnectionState() == CONNECTION_STATE_CONNECTED )
			{
				CTestNetworkMessage msg( 5 );
				pChan->AddNetMsg( &msg, false );
				msg.m_Data = 4;
 				pChan->AddNetMsg( &msg, false );
			}

			if ( pServerChan )
			{
				CTestNetworkMessage msg( 6 );
				pServerChan->AddNetMsg( &msg, false );
				msg.m_Data = 7;
 				pServerChan->AddNetMsg( &msg, false );
			}

			g_pNetworkSystem->ClientSendMessages();
			g_pNetworkSystem->ServerReceiveMessages();
			g_pNetworkSystem->ServerSendMessages();
			g_pNetworkSystem->ClientReceiveMessages();

			NetworkEvent_t *pEvent = g_pNetworkSystem->FirstNetworkEvent();
			for ( ; pEvent; pEvent = g_pNetworkSystem->NextNetworkEvent( ) )
			{
				switch ( pEvent->m_nType )
				{
				case NETWORK_EVENT_CONNECTED:
					pServerChan = ((NetworkConnectionEvent_t*)pEvent)->m_pChannel;
					break;

				case NETWORK_EVENT_DISCONNECTED:
					if ( pServerChan == ((NetworkDisconnectionEvent_t*)pEvent)->m_pChannel )
					{
						pServerChan = NULL;
					}
					break;

				case NETWORK_EVENT_MESSAGE_RECEIVED:
					{
						NetworkMessageReceivedEvent_t *pReceivedEvent = static_cast<NetworkMessageReceivedEvent_t*>( pEvent );
						if ( ( pReceivedEvent->m_pNetworkMessage->GetGroup() == TEST_GROUP ) && ( pReceivedEvent->m_pNetworkMessage->GetType() == TEST_MESSAGE_1 ) )
						{
							static_cast<CTestNetworkMessage*>( pReceivedEvent->m_pNetworkMessage )->Process();
						}
					}
					break;
				}
			}
		}
		nRetVal = 1;

		g_pNetworkSystem->DisconnectClientFromServer( pChan );
	}

shutdownClient:
	g_pNetworkSystem->ShutdownClient( );

shutdownServer:
	g_pNetworkSystem->ShutdownServer( );

	return nRetVal;
}




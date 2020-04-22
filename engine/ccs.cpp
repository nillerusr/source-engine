//========= Copyright Valve Corporation, All rights reserved. ============//
// ccs.cpp - "con command server" 
// Not to be confused with the game's server - this is a completely different thing that allows multiple game clients to connect to a single client, 
// so con commands can be sent simultaneously to multiple running game clients, screen captures can be sent in real-time to a single client for comparison purposes, 
// and all convar's can be diff'd between all connected clients and the server.
// The server portion of this code needs socketlib, which hasn't been ported to Linux yet, and shouldn't be shipped because it could be a security risk 
// (it's only intended for primarily graphics debugging and detailed comparisons of GL vs. D3D9).

#include "host_state.h"

//#define CON_COMMAND_SERVER_SUPPORT

#include "tier2/tier2.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"

#ifdef CON_COMMAND_SERVER_SUPPORT
#include <algorithm>
#include "socketlib/socketlib.h"
#define MINIZ_NO_ARCHIVE_APIS
#include "../../thirdparty/miniz/miniz.c"
#include "../../thirdparty/miniz/simple_bitmap.h"
#define STBI_NO_STDIO
#include "../../thirdparty/stb_image/stb_image.c"
#include "ivideomode.h"
#endif

#include "cmd.h"
#include "filesystem.h"
#include "render.h"
#include "icliententitylist.h"
#include "client.h"
#include "icliententity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
#ifdef STAGING_ONLY
CON_COMMAND_F( ccs_write_convars, "Write all convars to file.", FCVAR_CHEAT )
{
	FILE* pFile = fopen( "convars.txt", "w" );
	if ( !pFile )
	{
		ConMsg( "Unable to open convars.txt\n" );
		return;
	}

	ICvar::Iterator iter( g_pCVar );
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase *var = iter.Get();

		ConVar *pConVar = dynamic_cast< ConVar* >( var );
		if ( !pConVar )
			continue;

		const char *pName = pConVar->GetName();
		const char *pVal = pConVar->GetString();
		if ( ( !pName ) || ( !pVal ) )
			continue;

		fprintf( pFile, "%s=%s\n", pName, pVal );
	}
	fclose( pFile );
	ConMsg( "Wrote all convars to convars.txt\n" );
}
#endif

//-----------------------------------------------------------------------------

#ifdef CON_COMMAND_SERVER_SUPPORT

class frame_buf_window
{
	friend LRESULT CALLBACK static_window_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	static LRESULT CALLBACK static_window_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		frame_buf_window* pApp = reinterpret_cast<frame_buf_window*>(GetWindowLong(hWnd, 0));
		if (!pApp)
			pApp = frame_buf_window::m_pCur_app;

		Assert(pApp);

		return pApp->window_proc(hWnd, message, wParam, lParam);
	}

public:
	frame_buf_window(const char* pTitle = "frame_buf_window", int width = 640, int height = 480, int scaleX = 1, int scaleY = 1);

	virtual ~frame_buf_window();

	// Return true if you handle the window message.
	typedef bool (*user_window_proc_ptr_t)(LRESULT& hres, HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, void *pData);   
	void set_window_proc_callback(user_window_proc_ptr_t windowProcPtr, void *pData) { m_pWindow_proc = windowProcPtr; m_pWindow_proc_data = pData; }

	int width(void) const { return m_width; }
	int height(void) const { return m_height; }      

	void update(void);

	void close(void);

	const simple_bgr_bitmap& frameBuffer(void) const { return m_frame_buffer; }

	simple_bgr_bitmap& frameBuffer(void) { return m_frame_buffer; }

private:
	int m_width, m_height;
	int m_orig_width, m_orig_height;
	int m_orig_x, m_orig_y;
	int m_scale_x, m_scale_y;
	WNDCLASS m_window_class;
	HWND m_window;
	char m_bitmap_info[16 + sizeof(BITMAPINFO)];
	BITMAPINFO* m_pBitmap_hdr;
	HDC m_windowHDC;
	simple_bgr_bitmap m_frame_buffer;
	static frame_buf_window* m_pCur_app;

	user_window_proc_ptr_t m_pWindow_proc;
	void *m_pWindow_proc_data;
		
	void create_window(const char* pTitle);

	void create_bitmap(void);

	LRESULT window_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

}; // class frame_buf_window


frame_buf_window* frame_buf_window::m_pCur_app;

frame_buf_window::frame_buf_window(const char* pTitle, int width, int height, int scaleX, int scaleY) :
    m_width(width),
    m_height(height),
    m_orig_width(0), 
    m_orig_height(0), 
    m_orig_x(0), 
    m_orig_y(0),
    m_window(0),
    m_pBitmap_hdr(NULL),
    m_windowHDC(0),
    m_pWindow_proc(NULL),
    m_pWindow_proc_data(NULL)
{
	m_scale_x = scaleX;
	m_scale_y = scaleY;

    create_window(pTitle);
      
    create_bitmap();           
}
      
frame_buf_window::~frame_buf_window()
{  
    close();
}

void frame_buf_window::update(void)
{
	if ( m_window )
	{
		InvalidateRect(m_window, NULL, FALSE);

		SendMessage(m_window, WM_PAINT, 0, 0);

		MSG msg;
		while (PeekMessage(&msg, m_window, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		//Sleep(0);
	}
}
      
void frame_buf_window::close(void)
{ 
    m_frame_buffer.clear();
      
    if (m_window)
    {
        ReleaseDC(m_window, m_windowHDC);
        DestroyWindow(m_window);
        m_window = 0;
        m_windowHDC = 0;
    }
}
                   
void frame_buf_window::create_window(const char* pTitle)
{
    memset( &m_window_class, 0, sizeof( m_window_class ) );
    m_window_class.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    m_window_class.lpfnWndProc = static_window_proc;
    m_window_class.cbWndExtra = sizeof(DWORD);
    m_window_class.hCursor = LoadCursor(0, IDC_ARROW);
    m_window_class.lpszClassName = pTitle;
    m_window_class.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH); //GetStockObject(NULL_BRUSH);
    RegisterClass(&m_window_class);
            
    RECT rect;
    rect.left = rect.top = 0;
    rect.right = m_width * m_scale_x;
    rect.bottom = m_height * m_scale_y;
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, 0);
    rect.right -= rect.left;
    rect.bottom -= rect.top;

    m_orig_x = (GetSystemMetrics(SM_CXSCREEN) - rect.right) / 2;
    m_orig_y = (GetSystemMetrics(SM_CYSCREEN) - rect.bottom) / 2;

    m_orig_width = rect.right;
    m_orig_height = rect.bottom;

    m_pCur_app = this;
      
    m_window = CreateWindowEx(
        0, pTitle, pTitle, 
        WS_OVERLAPPEDWINDOW, 
        m_orig_x, m_orig_y, rect.right, rect.bottom, 0, 0, 0, 0);

    SetWindowLong(m_window, 0, reinterpret_cast<LONG>(this));
      
    m_pCur_app = NULL;

    ShowWindow(m_window, SW_NORMAL);            
}

void frame_buf_window::create_bitmap(void)
{
    memset( &m_bitmap_info, 0, sizeof( m_bitmap_info ) );
      
    m_pBitmap_hdr = reinterpret_cast<BITMAPINFO*>(&m_bitmap_info);
    m_pBitmap_hdr->bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    m_pBitmap_hdr->bmiHeader.biWidth         = m_width;
    m_pBitmap_hdr->bmiHeader.biHeight        = -m_height;
    m_pBitmap_hdr->bmiHeader.biBitCount      = 24;
    m_pBitmap_hdr->bmiHeader.biPlanes        = 1;
    m_pBitmap_hdr->bmiHeader.biCompression   = BI_RGB;//BI_BITFIELDS;
	// Only really needed for 32bpp BI_BITFIELDS
    reinterpret_cast<uint32*>(m_pBitmap_hdr->bmiColors)[0] = 0x00FF0000;
    reinterpret_cast<uint32*>(m_pBitmap_hdr->bmiColors)[1] = 0x0000FF00;
    reinterpret_cast<uint32*>(m_pBitmap_hdr->bmiColors)[2] = 0x000000FF;

    m_windowHDC = GetDC(m_window);
      
    m_frame_buffer.init( m_width, m_height );
	m_frame_buffer.cls( 30, 30, 30 );
	//m_frame_buffer.draw_text( 50, 200, 2, 255, 127, 128, "This is a test!" );
}         

LRESULT frame_buf_window::window_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (m_pWindow_proc)
    {
        LRESULT lres;
        bool status = m_pWindow_proc(lres, hWnd, message, wParam, lParam, m_pWindow_proc_data);
        if (status)
			return lres;
    }

    switch (message)
    {
        case WM_PAINT:
        {
			if (m_frame_buffer.is_valid())
			{
				RECT window_size;
				GetClientRect(hWnd, &window_size);

				StretchDIBits(m_windowHDC, 
					0, 0, window_size.right, window_size.bottom, 
					0, 0, m_width, m_height, 
					m_frame_buffer.get_ptr(), m_pBitmap_hdr, 
					DIB_RGB_COLORS, SRCCOPY);

				ValidateRect(hWnd, NULL);
			}
			break;
        }
        case WM_KEYDOWN:
        {
			const int cEscCode = 27;
			if (cEscCode != (wParam & 0xFF))
				break;
        }
        case WM_CLOSE:
        {
			close();

			break;
        }
    }            

	return DefWindowProc( hWnd, message, wParam, lParam );
}

class CConCommandServerProtocolTypes
{
public:
	enum 
	{ 
		cProtocolPort = 3779,
		cMaxClients = 4
	};

	struct PacketHeader_t
	{
		uint16 m_nID;
		uint16 m_nType;
		uint32 m_nTotalSize;
	};

	struct SetCameraPosPacket_t : PacketHeader_t
	{
		Vector m_Pos;
		QAngle m_Angle;
	};

	struct ScreenshotPacket_t : PacketHeader_t
	{
		uint m_nScreenshotID;
		char m_szFilename[256];
	};
			
	enum 
	{ 
		cPacketHeaderID = 0x1234,
		cPacketHeaderSize = sizeof( PacketHeader_t ),
	};

	enum PacketTypes_t
	{
		cPacketTypeMessage = 0,
		cPacketTypeCommand = 1,
		cPacketTypeScreenshotRequest = 2,
		cPacketTypeScreenshotReply = 3,
		cPacketTypeSetCameraPos = 4,
		cPacketTypeConVarDumpRequest = 5,
		cPacketTypeConVarDumpReply = 6,

		cPackerTypeTotalMessages
	};
};

struct DumpedConVar_t
{
	CUtlString m_Name;
	CUtlString m_Value;

	bool operator< (const DumpedConVar_t & rhs ) const { return V_strcmp( m_Name.Get(), rhs.m_Name.Get() ) < 0; }
};
typedef CUtlVector<DumpedConVar_t> DumpedConVarVector_t;

static void DumpConVars( DumpedConVarVector_t &conVars )
{
	ICvar::Iterator iter( g_pCVar );
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase *var = iter.Get();

		ConVar *pConVar = dynamic_cast< ConVar* >( var );
		if ( !pConVar )
			continue;

		const char *pName = pConVar->GetName();
		const char *pVal = pConVar->GetString();
		if ( ( !pName ) || ( !pVal ) )
			continue;

		int nIndex = conVars.AddToTail();
		conVars[nIndex].m_Name.Set( pName );
		conVars[nIndex].m_Value.Set( pVal );
	}
}

class CConCommandConnection : public CConCommandServerProtocolTypes
{
public:
	CConCommandConnection() : 
		m_pSocket( NULL ), 
		m_bDeleteSocket( false ),
		m_nEndpointIndex( -1 ), 
		m_bClientFlag( false ),
		m_bReceivedNewCameraPos( false ),
		m_nSendBufOfs( 0 ),
		m_bHasNewScreenshot( false ),
		m_nScreenshotID( 0 )
	{
		memset( &m_NewCameraPos, 0, sizeof( m_NewCameraPos ) );
	}

	~CConCommandConnection()
	{
		Deinit();
	}

	bool IsConnected() const { return m_nEndpointIndex >= 0; }

	bool Init( const char *pAddress )
	{
		Deinit();
		
		m_nEndpointIndex = 0;
		m_bClientFlag = true;

		m_RecvBuf.EnsureCapacity( 4096 );
		m_SendBuf.EnsureCapacity( 4096 );
		m_RecvBuf.SetCountNonDestructively( 0 );
		m_SendBuf.SetCountNonDestructively( 0 );
		m_nSendBufOfs = 0;

		m_pSocket = new CSocketConnection;
		m_bDeleteSocket = true;
		m_pSocket->Init( CT_CLIENT, SP_TCP );

		SocketErrorCode_t err = m_pSocket->ConnectToServer( pAddress, cProtocolPort );
		if ( err != SOCKET_SUCCESS )
		{
			Warning( "CONCMDSRV: Failed connecting to con cmd server \"%s\"!\n", pAddress );
			
			Deinit();

			return false;
		}
		
		uint n = 0;
		while ( m_pSocket->GetEndpointSocketState( 0 ) == SSTATE_CONNECTION_IN_PROGRESS )
		{
			bool bIsConnected = false;
			err = m_pSocket->PollClientConnectionState( &bIsConnected );
			if ( err != SOCKET_SUCCESS )
			{
				Warning( "CONCMDSRV: Failed connecting to con cmd server \"%s\"!\n", pAddress );

				Deinit();

				return false;
			}

			if ( bIsConnected )
				break;
			
			ThreadSleep( 10 );
			
			if ( ++n >= 500 )
			{
				Warning( "CONCMDSRV: Failed connecting to con cmd server \"%s\"!\n", pAddress );

				Deinit();

				return false;
			}
		}

		if ( m_pSocket->GetEndpointSocketState( 0 ) != SSTATE_CONNECTED )
		{
			Warning( "CONCMDSRV: Failed connecting to con cmd server \"%s\"!\n", pAddress );

			Deinit();

			return false;
		}

		int nFlag = 1;
		m_pSocket->SetSocketOpt( 0, IPPROTO_TCP, TCP_NODELAY, &nFlag, sizeof( int ) );

		return TickConnection();
	}

	bool Init( CSocketConnection *pSocket, int nEndpointIndex, bool bClientFlag )
	{
		Deinit();
				
		int nFlag = 1;
		pSocket->SetSocketOpt( nEndpointIndex, IPPROTO_TCP, TCP_NODELAY, &nFlag, sizeof( int ) );
				
		m_pSocket = pSocket;
		m_bDeleteSocket = false;
		m_nEndpointIndex = nEndpointIndex;
		m_bClientFlag = bClientFlag;

		m_RecvBuf.EnsureCapacity( 4096 );
		m_SendBuf.EnsureCapacity( 4096 );

		m_RecvBuf.SetCountNonDestructively( 0 );
		m_SendBuf.SetCountNonDestructively( 0 );
		m_nSendBufOfs = 0;

		return TickConnection();
	}

	void Deinit()
	{
		if ( m_nEndpointIndex < 0 )
			return;

		ConMsg( "CONCMDSRV: Disconnecting endpoint %i\n", m_nEndpointIndex );

		if ( m_pSocket )
		{
			m_pSocket->ResetEndpoint( m_nEndpointIndex );
			
			if ( m_bDeleteSocket )
			{
				m_pSocket->Cleanup();
				delete m_pSocket;
			}
			m_pSocket = NULL;
		}

		m_nEndpointIndex = -1;
		m_bDeleteSocket = false;
		m_bClientFlag = false;
		
		m_RecvBuf.SetCountNonDestructively( 0 );
		m_SendBuf.SetCountNonDestructively( 0 );
		m_nSendBufOfs = 0;

		m_bHasNewScreenshot = false;
		m_nScreenshotID = 0;
		m_screenshot.clear();
		
		m_bReceivedNewCameraPos = false;
		memset( &m_NewCameraPos, 0, sizeof( m_NewCameraPos ) );
	}

	bool SendData( const void * pPacket, uint nSize )
	{
		if ( m_nEndpointIndex < 0 )
			return false;

		m_SendBuf.AddMultipleToTail( nSize, static_cast< const uint8 * >( pPacket ) );
		return TickConnectionSend();
	}

	bool TickConnectionRecv( )
	{
		for ( ; ; )
		{
			uint8 buf[4096];
			int nBytesRead = 0;
			SocketErrorCode_t nReadErr = m_pSocket->ReadFromEndpoint( m_nEndpointIndex, buf, sizeof( buf ), &nBytesRead );
			if ( nReadErr == SOCKET_ERR_READ_OPERATION_WOULD_BLOCK )
				break;
			else if ( nReadErr != SOCKET_SUCCESS )
			{
				Warning( "CONCMDSRV: Failed receiving data from client %i\n", m_nEndpointIndex );
				Deinit();
				return false;
			}

			m_RecvBuf.AddMultipleToTail( nBytesRead, buf );
			if ( m_RecvBuf.Count() >= cPacketHeaderSize )
			{
				if ( !ProcessMessages() )
				{
					Warning( "CONCMDSRV: Failed processing messages from client %i\n", m_nEndpointIndex );
					Deinit();
					return false;
				}
			}
		}
			
		return true;
	}

	bool TickConnectionSend( )
	{
		while ( m_nSendBufOfs < m_SendBuf.Count() )
		{
			int nBytesWritten = 0;
			SocketErrorCode_t nWriteErr = m_pSocket->WriteToEndpoint( m_nEndpointIndex, &m_SendBuf[ m_nSendBufOfs ], m_SendBuf.Count() - m_nSendBufOfs, &nBytesWritten );
			if ( nWriteErr == SOCKET_ERR_WRITE_OPERATION_WOULD_BLOCK )
				break;
			else if ( nWriteErr != SOCKET_SUCCESS )
			{
				Warning( "CONCMDSRV: Failed sending data to client %i\n", m_nEndpointIndex );
				Deinit();
				return false;
			}

			m_nSendBufOfs += nBytesWritten;
			if ( m_nSendBufOfs == m_SendBuf.Count() )
			{
				m_SendBuf.SetCountNonDestructively( 0 );
				m_nSendBufOfs = 0;
				break;
			}
		}

		return true;
	}

	bool TickConnection()
	{
		if ( !IsConnected() )
			return false;
				
		if ( !TickConnectionSend() )
			return false;
		if ( !TickConnectionRecv() )
			return false;

		if ( m_bReceivedNewCameraPos )
		{
			m_bReceivedNewCameraPos = false;
						
			char buf[256];
			//V_snprintf( buf, sizeof( buf ), "setpos_exact %f %f %f;setang_exact %f %f %f\n", m_NewCameraPos.m_Pos.x, m_NewCameraPos.m_Pos.y, m_NewCameraPos.m_Pos.z, m_NewCameraPos.m_Angle.x, m_NewCameraPos.m_Angle.y, m_NewCameraPos.m_Angle.z );
			V_snprintf( buf, sizeof( buf ), "setpos_exact 0x%X 0x%X 0x%X;setang_exact 0x%X 0x%X 0x%X\n", *(DWORD*)&m_NewCameraPos.m_Pos.x, *(DWORD*)&m_NewCameraPos.m_Pos.y, *(DWORD*)&m_NewCameraPos.m_Pos.z, *(DWORD*)&m_NewCameraPos.m_Angle.x, *(DWORD*)&m_NewCameraPos.m_Angle.y, *(DWORD*)&m_NewCameraPos.m_Angle.z );
			
			Cbuf_AddText( buf );
			Cbuf_Execute();
		}
				
		return true;
	}

	bool HasNewScreenshotFlag() const { return m_bHasNewScreenshot; }
	void ClearNewScreenshotFlag() { m_bHasNewScreenshot = false; }
	uint GetScreenshotID() const { return m_nScreenshotID; }
	simple_bitmap &GetScreenshot() { return m_screenshot; }

	bool HasNewConVarDumpFlag() const { return m_bHasNewConVarDump; }
	void ClearHasNewConVarDumpFlag() { m_bHasNewConVarDump = false; }
	DumpedConVarVector_t &GetConVarDumpVector() { return m_DumpedConVars; }
	const DumpedConVarVector_t &GetConVarDumpVector() const { return m_DumpedConVars; }

private:
	CSocketConnection *m_pSocket;
	bool m_bDeleteSocket;
	int m_nEndpointIndex;
	bool m_bClientFlag;

	CUtlVector<uint8> m_RecvBuf;

	CUtlVector<uint8> m_SendBuf;
	int m_nSendBufOfs;

	bool m_bReceivedNewCameraPos;
	SetCameraPosPacket_t m_NewCameraPos;

	simple_bitmap m_screenshot;
	bool m_bHasNewScreenshot;
	uint m_nScreenshotID;

	bool m_bHasNewConVarDump;
	DumpedConVarVector_t m_DumpedConVars;

	bool ProcessMessages()
	{
		while ( m_RecvBuf.Count() >= cPacketHeaderSize )
		{
			const int nCurRecvBufSize = m_RecvBuf.Count();

			const PacketHeader_t header( *reinterpret_cast<const PacketHeader_t *>( &m_RecvBuf[0] ) );
			if ( header.m_nID != cPacketHeaderID )
				return false;

			if ( header.m_nTotalSize < cPacketHeaderSize )
				return false;

			if ( header.m_nType >= cPackerTypeTotalMessages )
				return false;

			if ( (uint32)m_RecvBuf.Count() < header.m_nTotalSize )
				break;

			const uint nNumMessageBytes = header.m_nTotalSize - cPacketHeaderSize;
			const uint8 *pMsg = nNumMessageBytes ? &m_RecvBuf[cPacketHeaderSize] : NULL;

			switch ( header.m_nType )
			{
				case cPacketTypeMessage:
				{
					if ( nNumMessageBytes )
					{
						ConMsg( "CONCMDSRV: Message from client %i: ", m_nEndpointIndex );
						
						CUtlVectorFixedGrowable<char, 4096> buf;
						buf.SetCount( nNumMessageBytes + 1 );
						memcpy( &buf[0], pMsg, nNumMessageBytes );
						buf[nNumMessageBytes] = '\0';

						ConMsg( "%s\n", &buf[0] );
					}

					break;
				}
				case cPacketTypeScreenshotRequest:
				{
					const ScreenshotPacket_t &requestPacket = *reinterpret_cast<const ScreenshotPacket_t *>( &m_RecvBuf[0] );
					if ( requestPacket.m_nTotalSize != sizeof( ScreenshotPacket_t ) )
					{
						Warning( "CONCMDSRV: Invalid screenshot request packet!\n" );
						return false;
					}

					if ( !videomode )
						break;

					uint nWidth = videomode->GetModeWidth();
					uint nHeight = videomode->GetModeHeight();
					if ( !nWidth || !nHeight ) 
						break;

					static void *s_pBuf;
					if ( !s_pBuf )
					{
						s_pBuf = malloc( nWidth * nHeight * 3 );
					}
					videomode->ReadScreenPixels( 0, 0, nWidth, nHeight, s_pBuf, IMAGE_FORMAT_RGB888 );

					size_t nPNGSize = 0;
					void *pPNGData = tdefl_write_image_to_png_file_in_memory( s_pBuf, nWidth, nHeight, 3, &nPNGSize, true );

					if ( pPNGData )
					{
						ScreenshotPacket_t replyPacket;
						replyPacket.m_nID = cPacketHeaderID;
						replyPacket.m_nTotalSize = sizeof( replyPacket ) + nPNGSize;
						replyPacket.m_nType = cPacketTypeScreenshotReply;
						V_strcpy( replyPacket.m_szFilename, requestPacket.m_szFilename );
						replyPacket.m_nScreenshotID = requestPacket.m_nScreenshotID;
						
						if ( !SendData( &replyPacket, sizeof( replyPacket ) ) ) 
						{
							free( pPNGData );
							return false;
						}

						if ( !SendData( pPNGData, nPNGSize ) )
						{
							free( pPNGData );
							return false;
						}
						
						free( pPNGData );
					}
					
					break;
				}
				case cPacketTypeScreenshotReply:
				{
					const ScreenshotPacket_t &replyPacket = *reinterpret_cast<const ScreenshotPacket_t *>( &m_RecvBuf[0] );
					if ( replyPacket.m_nTotalSize <= sizeof( ScreenshotPacket_t ) )
					{
						Warning( "CONCMDSRV: Invalid screenshot reply packet!\n" );
						return false;
					}

					const void *pPNGData = &m_RecvBuf[sizeof(ScreenshotPacket_t)];
					uint nPNGDataSize = header.m_nTotalSize - sizeof(ScreenshotPacket_t);
					
					if ( !replyPacket.m_nScreenshotID )
					{
						char szFilename[512];
						V_snprintf( szFilename, sizeof( szFilename ), "%s_%u.png", replyPacket.m_szFilename, m_nEndpointIndex );

						FileHandle_t fh = g_pFullFileSystem->Open( szFilename, "wb" );
						if ( fh )
						{
							g_pFullFileSystem->Write( pPNGData, nPNGDataSize, fh );
							g_pFullFileSystem->Close(fh);
						}
					}
					else
					{
						int nWidth = 0, nHeight = 0, nComp = 3;
						unsigned char *pImageData = stbi_load_from_memory( reinterpret_cast< stbi_uc const * >( pPNGData ), nPNGDataSize, &nWidth, &nHeight, &nComp, 3);

						if ( !pImageData )
						{
							Warning( "CONCMDSRV: Failed unpacking PNG screenshot!\n" );
							return false;
						}

						m_bHasNewScreenshot = true;
						m_nScreenshotID = replyPacket.m_nScreenshotID;

						if ( ( (int)m_screenshot.width() != nWidth ) || ( (int)m_screenshot.height() != nHeight ) )
						{
							m_screenshot.init( nWidth, nHeight );
						}

						memcpy( m_screenshot.get_ptr(), pImageData, nWidth * nHeight * 3 );
													
						stbi_image_free( pImageData );
					}

					break;
				}
				case cPacketTypeCommand:
				{
					if ( nNumMessageBytes )
					{
						//ConMsg( "CONCMDSRV: Console command from client %i: ", m_nEndpointIndex );

						CUtlVectorFixedGrowable<char, 4096> buf;
						buf.SetCount( nNumMessageBytes + 1 );
						memcpy( &buf[0], pMsg, nNumMessageBytes );
						buf[nNumMessageBytes] = '\0';
					
						//ConMsg( "\"%s\"\n", &buf[0] );

						Cbuf_AddText( &buf[0] );
						Cbuf_AddText( "\n" );
						Cbuf_Execute();
					}

					break;
				}
				case cPacketTypeSetCameraPos:
				{
					const SetCameraPosPacket_t &setCameraPosPacket = reinterpret_cast<const SetCameraPosPacket_t &>( m_RecvBuf[0] );
					if ( setCameraPosPacket.m_nTotalSize != sizeof( SetCameraPosPacket_t ) )
					{
						Warning( "CONCMDSRV: Invalid set camera pos packet!\n" );
						return false;
					}

					m_bReceivedNewCameraPos = true;
					m_NewCameraPos = setCameraPosPacket;
					break;
				}
				case cPacketTypeConVarDumpRequest:
				{
					CUtlVector<uint8> conVarData;

					DumpedConVarVector_t conVars;
					DumpConVars( conVars );

					for ( int i = 0; i < conVars.Count(); i++ )
					{
						const char *pName = conVars[i].m_Name.Get();
						const char *pVal = conVars[i].m_Value.Get();
						if ( ( !pName ) || ( !pVal ) )
							continue;

						conVarData.AddMultipleToTail( V_strlen( pName ) + 1, reinterpret_cast<const uint8 *>( pName ) );
						conVarData.AddMultipleToTail( V_strlen( pVal ) + 1, reinterpret_cast<const uint8 *>( pVal ) );
					}

					PacketHeader_t replyPacket;
					replyPacket.m_nID = cPacketHeaderID;
					replyPacket.m_nTotalSize = sizeof( replyPacket ) + conVarData.Count();
					replyPacket.m_nType = cPacketTypeConVarDumpReply;
					if ( !SendData( &replyPacket, sizeof( replyPacket ) ) )
						return false;
					if ( conVarData.Count() )
					{
						if ( !SendData( &conVarData[0], conVarData.Count() ) )
							return false;
					}

					break;
				}
				case cPacketTypeConVarDumpReply:
				{
					m_bHasNewConVarDump = true;
					m_DumpedConVars.SetCountNonDestructively( 0 );
					m_DumpedConVars.EnsureCapacity( 4096 );

					const char *pCur = reinterpret_cast<const char *>( pMsg );
					const char *pEnd = reinterpret_cast<const char *>( pMsg ) + nNumMessageBytes;

					while ( pCur < pEnd )
					{
						const uint nBytesLeft = pEnd - pCur;

						uint nNameLen = V_strlen( pCur );
						if ( ( nNameLen + 1 ) >= nBytesLeft )
						{
							Warning( "CONCMDSRV: Failed parsing convar dump reply!\n" );
							Assert( 0 );
							return false;
						}

						uint nValLen = V_strlen( pCur + nNameLen + 1 );
						
						uint nTotalLenInBytes = nNameLen + 1 + nValLen + 1;
						if ( nTotalLenInBytes > nBytesLeft )
						{
							Warning( "CONCMDSRV: Failed parsing convar dump reply!\n" );
							Assert( 0 );
							return false;
						}

						int nIndex = m_DumpedConVars.AddToTail();
						m_DumpedConVars[nIndex].m_Name.Set( pCur );
						m_DumpedConVars[nIndex].m_Value.Set( pCur + nNameLen + 1 );
						
						pCur += nTotalLenInBytes;
					}

					ConMsg( "Received convar dump reply from endpoint %i, %u total convars\n", m_nEndpointIndex, m_DumpedConVars.Count() );
					
					break;
				}
				default:
				{
					return false;
				}
			}

			// In case the connection gets lost during a send to reply
			if ( ( m_nEndpointIndex < 0 ) || ( nCurRecvBufSize != m_RecvBuf.Count() ) )
				return false;

			int nNumBytesRemaining = m_RecvBuf.Count() - header.m_nTotalSize;
			Assert( nNumBytesRemaining >= 0 );
			if ( nNumBytesRemaining >= 0 )
			{
				memmove( &m_RecvBuf[0], &m_RecvBuf[0] + header.m_nTotalSize, nNumBytesRemaining );
				m_RecvBuf.SetCountNonDestructively( nNumBytesRemaining );
			}
		}

		return true;
	}
};

ConVar ccs_broadcast_camera( "ccs_broadcast_camera", "0", FCVAR_CHEAT );
ConVar ccs_camera_sync( "ccs_camera_sync", "0", FCVAR_CHEAT );
ConVar ccs_remote_screenshots( "ccs_remote_screenshots", "0", FCVAR_CHEAT );
ConVar ccs_remote_screenshots_delta( "ccs_remote_screenshots_delta", "1", FCVAR_CHEAT );

class CConCommandServer : public CConCommandServerProtocolTypes
{
	static bool UserWindowProc( LRESULT& hres, HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, void *pData )
	{
		CConCommandServer *pServer = static_cast< CConCommandServer * >( pData );

		switch ( message )
		{
			case WM_KEYDOWN:
			{
				uint cKey = ( wParam & 0xFF );
				if ( ( cKey >= '0' ) && ( cKey <= '9' ) )
				{
					pServer->m_nViewEndpointIndex = cKey - '0';
				}								
			}
		}

		return false;
	}

public:
	CConCommandServer() : 
		m_bInitialized( false ),
		m_pFrameBufWindow( NULL ),
		m_nViewEndpointIndex( 0 )
	{
	}

	~CConCommandServer()
	{
		Deinit();
	}

	bool Init()
	{
		Deinit();

		m_Socket.Init( CT_SERVER, SP_TCP );

		if ( m_Socket.Listen( cProtocolPort, cMaxClients ) != SOCKET_SUCCESS )
		{
			Warning( "CONCMDSRV: CConCommandServer:Init: Failed to create listen socket!\n" );
			return false;
		}

		ConMsg( "CONCMDSVR: Listening for connections\n" );
		
		uint nWidth = 1280, nHeight = 1024;
		if ( videomode )
		{
			nWidth = videomode->GetModeWidth();
			nHeight = videomode->GetModeHeight();
		}
		m_pFrameBufWindow = new frame_buf_window( "CCS", nWidth, nHeight );
		m_pFrameBufWindow->set_window_proc_callback( UserWindowProc, this );

		m_bInitialized = true;
		return true;
	}

	void Deinit()
	{
		if ( !m_bInitialized )
			return;

		delete m_pFrameBufWindow;
		m_pFrameBufWindow = NULL;
		
		for ( int i = 0; i < cMaxClients; i++ )
		{
			m_Clients[i].Deinit();
		}

		m_Socket.Cleanup();
		m_nViewEndpointIndex = 0;

		ConMsg( "CONCMDSVR: Deinitialized\n" );
				
		m_bInitialized = false;
	}

	bool IsInitialized()
	{
		return m_bInitialized;
	}
	
	void TickFrame( float flTime )
	{
		if ( !m_bInitialized )
			return;

		AcceptNewConnections();

		if ( ccs_broadcast_camera.GetBool() )
		{
			Vector vecOrigin = MainViewOrigin();
			QAngle angles( 0, 0, 0 );

			IClientEntity *localPlayer = entitylist ? entitylist->GetClientEntity( cl.m_nPlayerSlot + 1 ) : NULL;
			if ( localPlayer )
			{
				vecOrigin = localPlayer->GetAbsOrigin();
				angles = localPlayer->GetAbsAngles();
			}
				
			SetCameraPosPacket_t setCameraPacket;
			setCameraPacket.m_nID = cPacketHeaderID;
			setCameraPacket.m_nType = cPacketTypeSetCameraPos;
			setCameraPacket.m_nTotalSize = sizeof( setCameraPacket );
			setCameraPacket.m_Pos = vecOrigin;
			setCameraPacket.m_Angle = angles;
			SendDataToAllClients( &setCameraPacket, sizeof( setCameraPacket ) );

			if ( ccs_camera_sync.GetBool() )
			{
				ccs_camera_sync.SetValue( false );
				char buf[256];
				V_snprintf( buf, sizeof( buf ), "setpos_exact 0x%X 0x%X 0x%X;setang_exact 0x%X 0x%X 0x%X\n", *(DWORD*)&setCameraPacket.m_Pos.x, *(DWORD*)&setCameraPacket.m_Pos.y, *(DWORD*)&setCameraPacket.m_Pos.z, *(DWORD*)&setCameraPacket.m_Angle.x, *(DWORD*)&setCameraPacket.m_Angle.y, *(DWORD*)&setCameraPacket.m_Angle.z );
				Cbuf_AddText( buf );
				Cbuf_Execute();
			}
		}

		if ( ccs_remote_screenshots.GetBool() )
		{
			static double flTotalTime;
			flTotalTime += flTime;
			if ( flTotalTime > .5f )
			{
				flTotalTime = 0.0f;

				ScreenshotPacket_t screenshotPacket;
				screenshotPacket.m_nID = cPacketHeaderID;
				screenshotPacket.m_nType = cPacketTypeScreenshotRequest;
				screenshotPacket.m_nTotalSize = sizeof( screenshotPacket );
				screenshotPacket.m_szFilename[0] = '\0';
				screenshotPacket.m_nScreenshotID = 1;
				SendDataToAllClients( &screenshotPacket, sizeof( screenshotPacket ) );
			}
		}
						
		bool bHasUpdatedScreenshot = false;
				
		simple_bgr_bitmap &frameBuf = m_pFrameBufWindow->frameBuffer();						

		for ( int i = 0; i < cMaxClients; i++ )
		{
			CConCommandConnection &client = m_Clients[i];
			if ( !client.IsConnected() )
				continue;
			
			client.TickConnection();

			if ( client.HasNewConVarDumpFlag() )
			{
				client.ClearHasNewConVarDumpFlag();

				diffClientsDumpedConVars( i, client.GetConVarDumpVector() );
			}

			if ( m_nViewEndpointIndex == i )
			{
				if ( client.HasNewScreenshotFlag() )
				{
					client.ClearNewScreenshotFlag();

					simple_bitmap &clientScreenshot = client.GetScreenshot();

					if ( ccs_remote_screenshots_delta.GetBool() )
					{
						if ( videomode && !bHasUpdatedScreenshot )
						{
							bHasUpdatedScreenshot = true;

							uint nScreenWidth = videomode->GetModeWidth();
							uint nScreenHeight = videomode->GetModeHeight();

							if ( ( m_screenshot.width() != nScreenWidth ) || ( m_screenshot.height() != nScreenHeight ) )
							{
								m_screenshot.init( nScreenWidth, nScreenHeight );
							}

							videomode->ReadScreenPixels( 0, 0, nScreenWidth, nScreenHeight, m_screenshot.get_ptr(), IMAGE_FORMAT_RGB888 );
						}

						uint mx = MIN( clientScreenshot.width(), m_screenshot.width() ); 
						mx = MIN( mx, frameBuf.width() );
					
						uint my = MIN( clientScreenshot.height(), m_screenshot.height() );
						my = MIN( my, frameBuf.height() );

						for ( uint y = 0; y < my; ++y )
						{
							const uint8 *pSrc1 = clientScreenshot.get_scanline( y );
							const uint8 *pSrc2 = m_screenshot.get_scanline( y );
							uint8 *pDst = frameBuf.get_scanline( y );

							for ( uint x = 0; x < mx; ++x )
							{
								int r = 2 * ( pSrc1[0] - pSrc2[0] ) + 128; 
								int g = 2 * ( pSrc1[1] - pSrc2[1] ) + 128; 
								int b = 2 * ( pSrc1[2] - pSrc2[2] ) + 128; 

								if ( ( r | g | b ) & 0xFFFFFF00 ) 
								{
									if ( r & 0xFFFFFF00 ) { r = (~( r >> 31 )) & 0xFF; }
									if ( g & 0xFFFFFF00 ) { g = (~( g >> 31 )) & 0xFF; }
									if ( b & 0xFFFFFF00 ) { b = (~( b >> 31 )) & 0xFF; }
								}

								pDst[0] = (uint8)b;
								pDst[1] = (uint8)g;
								pDst[2] = (uint8)r;
							
								pSrc1 += 3;
								pSrc2 += 3;
								pDst += 3;
							}
						}

					}
					else
					{
						frameBuf.blit( 0, 0, (simple_bgr_bitmap &)clientScreenshot, true );
					}
				}
			}
		}

		if ( m_pFrameBufWindow )
		{
			m_pFrameBufWindow->update();
		}
	}

	void SendMessageToClients( const char * pCmd )
	{
		if ( !m_bInitialized )
			return;

		int n = V_strlen( pCmd );

		char buf[4096];
		if ( ( n >= 2 ) && ( pCmd[0] == '\"' ) && ( pCmd[ n - 1 ] == '\"' ) )
		{
			memcpy( buf, pCmd + 1, n - 2 );
			buf[n - 2] = '\0';
			pCmd = buf;
		}

		SendStringToClients( pCmd, cPacketTypeMessage );
	}

	void SendConCommandToClients( const char * pCmd, bool bExecLocally = false )
	{
		int n = V_strlen( pCmd );

		char buf[4096];
		if ( ( n >= 2 ) && ( pCmd[0] == '\"' ) && ( pCmd[ n - 1 ] == '\"' ) )
		{
			memcpy( buf, pCmd + 1, n - 2 );
			buf[n - 2] = '\0';
			pCmd = buf;
		}

		if ( m_bInitialized )
		{
			SendStringToClients( pCmd, cPacketTypeCommand );
		}

		if ( bExecLocally )
		{
			Cbuf_AddText( pCmd );
			Cbuf_AddText( "\n" );
			Cbuf_Execute();
		}
	}
	
	void SendCameraPosAndAngleToClients( Vector &vecOrigin, QAngle &angles )
	{
		if ( !m_bInitialized )
			return;

		SetCameraPosPacket_t packet;

		packet.m_nID = cPacketHeaderID;
		packet.m_nType = cPacketTypeSetCameraPos;
		packet.m_nTotalSize = sizeof( packet );
		packet.m_Pos = vecOrigin;
		packet.m_Angle = angles;

		SendDataToAllClients( &packet, sizeof( packet ) );
	}

	void SendScreenshotRequestToClients( const char *pFilename )
	{
		if ( !m_bInitialized )
			return;

		ScreenshotPacket_t packet;

		packet.m_nID = cPacketHeaderID;
		packet.m_nType = cPacketTypeScreenshotRequest;
		packet.m_nTotalSize = sizeof( packet );
		V_strncpy( packet.m_szFilename, pFilename, sizeof( packet.m_szFilename ) );
		packet.m_nScreenshotID = 0;

		SendDataToAllClients( &packet, sizeof( packet ) );
	}

	void DiffConVarsOfAllClients()
	{
		if ( !m_bInitialized )
			return;

		PacketHeader_t packet;
		packet.m_nID = cPacketHeaderID;
		packet.m_nTotalSize = sizeof( PacketHeader_t );
		packet.m_nType = cPacketTypeConVarDumpRequest;
		SendDataToAllClients( &packet, sizeof( packet ) );
	}
	
private:
	bool m_bInitialized;
	CSocketConnection m_Socket;
			
	CConCommandConnection m_Clients[cMaxClients];
	frame_buf_window *m_pFrameBufWindow;

	simple_bitmap m_screenshot;
	int m_nViewEndpointIndex;
	
	void AcceptNewConnections()
	{
		for ( ; ; )
		{
			int nNewEndpointIndex = -1;
			SocketErrorCode_t err = m_Socket.TryAcceptIncomingConnection( &nNewEndpointIndex );
			if ( ( err != SOCKET_SUCCESS ) || ( nNewEndpointIndex < 0 ) )
				break;
			
			int i;
			for ( i = 0; i < cMaxClients; i++ )
			{
				if ( !m_Clients[i].IsConnected() )
					break;
			}
			if ( i == cMaxClients )
			{
				m_Socket.ResetEndpoint( nNewEndpointIndex );
				Warning( "CONCMDSRV: Too many active connections!\n" );
				break;
			}

			if ( !m_Clients[i].Init( &m_Socket, nNewEndpointIndex, false ) )
			{
				Warning( "CONCMDSRV: Failed accepting connection from endpoint %i\n", nNewEndpointIndex );
			}
			else
			{
				ConMsg( "CONCMDSRV: Accepting connection at endpoint %i\n", nNewEndpointIndex );
			}
		}
	}
	
	bool SendDataToAllClients( const void *p, uint nSize )
	{
		if ( !nSize )
			return true;

		bool bSuccess = true;

		for ( uint i = 0; i < cMaxClients; i++ )
		{
			if ( !m_Clients[i].IsConnected() )
				continue;

			if ( !m_Clients[i].SendData( p, nSize ) )
			{
				bSuccess = false;
			}
		}

		return bSuccess;
	}

	void SendStringToClients( const char *pCmd, PacketTypes_t nType )
	{
		uint8 buf[8192];

		uint nStrLen = strlen( pCmd );
		const uint nMaxStrLen = sizeof( buf ) - cPacketHeaderSize;
		nStrLen = MIN( nStrLen, nMaxStrLen );

		PacketHeader_t &hdr = reinterpret_cast<PacketHeader_t &>(buf[0]);
		hdr.m_nID = cPacketHeaderID;
		hdr.m_nType = nType;
		hdr.m_nTotalSize = cPacketHeaderSize + nStrLen;
		memcpy( buf + cPacketHeaderSize, pCmd, nStrLen );

		SendDataToAllClients( buf, hdr.m_nTotalSize );
	}
		
	static int FindConVar( const DumpedConVarVector_t &sortedConVars, const char *pName )
	{
		int l = 0, h = sortedConVars.Count() - 1;
		while ( l <= h )
		{
			int m = ( l + h ) >> 1;
			int d = V_strcmp( sortedConVars[m].m_Name.Get(), pName );
			if ( !d )
				return m;
			else if ( d > 0 )
				h = m - 1;
			else
				l = m + 1;
		}
		return -1;
	}

	static void DiffConVars( DumpedConVarVector_t &serverConVars, DumpedConVarVector_t &clientConVars )
	{
		if ( serverConVars.Count() ) 
		{
			std::sort( &serverConVars.Head(), &serverConVars.Tail() + 1 );
		}

		if ( clientConVars.Count() ) 
		{
			std::sort( &clientConVars.Head(), &clientConVars.Tail() + 1 );
		}

		CUtlVector<bool> clientConVarFoundFlags;
		clientConVarFoundFlags.SetCount( clientConVars.Count() );
		memset( &clientConVarFoundFlags.Head(), 0, sizeof( bool ) * clientConVars.Count() );

		uint nTotalNotFound = 0;
		uint nTotalMatches = 0;
		uint nTotalMismatches = 0;

		for ( int i = 0; i < serverConVars.Count(); i++ )
		{
			const DumpedConVar_t &serverConVar = serverConVars[i];
			const char *pServerConVarName = serverConVar.m_Name.Get();
			const char *pServerConVarValue = serverConVar.m_Value.Get();

			Assert( FindConVar( serverConVars, pServerConVarName ) != -1 );

			int nClientConVarIndex = FindConVar( clientConVars, pServerConVarName );
			if ( nClientConVarIndex < 0 )
			{
				Warning( "%s: Can't find server convar on client, value: \"%s\"\n", pServerConVarName, pServerConVarValue );
				nTotalNotFound++;
			}
			else
			{
				clientConVarFoundFlags[nClientConVarIndex] = true;

				if ( V_strcmp( pServerConVarValue, clientConVars[nClientConVarIndex].m_Value.Get() ) == 0 )
				{
					nTotalMatches++;
				}
				else
				{
					nTotalMismatches++;
					Warning( "%s: Convar diff: server=\"%s\", client=\"%s\"\n", pServerConVarName, pServerConVarValue, clientConVars[nClientConVarIndex].m_Value );
				}
			}
		}

		uint nTotalUnmatched = 0;
		for ( int i = 0; i < clientConVars.Count(); ++i )
		{
			if ( !clientConVarFoundFlags[i] )
			{
				nTotalUnmatched++;
				Warning( "%s: Client convar does not exist on server, value: \"%s\"\n", clientConVars[i].m_Name.Get(), clientConVars[i].m_Value.Get() );
			}
		}

		ConMsg( "--- Summary:\n");
		ConMsg( "Total server convars: %u\n", serverConVars.Count() );
		ConMsg( "Total client convars: %u\n", clientConVars.Count() );
		ConMsg( "Total server convars not found on client: %u\n", nTotalNotFound );
		ConMsg( "Total server convars that match clients: %u\n", nTotalMatches );
		ConMsg( "Total server convars that mismatch clients: %u\n", nTotalMismatches );
		ConMsg( "Total client convars that don't exist on server: %u\n", nTotalUnmatched );
	}

	void diffClientsDumpedConVars( int nClientIndex, DumpedConVarVector_t &clientConVars )
	{
		DumpedConVarVector_t serverConVars;
		DumpConVars( serverConVars );
		ConMsg( "Convar diff of client %i:\n", nClientIndex );
		DiffConVars( serverConVars, clientConVars );
	}
};

CConCommandServer g_ConCommandServer;

CON_COMMAND_F( ccs_start, "Start the con command server.", FCVAR_CHEAT )
{
	if ( !g_ConCommandServer.IsInitialized() )
	{
		g_ConCommandServer.Init();
	}
	else
	{
		Warning( "CONCMDSVR: Already initialized\n" );
	}
}

CON_COMMAND_F( ccs_stop, "Stop the con command server.", FCVAR_CHEAT )
{
	if ( g_ConCommandServer.IsInitialized() )
	{
		g_ConCommandServer.Deinit();
	}
	else
	{
		Warning( "CONCMDSVR: Not initialized\n" );
	}
}

CON_COMMAND_F( ccs_msg, "Send a message to any connected con command server clients.", FCVAR_CHEAT )
{
	if ( g_ConCommandServer.IsInitialized() )
	{
		if ( args.ArgC() < 2 )
		{
			Warning( "Usage: ccs_msg \"message\"\n" );
			return;
		}

		g_ConCommandServer.SendMessageToClients( args.ArgS() );
	}
	else
	{
		Warning( "CONCMDSVR: Not initialized\n" );
	}
}

CON_COMMAND_F( ccs_cmd, "Send a console message to any connected con command server clients.", FCVAR_CHEAT )
{
	if ( g_ConCommandServer.IsInitialized() )
	{
		if ( args.ArgC() < 2 )
		{
			ConMsg( "Usage: ccs_cmd \"command\"\n" );
			return;
		}

		g_ConCommandServer.SendConCommandToClients( args.ArgS() );
	}
	else
	{
		Warning( "CONCMDSVR: Not initialized\n" );
	}
}

CON_COMMAND_F( ccs_cmd_local, "Send a console message to any connected con command server clients, and also issue the command locally.", FCVAR_CHEAT )
{
	if ( g_ConCommandServer.IsInitialized() )
	{
		if ( args.ArgC() < 2 )
		{
			ConMsg( "Usage: ccs_cmd_local \"command\"\n" );
			return;
		}

		g_ConCommandServer.SendConCommandToClients( args.ArgS(), true );
	}
	else
	{
		Warning( "CONCMDSVR: Not initialized\n" );
	}
}

CON_COMMAND_F( ccs_screenshot, "Request screenshots from connected clients", FCVAR_CHEAT )
{
	if ( g_ConCommandServer.IsInitialized() )
	{
		const char *pBaseFileName = "ccs_screenshot";
		if ( args.ArgC() >= 2 )
		{
			pBaseFileName = args.Arg( 1 );
		}
		g_ConCommandServer.SendScreenshotRequestToClients( pBaseFileName );
	}
	else
	{
		Warning( "CONCMDSVR: Not initialized\n" );
	}
}

CON_COMMAND_F( ccs_diff_convars, "Diffs server's convars vs. all connected clients", FCVAR_CHEAT )
{ 
	if ( g_ConCommandServer.IsInitialized() )
	{
		g_ConCommandServer.DiffConVarsOfAllClients();
	}
	else
	{
		Warning( "CONCMDSVR: Not initialized\n" );
	}
}

CON_COMMAND_F( ccs_dump_convars, "Dump all convars to console", FCVAR_CHEAT )
{ 
	DumpedConVarVector_t conVars;
	DumpConVars( conVars );

	for ( int i = 0; i < conVars.Count(); i++ )
	{
		ConMsg( "%s %s\n", conVars[i].m_Name.Get(), conVars[i].m_Value.Get() );
	}
	ConMsg( "Dumped %i convars\n", conVars.Count() );
}

CConCommandConnection g_ConCommandConnection;

CON_COMMAND_F( ccs_connect, "Connect to a con cmd server.", FCVAR_CHEAT )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg( "Usage: ccs_connect \"address\"\n" );
		return;
	}

	if ( g_ConCommandConnection.IsConnected() )
	{
		g_ConCommandConnection.Deinit();
		Warning( "CONCMDSRV: Disconnected\n" );
	}

	if ( g_ConCommandConnection.Init( args.ArgS() ) )
	{
		ConMsg( "CONCMDSRV: Connected\n" );
	}
	else
	{
		Warning( "CONCMDSRV: Failed to connect!\n" );
	}
}

CON_COMMAND_F( ccs_disconnect, "Disconnect from a con cmd server.", FCVAR_CHEAT )
{
	if ( !g_ConCommandConnection.IsConnected() )
	{
		Warning( "CONCMDSRV: Not connected!\n" );
	}
	else
	{	
		g_ConCommandConnection.Deinit();
		
		Warning( "CONCMDSRV: Disconnected\n" );
	}
}

#endif // CON_COMMAND_SERVER_SUPPORT

void CCS_Init()
{
}

void CCS_Shutdown()
{
#ifdef CON_COMMAND_SERVER_SUPPORT
	g_ConCommandConnection.Deinit();
	g_ConCommandServer.Deinit();
#endif
}

void CCS_Tick( float flTime )
{
	(void)flTime;

#ifdef CON_COMMAND_SERVER_SUPPORT
	if ( g_ConCommandConnection.IsConnected() )
	{
		g_ConCommandConnection.TickConnection();
	}
	if ( g_ConCommandServer.IsInitialized() )
	{
		g_ConCommandServer.TickFrame( flTime );
	}
#endif
}

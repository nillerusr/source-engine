//========= Copyright Valve Corporation, All rights reserved. ============//
// smp.cpp : Main window procedure
//

#include "stdafx.h"
#include "resource.h"
#include "initguid.h"
#include "CWMPHost.h"
#include <commctrl.h>
#include <windows.h>
#include <psapi.h>
#include <math.h>
#include <cstdio>
#include <vector>
#include <string>
#include <strstream>
#include <fstream>


#include "IceKey.h"


CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
END_OBJECT_MAP()


#define ID_SKIP_FADE_TIMER 1
#define ID_DRAW_TIMER 2


const float FADE_TIME = 1.0f;
const int MAX_BLUR_STEPS = 100;

HINSTANCE g_hInstance;
HWND g_hBlackFadingWindow = 0;
bool g_bFadeIn = true;
bool g_bFrameCreated = false;
CWMPHost g_frame;
CWMPHost *g_pFrame = NULL;
HDC g_hdcCapture = 0;
HDC g_hdcBlend = 0;
HBITMAP g_hbmCapture = 0;
HBITMAP g_hbmBlend = 0;
HMONITOR g_hMonitor = 0;

int g_screenWidth = 0;
int g_screenHeight = 0;

LPTSTR g_lpCommandLine = NULL;
std::string g_redirectTarget;
std::string g_URL;
bool g_bReportStats = false;
bool g_bUseLocalSteamServer = false;

double g_timeAtFadeStart = 0.0;
int g_nBlurSteps = 0;


void LogPlayerEvent( EventType_t e );


enum OSVersion
{
	OSV_95,
	OSV_95OSR2,
	OSV_98,
	OSV_98SE,
	OSV_ME,
	OSV_NT4,
	OSV_2000,
	OSV_SERVER2003,
	OSV_XP,
	OSV_XPSP1,
	OSV_XPSP2,
	OSV_XPSP3,
	OSV_VISTA,
	OSV_UNKNOWN,
};

OSVersion DetectOSVersion()
{
	OSVERSIONINFO version;
	version.dwOSVersionInfoSize = sizeof( version );
	GetVersionEx( &version );
	if ( version.dwPlatformId & VER_PLATFORM_WIN32_WINDOWS )
	{
		if ( version.dwMajorVersion != 4 )
			return OSV_UNKNOWN;

		switch ( version.dwMinorVersion )
		{
		case 0:
			return strstr( version.szCSDVersion, _TEXT( " C" ) ) == version.szCSDVersion ? OSV_95OSR2 : OSV_95;
		case 10:
			return ( strstr( version.szCSDVersion, _TEXT( " A" ) ) == version.szCSDVersion ) || ( strstr( version.szCSDVersion, _TEXT( " B" ) ) == version.szCSDVersion ) ? OSV_98SE : OSV_98;
		case 90:
			return OSV_ME;
		}
	}
	else if ( version.dwPlatformId & VER_PLATFORM_WIN32_NT )
	{
		if ( version.dwMajorVersion == 4 )
			return OSV_NT4; // or mabye NT3.5???
		
		if ( version.dwMajorVersion == 6 )
			return OSV_VISTA;

		if ( version.dwMajorVersion != 5 )
			return OSV_UNKNOWN;

		switch ( version.dwMinorVersion )
		{
		case 0:
			return OSV_2000;
		case 1:
			{
				if ( version.szCSDVersion == NULL )
					return OSV_XP;

				if ( strstr( version.szCSDVersion, _TEXT( "Service Pack 1" ) ) != NULL )
					return OSV_XPSP1;

				if ( strstr( version.szCSDVersion, _TEXT( "Service Pack 2" ) ) != NULL )
					return OSV_XPSP2;

				if ( strstr( version.szCSDVersion, _TEXT( "Service Pack 3" ) ) != NULL )
					return OSV_XPSP3;
			}
		case 2:
			return OSV_SERVER2003; // or maybe XP64???
		}
	}

	return OSV_UNKNOWN;
}

const OSVersion g_osVersion = DetectOSVersion();



#define BLUR

#define USE_D3D8
#ifdef USE_D3D8

#include <d3d8.h>
IDirect3D8*				g_pD3D = NULL;
IDirect3DDevice8*		g_pd3dDevice = NULL;
IDirect3DVertexBuffer8*	g_pDrawVB = NULL;
IDirect3DTexture8*		g_pImg = NULL;
int						g_nDrawStride = 0;
DWORD					g_dwDrawFVF = 0;

#ifdef BLUR

int						g_nBlurStride = 0;
DWORD					g_dwBlurFVF = 0;
IDirect3DVertexBuffer8*	g_pBlurVB = NULL;
IDirect3DTexture8*		g_pTex = NULL;
IDirect3DTexture8*		g_pRT = NULL;
IDirect3DSurface8*		g_pBackBuf = NULL;

#endif // BLUR

#endif // USE_D3D8


DWORD g_dwUseVMROverlayOldValue = 0;
bool g_bUseVMROverlayValueExists = false;


void SetRegistryValue( const char *pKeyName, const char *pValueName, DWORD dwValue, DWORD &dwOldValue, bool &bValueExisted )
{
	HKEY hKey = 0;
	LONG rval = RegCreateKeyEx( HKEY_CURRENT_USER, pKeyName, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL );
	if ( rval != ERROR_SUCCESS )
	{
		OutputDebugString( "unable to open registry key: " );
		OutputDebugString( pKeyName );
		OutputDebugString( "\n" );
		return;
	}

	DWORD dwType = 0;
	DWORD dwSize = sizeof( dwOldValue );

	// amusingly enough, if pValueName doesn't exist, RegQueryValueEx returns ERROR_FILE_NOT_FOUND
	rval = RegQueryValueEx( hKey, pValueName, NULL, &dwType, ( LPBYTE )&dwOldValue, &dwSize );
	bValueExisted = ( rval == ERROR_SUCCESS );

	rval = RegSetValueEx( hKey, pValueName, 0, REG_DWORD, ( CONST BYTE* )&dwValue, sizeof( dwValue ) );
	if ( rval != ERROR_SUCCESS )
	{
		OutputDebugString( "unable to write registry value " );
		OutputDebugString( pValueName );
		OutputDebugString( " in key " );
		OutputDebugString( pKeyName );
		OutputDebugString( "\n" );
	}

	RegCloseKey( hKey );
}

void RestoreRegistryValue( const char *pKeyName, const char *pValueName, DWORD dwOldValue, bool bValueExisted )
{
	HKEY hKey = 0;
	LONG rval = RegCreateKeyEx( HKEY_CURRENT_USER, pKeyName, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL );
	if ( rval != ERROR_SUCCESS )
	{
		OutputDebugString( "unable to open registry key: " );
		OutputDebugString( pKeyName );
		OutputDebugString( "\n" );
		return;
	}

	if ( bValueExisted )
	{
		rval = RegSetValueEx( hKey, pValueName, 0, REG_DWORD, ( CONST BYTE* )&dwOldValue, sizeof( dwOldValue ) );
	}
	else
	{
		rval = RegDeleteValue( hKey, pValueName );
	}
	if ( rval != ERROR_SUCCESS )
	{
		OutputDebugString( "SetRegistryValue FAILED!\n" );
	}

	RegCloseKey( hKey );
}

bool GetRegistryString( const char *pKeyName, const char *pValueName, const char *pValueString, int nValueLen )
{
	HKEY hKey = 0;
	LONG rval = RegCreateKeyEx( HKEY_CURRENT_USER, pKeyName, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL );
	if ( rval != ERROR_SUCCESS )
	{
		OutputDebugString( "unable to open registry key: " );
		OutputDebugString( pKeyName );
		OutputDebugString( "\n" );
		return false;
	}

	DWORD dwType = 0;
	rval = RegQueryValueEx( hKey, pValueName, NULL, &dwType, ( LPBYTE )pValueString, ( DWORD* )&nValueLen );

	RegCloseKey( hKey );

	if ( rval != ERROR_SUCCESS || dwType != REG_SZ )
	{
		OutputDebugString( "unable to read registry string: " );
		OutputDebugString( pValueName );
		OutputDebugString( "\n" );
		return false;
	}

	return true;
}

struct EventData_t
{
	EventData_t( int t, float pos, EventType_t e )
		: time( t ), position( pos ), event( e )
	{
	}
	int time;			// real time
	float position;		// movie position
	EventType_t event;	// event type
};

const char *GetEventName( EventType_t event )
{
	switch ( event )
	{
	case ET_APPLAUNCH:	return "al";
	case ET_APPEXIT:	return "ae";
	case ET_CLOSE:		return "cl";
	case ET_FADEOUT:	return "fo";

	case ET_MEDIABEGIN:	return "mb";
	case ET_MEDIAEND:	return "me";

	case ET_JUMPHOME:	return "jh";
	case ET_JUMPEND:	return "je";

	case ET_PLAY:		return "pl";
	case ET_PAUSE:		return "ps";
	case ET_STOP:		return "st";
	case ET_SCRUBFROM:	return "jf";
	case ET_SCRUBTO:	return "jt";
	case ET_STEPFWD:	return "sf";
	case ET_STEPBCK:	return "sb";
	case ET_JUMPFWD:	return "jf";
	case ET_JUMPBCK:	return "jb";
	case ET_REPEAT:		return "rp";

	case ET_MAXIMIZE:	return "mx";
	case ET_MINIMIZE:	return "mn";
	case ET_RESTORE:	return "rs";

	default: return "<unknown>";
	}
}

std::vector< EventData_t > g_events;

void LogPlayerEvent( EventType_t e, float pos )
{
	if ( !g_bReportStats )
		return;

	static int s_firstTick = GetTickCount();
	int time = GetTickCount() - s_firstTick;

#if 0
	char msg[ 256 ];
	sprintf( msg, "event %s at time %d and pos %d\n", GetEventName( e ), time, int( 1000 * pos ) );
	OutputDebugString( msg );
#endif

	bool bDropEvent = false;

	int nEvents = g_events.size();
	if ( ( e == ET_STEPFWD || e == ET_STEPBCK ) && nEvents >= 2 )
	{
		const EventData_t &e1 = g_events[ nEvents - 1 ];
		const EventData_t &e2 = g_events[ nEvents - 2 ];
		if ( ( e1.event == e || e1.event == ET_REPEAT ) && e2.event == e )
		{
			// only store starting and ending stepfwd or stepbck events, since there can be so many
			// also keep events that are more than a second apart
			if ( e1.event == ET_REPEAT )
			{
				// keep dropping events while e1 isn't before a gap
				bDropEvent = time - e1.time < 1000;
			}
			else
			{
				// e2 was kept last time, so keep e1 if e2 was kept because it was before a gap
				bDropEvent = e1.time - e2.time < 1000;
			}
		}
	}

	if ( bDropEvent )
	{
		g_events[ nEvents - 1 ] = EventData_t( time, pos, ET_REPEAT );
	}
	else
	{
		g_events.push_back( EventData_t( time, pos, e ) );
	}
}

char C2M_UPLOADDATA						= 'q';
char C2M_UPLOADDATA_PROTOCOL_VERSION	= 1;
char C2M_UPLOADDATA_DATA_VERSION		= 1;

inline void WriteHexDigit( std::ostream &os, byte src )
{
	os.put( ( src <= 9 ) ? src + '0' : src - 10 + 'A' );
}

inline void WriteByte( std::ostream &os, byte src )
{
	WriteHexDigit( os, src >> 4 );
	WriteHexDigit( os, src & 0xf );
}

inline void WriteShort( std::ostream &os, unsigned short src )
{
	WriteByte( os, ( byte )( ( src >> 8 ) & 0xff ) );
	WriteByte( os, ( byte )( src & 0xff ) );
}

inline void WriteInt24( std::ostream &os, int src )
{
	WriteByte( os, ( byte )( ( src >> 16 ) & 0xff ) );
	WriteByte( os, ( byte )( ( src >> 8 ) & 0xff ) );
	WriteByte( os, ( byte )( src & 0xff ) );
}

inline void WriteInt( std::ostream &os, int src )
{
	WriteByte( os, ( byte )( ( src >> 24 ) & 0xff ) );
	WriteByte( os, ( byte )( ( src >> 16 ) & 0xff ) );
	WriteByte( os, ( byte )( ( src >> 8 ) & 0xff ) );
	WriteByte( os, ( byte )( src & 0xff ) );
}

inline void WriteFloat( std::ostream &os, float src )
{
	WriteInt( os, *( int* )&src );
}

void WriteUUID( std::ostream &os, const UUID &uuid )
{
	WriteInt( os, uuid.Data1 );
	WriteShort( os, uuid.Data2 );
	WriteShort( os, uuid.Data3 );
	for ( int i = 0; i < 8; ++i )
	{
		WriteByte( os, uuid.Data4[ i ] );
	}
}

bool QueryOrGenerateUserID( UUID &userId )
{
	HKEY hKey = 0;
	LONG rval = RegCreateKeyEx( HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL );
	if ( rval != ERROR_SUCCESS )
	{
		UuidCreate( &userId );
		return false;
	}

	DWORD dwType = 0;
	unsigned char idstr[ 40 ];
	DWORD dwSize = sizeof( idstr );

	rval = RegQueryValueEx( hKey, "smpid", NULL, &dwType, ( LPBYTE )idstr, &dwSize );
	if ( rval != ERROR_SUCCESS || dwType != REG_SZ )
	{
		UuidCreate( &userId );

		unsigned char *outstring = NULL;
		UuidToString( &userId, &outstring );
		if ( outstring == NULL || *outstring == '\0' )
		{
			RegCloseKey( hKey );
			return false;
		}

		rval = RegSetValueEx( hKey, "smpid", 0, REG_SZ, ( CONST BYTE* )outstring, sizeof( idstr ) );

		RpcStringFree( &outstring );
		RegCloseKey( hKey );

		return rval == ERROR_SUCCESS;
	}

	if ( RPC_S_OK != UuidFromString( idstr, &userId ) )
		return false;

	RegCloseKey( hKey );
	return true;
}

void PrintStats( const char *pStatsFilename )
{
	std::ofstream os( pStatsFilename, std::ios_base::out | std::ios_base::binary );

	// user id
	UUID userId;
	QueryOrGenerateUserID( userId );
	unsigned char *userIdStr;
	UuidToStringA( &userId, &userIdStr );
	os << userIdStr << "\n";
	RpcStringFree( &userIdStr );

	// filename
	int nOffset = g_URL.find_last_of( "/\\" );
	if ( nOffset == g_URL.npos )
		nOffset = 0;
	std::string filename = g_URL.substr( nOffset + 1 );
	os << filename << '\n';

	// number of events
	int nEvents = g_events.size();
	os << nEvents << "\n";

	// event data (tab-delimited)
	for ( int i = 0; i < nEvents; ++i )
	{
		os << GetEventName( g_events[ i ].event ) << "\t";
		os << g_events[ i ].time << "\t";
		os << int( 1000 * g_events[ i ].position ) << "\n";
	}
}

void UploadStats()
{
	char pathname[ 256 ];
	if ( !GetRegistryString( "Software\\Valve\\Steam", "SteamExe", pathname, sizeof( pathname ) ) )
		return;

	char *pExeName = strrchr( pathname, '/' );
	if ( !pExeName )
		return;

	*pExeName = '\0'; // truncate exe filename to just pathname

	char filename[ 256 ];
	sprintf( filename, "%s/smpstats.txt", pathname );

	PrintStats( filename );

	::ShellExecuteA( NULL, "open", "steam://smp/smpstats.txt", NULL, NULL, SW_SHOWNORMAL );
}

void RestoreRegistry()
{
	static bool s_bDone = false;
	if ( s_bDone )
		return;

	s_bDone = true;
	RestoreRegistryValue( "Software\\Microsoft\\MediaPlayer\\Preferences\\VideoSettings", "UseVMROverlay", g_dwUseVMROverlayOldValue, g_bUseVMROverlayValueExists );
}


#ifdef USE_D3D8

void CleanupD3D()
{
	if ( g_pDrawVB )
	{
		g_pDrawVB->Release();
		g_pDrawVB = NULL;
	}
	if ( g_pImg )
	{
		g_pImg->Release();
		g_pImg = NULL;
	}
#ifdef BLUR
	if ( g_pBackBuf )
	{
		g_pBackBuf->Release();
		g_pBackBuf = NULL;
	}
	if ( g_pBlurVB )
	{
		g_pBlurVB->Release();
		g_pBlurVB = NULL;
	}
	if ( g_pTex )
	{
		g_pTex->Release();
		g_pTex = NULL;
	}
	if ( g_pRT )
	{
		g_pRT->Release();
		g_pRT = NULL;
	}
#endif // BLUR
	if ( g_pd3dDevice )
	{
		g_pd3dDevice->Release();
		g_pd3dDevice = NULL;
	}
	if ( g_pD3D )
	{
		g_pD3D->Release();
		g_pD3D = NULL;
	}
}

void InitTextureStageState( int nStage, DWORD dwColorOp, DWORD dwColorArg1, DWORD dwColorArg2, DWORD dwColorArg0 = D3DTA_CURRENT )
{
	g_pd3dDevice->SetTextureStageState( nStage, D3DTSS_COLOROP,   dwColorOp );
	g_pd3dDevice->SetTextureStageState( nStage, D3DTSS_COLORARG1, dwColorArg1 );
	g_pd3dDevice->SetTextureStageState( nStage, D3DTSS_COLORARG2, dwColorArg2 );
	g_pd3dDevice->SetTextureStageState( nStage, D3DTSS_COLORARG0, dwColorArg0 );
	g_pd3dDevice->SetTextureStageState( nStage, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
	g_pd3dDevice->SetTextureStageState( nStage, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
	g_pd3dDevice->SetTextureStageState( nStage, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
	g_pd3dDevice->SetTextureStageState( nStage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
	g_pd3dDevice->SetTextureStageState( nStage, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
}

bool InitD3D( HWND hWnd, bool blur )
{
	g_pD3D = Direct3DCreate8( D3D_SDK_VERSION );
	if ( !g_pD3D )
	{
		OutputDebugString( "Direct3DCreate8 FAILED!\n" );
		CleanupD3D();
		return false;
	}

	D3DDISPLAYMODE d3ddm;
	bool bFound = false;
	int nAdapters = g_pD3D->GetAdapterCount();
	int nAdapterIndex = 0;
	for ( ; nAdapterIndex < nAdapters; ++nAdapterIndex )
	{
		if ( g_pD3D->GetAdapterMonitor( nAdapterIndex ) == g_hMonitor )
		{
			if ( FAILED( g_pD3D->GetAdapterDisplayMode( nAdapterIndex, &d3ddm ) ) )
			{
				OutputDebugString( "GetAdapterDisplayMode FAILED!\n" );
				CleanupD3D();
				return false;
			}

			MONITORINFO mi;
			mi.cbSize = sizeof( mi );
			GetMonitorInfo( g_hMonitor, &mi );
			bFound = true;
			break;
		}
	}
	if ( !bFound )
	{
		OutputDebugString( "Starting monitor not found when creating D3D device!\n" );
		CleanupD3D();
		return false;
	}

	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory( &d3dpp, sizeof( d3dpp ) );
	d3dpp.BackBufferWidth	= g_screenWidth;
	d3dpp.BackBufferHeight	= g_screenHeight;
	d3dpp.BackBufferFormat	= d3ddm.Format;
	d3dpp.BackBufferCount	= 1;
	d3dpp.MultiSampleType	= D3DMULTISAMPLE_NONE;
	d3dpp.SwapEffect		= D3DSWAPEFFECT_DISCARD;
	d3dpp.hDeviceWindow		= NULL;
	d3dpp.Windowed			= FALSE;
	d3dpp.FullScreen_RefreshRateInHz		= D3DPRESENT_RATE_DEFAULT;
	d3dpp.FullScreen_PresentationInterval	= D3DPRESENT_INTERVAL_ONE;

	if ( FAILED( g_pD3D->CreateDevice( nAdapterIndex, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &g_pd3dDevice ) ) )
	{
		OutputDebugString( "CreateDevice FAILED!\n" );
		CleanupD3D();
		return false;
	}

	// create and fill vertex buffer(s)
	float du = 0.5f / g_screenWidth;
	float dv = 0.5f / g_screenHeight;
	float u0 = du;
	float u1 = 1.0f + du;
	float v0 = dv;
	float v1 = 1.0f + dv;
	float drawverts[] =
	{ // x, y, z, u, v
		-1, -1, 0, u0, v0,
		-1,  1, 0, u0, v1,
		 1, -1, 0, u1, v0,
		 1,  1, 0, u1, v1,
	};
	g_dwDrawFVF = D3DFVF_XYZ | D3DFVF_TEX1;
	g_nDrawStride = sizeof( drawverts ) / 4;

	if ( FAILED( g_pd3dDevice->CreateVertexBuffer( sizeof( drawverts ), D3DUSAGE_WRITEONLY, g_dwDrawFVF, D3DPOOL_MANAGED, &g_pDrawVB ) ) )
	{
		OutputDebugString( "CreateVertexBuffer( g_pDrawVB ) FAILED!\n" );
		CleanupD3D();
		return false;
	}

	BYTE* pDrawVBMem;
	if ( FAILED( g_pDrawVB->Lock( 0, sizeof( drawverts ), &pDrawVBMem, 0 ) ) )
	{
		OutputDebugString( "g_pDrawVB->Lock FAILED!\n" );
		CleanupD3D();
		return false;
	}
	memcpy( pDrawVBMem, drawverts, sizeof( drawverts ) );
	g_pDrawVB->Unlock();

	g_pd3dDevice->SetStreamSource( 0, g_pDrawVB, g_nDrawStride );
	g_pd3dDevice->SetVertexShader( g_dwDrawFVF );
#ifdef BLUR
	if ( blur )
	{
		float f = 2.0f / ( 2.0f + sqrt( 2.0f ) );
		float ds = 2.0f * f / g_screenWidth;
		float dt = 2.0f * f / g_screenHeight;
		float s0 = ( 0.5f - f ) / g_screenWidth;
		float s1 = 1.0f + s0;
		float t0 = ( 0.5f - f ) / g_screenHeight;
		float t1 = 1.0f + t0;
		float blurverts[] =
		{ // x, y, z, u, v
			-1, -1, 0, s0, t1, s0+ds, t1, s0, t1+dt, s0+ds, t1+dt,
			-1,  1, 0, s0, t0, s0+ds, t0, s0, t0+dt, s0+ds, t0+dt,
			1, -1, 0, s1, t1, s1+ds, t1, s1, t1+dt, s1+ds, t1+dt,
			1,  1, 0, s1, t0, s1+ds, t0, s1, t0+dt, s1+ds, t0+dt,
		};
		g_dwBlurFVF = D3DFVF_XYZ | D3DFVF_TEX4;
		g_nBlurStride = sizeof( blurverts ) / 4;

		if ( FAILED( g_pd3dDevice->CreateVertexBuffer( sizeof( blurverts ), D3DUSAGE_WRITEONLY, g_dwBlurFVF, D3DPOOL_MANAGED, &g_pBlurVB ) ) )
		{
			OutputDebugString( "CreateVertexBuffer( g_pBlurVB ) FAILED!\n" );
			CleanupD3D();
			return false;
		}

		BYTE* pBlurVBMem;
		if ( FAILED( g_pBlurVB->Lock( 0, sizeof( blurverts ), &pBlurVBMem, 0 ) ) )
		{
			OutputDebugString( "g_pBlurVB->Lock FAILED!\n" );
			CleanupD3D();
			return false;
		}
		memcpy( pBlurVBMem, blurverts, sizeof( blurverts ) );
		g_pBlurVB->Unlock();
	}
#endif // BLUR

	// create and fill texture
	if ( FAILED( g_pd3dDevice->CreateTexture( g_screenWidth, g_screenHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_pImg ) ) )
	{
		OutputDebugString( "CreateTexture( g_pImg ) FAILED!\n" );
		CleanupD3D();
		return false;
	}

	D3DLOCKED_RECT lockedRect;
	if ( FAILED( g_pImg->LockRect( 0, &lockedRect, NULL, 0 ) ) )
	{
		OutputDebugString( "g_pImg->LockRect FAILED!\n" );
		CleanupD3D();
		return false;
	}

	BITMAPINFO bitmapInfo;
	bitmapInfo.bmiHeader.biSize          = sizeof( bitmapInfo.bmiHeader );
	bitmapInfo.bmiHeader.biWidth         = g_screenWidth;
	bitmapInfo.bmiHeader.biHeight        = g_screenHeight;
	bitmapInfo.bmiHeader.biPlanes        = 1;
	bitmapInfo.bmiHeader.biBitCount      = 32;
	bitmapInfo.bmiHeader.biCompression   = BI_RGB;
	bitmapInfo.bmiHeader.biSizeImage     = 0;
	bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biClrUsed       = 0;
	bitmapInfo.bmiHeader.biClrImportant  = 0;

	if ( GetDIBits( g_hdcCapture, g_hbmCapture, 0, g_screenHeight, lockedRect.pBits, &bitmapInfo, DIB_RGB_COLORS ) != g_screenHeight )
	{
		OutputDebugString( "GetDIBits FAILED to get the full image!\n" );
	}

	g_pImg->UnlockRect( 0 );

#ifdef BLUR
	if ( blur )
	{
		if ( FAILED( g_pd3dDevice->CreateTexture( g_screenWidth, g_screenHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_pTex ) ) )
		{
			OutputDebugString( "CreateTexture( g_pTex ) FAILED!\n" );
			CleanupD3D();
			return false;
		}

		if ( FAILED( g_pd3dDevice->CreateTexture( g_screenWidth, g_screenHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_pRT ) ) )
		{
			OutputDebugString( "CreateTexture( g_pRT ) FAILED!\n" );
			CleanupD3D();
			return false;
		}

		IDirect3DSurface8 *pTexSurf = NULL;
		g_pTex->GetSurfaceLevel( 0, &pTexSurf );
		IDirect3DSurface8 *pImgSurf = NULL;
		g_pImg->GetSurfaceLevel( 0, &pImgSurf );

		RECT rect = { 0, 0, g_screenWidth, g_screenHeight };
		POINT pt = { 0,  0 };
		g_pd3dDevice->CopyRects( pImgSurf, &rect, 1, pTexSurf, &pt );

		pTexSurf->Release();
		pImgSurf->Release();
	}
#endif // BLUR

	g_pd3dDevice->SetTexture( 0, g_pImg );

	InitTextureStageState( 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_TFACTOR );
#ifdef BLUR
	if ( blur )
	{
		g_pd3dDevice->SetTexture( 0, g_pTex );
//		g_pd3dDevice->SetTexture( 1, g_pTex );
//		g_pd3dDevice->SetTexture( 2, g_pTex );
//		g_pd3dDevice->SetTexture( 3, g_pTex );

		DWORD op = D3DTOP_DISABLE; // D3DTOP_MULTIPLYADD;
		InitTextureStageState( 1, op, D3DTA_TEXTURE, D3DTA_TFACTOR, D3DTA_CURRENT );
		InitTextureStageState( 2, op, D3DTA_TEXTURE, D3DTA_TFACTOR, D3DTA_CURRENT );
		InitTextureStageState( 3, op, D3DTA_TEXTURE, D3DTA_TFACTOR, D3DTA_CURRENT );
	}
#endif // BLUR

	return true;
}

void DrawD3DFade( BYTE fade, bool blur )
{
	if ( g_pd3dDevice )
	{
#ifdef BLUR
		if ( g_pTex )
		{
			if ( blur )
			{
				IDirect3DSurface8 *pRTSurf = NULL;
				g_pRT->GetSurfaceLevel( 0, &pRTSurf );
				g_pd3dDevice->SetRenderTarget( pRTSurf, NULL );

				if ( g_pBackBuf )
				{
					g_pBackBuf->Release();
					g_pBackBuf = NULL;
				}

				g_pd3dDevice->BeginScene();

				g_pd3dDevice->SetTexture( 0, g_pTex );
				g_pd3dDevice->SetTexture( 1, g_pTex );
				g_pd3dDevice->SetTexture( 2, g_pTex );
				g_pd3dDevice->SetTexture( 3, g_pTex );

				g_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_MULTIPLYADD );
				g_pd3dDevice->SetTextureStageState( 2, D3DTSS_COLOROP,   D3DTOP_MULTIPLYADD );
				g_pd3dDevice->SetTextureStageState( 3, D3DTSS_COLOROP,   D3DTOP_MULTIPLYADD );

				g_pd3dDevice->SetStreamSource( 0, g_pBlurVB, g_nBlurStride );
				g_pd3dDevice->SetVertexShader( g_dwBlurFVF );

				DWORD quarter = 0x3f | ( 0x3f << 8 ) | ( 0x3f << 16 ) | ( 0x3f << 24 );
				g_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, quarter );
				g_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

				g_pd3dDevice->EndScene();

				pRTSurf->Release();

				g_pd3dDevice->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &g_pBackBuf );
				g_pd3dDevice->SetRenderTarget( g_pBackBuf, NULL );

				IDirect3DTexture8 *pTemp = g_pTex;
				g_pTex = g_pRT;
				g_pRT = pTemp;

				g_pd3dDevice->SetTexture( 0, g_pTex );
				g_pd3dDevice->SetTexture( 1, NULL );
				g_pd3dDevice->SetTexture( 2, NULL );
				g_pd3dDevice->SetTexture( 3, NULL );

				g_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
				g_pd3dDevice->SetTextureStageState( 2, D3DTSS_COLOROP,   D3DTOP_DISABLE );
				g_pd3dDevice->SetTextureStageState( 3, D3DTSS_COLOROP,   D3DTOP_DISABLE );

				g_pd3dDevice->SetStreamSource( 0, g_pDrawVB, g_nDrawStride );
				g_pd3dDevice->SetVertexShader( g_dwDrawFVF );
			}
		}
#endif

		g_pd3dDevice->BeginScene();

		//					DWORD factor = 0xff | ( fade << 8 ) | ( fade << 16 ) | ( fade << 24 );
		DWORD factor = fade | ( fade << 8 ) | ( fade << 16 ) | ( fade << 24 );
		g_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, factor );
		g_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

		g_pd3dDevice->EndScene();
		g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
	}
}

#endif // USE_D3D8

int g_nTimingIndex = 0;
double g_timings[ 65536 ];

LRESULT CALLBACK WinProc( HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
	switch ( msg )
	{
	case WM_CREATE:
		{
			g_timeAtFadeStart = 0.0;
			g_nBlurSteps = 0;

			g_hBlackFadingWindow = hWnd;

#ifndef USE_D3D8
			MONITORINFO mi;
			mi.cbSize = sizeof( mi );
			if ( GetMonitorInfo( g_hMonitor, &mi ) )
			{
				SetWindowPos( hWnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, 0 );
			}
#endif

#ifdef USE_D3D8
			InitD3D( hWnd, true );
#endif // USE_D3D8
			if ( !g_bFadeIn )
			{
				g_pFrame->ShowWindow( SW_HIDE );
				g_pFrame->PostMessage( WM_DESTROY, 0, 0 );
			}

			InvalidateRect( hWnd, NULL, TRUE );

			SetTimer( hWnd, ID_SKIP_FADE_TIMER, 1000, NULL ); // if the fade doesn't start in 1 second, then just jump to the video
			SetTimer( hWnd, ID_DRAW_TIMER, 10, NULL ); // draw timer
		}
		break;

#ifdef USE_D3D8
	case WM_TIMER: // WM_ERASEBKGND:
#else
	case WM_TIMER: // WM_ERASEBKGND:
//	case WM_NCPAINT:
#endif
		if ( wparam == ID_DRAW_TIMER )
		{
			static LARGE_INTEGER s_nPerformanceFrequency;
			if ( g_timeAtFadeStart == 0.0 )
			{
				LARGE_INTEGER nTimeAtFadeStart;
				QueryPerformanceCounter( &nTimeAtFadeStart );
				QueryPerformanceFrequency( &s_nPerformanceFrequency );
				g_timeAtFadeStart = nTimeAtFadeStart.QuadPart / double( s_nPerformanceFrequency.QuadPart );

				KillTimer( hWnd, ID_SKIP_FADE_TIMER );
				SetTimer( hWnd, ID_SKIP_FADE_TIMER, 100 + UINT( FADE_TIME * 1000 ), NULL ); // restart skip fade timer and give it an extra 100ms to allow the fade to draw fully black once
			}

			LARGE_INTEGER time;
			QueryPerformanceCounter( &time );
			g_timings[ g_nTimingIndex++ ] = time.QuadPart / double( s_nPerformanceFrequency.QuadPart );
			float dt = ( float )( time.QuadPart / double( s_nPerformanceFrequency.QuadPart ) - g_timeAtFadeStart );

			bool bFadeFinished = dt >= FADE_TIME;
			float fraction = bFadeFinished ? 1.0f : dt / FADE_TIME;
/*
			char str[ 256 ];
			sprintf( str, "A - dt = %f\t time = %f \tfade_finished = %s\n", dt, g_timings[ g_nTimingIndex - 1 ], bFadeFinished ? "true" : "false" );
			OutputDebugString( str );
*/
			bool blur = g_bFadeIn && ( int( fraction * MAX_BLUR_STEPS ) > g_nBlurSteps );
			if ( blur )
			{
				++g_nBlurSteps;
			}

			BYTE fade = BYTE( fraction * 255.999f );
			if ( g_bFadeIn )
			{
				fade = 255 - fade;
			}
/*
			char str[ 256 ];
			sprintf( str, "fade = %d\n", fade );
			OutputDebugString( str );
*/
#ifdef USE_D3D8
			DrawD3DFade( fade, blur );
#else // USE_D3D8
//				HDC hdc = GetDCEx( hWnd, ( HRGN )wparam, DCX_WINDOW | DCX_INTERSECTRGN );

			if ( !PatBlt( g_hdcBlend, 0, 0, g_screenWidth, g_screenHeight, BLACKNESS ) )
			{
				OutputDebugString( "PatBlt FAILED!\n" );
			}

//				fade = 128;
			BLENDFUNCTION blendfunc = { AC_SRC_OVER, 0, fade, 0 };
			if ( !::AlphaBlend( g_hdcBlend, 0, 0, g_screenWidth, g_screenHeight, g_hdcCapture, 0, 0, g_screenWidth, g_screenHeight, blendfunc ) )
			{
				OutputDebugString( "AlphaBlend FAILED!\n" );
			}

			if ( !BitBlt( ( HDC )wparam, 0, 0, g_screenWidth, g_screenHeight, g_hdcBlend, 0, 0, SRCCOPY ) )
			{
				OutputDebugString( "BitBlt FAILED!\n" );
			}

//			ReleaseDC( hWnd, hdc );
//			RedrawWindow( hWnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE );
#endif // USE_D3D8
/*
//			char str[ 256 ];
			sprintf( str, "B - dt = %f\t time = %f \tfade_finished = %s\n", dt, g_timings[ g_nTimingIndex - 1 ], bFadeFinished ? "true" : "false" );
			OutputDebugString( str );
*/
			if ( !bFadeFinished )
				break;

			// fall-through intentional
//			OutputDebugString( "Fall-through from erase background\n" );
		}

//	case WM_TIMER:
		{
			if ( msg == WM_TIMER )
			{
/*
				char str[ 256 ];
				sprintf( str, "Timer 0x%x triggered for window 0x%x\n", wparam, hWnd );
				OutputDebugString( str );
*/
				if ( wparam == ID_DRAW_TIMER )
				{
//					UpdateWindow( hWnd );
//					InvalidateRect( hWnd, NULL, TRUE );
//					break;
				}
			}

			KillTimer( hWnd, ID_SKIP_FADE_TIMER );
			KillTimer( hWnd, ID_DRAW_TIMER );

			if ( !g_bFadeIn )
			{
//				OutputDebugString( "closing fade window\n" );
				ShowWindow( hWnd, SW_HIDE );
				PostMessage( hWnd, WM_CLOSE, 0, 0 );
				return 1;
			}
			else if ( !g_bFrameCreated )
			{
				g_bFrameCreated = true;
#ifdef USE_D3D8
//				OutputDebugString( "Cleanup D3D\n" );
				CleanupD3D();
#endif
				g_pFrame = &g_frame;
				g_pFrame->GetWndClassInfo().m_wc.hIcon = LoadIcon( _Module.GetResourceInstance(), MAKEINTRESOURCE( IDI_ICON ) );
				RECT rcPos = { CW_USEDEFAULT, 0, 0, 0 };

//				OutputDebugString( "Create WMP frame\n" );

				if ( g_osVersion < OSV_XP )
				{
					g_pFrame->Create( GetDesktopWindow(), rcPos, _T( "Steam Media Player" ), WS_OVERLAPPEDWINDOW, 0, ( UINT )0 );
				}
				else
				{
					g_pFrame->Create( GetDesktopWindow(), rcPos, _T( "Steam Media Player" ), WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, ( UINT )0 );

					g_pFrame->ShowWindow( SW_SHOW );
				}

//				OutputDebugString( "Create WMP frame - done\n" );
			}

			// close WMP window once we paint the fullscreen fade window
			if ( !g_bFadeIn )
			{
				g_pFrame->ShowWindow( SW_HIDE );
			}
		}
		return 1;

	case WM_KEYDOWN:
		if ( wparam == VK_ESCAPE )
		{
			::DestroyWindow( hWnd );
		}
		break;

	case WM_DESTROY:
		g_hBlackFadingWindow = NULL;

#ifdef USE_D3D8
		CleanupD3D();
#endif

		if ( g_bFrameCreated )
		{
			g_bFrameCreated = false;

			g_pFrame->DestroyWindow();
			g_pFrame = NULL;
		}

		::PostQuitMessage( 0 );
		break;
	}

	return DefWindowProc( hWnd, msg, wparam, lparam );
}

bool ShowFadeWindow( bool bShow )
{
	if ( bShow )
	{
		g_timeAtFadeStart = 0.0;
		g_bFadeIn = false;

		SetTimer( g_hBlackFadingWindow, ID_DRAW_TIMER, 10, NULL );

		if ( g_pFrame )
		{
			g_pFrame->ShowWindow( SW_HIDE );
		}
#ifdef USE_D3D8
		if ( g_osVersion < OSV_XP )
		{
			::SetWindowPos( g_hBlackFadingWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
		}
		InitD3D( g_hBlackFadingWindow, false );
#else // USE_D3D8
		::SetWindowPos( g_hBlackFadingWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
//		::ShowWindow( g_hBlackFadingWindow, SW_SHOWMAXIMIZED );
#endif // USE_D3D8
		InvalidateRect( g_hBlackFadingWindow, NULL, TRUE );
	}
	else
	{
		if ( g_osVersion < OSV_XP )
		{
//			OutputDebugString( "hiding fade window\n" );
			ShowWindow( g_hBlackFadingWindow, SW_HIDE );
		}
		else
		{
//			OutputDebugString( "Deferring erase on fade window\n" );
			::SetWindowPos( g_hBlackFadingWindow, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_DEFERERASE );
		}
	}
	return true;
}

HWND CreateFullscreenWindow( bool bFadeIn )
{
	if ( g_hBlackFadingWindow )
		return g_hBlackFadingWindow;

	static s_bRegistered = false;
	if ( !s_bRegistered )
	{
		WNDCLASS wc;
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = ( WNDPROC )WinProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = g_hInstance;
		wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
		wc.hCursor = NULL;
		wc.hbrBackground = NULL;
		wc.lpszMenuName =  NULL;
		wc.lpszClassName = "myclass";
	 
		if ( !RegisterClass( &wc ) ) 
			return 0;

		s_bRegistered = true;
	}

	g_bFadeIn = bFadeIn;
	DWORD windowStyle = WS_POPUP;
#ifndef USE_D3D8
	windowStyle |= WS_MAXIMIZE | WS_EX_TOPMOST | WS_VISIBLE;
#endif
	if ( g_osVersion < OSV_XP )
	{
		windowStyle |= WS_MAXIMIZE | WS_EX_TOPMOST | WS_VISIBLE;
	}

	MONITORINFO mi;
	mi.cbSize = sizeof( mi );
	if ( !GetMonitorInfo( g_hMonitor, &mi ) )
	{
		GetClientRect( GetDesktopWindow(), &mi.rcMonitor );
	}

	g_hBlackFadingWindow = CreateWindow( "myclass", _T( "Steam Media Player" ), windowStyle, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, NULL, NULL, g_hInstance, NULL );
#ifndef USE_D3D8
	ShowWindow( g_hBlackFadingWindow, SW_SHOWMAXIMIZED );
#endif
	if ( g_osVersion < OSV_XP )
	{
		ShowWindow( g_hBlackFadingWindow, SW_SHOWMAXIMIZED );
	}

	while ( ShowCursor( FALSE ) >= 0 )
		;

	return g_hBlackFadingWindow;
}

bool CreateDesktopBitmaps()
{
	MONITORINFOEX mi;
	mi.cbSize = sizeof( mi );
	if ( !GetMonitorInfo( g_hMonitor, &mi ) )
		return false;

	g_screenWidth  = mi.rcMonitor.right - mi.rcMonitor.left;
	g_screenHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;

	HDC hdcScreen = CreateDC( mi.szDevice, mi.szDevice, NULL, NULL );
	if ( !hdcScreen )
		return false;

	g_hdcCapture = CreateCompatibleDC( hdcScreen );
	g_hdcBlend   = CreateCompatibleDC( hdcScreen );
	if ( !g_hdcCapture || !g_hdcBlend )
		return false;

	if ( ( GetDeviceCaps( hdcScreen, SHADEBLENDCAPS ) & SB_CONST_ALPHA ) == 0 )
	{
		OutputDebugString( "display doesn't support AlphaBlend!\n" );
	}

	if ( ( GetDeviceCaps( hdcScreen, RASTERCAPS ) & RC_BITBLT ) == 0 )
	{
		OutputDebugString( "display doesn't support BitBlt!\n" );
	}

	if ( GetDeviceCaps( hdcScreen, BITSPIXEL ) < 32 )
	{
		OutputDebugString( "display doesn't support 32bpp!\n" );
	}

	if ( g_screenWidth != GetDeviceCaps( hdcScreen, HORZRES ) ||
		g_screenHeight != GetDeviceCaps( hdcScreen, VERTRES ) )
	{
		OutputDebugString( "Screen DC size differs from monitor size!\n" );
	}

	g_hbmCapture = CreateCompatibleBitmap( hdcScreen, g_screenWidth, g_screenHeight );
	g_hbmBlend   = CreateCompatibleBitmap( hdcScreen, g_screenWidth, g_screenHeight );
	if ( !g_hbmCapture || !g_hbmBlend )
		return false;

	HGDIOBJ oldCaptureObject = SelectObject( g_hdcCapture, g_hbmCapture );
	HGDIOBJ oldBlendObject   = SelectObject( g_hdcBlend,   g_hbmBlend   );

	if ( !BitBlt( g_hdcCapture, 0, 0, g_screenWidth, g_screenHeight, hdcScreen, 0, 0, SRCCOPY ) )
		return false;

	SelectObject( g_hdcCapture, oldCaptureObject );
	SelectObject( g_hdcBlend,   oldBlendObject   );

	return true;
}

void PrintLastError( const char *pPrefix )
{
#ifdef _DEBUG
	DWORD dw = GetLastError();

	LPVOID lpMsgBuf;
	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, dw, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
		( LPTSTR )&lpMsgBuf, 0, NULL );

	OutputDebugString( pPrefix );
	char msg[ 256 ];
	sprintf( msg, "(%d) ", dw );
	OutputDebugString( msg );
	OutputDebugString( ( char * )lpMsgBuf );

	LocalFree( lpMsgBuf );
#endif
}

void KillOtherSMPs()
{
	DWORD nBytesReturned = 0;
	DWORD procIds[ 1024 ];
	if ( !EnumProcesses( procIds, sizeof( procIds ), &nBytesReturned ) )
	{
		PrintLastError( "EnumProcesses Error: " );
		return;
	}

	DWORD dwCurrentProcessId = GetCurrentProcessId();

	int nProcIds = nBytesReturned / sizeof( DWORD );
	for ( int i = 0; i < nProcIds; ++i )
	{
		if ( procIds[ i ] == dwCurrentProcessId )
			continue;

		if ( procIds[ i ] == 0 ) // system idle process
			continue;

		HANDLE hProcess = OpenProcess( PROCESS_TERMINATE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, procIds[ i ] );
		if ( !hProcess )
		{
			PrintLastError( "OpenProcess Error: " );
			continue;
		}

		HMODULE hMod[ 1 ];
		DWORD cbNeeded;
		if ( !EnumProcessModules( hProcess, hMod, sizeof( hMod ), &cbNeeded ) )
		{
			PrintLastError( "EnumProcessModules Error: " );
			continue;
		}

		char processName[ 1024 ];
		int nChars = GetModuleBaseName( hProcess, hMod[ 0 ], processName, sizeof( processName ) / sizeof( char ) );
		if ( nChars >= sizeof( processName ) )
		{
			PrintLastError( "GetModuleBaseName Error: " );
			continue;
		}

		if ( strcmp( processName, "smp.exe" ) == 0 )
		{
			OutputDebugString( "!!! Killing smp.exe !!!\n" );
			TerminateProcess( hProcess, 0 );
		}

		if ( !CloseHandle( hProcess ) )
		{
			PrintLastError( "CloseHandle Error: " );
			continue;
		}
	}
}

void ParseCommandLine( const char *cmdline, std::vector< std::string > &params )
{
	params.push_back( "" );

	bool quoted = false;
	for ( const char *cp = cmdline; *cp; ++cp )
	{
		if ( *cp == '\"' )
		{
			quoted = !quoted;
		}
		else if ( isspace( *cp ) && !quoted )
		{
			if ( !params.back().empty() )
			{
				params.push_back( "" );
			}
		}
		else
		{
			params.back().push_back( *cp );
		}
	}

	if ( params.back().empty() )
	{
		params.pop_back();
	}
}

/////////////////////////////////////////////////////////////////////////////
//
extern "C" int WINAPI _tWinMain( HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpCmdLine, int /*nShowCmd*/ )
{
	g_hInstance = hInstance;

	KillOtherSMPs();

	g_lpCommandLine = lpCmdLine;
	if ( lpCmdLine == NULL || *lpCmdLine == '\0' )
		return 0;

	std::vector< std::string > params;
	ParseCommandLine( lpCmdLine, params );
	int nParams = params.size();
	for ( int i = 0; i < nParams; ++i )
	{
		if ( params[ i ][ 0 ] == '-' || params[ i ][ 0 ] == '/' )
		{
			const char *pOption = params[ i ].c_str() + 1;
			if ( strcmp( pOption, "reportstats" ) == 0 )
			{
				g_bReportStats = true;
			}
			else if ( strcmp( pOption, "localsteamserver" ) == 0 )
			{
				g_bUseLocalSteamServer = true;
			}
			else if ( strcmp( pOption, "redirect" ) == 0 )
			{
				++i;
				g_redirectTarget = params[ i ];
			}
		}
		else
		{
			g_URL = params[ i ];
		}
	}

	SetRegistryValue( "Software\\Microsoft\\MediaPlayer\\Preferences\\VideoSettings", "UseVMROverlay", 0, g_dwUseVMROverlayOldValue, g_bUseVMROverlayValueExists );
	atexit( RestoreRegistry );

	LogPlayerEvent( ET_APPLAUNCH, 0.0f );

	lpCmdLine = GetCommandLine(); //this line necessary for _ATL_MIN_CRT

	CoInitialize( 0 );
    _Module.Init( ObjectMap, hInstance, &LIBID_ATLLib );

	::InitCommonControls();

	POINT pt;
	GetCursorPos( &pt );
	g_hMonitor = MonitorFromPoint( pt, MONITOR_DEFAULTTONEAREST );

	if ( !CreateDesktopBitmaps() )
	{
		OutputDebugString( "CreateDesktopBitmaps FAILED!\n" );
	}

	ShowCursor( FALSE );
	CreateFullscreenWindow( true );

	MSG msg;
	while ( GetMessage( &msg, 0, 0, 0 ) )
	{
		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}

	LogPlayerEvent( ET_APPEXIT );
	if ( g_bReportStats )
	{
		UploadStats();
	}

	if ( !g_redirectTarget.empty() )
	{
		::ShellExecuteA( NULL, "open", g_redirectTarget.c_str(), NULL, NULL, SW_SHOWNORMAL );
	}

	_Module.Term();
	CoUninitialize();

	RestoreRegistry();

	return 0;
}

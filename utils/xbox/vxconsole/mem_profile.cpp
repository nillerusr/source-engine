//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	MEM_PROFILE.CPP
//
//	Memory Profiling Display
//=====================================================================================//
#include "vxconsole.h"

#define PROFILE_MAXSAMPLES		512
#define PROFILE_MEMORYHEIGHT	100
#define PROFILE_NUMMINORTICKS	3
#define PROFILE_LABELWIDTH		50
#define PROFILE_SCALESTEPS		8
#define PROFILE_MINSCALE		0.2f
#define PROFILE_MAXSCALE		3.0f
#define PROFILE_NUMMINORTICKS	3
#define PROFILE_MAJORTICKMB		16
#define PROFILE_WARNINGMB		10
#define PROFILE_SEVEREMB		5

#define ID_MEMPROFILE				1

HWND		g_memProfile_hWnd;
RECT		g_memProfile_WindowRect;
int			g_memProfile_tickMarks;
int			g_memProfile_colors;
int			g_memProfile_scale;
UINT_PTR	g_memProfile_Timer;
int			g_memProfile_numSamples;
int			g_memProfile_samples[PROFILE_MAXSAMPLES];

//-----------------------------------------------------------------------------
//	MemProfile_SaveConfig
// 
//-----------------------------------------------------------------------------
void MemProfile_SaveConfig()
{
	char			buff[256];
	WINDOWPLACEMENT wp;

	// profile history
	if ( g_memProfile_hWnd )
	{
		memset( &wp, 0, sizeof( wp ) );
		wp.length = sizeof( WINDOWPLACEMENT );
		GetWindowPlacement( g_memProfile_hWnd, &wp );
		g_memProfile_WindowRect = wp.rcNormalPosition;
		sprintf( buff, "%d %d %d %d", wp.rcNormalPosition.left, wp.rcNormalPosition.top, wp.rcNormalPosition.right, wp.rcNormalPosition.bottom );
		Sys_SetRegistryString( "MemProfileWindowRect", buff );
	}

	Sys_SetRegistryInteger( "MemProfileScale", g_memProfile_scale );
	Sys_SetRegistryInteger( "MemProfileTickMarks", g_memProfile_tickMarks );
	Sys_SetRegistryInteger( "MemProfileColors", g_memProfile_colors );
}

//-----------------------------------------------------------------------------
//	MemProfile_LoadConfig	
// 
//-----------------------------------------------------------------------------
void MemProfile_LoadConfig()
{
	int		numArgs;
	char	buff[256];

	// profile history
	Sys_GetRegistryString( "MemProfileWindowRect", buff, "", sizeof( buff ) );
	numArgs = sscanf( buff, "%d %d %d %d", &g_memProfile_WindowRect.left, &g_memProfile_WindowRect.top, &g_memProfile_WindowRect.right, &g_memProfile_WindowRect.bottom );
	if ( numArgs != 4 )
	{
		memset( &g_memProfile_WindowRect, 0, sizeof( g_memProfile_WindowRect ) );
	}

	Sys_GetRegistryInteger( "MemProfileScale", 0, g_memProfile_scale );
	if ( g_memProfile_scale < -PROFILE_SCALESTEPS || g_memProfile_scale > PROFILE_SCALESTEPS )
	{
		g_memProfile_scale = 0;
	}

	Sys_GetRegistryInteger( "MemProfileTickMarks", 1, g_memProfile_tickMarks );
	Sys_GetRegistryInteger( "MemProfileColors", 1, g_memProfile_colors );
}

//-----------------------------------------------------------------------------
//	MemProfile_SetTitle
// 
//-----------------------------------------------------------------------------
void MemProfile_SetTitle()
{
	char	titleBuff[128];

	if ( g_memProfile_hWnd )
	{
		strcpy( titleBuff, "Free Memory Available" );
		if ( g_memProfile_Timer )
		{
			strcat( titleBuff, " [ON]" );
		}

		SetWindowText( g_memProfile_hWnd, titleBuff );
	}
}

//-----------------------------------------------------------------------------
// MemProfile_EnableProfiling
//
//-----------------------------------------------------------------------------
void MemProfile_EnableProfiling( bool bEnable )
{
	if ( !g_memProfile_hWnd )
	{
		return;
	}

	UINT_PTR timer = TIMERID_MEMPROFILE;
	if ( bEnable && !g_memProfile_Timer )
	{
		// run at 10Hz
		g_memProfile_Timer = SetTimer( g_memProfile_hWnd, timer, 100, NULL );
	}
	else if ( !bEnable && g_memProfile_Timer )
	{
		KillTimer( g_memProfile_hWnd, timer );
		g_memProfile_Timer = NULL;
	}
}

//-----------------------------------------------------------------------------
// MemProfile_UpdateWindow
//
//-----------------------------------------------------------------------------
void MemProfile_UpdateWindow()
{
	if ( g_memProfile_hWnd && !IsIconic( g_memProfile_hWnd ) )
	{
		// visible - force a client repaint
		InvalidateRect( g_memProfile_hWnd, NULL, true );
	}
}

//-----------------------------------------------------------------------------
// rc_FreeMemory
//
//-----------------------------------------------------------------------------
int rc_FreeMemory( char* commandPtr )
{
	int	errCode = -1;
	int	freeMemory;

	char *cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
	{
		goto cleanUp;
	}
	sscanf( cmdToken, "%x", &freeMemory );

	g_memProfile_samples[g_memProfile_numSamples % PROFILE_MAXSAMPLES] = freeMemory;
	g_memProfile_numSamples++;

	DebugCommand( "FreeMemory( 0x%8.8x )\n", freeMemory );

	MemProfile_UpdateWindow();

	// success
	errCode = 0;

cleanUp:
	return ( errCode );
}

//-----------------------------------------------------------------------------
//	MemProfile_ZoomIn
// 
//-----------------------------------------------------------------------------
void MemProfile_ZoomIn( int& scale, int numSteps )
{
	scale++;
	if ( scale > numSteps )
	{
		scale = numSteps;
		return;
	}
	MemProfile_UpdateWindow();
}

//-----------------------------------------------------------------------------
//	MemProfile_ZoomOut
// 
//-----------------------------------------------------------------------------
void MemProfile_ZoomOut( int& scale, int numSteps )
{
	scale--;
	if ( scale < -numSteps )
	{
		scale = -numSteps;
		return;
	}
	MemProfile_UpdateWindow();
}

//-----------------------------------------------------------------------------
//	MemProfile_CalcScale
// 
//-----------------------------------------------------------------------------
float MemProfile_CalcScale( int scale, int numSteps, float min, float max )
{
	float t;

	// from integral scale [-numSteps..numSteps] to float scale [min..max]
	t = ( float )( scale + numSteps )/( float )( 2*numSteps );
	t = min + t*( max-min );

	return t;
}

//-----------------------------------------------------------------------------
//	MemProfile_Draw
// 
//-----------------------------------------------------------------------------
void MemProfile_Draw( HDC hdc, RECT* clientRect )
{
	char	labelBuff[128];
	HPEN	hBlackPen;
	HPEN	hPenOld;
	HPEN	hNullPen;
	HPEN	hGreyPen;
	HBRUSH	hColoredBrush;
	HBRUSH	hBrushOld;
	HFONT	hFontOld;
	int		currentSample;
	int		numTicks;
	int		memoryHeight;
	int		windowWidth;
	int		windowHeight;
	int		x;
	int		y;
	int		y0;
	int		i;
	int		j;
	int		h;
	int		numbars;
	RECT	rect;
	float	t;
	float	scale;

	hBlackPen  = CreatePen( PS_SOLID, 1, RGB( 0,0,0 ) );
	hGreyPen   = CreatePen( PS_SOLID, 1, Sys_ColorScale( g_backgroundColor, 0.85f ) );
	hNullPen   = CreatePen( PS_NULL, 0, RGB( 0,0,0 ) );
	hPenOld    = ( HPEN )SelectObject( hdc, hBlackPen );
	hFontOld   = SelectFont( hdc, g_hProportionalFont );

	// zoom
	scale        = MemProfile_CalcScale( g_memProfile_scale, PROFILE_SCALESTEPS, PROFILE_MINSCALE, PROFILE_MAXSCALE );
	memoryHeight = ( int )( PROFILE_MEMORYHEIGHT*scale );
	windowWidth  = clientRect->right-clientRect->left;
	windowHeight = clientRect->bottom-clientRect->top;

	numTicks = windowHeight/memoryHeight + 2;
	if ( numTicks < 0 )
	{
		numTicks = 1;
	}
	else if ( numTicks > 512/PROFILE_MAJORTICKMB + 1 )
	{
		numTicks = 512/PROFILE_MAJORTICKMB + 1;
	}

	SetBkColor( hdc, g_backgroundColor );

	x = 0;
	y = windowHeight;
	for ( i=0; i<numTicks; i++ )
	{
		// major ticks
		SelectObject( hdc, hBlackPen );
		MoveToEx( hdc, 0, y, NULL );
		LineTo( hdc, windowWidth, y );

		if ( g_memProfile_tickMarks )
		{
			// could be very zoomed out, gap must be enough for label, otherwise don't draw
			int gapY = memoryHeight/( PROFILE_NUMMINORTICKS+1 );
			if ( gapY >= 10 )
			{
				// minor ticks
				y0 = y;
				SelectObject( hdc, hGreyPen );
				for ( j=0; j<PROFILE_NUMMINORTICKS; j++ )
				{
					y0 += gapY;
					MoveToEx( hdc, 0, y0, NULL );
					LineTo( hdc, windowWidth, y0 );
				}
			}
		}

		// tick labels
		if ( i )
		{
			rect.left   = windowWidth - 50;
			rect.right  = windowWidth;
			rect.top    = y - 20;
			rect.bottom = y;
			sprintf( labelBuff, "%d MB", i*PROFILE_MAJORTICKMB );
			DrawText( hdc, labelBuff, -1, &rect, DT_RIGHT|DT_SINGLELINE|DT_BOTTOM );
		}

		y -= memoryHeight;
	}

	// vertical bars
	if ( g_memProfile_numSamples )
	{
		SelectObject( hdc, hNullPen );

		numbars = windowWidth-PROFILE_LABELWIDTH;
		currentSample = g_memProfile_numSamples-1;
		for ( x=numbars-1; x>=0; x-=4 )
		{
			float sample = g_memProfile_samples[currentSample % PROFILE_MAXSAMPLES]/( 1024.0f * 1024.0f );

			y = windowHeight;
			t  = sample/(float)PROFILE_MAJORTICKMB;
			h  = ( int )( t * ( float )memoryHeight );
			if ( h )
			{
				if ( h > windowHeight )
					h = windowHeight;
		
				COLORREF barColor;
				if ( sample >= PROFILE_WARNINGMB )
				{
					barColor = RGB( 100, 255, 100 );
				}
				else if ( sample >= PROFILE_SEVEREMB )
				{
					barColor = RGB( 255, 255, 100 );
				}
				else
				{
					barColor = RGB( 255, 0, 0 );
				}

				hColoredBrush = CreateSolidBrush( g_memProfile_colors ? barColor : RGB( 80, 80, 80 ) );
				hBrushOld = ( HBRUSH )SelectObject( hdc, hColoredBrush );

				Rectangle( hdc, x-4, y-h, x, y+1 );
				y -= h;

				SelectObject( hdc, hBrushOld );
				DeleteObject( hColoredBrush );
			}

			currentSample--;
			if ( currentSample < 0 )
			{
				// no data
				break;
			}
		}
	}

	SelectObject( hdc, hFontOld );
	SelectObject( hdc, hPenOld );
	DeleteObject( hBlackPen );
	DeleteObject( hGreyPen );
}

//-----------------------------------------------------------------------------
// MemProfile_TimerProc
//
//-----------------------------------------------------------------------------
void MemProfile_TimerProc( HWND hwnd, UINT_PTR idEvent )
{
	static bool busy = false;

	if ( busy )
	{
		return;
	}

	busy = true;

	if ( g_connectedToApp )
	{
		// send as async
		DmAPI_SendCommand( VXCONSOLE_COMMAND_PREFIX "!" "__memory__ quiet", false );
	}

	busy = false;
}

//-----------------------------------------------------------------------------
//	MemProfile_WndProc
// 
//-----------------------------------------------------------------------------
LRESULT CALLBACK MemProfile_WndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	WORD			wID = LOWORD( wParam );
	HDC				hdc;
	PAINTSTRUCT		ps; 
	RECT			rect;
	CREATESTRUCT	*createStructPtr;

	switch ( message )
	{
	case WM_CREATE:
		// set the window identifier
		createStructPtr = ( CREATESTRUCT* )lParam;
		SetWindowLong( hwnd, GWL_USERDATA+0, ( LONG )createStructPtr->lpCreateParams );

		// clear samples
		g_memProfile_numSamples = 0;
		memset( g_memProfile_samples, 0, sizeof( g_memProfile_samples ) );
		return 0L;

	case WM_DESTROY:
		MemProfile_SaveConfig();
		MemProfile_EnableProfiling( false );
		g_memProfile_hWnd = NULL;
		return 0L;

	case WM_INITMENU:
		CheckMenuItem( ( HMENU )wParam, IDM_MEMPROFILE_TICKMARKS, MF_BYCOMMAND | ( g_memProfile_tickMarks ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( ( HMENU )wParam, IDM_MEMPROFILE_COLORS, MF_BYCOMMAND | ( g_memProfile_colors ? MF_CHECKED : MF_UNCHECKED ) );
		CheckMenuItem( ( HMENU )wParam, IDM_MEMPROFILE_ENABLE, MF_BYCOMMAND | ( g_memProfile_Timer != NULL ? MF_CHECKED : MF_UNCHECKED ) );
		return 0L;

	case WM_PAINT:
		GetClientRect( hwnd, &rect );
		hdc = BeginPaint( hwnd, &ps ); 
		MemProfile_Draw( hdc, &rect );
		EndPaint( hwnd, &ps ); 
		return 0L;

	case WM_SIZE:
		// force a redraw
		MemProfile_UpdateWindow();
		return 0L;

	case WM_TIMER:
		if ( wID == TIMERID_MEMPROFILE )
		{
			MemProfile_TimerProc( hwnd, TIMERID_MEMPROFILE );
			return 0L;
		}
		break;

	case WM_KEYDOWN:
		switch ( wParam )
		{
		case VK_INSERT:
			MemProfile_ZoomIn( g_memProfile_scale, PROFILE_SCALESTEPS );
			return 0L;

		case VK_DELETE:
			MemProfile_ZoomOut( g_memProfile_scale, PROFILE_SCALESTEPS );
			return 0L;
		}
		break;

	case WM_COMMAND:
		switch ( wID )
		{
		case IDM_MEMPROFILE_TICKMARKS:
			g_memProfile_tickMarks ^= 1;
			MemProfile_UpdateWindow();
			return 0L;

		case IDM_MEMPROFILE_COLORS:
			g_memProfile_colors ^= 1;
			MemProfile_UpdateWindow();
			return 0L;

		case IDM_MEMPROFILE_ZOOMIN:
			MemProfile_ZoomIn( g_memProfile_scale, PROFILE_SCALESTEPS );
			return 0L;

		case IDM_MEMPROFILE_ZOOMOUT:
			MemProfile_ZoomOut( g_memProfile_scale, PROFILE_SCALESTEPS );
			return 0L;

		case IDM_MEMPROFILE_ENABLE:
			bool bEnable = ( g_memProfile_Timer != NULL );
			bEnable ^= 1;
			MemProfile_EnableProfiling( bEnable );
			MemProfile_SetTitle();
			return 0L;
		}
		break;
	}	
	return ( DefWindowProc( hwnd, message, wParam, lParam ) );
}

//-----------------------------------------------------------------------------
//	MemProfile_Open
// 
//-----------------------------------------------------------------------------
void MemProfile_Open()
{
	HWND	hWnd;
	
	if ( g_memProfile_hWnd )
	{
		// only one profile instance
		if ( IsIconic( g_memProfile_hWnd ) )
			ShowWindow( g_memProfile_hWnd, SW_RESTORE );
		SetForegroundWindow( g_memProfile_hWnd );
		return;
	}

	hWnd = CreateWindowEx( 
				WS_EX_CLIENTEDGE,
				"MEMPROFILECLASS",
				"",
				WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX|WS_MINIMIZEBOX|WS_MAXIMIZEBOX,
				0,
				0,
				600,
				500,
				g_hDlgMain,
				NULL,
				g_hInstance,
				( void* )ID_MEMPROFILE );
	g_memProfile_hWnd = hWnd;

	MemProfile_EnableProfiling( true );
	MemProfile_SetTitle();

	if ( g_memProfile_WindowRect.right && g_memProfile_WindowRect.bottom )
		MoveWindow( g_memProfile_hWnd, g_memProfile_WindowRect.left, g_memProfile_WindowRect.top, g_memProfile_WindowRect.right-g_memProfile_WindowRect.left, g_memProfile_WindowRect.bottom-g_memProfile_WindowRect.top, FALSE );
	ShowWindow( g_memProfile_hWnd, SHOW_OPENWINDOW );
}

//-----------------------------------------------------------------------------
//	MemProfile_Init
// 
//-----------------------------------------------------------------------------
bool MemProfile_Init()
{
	WNDCLASS wndclass;

	// set up our window class
	memset( &wndclass, 0, sizeof( wndclass ) );
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = MemProfile_WndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 0;
	wndclass.hInstance     = g_hInstance;
	wndclass.hIcon         = g_hIcons[ICON_APPLICATION];
	wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = g_hBackgroundBrush;
	wndclass.lpszMenuName  = MAKEINTRESOURCE( MENU_MEMPROFILE );
	wndclass.lpszClassName = "MEMPROFILECLASS";
	if ( !RegisterClass( &wndclass ) )
		return false;
	
	MemProfile_LoadConfig();

	return true;
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A simple app which looks for the HL2 wise installer and ticks the progress bar due
//  to a bug with installing more than 2GB of data using the current ver of the windows installer
//
//=============================================================================//

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <commctrl.h>
#include <stdlib.h>


#define FIND_WINDOW_TEXT_PROGRESSDIALOG			"vHackWiseProgressDialog092304"
#define FIND_WINDOW_TEXT_CHANGEDISKDIALOG		"vHackWiseProgressDialogChangeCD092304"
#define FIND_WINDOW_TEXT_CANCELDIALOG			"vHackWiseProgressDialogCancel092304"

static char szAppName[] = "vWiseProgressBarHackWndClass";

// The full bar is this many ticks (which are about 100 msec apart, so 30 seconds to walk bar
#define PROGRESS_TICKS 75
#define PROGRESS_WAIT_TICKS	 20

// After this long, if we didn't find the setup dialog, exit the application
#define SEARCH_TIMEOUT_SECONDS				60

#define WISE_PROGRESS_BAR_WINDOW_CLASS		"msctls_progress32"

//-----------------------------------------------------------------------------
// Purpose: Globals
//-----------------------------------------------------------------------------
struct Globals_t
{

	DWORD		m_nLastThink;
	DWORD		m_nStartTick;

	bool		m_bFoundWindow;
	HWND		m_hProgressBar;
	HWND		m_hDialog;

	UINT		m_nTickCounter;
};

static Globals_t	g;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct FindParams_t
{
	HWND		wnd;
	char		searchtext[ 512 ];
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwnd - 
//			lParam - 
// Output : BOOL CALLBACK
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumChildrenLookingForSpecialControl(HWND hwnd,LPARAM lParam)
{
	FindParams_t *p = ( FindParams_t *)lParam;

	char buf[ 512 ];
	
	GetWindowText( hwnd, buf, sizeof( buf ) );
	if ( !stricmp( buf, p->searchtext ) )
	{
		p->wnd = hwnd;   
		return FALSE;
	}
	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwnd - 
//			lParam - 
// Output : BOOL CALLBACK
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumChildWindowsProc(HWND hwnd, LPARAM lParam)
{
	// Now search for the special hidden text control inside a top level window

	FindParams_t *p = ( FindParams_t *)lParam;

	FindParams_t special;
	memset( &special, 0, sizeof( special ) );
	strcpy( special.searchtext, p->searchtext );

	EnumChildWindows( hwnd, EnumChildrenLookingForSpecialControl, (LPARAM)&special );
	if ( special.wnd != NULL )
	{
		p->wnd = hwnd;
		return FALSE;
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : root - 
//			*text - 
// Output : HWND
//-----------------------------------------------------------------------------
HWND FindWindowHavingChildWithSpecifiedText( HWND root, char const *text )
{
	FindParams_t params;
	memset( &params, 0, sizeof( params ) );

	strncpy( params.searchtext, text, sizeof( params.searchtext ) - 1 );

	EnumChildWindows( root, EnumChildWindowsProc, (LPARAM)&params );

	return params.wnd;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *text - 
// Output : HWND
//-----------------------------------------------------------------------------
HWND FindTopLevelWindowHavingChildWithSpecifiedText( char const *text )
{
	return FindWindowHavingChildWithSpecifiedText( GetDesktopWindow(), text );
}

//-----------------------------------------------------------------------------
// Purpose: Search for window of class WISE_PROGRESS_BAR_WINDOW_CLASS
// Input  : hwnd - 
//			lParam - 
// Output : BOOL CALLBACK
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumFindProgressBarInDialog( HWND hwnd,LPARAM lParam )
{
	char buf[100];
	
	GetClassName( hwnd, buf, sizeof( buf ) );
	if ( !stricmp( buf, WISE_PROGRESS_BAR_WINDOW_CLASS ) )
	{
		*(HWND*)lParam = hwnd;   
		return FALSE;
	}
	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: Hides the window
// Input  : visible - 
//-----------------------------------------------------------------------------
void ShowProgressBar( bool visible )
{
	if ( !g.m_hProgressBar )
		return;

	DWORD style = GetWindowLong( g.m_hProgressBar, GWL_STYLE );
	if ( visible )
	{
		style |= WS_VISIBLE;
	}
	else
	{
		style &= ~WS_VISIBLE;
	}

	SetWindowLong( g.m_hProgressBar, GWL_STYLE, style );
	InvalidateRect( g.m_hDialog, NULL, TRUE );
}

//-----------------------------------------------------------------------------
// Purpose: Search for the progress dialog
//-----------------------------------------------------------------------------
void SearchForWindow()
{
	HWND hProgressDialog = FindTopLevelWindowHavingChildWithSpecifiedText( FIND_WINDOW_TEXT_PROGRESSDIALOG );
	if ( !hProgressDialog )
	{
		return;
	}

	HWND hWndChild = NULL;

	EnumChildWindows( hProgressDialog, EnumFindProgressBarInDialog, (LPARAM)&hWndChild );
	if ( !hWndChild )
		return;

	g.m_bFoundWindow = true;
	g.m_hProgressBar = hWndChild;
	g.m_hDialog = hProgressDialog;

	// Hide the progress bar on the dialog since we'll be drawing our own
	ShowProgressBar( false );
}

void DrawProgressBar( HDC dc, bool showTicks, float frac )
{
	// Get progress bar rectangle
	RECT rc;
	GetClientRect( g.m_hProgressBar, &rc );

	//InflateRect( &rc, 2, 2 );

	int w = rc.right - rc.left;
	int h = rc.bottom - rc.top;

	HDC dcMemory = CreateCompatibleDC( dc );
	HBITMAP bmMemory = CreateCompatibleBitmap( dc, w, h );
	HBITMAP bmOld = (HBITMAP)SelectObject( dcMemory, bmMemory );

	{

		HBRUSH clearColor = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );

		FillRect( dcMemory, &rc, clearColor );

		// Create blue tick brush
		HBRUSH br = CreateSolidBrush( RGB( 2, 62, 134 ) );
		// Create background brush of same color as dialog background
		HBRUSH bg = CreateSolidBrush( RGB( 153, 175, 199) );

		// Create a black / shadow colored pen to frame the progress bar
		HPEN blackpen;
		blackpen = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNSHADOW ) );

		// Select items into dcMemory
		HPEN oldPen = (HPEN)SelectObject( dcMemory, blackpen );
		HBRUSH oldBrush = (HBRUSH)SelectObject( dcMemory, bg );

		rc.bottom = rc.top + 15;
		RoundRect( dcMemory, rc.left, rc.top, rc.right, rc.bottom, 5, 5 );

		// Inset by one unit
		InflateRect( &rc, -1, -1 );

		if ( showTicks )
		{
			HRGN clipRegion = (HRGN)CreateRectRgn( rc.left+1, rc.top, rc.right-1, rc.bottom );;
		
			SelectClipRgn( dcMemory, clipRegion );

			int numblocks = 8;
			int blockwidth = 6;
			int blockgap = 2;

			int size = numblocks * ( blockwidth + blockgap );

			// Determine width of progress bar work area
			int width = rc.right - rc.left + 2 * size;
			
			// Compute right edge of progress bar
			RECT rcProgress = rc;
			rcProgress.right = rcProgress.left - size + ( int )( frac * width + 0.5f );
			rcProgress.left = rcProgress.right - size;

			for ( int block = 0; block < numblocks; ++block )
			{
				RECT rcBlock;
				rcBlock.left   = rcProgress.left + block * ( blockwidth + blockgap );
				rcBlock.right  = rcBlock.left + blockwidth;
				rcBlock.top	   = rcProgress.top + 1;
				rcBlock.bottom = rcProgress.bottom - 1;

				// Fill in progress bar
				FillRect( dcMemory, &rcBlock, br );
			}

			SelectClipRgn( dcMemory, NULL );
			DeleteObject( clipRegion );
		}

		// Restore GDI states
		SelectObject( dcMemory, oldBrush );
		SelectObject( dcMemory, oldPen );

		DeleteObject( blackpen );

		DeleteObject( bg );
		DeleteObject( br );
		DeleteObject( clearColor );
	}

	POINT pt;
	pt.x = pt.y = 0;

	// Convert top left of progress bar to screen space
	ClientToScreen( g.m_hProgressBar, &pt );
	// and then back to dialog relative space
	ScreenToClient( g.m_hDialog, &pt );

	// Offset the progress bar rect to the right position in the dialog
	OffsetRect( &rc, pt.x, pt.y );

	BitBlt( dc, rc.left, rc.top, w, h, dcMemory, 0, 0, SRCCOPY );

	SelectObject( dcMemory, bmOld );
	DeleteObject( bmMemory );

	DeleteObject( dcMemory );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void UpdateProgress()
{
	// If the "Insert next CD" or "Exit Setup" dialogs are showing, stop advancing the progress bar
	HWND hSwapDiskDialog = FindWindowHavingChildWithSpecifiedText( GetDesktopWindow(), FIND_WINDOW_TEXT_CHANGEDISKDIALOG );
	HWND hCancelDialog = FindWindowHavingChildWithSpecifiedText( GetDesktopWindow(), FIND_WINDOW_TEXT_CANCELDIALOG );
	if ( !hSwapDiskDialog && !hCancelDialog )
	{	
		g.m_nTickCounter++;
	}

	if ( !g.m_hProgressBar || !g.m_hDialog )
		return;

	int remainder = ( g.m_nTickCounter % PROGRESS_TICKS );

	bool showTicks = ( remainder <= PROGRESS_WAIT_TICKS ) ? false : true;

	int currentTick = max( 0, remainder - PROGRESS_WAIT_TICKS );
	int totalTicks = PROGRESS_TICKS - PROGRESS_WAIT_TICKS;

	float frac = ( float )( currentTick % totalTicks ) / ( float )( totalTicks - 1 );

	HDC dc = GetDC( g.m_hDialog );
	{
		// Draw the progress bar
		DrawProgressBar( dc, showTicks, frac );
	}
	ReleaseDC( g.m_hDialog, dc );
}

//-----------------------------------------------------------------------------
// Purpose: Either searches for window or updates progress
//  The app will quit if the dialog is found and then goes away
//	The app wil also quit if the dialog was not found after waiting 60 seconds
//-----------------------------------------------------------------------------
void Think()
{
	// Only think once every 100 msec
	DWORD curTick = GetTickCount();
	if ( curTick - g.m_nLastThink < 50 )
	{
		return;
	}

	g.m_nLastThink = curTick;

	// Haven't found window yet, keep searching
	if ( !g.m_bFoundWindow )
	{
		SearchForWindow();

		// Wise never got going..., abort this app...
		if ( ( curTick - g.m_nStartTick ) > ( SEARCH_TIMEOUT_SECONDS * 1000 ) )
		{
			PostQuitMessage( 0 );
		}
	}
	else
	{
		// Only redraw progress once every 100 msec
		UpdateProgress();

		// If the progress dialog does away, exit this app immediately
		if ( !IsWindow( g.m_hDialog ) )
		{
			PostQuitMessage( 0 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Main entry point
// Input  : hInstance - 
//			hPrevInstance - 
//			nCmdShow - 
// Output : int APIENTRY
//-----------------------------------------------------------------------------
int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{

	HWND     hwnd ;
	WNDCLASS wndclass ;
	
	wndclass.style         = CS_HREDRAW | CS_VREDRAW ;
	wndclass.lpfnWndProc   = DefWindowProc;
	wndclass.cbClsExtra    = 0 ;
	wndclass.cbWndExtra    = 0 ;
	wndclass.hInstance     = hInstance ;
	wndclass.hIcon         = NULL;
	wndclass.hCursor       = NULL;
	wndclass.hbrBackground = NULL;
	wndclass.lpszMenuName  = NULL ;
	wndclass.lpszClassName = szAppName ;
	
	if ( !RegisterClass (&wndclass) )
	{
		return 0 ;
	}
	
	hwnd = CreateWindow
	(
		szAppName, 
		TEXT (""),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hInstance, NULL
	);
	
	if (!hwnd)
		return 0 ;
	
	ShowWindow( hwnd, SW_HIDE ) ;

	// Remember when we started
	g.m_nStartTick = GetTickCount();

	bool done = false;
	while ( 1 )
	{
		MSG msg;

	    while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
		{
			if ( msg.message == WM_QUIT )
			{
				done = true;
				break;
			}

			TranslateMessage (&msg) ;
			DispatchMessage (&msg) ;
		}
		
		if ( done )
			break;

		Think();
		Sleep( 20 );
	}

	// Restore progress bar as needed
	ShowProgressBar( true );

	return 0;
}




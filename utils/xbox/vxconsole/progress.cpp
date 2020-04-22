//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	PROGRESS.CPP
//
//	Progress Metering Utility.
//=====================================================================================//
#include "vxconsole.h"

#define PROGRESS_WIDTH		425
#define PROGRESS_HEIGHT		170

#define ID_PROGRESS_STATUS1	100
#define ID_PROGRESS_STATUS2	101
#define ID_PROGRESS_STATUS3	102
#define ID_PROGRESS_PERCENT	103
#define ID_PROGRESS_METER	104
#define ID_PROGRESS_CANCEL	105

//-----------------------------------------------------------------------------
//	CProgress_WndProc
// 
//-----------------------------------------------------------------------------
LRESULT CALLBACK Progress_WndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	CProgress		*pProgress;
	CREATESTRUCT	*createStructPtr;

	switch ( message )
	{
		case WM_CREATE:
			createStructPtr = ( CREATESTRUCT* )lParam;
			SetWindowLong( hwnd, GWL_USERDATA+0, ( LONG )createStructPtr->lpCreateParams );
			return 0L;

		case WM_DESTROY:
			pProgress = ( CProgress* )GetWindowLong( hwnd, GWL_USERDATA+0 );
			if ( pProgress )
				pProgress->m_hWnd = NULL;
			return 0L;
	
		case WM_CTLCOLORSTATIC:
			SetBkColor( ( HDC )wParam, g_backgroundColor );
			return ( BOOL )g_hBackgroundBrush;

		case WM_COMMAND: 
            switch ( LOWORD( wParam ) ) 
            {
				case ID_PROGRESS_CANCEL: 
					pProgress = ( CProgress* )GetWindowLong( hwnd, GWL_USERDATA+0 );
					if ( pProgress )
						pProgress->m_bCancelPressed = true;
					return ( TRUE ); 
			}
			break;
	}	

	return ( DefWindowProc( hwnd, message, wParam, lParam ) );
}

//-----------------------------------------------------------------------------
//	CProgress::Update
//
//	Pump the message loop
//-----------------------------------------------------------------------------
void CProgress::Update()
{
	MSG	msg;

	while ( PeekMessage( &msg, NULL, 0, 0, PM_NOYIELD|PM_REMOVE ) )
    {
        if ( !TranslateAccelerator( g_hDlgMain, g_hAccel, &msg ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
    }
}

//-----------------------------------------------------------------------------
//	CProgress::IsCancel
//
//-----------------------------------------------------------------------------
bool CProgress::IsCancel()
{
	return m_bCancelPressed;	
}

//-----------------------------------------------------------------------------
//	CProgress::SetMeter
// 
//-----------------------------------------------------------------------------
void CProgress::SetMeter( int currentPos, int range )
{
	char	buff[16];
	int		percent;

	if ( !m_hWnd || !m_hWndPercent || !m_hWndMeter )
		return;

	if ( range >= 0 )
	{
		SendMessage( m_hWndMeter, PBM_SETRANGE, 0, MAKELPARAM( 0, range ) ); 
		m_range = range;
	}
	SendMessage( m_hWndMeter, PBM_SETPOS, currentPos, 0 ); 

	if ( m_range > 0 )
	{
		percent = ( int )( 100.0f*currentPos/m_range );
		if ( percent > 100 )
			percent = 100;
	}
	else
		percent = 0;
	sprintf( buff, "%d%%", percent );
	SetWindowText( m_hWndPercent, buff );

	Update();
}

//-----------------------------------------------------------------------------
//	CProgress::SetStatus
// 
//-----------------------------------------------------------------------------
void CProgress::SetStatus( const char *line1, const char *line2, const char *line3 )
{
	if ( !m_hWnd )
		return;

	if ( line1 )
		SetWindowText( m_hWndStatus1, line1 );
	if ( line2 )
		SetWindowText( m_hWndStatus2, line2 );
	if ( line3 )
		SetWindowText( m_hWndStatus3, line3 );

	Update();
}

//-----------------------------------------------------------------------------
//	CProgress::Open
// 
//-----------------------------------------------------------------------------
void CProgress::Open( const char* title, bool canCancel, bool bHasMeter )
{
	HWND	hWnd;
	RECT	clientRect;
	RECT	parentRect;
	int		cx;
	int		cy;
	int		cw;
	int		ch;
	int		y;
	int		dialogHeight;

	dialogHeight = PROGRESS_HEIGHT;
	if ( !canCancel )
		dialogHeight -= 25;
	if ( !bHasMeter )
		dialogHeight -= GetSystemMetrics( SM_CYVSCROLL );

	hWnd = CreateWindowEx( 
				WS_EX_CLIENTEDGE,
				"PROGRESSCLASS",
				title,
				WS_POPUP|WS_CAPTION,
				0,
				0,
				PROGRESS_WIDTH,
				dialogHeight,
				g_hDlgMain,
				NULL,
				g_hInstance,
				( void* )this );
	m_hWnd = hWnd;
	if ( !m_hWnd )
		return;

	// status text line #1
	GetClientRect( m_hWnd, &clientRect ); 
	y = 10;
	hWnd = CreateWindowEx( 
				0,
				WC_STATIC, 
				"", 
				WS_VISIBLE|WS_CHILD|SS_WORDELLIPSIS, 
				8, 
				10, 
				clientRect.right-clientRect.left-2*8 - 50,
				20, 
				m_hWnd,
				( HMENU )ID_PROGRESS_STATUS1,
				g_hInstance,
				NULL ); 
	m_hWndStatus1 = hWnd;
	y += 20;

	// status text line #2
	hWnd = CreateWindowEx( 
				0,
				WC_STATIC, 
				"", 
				WS_VISIBLE|WS_CHILD|SS_PATHELLIPSIS, 
				8, 
				y, 
				clientRect.right-clientRect.left-2*8 -50,
				20, 
				m_hWnd,
				( HMENU )ID_PROGRESS_STATUS2,
				g_hInstance,
				NULL ); 
	m_hWndStatus2 = hWnd;
	y += 20;

	// status text line #3
	hWnd = CreateWindowEx( 
				0,
				WC_STATIC, 
				"", 
				WS_VISIBLE|WS_CHILD|SS_PATHELLIPSIS, 
				8, 
				y, 
				clientRect.right-clientRect.left-2*8 -50,
				20, 
				m_hWnd,
				( HMENU )ID_PROGRESS_STATUS3,
				g_hInstance,
				NULL ); 
	m_hWndStatus3 = hWnd;
	y += 20;

	// set font
	SendMessage( m_hWndStatus1, WM_SETFONT, ( WPARAM )g_hProportionalFont, TRUE );
	SendMessage( m_hWndStatus2, WM_SETFONT, ( WPARAM )g_hProportionalFont, TRUE );
	SendMessage( m_hWndStatus3, WM_SETFONT, ( WPARAM )g_hProportionalFont, TRUE );

	if ( bHasMeter )
	{
		// percent
		hWnd = CreateWindowEx( 
					0,
					WC_STATIC, 
					"0%", 
					WS_VISIBLE|WS_CHILD|SS_RIGHT, 
					( clientRect.right-clientRect.left ) - 2*8 - 50, 
					y - 20, 
					50,
					20, 
					m_hWnd,
					( HMENU )ID_PROGRESS_PERCENT,
					g_hInstance,
					NULL ); 
		m_hWndPercent = hWnd;
		SendMessage( m_hWndPercent, WM_SETFONT, ( WPARAM )g_hProportionalFont, TRUE );

		// progress meter
		ch = GetSystemMetrics( SM_CYVSCROLL ); 
		cw = ( clientRect.right-clientRect.left ) - 2*8;
		cx = ( clientRect.left + clientRect.right )/2 - cw/2;
		cy = y;
		hWnd = CreateWindowEx( 
			WS_EX_CLIENTEDGE,
			PROGRESS_CLASS,
			NULL,
			WS_VISIBLE|WS_CHILD, 
			cx,
			cy,
			cw,
			ch, 
			m_hWnd,
			( HMENU )ID_PROGRESS_METER,
			g_hInstance,
			NULL );
		m_hWndMeter = hWnd;
		y = cy+ch;

		// ensure bar is reset
		SendMessage( m_hWndMeter, PBM_SETRANGE, 0, 0 ); 
		SendMessage( m_hWndMeter, PBM_SETPOS, 0, 0 ); 
	}
	else
	{
		m_hWndPercent = NULL;
		m_hWndMeter = NULL;
	}

	m_bCancelPressed = false;
	if ( canCancel )
	{
	    ch = 25; 
		cw = 80;
		cx = ( clientRect.left + clientRect.right )/2 - cw/2;
		cy = clientRect.bottom - 8 - ch;

		// cancel button
		hWnd = CreateWindowEx( 
					0,
					WC_BUTTON, 
					"Cancel", 
					WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON, 
					cx, 
					cy, 
					cw,
					ch, 
					m_hWnd,
					( HMENU )ID_PROGRESS_CANCEL,
					g_hInstance,
					NULL ); 
		m_hWndCancel = hWnd;

		SendMessage( m_hWndCancel, WM_SETFONT, ( WPARAM )g_hProportionalFont, TRUE );
	}

	// get parent rectangle
	GetWindowRect( g_hDlgMain, &parentRect );
	cx = ( parentRect.left + parentRect.right )/2 - PROGRESS_WIDTH/2;
	cy = ( parentRect.top + parentRect.bottom )/2 - dialogHeight/2;

	MoveWindow( m_hWnd, cx, cy, PROGRESS_WIDTH, dialogHeight, FALSE );
	ShowWindow( m_hWnd, SHOW_OPENWINDOW );
}

//-----------------------------------------------------------------------------
//	CProgress::~CProgress
// 
//-----------------------------------------------------------------------------
CProgress::~CProgress()
{
	if ( !m_hWnd )
		return;

	DestroyWindow( m_hWnd );
	m_hWnd = NULL;
}

//-----------------------------------------------------------------------------
//	CProgress::CProgress
// 
//-----------------------------------------------------------------------------
CProgress::CProgress()
{
	// set up our window class
	WNDCLASS wndclass;
	memset( &wndclass, 0, sizeof( wndclass ) );
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = Progress_WndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = sizeof( CProgress* );
	wndclass.hInstance     = g_hInstance;
	wndclass.hIcon         = g_hIcons[ICON_APPLICATION];
	wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = g_hBackgroundBrush;
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName = "PROGRESSCLASS";
	RegisterClass( &wndclass );

	m_hWnd = 0;
	m_bCancelPressed = false;
}

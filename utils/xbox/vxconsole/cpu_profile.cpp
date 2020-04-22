//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	CPU_PROFILE.CPP
//
//	Cpu Profiling Display
//=====================================================================================//
#include "vxconsole.h"

#define PROFILE_MAXCOUNTERS				64
#define PROFILE_MAXSAMPLES				512

#define PROFILE_HISTORY_TIMINGHEIGHT	100
#define PROFILE_HISTORY_NUMMINORTICKS	3
#define PROFILE_HISTORY_LABELWIDTH		50
#define PROFILE_HISTORY_SCALESTEPS		5
#define PROFILE_HISTORY_MINSCALE		0.3f
#define PROFILE_HISTORY_MAXSCALE		3.0f

#define PROFILE_SAMPLES_ITEMHEIGHT		15
#define PROFILE_SAMPLES_BARHEIGHT		10
#define PROFILE_SAMPLES_TIMINGWIDTH		200
#define PROFILE_SAMPLES_LABELWIDTH		150
#define PROFILE_SAMPLES_LABELGAP		5
#define PROFILE_SAMPLES_NUMMINORTICKS	3
#define PROFILE_SAMPLES_PEAKHOLDTIME	3000
#define PROFILE_SAMPLES_SCALESTEPS		10
#define PROFILE_SAMPLES_MINSCALE		0.3f
#define PROFILE_SAMPLES_MAXSCALE		3.0f

#define ID_CPUPROFILE_SAMPLES			1
#define ID_CPUPROFILE_HISTORY			2

typedef struct
{
	unsigned int	samples[PROFILE_MAXSAMPLES];
	unsigned int	peakSample;
	char			label[64];
	COLORREF		color;
} profileCounter_t;

HWND				g_cpuProfile_hWndSamples;
HWND				g_cpuProfile_hWndHistory;
int					g_cpuProfile_numCounters;
profileCounter_t	g_cpuProfile_counters[PROFILE_MAXCOUNTERS];
RECT				g_cpuProfile_samplesWindowRect;
RECT				g_cpuProfile_historyWindowRect;
DWORD				g_cpuProfile_lastPeakTime;
bool				g_cpuProfile_history_tickMarks = true;
bool				g_cpuProfile_history_colors = true;
int					g_cpuProfile_history_scale;
bool				g_cpuProfile_samples_tickMarks = true;
bool				g_cpuProfile_samples_colors = true;
int					g_cpuProfile_samples_scale;
int					g_cpuProfile_numSamples;
int					g_cpuProfile_fpsLabels;

//-----------------------------------------------------------------------------
//	CpuProfile_SaveConfig
// 
//-----------------------------------------------------------------------------
void CpuProfile_SaveConfig()
{
	char			buff[256];
	WINDOWPLACEMENT wp;

	// profile samples
	if ( g_cpuProfile_hWndSamples )
	{
		memset( &wp, 0, sizeof( wp ) );
		wp.length = sizeof( WINDOWPLACEMENT );
		GetWindowPlacement( g_cpuProfile_hWndSamples, &wp );
		g_cpuProfile_samplesWindowRect = wp.rcNormalPosition;
		sprintf( buff, "%d %d %d %d", wp.rcNormalPosition.left, wp.rcNormalPosition.top, wp.rcNormalPosition.right, wp.rcNormalPosition.bottom );
		Sys_SetRegistryString( "profileSamplesWindowRect", buff );
	}
	Sys_SetRegistryInteger( "profileSamplesScale", g_cpuProfile_samples_scale );

	// profile history
	if ( g_cpuProfile_hWndHistory )
	{
		memset( &wp, 0, sizeof( wp ) );
		wp.length = sizeof( WINDOWPLACEMENT );
		GetWindowPlacement( g_cpuProfile_hWndHistory, &wp );
		g_cpuProfile_historyWindowRect = wp.rcNormalPosition;
		sprintf( buff, "%d %d %d %d", wp.rcNormalPosition.left, wp.rcNormalPosition.top, wp.rcNormalPosition.right, wp.rcNormalPosition.bottom );
		Sys_SetRegistryString( "profileHistoryWindowRect", buff );
	}
	Sys_SetRegistryInteger( "profileHistoryScale", g_cpuProfile_history_scale );

	Sys_SetRegistryInteger( "cpuProfileFpsLabels", g_cpuProfile_fpsLabels );
}

//-----------------------------------------------------------------------------
//	CpuProfile_LoadConfig	
// 
//-----------------------------------------------------------------------------
void CpuProfile_LoadConfig()
{
	int		numArgs;
	char	buff[256];

	// profile samples
	Sys_GetRegistryString( "profileSamplesWindowRect", buff, "", sizeof( buff ) );
	numArgs = sscanf( buff, "%d %d %d %d", &g_cpuProfile_samplesWindowRect.left, &g_cpuProfile_samplesWindowRect.top, &g_cpuProfile_samplesWindowRect.right, &g_cpuProfile_samplesWindowRect.bottom );
	if ( numArgs != 4 )
		memset( &g_cpuProfile_samplesWindowRect, 0, sizeof( g_cpuProfile_samplesWindowRect ) );
	Sys_GetRegistryInteger( "profileSamplesScale", 0, g_cpuProfile_samples_scale );
	if ( g_cpuProfile_samples_scale < -PROFILE_SAMPLES_SCALESTEPS || g_cpuProfile_samples_scale > PROFILE_SAMPLES_SCALESTEPS )
		g_cpuProfile_samples_scale = 0;

	// profile history
	Sys_GetRegistryString( "profileHistoryWindowRect", buff, "", sizeof( buff ) );
	numArgs = sscanf( buff, "%d %d %d %d", &g_cpuProfile_historyWindowRect.left, &g_cpuProfile_historyWindowRect.top, &g_cpuProfile_historyWindowRect.right, &g_cpuProfile_historyWindowRect.bottom );
	if ( numArgs != 4 )
		memset( &g_cpuProfile_historyWindowRect, 0, sizeof( g_cpuProfile_historyWindowRect ) );
	Sys_GetRegistryInteger( "profileHistoryScale", 0, g_cpuProfile_history_scale );
	if ( g_cpuProfile_history_scale < -PROFILE_HISTORY_SCALESTEPS || g_cpuProfile_history_scale > PROFILE_HISTORY_SCALESTEPS )
		g_cpuProfile_history_scale = 0;

	Sys_GetRegistryInteger( "cpuProfileFpsLabels", 0, g_cpuProfile_fpsLabels );
}

//-----------------------------------------------------------------------------
//	CpuProfile_SetTitle
// 
//-----------------------------------------------------------------------------
void CpuProfile_SetTitle()
{
	char	titleBuff[128];

	if ( g_cpuProfile_hWndSamples )
	{
		strcpy( titleBuff, "CPU Usage Snapshot" );
		if ( VProf_GetState() == VPROF_CPU )
			strcat( titleBuff, " [ON]" );

		SetWindowText( g_cpuProfile_hWndSamples, titleBuff );
	}

	if ( g_cpuProfile_hWndHistory )
	{
		strcpy( titleBuff, "CPU Usage History" );
		if ( VProf_GetState() == VPROF_CPU )
			strcat( titleBuff, " [ON]" );

		SetWindowText( g_cpuProfile_hWndHistory, titleBuff );
	}
}

//-----------------------------------------------------------------------------
// CpuProfile_UpdateWindow
//
//-----------------------------------------------------------------------------
void CpuProfile_UpdateWindow()
{
	if ( g_cpuProfile_hWndSamples && !IsIconic( g_cpuProfile_hWndSamples ) )
	{
		// visible - force a client repaint
		InvalidateRect( g_cpuProfile_hWndSamples, NULL, true );
	}

	if ( g_cpuProfile_hWndHistory && !IsIconic( g_cpuProfile_hWndHistory ) )
	{
		// visible - force a client repaint
		InvalidateRect( g_cpuProfile_hWndHistory, NULL, true );
	}
}

//-----------------------------------------------------------------------------
// rc_SetCpuProfile
//
//-----------------------------------------------------------------------------
int rc_SetCpuProfile( char* commandPtr )
{
	int				i;
	char*			cmdToken;
	int				retAddr;
	int				errCode = -1;
	xrProfile_t*	localList;
	int				profileList;
	int				numProfiles;
	int				retVal;

	// get numProfiles
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken,"%x",&numProfiles );

	// get profile attributes
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &profileList );

	// get retAddr
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken,"%x",&retAddr );

	localList = new xrProfile_t[numProfiles];
	memset( localList, 0, numProfiles*sizeof( xrProfile_t ) );

	// get the caller's profile list 
	DmGetMemory( ( void* )profileList, numProfiles*sizeof( xrProfile_t ), localList, NULL );

	g_cpuProfile_numCounters = numProfiles;
	if ( g_cpuProfile_numCounters > PROFILE_MAXCOUNTERS-1 )
		g_cpuProfile_numCounters = PROFILE_MAXCOUNTERS-1;
	
	for ( i=0; i<g_cpuProfile_numCounters; i++ )
	{
		// swap the structure
		localList[i].color = BigDWord( localList[i].color );

		// clear the old counter
		memset( &g_cpuProfile_counters[i], 0, sizeof( profileCounter_t ) );

		V_strncpy( g_cpuProfile_counters[i].label, localList[i].labelString, sizeof( g_cpuProfile_counters[i].label ) );
		g_cpuProfile_counters[i].color = localList[i].color;
	}

	// build out the reserved last counter as total count
	memset( &g_cpuProfile_counters[g_cpuProfile_numCounters], 0, sizeof( profileCounter_t ) );
	strcpy( g_cpuProfile_counters[g_cpuProfile_numCounters].label, "Total" );
	g_cpuProfile_counters[i].color = RGB( 255,255,255 );
	g_cpuProfile_numCounters++;

	// set the return code
	retVal = g_cpuProfile_numCounters-1;
	int xboxRetVal = BigDWord( retVal );
	DmSetMemory( ( void* )retAddr, sizeof( int ), &xboxRetVal, NULL );

	DebugCommand( "0x%8.8x = SetCpuProfile( 0x%8.8x, 0x%8.8x )\n", retVal, numProfiles, profileList );

	delete [] localList;

	// success
	errCode = 0;

cleanUp:
	return ( errCode );
}

//-----------------------------------------------------------------------------
// rc_SetCpuProfileData
//
//-----------------------------------------------------------------------------
int rc_SetCpuProfileData( char* commandPtr )
{
	int				i;
	int				total;
	char*			cmdToken;
	int				errCode = -1;
	int				counters;
	int				currentSample;
	bool			newPeaks;
	unsigned int	localCounters[PROFILE_MAXCOUNTERS];
	DWORD			newTime;

	// get profiles
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
	{
		goto cleanUp;
	}
	sscanf( cmdToken, "%x", &counters );

	// get the caller's profile list 
	if ( g_cpuProfile_numCounters )
	{
		DmGetMemory( ( void* )counters, ( g_cpuProfile_numCounters-1 )*sizeof( int ), localCounters, NULL );
	}

	// timeout peaks
	newTime = Sys_GetSystemTime();
	if ( newTime - g_cpuProfile_lastPeakTime > PROFILE_SAMPLES_PEAKHOLDTIME )
	{
		g_cpuProfile_lastPeakTime = newTime;
		newPeaks = true;
	} 
	else
	{
		newPeaks = false;
	}

	// next sample
	currentSample = g_cpuProfile_numSamples % PROFILE_MAXSAMPLES;
	g_cpuProfile_numSamples++;

	total = 0;
	for ( i=0; i<g_cpuProfile_numCounters; i++ )
	{
		// swap
		localCounters[i] = BigDWord( localCounters[i] );

		if ( i != g_cpuProfile_numCounters-1 )
		{
			g_cpuProfile_counters[i].samples[currentSample] = localCounters[i];
			total += localCounters[i];
		}
		else
		{
			// reserved total counter
			g_cpuProfile_counters[i].samples[currentSample] = total;
		}

		if ( newPeaks || g_cpuProfile_counters[i].peakSample < g_cpuProfile_counters[i].samples[currentSample] )
		{
			g_cpuProfile_counters[i].peakSample = g_cpuProfile_counters[i].samples[currentSample];
		}
	}

	DebugCommand( "SetCpuProfileData( 0x%8.8x )\n", counters );

	CpuProfile_UpdateWindow();

	// success
	errCode = 0;

cleanUp:
	return ( errCode );
}

//-----------------------------------------------------------------------------
//	CpuProfile_ZoomIn
// 
//-----------------------------------------------------------------------------
void CpuProfile_ZoomIn( int& scale, int numSteps )
{
	scale++;
	if ( scale > numSteps )
	{
		scale = numSteps;
		return;
	}
	CpuProfile_UpdateWindow();
}

//-----------------------------------------------------------------------------
//	CpuProfile_ZoomOut
// 
//-----------------------------------------------------------------------------
void CpuProfile_ZoomOut( int& scale, int numSteps )
{
	scale--;
	if ( scale < -numSteps )
	{
		scale = -numSteps;
		return;
	}
	CpuProfile_UpdateWindow();
}

//-----------------------------------------------------------------------------
//	CpuProfile_CalcScale
// 
//-----------------------------------------------------------------------------
float CpuProfile_CalcScale( int scale, int numSteps, float min, float max )
{
	float t;

	// from integral scale [-numSteps..numSteps] to float scale [min..max]
	t = ( float )( scale + numSteps )/( float )( 2*numSteps );
	t = min + t*( max-min );

	return t;
}

//-----------------------------------------------------------------------------
//	ProfileSamples_Draw
// 
//-----------------------------------------------------------------------------
void ProfileSamples_Draw( HDC hdc, RECT* clientRect )
{
	int		i;
	int		j;
	int		x;
	int		y;
	int		x0;
	int		y0;
	int		w;
	float	t;
	float	scale;
	float	sampleTime;
	char	labelBuff[128];
	HPEN	hBlackPen;
	HPEN	hPenOld;
	HPEN	hGreyPen;
	HBRUSH	hColoredBrush;	    
	HBRUSH	hbrushOld;
	HFONT	hFontOld;
	RECT	rect;
	int		currentSample;
	int		numTicks;
	int		timingWidth;
	int		windowWidth;
	int		windowHeight;

	hBlackPen = CreatePen( PS_SOLID, 1, RGB( 0,0,0 ) );
	hGreyPen  = CreatePen( PS_SOLID, 1, Sys_ColorScale( g_backgroundColor, 0.85f ) );
	hPenOld   = ( HPEN )SelectObject( hdc, hBlackPen );
	hFontOld  = SelectFont( hdc, g_hProportionalFont );

	SetBkColor( hdc, g_backgroundColor );

	// zoom
	scale        = CpuProfile_CalcScale( g_cpuProfile_samples_scale, PROFILE_SAMPLES_SCALESTEPS, PROFILE_SAMPLES_MINSCALE, PROFILE_SAMPLES_MAXSCALE );
	timingWidth  = ( int )( PROFILE_SAMPLES_TIMINGWIDTH*scale );
	windowWidth  = clientRect->right-clientRect->left;
	windowHeight = clientRect->bottom-clientRect->top;

	numTicks = ( windowWidth-PROFILE_SAMPLES_LABELWIDTH )/timingWidth + 1;
	if ( numTicks < 0 )
		numTicks = 1;

	rect.left   = 0;
	rect.right  = PROFILE_SAMPLES_LABELWIDTH;
	rect.top    = 0;
	rect.bottom = PROFILE_SAMPLES_ITEMHEIGHT;
	DrawText( hdc, "Name", -1, &rect, DT_LEFT );

	// draw timing ticks
	x = PROFILE_SAMPLES_LABELWIDTH;
	y = 0;
	for ( i=0; i<numTicks; i++ )
	{
		// tick labels
		rect.left   = x-40;
		rect.right  = x+40;
		rect.top    = y;
		rect.bottom = y+PROFILE_SAMPLES_ITEMHEIGHT;
		if ( !g_cpuProfile_fpsLabels )
			sprintf( labelBuff, "%.2fms", i*( 1000.0f/60.0f ) );
		else
			sprintf( labelBuff, "%.2ffps", i == 0 ? 0 : 60.0f/i );
		DrawText( hdc, labelBuff, -1, &rect, DT_CENTER );

		// major ticks
		x0 = x;
		y0 = y + PROFILE_SAMPLES_ITEMHEIGHT;
		SelectObject( hdc, hBlackPen );
		MoveToEx( hdc, x0, y0, NULL );
		LineTo( hdc, x0, y0+windowHeight );

		if ( g_cpuProfile_samples_tickMarks &&  g_cpuProfile_samples_scale > -PROFILE_SAMPLES_SCALESTEPS )
		{
			// minor ticks
			x0 = x;
			y0 = y + PROFILE_SAMPLES_ITEMHEIGHT;
			SelectObject( hdc, hGreyPen );
			for ( j=0; j<PROFILE_SAMPLES_NUMMINORTICKS; j++ )
			{
				x0 += timingWidth/( PROFILE_SAMPLES_NUMMINORTICKS+1 );

				MoveToEx( hdc, x0, y0, NULL );
				LineTo( hdc, x0, y0+windowHeight );
			}
		}
		x += timingWidth;
	}

	// seperator
	SelectObject( hdc, hBlackPen );
	MoveToEx( hdc, 0, PROFILE_SAMPLES_ITEMHEIGHT, NULL );
	LineTo( hdc, windowWidth, PROFILE_SAMPLES_ITEMHEIGHT );

	// draw labels
	x = 0;
	y = PROFILE_SAMPLES_ITEMHEIGHT;
	for ( i=0; i<g_cpuProfile_numCounters; i++ )
	{
		if ( !g_cpuProfile_counters[i].label )
			continue;

		rect.left   = x;
		rect.right  = x+PROFILE_SAMPLES_LABELWIDTH-PROFILE_SAMPLES_LABELGAP;
		rect.top    = y;
		rect.bottom = y+PROFILE_SAMPLES_ITEMHEIGHT;
		DrawText( hdc, g_cpuProfile_counters[i].label, -1, &rect, DT_VCENTER|DT_RIGHT|DT_SINGLELINE|DT_END_ELLIPSIS|DT_MODIFYSTRING );

		// draw the under line
		MoveToEx( hdc, x, y+PROFILE_SAMPLES_ITEMHEIGHT, NULL );
		LineTo( hdc, x+PROFILE_SAMPLES_LABELWIDTH, y+PROFILE_SAMPLES_ITEMHEIGHT );

		y += PROFILE_SAMPLES_ITEMHEIGHT;
	}

	// draw bars
	SelectObject( hdc, hBlackPen );
	x = PROFILE_SAMPLES_LABELWIDTH;
	y = PROFILE_SAMPLES_ITEMHEIGHT;
	currentSample = g_cpuProfile_numSamples-1;
	if ( currentSample < 0 )
		currentSample = 0;
	else
		currentSample %= PROFILE_MAXSAMPLES;
	for ( i=0; i<g_cpuProfile_numCounters; i++ )
	{
		if ( !g_cpuProfile_counters[i].label )
			continue;

		hColoredBrush = CreateSolidBrush( g_cpuProfile_samples_colors ? g_cpuProfile_counters[i].color : g_backgroundColor );
		hbrushOld = ( HBRUSH )SelectObject( hdc, hColoredBrush );

		// bar - count is in us i.e. 1 major tick = 16667us
		t  = ( float )g_cpuProfile_counters[i].samples[currentSample]/( 1000000.0f/60.0f );
		w  = ( int )( t * ( float )timingWidth );
		if ( w > windowWidth )
			w = windowWidth;
		x0 = x;
		y0 = y + ( PROFILE_SAMPLES_ITEMHEIGHT-PROFILE_SAMPLES_BARHEIGHT )/2 + 1;
		Rectangle( hdc, x0, y0, x0 + w, y0 + PROFILE_SAMPLES_BARHEIGHT );

		// peak
		t  = ( float )g_cpuProfile_counters[i].peakSample/( 1000000.0f/60.0f );
		w  = ( int )( t * ( float )timingWidth );
		if ( w > windowWidth )
			w = windowWidth;
		x0 = x + w;
		y0 = y + PROFILE_SAMPLES_ITEMHEIGHT/2 + 1;

		POINT points[4];
		points[0].x = x0;
		points[0].y = y0-4;
		points[1].x = x0+4;
		points[1].y = y0;
		points[2].x = x0;
		points[2].y = y0+4;
		points[3].x = x0-4;
		points[3].y = y0;
		Polygon( hdc, points, 4 );

		SelectObject( hdc, hbrushOld );
		DeleteObject( hColoredBrush );

		// draw peak times
		sampleTime = ( float )g_cpuProfile_counters[i].peakSample/1000.0f;
		if ( sampleTime >= 0.01F )
		{
			sprintf( labelBuff, "%.2f", sampleTime );
			rect.left   = x0 + 8;
			rect.right  = x0 + 8 + 100;
			rect.top    = y;
			rect.bottom = y + PROFILE_SAMPLES_ITEMHEIGHT;
			DrawText( hdc, labelBuff, -1, &rect, DT_VCENTER|DT_LEFT|DT_SINGLELINE );
		}

		y += PROFILE_SAMPLES_ITEMHEIGHT;
	}

	SelectObject( hdc, hFontOld );
	SelectObject( hdc, hPenOld );
	DeleteObject( hBlackPen );
	DeleteObject( hGreyPen );
}

//-----------------------------------------------------------------------------
//	ProfileHistory_Draw
// 
//-----------------------------------------------------------------------------
void ProfileHistory_Draw( HDC hdc, RECT* clientRect )
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
	int		timingHeight;
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
	scale        = CpuProfile_CalcScale( g_cpuProfile_history_scale, PROFILE_HISTORY_SCALESTEPS, PROFILE_HISTORY_MINSCALE, PROFILE_HISTORY_MAXSCALE );
	timingHeight = ( int )( PROFILE_HISTORY_TIMINGHEIGHT*scale );
	windowWidth  = clientRect->right-clientRect->left;
	windowHeight = clientRect->bottom-clientRect->top;

	numTicks = windowHeight/timingHeight + 2;
	if ( numTicks < 0 )
		numTicks = 1;

	SetBkColor( hdc, g_backgroundColor );

	x = 0;
	y = windowHeight;
	for ( i=0; i<numTicks; i++ )
	{
		// major ticks
		SelectObject( hdc, hBlackPen );
		MoveToEx( hdc, 0, y, NULL );
		LineTo( hdc, windowWidth, y );

		if ( g_cpuProfile_history_tickMarks && g_cpuProfile_history_scale > -PROFILE_HISTORY_SCALESTEPS )
		{
			// minor ticks
			y0 = y;
			SelectObject( hdc, hGreyPen );
			for ( j=0; j<PROFILE_HISTORY_NUMMINORTICKS; j++ )
			{
				y0 += timingHeight/( PROFILE_SAMPLES_NUMMINORTICKS+1 );
				MoveToEx( hdc, 0, y0, NULL );
				LineTo( hdc, windowWidth, y0 );
			}
		}

		// tick labels
		if ( i )
		{
			rect.left   = windowWidth-50;
			rect.right  = windowWidth;
			rect.top    = y-20;
			rect.bottom = y;
			if ( !g_cpuProfile_fpsLabels )
				sprintf( labelBuff, "%.2fms", i*( 1000.0f/60.0f ) );
			else
				sprintf( labelBuff, "%.2ffps", 60.0f/i );
			DrawText( hdc, labelBuff, -1, &rect, DT_RIGHT|DT_SINGLELINE|DT_BOTTOM );
		}

		y -= timingHeight;
	}

	// vertical bars
	if ( g_cpuProfile_numSamples )
	{
		SelectObject( hdc, hNullPen );

		numbars       = windowWidth-PROFILE_HISTORY_LABELWIDTH;
		currentSample = g_cpuProfile_numSamples-1;
		for ( x=numbars-1; x>=0; x-=4 )
		{
			// all the counters at this sample
			y = windowHeight;
			for ( j=0; j<g_cpuProfile_numCounters-1; j++ )
			{
				if ( !g_cpuProfile_counters[j].label )
					continue;

				t  = ( float )g_cpuProfile_counters[j].samples[currentSample % PROFILE_MAXSAMPLES]/( 1000000.0f/60.0f );
				h  = ( int )( t * ( float )timingHeight );
				if ( h )
				{
					if ( h > windowHeight )
						h = windowHeight;

					hColoredBrush = CreateSolidBrush( g_cpuProfile_history_colors ? g_cpuProfile_counters[j].color : RGB( 80,80,80 ) );
					hBrushOld = ( HBRUSH )SelectObject( hdc, hColoredBrush );

					Rectangle( hdc, x-4, y-h, x, y+1 );
					y -= h;

					SelectObject( hdc, hBrushOld );
					DeleteObject( hColoredBrush );
				}
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
//	CpuProfile_WndProc
// 
//-----------------------------------------------------------------------------
LRESULT CALLBACK CpuProfile_WndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    WORD			wID = LOWORD( wParam );
	HDC				hdc;
	PAINTSTRUCT		ps; 
	RECT			rect;
	int				id;
	bool			bIsSamples;
	bool			bIsHistory;
	CREATESTRUCT	*createStructPtr;

	// identify window
	id = ( int )GetWindowLong( hwnd, GWL_USERDATA+0 );
	bIsSamples = ( id == ID_CPUPROFILE_SAMPLES );
	bIsHistory = ( id == ID_CPUPROFILE_HISTORY );

	switch ( message )
	{
		case WM_CREATE:
			// set the window identifier
			createStructPtr = ( CREATESTRUCT* )lParam;
			SetWindowLong( hwnd, GWL_USERDATA+0, ( LONG )createStructPtr->lpCreateParams );

			// reset peaks
			g_cpuProfile_lastPeakTime = 0;
			return 0L;

		case WM_DESTROY:
			CpuProfile_SaveConfig();

			if ( bIsSamples )
				g_cpuProfile_hWndSamples = NULL;
			else if ( bIsHistory )
				g_cpuProfile_hWndHistory = NULL;

			if ( VProf_GetState() == VPROF_CPU )
			{
				VProf_Enable( VPROF_OFF );
			}
			return 0L;
	
        case WM_INITMENU:
			if ( bIsSamples )
			{
				CheckMenuItem( ( HMENU )wParam, IDM_CPUPROFILE_TICKMARKS, MF_BYCOMMAND | ( g_cpuProfile_samples_tickMarks ? MF_CHECKED : MF_UNCHECKED ) );
				CheckMenuItem( ( HMENU )wParam, IDM_CPUPROFILE_COLORS, MF_BYCOMMAND | ( g_cpuProfile_samples_colors ? MF_CHECKED : MF_UNCHECKED ) );
			}
			else if ( bIsHistory )
			{
				CheckMenuItem( ( HMENU )wParam, IDM_CPUPROFILE_TICKMARKS, MF_BYCOMMAND | ( g_cpuProfile_history_tickMarks ? MF_CHECKED : MF_UNCHECKED ) );
				CheckMenuItem( ( HMENU )wParam, IDM_CPUPROFILE_COLORS, MF_BYCOMMAND | ( g_cpuProfile_history_colors ? MF_CHECKED : MF_UNCHECKED ) );
			}
			CheckMenuItem( ( HMENU )wParam, IDM_CPUPROFILE_FPSLABELS, MF_BYCOMMAND | ( g_cpuProfile_fpsLabels ? MF_CHECKED : MF_UNCHECKED ) );
			CheckMenuItem( ( HMENU )wParam, IDM_CPUPROFILE_ENABLE, MF_BYCOMMAND | ( VProf_GetState() == VPROF_CPU ? MF_CHECKED : MF_UNCHECKED ) );
			return 0L;

		case WM_PAINT:
			GetClientRect( hwnd, &rect );
			hdc = BeginPaint( hwnd, &ps ); 
			if ( bIsSamples )
				ProfileSamples_Draw( hdc, &rect );
			else if ( bIsHistory )
				ProfileHistory_Draw( hdc, &rect );
            EndPaint( hwnd, &ps ); 
			return 0L;

		case WM_SIZE:
			// force a redraw
			CpuProfile_UpdateWindow();
			return 0L;
	
		case WM_KEYDOWN:
			switch ( wParam )
			{
				case VK_INSERT:
					if ( bIsSamples )
						CpuProfile_ZoomIn( g_cpuProfile_samples_scale, PROFILE_SAMPLES_SCALESTEPS );
					else if ( bIsHistory )
						CpuProfile_ZoomIn( g_cpuProfile_history_scale, PROFILE_HISTORY_SCALESTEPS );
					return 0L;

				case VK_DELETE:
					if ( bIsSamples )
						CpuProfile_ZoomOut( g_cpuProfile_samples_scale, PROFILE_SAMPLES_SCALESTEPS );
					else if ( bIsHistory )
						CpuProfile_ZoomOut( g_cpuProfile_history_scale, PROFILE_HISTORY_SCALESTEPS );
					return 0L;
			}
			break;

        case WM_COMMAND:
            switch ( wID )
            {
				case IDM_CPUPROFILE_TICKMARKS:
					if ( bIsSamples )
						g_cpuProfile_samples_tickMarks ^= 1;
					else if ( bIsHistory )
						g_cpuProfile_history_tickMarks ^= 1;
					CpuProfile_UpdateWindow();
					return 0L;

				case IDM_CPUPROFILE_COLORS:
					if ( bIsSamples )
						g_cpuProfile_samples_colors ^= 1;
					else if ( bIsHistory )
						g_cpuProfile_history_colors ^= 1;
					CpuProfile_UpdateWindow();
					return 0L;

				case IDM_CPUPROFILE_FPSLABELS:
					g_cpuProfile_fpsLabels ^= 1;
					CpuProfile_UpdateWindow();
					return 0L;

				case IDM_CPUPROFILE_ZOOMIN:
					if ( bIsSamples )
						CpuProfile_ZoomIn( g_cpuProfile_samples_scale, PROFILE_SAMPLES_SCALESTEPS );
					else if ( bIsHistory )
						CpuProfile_ZoomIn( g_cpuProfile_history_scale, PROFILE_HISTORY_SCALESTEPS );
					return 0L;

				case IDM_CPUPROFILE_ZOOMOUT:
					if ( bIsSamples )
						CpuProfile_ZoomOut( g_cpuProfile_samples_scale, PROFILE_SAMPLES_SCALESTEPS );
					else if ( bIsHistory )
						CpuProfile_ZoomOut( g_cpuProfile_history_scale, PROFILE_HISTORY_SCALESTEPS );
					return 0L;

				case IDM_CPUPROFILE_ENABLE:
					bool bEnable = ( VProf_GetState() == VPROF_CPU );
					bEnable ^= 1;
					if ( !bEnable )
						VProf_Enable( VPROF_OFF );
					else
						VProf_Enable( VPROF_CPU );
					CpuProfile_SetTitle();
					return 0L;
			}
			break;
	}	
	return ( DefWindowProc( hwnd, message, wParam, lParam ) );
}

//-----------------------------------------------------------------------------
//	CpuProfileHistory_Open
// 
//-----------------------------------------------------------------------------
void CpuProfileHistory_Open()
{
	HWND	hWnd;
	
	if ( g_cpuProfile_hWndHistory )
	{
		// only one profile instance
		if ( IsIconic( g_cpuProfile_hWndHistory ) )
			ShowWindow( g_cpuProfile_hWndHistory, SW_RESTORE );
		SetForegroundWindow( g_cpuProfile_hWndHistory );
		return;
	}

	if ( VProf_GetState() == VPROF_OFF )
	{
		VProf_Enable( VPROF_CPU );
	}

	hWnd = CreateWindowEx( 
				WS_EX_CLIENTEDGE,
				"CPUPROFILEHISTORYCLASS",
				"",
				WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX|WS_MINIMIZEBOX|WS_MAXIMIZEBOX,
				0,
				0,
				600,
				500,
				g_hDlgMain,
				NULL,
				g_hInstance,
				( void* )ID_CPUPROFILE_HISTORY );
	g_cpuProfile_hWndHistory = hWnd;

	CpuProfile_SetTitle();

	if ( g_cpuProfile_historyWindowRect.right && g_cpuProfile_historyWindowRect.bottom )
		MoveWindow( g_cpuProfile_hWndHistory, g_cpuProfile_historyWindowRect.left, g_cpuProfile_historyWindowRect.top, g_cpuProfile_historyWindowRect.right-g_cpuProfile_historyWindowRect.left, g_cpuProfile_historyWindowRect.bottom-g_cpuProfile_historyWindowRect.top, FALSE );
	ShowWindow( g_cpuProfile_hWndHistory, SHOW_OPENWINDOW );
}

//-----------------------------------------------------------------------------
//	CpuProfileSamples_Open
// 
//-----------------------------------------------------------------------------
void CpuProfileSamples_Open()
{
	HWND	hWnd;
	
	if ( g_cpuProfile_hWndSamples )
	{
		// only one profile instance
		if ( IsIconic( g_cpuProfile_hWndSamples ) )
			ShowWindow( g_cpuProfile_hWndSamples, SW_RESTORE );
		SetForegroundWindow( g_cpuProfile_hWndSamples );
		return;
	}

	if ( VProf_GetState() == VPROF_OFF )
	{
		VProf_Enable( VPROF_CPU );
	}

	hWnd = CreateWindowEx( 
				WS_EX_CLIENTEDGE,
				"CPUPROFILESAMPLESCLASS",
				"",
				WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX|WS_MINIMIZEBOX|WS_MAXIMIZEBOX,
				0,
				0,
				600,
				500,
				g_hDlgMain,
				NULL,
				g_hInstance,
				( void* )ID_CPUPROFILE_SAMPLES );
	g_cpuProfile_hWndSamples = hWnd;

	CpuProfile_SetTitle();

	if ( g_cpuProfile_samplesWindowRect.right && g_cpuProfile_samplesWindowRect.bottom )
		MoveWindow( g_cpuProfile_hWndSamples, g_cpuProfile_samplesWindowRect.left, g_cpuProfile_samplesWindowRect.top, g_cpuProfile_samplesWindowRect.right-g_cpuProfile_samplesWindowRect.left, g_cpuProfile_samplesWindowRect.bottom-g_cpuProfile_samplesWindowRect.top, FALSE );
	ShowWindow( g_cpuProfile_hWndSamples, SHOW_OPENWINDOW );
}

//-----------------------------------------------------------------------------
//	CpuProfile_Clear
// 
//-----------------------------------------------------------------------------
void CpuProfile_Clear()
{
	// clear counters and history
	g_cpuProfile_numCounters = 0;
	g_cpuProfile_numSamples  = 0;

	CpuProfile_UpdateWindow();
}

//-----------------------------------------------------------------------------
//	CpuProfile_Init
// 
//-----------------------------------------------------------------------------
bool CpuProfile_Init()
{
	WNDCLASS wndclass;

	// set up our window class
	memset( &wndclass, 0, sizeof( wndclass ) );
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = CpuProfile_WndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 0;
	wndclass.hInstance     = g_hInstance;
	wndclass.hIcon         = g_hIcons[ICON_APPLICATION];
	wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = g_hBackgroundBrush;
	wndclass.lpszMenuName  = MAKEINTRESOURCE( MENU_CPUPROFILE );
	wndclass.lpszClassName = "CPUPROFILESAMPLESCLASS";
	if ( !RegisterClass( &wndclass ) )
		return false;

	// set up our window class
	memset( &wndclass, 0, sizeof( wndclass ) );
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = CpuProfile_WndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 0;
	wndclass.hInstance     = g_hInstance;
	wndclass.hIcon         = g_hIcons[ICON_APPLICATION];
	wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = g_hBackgroundBrush;
	wndclass.lpszMenuName  = MAKEINTRESOURCE( MENU_CPUPROFILE );
	wndclass.lpszClassName = "CPUPROFILEHISTORYCLASS";
	if ( !RegisterClass( &wndclass ) )
		return false;
	
	CpuProfile_LoadConfig();

	return true;
}

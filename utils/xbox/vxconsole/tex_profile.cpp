//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	TEX_PROFILE.CPP
//
//	Texture Profiling Display.
//=====================================================================================//
#include "vxconsole.h"

#define TEXPROFILE_MAXCOUNTERS				64
#define TEXPROFILE_MAXSAMPLES				512

#define TEXPROFILE_MAJORTICKSIZE			4
#define TEXPROFILE_MAJORTICKBYTES			( TEXPROFILE_MAJORTICKSIZE*1024.0f*1024.0f )

#define TEXPROFILE_HISTORY_MAJORTICKHEIGHT	100
#define TEXPROFILE_HISTORY_NUMMINORTICKS	3
#define TEXPROFILE_HISTORY_LABELWIDTH		50
#define TEXPROFILE_HISTORY_SCALESTEPS		5
#define TEXPROFILE_HISTORY_MINSCALE			0.3f
#define TEXPROFILE_HISTORY_MAXSCALE			3.0f

#define TEXPROFILE_SAMPLES_ITEMHEIGHT		15
#define TEXPROFILE_SAMPLES_BARHEIGHT		10
#define TEXPROFILE_SAMPLES_MAJORTICKWIDTH	200
#define TEXPROFILE_SAMPLES_LABELWIDTH		150
#define TEXPROFILE_SAMPLES_LABELGAP			5
#define TEXPROFILE_SAMPLES_NUMMINORTICKS	3
#define TEXPROFILE_SAMPLES_PEAKHOLDTIME		3000
#define TEXPROFILE_SAMPLES_SCALESTEPS		10
#define TEXPROFILE_SAMPLES_MINSCALE			0.3f
#define TEXPROFILE_SAMPLES_MAXSCALE			3.0f

#define ID_TEXPROFILE_SAMPLES			1
#define ID_TEXPROFILE_HISTORY			2

typedef struct
{
	unsigned int	samples[TEXPROFILE_MAXSAMPLES];
	unsigned int	peakSample;
	char			label[64];
	COLORREF		color;
} profileCounter_t;

HWND				g_texProfile_hWndSamples;
HWND				g_texProfile_hWndHistory;
int					g_texProfile_numCounters;
profileCounter_t	g_texProfile_counters[TEXPROFILE_MAXCOUNTERS];
RECT				g_texProfile_samplesWindowRect;
RECT				g_texProfile_historyWindowRect;
DWORD				g_texProfile_lastPeakTime;
bool				g_texProfile_history_tickMarks = true;
bool				g_texProfile_history_colors = true;
int					g_texProfile_history_scale;
bool				g_texProfile_samples_tickMarks = true;
bool				g_texProfile_samples_colors = true;
int					g_texProfile_samples_scale;
int					g_texProfile_numSamples;
int					g_texProfile_currentFrame;

//-----------------------------------------------------------------------------
//	TexProfile_LoadConfig	
// 
//-----------------------------------------------------------------------------
void TexProfile_LoadConfig()
{
	int		numArgs;
	char	buff[256];

	// profile samples
	Sys_GetRegistryString( "texProfileSamplesWindowRect", buff, "", sizeof( buff ) );
	numArgs = sscanf( buff, "%d %d %d %d", &g_texProfile_samplesWindowRect.left, &g_texProfile_samplesWindowRect.top, &g_texProfile_samplesWindowRect.right, &g_texProfile_samplesWindowRect.bottom );
	if ( numArgs != 4 || g_texProfile_samplesWindowRect.left < 0 || g_texProfile_samplesWindowRect.top < 0 || g_texProfile_samplesWindowRect.right < 0 || g_texProfile_samplesWindowRect.bottom < 0 )
		memset( &g_texProfile_samplesWindowRect, 0, sizeof( g_texProfile_samplesWindowRect ) );
	Sys_GetRegistryInteger( "texProfileSamplesScale", 0, g_texProfile_samples_scale );
	if ( g_texProfile_samples_scale < -TEXPROFILE_SAMPLES_SCALESTEPS || g_texProfile_samples_scale > TEXPROFILE_SAMPLES_SCALESTEPS )
		g_texProfile_samples_scale = 0;

	// profile history
	Sys_GetRegistryString( "texProfileHistoryWindowRect", buff, "", sizeof( buff ) );
	numArgs = sscanf( buff, "%d %d %d %d", &g_texProfile_historyWindowRect.left, &g_texProfile_historyWindowRect.top, &g_texProfile_historyWindowRect.right, &g_texProfile_historyWindowRect.bottom );
	if ( numArgs != 4 || g_texProfile_historyWindowRect.left < 0 || g_texProfile_historyWindowRect.top < 0 || g_texProfile_historyWindowRect.right < 0 || g_texProfile_historyWindowRect.bottom < 0 )
		memset( &g_texProfile_historyWindowRect, 0, sizeof( g_texProfile_historyWindowRect ) );
	Sys_GetRegistryInteger( "texProfileHistoryScale", 0, g_texProfile_history_scale );
	if ( g_texProfile_history_scale < -TEXPROFILE_HISTORY_SCALESTEPS || g_texProfile_history_scale > TEXPROFILE_HISTORY_SCALESTEPS )
		g_texProfile_history_scale = 0;

	Sys_GetRegistryInteger( "texProfileCurrentFrame", 0, g_texProfile_currentFrame );
}

//-----------------------------------------------------------------------------
//	TexProfile_SaveConfig
// 
//-----------------------------------------------------------------------------
void TexProfile_SaveConfig()
{
	char			buff[256];
	WINDOWPLACEMENT wp;

	// profile samples
	if ( g_texProfile_hWndSamples )
	{
		memset( &wp, 0, sizeof( wp ) );
		wp.length = sizeof( WINDOWPLACEMENT );
		GetWindowPlacement( g_texProfile_hWndSamples, &wp );
		g_texProfile_samplesWindowRect = wp.rcNormalPosition;
		sprintf( buff, "%d %d %d %d", wp.rcNormalPosition.left, wp.rcNormalPosition.top, wp.rcNormalPosition.right, wp.rcNormalPosition.bottom );
		Sys_SetRegistryString( "texProfileSamplesWindowRect", buff );
	}
	Sys_SetRegistryInteger( "texProfileSamplesScale", g_texProfile_samples_scale );

	// profile history
	if ( g_texProfile_hWndHistory )
	{
		memset( &wp, 0, sizeof( wp ) );
		wp.length = sizeof( WINDOWPLACEMENT );
		GetWindowPlacement( g_texProfile_hWndHistory, &wp );
		g_texProfile_historyWindowRect = wp.rcNormalPosition;
		sprintf( buff, "%d %d %d %d", wp.rcNormalPosition.left, wp.rcNormalPosition.top, wp.rcNormalPosition.right, wp.rcNormalPosition.bottom );
		Sys_SetRegistryString( "texProfileHistoryWindowRect", buff );
	}
	Sys_SetRegistryInteger( "texProfileHistoryScale", g_texProfile_history_scale );

	Sys_SetRegistryInteger( "texProfileCurrentFrame", g_texProfile_currentFrame );
}

//-----------------------------------------------------------------------------
//	TexProfile_SetTitle
// 
//-----------------------------------------------------------------------------
void TexProfile_SetTitle()
{
	char	titleBuff[128];

	if ( g_texProfile_hWndSamples )
	{
		strcpy( titleBuff, "D3D Usage Snapshot" );
		if ( VProf_GetState() == VPROF_TEXTURE || VProf_GetState() == VPROF_TEXTUREFRAME )
			strcat( titleBuff, " [ON]" );
		if ( g_texProfile_currentFrame )
			strcat( titleBuff, " [FRAME]" );

		SetWindowText( g_texProfile_hWndSamples, titleBuff );
	}

	if ( g_texProfile_hWndHistory )
	{
		strcpy( titleBuff, "D3D Usage History" );
		if ( VProf_GetState() == VPROF_TEXTURE || VProf_GetState() == VPROF_TEXTUREFRAME )
			strcat( titleBuff, " [ON]" );
		if ( g_texProfile_currentFrame )
			strcat( titleBuff, " [FRAME]" );

		SetWindowText( g_texProfile_hWndHistory, titleBuff );
	}
}

//-----------------------------------------------------------------------------
// TexProfile_UpdateWindow
//
//-----------------------------------------------------------------------------
void TexProfile_UpdateWindow()
{
	if ( g_texProfile_hWndSamples && !IsIconic( g_texProfile_hWndSamples ) )
	{
		// visible - force a client repaint
		InvalidateRect( g_texProfile_hWndSamples, NULL, true );
	}

	if ( g_texProfile_hWndHistory && !IsIconic( g_texProfile_hWndHistory ) )
	{
		// visible - force a client repaint
		InvalidateRect( g_texProfile_hWndHistory, NULL, true );
	}
}

//-----------------------------------------------------------------------------
// rc_SetTexProfile
//
//-----------------------------------------------------------------------------
int rc_SetTexProfile( char* commandPtr )
{
	int				i;
	char*			cmdToken;
	int				retAddr;
	int				retVal;
	int				errCode = -1;
	xrProfile_t*	localList;
	int				profileList;
	int				numProfiles;

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

	g_texProfile_numCounters = numProfiles;
	if ( g_texProfile_numCounters > TEXPROFILE_MAXCOUNTERS-1 )
		g_texProfile_numCounters = TEXPROFILE_MAXCOUNTERS-1;
	
	for ( i=0; i<g_texProfile_numCounters; i++ )
	{
		// swap the structure
		localList[i].color = BigDWord( localList[i].color );

		// clear the old counter
		memset( &g_texProfile_counters[i], 0, sizeof( profileCounter_t ) );

		V_strncpy( g_texProfile_counters[i].label, localList[i].labelString, sizeof( g_texProfile_counters[i].label ) );
		g_texProfile_counters[i].color = localList[i].color;
	}

	// build out the reserved last counter as total count
	memset( &g_texProfile_counters[g_texProfile_numCounters], 0, sizeof( profileCounter_t ) );
	strcpy( g_texProfile_counters[g_texProfile_numCounters].label, "Total" );
	g_texProfile_counters[i].color = RGB( 255,255,255 );
	g_texProfile_numCounters++;
	
	// set the return code
	retVal = g_texProfile_numCounters-1;
	int xboxRetVal = BigDWord( retVal );
	DmSetMemory( ( void* )retAddr, sizeof( int ), &xboxRetVal, NULL );

	DebugCommand( "0x%8.8x = SetTexProfile( 0x%8.8x, 0x%8.8x )\n", retVal, numProfiles, profileList );

	delete [] localList;

	// success
	errCode = 0;

cleanUp:
	return ( errCode );
}

//-----------------------------------------------------------------------------
// rc_SetTexProfileData
//
//-----------------------------------------------------------------------------
int rc_SetTexProfileData( char* commandPtr )
{
	int				i;
	char*			cmdToken;
	int				errCode = -1;
	int				counters;
	int				currentSample;
	int				total;
	bool			newPeaks;
	unsigned int	localCounters[TEXPROFILE_MAXCOUNTERS];
	DWORD			newTime;

	// get profiles
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &counters );

	// get the caller's profile list 
	if ( g_texProfile_numCounters )
		DmGetMemory( ( void* )counters, ( g_texProfile_numCounters-1 )*sizeof( int ), localCounters, NULL );

	// timeout peaks
	newTime = Sys_GetSystemTime();
	if ( newTime - g_texProfile_lastPeakTime > TEXPROFILE_SAMPLES_PEAKHOLDTIME )
	{
		g_texProfile_lastPeakTime = newTime;
		newPeaks = true;
	} 
	else
		newPeaks = false;

	// next sample
	currentSample = g_texProfile_numSamples % TEXPROFILE_MAXSAMPLES;
	g_texProfile_numSamples++;

	total = 0;
	for ( i=0; i<g_texProfile_numCounters; i++ )
	{
		if ( i != g_texProfile_numCounters-1 )
		{
			g_texProfile_counters[i].samples[currentSample] = localCounters[i];
			total += localCounters[i];
		}
		else
		{
			// reserved total counter
			g_texProfile_counters[i].samples[currentSample] = total;
		}

		if ( newPeaks || g_texProfile_counters[i].peakSample < g_texProfile_counters[i].samples[currentSample] )
			g_texProfile_counters[i].peakSample = g_texProfile_counters[i].samples[currentSample];
	}

	DebugCommand( "SetTexProfileData( 0x%8.8x )\n", counters );

	TexProfile_UpdateWindow();

	// success
	errCode = 0;

cleanUp:
	return ( errCode );
}

//-----------------------------------------------------------------------------
//	TexProfile_ZoomIn
// 
//-----------------------------------------------------------------------------
void TexProfile_ZoomIn( int& scale, int numSteps )
{
	scale++;
	if ( scale > numSteps )
	{
		scale = numSteps;
		return;
	}
	TexProfile_UpdateWindow();
}

//-----------------------------------------------------------------------------
//	TexProfile_ZoomOut
// 
//-----------------------------------------------------------------------------
void TexProfile_ZoomOut( int& scale, int numSteps )
{
	scale--;
	if ( scale < -numSteps )
	{
		scale = -numSteps;
		return;
	}
	TexProfile_UpdateWindow();
}

//-----------------------------------------------------------------------------
//	TexProfile_CalcScale
// 
//-----------------------------------------------------------------------------
float TexProfile_CalcScale( int scale, int numSteps, float min, float max )
{
	float t;

	// from integral scale [-numSteps..numSteps] to float scale [min..max]
	t = ( float )( scale + numSteps )/( float )( 2*numSteps );
	t = min + t*( max-min );

	return t;
}

//-----------------------------------------------------------------------------
//	TexProfileSamples_Draw
// 
//-----------------------------------------------------------------------------
void TexProfileSamples_Draw( HDC hdc, RECT* clientRect )
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
	float	sample;
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
	int		tickWidth;
	int		windowWidth;
	int		windowHeight;

	hBlackPen = CreatePen( PS_SOLID, 1, RGB( 0,0,0 ) );
	hGreyPen  = CreatePen( PS_SOLID, 1, Sys_ColorScale( g_backgroundColor, 0.85f ) );
	hPenOld   = ( HPEN )SelectObject( hdc, hBlackPen );
	hFontOld  = SelectFont( hdc, g_hProportionalFont );

	SetBkColor( hdc, g_backgroundColor );

	// zoom
	scale        = TexProfile_CalcScale( g_texProfile_samples_scale, TEXPROFILE_SAMPLES_SCALESTEPS, TEXPROFILE_SAMPLES_MINSCALE, TEXPROFILE_SAMPLES_MAXSCALE );
	tickWidth    = ( int )( TEXPROFILE_SAMPLES_MAJORTICKWIDTH*scale );
	windowWidth  = clientRect->right-clientRect->left;
	windowHeight = clientRect->bottom-clientRect->top;

	numTicks = ( windowWidth-TEXPROFILE_SAMPLES_LABELWIDTH )/tickWidth + 1;
	if ( numTicks < 0 )
		numTicks = 1;

	rect.left   = 0;
	rect.right  = TEXPROFILE_SAMPLES_LABELWIDTH;
	rect.top    = 0;
	rect.bottom = TEXPROFILE_SAMPLES_ITEMHEIGHT;
	DrawText( hdc, "Name", -1, &rect, DT_LEFT );

	// draw size ticks
	x = TEXPROFILE_SAMPLES_LABELWIDTH;
	y = 0;
	for ( i=0; i<numTicks; i++ )
	{
		// tick labels
		rect.left   = x-40;
		rect.right  = x+40;
		rect.top    = y;
		rect.bottom = y+TEXPROFILE_SAMPLES_ITEMHEIGHT;
		sprintf( labelBuff, "%dMB", i*TEXPROFILE_MAJORTICKSIZE );
		DrawText( hdc, labelBuff, -1, &rect, DT_CENTER );

		// major ticks
		x0 = x;
		y0 = y + TEXPROFILE_SAMPLES_ITEMHEIGHT;
		SelectObject( hdc, hBlackPen );
		MoveToEx( hdc, x0, y0, NULL );
		LineTo( hdc, x0, y0+windowHeight );

		if ( g_texProfile_samples_tickMarks &&  g_texProfile_samples_scale > -TEXPROFILE_SAMPLES_SCALESTEPS )
		{
			// minor ticks
			x0 = x;
			y0 = y + TEXPROFILE_SAMPLES_ITEMHEIGHT;
			SelectObject( hdc, hGreyPen );
			for ( j=0; j<TEXPROFILE_SAMPLES_NUMMINORTICKS; j++ )
			{
				x0 += tickWidth/( TEXPROFILE_SAMPLES_NUMMINORTICKS+1 );

				MoveToEx( hdc, x0, y0, NULL );
				LineTo( hdc, x0, y0+windowHeight );
			}
		}
		x += tickWidth;
	}

	// seperator
	SelectObject( hdc, hBlackPen );
	MoveToEx( hdc, 0, TEXPROFILE_SAMPLES_ITEMHEIGHT, NULL );
	LineTo( hdc, windowWidth, TEXPROFILE_SAMPLES_ITEMHEIGHT );

	// draw labels
	x = 0;
	y = TEXPROFILE_SAMPLES_ITEMHEIGHT;
	for ( i=0; i<g_texProfile_numCounters; i++ )
	{
		if ( !g_texProfile_counters[i].label )
			continue;

		rect.left   = x;
		rect.right  = x+TEXPROFILE_SAMPLES_LABELWIDTH-TEXPROFILE_SAMPLES_LABELGAP;
		rect.top    = y;
		rect.bottom = y+TEXPROFILE_SAMPLES_ITEMHEIGHT;
		DrawText( hdc, g_texProfile_counters[i].label, -1, &rect, DT_VCENTER|DT_RIGHT|DT_SINGLELINE|DT_END_ELLIPSIS|DT_MODIFYSTRING );

		// draw the under line
		MoveToEx( hdc, x, y+TEXPROFILE_SAMPLES_ITEMHEIGHT, NULL );
		LineTo( hdc, x+TEXPROFILE_SAMPLES_LABELWIDTH, y+TEXPROFILE_SAMPLES_ITEMHEIGHT );

		y += TEXPROFILE_SAMPLES_ITEMHEIGHT;
	}

	// draw bars
	SelectObject( hdc, hBlackPen );
	x = TEXPROFILE_SAMPLES_LABELWIDTH;
	y = TEXPROFILE_SAMPLES_ITEMHEIGHT;
	currentSample = g_texProfile_numSamples-1;
	if ( currentSample < 0 )
		currentSample = 0;
	else
		currentSample %= TEXPROFILE_MAXSAMPLES;
	for ( i=0; i<g_texProfile_numCounters; i++ )
	{
		if ( !g_texProfile_counters[i].label )
			continue;

		hColoredBrush = CreateSolidBrush( g_texProfile_samples_colors ? g_texProfile_counters[i].color : g_backgroundColor );
		hbrushOld = ( HBRUSH )SelectObject( hdc, hColoredBrush );

		// bar - count is in bytes, scale to major tick
		t  = ( float )g_texProfile_counters[i].samples[currentSample]/TEXPROFILE_MAJORTICKBYTES;
		w  = ( int )( t * ( float )tickWidth );
		if ( w > windowWidth )
			w = windowWidth;
		x0 = x;
		y0 = y + ( TEXPROFILE_SAMPLES_ITEMHEIGHT-TEXPROFILE_SAMPLES_BARHEIGHT )/2 + 1;
		Rectangle( hdc, x0, y0, x0 + w, y0 + TEXPROFILE_SAMPLES_BARHEIGHT );

		// peak
		t  = ( float )g_texProfile_counters[i].peakSample/TEXPROFILE_MAJORTICKBYTES;
		w  = ( int )( t * ( float )tickWidth );
		if ( w > windowWidth )
			w = windowWidth;
		x0 = x + w;
		y0 = y + TEXPROFILE_SAMPLES_ITEMHEIGHT/2 + 1;

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

		// draw peak sizes
		sample = ( float )g_texProfile_counters[i].peakSample/1024.0f;
		if ( sample >= 0.01F )
		{
			sprintf( labelBuff, "%.2f MB", sample/1024.0f );
			rect.left   = x0 + 8;
			rect.right  = x0 + 8 + 100;
			rect.top    = y;
			rect.bottom = y + TEXPROFILE_SAMPLES_ITEMHEIGHT;
			DrawText( hdc, labelBuff, -1, &rect, DT_VCENTER|DT_LEFT|DT_SINGLELINE );
		}

		y += TEXPROFILE_SAMPLES_ITEMHEIGHT;
	}

	SelectObject( hdc, hFontOld );
	SelectObject( hdc, hPenOld );
	DeleteObject( hBlackPen );
	DeleteObject( hGreyPen );
}

//-----------------------------------------------------------------------------
//	TexProfileHistory_Draw
// 
//-----------------------------------------------------------------------------
void TexProfileHistory_Draw( HDC hdc, RECT* clientRect )
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
	int		tickHeight;
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
	scale        = TexProfile_CalcScale( g_texProfile_history_scale, TEXPROFILE_HISTORY_SCALESTEPS, TEXPROFILE_HISTORY_MINSCALE, TEXPROFILE_HISTORY_MAXSCALE );
	tickHeight   = ( int )( TEXPROFILE_HISTORY_MAJORTICKHEIGHT*scale );
	windowWidth  = clientRect->right-clientRect->left;
	windowHeight = clientRect->bottom-clientRect->top;

	numTicks = windowHeight/tickHeight + 2;
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

		if ( g_texProfile_history_tickMarks && g_texProfile_history_scale > -TEXPROFILE_HISTORY_SCALESTEPS )
		{
			// minor ticks
			y0 = y;
			SelectObject( hdc, hGreyPen );
			for ( j=0; j<TEXPROFILE_HISTORY_NUMMINORTICKS; j++ )
			{
				y0 += tickHeight/( TEXPROFILE_SAMPLES_NUMMINORTICKS+1 );
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
			sprintf( labelBuff, "%dMB", i*TEXPROFILE_MAJORTICKSIZE );
			DrawText( hdc, labelBuff, -1, &rect, DT_RIGHT|DT_SINGLELINE|DT_BOTTOM );
		}

		y -= tickHeight;
	}

	// vertical bars
	if ( g_texProfile_numSamples )
	{
		SelectObject( hdc, hNullPen );

		numbars       = windowWidth-TEXPROFILE_HISTORY_LABELWIDTH;
		currentSample = g_texProfile_numSamples-1;
		for ( x=numbars-1; x>=0; x-=4 )
		{
			// all the counters at this sample
			y = windowHeight;
			for ( j=0; j<g_texProfile_numCounters-1; j++ )
			{
				if ( !g_texProfile_counters[j].label )
					continue;

				t  = ( float )g_texProfile_counters[j].samples[currentSample % TEXPROFILE_MAXSAMPLES]/TEXPROFILE_MAJORTICKBYTES;
				h  = ( int )( t * ( float )tickHeight );
				if ( h )
				{
					if ( h > windowHeight )
						h = windowHeight;

					hColoredBrush = CreateSolidBrush( g_texProfile_history_colors ? g_texProfile_counters[j].color : RGB( 80,80,80 ) );
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
//	TexProfile_WndProc
// 
//-----------------------------------------------------------------------------
LRESULT CALLBACK TexProfile_WndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    WORD			wID = LOWORD( wParam );
	HDC				hdc;
	PAINTSTRUCT		ps; 
	RECT			rect;
	int				id;
	bool			bIsSamples;
	bool			bIsHistory;
	CREATESTRUCT	*createStructPtr;
	bool			bIsEnabled;

	// identify window
	id = ( int )GetWindowLong( hwnd, GWL_USERDATA+0 );
	bIsSamples = ( id == ID_TEXPROFILE_SAMPLES );
	bIsHistory = ( id == ID_TEXPROFILE_HISTORY );
	
	switch ( message )
	{
		case WM_CREATE:
			// set the window identifier
			createStructPtr = ( CREATESTRUCT* )lParam;
			SetWindowLong( hwnd, GWL_USERDATA+0, ( LONG )createStructPtr->lpCreateParams );

			// reset peaks
			g_texProfile_lastPeakTime = 0;
			return 0L;

		case WM_DESTROY:
			TexProfile_SaveConfig();

			if ( bIsSamples )
				g_texProfile_hWndSamples = NULL;
			else if ( bIsHistory )
				g_texProfile_hWndHistory = NULL;

			if ( VProf_GetState() == VPROF_TEXTURE || VProf_GetState() == VPROF_TEXTUREFRAME )
			{
				VProf_Enable( VPROF_OFF );
			}
			return 0L;
	
        case WM_INITMENU:
			if ( bIsSamples )
			{
				CheckMenuItem( ( HMENU )wParam, IDM_TEXPROFILE_TICKMARKS, MF_BYCOMMAND | ( g_texProfile_samples_tickMarks ? MF_CHECKED : MF_UNCHECKED ) );
				CheckMenuItem( ( HMENU )wParam, IDM_TEXPROFILE_COLORS, MF_BYCOMMAND | ( g_texProfile_samples_colors ? MF_CHECKED : MF_UNCHECKED ) );
			}
			else if ( bIsHistory )
			{
				CheckMenuItem( ( HMENU )wParam, IDM_TEXPROFILE_TICKMARKS, MF_BYCOMMAND | ( g_texProfile_history_tickMarks ? MF_CHECKED : MF_UNCHECKED ) );
				CheckMenuItem( ( HMENU )wParam, IDM_TEXPROFILE_COLORS, MF_BYCOMMAND | ( g_texProfile_history_colors ? MF_CHECKED : MF_UNCHECKED ) );
			}
            CheckMenuItem( ( HMENU )wParam, IDM_TEXPROFILE_ENABLE, MF_BYCOMMAND | ( ( VProf_GetState() == VPROF_TEXTURE || VProf_GetState() == VPROF_TEXTUREFRAME ) ? MF_CHECKED : MF_UNCHECKED ) );
			CheckMenuItem( ( HMENU )wParam, IDM_TEXPROFILE_CURRENTFRAME, MF_BYCOMMAND | ( g_texProfile_currentFrame ? MF_CHECKED : MF_UNCHECKED ) );
			return 0L;

		case WM_PAINT:
			GetClientRect( hwnd, &rect );
			hdc = BeginPaint( hwnd, &ps ); 
			if ( bIsSamples )
				TexProfileSamples_Draw( hdc, &rect );
			else if ( bIsHistory )
				TexProfileHistory_Draw( hdc, &rect );
            EndPaint( hwnd, &ps ); 
			return 0L;

		case WM_SIZE:
			// force a redraw
			TexProfile_UpdateWindow();
			return 0L;
	
		case WM_KEYDOWN:
			switch ( wParam )
			{
				case VK_INSERT:
					if ( bIsSamples )
						TexProfile_ZoomIn( g_texProfile_samples_scale, TEXPROFILE_SAMPLES_SCALESTEPS );
					else if ( bIsHistory )
						TexProfile_ZoomIn( g_texProfile_history_scale, TEXPROFILE_HISTORY_SCALESTEPS );
					return 0L;

				case VK_DELETE:
					if ( bIsSamples )
						TexProfile_ZoomOut( g_texProfile_samples_scale, TEXPROFILE_SAMPLES_SCALESTEPS );
					else if ( bIsHistory )
						TexProfile_ZoomOut( g_texProfile_history_scale, TEXPROFILE_HISTORY_SCALESTEPS );
					return 0L;
			}
			break;

        case WM_COMMAND:
            switch ( wID )
            {
				case IDM_TEXPROFILE_TICKMARKS:
					if ( bIsSamples )
						g_texProfile_samples_tickMarks ^= 1;
					else if ( bIsHistory )
						g_texProfile_history_tickMarks ^= 1;
					TexProfile_UpdateWindow();
					return 0L;

				case IDM_TEXPROFILE_COLORS:
					if ( bIsSamples )
						g_texProfile_samples_colors ^= 1;
					else if ( bIsHistory )
						g_texProfile_history_colors ^= 1;
					TexProfile_UpdateWindow();
					return 0L;

				case IDM_TEXPROFILE_ZOOMIN:
					if ( bIsSamples )
						TexProfile_ZoomIn( g_texProfile_samples_scale, TEXPROFILE_SAMPLES_SCALESTEPS );
					else if ( bIsHistory )
						TexProfile_ZoomIn( g_texProfile_history_scale, TEXPROFILE_HISTORY_SCALESTEPS );
					return 0L;

				case IDM_TEXPROFILE_ZOOMOUT:
					if ( bIsSamples )
						TexProfile_ZoomOut( g_texProfile_samples_scale, TEXPROFILE_SAMPLES_SCALESTEPS );
					else if ( bIsHistory )
						TexProfile_ZoomOut( g_texProfile_history_scale, TEXPROFILE_HISTORY_SCALESTEPS );
					return 0L;

				case IDM_TEXPROFILE_ENABLE:
					bIsEnabled = ( VProf_GetState() == VPROF_TEXTURE || VProf_GetState() == VPROF_TEXTUREFRAME );
					bIsEnabled ^= 1;
					if ( !bIsEnabled )
						VProf_Enable( VPROF_OFF );
					else
					{
						if ( !g_texProfile_currentFrame )
							VProf_Enable( VPROF_TEXTURE );
						else
							VProf_Enable( VPROF_TEXTUREFRAME );
					}
					TexProfile_SetTitle();
					return 0L;

				case IDM_TEXPROFILE_CURRENTFRAME:
					bIsEnabled = ( VProf_GetState() == VPROF_TEXTURE || VProf_GetState() == VPROF_TEXTUREFRAME );
					g_texProfile_currentFrame ^= 1;
					if ( bIsEnabled )
					{
						if ( !g_texProfile_currentFrame )
							VProf_Enable( VPROF_TEXTURE );
						else
							VProf_Enable( VPROF_TEXTUREFRAME );
					}
					TexProfile_SetTitle();
					return 0L;
			}
			break;
	}	
	return ( DefWindowProc( hwnd, message, wParam, lParam ) );
}

//-----------------------------------------------------------------------------
//	TexProfileHistory_Open
// 
//-----------------------------------------------------------------------------
void TexProfileHistory_Open()
{
	HWND	hWnd;
	
	if ( g_texProfile_hWndHistory )
	{
		// only one profile instance
		if ( IsIconic( g_texProfile_hWndHistory ) )
			ShowWindow( g_texProfile_hWndHistory, SW_RESTORE );
		SetForegroundWindow( g_texProfile_hWndHistory );
		return;
	}

	if ( VProf_GetState() == VPROF_OFF )
	{
		if ( !g_texProfile_currentFrame )
			VProf_Enable( VPROF_TEXTURE );
		else
			VProf_Enable( VPROF_TEXTUREFRAME );
	}

	hWnd = CreateWindowEx( 
				WS_EX_CLIENTEDGE,
				"TEXPROFILEHISTORYCLASS",
				"",
				WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX|WS_MINIMIZEBOX|WS_MAXIMIZEBOX,
				0,
				0,
				600,
				500,
				g_hDlgMain,
				NULL,
				g_hInstance,
				( void* )ID_TEXPROFILE_HISTORY );
	g_texProfile_hWndHistory = hWnd;

	TexProfile_SetTitle();

	if ( g_texProfile_historyWindowRect.right && g_texProfile_historyWindowRect.bottom )
		MoveWindow( g_texProfile_hWndHistory, g_texProfile_historyWindowRect.left, g_texProfile_historyWindowRect.top, g_texProfile_historyWindowRect.right-g_texProfile_historyWindowRect.left, g_texProfile_historyWindowRect.bottom-g_texProfile_historyWindowRect.top, FALSE );
	ShowWindow( g_texProfile_hWndHistory, SHOW_OPENWINDOW );
}

//-----------------------------------------------------------------------------
//	TexProfileSamples_Open
// 
//-----------------------------------------------------------------------------
void TexProfileSamples_Open()
{
	HWND	hWnd;
	
	if ( g_texProfile_hWndSamples )
	{
		// only one profile instance
		if ( IsIconic( g_texProfile_hWndSamples ) )
			ShowWindow( g_texProfile_hWndSamples, SW_RESTORE );
		SetForegroundWindow( g_texProfile_hWndSamples );
		return;
	}

	if ( VProf_GetState() == VPROF_OFF )
	{
		if ( !g_texProfile_currentFrame )
			VProf_Enable( VPROF_TEXTURE );
		else
			VProf_Enable( VPROF_TEXTUREFRAME );
	}

	hWnd = CreateWindowEx( 
				WS_EX_CLIENTEDGE,
				"TEXPROFILESAMPLESCLASS",
				"",
				WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX|WS_MINIMIZEBOX|WS_MAXIMIZEBOX,
				0,
				0,
				600,
				500,
				g_hDlgMain,
				NULL,
				g_hInstance,
				( void* )ID_TEXPROFILE_SAMPLES );
	g_texProfile_hWndSamples = hWnd;

	TexProfile_SetTitle();

	if ( g_texProfile_samplesWindowRect.right && g_texProfile_samplesWindowRect.bottom )
		MoveWindow( g_texProfile_hWndSamples, g_texProfile_samplesWindowRect.left, g_texProfile_samplesWindowRect.top, g_texProfile_samplesWindowRect.right-g_texProfile_samplesWindowRect.left, g_texProfile_samplesWindowRect.bottom-g_texProfile_samplesWindowRect.top, FALSE );
	ShowWindow( g_texProfile_hWndSamples, SHOW_OPENWINDOW );
}

//-----------------------------------------------------------------------------
//	TexProfile_Clear
// 
//-----------------------------------------------------------------------------
void TexProfile_Clear()
{
	// clear counters and history
	g_texProfile_numCounters = 0;
	g_texProfile_numSamples  = 0;

	TexProfile_UpdateWindow();
}

//-----------------------------------------------------------------------------
//	TexProfile_Init
// 
//-----------------------------------------------------------------------------
bool TexProfile_Init()
{
	WNDCLASS wndclass;

	// set up our window class
	memset( &wndclass, 0, sizeof( wndclass ) );
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = TexProfile_WndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 0;
	wndclass.hInstance     = g_hInstance;
	wndclass.hIcon         = g_hIcons[ICON_APPLICATION];
	wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = g_hBackgroundBrush;
	wndclass.lpszMenuName  = MAKEINTRESOURCE( MENU_TEXPROFILE );
	wndclass.lpszClassName = "TEXPROFILESAMPLESCLASS";
	if ( !RegisterClass( &wndclass ) )
		return false;

	// set up our window class
	memset( &wndclass, 0, sizeof( wndclass ) );
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = TexProfile_WndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 0;
	wndclass.hInstance     = g_hInstance;
	wndclass.hIcon         = g_hIcons[ICON_APPLICATION];
	wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = g_hBackgroundBrush;
	wndclass.lpszMenuName  = MAKEINTRESOURCE( MENU_TEXPROFILE );
	wndclass.lpszClassName = "TEXPROFILEHISTORYCLASS";
	if ( !RegisterClass( &wndclass ) )
		return false;
	
	TexProfile_LoadConfig();

	return true;
}
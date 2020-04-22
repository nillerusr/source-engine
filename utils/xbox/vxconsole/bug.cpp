//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	BUG.CPP
//
//	Tracker bridge
//=====================================================================================//
#include "vxconsole.h"

// two bug systems, certain games are tied to a specific system
#define BUG_REPORTER_DLLNAME_1		"bugreporter.dll" 
#define BUG_REPORTER_DLLNAME_2		"bugreporter_filequeue.dll" 

#define BUG_REPOSITORY_URL			"\\\\fileserver\\bugs"
#define REPOSITORY_VALIDATION_FILE	"info.txt"
#define BUG_ERRORTITLE				"Bug Error"
#define BUG_COLOR					( RGB( 0,255,255 ) )

HWND			g_bug_hWnd;
HMODULE			g_bug_hBugReporter1;
HMODULE			g_bug_hBugReporter2;
IBugReporter	*g_bug_pReporter1;
IBugReporter	*g_bug_pReporter2;
IBugReporter	*g_bug_pReporter;
char			g_bug_szTitle[512];
char			g_bug_szDescription[1024];
char			g_bug_szOwner[128];
char			g_bug_szSeverity[128];
char			g_bug_szReportType[128];
char			g_bug_szPriority[128];
char			g_bug_szArea[128];
char			g_bug_szMapNumber[128];
int				g_bug_GameType;	
bool			g_bug_bActive;
char			g_bug_szScreenshot[MAX_PATH];
char			g_bug_szSavegame[MAX_PATH];
char			g_bug_szBSPName[MAX_PATH];
xrMapInfo_t		g_bug_mapInfo;
bool			g_bug_bCompressScreenshot;
bool			g_bug_bFirstCommand;

struct GameSystem_t
{
	const char *pFriendlyName;
	const char *pGameName;
	bool		bUsesSystem1;
};

GameSystem_t g_Games[] = 
{
	{ "Half Life 2",		"hl2",		true },
	{ "Episode 1",			"episodic",	true },
	{ "Episode 2",			"ep2",		true },
	{ "Portal",				"portal",	false },
	{ "Team Fortress 2",	"tf",		false },
};

static const char *GetRepositoryURL( void )
{
	const char *pURL = g_bug_pReporter->GetRepositoryURL();
	if ( pURL )
	{
		return pURL;
	}

	return BUG_REPOSITORY_URL;
}

const char *GetSubmissionURL( int bugid )
{
	const char *pURL = g_bug_pReporter->GetSubmissionURL();
	if ( pURL )
	{
		return pURL;
	}

	static char url[MAX_PATH];
	Q_snprintf( url, sizeof(url), "%s/%i", GetRepositoryURL(), bugid );

	return url;
}

//-----------------------------------------------------------------------------
//	BugReporter_SelectReporter
// 
//-----------------------------------------------------------------------------
bool BugReporter_SelectReporter( const char *pGameName )
{	
	int system = -1;
	for ( int i = 0; i < ARRAYSIZE( g_Games ); i++ )
	{
		if ( pGameName && !V_stricmp( g_Games[i].pGameName, pGameName ) )
		{
			system = i;
			break;
		}
	}

	if ( system == -1 )
	{
		// not found, slam to first
		system = 0;
	}

	// games uses different systems
	int oldGameType = g_bug_GameType;
	g_bug_GameType = system;
	if ( g_Games[system].bUsesSystem1 )
	{
		g_bug_pReporter = g_bug_pReporter1;
	}
	else
	{
		g_bug_pReporter = g_bug_pReporter2;
	}

	// force the game area
	if ( g_Games[g_bug_GameType].bUsesSystem1 )
	{
		strcpy( g_bug_szArea, "XBOX 360" );
	}
	else
	{
		strcpy( g_bug_szArea, g_Games[g_bug_GameType].pFriendlyName );
	}

	bool bChanged = ( oldGameType != g_bug_GameType );
	return bChanged;
}

//-----------------------------------------------------------------------------
//	BugReporter_LoadDLL
// 
//-----------------------------------------------------------------------------
IBugReporter *BugReporter_LoadDLL( const char *pDLLName, HMODULE *phModule )
{
	HMODULE hModule;
	IBugReporter *pBugReporter;

	*phModule = NULL;

	hModule = LoadLibrary( pDLLName );
	if ( !hModule )
	{
		Sys_MessageBox( BUG_ERRORTITLE, "Could not open '%s'\n", pDLLName );
		return NULL;
	}

	FARPROC pCreateInterface = GetProcAddress( hModule, CREATEINTERFACE_PROCNAME );
	if ( !pCreateInterface )
	{
		Sys_MessageBox( BUG_ERRORTITLE, "Missing '%s' interface for '%s'\n", CREATEINTERFACE_PROCNAME, pDLLName );
		return NULL;
	}

	pBugReporter = (IBugReporter *)((CreateInterfaceFn)pCreateInterface)( INTERFACEVERSION_BUGREPORTER, NULL );
	if ( !pBugReporter )
	{
		Sys_MessageBox( BUG_ERRORTITLE, "Missing interface '%s' for '%s'\n", INTERFACEVERSION_BUGREPORTER, pDLLName );
		return NULL;
	}

	bool bSuccess = pBugReporter->Init( NULL );
	if ( !bSuccess )
	{
		return NULL;
	}

	*phModule = hModule;
	return pBugReporter;
}

//-----------------------------------------------------------------------------
//	BugReporter_GetInterfaces
// 
//-----------------------------------------------------------------------------
bool BugReporter_GetInterfaces()
{
	if ( !g_bug_pReporter1 )
	{
		g_bug_pReporter1 = BugReporter_LoadDLL( BUG_REPORTER_DLLNAME_1, &g_bug_hBugReporter1 );
		if ( !g_bug_pReporter1 )
		{
			Sys_MessageBox( BUG_ERRORTITLE, "PVCS intialization failed!\n" );
		}
	}

	if ( !g_bug_pReporter2 )
	{
		g_bug_pReporter2 = BugReporter_LoadDLL( BUG_REPORTER_DLLNAME_2, &g_bug_hBugReporter2 );
		if ( !g_bug_pReporter2 )
		{
			Sys_MessageBox( BUG_ERRORTITLE, "BugBait intialization failed!\n" );
		}
	}

	if ( g_bug_pReporter1 )
	{
		// determine submitter name
		ConsoleWindowPrintf( BUG_COLOR, "Bug Reporter: PVCS Username: '%s' Display As: '%s'\n", g_bug_pReporter1->GetUserName(), g_bug_pReporter1->GetUserName_Display() );
	}
	if ( g_bug_pReporter2 )
	{
		// determine submitter name
		ConsoleWindowPrintf( BUG_COLOR, "Bug Reporter: BugBait Username: '%s' Display As: '%s'\n", g_bug_pReporter2->GetUserName(), g_bug_pReporter2->GetUserName_Display() );
	}
	
	BugReporter_SelectReporter( NULL );

	// See if we can see the bug repository right now
	char fn[MAX_PATH];
	V_snprintf( fn, sizeof( fn ), "%s/%s", GetRepositoryURL(), REPOSITORY_VALIDATION_FILE );
	Sys_NormalizePath( fn, false );

	FILE *fp = fopen( fn, "rb" );
	if ( fp )
	{
		ConsoleWindowPrintf( BUG_COLOR, "PVCS Repository '%s'\n", GetRepositoryURL() );
		fclose( fp );
	}
	else
	{
		Sys_MessageBox( BUG_ERRORTITLE, "Unable to see '%s', check permissions and network connectivity.\n", fn );
		return false;
	}

	// success
	return true;
}

//-----------------------------------------------------------------------------
//	BugReporter_FreeInterfaces
// 
//-----------------------------------------------------------------------------
void BugReporter_FreeInterfaces()
{
	if ( g_bug_pReporter1 )
	{
		g_bug_pReporter1->Shutdown();
		g_bug_pReporter1 = NULL;
		FreeLibrary( g_bug_hBugReporter1 );
		g_bug_hBugReporter1 = NULL;
	}

	if ( g_bug_pReporter2 )
	{
		g_bug_pReporter2->Shutdown();
		g_bug_pReporter2 = NULL;
		FreeLibrary( g_bug_hBugReporter2 );
		g_bug_hBugReporter2 = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Expanded data destination object for CUtlBuffer output
//-----------------------------------------------------------------------------
struct JPEGDestinationManager_t
{
	struct jpeg_destination_mgr pub; // public fields
	
	CUtlBuffer	*pBuffer;		// target/final buffer
	byte		*buffer;		// start of temp buffer
};

// choose an efficiently bufferaable size
#define OUTPUT_BUF_SIZE  4096	

//-----------------------------------------------------------------------------
// Purpose:  Initialize destination --- called by jpeg_start_compress
//  before any data is actually written.
//-----------------------------------------------------------------------------
void init_destination( j_compress_ptr cinfo )
{
	JPEGDestinationManager_t *dest = ( JPEGDestinationManager_t *) cinfo->dest;
	
	// Allocate the output buffer --- it will be released when done with image
	dest->buffer = (byte *)
		(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
		OUTPUT_BUF_SIZE * sizeof(byte));
	
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

//-----------------------------------------------------------------------------
// Purpose: Empty the output buffer --- called whenever buffer fills up.
// Input  : boolean - 
//-----------------------------------------------------------------------------
boolean empty_output_buffer( j_compress_ptr cinfo )
{
	JPEGDestinationManager_t *dest = ( JPEGDestinationManager_t * ) cinfo->dest;
	
	CUtlBuffer *buf = dest->pBuffer;

	// Add some data
	buf->Put( dest->buffer, OUTPUT_BUF_SIZE );
	
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
	
	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: Terminate destination --- called by jpeg_finish_compress
// after all data has been written.  Usually needs to flush buffer.
//
// NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
// application must deal with any cleanup that should happen even
// for error exit.
//-----------------------------------------------------------------------------
void term_destination( j_compress_ptr cinfo )
{
	JPEGDestinationManager_t *dest = (JPEGDestinationManager_t *) cinfo->dest;
	size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;
	
	CUtlBuffer *buf = dest->pBuffer;

	/* Write any data remaining in the buffer */
	if (datacount > 0) 
	{
		buf->Put( dest->buffer, datacount );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set up functions for writing data to a CUtlBuffer instead of FILE *
//-----------------------------------------------------------------------------
void jpeg_UtlBuffer_dest( j_compress_ptr cinfo, CUtlBuffer *pBuffer )
{
	JPEGDestinationManager_t *dest;
	
	/* The destination object is made permanent so that multiple JPEG images
	* can be written to the same file without re-executing jpeg_stdio_dest.
	* This makes it dangerous to use this manager and a different destination
	* manager serially with the same JPEG object, because their private object
	* sizes may be different.  Caveat programmer.
	*/
	if (cinfo->dest == NULL) {	/* first time for this JPEG object? */
		cinfo->dest = (struct jpeg_destination_mgr *)
			(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
			sizeof(JPEGDestinationManager_t));
	}
	
	dest = ( JPEGDestinationManager_t * ) cinfo->dest;

	dest->pub.init_destination		= init_destination;
	dest->pub.empty_output_buffer	= empty_output_buffer;
	dest->pub.term_destination		= term_destination;
	dest->pBuffer					= pBuffer;
}

//-----------------------------------------------------------------------------
//	BugDlg_CompressScreenshot
//
//	Compress .BMP to .JPG, Delete .BMP
//-----------------------------------------------------------------------------
bool BugDlg_CompressScreenshot()
{
	if ( !g_bug_szScreenshot[0] )
	{
		return false;
	}

	bool bSuccess = false;
	HBITMAP hBitmap = NULL;
	HDC hDC = NULL;
	char *pBMPBits = NULL;

	CUtlBuffer buf( 0, 0 );

	hBitmap = (HBITMAP)LoadImage( NULL, g_bug_szScreenshot, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_DEFAULTSIZE | LR_LOADFROMFILE );
	if ( !hBitmap )
		goto cleanUp;

	hDC = CreateCompatibleDC( NULL );
	if ( !hDC )
		goto cleanUp;

	BITMAPINFO bitmapInfo;
	ZeroMemory( &bitmapInfo, sizeof( BITMAPINFO ) );
	bitmapInfo.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );

	// populate the bmp info
	if ( !GetDIBits( hDC, hBitmap, 0, 0, NULL, &bitmapInfo, DIB_RGB_COLORS ) )
		goto cleanUp;

	pBMPBits = (char *)Sys_Alloc( bitmapInfo.bmiHeader.biSizeImage );
	if ( !pBMPBits )
		goto cleanUp;
	
	// could be bottom-up or top-down
	int nHeight = abs( bitmapInfo.bmiHeader.biHeight );

	if ( bitmapInfo.bmiHeader.biBitCount != 32 )
	{
		// unexpected format
		goto cleanUp;
	}

	if ( bitmapInfo.bmiHeader.biCompression != BI_RGB && bitmapInfo.bmiHeader.biCompression != BI_BITFIELDS )
	{
		// unexpected format
		goto cleanUp;
	}

	// don't want color masks
	bitmapInfo.bmiHeader.biCompression = BI_RGB;

	// get the raw bits
	if ( !GetDIBits( hDC, hBitmap, 0, nHeight, pBMPBits, &bitmapInfo, DIB_RGB_COLORS ) )
		goto cleanUp;

	JSAMPROW row_pointer[1]; 

	// compression data structure
	struct jpeg_compress_struct cinfo;
	ZeroMemory( &cinfo, sizeof( jpeg_compress_struct ) );

	// point at stderr
	struct jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error( &jerr );

	// create compressor
	jpeg_create_compress( &cinfo );

	// Hook CUtlBuffer to compression
	jpeg_UtlBuffer_dest( &cinfo, &buf );

	// image width and height, in pixels
	cinfo.image_width = bitmapInfo.bmiHeader.biWidth;
	cinfo.image_height = nHeight;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;

	// Apply settings
	jpeg_set_defaults( &cinfo );
	jpeg_set_quality( &cinfo, 50, TRUE );

	// Start compressor
	jpeg_start_compress( &cinfo, TRUE);
	
	char *pRowBuffer = (char*)_alloca( bitmapInfo.bmiHeader.biWidth * 3 );
	row_pointer[0] = (JSAMPROW)pRowBuffer;

	// Write scanlines
	while ( cinfo.next_scanline < cinfo.image_height ) 
	{
		char *pSrc;
		if ( bitmapInfo.bmiHeader.biHeight < 0 )
		{
			// top down
			pSrc = &pBMPBits[cinfo.next_scanline * bitmapInfo.bmiHeader.biWidth * 4];
		}
		else
		{
			// bottom up
			pSrc = &pBMPBits[(nHeight-1 - cinfo.next_scanline) * bitmapInfo.bmiHeader.biWidth * 4];
		}
		
		// convert to BGR to RGB
		char *pDst = pRowBuffer;
		for ( int i=0; i<bitmapInfo.bmiHeader.biWidth; i++ )
		{
			pDst[0] = pSrc[2];
			pDst[1] = pSrc[1];
			pDst[2] = pSrc[0];
			pSrc += 4;
			pDst += 3;
		}		
		jpeg_write_scanlines( &cinfo, row_pointer, 1 );
	}

	// Finalize image
	jpeg_finish_compress( &cinfo );

	char jpgFilename[MAX_PATH];
	Sys_StripExtension( g_bug_szScreenshot, jpgFilename, sizeof( jpgFilename ) );
	Sys_AddExtension( ".jpg", jpgFilename, sizeof( jpgFilename ) );
	if ( !Sys_SaveFile( jpgFilename, buf.Base(), buf.TellMaxPut() ) )
		goto cleanUp;

	// remove the uncompressed version
	unlink( g_bug_szScreenshot );
	strcpy( g_bug_szScreenshot, jpgFilename );

	bSuccess = true;

cleanUp:
	if ( hBitmap )
		DeleteObject( hBitmap );
	if ( hDC )
		DeleteDC( hDC );
	if ( pBMPBits )
		Sys_Free( pBMPBits );

	return bSuccess;
}


//-----------------------------------------------------------------------------
//	BugDlg_GetAppData
//
//-----------------------------------------------------------------------------
void BugDlg_GetAppData( HWND hWnd )
{
	// clear stale data from previous query
	memset( &g_bug_mapInfo, 0, sizeof( g_bug_mapInfo ) );

	SetDlgItemText( hWnd, IDC_BUG_POSITION_LABEL, "" );
	SetDlgItemText( hWnd, IDC_BUG_ORIENTATION_LABEL, "" );
	SetDlgItemText( hWnd, IDC_BUG_MAP_LABEL, "" );
	SetDlgItemText( hWnd, IDC_BUG_BUILD_LABEL, "" );

	EnableWindow( GetDlgItem( hWnd, IDC_BUG_SAVEGAME ), false );
	EnableWindow( GetDlgItem( hWnd, IDC_BUG_INCLUDEBSP ), false );
	
	if ( g_connectedToApp )
	{
		// send to app, responds with position info
		if ( !g_bug_bFirstCommand )
		{
			// first command must send pause and status to be processed correctly
			g_bug_bFirstCommand = true;
			ProcessCommand( "pause ; status" );
		}
		else
		{
			ProcessCommand( "status" );
		}
	}
}

//-----------------------------------------------------------------------------
//	BugDlg_GetDataFileBase
//
//-----------------------------------------------------------------------------
void BugDlg_GetDataFileBase( char const *suffix, bool bLocalPath, char *buf, int bufsize )
{
	char	filepath[MAX_PATH];
	char	filename[MAX_PATH];

	struct tm t;

	time_t ltime;
	time( &ltime );
	tm *pTime = localtime( &ltime );
	memcpy( &t, pTime, sizeof( t ) );

	char who[128];
	strncpy( who, suffix, sizeof( who ) );
	_strlwr( who );

	if ( bLocalPath )
	{
		// add a qualified local path 
		// used as a gathering store before uploading to server
		strcpy( filepath, g_localPath );
		Sys_AddFileSeperator( filepath, sizeof( filepath ) );
		strcat( filepath, "bug/" );
		Sys_NormalizePath( filepath, false );
		Sys_CreatePath( filepath );
	}
	else
	{
		filepath[0] = '\0';
	}

	sprintf( filename, "%i_%02i_%02i_%s", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, who );
	V_snprintf( buf, bufsize, "%s%s", filepath, filename );
}

//-----------------------------------------------------------------------------
//	BugDlg_CheckSubmit
//
//-----------------------------------------------------------------------------
bool BugDlg_CheckSubmit( HWND hWnd )
{
	bool bEnableSubmit = false;

	if ( g_Games[g_bug_GameType].bUsesSystem1 )
	{
		if ( g_bug_szTitle[0] &&
			g_bug_szDescription[0] &&
			g_bug_szSeverity[0] &&
			g_bug_szOwner[0] &&
			g_bug_szReportType[0] &&
			g_bug_szPriority[0] &&
			g_bug_szArea[0] )
		{
			bEnableSubmit = true;
		}
	}
	else
	{
		if ( g_bug_szTitle[0] &&
			g_bug_szDescription[0] &&
			g_bug_szSeverity[0] &&
			g_bug_szOwner[0] &&
			g_bug_szArea[0] &&
			g_bug_szMapNumber[0] )
		{
			bEnableSubmit = true;
		}
	}

	EnableWindow( GetDlgItem( hWnd, IDC_BUG_SUBMIT ), bEnableSubmit );

	return bEnableSubmit;
}

//-----------------------------------------------------------------------------
//	BugDlg_GetChanges
//
//-----------------------------------------------------------------------------
bool BugDlg_GetChanges( HWND hWnd )
{
	int curSel;

	// title
	GetDlgItemText( hWnd, IDC_BUG_TITLE, g_bug_szTitle, sizeof( g_bug_szTitle ) );

	// description
	GetDlgItemText( hWnd, IDC_BUG_DESCRIPTION, g_bug_szDescription, sizeof( g_bug_szDescription ) );

	// owner
	curSel = SendDlgItemMessage( hWnd, IDC_BUG_OWNER, CB_GETCURSEL, 0, 0 );
	if ( curSel == CB_ERR )
	{
		g_bug_szOwner[0] = '\0';
	}
	else
	{
		strncpy( g_bug_szOwner, g_bug_pReporter->GetDisplayName( curSel ), sizeof( g_bug_szOwner ) );
		g_bug_szOwner[sizeof( g_bug_szOwner )-1] = '\0';

		if ( V_stristr( g_bug_szOwner, "unassigned" ) )
		{
			g_bug_szOwner[0] = '\0';
		}
	}

	// severity
	curSel = SendDlgItemMessage( hWnd, IDC_BUG_SEVERITY, CB_GETCURSEL, 0, 0 );
	if ( curSel == CB_ERR )
	{
		g_bug_szSeverity[0] = '\0';
	}
	else
	{
		V_strncpy( g_bug_szSeverity, g_bug_pReporter->GetSeverity( curSel ), sizeof( g_bug_szSeverity ) );
	}

	// report type
	curSel = SendDlgItemMessage( hWnd, IDC_BUG_REPORTTYPE, CB_GETCURSEL, 0, 0 );
	if ( curSel == CB_ERR )
	{
		g_bug_szReportType[0] = '\0';
	}
	else
	{
		V_strncpy( g_bug_szReportType, g_bug_pReporter->GetReportType( curSel ), sizeof( g_bug_szReportType ) );
	}

	// priority
	curSel = SendDlgItemMessage( hWnd, IDC_BUG_PRIORITY, CB_GETCURSEL, 0, 0 );
	if ( curSel == CB_ERR )
	{
		g_bug_szPriority[0] = '\0';
	}
	else
	{
		V_strncpy( g_bug_szPriority, g_bug_pReporter->GetPriority( curSel ), sizeof( g_bug_szPriority ) );
	}

	// area
	curSel = SendDlgItemMessage( hWnd, IDC_BUG_AREA, CB_GETCURSEL, 0, 0 );
	int areaIndex = 0;
	if ( curSel == CB_ERR )
	{
		g_bug_szArea[0] = '\0';
	}
	else
	{
		V_strncpy( g_bug_szArea, g_bug_pReporter->GetArea( curSel ), sizeof( g_bug_szArea ) );
		areaIndex = curSel;
	}

	if ( !g_Games[g_bug_GameType].bUsesSystem1 )
	{
		// map number
		curSel = SendDlgItemMessage( hWnd, IDC_BUG_MAPNUMBER, CB_GETCURSEL, 0, 0 );
		if ( curSel == CB_ERR )
		{
			g_bug_szMapNumber[0] = '\0';
		}
		else
		{
			V_strncpy( g_bug_szMapNumber, g_bug_pReporter->GetLevel( areaIndex, curSel ), sizeof( g_bug_szMapNumber ) );
		}
	}

	g_bug_bCompressScreenshot = ( IsDlgButtonChecked( hWnd, IDC_BUG_COMPRESS_SCREENSHOT ) != 0 );

	BugDlg_CheckSubmit( hWnd );

	// success
	return true;
}

//-----------------------------------------------------------------------------
//	BugDlg_UpdateReporter
//
//-----------------------------------------------------------------------------
bool BugDlg_UpdateReporter( HWND hWnd )
{
	// game
	int newGameType;
	int curSel = SendDlgItemMessage( hWnd, IDC_BUG_GAME, CB_GETCURSEL, 0, 0 );
	if ( curSel == CB_ERR )
	{
		newGameType = 0;
	}
	else
	{
		newGameType = curSel;
	}

	return BugReporter_SelectReporter( g_Games[newGameType].pGameName );
}

//-----------------------------------------------------------------------------
//	BugDlg_Populate
//
//-----------------------------------------------------------------------------
void BugDlg_Populate( HWND hWnd )
{
	int i;
	int	count;
	int	curSel;

	g_bug_bActive = false;

	// title
	SendDlgItemMessage( hWnd, IDC_BUG_TITLE, EM_LIMITTEXT, sizeof( g_bug_szTitle )-1, 0 ); 
	SetDlgItemText( hWnd, IDC_BUG_TITLE, g_bug_szTitle );

	// description
	SendDlgItemMessage( hWnd, IDC_BUG_DESCRIPTION, EM_LIMITTEXT, sizeof( g_bug_szDescription )-1, 0 ); 
	SetDlgItemText( hWnd, IDC_BUG_DESCRIPTION, g_bug_szDescription );

	// submitter
	SetDlgItemText( hWnd, IDC_BUG_SUBMITTER_LABEL, g_bug_pReporter->GetUserName_Display() );

	// owner
	SendDlgItemMessage( hWnd, IDC_BUG_OWNER, CB_RESETCONTENT, 0, 0 );
	count = g_bug_pReporter->GetDisplayNameCount();
	for ( i = 0; i < count; i++ )
	{
		SendDlgItemMessage( hWnd, IDC_BUG_OWNER, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)g_bug_pReporter->GetDisplayName( i ) );
	}
	if ( count )
	{
		curSel = SendDlgItemMessage( hWnd, IDC_BUG_OWNER, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)(LPCSTR)g_bug_szOwner );
		if ( curSel != CB_ERR )
		{
			SendDlgItemMessage( hWnd, IDC_BUG_OWNER, CB_SETCURSEL, curSel, 0 );
		}
	}

	// severity
	SendDlgItemMessage( hWnd, IDC_BUG_SEVERITY, CB_RESETCONTENT, 0, 0 );
	count = g_bug_pReporter->GetSeverityCount();
	for ( i = 0; i < count; i++ )
	{
		SendDlgItemMessage( hWnd, IDC_BUG_SEVERITY, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)g_bug_pReporter->GetSeverity( i ) );
	}
	if ( count )
	{
		curSel = SendDlgItemMessage( hWnd, IDC_BUG_SEVERITY, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)(LPCSTR)g_bug_szSeverity );
		if ( curSel != CB_ERR )
		{
			SendDlgItemMessage( hWnd, IDC_BUG_SEVERITY, CB_SETCURSEL, curSel, 0 );
		}
	}

	// report type
	SendDlgItemMessage( hWnd, IDC_BUG_REPORTTYPE, CB_RESETCONTENT, 0, 0 );
	EnableWindow( GetDlgItem( hWnd, IDC_BUG_REPORTTYPE ), g_Games[g_bug_GameType].bUsesSystem1 );
	if ( g_Games[g_bug_GameType].bUsesSystem1 )
	{
		count = g_bug_pReporter->GetReportTypeCount();
		for ( i = 0; i < count; i++ )
		{
			SendDlgItemMessage( hWnd, IDC_BUG_REPORTTYPE, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)g_bug_pReporter->GetReportType( i ) );
		}
		if ( count )
		{
			curSel = SendDlgItemMessage( hWnd, IDC_BUG_REPORTTYPE, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)(LPCSTR)g_bug_szReportType );
			if ( curSel != CB_ERR )
			{
				SendDlgItemMessage( hWnd, IDC_BUG_REPORTTYPE, CB_SETCURSEL, curSel, 0 );
			}
		}
	}

	// priority
	SendDlgItemMessage( hWnd, IDC_BUG_PRIORITY, CB_RESETCONTENT, 0, 0 );
	EnableWindow( GetDlgItem( hWnd, IDC_BUG_PRIORITY ), g_Games[g_bug_GameType].bUsesSystem1 );
	if ( g_Games[g_bug_GameType].bUsesSystem1 )
	{
		count = g_bug_pReporter->GetPriorityCount();
		for ( i = 0; i < count; i++ )
		{
			SendDlgItemMessage( hWnd, IDC_BUG_PRIORITY, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)g_bug_pReporter->GetPriority( i ) );
		}
		if ( count )
		{
			curSel = SendDlgItemMessage( hWnd, IDC_BUG_PRIORITY, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)(LPCSTR)g_bug_szPriority );
			if ( curSel != CB_ERR )
			{
				SendDlgItemMessage( hWnd, IDC_BUG_PRIORITY, CB_SETCURSEL, curSel, 0 );
			}
		}
	}

	// area
	SendDlgItemMessage( hWnd, IDC_BUG_AREA, CB_RESETCONTENT, 0, 0 );
	count = g_bug_pReporter->GetAreaCount();
	int areaIndex = 0;
	for ( i = 0; i < count; i++ )
	{
		SendDlgItemMessage( hWnd, IDC_BUG_AREA, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)g_bug_pReporter->GetArea( i ) );
	}
	if ( count )
	{
		curSel = SendDlgItemMessage( hWnd, IDC_BUG_AREA, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)(LPCSTR)g_bug_szArea );
		if ( curSel != CB_ERR )
		{
			SendDlgItemMessage( hWnd, IDC_BUG_AREA, CB_SETCURSEL, curSel, 0 );
			areaIndex = curSel;
		}
	}

	// map name or number
	SendDlgItemMessage( hWnd, IDC_BUG_MAPNUMBER, CB_RESETCONTENT, 0, 0 );
	EnableWindow( GetDlgItem( hWnd, IDC_BUG_MAPNUMBER ), g_Games[g_bug_GameType].bUsesSystem1 == false );
	if ( !g_Games[g_bug_GameType].bUsesSystem1 )
	{
		// new system has map names
		count = g_bug_pReporter->GetLevelCount( areaIndex );
		for ( i = 0; i < count; i++ )
		{
			SendDlgItemMessage( hWnd, IDC_BUG_MAPNUMBER, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)g_bug_pReporter->GetLevel( areaIndex, i ) );
		}
	}
	if ( count )
	{
		curSel = SendDlgItemMessage( hWnd, IDC_BUG_MAPNUMBER, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)(LPCSTR)g_bug_szMapNumber );
		if ( curSel != CB_ERR )
		{
			SendDlgItemMessage( hWnd, IDC_BUG_MAPNUMBER, CB_SETCURSEL, curSel, 0 );
		}
	}

	// game
	SendDlgItemMessage( hWnd, IDC_BUG_GAME, CB_RESETCONTENT, 0, 0 );
	count = ARRAYSIZE( g_Games );
	for ( i = 0; i < count; i++ )
	{
		SendDlgItemMessage( hWnd, IDC_BUG_GAME, CB_ADDSTRING, 0, (LPARAM)(LPCSTR)g_Games[i].pFriendlyName );
	}
	if ( count )
	{
		curSel = SendDlgItemMessage( hWnd, IDC_BUG_GAME, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)(LPCSTR)g_Games[g_bug_GameType].pFriendlyName );
		if ( curSel != CB_ERR )
		{
			SendDlgItemMessage( hWnd, IDC_BUG_GAME, CB_SETCURSEL, curSel, 0 );
		}
	}

	CheckDlgButton( hWnd, IDC_BUG_COMPRESS_SCREENSHOT, g_bug_bCompressScreenshot ? BST_CHECKED : BST_UNCHECKED );

	SetDlgItemText( hWnd, IDC_BUG_TAKESHOT_LABEL, g_bug_szScreenshot );
	SetDlgItemText( hWnd, IDC_BUG_SAVEGAME_LABEL, g_bug_szSavegame );
	SetDlgItemText( hWnd, IDC_BUG_INCLUDEBSP_LABEL, g_bug_szBSPName );
	SetDlgItemText( hWnd, IDC_BUG_INCLUDEVMF_LABEL, "" );

	EnableWindow( GetDlgItem( hWnd, IDC_BUG_TAKESHOT ), g_connectedToApp );
	EnableWindow( GetDlgItem( hWnd, IDC_BUG_INCLUDEVMF ), false );
	EnableWindow( GetDlgItem( hWnd, IDC_BUG_UPDATE ), g_connectedToApp );

	BugDlg_GetAppData( hWnd );

	BugDlg_CheckSubmit( hWnd );

	g_bug_bActive = true;
}

//-----------------------------------------------------------------------------
//	BugDlg_TakeScreenshot
//
//-----------------------------------------------------------------------------
void BugDlg_TakeScreenshot( HWND hWnd )
{
	char	buff[1024];

	SetDlgItemText( hWnd, IDC_BUG_TAKESHOT_LABEL, "Working..." );

	BugDlg_GetDataFileBase( g_bug_pReporter->GetUserName(), true, g_bug_szScreenshot, sizeof( g_bug_szScreenshot ) );
	strcat( g_bug_szScreenshot, ".bmp" );

	// remove local version
	unlink( g_bug_szScreenshot );

	sprintf( buff, "*screenshot \"%s\"", g_bug_szScreenshot );
	if ( !ProcessCommand( buff  ) )
	{
		g_bug_szScreenshot[0] = '\0';
	}

	SetDlgItemText( hWnd, IDC_BUG_TAKESHOT_LABEL, g_bug_szScreenshot );
}

//-----------------------------------------------------------------------------
//	BugDlg_SaveGame
//
//-----------------------------------------------------------------------------
void BugDlg_SaveGame( HWND hWnd )
{
	char	buff[1024];
	char	savename[MAX_PATH];
	char	remoteFile[MAX_PATH];

	if ( !g_bug_mapInfo.savePath[0] )
	{
		return;
	}

	SetDlgItemText( hWnd, IDC_BUG_SAVEGAME_LABEL, "Working..." );

	BugDlg_GetDataFileBase( g_bug_pReporter->GetUserName(), true, g_bug_szSavegame, sizeof( g_bug_szSavegame ) );
	Sys_StripPath( g_bug_szSavegame, savename, sizeof( savename ) );
	strcat( g_bug_szSavegame, ".360.sav" );

	sprintf( remoteFile, "%s\\%s.360.sav", g_bug_mapInfo.savePath, savename );

	// delete file locally
	unlink( g_bug_szSavegame );

	// delete file remotely
	DmDeleteFile( remoteFile, false );

	// save, and wait to ensure async completes
	sprintf( buff, "save \"%s\" notmostrecent wait", savename );
	if ( !ProcessCommand( buff ) )
	{
		// failed
		g_bug_szSavegame[0] = '\0';
	}
	else
	{
		DM_FILE_ATTRIBUTES fileAttributes;
		for (int i=0; i<5; i++)
		{
			// wait for the save file to appear
			HRESULT hr = DmGetFileAttributes( remoteFile, &fileAttributes );
			if ( hr == XBDM_NOERR )
				break;

			// wait for it
			Sleep( 1000 );
		}

		HRESULT hr = DmReceiveFile( g_bug_szSavegame, remoteFile );
		if ( hr != XBDM_NOERR )
		{
			// failed
			g_bug_szSavegame[0] = '\0';
		}
	}

	SetDlgItemText( hWnd, IDC_BUG_SAVEGAME_LABEL, g_bug_szSavegame );
}

//-----------------------------------------------------------------------------
//	BugDlg_IncludeBSP
//
//-----------------------------------------------------------------------------
void BugDlg_IncludeBSP( HWND hWnd )
{
	if ( !g_bug_mapInfo.mapPath[0] )
	{
		return;
	}

	SetDlgItemText( hWnd, IDC_BUG_INCLUDEBSP_LABEL, "Working..." );

	BugDlg_GetDataFileBase( g_bug_pReporter->GetUserName(), true, g_bug_szBSPName, sizeof( g_bug_szBSPName ) );
	strcat( g_bug_szBSPName, ".360.bsp" );

	// remove local version
	unlink( g_bug_szBSPName );

	// get the file locally
	HRESULT hr = DmReceiveFile( g_bug_szBSPName, g_bug_mapInfo.mapPath );
	if ( hr != XBDM_NOERR )
	{
		// failed
		g_bug_szBSPName[0] = '\0';
	}

	SetDlgItemText( hWnd, IDC_BUG_INCLUDEBSP_LABEL, g_bug_szBSPName );
}

//-----------------------------------------------------------------------------
//	BugDlg_ResetAndPopulate
//
//-----------------------------------------------------------------------------
void BugDlg_ResetAndPopulate( HWND hWnd, bool bFullReset = true )
{
	// reset all fields
	if ( bFullReset )
	{
		g_bug_szTitle[0] = '\0';
		g_bug_szDescription[0] = '\0';
	}
	g_bug_szOwner[0] = '\0';
	g_bug_szSeverity[0] = '\0';
	g_bug_szReportType[0] = '\0';
	g_bug_szPriority[0] = '\0';
	g_bug_szMapNumber[0] = '\0';

	if ( bFullReset )
	{
		g_bug_szScreenshot[0] = '\0';
		g_bug_szSavegame[0] = '\0';
		g_bug_szBSPName[0] = '\0';
	}

	g_bug_bCompressScreenshot = true;

	memset( &g_bug_mapInfo, 0, sizeof( g_bug_mapInfo ) );

	// populate with reset fields
	BugDlg_Populate( hWnd );
}

//-----------------------------------------------------------------------------
//	BugDlg_Setup
//
//-----------------------------------------------------------------------------
void BugDlg_Setup( HWND hWnd )
{
	g_bug_hWnd = hWnd;

	// clear stale data from app
	g_bug_bFirstCommand = false;
	memset( &g_bug_mapInfo, 0, sizeof( g_bug_mapInfo ) );

	// always reset these fields
	g_bug_szTitle[0] = '\0';
	g_bug_szDescription[0] = '\0';
	g_bug_szScreenshot[0] = '\0';
	g_bug_szSavegame[0] = '\0';
	g_bug_szBSPName[0] = '\0';

	g_bug_bCompressScreenshot = true;

	memset( &g_bug_mapInfo, 0, sizeof( g_bug_mapInfo ) );

	BugDlg_Populate( hWnd );
}

//-----------------------------------------------------------------------------
//	BugDlg_UploadFile
//
//-----------------------------------------------------------------------------
bool BugDlg_UploadFile( char const *pLocalName, char const *pRemoteName, bool bDeleteLocal )
{
	FILE	*fp;
	int		len;
	void	*pLocalData;

	ConsoleWindowPrintf( BUG_COLOR, "Uploading %s to %s\n", pLocalName, pRemoteName );

	len = Sys_LoadFile( pLocalName, &pLocalData );
	if ( !pLocalData || !len )
	{
		ConsoleWindowPrintf( XBX_CLR_RED, "UploadFile: Unable to open local path '%s'\n", pLocalName );
		return false;
	}

	Sys_CreatePath( pRemoteName );

	fp = fopen( pRemoteName, "wb" );
	if ( !fp )
	{
		ConsoleWindowPrintf( XBX_CLR_RED, "UploadFile: Unable to open remote path '%s'\n", pRemoteName );
		Sys_Free( pLocalData );
		return false;
	}

	fwrite( pLocalData, len, 1, fp );

	fclose( fp );
	Sys_Free( pLocalData );

	if ( bDeleteLocal )
	{
		unlink( pLocalName );
	}

	return true;
}

//-----------------------------------------------------------------------------
//	BugDlg_UploadBugSubmission
//
//	Expects fully qualified source paths
//-----------------------------------------------------------------------------
bool BugDlg_UploadBugSubmission( int bugID, char const *pSavefile, char const *pScreenshot, char const *pBspFile, char const *pVmfFile )
{
	char	szFilename[MAX_PATH];
	char	szLocalfile[MAX_PATH];
	char	szRemotefile[MAX_PATH];
	bool	bSuccess = true;

	if ( pSavefile && pSavefile[0] )
	{
		V_snprintf( szLocalfile, sizeof( szLocalfile ), "%s", pSavefile );
		Sys_StripPath( pSavefile, szFilename, sizeof( szFilename ) );
		V_snprintf( szRemotefile, sizeof( szRemotefile ), "%s/%s", GetSubmissionURL( bugID ), szFilename );
		Sys_NormalizePath( szLocalfile, false );
		Sys_NormalizePath( szRemotefile, false );

		if ( !BugDlg_UploadFile( szLocalfile, szRemotefile, false ) )
		{
			bSuccess = false;
		}
	}

	if ( pScreenshot && pScreenshot[0] )
	{
		V_snprintf( szLocalfile, sizeof( szLocalfile ), "%s", pScreenshot );
		Sys_StripPath( pScreenshot, szFilename, sizeof( szFilename ) );
		V_snprintf( szRemotefile, sizeof( szRemotefile ), "%s/%s", GetSubmissionURL( bugID ), szFilename );
		Sys_NormalizePath( szLocalfile, false );
		Sys_NormalizePath( szRemotefile, false );
		
		if ( !BugDlg_UploadFile( szLocalfile, szRemotefile, true ) )
		{
			bSuccess = false;
		}
	}

	if ( pBspFile && pBspFile[0] )
	{
		V_snprintf( szLocalfile, sizeof( szLocalfile ), "%s", pBspFile );
		Sys_StripPath( pBspFile, szFilename, sizeof( szFilename ) );
		V_snprintf( szRemotefile, sizeof( szRemotefile ), "%s/%s", GetSubmissionURL( bugID ), szFilename );
		Sys_NormalizePath( szLocalfile, false );
		Sys_NormalizePath( szRemotefile, false );
		
		if ( !BugDlg_UploadFile( szLocalfile, szRemotefile, true ) )
		{
			bSuccess = false;
		}
	}

	return bSuccess;
}

//-----------------------------------------------------------------------------
//	BugDlg_Submit
//
//-----------------------------------------------------------------------------
bool BugDlg_Submit( HWND hWnd )
{
	char	title[1024];
	char	miscInfo[1024];
	char	basename[MAX_PATH];
	char	filename[MAX_PATH];
	char	positionName[MAX_PATH];
	char	orientationName[MAX_PATH];
	char	buildName[MAX_PATH];
	char	mapName[MAX_PATH];
	bool	bSuccess = false;

	sprintf( positionName, "%f %f %f", g_bug_mapInfo.position[0], g_bug_mapInfo.position[1], g_bug_mapInfo.position[2] );
	SetDlgItemText( g_bug_hWnd, IDC_BUG_POSITION_LABEL, positionName );

	sprintf( orientationName, "%f %f %f", g_bug_mapInfo.angle[0], g_bug_mapInfo.angle[1], g_bug_mapInfo.angle[2] );
	SetDlgItemText( g_bug_hWnd, IDC_BUG_ORIENTATION_LABEL, orientationName );

	sprintf( buildName, "%d", g_bug_mapInfo.build );
	SetDlgItemText( g_bug_hWnd, IDC_BUG_BUILD_LABEL, buildName );

	V_FileBase( g_bug_mapInfo.mapPath, mapName, sizeof( mapName ) );
	char *pExtension = V_stristr( mapName, ".bsp" );
	if ( pExtension )
	{
		*pExtension = '\0';
	}
	pExtension = V_stristr( mapName, ".360" );
	if ( pExtension )
	{
		*pExtension = '\0';
	}

	V_snprintf( miscInfo, sizeof( miscInfo ), "skill %d", g_bug_mapInfo.skill );

	// Stuff bug data files up to server
	g_bug_pReporter->StartNewBugReport();

	g_bug_pReporter->SetOwner( g_bug_pReporter->GetUserNameForDisplayName( g_bug_szOwner ) );
	g_bug_pReporter->SetSubmitter( NULL );

	if ( mapName[0] )
		V_snprintf( title, sizeof( title ), "%s: %s", mapName, g_bug_szTitle );
	else
		V_snprintf( title, sizeof( title ), "%s", g_bug_szTitle );
	g_bug_pReporter->SetTitle( title );

	g_bug_pReporter->SetDescription( g_bug_szDescription );
	g_bug_pReporter->SetLevel( mapName );
	g_bug_pReporter->SetPosition( positionName );
	g_bug_pReporter->SetOrientation( orientationName );
	g_bug_pReporter->SetBuildNumber( buildName );

	g_bug_pReporter->SetSeverity( g_bug_szSeverity );
	g_bug_pReporter->SetPriority( g_bug_szPriority );
	g_bug_pReporter->SetArea( g_bug_szArea );
	g_bug_pReporter->SetMapNumber( g_bug_szMapNumber );
	g_bug_pReporter->SetReportType( g_bug_szReportType );
	g_bug_pReporter->SetMiscInfo( miscInfo );

	g_bug_pReporter->SetDriverInfo( "" );
	g_bug_pReporter->SetExeName( "" );
	g_bug_pReporter->SetGameDirectory( "" );
	g_bug_pReporter->SetRAM( 0 );
	g_bug_pReporter->SetCPU( 0 );
	g_bug_pReporter->SetProcessor( "" );
	g_bug_pReporter->SetDXVersion( 0, 0, 0, 0 );
	g_bug_pReporter->SetOSVersion( "" );
	g_bug_pReporter->ResetIncludedFiles();
	g_bug_pReporter->SetZipAttachmentName( "" );

	if ( g_bug_szScreenshot[0] )
	{
		if ( g_bug_bCompressScreenshot )
		{
			BugDlg_CompressScreenshot();
		}

		// strip the fully qualified path into filename only
		Sys_StripPath( g_bug_szScreenshot, basename, sizeof( basename ) );
		V_snprintf( filename, sizeof( filename ), "%s/BugId/%s", GetRepositoryURL(), basename );
		Sys_NormalizePath( filename, false );
		g_bug_pReporter->SetScreenShot( filename );
	}

	if ( g_bug_szSavegame[0] )
	{
		// strip the fully qualified path into filename only
		Sys_StripPath( g_bug_szSavegame, basename, sizeof( basename ) );
		V_snprintf( filename, sizeof( filename ), "%s/BugId/%s", GetRepositoryURL(), basename );
		Sys_NormalizePath( filename, false );
		g_bug_pReporter->SetSaveGame( filename );
	}

	if ( g_bug_szBSPName[0] )
	{
		// strip the fully qualified path into filename only
		Sys_StripPath( g_bug_szBSPName, basename, sizeof( basename ) );
		V_snprintf( filename, sizeof( filename ), "%s/BugId/%s", GetRepositoryURL(), basename );
		Sys_NormalizePath( filename, false );
		g_bug_pReporter->SetBSPName( filename );
	}
	
	int bugID = -1;

	bSuccess = g_bug_pReporter->CommitBugReport( bugID );
	if ( bSuccess )
	{
		if ( !BugDlg_UploadBugSubmission( bugID, g_bug_szSavegame, g_bug_szScreenshot, g_bug_szBSPName, NULL ) )
		{
			Sys_MessageBox( BUG_ERRORTITLE, "Unable to upload files to bug repository!\n" );
			bSuccess = false;
		}
	}
	else
	{
		Sys_MessageBox( BUG_ERRORTITLE, "Unable to post bug report to database!\n" );
	}

	if ( bSuccess )
	{
		if ( g_Games[g_bug_GameType].bUsesSystem1 )
		{
			ConsoleWindowPrintf( BUG_COLOR, "Bug Reporter: PVCS submission succeeded for bug! (%d)\n", bugID );
		}
		else
		{
			ConsoleWindowPrintf( BUG_COLOR, "Bug Reporter: BugBait submission succeeded for bug!\n" );
		}
	}
	else
	{
		ConsoleWindowPrintf( XBX_CLR_RED, "Bug Reporter: Submission failed\n" );
	}

	return bSuccess;
}

//-----------------------------------------------------------------------------
//	BugDlg_Proc
//
//-----------------------------------------------------------------------------
BOOL CALLBACK BugDlg_Proc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam ) 
{ 
	switch ( message ) 
	{ 
		case WM_INITDIALOG:
			BugDlg_Setup( hwnd );
			return ( TRUE );

		case WM_COMMAND: 
            switch ( LOWORD( wParam ) ) 
            {
				case IDC_BUG_TAKESHOT:
					BugDlg_TakeScreenshot( hwnd );
					break;

				case IDC_BUG_SAVEGAME:
					BugDlg_SaveGame( hwnd );
					break;

				case IDC_BUG_INCLUDEBSP:
					BugDlg_IncludeBSP( hwnd );
					break;

				case IDC_BUG_INCLUDEVMF:
					// not implemented, no reason to
					break;

				case IDC_BUG_CLEARFORM: 
					BugDlg_ResetAndPopulate( hwnd );
					return TRUE;

				case IDC_BUG_UPDATE: 
					BugDlg_GetAppData( hwnd );
					return TRUE;

				case IDC_BUG_OWNER:
				case IDC_BUG_SEVERITY:
				case IDC_BUG_REPORTTYPE:
				case IDC_BUG_PRIORITY:
				case IDC_BUG_AREA:
				case IDC_BUG_MAPNUMBER:
					if ( g_bug_bActive && HIWORD( wParam ) == CBN_CLOSEUP  )
					{
						BugDlg_GetChanges( hwnd );
						return TRUE;
					}
					break;

				case IDC_BUG_GAME:
					if ( HIWORD( wParam ) == CBN_CLOSEUP )
					{
						if ( BugDlg_UpdateReporter( hwnd ) )
						{
							// reporter changed, clear critical parts of form
							BugDlg_ResetAndPopulate( hwnd, false );
						}
						return TRUE;
					}
					break;

				case IDC_BUG_TITLE:
				case IDC_BUG_DESCRIPTION:
					if ( g_bug_bActive && HIWORD( wParam ) == EN_CHANGE  )
					{
						BugDlg_GetChanges( hwnd );
						return TRUE;
					}
					break;

				case IDC_BUG_COMPRESS_SCREENSHOT:
					BugDlg_GetChanges( hwnd );
					return TRUE;

				case IDC_BUG_SUBMIT: 
					if ( !BugDlg_Submit( hwnd ) )
						break;
					// fall through
				case IDCANCEL:
				case IDC_CANCEL: 
					if ( g_connectedToApp && g_bug_bFirstCommand )
					{
						ProcessCommand( "unpause" );
					}
					EndDialog( hwnd, wParam );
					return ( TRUE ); 
			}
			break; 
	}
	return ( FALSE ); 
} 

//-----------------------------------------------------------------------------
//	BugDlg_SaveConfig
// 
//-----------------------------------------------------------------------------
void BugDlg_SaveConfig()
{
	Sys_SetRegistryString( "bug_owner", g_bug_szOwner );
	Sys_SetRegistryString( "bug_severity", g_bug_szSeverity );
	Sys_SetRegistryString( "bug_reporttype", g_bug_szReportType );
	Sys_SetRegistryString( "bug_priority", g_bug_szPriority );
	Sys_SetRegistryInteger( "bug_gametype", g_bug_GameType );
}

//-----------------------------------------------------------------------------
//	BugDlg_LoadConfig	
// 
//-----------------------------------------------------------------------------
void BugDlg_LoadConfig()
{
	// get our config
	Sys_GetRegistryString( "bug_owner", g_bug_szOwner, "", sizeof( g_bug_szOwner ) );
	Sys_GetRegistryString( "bug_severity", g_bug_szSeverity, "", sizeof( g_bug_szSeverity ) );
	Sys_GetRegistryString( "bug_reporttype", g_bug_szReportType, "", sizeof( g_bug_szReportType ) );
	Sys_GetRegistryString( "bug_priority", g_bug_szPriority, "", sizeof( g_bug_szPriority ) );
	Sys_GetRegistryInteger( "bug_gametype", 0, g_bug_GameType );

	// start with expected reporter
	BugReporter_SelectReporter( g_Games[g_bug_GameType].pGameName );
}

//-----------------------------------------------------------------------------
//	BugDlg_Open
//
//-----------------------------------------------------------------------------
void BugDlg_Open( void )
{
	int		result;
	
	// need access to bug databases via DLLs
	if ( !BugReporter_GetInterfaces() )
	{
		return;
	}

	BugDlg_LoadConfig();

	result = DialogBox( g_hInstance, MAKEINTRESOURCE( IDD_BUG ), g_hDlgMain, ( DLGPROC )BugDlg_Proc );	
	if ( LOWORD( result ) == IDC_BUG_SUBMIT )
	{
		BugDlg_SaveConfig();
	}

	g_bug_hWnd = NULL;
}

//-----------------------------------------------------------------------------
//	BugDlg_Init
// 
//-----------------------------------------------------------------------------
bool BugDlg_Init()
{
	return true;
}

//-----------------------------------------------------------------------------
// rc_MapInfo
//
// Sent from application with bug dialog info
//-----------------------------------------------------------------------------
int rc_MapInfo( char* commandPtr )
{
	char*			cmdToken;
	int				errCode;
	int				infoAddr;
	int				retVal;
	int				retAddr;
	char			buff[128];

	// success
	errCode = 0;

	// get address of data
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &infoAddr );

	// get retAddr
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &retAddr );

	// get the caller's info data 
	DmGetMemory( ( void* )infoAddr, sizeof( xrMapInfo_t ), &g_bug_mapInfo, NULL );

	// swap the structure
	BigFloat( &g_bug_mapInfo.position[0], &g_bug_mapInfo.position[0] );
	BigFloat( &g_bug_mapInfo.position[1], &g_bug_mapInfo.position[1] );
	BigFloat( &g_bug_mapInfo.position[2], &g_bug_mapInfo.position[2] );
	BigFloat( &g_bug_mapInfo.angle[0], &g_bug_mapInfo.angle[0] );
	BigFloat( &g_bug_mapInfo.angle[1], &g_bug_mapInfo.angle[1] );
	BigFloat( &g_bug_mapInfo.angle[2], &g_bug_mapInfo.angle[2] );
	g_bug_mapInfo.build = BigDWord( g_bug_mapInfo.build );
	g_bug_mapInfo.skill = BigDWord( g_bug_mapInfo.skill );

	Sys_NormalizePath( g_bug_mapInfo.savePath, false );
	Sys_NormalizePath( g_bug_mapInfo.mapPath, false );

	if ( g_bug_hWnd )
	{
		if ( g_bug_mapInfo.mapPath[0] )
		{
			Sys_StripPath( g_bug_mapInfo.mapPath, buff, sizeof( buff ) );
			SetDlgItemText( g_bug_hWnd, IDC_BUG_MAP_LABEL, buff );

			if ( !g_Games[g_bug_GameType].bUsesSystem1 )
			{
				char *pExtension = V_stristr( buff, ".bsp" );
				if ( pExtension )
				{
					*pExtension = '\0';
				}
				pExtension = V_stristr( buff, ".360" );
				if ( pExtension )
				{
					*pExtension = '\0';
				}
				V_strncpy( g_bug_szMapNumber, buff, sizeof( g_bug_szMapNumber ) );

				int curSel = SendDlgItemMessage( g_bug_hWnd, IDC_BUG_MAPNUMBER, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)(LPCSTR)g_bug_szMapNumber );
				if ( curSel == CB_ERR )
					curSel = 0;
				SendDlgItemMessage( g_bug_hWnd, IDC_BUG_MAPNUMBER, CB_SETCURSEL, curSel, 0 );
			}

			sprintf( buff, "%.2f %.2f %.2f", g_bug_mapInfo.position[0], g_bug_mapInfo.position[1], g_bug_mapInfo.position[2] );
			SetDlgItemText( g_bug_hWnd, IDC_BUG_POSITION_LABEL, buff );

			sprintf( buff, "%.2f %.2f %.2f", g_bug_mapInfo.angle[0], g_bug_mapInfo.angle[1], g_bug_mapInfo.angle[2] );
			SetDlgItemText( g_bug_hWnd, IDC_BUG_ORIENTATION_LABEL, buff );
		}
		else
		{
			SetDlgItemText( g_bug_hWnd, IDC_BUG_MAP_LABEL, "" );
			SetDlgItemText( g_bug_hWnd, IDC_BUG_POSITION_LABEL, "" );
			SetDlgItemText( g_bug_hWnd, IDC_BUG_ORIENTATION_LABEL, "" );
		}

		sprintf( buff, "%d", g_bug_mapInfo.build );
		SetDlgItemText( g_bug_hWnd, IDC_BUG_BUILD_LABEL, buff );

		EnableWindow( GetDlgItem( g_bug_hWnd, IDC_BUG_SAVEGAME ), g_bug_mapInfo.savePath[0] != '\0' && g_bug_mapInfo.mapPath[0] != '\0' );
		EnableWindow( GetDlgItem( g_bug_hWnd, IDC_BUG_INCLUDEBSP ), g_bug_mapInfo.mapPath[0] != '\0' );
	}
	
	// return the result
	retVal = 0;
	int xboxRetVal = BigDWord( retVal );
	DmSetMemory( ( void* )retAddr, sizeof( int ), &xboxRetVal, NULL );

	DebugCommand( "0x%8.8x = MapInfo( 0x%8.8x )\n", retVal, infoAddr );

cleanUp:	
	return errCode;
}


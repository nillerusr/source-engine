//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// SteamDebugHelperDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SteamDebugHelper.h"
#include "SteamDebugHelperDlg.h"
#include "filesystem.h"
#include "interface.h"
#include "filesystem_tools.h"
#include <io.h>
#include <direct.h>
#include "tier0/icommandline.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define CHECK( cmd ) \
	if ( !(cmd) ) \
		Error( "%s failed", #cmd );

#define CHECK_1STR( cmd, a ) \
	if ( !(cmd) ) \
		Error( "%s failed (%s)", #cmd, a );

#define CHECK_2STR( cmd, a, b ) \
	if ( !(cmd) ) \
		Error( "%s failed (%s, %s)", #cmd, a, b );


/////////////////////////////////////////////////////////////////////////////
// CSteamDebugHelperDlg dialog

CSteamDebugHelperDlg::CSteamDebugHelperDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CSteamDebugHelperDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CSteamDebugHelperDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CSteamDebugHelperDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSteamDebugHelperDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CSteamDebugHelperDlg, CDialog)
	//{{AFX_MSG_MAP(CSteamDebugHelperDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(ID_SETUP_FOR_DEBUGGING, OnSetupForDebugging)
	ON_BN_CLICKED(ID_UNSETUP_FOR_DEBUGGING, OnUnsetupForDebugging)
	ON_BN_CLICKED(ID_START_STEAM, OnStartSteam)
	ON_BN_CLICKED(ID_EDIT_CONFIG_FILE, OnEditConfigFile)
	ON_BN_CLICKED(ID_EDIT_CHOOSE_CONFIG_FILE, OnEditChooseConfigFile)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSteamDebugHelperDlg message handlers

SpewRetval_t MySpewOutput( SpewType_t spewType, char const *pMsg )
{
	if ( spewType == SPEW_ERROR )
	{
		::MessageBox( NULL, pMsg, "Error", MB_OK | MB_TASKMODAL );
		TerminateProcess( GetCurrentProcess(), 1 );
	}

	if ( spewType == SPEW_ASSERT )
		return SPEW_DEBUGGER;

	return SPEW_CONTINUE;
}


BOOL CSteamDebugHelperDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	SpewOutputFunc( MySpewOutput );

	// Load the file system.
	CommandLine()->CreateCmdLine( __argc, __argv );
	FileSystem_Init( NULL, 0, FS_INIT_COMPATIBILITY_MODE );

	if ( __argc >= 2 )
	{
		SetConfigFilename( __argv[1] );
	}
	
	// Make sure the config file parses.
	KeyValues *pTest = LoadConfigFile();
	pTest->deleteThis();

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CSteamDebugHelperDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CSteamDebugHelperDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}


KeyValues* CSteamDebugHelperDlg::LoadConfigFile()
{
	if ( m_ConfigFilename.GetLength() == 0 )
		return NULL;

	KeyValues *pKV = ::new KeyValues( "" );
	if ( !pKV->LoadFromFile( g_pFileSystem, m_ConfigFilename ) )
	{
		Error( "Error parsing %s.", m_ConfigFilename );
	}


	// Get values from the kv file.
	m_pSteamAppCfg = pKV->FindKey( "SteamApp.Cfg" );
	m_pSourceExeDir = pKV->GetString( "SourceExeDir", NULL );
	m_pSteamAppDir = pKV->GetString( "SteamAppDir", NULL );
	if ( !m_pSteamAppCfg || !m_pSourceExeDir || !m_pSteamAppDir )
		Error( "Missing one or more keys in the config file." );

	// Steam base dir is the app dir but 3 slashes back.
	Q_strncpy( m_SteamBaseDir, m_pSteamAppDir, sizeof( m_SteamBaseDir ) );
	for ( int i=0; i < 3; i++ )
	{
		char *pSlash = strrchr( m_SteamBaseDir, '/' );
		if ( !pSlash )
			pSlash = strrchr( m_SteamBaseDir, '\\' );

		if ( !pSlash )
			Error( "SteamAppDir %s invalid.", m_pSteamAppDir );
		else	
			*pSlash = 0;
	}
	return pKV;
}


void CSteamDebugHelperDlg::OnSetupForDebugging() 
{
	char src[512], dest[512];
	KeyValues *pCur;
	KeyValues *pKV = LoadConfigFile();
	if ( !pKV )
		return;

	HCURSOR hOldCursor = GetCursor();
	SetCursor( LoadCursor( AfxGetInstanceHandle(), IDC_WAIT ) );

	// steam.dll
	Q_snprintf( src, sizeof( src ), "%s\\steam.dll", m_SteamBaseDir );
	Q_snprintf( dest, sizeof( dest ), "%s\\steam.dll", m_pSteamAppDir );
	CHECK_2STR( CopyFile( src, dest, false ), src, dest );

	// steam.cfg
	Q_snprintf( src, sizeof( src ), "%s\\steam.cfg", m_SteamBaseDir );
	Q_snprintf( dest, sizeof( dest ), "%s\\steam.cfg", m_pSteamAppDir );
	CopyFile( src, dest, false );

	// now build steamapp.cfg
	Q_snprintf( dest, sizeof( dest ), "%s\\SteamApp.cfg", m_pSteamAppDir );
	FILE *fp = fopen( dest, "wt" );
	CHECK( fp );
	for ( pCur=m_pSteamAppCfg->GetFirstValue(); pCur; pCur=pCur->GetNextValue() )
	{
		fprintf( fp, "%s\n", pCur->GetString() );
	}
	fprintf( fp, "SteamInstallPath=\"%s\"", m_SteamBaseDir );
	fclose( fp );

	// Now copy each binary up there and make it read-only.
	for ( pCur=pKV->GetFirstValue(); pCur; pCur=pCur->GetNextValue() )
	{
		const char *pName = pCur->GetName();
		if ( Q_stricmp( pName, "binary" ) == 0 )
		{
			Q_snprintf( src, sizeof( src ), "%s\\%s", m_pSourceExeDir, pCur->GetString() );
			Q_snprintf( dest, sizeof( dest ), "%s\\%s", m_pSteamAppDir, pCur->GetString() );
			
			if ( _access( dest, 0 ) == 0 )
			{
				CHECK_1STR( SetFileAttributes( dest, FILE_ATTRIBUTE_NORMAL ), dest );
			}

			CHECK_2STR( CopyFile( src, dest, false ), src, dest );
			CHECK_1STR( SetFileAttributes( dest, FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_READONLY ), dest );
		}
	}

	SetCursor( hOldCursor );

	MessageBox( "Setup successfully!", MB_OK );

	//pKV->deleteThis();	// note: leak the memory here knowingly to avoid warnings from the stupid way keyvalues overload memory allocation
}

void CSteamDebugHelperDlg::OnUnsetupForDebugging() 
{
	KeyValues *pKV = LoadConfigFile();

	char dest[512];
	KeyValues *pCur;
	
	// Delete the steamapp.cfg, steam.dll, and steam.cfg files.
	Q_snprintf( dest, sizeof( dest ), "%s\\steam.dll", m_pSteamAppDir );
	CHECK_1STR( SetFileAttributes( dest, FILE_ATTRIBUTE_NORMAL ), dest );
	CHECK_1STR( DeleteFile( dest ), dest );

	// steam.cfg
	Q_snprintf( dest, sizeof( dest ), "%s\\steam.cfg", m_pSteamAppDir );
	SetFileAttributes( dest, FILE_ATTRIBUTE_NORMAL );
	DeleteFile( dest );

	// steamapp.cfg
	Q_snprintf( dest, sizeof( dest ), "%s\\steamapp.cfg", m_pSteamAppDir );
	CHECK_1STR( SetFileAttributes( dest, FILE_ATTRIBUTE_NORMAL ), dest );
	CHECK_1STR( DeleteFile( dest ), dest );

	for ( pCur=pKV->GetFirstValue(); pCur; pCur=pCur->GetNextValue() )
	{
		const char *pName = pCur->GetName();
		if ( Q_stricmp( pName, "binary" ) == 0 )
		{
			Q_snprintf( dest, sizeof( dest ), "%s\\%s", m_pSteamAppDir, pCur->GetString() );
			if ( _access( dest, 0 ) == 0 )
			{
				CHECK_1STR( SetFileAttributes( dest, FILE_ATTRIBUTE_NORMAL ), dest );
		
				CHECK_1STR( DeleteFile( dest ), dest );
			}
		}
	}

	MessageBox( "Un-setup successfully!", MB_OK );

	//pKV->deleteThis();	// note: leak the memory here knowingly to avoid warnings from the stupid way keyvalues overload memory allocation
}

void CSteamDebugHelperDlg::OnStartSteam() 
{
	STARTUPINFO si;
	memset( &si, 0, sizeof( si ) );
	si.cb = sizeof( si );

	PROCESS_INFORMATION pi;

	char dest[512];
	Q_snprintf( dest, sizeof( dest ), "%s\\steam.exe", m_SteamBaseDir );
	CreateProcess( 
		dest,	// app name
		NULL,	// command line
		NULL,	// process attr
		NULL,	// thread attr
		false,	// inherit handles
		0,		// flags
		NULL,	// environment
		m_SteamBaseDir,	// cur directory
		&si,	// startup info
		&pi		// process info
		);
}

void CSteamDebugHelperDlg::OnEditConfigFile() 
{
	char str[512];
	Q_snprintf( str, sizeof( str ), "notepad \"%s\"", m_ConfigFilename );

	STARTUPINFO si;
	memset( &si, 0, sizeof( si ) );
	si.cb = sizeof( si );

	PROCESS_INFORMATION pi;

	CreateProcess( 
		NULL,	// app name
		str,	// command line
		NULL,	// process attr
		NULL,	// thread attr
		false,	// inherit handles
		0,		// flags
		NULL,	// environment
		NULL,	// cur directory
		&si,	// startup info
		&pi		// process info
		);
}

void CSteamDebugHelperDlg::OnEditChooseConfigFile() 
{
	CFileDialog dlg( 
		true,
		"cfg",
		NULL,
		0,
		"CFG Files (*.cfg)|*.cfg||",
		this );

	if ( dlg.DoModal() == IDOK )
	{
		SetConfigFilename( dlg.GetPathName() );
	}
}

void CSteamDebugHelperDlg::SetConfigFilename( const char *pName )
{
	char absPath[MAX_PATH];
	MakeAbsolutePath( absPath, sizeof( absPath ), pName );

	if ( _access( absPath, 0 ) == 0 )
	{
		m_ConfigFilename = absPath;
	}
	else
	{
		char str[512];
		Q_snprintf( str, sizeof( str ), "%s doesn't exist.", absPath );
		AfxMessageBox( str, MB_OK );
		return;
	}

	m_ConfigFilename = absPath;

	const char *pConfigFilename = absPath;
	const char *pTest1 = strrchr( pConfigFilename, '\\' ) + 1;
	const char *pTest2 = strrchr( pConfigFilename, '/' ) + 1;
	const char *pBaseName = max( pTest1, max( pTest2, pConfigFilename ) );
	SetWindowText( CString( "SteamDebugHelper - " ) + pBaseName );

	::EnableWindow( ::GetDlgItem( m_hWnd, ID_EDIT_CONFIG_FILE ), true );
	::EnableWindow( ::GetDlgItem( m_hWnd, ID_SETUP_FOR_DEBUGGING ), true );
	::EnableWindow( ::GetDlgItem( m_hWnd, ID_UNSETUP_FOR_DEBUGGING ), true );
}

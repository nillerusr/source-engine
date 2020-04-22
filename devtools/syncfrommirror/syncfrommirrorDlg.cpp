//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// syncfrommirrorDlg.cpp : implementation file
//

#include "stdafx.h"
#include "syncfrommirror.h"
#include "syncfrommirrorDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSyncfrommirrorDlg dialog

CSyncfrommirrorDlg::CSyncfrommirrorDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CSyncfrommirrorDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CSyncfrommirrorDlg)
	m_WorkingFolder = _T("");
	m_bHL2 = FALSE;
	m_bTF2 = FALSE;
	m_bHL1Port = FALSE;
	m_bDOD = FALSE;
	m_bCSPort = FALSE;
	m_bMakeCommandPrompts = FALSE;
	//}}AFX_DATA_INIT
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CSyncfrommirrorDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSyncfrommirrorDlg)
	DDX_Control(pDX, IDC_EDIT1, m_Output);
	DDX_Text(pDX, IDC_EDIT2, m_WorkingFolder);
	DDX_Check(pDX, IDC_CHECK1, m_bHL2);
	DDX_Check(pDX, IDC_CHECK2, m_bHL1Port);
	DDX_Check(pDX, IDC_CHECK3, m_bCSPort);
	DDX_Check(pDX, IDC_CHECK4, m_bTF2);
	DDX_Check(pDX, IDC_MAKECOMMANDPROMPTS, m_bMakeCommandPrompts);
	DDX_Check(pDX, IDC_DOD, m_bDOD);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CSyncfrommirrorDlg, CDialog)
	//{{AFX_MSG_MAP(CSyncfrommirrorDlg)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON1, OnStop)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSyncfrommirrorDlg message handlers

BOOL CSyncfrommirrorDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	m_bHL2 = AfxGetApp()->GetProfileInt( "settings", "hl2", 1 );
	m_bHL1Port = AfxGetApp()->GetProfileInt( "settings", "hl1", 0 );
	m_bCSPort = AfxGetApp()->GetProfileInt( "settings", "cstrike", 0 );
	m_bDOD = AfxGetApp()->GetProfileInt( "settings", "dod", 0 );
	m_bMakeCommandPrompts = AfxGetApp()->GetProfileInt( "settings", "makecommandprompts", 0 );
	m_WorkingFolder = AfxGetApp()->GetProfileString( "settings", "working_folder" );

	// Calculate layout info for resizing.
	CRect rectDlg;
	GetClientRect( &rectDlg );

	// Edit control
	CRect rectEdit;
	GetDlgItem( IDC_EDIT1 )->GetWindowRect( &rectEdit );
	ScreenToClient( rectEdit );
	m_nEditRightMargin = rectDlg.right - rectEdit.right;
	m_nEditBottomMargin = rectDlg.bottom - rectEdit.bottom;

	// Buttons
	CRect rectButton;
	GetDlgItem( IDOK )->GetWindowRect( &rectButton );
	ScreenToClient( rectButton );
	m_nButtonOffset = rectDlg.right - rectButton.left;
	
//	LoadSettings();
	UpdateData(false);
	// TODO: Add extra initialization here
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CSyncfrommirrorDlg::OnPaint() 
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

HCURSOR CSyncfrommirrorDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CSyncfrommirrorDlg::RunSync( const char *pszType )
{
	static const char *pszCmdLine = "cmd.exe /c \\\\hl2vss.valvesoftware.com\\hl2vss\\win32\\syncfrommirror.bat %s %s";
	if ( m_bMakeCommandPrompts )
		pszCmdLine = "cmd.exe /k \\\\hl2vss.valvesoftware.com\\hl2vss\\win32\\syncfrommirror.bat %s %s";

	CString strCmdLine;

	CString &strMain = m_WorkingFolder;
	strMain.TrimLeft(" \"");
	strMain.TrimRight(" \"");

	CString workingFolder;
	workingFolder = "\"";
	workingFolder += strMain;
	workingFolder += "\"";

	strCmdLine.Format(pszCmdLine, pszType, workingFolder.operator const char *() );

	GetDlgItem(IDOK)->EnableWindow(FALSE);
	GetDlgItem(IDCANCEL)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON1)->EnableWindow();

	if ( m_bMakeCommandPrompts )
	{
		RunCommandLine( strCmdLine, "c:\\" );
	}
	else
	{
		m_Redirector.Run(  strCmdLine, &m_Output, "c:\\" );
	}

	GetDlgItem(IDOK)->EnableWindow();
	GetDlgItem(IDCANCEL)->EnableWindow();
	GetDlgItem(IDC_BUTTON1)->EnableWindow(FALSE);
}

void CSyncfrommirrorDlg::RunCommandLine( const char *pCmdLine, const char *pWorkingDir )
{
	STARTUPINFO startupInfo;
	memset( &startupInfo, 0, sizeof( startupInfo ) );
	startupInfo.cb = sizeof( startupInfo );

	PROCESS_INFORMATION pi;
	memset( &pi, 0, sizeof( pi ) );
	
	CreateProcess( 
		NULL,
		(char*)pCmdLine,
		NULL,
		NULL,
		FALSE,
		CREATE_NEW_CONSOLE,
		NULL,
		pWorkingDir,
		&startupInfo,
		&pi );
}

void CSyncfrommirrorDlg::OnOK() 
{
	UpdateData();

	AfxGetApp()->WriteProfileInt( "settings", "hl2", m_bHL2 );
	AfxGetApp()->WriteProfileInt( "settings", "hl1", m_bHL1Port );
	AfxGetApp()->WriteProfileInt( "settings", "dod", m_bDOD );
	AfxGetApp()->WriteProfileInt( "settings", "cstrike", m_bCSPort );
	AfxGetApp()->WriteProfileInt( "settings", "makecommandprompts", m_bMakeCommandPrompts );
	AfxGetApp()->WriteProfileString( "settings", "working_folder", m_WorkingFolder);

	if ( m_WorkingFolder.GetLength() == 0 )
	{
		MessageBox( "You must set your working folder (e.g. u:\\dev)", "Error", MB_ICONWARNING );
		return;
	}

	m_Output.SetSel( 0, -1 );
	m_Output.Clear();

	if  (m_bHL2 && m_bHL1Port && m_bCSPort && m_bDOD )
	{
		RunSync( "all" );
	}
	else
	{
		if ( m_bHL2 )
			RunSync( "hl2" );
		if ( m_bHL1Port )
			RunSync( "hl1" );
		if ( m_bCSPort )
			RunSync( "cstrike" );
		if ( m_bDOD )
			RunSync( "dod" );
	}

	// If it spawned command prompts, then exit.
	if ( m_bMakeCommandPrompts )
		EndDialog( 0 );

	FlashWindow( TRUE );
	
	//CDialog::OnOK();
}

void CSyncfrommirrorDlg::OnStop() 
{
	m_Redirector.Stop();
	
}

void CSyncfrommirrorDlg::OnSize( UINT nType, int cx, int cy )
{
	if ( GetDlgItem( IDC_EDIT1 ) )
	{
		CRect rectDlg;
		GetClientRect( &rectDlg );

		CRect rectEdit;
		GetDlgItem( IDC_EDIT1 )->GetWindowRect( &rectEdit );
		ScreenToClient( rectEdit );
		GetDlgItem( IDC_EDIT1 )->MoveWindow( rectEdit.left, rectEdit.top, ( rectDlg.right - m_nEditRightMargin ) - rectEdit.left, ( rectDlg.bottom - m_nEditBottomMargin ) - rectEdit.top, TRUE );

		CRect rectButton;
		GetDlgItem( IDOK )->GetWindowRect( &rectButton );
		ScreenToClient( rectButton );
		GetDlgItem( IDOK )->MoveWindow( rectDlg.right - m_nButtonOffset, rectButton.top, rectButton.Width(  ), rectButton.Height(  ), TRUE );

		GetDlgItem( IDCANCEL )->GetWindowRect( &rectButton );
		ScreenToClient( rectButton );
		GetDlgItem( IDCANCEL )->MoveWindow( rectDlg.right - m_nButtonOffset, rectButton.top, rectButton.Width(  ), rectButton.Height(  ), TRUE );

		GetDlgItem( IDC_BUTTON1 )->GetWindowRect( &rectButton );
		ScreenToClient( rectButton );
		GetDlgItem( IDC_BUTTON1 )->MoveWindow( rectDlg.right - m_nButtonOffset, rectButton.top, rectButton.Width(  ), rectButton.Height(  ), TRUE );
	}

	CDialog::OnSize( nType, cx, cy );
}

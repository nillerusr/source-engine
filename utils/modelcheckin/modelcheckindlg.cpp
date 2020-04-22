//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// ModelCheckInDlg.cpp : implementation file
//

#include "stdafx.h"
#include "ModelCheckIn.h"
#include "ModelCheckInDlg.h"
#include <direct.h>
#include <sys/stat.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CModelCheckInDlg dialog

CModelCheckInDlg::CModelCheckInDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CModelCheckInDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CModelCheckInDlg)
	m_HL2GameDirectory = _T("");
	m_TF2GameDirectory = _T("");
	m_UserName = _T("");
	m_HL2Radio = -1;
	m_TF2Radio = -1;
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CModelCheckInDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CModelCheckInDlg)
	DDX_Text(pDX, IDC_EDIT_HL2_GAME_DIRECTORY, m_HL2GameDirectory);
	DDX_Text(pDX, IDC_EDIT_TF2_GAME_DIRECTORY, m_TF2GameDirectory);
	DDX_Text(pDX, IDC_EDIT_USERNAME, m_UserName);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CModelCheckInDlg, CDialog)
	//{{AFX_MSG_MAP(CModelCheckInDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP

	ON_COMMAND(ID_FILE_CHECK_OUT,  OnFileCheckOut)
	ON_COMMAND(ID_FILE_CHECK_IN,   OnFileCheckIn)
	ON_COMMAND(ID_FILE_EXIT,   OnFileExit)

END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CModelCheckInDlg message handlers

BOOL CModelCheckInDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Restore state
	RestoreStateFromRegistry();
	UpdateData( FALSE );

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CModelCheckInDlg::OnPaint() 
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
HCURSOR CModelCheckInDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}


//-----------------------------------------------------------------------------
// Resets our last used directory
//-----------------------------------------------------------------------------

void CModelCheckInDlg::ResetDirectoryEntry( CString &fullPath, char *pRegEntry )
{
	// reset the last directory...
	CString temp = fullPath;
	int i = temp.ReverseFind( '\\' );
	int j = temp.ReverseFind( '/' );
	if (i < j) i = j;
	temp.SetAt(i, 0);

	AfxGetApp()->WriteProfileString( MDL_CHECKOUT_REG_CLASS, pRegEntry, temp );
}


//-----------------------------------------------------------------------------
// Stores state
//-----------------------------------------------------------------------------

void CModelCheckInDlg::StoreStateIntoRegistry( )
{
	AfxGetApp()->WriteProfileString( MDL_CHECKOUT_REG_CLASS, 
		MDL_CHECKOUT_REG_USER, m_UserName );
	AfxGetApp()->WriteProfileString( MDL_CHECKOUT_REG_CLASS, 
		MDL_CHECKOUT_REG_HL2_PATH, m_HL2GameDirectory );
	AfxGetApp()->WriteProfileString( MDL_CHECKOUT_REG_CLASS, 
		MDL_CHECKOUT_REG_TF2_PATH, m_TF2GameDirectory );
}


//-----------------------------------------------------------------------------
// Restores state
//-----------------------------------------------------------------------------

void CModelCheckInDlg::RestoreStateFromRegistry( )
{
	m_UserName = AfxGetApp()->GetProfileString( MDL_CHECKOUT_REG_CLASS, 
		MDL_CHECKOUT_REG_USER, "" );
	m_HL2GameDirectory = AfxGetApp()->GetProfileString( MDL_CHECKOUT_REG_CLASS, 
		MDL_CHECKOUT_REG_HL2_PATH, "u:/hl2/hl2" );
	m_TF2GameDirectory = AfxGetApp()->GetProfileString( MDL_CHECKOUT_REG_CLASS, 
		MDL_CHECKOUT_REG_TF2_PATH, "u:/hl2/tf2" );
}


//-----------------------------------------------------------------------------
// Extensions of all files related to the model file
//-----------------------------------------------------------------------------

static char* s_ppExtensions[] = 
{
	".dx7_2bone.vtx",
	".dx80.vtx",
	".phy",
	".mdl",
	""
};


static char* s_ppProjectDir[] = 
{
	"$/HL2/release/dev/hl2/",
	"$/TF2/release/dev/tf2/"
};

//-----------------------------------------------------------------------------
// Checks it out/ checks it in baby
//-----------------------------------------------------------------------------

void CModelCheckInDlg::PerformCheckoutCommand( ProjectType_t project, 
	char const* pRelativeDir, char const* pDestPath, char const* pFileName )
{
	char error[1024];
	char buf[1024];
	int len = 0;

	int currExtension;
	for ( currExtension = 0; s_ppExtensions[currExtension][0]; ++currExtension )
	{
		// Check for the existence of the file in source safe...
		sprintf( buf, "ss filetype %s%s/%s%s -O- -y%s\n",
			s_ppProjectDir[project], pRelativeDir, pFileName, s_ppExtensions[currExtension],
			(char const*)m_UserName );
		int retVal = system( buf );
		if (retVal > 0)
			continue;

		// It's there, try to check it out
		sprintf( buf, "ss checkout %s%s/%s%s -GL%s -GWA -O- -y%s\n",
			s_ppProjectDir[project], pRelativeDir, pFileName, s_ppExtensions[currExtension],
			pDestPath, (char const*)m_UserName );
		retVal = system( buf );
		if (retVal > 0)
		{
			len += sprintf( &error[len], "*** SourceSafe error attempting to check out \"%s%s\"\n", 
				pFileName, s_ppExtensions[currExtension] );
		}
	}

	if (len > 0)
	{
		MessageBox( error, "Error!" );					
	}
}

//-----------------------------------------------------------------------------
// Checks it out/ checks it in baby
//-----------------------------------------------------------------------------

void CModelCheckInDlg::PerformCheckinCommand( ProjectType_t project, 
	char const* pRelativeDir, char const* pDestPath, char const* pFileName )
{
	char buf[1024];
	char error[1024];
	int len = 0;

	int currExtension;
	for ( currExtension = 0; s_ppExtensions[currExtension][0]; ++currExtension )
	{
		// Check for the existence of the file on disk. If it's not there, don't bother
		sprintf( buf, "%s/%s%s", pDestPath, pFileName, s_ppExtensions[currExtension] );
		struct _stat statbuf;
		int result = _stat( buf, &statbuf );
		if (result != 0)
			continue;

		// Check for the existence of the file in source safe...
		sprintf( buf, "ss filetype %s%s/%s%s -O- -y%s\n",
			s_ppProjectDir[project], pRelativeDir, pFileName, s_ppExtensions[currExtension],
			(char const*)m_UserName );
		int retVal = system( buf );
		if (retVal > 0)
		{
			// Try to add the file to source safe...
			sprintf( buf, "ss add %s%s/%s%s -GL%s -O- -y%s\n",
				s_ppProjectDir[project], pRelativeDir, pFileName, s_ppExtensions[currExtension],
				pDestPath, (char const*)m_UserName );
			int retVal = system( buf );
			if (retVal > 0)
			{
				len += sprintf( &error[len], "SourceSafe error attempting to add \"%s%s\"\n", 
					pFileName, s_ppExtensions[currExtension] );
			}
		}
		else
		{
			// It's there, just check it in
			sprintf( buf, "ss checkin %s%s/%s%s -GL%s -O- -y%s\n",
				s_ppProjectDir[project], pRelativeDir, pFileName, s_ppExtensions[currExtension],
				pDestPath, (char const*)m_UserName );
			retVal = system( buf );
			if (retVal > 0)
			{
				len += sprintf( &error[len], "SourceSafe error attempting to check in \"%s%s\"\n", 
					pFileName, s_ppExtensions[currExtension] );
			}
		}
	}

	if (len > 0)
	{
		MessageBox( error, "Error!" );					
	}
}

//-----------------------------------------------------------------------------
// Fixes up a filename
//-----------------------------------------------------------------------------

static int FixupFileName( char const* pInFile, char* pOutBuf )
{
	if (!pInFile)
	{
		*pOutBuf = '\0';
		return 0;
	}

	int len = strlen(pInFile); 
	for (int i = 0; i <= len; ++i)
	{
		pOutBuf[i] = tolower( pInFile[i] );
		if (pOutBuf[i] == '\\')
			pOutBuf[i] = '/';
	}

	// Make sure no trailing '/'
	if (pOutBuf[len - 1] == '/')
		pOutBuf[--len] = '\0';

	return len;
}


//-----------------------------------------------------------------------------
// Gets the relative file name
//-----------------------------------------------------------------------------

CModelCheckInDlg::ProjectType_t CModelCheckInDlg::ComputeRelativeFileName( char const* pInFile, char* pRelativeFile )
{
	// Depending on which project is selected, strip out the game directory
	char tempIn[MAX_PATH];
	char tempPath[MAX_PATH];

	FixupFileName( pInFile, tempIn );

	int len;
	if (!m_HL2GameDirectory.IsEmpty())
	{
		len = FixupFileName( m_HL2GameDirectory, tempPath );
		if (!strncmp( tempIn, m_HL2GameDirectory, len ))
		{
			strcpy( pRelativeFile, &tempIn[len+1] );
			return PROJECT_HL2;
		}
	}

	if (!m_TF2GameDirectory.IsEmpty())
	{
		len = FixupFileName( m_TF2GameDirectory, tempPath );
		if (!strncmp( tempIn, m_TF2GameDirectory, len ))
		{
			strcpy( pRelativeFile, &tempIn[len+1] );
			return PROJECT_TF2;
		}
	}

	return PROJECT_ERROR;
}


//-----------------------------------------------------------------------------
// Have we typed in anything for our user name?
//-----------------------------------------------------------------------------

bool CModelCheckInDlg::CheckInfo() 
{
	if (m_UserName.IsEmpty())
	{
		MessageBox( "Please enter your user name.\n", "Error!" );
		return false;
	}

	if (m_HL2GameDirectory.IsEmpty() && m_TF2GameDirectory.IsEmpty())
	{
		MessageBox( "Please enter the game directories for HL2 and/or TF2.\n", "Error!" );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Resets our last used directory
//-----------------------------------------------------------------------------

CModelCheckInDlg::ProjectType_t CModelCheckInDlg::GetFileNames( char const* pTitle, 
					char*& pRelativePath, char*& pFileName, char*& pDestPath ) 
{
	UpdateData( TRUE );

	StoreStateIntoRegistry();

	if (!CheckInfo())
		return PROJECT_ERROR;
	
	CFileDialog dlg( TRUE, ".mdl", "*.mdl", OFN_HIDEREADONLY, 
		"Model Files (*.mdl)|*.mdl||" );
	
	// Set up initial paths
	char pCwd[MAX_PATH];
	_getcwd( pCwd, MAX_PATH );
															   
	// Setup the title and initial directory
	CString temp = AfxGetApp()->GetProfileString( MDL_CHECKOUT_REG_CLASS, 
		MDL_CHECKOUT_REG_LAST_PATH, pCwd );
	dlg.m_ofn.lpstrInitialDir = temp;
	dlg.m_ofn.lpstrTitle = pTitle;
	
	// Grab the data from the dialog...
	if (dlg.DoModal() != IDOK) 
		return PROJECT_ERROR;
	
	// Get the relative file name
	static char relativeFile[MAX_PATH];
	ProjectType_t projectType = ComputeRelativeFileName( dlg.GetPathName(), relativeFile );
	if (projectType == PROJECT_ERROR)
	{
		char buf[MAX_PATH];
		sprintf( buf, "%s\nwas not found under either game directory!\n", dlg.GetPathName() );
		MessageBox( buf, "Error!" );
		return PROJECT_ERROR;
	}
	
	// reset the last directory...
	ResetDirectoryEntry( dlg.GetPathName(), MDL_CHECKOUT_REG_LAST_PATH );
	
	// Split into relative file + path...
	char* pSlash = strrchr( relativeFile, '/' );
	if (pSlash)
	{
		*pSlash = 0;
		pRelativePath = relativeFile;
		pFileName = pSlash + 1;
	}
	else
	{
		pFileName = relativeFile;
		pRelativePath = "";
	}

	// Remove extension
	char* pExt = strchr( pFileName, '.' );
	if (pExt)
		*pExt = 0;

	static char destPath[MAX_PATH];
	FixupFileName( dlg.GetPathName(), destPath );
	pSlash = strrchr( destPath, '/' );
	if (pSlash)
		*pSlash = 0;
	else
		destPath[0] = 0;
	pDestPath = destPath;

	return projectType;
}



//-----------------------------------------------------------------------------
// Does checkout and check-in
//-----------------------------------------------------------------------------

void CModelCheckInDlg::OnFileCheckOut() 
{
	char* pRelativePath;
	char* pFileName;
	char* pDestPath;
	ProjectType_t projectType;
	projectType = GetFileNames( "Select MDL file to check out...", pRelativePath,
								pFileName, pDestPath );

	if (projectType != PROJECT_ERROR)
	{
		PerformCheckoutCommand( projectType, pRelativePath, pDestPath, pFileName ); 
	}
}


void CModelCheckInDlg::OnFileCheckIn()
{
	char* pRelativePath;
	char* pFileName;
	char* pDestPath;
	ProjectType_t projectType;
	projectType = GetFileNames( "Select MDL file to check in...", pRelativePath,
								pFileName, pDestPath );

	if (projectType != PROJECT_ERROR)
	{
		PerformCheckinCommand( projectType, pRelativePath, pDestPath, pFileName ); 
	}
}

void CModelCheckInDlg::OnFileExit()
{
	PostQuitMessage(0);
}

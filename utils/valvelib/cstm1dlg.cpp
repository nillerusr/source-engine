//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// cstm1dlg.cpp : implementation file
//

#include "stdafx.h"
#include "valvelib.h"
#include "cstm1dlg.h"
#include "valvelibaw.h"

#ifdef _PSEUDO_DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CCustom1Dlg dialog


CCustom1Dlg::CCustom1Dlg()
	: CAppWizStepDlg(CCustom1Dlg::IDD)
{
	//{{AFX_DATA_INIT(CCustom1Dlg)
	m_RootPath = _T("");
	m_TargetPath = _T("");
	m_ProjectType = -1;
	m_ToolProject = FALSE;
	m_ImplibPath = _T("");
	m_PublicProject = FALSE;
	m_ConsoleApp = FALSE;
	m_PublishImportLib = FALSE;
	m_SrcPath = _T("");
	//}}AFX_DATA_INIT

	m_ProjectType = 0;
	m_PublicProject = TRUE;
	m_SrcPath = "src";
}


void CCustom1Dlg::DoDataExchange(CDataExchange* pDX)
{
	if (pDX->m_bSaveAndValidate == 0)
	{
		// Refresh the paths based on project information...
		if (m_RootPath.GetLength() == 0)
		{
			if (!Valvelibaw.m_Dictionary.Lookup("FULL_DIR_PATH", m_RootPath))
				m_RootPath = "u:\\hl2\\";

			m_RootPath.MakeLower();

			m_ToolProject = (m_RootPath.Find( "util" ) >= 0);

			int idx = m_RootPath.Find( m_SrcPath );	// look for src tree
			if (idx >= 0)
			{
				m_RootPath = m_RootPath.Left( idx );
			}

			m_TargetPath = m_RootPath;
			m_TargetPath += m_SrcPath;
			m_TargetPath += "\\lib\\public\\";

			m_ImplibPath = "unused";
			EnableCheckboxes();
		}
	}

	CAppWizStepDlg::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CCustom1Dlg)
	DDX_Text(pDX, IDC_EDIT_ROOT_PATH, m_RootPath);
	DDX_Text(pDX, IDC_EDIT_TARGET_PATH, m_TargetPath);
	DDX_CBIndex(pDX, IDC_SELECT_PROJECT_TYPE, m_ProjectType);
	DDX_Check(pDX, IDC_CHECK_TOOL, m_ToolProject);
	DDX_Text(pDX, IDC_EDIT_IMPLIB_PATH, m_ImplibPath);
	DDX_Check(pDX, IDC_CHECK_PUBLIC, m_PublicProject);
	DDX_Check(pDX, IDC_CHECK_CONSOLE_APP, m_ConsoleApp);
	DDX_Check(pDX, IDC_CHECK_PUBLISH_IMPORT, m_PublishImportLib);
	DDX_Text(pDX, IDC_EDIT_SRC_PATH, m_SrcPath);
	//}}AFX_DATA_MAP
}

static void FixupPath( CString& path )
{
	int idx;
	idx = path.Find("/");
	while (idx >= 0)
	{
		path.SetAt(idx, '\\' );
		idx = path.Find("/");
	}

	bool hasTerminatingSlash = ( path.Right(1).Find("\\") >= 0 );
	if (!hasTerminatingSlash)
		path += '\\';

	path.MakeLower();
}

static int CountSlashes( CString& path )
{
	int count = 0;

	int idx = path.Find("\\", 0);
	while (idx >= 0)
	{
		++count;
		idx = path.Find("\\", idx+1 );
	}

	return count;
}

bool CCustom1Dlg::ComputeRelativePath( )
{
	if ( m_RootPath.GetAt(1) != ':' )
	{
		MessageBox( "Error! The root path must specify a drive!", "Bogus Root Path!", MB_ICONERROR | MB_OK );
		return false;
	}

	if ( m_TargetPath.GetAt(1) != ':' )
	{
		MessageBox( "Error! The target path must specify a drive!", "Bogus Target Path!", MB_ICONERROR | MB_OK );
		return false;
	}

	CString sourcePath;
	if (!Valvelibaw.m_Dictionary.Lookup("FULL_DIR_PATH", sourcePath ))
	{
		MessageBox( "I can't seem to find the source path!??!", "Umm... Get Brian", MB_ICONERROR | MB_OK );
		return false;
	}

	FixupPath( m_RootPath );
	FixupPath( m_TargetPath );
	FixupPath( sourcePath );

	CString srcRootPath = m_RootPath;
	srcRootPath += m_SrcPath;
	srcRootPath += "\\";
	FixupPath( srcRootPath );

	if (sourcePath.Find( srcRootPath ) != 0)
	{
		MessageBox( "Error! The source path must lie under the root source path!", "Bogus Root Path!", MB_ICONERROR | MB_OK );
		return false;
	}

	if (m_TargetPath.Find( m_RootPath ) != 0)
	{
		MessageBox( "Error! The target path must lie under the root path!", "Bogus Target Path!", MB_ICONERROR | MB_OK );
		return false;
	}

	int rootLen = m_RootPath.GetLength();
	int rootSrcLen = srcRootPath.GetLength();
	int sourceLen = sourcePath.GetLength();
	int targetLen = m_TargetPath.GetLength();
	CString relativePath = m_TargetPath.Right( targetLen - rootLen );

	// Now that we've got the relative source path, 
	// find out how many slashes are in it;
	// that'll tell us how many paths to back up....
	int i;
	CString relativeSourcePath = sourcePath.Right( sourceLen - rootLen );
	int numSlashes = CountSlashes(relativeSourcePath);
	CString targetRelativePath;
	for ( i = 0; i < numSlashes; ++i )
	{
		targetRelativePath += "..\\";
	}

	// Now that we've got the relative source path, 
	// find out how many slashes are in it;
	// that'll tell us how many paths to back up....
	CString rootSrcToProj = sourcePath.Right( sourceLen - rootSrcLen );
	numSlashes = CountSlashes(rootSrcToProj);
	CString projToRootSrc;
	for ( i = 0; i < numSlashes; ++i )
	{
		projToRootSrc += "..\\";
	}

	Valvelibaw.m_Dictionary["VALVE_ROOT_RELATIVE_PATH"] = targetRelativePath;
	Valvelibaw.m_Dictionary["VALVE_SRC_RELATIVE_PATH"] = projToRootSrc;
	targetRelativePath += relativePath;
	Valvelibaw.m_Dictionary["VALVE_ROOT_PATH"] = m_RootPath;
	Valvelibaw.m_Dictionary["VALVE_ROOT_SRC_PATH"] = srcRootPath;
	Valvelibaw.m_Dictionary["VALVE_TARGET_PATH"] = m_TargetPath;
	Valvelibaw.m_Dictionary["VALVE_RELATIVE_PATH"] = targetRelativePath;

	if (m_ToolProject)
		Valvelibaw.m_Dictionary["VALVE_TOOL"] = "1";
	if (m_PublicProject && (m_ProjectType != 2))
		Valvelibaw.m_Dictionary["VALVE_PUBLIC_PROJECT"] = "1";
	if (m_PublishImportLib && (m_ProjectType == 1))
		Valvelibaw.m_Dictionary["VALVE_PUBLISH_IMPORT_LIB"] = "1";

	// Import libraries
	if (m_ProjectType == 1)
	{
		if ( m_ImplibPath.GetAt(1) != ':' )
		{
			MessageBox( "Error! The import library path must specify a drive!", "Bogus Import Library Path!", MB_ICONERROR | MB_OK );
			return false;
		}

		if (m_ImplibPath.Find( srcRootPath ) != 0)
		{
			MessageBox( "Error! The import library path must lie under the root src path!", "Bogus Target Path!", MB_ICONERROR | MB_OK );
			return false;
		}

		int implibLen = m_ImplibPath.GetLength();
		relativePath = m_ImplibPath.Right( implibLen - rootSrcLen );
		int numSlashes = CountSlashes(rootSrcToProj);
		CString implibRelativePath;
		for (int i = 0; i < numSlashes; ++i )
		{
			implibRelativePath += "..\\";
		}
		implibRelativePath += relativePath;

		Valvelibaw.m_Dictionary["VALVE_IMPLIB_PATH"] = m_ImplibPath;
		Valvelibaw.m_Dictionary["VALVE_IMPLIB_RELATIVE_PATH"] = implibRelativePath;
	}

	return true;
}

// This is called whenever the user presses Next, Back, or Finish with this step
//  present.  Do all validation & data exchange from the dialog in this function.
BOOL CCustom1Dlg::OnDismiss()
{
	if (!UpdateData(TRUE))
		return FALSE;

	if (!ComputeRelativePath())
		return FALSE;

	switch( m_ProjectType )
	{
	case 0:
		Valvelibaw.m_Dictionary["VALVE_TARGET_TYPE"] = "lib"; 
		Valvelibaw.m_Dictionary["PROJTYPE_LIB"]	= "1";
		break;

	case 1:
		Valvelibaw.m_Dictionary["VALVE_TARGET_TYPE"] = "dll"; 
		Valvelibaw.m_Dictionary["PROJTYPE_DLL"]	= "1";
		break;

	case 2:
		Valvelibaw.m_Dictionary["VALVE_TARGET_TYPE"] = "exe"; 

		if (m_ConsoleApp)
		{
			Valvelibaw.m_Dictionary["PROJTYPE_CON"]	= "1";
		}
		break;
	}
   
	return TRUE;	// return FALSE if the dialog shouldn't be dismissed
}


BEGIN_MESSAGE_MAP(CCustom1Dlg, CAppWizStepDlg)
	//{{AFX_MSG_MAP(CCustom1Dlg)
	ON_CBN_SELCHANGE(IDC_SELECT_PROJECT_TYPE, OnSelchangeSelectProjectType)
	ON_EN_CHANGE(IDC_EDIT_ROOT_PATH, OnChangeEditRootPath)
	ON_BN_CLICKED(IDC_CHECK_PUBLIC, OnCheckPublic)
	ON_BN_CLICKED(IDC_CHECK_TOOL, OnCheckTool)
	ON_BN_CLICKED(IDC_CHECK_PUBLISH_IMPORT, OnCheckPublishImport)
	ON_EN_CHANGE(IDC_EDIT_SRC_PATH, OnChangeEditSrcPath)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CCustom1Dlg message handlers

void CCustom1Dlg::RecomputeTargetPath() 
{
	bool hasTerminatingSlash = ( m_RootPath.Right(1).FindOneOf("\\/") >= 0 );

	m_TargetPath = m_RootPath;
	if (!hasTerminatingSlash)
		m_TargetPath += '\\';
	m_ImplibPath = m_TargetPath;

	switch( m_ProjectType )
	{
	case 0:
		// static library
		m_TargetPath += m_SrcPath;
		m_TargetPath += m_PublicProject ? "\\lib\\public\\" : "\\lib\\common\\";
		m_ImplibPath = "unused";
		break;

	case 1:
		m_TargetPath += "bin\\";
		m_ImplibPath += m_SrcPath;
		m_ImplibPath += m_PublicProject ? "\\lib\\public\\" : "\\lib\\common\\";
		break;

	case 2:
		m_TargetPath += "bin\\";
		m_ImplibPath = "unused";
		break;
	}

	UpdateData(FALSE);
}

void CCustom1Dlg::EnableCheckboxes()
{
	CWnd* pConsoleApp = GetDlgItem(IDC_CHECK_CONSOLE_APP);
	CWnd* pPublishImport = GetDlgItem(IDC_CHECK_PUBLISH_IMPORT);
	CWnd* pImportLib = GetDlgItem(IDC_EDIT_IMPLIB_PATH);
	switch (m_ProjectType)
	{
	case 0:
		pConsoleApp->EnableWindow( false );
		pPublishImport->EnableWindow( false );
		pImportLib->EnableWindow( false );
		break;

	case 1:
		pConsoleApp->EnableWindow( false );
		pPublishImport->EnableWindow( true );
		pImportLib->EnableWindow( m_PublishImportLib );
		break;

	case 2:
		pConsoleApp->EnableWindow( true );
		pPublishImport->EnableWindow( false );
		pImportLib->EnableWindow( false );
		break;
	}
}

void CCustom1Dlg::OnSelchangeSelectProjectType() 
{
	if (!UpdateData(TRUE))
		return;

	RecomputeTargetPath();
	EnableCheckboxes();
}

void CCustom1Dlg::OnChangeEditRootPath() 
{	
	if (!UpdateData(TRUE))
		return;

	RecomputeTargetPath();
}

void CCustom1Dlg::OnCheckPublic() 
{
	if (!UpdateData(TRUE))
		return;

	RecomputeTargetPath();
}

void CCustom1Dlg::OnCheckTool() 
{
	if (!UpdateData(TRUE))
		return;

	RecomputeTargetPath();
}

void CCustom1Dlg::OnCheckPublishImport() 
{
	if (!UpdateData(TRUE))
		return;
	
	EnableCheckboxes();
}

void CCustom1Dlg::OnChangeEditSrcPath() 
{
	if (!UpdateData(TRUE))
		return;

	RecomputeTargetPath();
}

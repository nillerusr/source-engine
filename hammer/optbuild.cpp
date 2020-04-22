//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "hammer.h"
#include "GameConfig.h"
#include "OPTBuild.h"
#include "Options.h"
#include "shlobj.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


void UpdateConfigList(CComboBox &combo);
void SelectActiveConfig(CComboBox &combo);


// dvs: this is duplicated in RunMapExpertDlg.cpp!!
enum
{
	id_InsertParmMapFileNoExt = 0x100,
	id_InsertParmMapFile,
	id_InsertParmMapPath,
	id_InsertParmBspDir,
	id_InsertParmExeDir,
	id_InsertParmGameDir,

	id_InsertParmEnd
};


void EditorUtil_ConvertPath(CString &str, bool bSave);
void EditorUtil_TransferPath(CDialog *pDlg, int nIDC, char *szDest, bool bSave);


COPTBuild::COPTBuild()
	: CPropertyPage(COPTBuild::IDD)
{
	//{{AFX_DATA_INIT(COPTBuild)
	//}}AFX_DATA_INIT

	m_pConfig = NULL;
}


void COPTBuild::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COPTBuild)
	DDX_Control(pDX, IDC_BSPDIR, m_cBSPDir);
	DDX_Control(pDX, IDC_VIS, m_cVIS);
	DDX_Control(pDX, IDC_LIGHT, m_cLIGHT);
	DDX_Control(pDX, IDC_GAME, m_cGame);
	DDX_Control(pDX, IDC_BSP, m_cBSP);
	DDX_Control(pDX, IDC_CONFIGS, m_cConfigs);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COPTBuild, CPropertyPage)
	//{{AFX_MSG_MAP(COPTBuild)
	ON_BN_CLICKED(IDC_BROWSE_BSP, OnBrowseBsp)
	ON_BN_CLICKED(IDC_BROWSE_GAME, OnBrowseGame)
	ON_BN_CLICKED(IDC_BROWSE_LIGHT, OnBrowseLight)
	ON_BN_CLICKED(IDC_BROWSE_VIS, OnBrowseVis)
	ON_CBN_SELCHANGE(IDC_CONFIGS, OnSelchangeConfigs)
	ON_BN_CLICKED(IDC_PARMS_BSP, OnParmsBsp)
	ON_BN_CLICKED(IDC_PARMS_GAME, OnParmsGame)
	ON_BN_CLICKED(IDC_PARMS_LIGHT, OnParmsLight)
	ON_BN_CLICKED(IDC_PARMS_VIS, OnParmsVis)
	ON_BN_CLICKED(IDC_BROWSE_BSPDIR, OnBrowseBspdir)
	ON_COMMAND_EX_RANGE(id_InsertParmMapFileNoExt, id_InsertParmEnd, HandleInsertParm)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COPTBuild::DoBrowse(CWnd *pWnd)
{
	// Convert $Steam tokens to the real paths.
	CString str;
	pWnd->GetWindowText(str);
	EditorUtil_ConvertPath(str, true);

	CFileDialog dlg(TRUE, ".exe", str, OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, "Programs (*.exe)|*.exe||", this);
	if (dlg.DoModal() == IDCANCEL)
		return;

	// Convert back to $Steam tokens.
	str = dlg.GetPathName();
	EditorUtil_ConvertPath(str, false);
	pWnd->SetWindowText(str);
}

void COPTBuild::OnBrowseBsp() 
{
	DoBrowse(&m_cBSP);
}

void COPTBuild::OnBrowseGame() 
{
	DoBrowse(&m_cGame);
}

void COPTBuild::OnBrowseLight() 
{
	DoBrowse(&m_cLIGHT);
}

void COPTBuild::OnBrowseVis() 
{
	DoBrowse(&m_cVIS);
}

void COPTBuild::OnSelchangeConfigs() 
{
	SaveInfo(m_pConfig);

	m_pConfig = NULL;

	int iCurSel = m_cConfigs.GetCurSel();
	
	BOOL bKillFields = (iCurSel == CB_ERR) ? FALSE : TRUE;
	m_cBSP.EnableWindow(bKillFields);
	m_cLIGHT.EnableWindow(bKillFields);
	m_cVIS.EnableWindow(bKillFields);
	m_cGame.EnableWindow(bKillFields);
	m_cBSPDir.EnableWindow(bKillFields);
	
	if(iCurSel == CB_ERR)
		return;

	// get pointer to the configuration
	m_pConfig = Options.configs.FindConfig(m_cConfigs.GetItemData(iCurSel));

	// update dialog data
	EditorUtil_TransferPath(this, IDC_BSP, m_pConfig->szBSP, false);
	EditorUtil_TransferPath(this, IDC_LIGHT, m_pConfig->szLIGHT, false);
	EditorUtil_TransferPath(this, IDC_VIS, m_pConfig->szVIS, false);
	EditorUtil_TransferPath(this, IDC_GAME, m_pConfig->szExecutable, false);
	EditorUtil_TransferPath(this, IDC_BSPDIR, m_pConfig->szBSPDir, false);
}


void COPTBuild::SaveInfo(CGameConfig *pConfig)
{
	if (!pConfig)
	{
		return;
	}

	EditorUtil_TransferPath(this, IDC_BSP, m_pConfig->szBSP, true);
	EditorUtil_TransferPath(this, IDC_LIGHT, m_pConfig->szLIGHT, true);
	EditorUtil_TransferPath(this, IDC_VIS, m_pConfig->szVIS, true);
	EditorUtil_TransferPath(this, IDC_GAME, m_pConfig->szExecutable, true);
	EditorUtil_TransferPath(this, IDC_BSPDIR, m_pConfig->szBSPDir, true);
}


void COPTBuild::UpdateConfigList()
{
	m_pConfig = NULL;

	::UpdateConfigList(m_cConfigs);
	::SelectActiveConfig(m_cConfigs);

	OnSelchangeConfigs();
	SetModified();
}

BOOL COPTBuild::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
	UpdateConfigList();
	SetModified(TRUE);
	
	return TRUE;
}

BOOL COPTBuild::OnApply() 
{
	SaveInfo(m_pConfig);
	
	return CPropertyPage::OnApply();
}

BOOL COPTBuild::HandleInsertParm(UINT nID)
// insert a parm at the current cursor location into the parameters
//  edit control
{
	LPCTSTR pszInsert = 0;

	switch (nID)
	{
		case id_InsertParmMapFileNoExt:
			pszInsert = "$file";
			break;
		case id_InsertParmMapFile:
			pszInsert = "$file.$ext";
			break;
		case id_InsertParmMapPath:
			pszInsert = "$path";
			break;
		case id_InsertParmExeDir:
			pszInsert = "$exedir";
			break;
		case id_InsertParmBspDir:
			pszInsert = "$bspdir";
			break;
		case id_InsertParmGameDir:
			pszInsert = "$gamedir";
			break;
	}

	Assert(pszInsert != NULL);
	if (!pszInsert)
	{
		return TRUE;
	}

	m_pAddParmWnd->ReplaceSel(pszInsert);

	return TRUE;
}


void COPTBuild::InsertParm(UINT nID, CEdit *pEdit)
{
	m_pAddParmWnd = pEdit;

	// two stages - name/description OR data itself
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, id_InsertParmMapFileNoExt, "Map Filename (no extension)");
	menu.AppendMenu(MF_STRING, id_InsertParmMapFile, "Map Filename (with extension)");
	menu.AppendMenu(MF_STRING, id_InsertParmMapPath, "Map Path (no filename)");
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, id_InsertParmExeDir, "Game Executable Directory");
	menu.AppendMenu(MF_STRING, id_InsertParmBspDir, "BSP Directory");
	menu.AppendMenu(MF_STRING, id_InsertParmGameDir, "Game Directory");

	// track menu
	CWnd *pButton = GetDlgItem(nID);
	CRect r;
	pButton->GetWindowRect(r);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, r.left, r.bottom, this, NULL);
}


void COPTBuild::OnParmsBsp() 
{
	InsertParm(IDC_PARMS_BSP, &m_cBSP);	
}

void COPTBuild::OnParmsGame() 
{
	InsertParm(IDC_PARMS_GAME, &m_cGame);
}

void COPTBuild::OnParmsLight() 
{
	InsertParm(IDC_PARMS_LIGHT, &m_cLIGHT);
}

void COPTBuild::OnParmsVis() 
{
	InsertParm(IDC_PARMS_VIS, &m_cVIS);
}

void COPTBuild::OnBrowseBspdir() 
{
	CString str;
	m_cBSPDir.GetWindowText(str);
	EditorUtil_ConvertPath(str, true);

	char szTemp[MAX_PATH];
	Q_strncpy(szTemp, str, MAX_PATH);

	BROWSEINFO bi;
	memset(&bi, 0, sizeof bi);
	bi.hwndOwner = m_hWnd;
	bi.pszDisplayName = szTemp;
	bi.lpszTitle = "Select BSP file directory";
	bi.ulFlags = BIF_RETURNONLYFSDIRS;

	LPITEMIDLIST idl = SHBrowseForFolder(&bi);

	if (idl == NULL)
		return;

	SHGetPathFromIDList(idl, szTemp);
	CoTaskMemFree(idl);

	// Convert back to %STEAM%.
	str = szTemp;
	EditorUtil_ConvertPath(str, false);
	m_cBSPDir.SetWindowText(str);
}

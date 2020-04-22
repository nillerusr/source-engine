//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// RunMapExpertDlg.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "RunMapExpertDlg.h"
#include "RunMapCfgDlg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// CRunMapExpertDlg dialog

CRunMapExpertDlg::CRunMapExpertDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CRunMapExpertDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CRunMapExpertDlg)
	//}}AFX_DATA_INIT

	m_pActiveSequence = NULL;
	m_bNoUpdateCmd = FALSE;
	m_bSwitchMode = FALSE;
}


void CRunMapExpertDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRunMapExpertDlg)
	DDX_Control(pDX, IDC_USEPROCESSWND, m_cUseProcessWnd);
	DDX_Control(pDX, IDC_CONFIGURATIONS, m_cCmdSequences);
	DDX_Control(pDX, IDC_MOVEUP, m_cMoveUp);
	DDX_Control(pDX, IDC_MOVEDOWN, m_cMoveDown);
	DDX_Control(pDX, IDC_ENSUREFN, m_cEnsureFn);
	DDX_Control(pDX, IDC_ENSURECHECK, m_cEnsureCheck);
	DDX_Control(pDX, IDC_PARAMETERS, m_cParameters);
	DDX_Control(pDX, IDC_COMMAND, m_cCommand);
	//}}AFX_DATA_MAP

	DDX_Control(pDX, IDC_COMMANDLIST, m_cCommandList);
}


// dvs: this is duplicated in OPTBuild.cpp!!
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


enum
{
	id_BrExecutable = 0x150,
	id_BrChangeDir,
	id_BrCopyFile,
	id_BrDelFile,
	id_BrRenameFile,

	id_BrGameProgram,
	id_BrVISProgram,
	id_BrBSPProgram,
	id_BrLIGHTProgram,

	id_BrEnd
};


BEGIN_MESSAGE_MAP(CRunMapExpertDlg, CDialog)
	//{{AFX_MSG_MAP(CRunMapExpertDlg)
	ON_BN_CLICKED(IDC_BROWSECOMMAND, OnBrowsecommand)
	ON_LBN_SELCHANGE(IDC_COMMANDLIST, OnSelchangeCommandlist)
	ON_BN_CLICKED(IDC_INSERTPARM, OnInsertparm)
	ON_BN_CLICKED(IDC_MOVEDOWN, OnMovedown)
	ON_BN_CLICKED(IDC_MOVEUP, OnMoveup)
	ON_BN_CLICKED(IDC_NEW, OnNew)
	ON_BN_CLICKED(IDC_NORMAL, OnNormal)
	ON_BN_CLICKED(IDC_REMOVE, OnRemove)
	ON_EN_UPDATE(IDC_COMMAND, OnUpdateCommand)
	ON_EN_UPDATE(IDC_PARAMETERS, OnUpdateParameters)
	ON_BN_CLICKED(IDC_ENSURECHECK, OnEnsurecheck)
	ON_EN_UPDATE(IDC_ENSUREFN, OnUpdateEnsurefn)
	ON_CBN_SELCHANGE(IDC_CONFIGURATIONS, OnSelchangeConfigurations)
	ON_BN_CLICKED(IDC_EDITCONFIGS, OnEditconfigs)
	ON_BN_CLICKED(IDC_USEPROCESSWND, OnUseprocesswnd)
	ON_COMMAND_EX_RANGE(id_InsertParmMapFileNoExt, id_InsertParmEnd, HandleInsertParm)
	ON_COMMAND_EX_RANGE(id_BrExecutable, id_BrEnd, HandleInsertCommand)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRunMapExpertDlg message handlers

BOOL CRunMapExpertDlg::HandleInsertCommand(UINT nID)
// insert a parm at the current cursor location into the parameters
//  edit control
{
	PCCOMMAND pCommand = GetCommandAtIndex(NULL);

	if(!pCommand)
		return TRUE;	// no command

	if(nID == id_BrExecutable)
	{
		CFileDialog dlg(TRUE, "exe", NULL, OFN_HIDEREADONLY | 
			OFN_FILEMUSTEXIST |	OFN_NOCHANGEDIR, 
			"Executable Files|*.exe||", this);
		if(dlg.DoModal() == IDCANCEL)
			return TRUE;
		m_cCommand.SetWindowText(dlg.m_ofn.lpstrFile);
		pCommand->iSpecialCmd = 0;
	}
	else
	{
		pCommand->iSpecialCmd = 0;

		switch(nID)
		{
		case id_BrCopyFile:
			pCommand->iSpecialCmd = CCCopyFile;
			break;
		case id_BrDelFile:
			pCommand->iSpecialCmd = CCDelFile;
			break;
		case id_BrRenameFile:
			pCommand->iSpecialCmd = CCRenameFile;
			break;
		case id_BrChangeDir:
			pCommand->iSpecialCmd = CCChangeDir;
			break;
		case id_BrGameProgram:
			m_cCommand.SetWindowText("$game_exe");
			break;
		case id_BrVISProgram:
			m_cCommand.SetWindowText("$vis_exe");
			break;
		case id_BrLIGHTProgram:
			m_cCommand.SetWindowText("$light_exe");
			break;
		case id_BrBSPProgram:
			m_cCommand.SetWindowText("$bsp_exe");
			break;
		}

		if(pCommand->iSpecialCmd)
			pCommand->bLongFilenames = TRUE;

		OnSelchangeCommandlist();
		UpdateCommandWithEditFields(-1);
	}

	return TRUE;
}


void CRunMapExpertDlg::OnBrowsecommand(void)
{
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, id_BrExecutable, "Executable");
	menu.AppendMenu(MF_STRING, id_BrChangeDir, "Change Directory");
	menu.AppendMenu(MF_STRING, id_BrCopyFile, "Copy File");
	menu.AppendMenu(MF_STRING, id_BrDelFile, "Delete File");
	menu.AppendMenu(MF_STRING, id_BrRenameFile, "Rename File");
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, id_BrBSPProgram, "BSP program");
	menu.AppendMenu(MF_STRING, id_BrVISProgram, "VIS program");
	menu.AppendMenu(MF_STRING, id_BrLIGHTProgram, "LIGHT program");
	menu.AppendMenu(MF_STRING, id_BrGameProgram, "Game program");

	// track menu
	CWnd *pButton = GetDlgItem(IDC_BROWSECOMMAND);
	CRect r;
	pButton->GetWindowRect(r);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, r.left, r.bottom, this, NULL);
}


LPCTSTR CRunMapExpertDlg::GetCmdString(PCCOMMAND pCommand)
{
	switch(pCommand->iSpecialCmd)
	{
	case 0:
		return pCommand->szRun;
	case CCCopyFile:
		return "Copy File";
	case CCDelFile:
		return "Delete File";
	case CCRenameFile:
		return "Rename File";
	case CCChangeDir:
		return "Change Directory";
	}

	return "";
}

void CRunMapExpertDlg::OnSelchangeCommandlist() 
{
	int iIndex = -1;

	// change the selection in the command list - update the command
	//  and parameters edit boxes
	PCCOMMAND pCommand = GetCommandAtIndex(&iIndex);
	
	// enable/disable controls
	BOOL bEnable = pCommand ? TRUE : FALSE;
	int iEnableCmds[] =
	{
		// edit fields:
		IDC_COMMAND,
		IDC_PARAMETERS,
		IDC_ENSUREFN,

		// checkboxes/buttons:
		IDC_ENSURECHECK,
		IDC_USEPROCESSWND,
		IDC_INSERTPARM,
		IDC_BROWSECOMMAND,

		-1
	};

	m_bNoUpdateCmd = TRUE;
	for(int i = 0; iEnableCmds[i] != -1; i++)
	{
		CWnd *pWnd = GetDlgItem(iEnableCmds[i]);
		pWnd->EnableWindow(bEnable);
		if(bEnable == FALSE)
		{
		// ensure fields are cleared if we're disabling them
			if(i < 3)
				pWnd->SetWindowText("");
			else
				((CButton*)pWnd)->SetCheck(0);
		}
	}
	m_bNoUpdateCmd = FALSE;

	if(!pCommand)
		return;

	// set moveup/movedown buttons
	m_cMoveUp.EnableWindow(iIndex != 0);
	m_cMoveDown.EnableWindow(iIndex != m_cCommandList.GetCount() - 1);
	
	m_bNoUpdateCmd = TRUE;

	m_cCommand.SetWindowText(GetCmdString(pCommand));
	m_cParameters.SetWindowText(pCommand->szParms);
	m_cEnsureCheck.SetCheck(pCommand->bEnsureCheck);
	m_cEnsureFn.SetWindowText(pCommand->szEnsureFn);		
	m_cUseProcessWnd.SetCheck(pCommand->bUseProcessWnd);
	// don't forget to call this:
		OnEnsurecheck();

	m_bNoUpdateCmd = FALSE;
}


BOOL CRunMapExpertDlg::HandleInsertParm(UINT nID)
// insert a parm at the current cursor location into the parameters
//  edit control
{
	LPCTSTR pszInsert;

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
		default:
			pszInsert = "$gamedir";
			break;
	}

	Assert(pszInsert);
	if(!pszInsert)
		return TRUE;

	m_cParameters.ReplaceSel(pszInsert);

	return TRUE;
}


void CRunMapExpertDlg::OnInsertparm(void)
{
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
	CWnd *pButton = GetDlgItem(IDC_INSERTPARM);
	CRect r;
	pButton->GetWindowRect(r);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, r.left, r.bottom, this, NULL);
}


void CRunMapExpertDlg::DeleteCommand(int iIndex)
{
	// kill the command at that index (deletes the dataptr memory too)
	PCCOMMAND pCommand = GetCommandAtIndex(&iIndex);
	m_cCommandList.DeleteString(iIndex);

	if(iIndex >= m_cCommandList.GetCount()-1)
		iIndex = m_cCommandList.GetCount()-1;

	m_cCommandList.SetCurSel(iIndex);
	// selection has "changed"
	OnSelchangeCommandlist();

	delete pCommand;
}

void CRunMapExpertDlg::AddCommand(int iIndex, PCCOMMAND pCommand)
{
	// add a command to the list at the index specified in iIndex (-1 to add
	//  at end of list.) 
	CString str;
	str.Format("%s %s", GetCmdString(pCommand), pCommand->szParms);
	iIndex = m_cCommandList.InsertString(iIndex, str);
	m_cCommandList.SetItemDataPtr(iIndex, PVOID(pCommand));
}


void CRunMapExpertDlg::MoveCommand(int iIndex, BOOL bUp)
{
	PCCOMMAND pCommand = GetCommandAtIndex(&iIndex);
	if(!pCommand)
		return;

	// keep check state of item in listbox
	BOOL bChecked = m_cCommandList.GetCheck(iIndex);

	// don't bother with the string - that's made from the command/parms
	//  struct that the item's dataptr points to
	m_cCommandList.DeleteString(iIndex);

	int iNewIndex = iIndex + (bUp ? -1 : +1);
	AddCommand(iNewIndex, pCommand);

	// restore check state saved above
	m_cCommandList.SetCheck(iNewIndex, bChecked);

	// selection has changed
	m_cCommandList.SetCurSel(iNewIndex);
	OnSelchangeCommandlist();
}


void CRunMapExpertDlg::OnMovedown() 
{
	MoveCommand(-1, FALSE);
}

void CRunMapExpertDlg::OnMoveup() 
{
	MoveCommand(-1, TRUE);
}

void CRunMapExpertDlg::OnNew() 
{
	// add a command
	PCCOMMAND pCommand = new CCOMMAND;
	memset(pCommand, 0, sizeof(CCOMMAND));
	pCommand->bUseProcessWnd = TRUE;
	AddCommand(-1, pCommand);
	m_cCommandList.SetCurSel(m_cCommandList.GetCount()-1);
	// sleection has changed
	OnSelchangeCommandlist();
}

void CRunMapExpertDlg::OnNormal() 
{
	m_bSwitchMode = TRUE;
	SaveCommandsToSequence();

	CHammer *pApp = (CHammer*) AfxGetApp();
	pApp->SaveSequences();

	EndDialog(IDOK);
}

void CRunMapExpertDlg::UpdateCommandWithEditFields(int iIndex)
{
	PCCOMMAND pCommand = GetCommandAtIndex(&iIndex);
	
	// update command struct with edit fields:
	m_cCommand.GetWindowText(pCommand->szRun, MAX_PATH);
	m_cParameters.GetWindowText(pCommand->szParms, MAX_PATH);
	m_cEnsureFn.GetWindowText(pCommand->szEnsureFn, MAX_PATH);
	pCommand->bUseProcessWnd = m_cUseProcessWnd.GetCheck();
	pCommand->bEnsureCheck = m_cEnsureCheck.GetCheck();

	// save checked state..
	BOOL bCmdChecked = m_cCommandList.GetCheck(iIndex);

	// update list by deleting/adding command
	m_cCommandList.SetRedraw(FALSE);
	m_cCommandList.DeleteString(iIndex);
	AddCommand(iIndex, pCommand);
	m_cCommandList.SetCurSel(iIndex);
	m_cCommandList.SetRedraw(TRUE);
	m_cCommandList.Invalidate();

	m_cCommandList.SetCheck(iIndex, bCmdChecked);

	// DON'T call OnCommandlistSelchange() here
}

PCCOMMAND CRunMapExpertDlg::GetCommandAtIndex(int *piIndex)
{
	// make sure we're pointing at something:
	int iIndex = -1;
	if(piIndex == NULL)
		piIndex = &iIndex;
	
	// return the current command structure 
	if(piIndex[0] == -1)
		piIndex[0] = m_cCommandList.GetCurSel();
	if(piIndex[0] == LB_ERR)
		return NULL;
	PCCOMMAND pCommand = PCCOMMAND(m_cCommandList.GetItemDataPtr(piIndex[0]));
	return pCommand;
}

void CRunMapExpertDlg::OnRemove() 
{
	// kill the current command
	int iIndex = m_cCommandList.GetCurSel();
	if(iIndex == LB_ERR)
		return;
	DeleteCommand(iIndex);
}

void CRunMapExpertDlg::OnUpdateCommand() 
{
	if(!m_bNoUpdateCmd)
	{
		// make sure no special command is contained here ..
		// (this is only ever called when the user types
		//  in the command edit field.)
		PCCOMMAND pCommand = GetCommandAtIndex(NULL);
		if(pCommand->iSpecialCmd)
		{
			// clear out command .. set the noupdatecmd
			//  flag so we don't get into a stack overflow
			m_bNoUpdateCmd = TRUE;
			m_cCommand.SetWindowText("");
			m_bNoUpdateCmd = FALSE;
			pCommand->iSpecialCmd = 0;
		}
		UpdateCommandWithEditFields(-1);
	}
}

void CRunMapExpertDlg::OnUpdateParameters() 
{
	if(!m_bNoUpdateCmd)
		UpdateCommandWithEditFields(-1);	
}

void CRunMapExpertDlg::OnEnsurecheck() 
{
	if(!m_bNoUpdateCmd)
		UpdateCommandWithEditFields(-1);
	// enable/disable edit field
	m_cEnsureFn.EnableWindow(m_cEnsureCheck.GetCheck());
}

void CRunMapExpertDlg::OnUpdateEnsurefn() 
{
	if(!m_bNoUpdateCmd)
		UpdateCommandWithEditFields(-1);
}

void CRunMapExpertDlg::OnUseprocesswnd() 
{
	// update the command here..
	PCCOMMAND pCommand = GetCommandAtIndex(NULL);
	Assert(pCommand);
	pCommand->bUseProcessWnd = m_cUseProcessWnd.GetCheck() ? TRUE : FALSE;
}

void CRunMapExpertDlg::InitSequenceList()
{
	// add all the information from the CHammer object into
	//  the dialog box ..
	CHammer *pApp = (CHammer*) AfxGetApp();

	m_cCmdSequences.ResetContent();

	// add the configurations into the list ..
	int iSize = pApp->m_CmdSequences.GetSize();

	if(iSize == 0)
	{
		// add a default configuration
		CCommandSequence *pSeq = new CCommandSequence;
		strcpy(pSeq->m_szName, "Default");
		((CHammer*)AfxGetApp())->m_CmdSequences.Add(pSeq);
		iSize = 1;
	}

	for(int i = 0; i < iSize; i++)
	{
		CCommandSequence *pSeq = pApp->m_CmdSequences[i];
		int iIndex = m_cCmdSequences.AddString(pSeq->m_szName);
		m_cCmdSequences.SetItemDataPtr(iIndex, PVOID(pSeq));
	}
	
	m_pActiveSequence = NULL;
	m_cCmdSequences.SetCurSel(0);
	OnSelchangeConfigurations();
}

BOOL CRunMapExpertDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	int iSequence = AfxGetApp()->GetProfileInt("RunMapExpert", 
		"LastSequence", 0);

	InitSequenceList();

	m_cCmdSequences.SetCurSel(iSequence);
	OnSelchangeConfigurations();

	return TRUE;
}

void CRunMapExpertDlg::OnOK() 
{
	SaveCommandsToSequence();

	CHammer *pApp = (CHammer*) AfxGetApp();
	
	pApp->SaveSequences();

	CDialog::OnOK();
}

void CRunMapExpertDlg::SaveCommandsToSequence()
{
	if(!m_pActiveSequence)
		return;	// nothing set yet 

	int nCommands = m_cCommandList.GetCount();
	m_pActiveSequence->m_Commands.RemoveAll();
	for(int i = 0; i < nCommands; i++)
	{
		PCCOMMAND pCommand = PCCOMMAND(m_cCommandList.GetItemDataPtr(i));
		pCommand->bEnable = m_cCommandList.GetCheck(i);
		if (!strcmp(pCommand->szRun, "$game_exe"))
			pCommand->bNoWait = TRUE;
		m_pActiveSequence->m_Commands.Add(*pCommand);
		// free the memory:
		delete pCommand;
	}
}

void CRunMapExpertDlg::OnSelchangeConfigurations() 
{
	// save the current command list back into the previously active
	//  command sequence
	SaveCommandsToSequence();

	int iSel = m_cCmdSequences.GetCurSel();
	if(iSel == LB_ERR)	// nothing there
	{
		m_pActiveSequence = NULL;
		return;
	}

	AfxGetApp()->WriteProfileInt("RunMapExpert", "LastSequence", iSel);

	CCommandSequence *pSeq = (CCommandSequence*) 
		m_cCmdSequences.GetItemDataPtr(iSel);

	// delete strings from listbox (dataptrs already deleted 
	//  in SaveCommandsToSequence()) 
	m_cCommandList.ResetContent();

	m_pActiveSequence = pSeq;

	// add the commands from this sequence into the command listbox ..
	CCommandArray &Commands = pSeq->m_Commands;
	for(int i = 0; i < Commands.GetSize(); i++)
	{
		PCCOMMAND pCommand = new CCOMMAND(Commands[i]);
		AddCommand(i, pCommand);
		m_cCommandList.SetCheck(i, pCommand->bEnable);
	}

	// set to 0th element in list ..
	m_cCommandList.SetCurSel(0);
	OnSelchangeCommandlist();
}

void CRunMapExpertDlg::OnEditconfigs() 
{
	CRunMapCfgDlg dlg;
	SaveCommandsToSequence();
	if(dlg.DoModal() == IDOK)
		InitSequenceList();
}


void CRunMapExpertDlg::OnCancel() 
{
	SaveCommandsToSequence();

	CHammer *pApp = (CHammer*) AfxGetApp();
	pApp->SaveSequences();

	CDialog::OnCancel();
}

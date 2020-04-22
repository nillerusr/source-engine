//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "hammer.h"
#include "OPTGeneral.h"
#include "Options.h"


#pragma warning(disable:4244)


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

void EditorUtil_ConvertPath(CString &str, bool bSave);

IMPLEMENT_DYNCREATE(COPTGeneral, CPropertyPage)


BEGIN_MESSAGE_MAP(COPTGeneral, CPropertyPage)
	//{{AFX_MSG_MAP(COPTGeneral)
	ON_BN_CLICKED(IDC_INDEPENDENTWINDOWS, OnIndependentwindows)
	ON_BN_CLICKED(IDC_BROWSEAUTOSAVEDIR, OnBrowseAutosaveDir)
	ON_BN_CLICKED(IDC_ENABLEAUTOSAVE, OnEnableAutosave)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COPTGeneral::COPTGeneral(void) : CPropertyPage(COPTGeneral::IDD)
{
	//{{AFX_DATA_INIT(COPTGeneral)
	m_iMaxAutosavesPerMap = 0;
	m_iUndoLevels = 0;
	m_nMaxCameras = 5;
	//}}AFX_DATA_INIT
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COPTGeneral::~COPTGeneral(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: Ensures that our undo levels are at least 5.
//-----------------------------------------------------------------------------
void PASCAL DDV_UndoLevels(CDataExchange *pDX, int value)
{
	if (value < 5)
	{
		AfxMessageBox("Undo levels must be at least 5.", MB_ICONEXCLAMATION | MB_OK);
		pDX->Fail();
	}
}

void PASCAL DDV_MaxCameras(CDataExchange *pDX, int value)
{
	if (value < 1 || value > 100)
	{
		AfxMessageBox("Max cameras must be between 1 and 1000.", MB_ICONEXCLAMATION | MB_OK);
		pDX->Fail();
	}
}

void PASCAL DDV_AutosaveSpace(CDataExchange *pDX, int value)
{
	if ( value > 10000 )
	{
		AfxMessageBox("You have selected too much space for autosaving. The maximum value is 10000.", MB_ICONEXCLAMATION | MB_OK);
		pDX->Fail();
	}
}

void PASCAL DDV_NumberAutosaves(CDataExchange *pDX, int value)
{
	if ( value > 999 )
	{
		AfxMessageBox("Number of autosaves must be 0-999.", MB_ICONEXCLAMATION | MB_OK);
		pDX->Fail();
	}
}

void PASCAL DDV_AutosaveTimer(CDataExchange *pDX, int value)
{
	if ( value < 1 || value > 120 )
	{
		AfxMessageBox("Time must be between 1 - 120 minutes.", MB_ICONEXCLAMATION | MB_OK);
		pDX->Fail();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDX - 
//-----------------------------------------------------------------------------
void COPTGeneral::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COPTGeneral)
	DDX_Control(pDX, IDC_LOADWINPOSITIONS, m_cLoadWinPos);
	DDX_Control(pDX, IDC_INDEPENDENTWINDOWS, m_cIndependentWin);
	DDX_Control(pDX, IDC_UNDOSPIN, m_UndoSpin);
	DDX_Text(pDX, IDC_UNDO, m_iUndoLevels);	
	DDX_Text(pDX, IDC_MAX_CAMERAS, m_nMaxCameras);
	DDX_Check(pDX, IDC_STRETCH_ARCH, Options.general.bStretchArches);
	DDX_Check(pDX, IDC_GROUPWHILEIGNOREGROUPS, Options.general.bGroupWhileIgnore);
	DDX_Check(pDX, IDC_INDEPENDENTWINDOWS, Options.general.bIndependentwin);
	DDX_Check(pDX, IDC_LOADWINPOSITIONS, Options.general.bLoadwinpos);
	DDV_UndoLevels( pDX, m_iUndoLevels );
	DDV_MaxCameras( pDX, m_nMaxCameras );
	DDX_Control(pDX, IDC_ENABLEAUTOSAVE, m_cEnableAutosave);
	DDX_Check(pDX, IDC_ENABLEAUTOSAVE, Options.general.bEnableAutosave);
	DDX_Text(pDX, IDC_MAPITERATIONS, m_iMaxAutosavesPerMap);	
	DDX_Text(pDX, IDC_AUTOSAVESPACE, m_iMaxAutosaveSpace);	
	DDX_Text(pDX, IDC_SAVETIME, m_iTimeBetweenSaves);	
	DDX_Control(pDX, IDC_AUTOSAVEDIR, m_cAutosaveDir);
	DDX_Control(pDX, IDC_AUTOSAVETIMELABEL, m_cAutosaveTimeLabel);
	DDX_Control(pDX, IDC_SAVETIME, m_cAutosaveTime);
	DDX_Control(pDX, IDC_AUTOSAVESPACELABEL, m_cAutosaveSpaceLabel);
	DDX_Control(pDX, IDC_AUTOSAVESPACE, m_cAutosaveSpace);
	DDX_Control(pDX, IDC_AUTOSAVEITERATIONLABEL, m_cAutosaveIterationLabel);
	DDX_Control(pDX, IDC_MAPITERATIONS,	m_cAutosaveIterations);
	DDX_Control(pDX, IDC_AUTOSAVEDIRECTORYLABEL, m_cAutosaveDirectoryLabel);
    DDX_Control(pDX, IDC_BROWSEAUTOSAVEDIR, m_cAutosaveBrowseButton);
	
	DDV_AutosaveSpace( pDX, m_iMaxAutosaveSpace );
	DDV_NumberAutosaves( pDX, m_iMaxAutosavesPerMap );
	DDV_AutosaveTimer( pDX, m_iTimeBetweenSaves );
	DDX_Check(pDX, IDC_VGUI_MODELBROWSER, Options.general.bUseVGUIModelBrowser);
	//}}AFX_DATA_MAP		
	
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL COPTGeneral::OnInitDialog(void)
{
	char szAutosaveDir[MAX_PATH];
	APP()->GetDirectory( DIR_AUTOSAVE, szAutosaveDir );
	CString str( szAutosaveDir );

	m_nMaxCameras = Options.general.nMaxCameras;
	m_iUndoLevels = Options.general.iUndoLevels;
	m_iMaxAutosavesPerMap = Options.general.iMaxAutosavesPerMap;
	m_iMaxAutosaveSpace = Options.general.iMaxAutosaveSpace;
	m_iTimeBetweenSaves = Options.general.iTimeBetweenSaves;	
	
	CPropertyPage::OnInitDialog();

   	m_cEnableAutosave.SetCheck( Options.general.bEnableAutosave );

	m_cAutosaveDir.SetWindowText( str );

	// set undo range
	m_UndoSpin.SetRange(5, 999);

	OnEnableAutosave();
	OnIndependentwindows();

	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL COPTGeneral::OnApply(void)
{
	BOOL bResetTimer = FALSE;

	if( Options.general.iTimeBetweenSaves != m_iTimeBetweenSaves )
	{
		//if the time value has changed, the timer needs to be reset so autosaves
		//happen at the new interval in this instance of hammer
		bResetTimer = TRUE;
	}

	if( ( Options.general.iMaxAutosavesPerMap != 0 && m_iMaxAutosavesPerMap == 0 ) ||
		( Options.general.iMaxAutosavesPerMap == 0 && m_iMaxAutosavesPerMap != 0 ) )
	{
		//if the number of autosaves per map has changed to or away from 0 then 
		//the timer needs to be reset
		bResetTimer = TRUE;
	}

	Options.general.iUndoLevels = m_iUndoLevels;
	Options.general.nMaxCameras = m_nMaxCameras;
	Options.general.iMaxAutosavesPerMap = m_iMaxAutosavesPerMap;
	Options.general.iMaxAutosaveSpace = m_iMaxAutosaveSpace;
	Options.general.iTimeBetweenSaves = m_iTimeBetweenSaves;
	Options.general.bEnableAutosave = m_cEnableAutosave.GetCheck();
	CString str;
	m_cAutosaveDir.GetWindowText(str);
	
	if ( strcmp( Options.general.szAutosaveDir, str ) )
	{
		strcpy( Options.general.szAutosaveDir, str );
		bResetTimer = TRUE;
	}

	if( bResetTimer == TRUE )
	{		
		APP()->ResetAutosaveTimer();		
	}

    Options.PerformChanges(COptions::secGeneral);

	if ( Options.general.bEnableAutosave )
	{		
		if ( !APP()->VerifyAutosaveDirectory( Options.general.szAutosaveDir ) )
		{
			Options.general.bEnableAutosave = false;
			m_cEnableAutosave.SetCheck( Options.general.bEnableAutosave );
			OnEnableAutosave();		
			return FALSE;
		}
	}

	return(CPropertyPage::OnApply());
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COPTGeneral::OnIndependentwindows(void)
{
	m_cLoadWinPos.EnableWindow(m_cIndependentWin.GetCheck());
}

void COPTGeneral::OnEnableAutosave(void)
{
	int iEnabled = m_cEnableAutosave.GetCheck();
	m_cAutosaveDir.EnableWindow( iEnabled );
	m_cAutosaveTime.EnableWindow( iEnabled );
	m_cAutosaveTimeLabel.EnableWindow( iEnabled );		
	m_cAutosaveSpaceLabel.EnableWindow( iEnabled );
	m_cAutosaveSpace.EnableWindow( iEnabled );
	m_cAutosaveIterationLabel.EnableWindow( iEnabled );
	m_cAutosaveIterations.EnableWindow( iEnabled );
	m_cAutosaveDirectoryLabel.EnableWindow( iEnabled );
	m_cAutosaveBrowseButton.EnableWindow( iEnabled );
}


void COPTGeneral::OnBrowseAutosaveDir(void)
{
	char szTmp[MAX_PATH];
	if (!BrowseForFolder("Select Autosave Directory", szTmp))
	{
		return;
	}

	CString str(szTmp);
	EditorUtil_ConvertPath(str, false);
	m_cAutosaveDir.SetWindowText(str);
}

BOOL COPTGeneral::BrowseForFolder(char *pszTitle, char *pszDirectory)
{
	char szTmp[MAX_PATH];

	BROWSEINFO bi;
	memset(&bi, 0, sizeof bi);
	bi.hwndOwner = m_hWnd;
	bi.pszDisplayName = szTmp;
	bi.lpszTitle = pszTitle;
	bi.ulFlags = BIF_RETURNONLYFSDIRS;

	LPITEMIDLIST idl = SHBrowseForFolder(&bi);

	if(idl == NULL)
		return FALSE;

	SHGetPathFromIDList(idl, pszDirectory);
	CoTaskMemFree(idl);

	return TRUE;
}


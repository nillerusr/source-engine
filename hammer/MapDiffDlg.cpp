//========= Copyright Valve Corporation, All rights reserved. ============//
// MapDiffDlg.cpp : implementation file
//
#include "stdafx.h"
#include "GlobalFunctions.h"
#include "History.h"
#include "MainFrm.h"
#include "MapDiffDlg.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapSolid.h"
#include "MapView2D.h"
#include "MapWorld.h"
#include "ObjectProperties.h"	// For ObjectProperties::RefreshData
#include "Options.h"
#include "ToolManager.h"
#include "VisGroup.h"
#include "hammer.h"
#include "MapOverlay.h"
#include "GameConfig.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>
#include ".\mapdiffdlg.h"

CMapDiffDlg *s_pDlg = NULL;
CMapDoc *s_pCurrentMap = NULL;

// MapDiffDlg dialog

CMapDiffDlg::CMapDiffDlg(CWnd* pParent )
	: CDialog(CMapDiffDlg::IDD, pParent)
{
	m_bCheckSimilar = true;
}

void CMapDiffDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Check(pDX, IDC_SIMILARCHECK, m_bCheckSimilar);
	DDX_Control(pDX, IDC_MAPNAME, m_mapName);
}


BEGIN_MESSAGE_MAP(CMapDiffDlg, CDialog)
	ON_BN_CLICKED(IDC_SIMILARCHECK, OnBnClickedSimilarcheck)
	ON_BN_CLICKED(IDC_MAPBROWSE, OnBnClickedMapbrowse)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, OnBnClickedCancel)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

void CMapDiffDlg::MapDiff(CWnd *pwndParent, CMapDoc *pCurrentMapDoc)
{
	if (!s_pDlg)
	{
		s_pDlg = new CMapDiffDlg;
		s_pDlg->Create(IDD, pwndParent);
		s_pDlg->ShowWindow(SW_SHOW);
		s_pCurrentMap = pCurrentMapDoc;
	}	
}

// MapDiffDlg message handlers

void CMapDiffDlg::OnBnClickedSimilarcheck()
{
	// TODO: Add your control notification handler code here
	m_bCheckSimilar = !m_bCheckSimilar;
}

void CMapDiffDlg::OnBnClickedMapbrowse()
{
	CString	m_pszFilename;
	
	// TODO: Add your control notification handler code here
	static char szInitialDir[MAX_PATH] = "";
	if (szInitialDir[0] == '\0')
	{
		strcpy(szInitialDir, g_pGameConfig->szMapDir);
	}

	// TODO: need to prevent (or handle) opening VMF files when using old map file formats
	CFileDialog dlg(TRUE, NULL, NULL, OFN_LONGNAMES | OFN_HIDEREADONLY | OFN_NOCHANGEDIR, "Valve Map Files (*.vmf)|*.vmf|Valve Map Files Autosaves (*.vmf_autosave)|*.vmf_autosave|Worldcraft RMFs (*.rmf)|*.rmf|Worldcraft Maps (*.map)|*.map||");
	dlg.m_ofn.lpstrInitialDir = szInitialDir;
	int iRvl = dlg.DoModal();

	if (iRvl == IDCANCEL)
	{
		return;
	}

	//
	// Get the directory they browsed to for next time.
	//
	m_pszFilename = dlg.GetPathName();
	m_mapName.SetWindowText( m_pszFilename );
}

void CMapDiffDlg::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here
	OnOK();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapDiffDlg::OnOK()
{
	CString strFilename;
	m_mapName.GetWindowText( strFilename );
	CHammer *pApp = (CHammer*) AfxGetApp();
	CMapDoc *pDoc = (CMapDoc*) pApp->pMapDocTemplate->OpenDocumentFile( strFilename );	
    CUtlVector <int> IDList;
	
	const CMapObjectList *pChildren = pDoc->GetMapWorld()->GetChildren();

	FOR_EACH_OBJ( *pChildren, pos )
	{		
		int nID = pChildren->Element(pos)->GetID();
		IDList.AddToTail( nID );
	}	

	pDoc->OnCloseDocument();
	
	CVisGroup *resultsVisGroup = NULL;	
	pChildren = s_pCurrentMap->GetMapWorld()->GetChildren();
	int nTotalSimilarities = 0;
	if ( m_bCheckSimilar )
	{
		FOR_EACH_OBJ( *pChildren, pos )
		{
			CMapClass *pChild = pChildren->Element(pos)	;
			int ID = pChild->GetID();
			if ( IDList.Find( ID ) != -1 )
			{	
				if ( resultsVisGroup == NULL )
				{
					resultsVisGroup = s_pCurrentMap->VisGroups_AddGroup( "Similar" );
					nTotalSimilarities++;
				}
				pChild->AddVisGroup( resultsVisGroup );
			}
		}	
	}
	if ( nTotalSimilarities > 0 )
	{
		GetMainWnd()->MessageBox( "Similarities were found and placed into the \"Similar\" visgroup.", "Map Similarities Found", MB_OK | MB_ICONEXCLAMATION);
	}
	s_pCurrentMap->VisGroups_UpdateAll();
	DestroyWindow();
}

//-----------------------------------------------------------------------------
// Purpose: Called when our window is being destroyed.
//-----------------------------------------------------------------------------
void CMapDiffDlg::OnDestroy()
{
	delete this;
	s_pDlg = NULL;
	s_pCurrentMap = NULL;
}


void CMapDiffDlg::OnBnClickedCancel()
{
	// TODO: Add your control notification handler code here
	DestroyWindow();
}

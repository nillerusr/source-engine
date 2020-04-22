//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "ReplaceTexDlg.h"
#include "MainFrm.h"
#include "GlobalFunctions.h"
#include "TextureBrowser.h"
#include "TextureSystem.h"
#include "mapdoc.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


CReplaceTexDlg::CReplaceTexDlg(int nSelected, CWnd* pParent /*=NULL*/)
	: CDialog(CReplaceTexDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CReplaceTexDlg)
	m_iSearchAll = nSelected ? FALSE : TRUE;
	m_strFind = _T("");
	m_strReplace = _T("");
	m_iAction = 0;
	m_bMarkOnly = FALSE;
	m_bHidden = FALSE;
	m_bRescaleTextureCoordinates = false;
	//}}AFX_DATA_INIT

	m_nSelected = nSelected;
}


void CReplaceTexDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CReplaceTexDlg)
	DDX_Control(pDX, IDC_FIND, m_cFind);
	DDX_Control(pDX, IDC_REPLACE, m_cReplace);
	DDX_Control(pDX, IDC_REPLACEPIC, m_cReplacePic);
	DDX_Control(pDX, IDC_FINDPIC, m_cFindPic);
	DDX_Radio(pDX, IDC_INMARKED, m_iSearchAll);
	DDX_Text(pDX, IDC_FIND, m_strFind);
	DDX_Text(pDX, IDC_REPLACE, m_strReplace);
	DDX_Radio(pDX, IDC_ACTION, m_iAction);
	DDX_Check(pDX, IDC_MARKONLY, m_bMarkOnly);
	DDX_Check(pDX, IDC_HIDDEN, m_bHidden);
	DDX_Check(pDX, IDC_RESCALETEXTURECOORDINATES, m_bRescaleTextureCoordinates);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CReplaceTexDlg, CDialog)
	//{{AFX_MSG_MAP(CReplaceTexDlg)
	ON_BN_CLICKED(IDC_BROWSEREPLACE, OnBrowsereplace)
	ON_BN_CLICKED(IDC_BROWSEFIND, OnBrowsefind)
	ON_EN_UPDATE(IDC_FIND, OnUpdateFind)
	ON_EN_UPDATE(IDC_REPLACE, OnUpdateReplace)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CReplaceTexDlg message handlers


void CReplaceTexDlg::BrowseTex(int iEdit)
{
	CString strTex;
	CWnd *pWnd = GetDlgItem(iEdit);

	pWnd->GetWindowText(strTex);

	CTextureBrowser *pBrowser = new CTextureBrowser(GetMainWnd());
	pBrowser->SetUsed(iEdit == IDC_FIND);
	pBrowser->SetInitialTexture(strTex);

	if (pBrowser->DoModal() == IDOK)
	{
		IEditorTexture *pTex = g_Textures.FindActiveTexture(pBrowser->m_cTextureWindow.szCurTexture);
		char szName[MAX_PATH];
		if (pTex != NULL)
		{
			pTex->GetShortName(szName);
		}
		else
		{
			szName[0] = '\0';
		}
		pWnd->SetWindowText(szName);
	}

	delete pBrowser;
}

void CReplaceTexDlg::OnBrowsereplace() 
{
	BrowseTex(IDC_REPLACE);
}

void CReplaceTexDlg::OnBrowsefind() 
{
	BrowseTex(IDC_FIND);
}

//
// find/replace text string updates:
//

void CReplaceTexDlg::OnUpdateFind() 
{
	// get texture window and set texture in there
	CString strTex;
	m_cFind.GetWindowText(strTex);
	IEditorTexture *pTex = g_Textures.FindActiveTexture(strTex);
	m_cFindPic.SetTexture(pTex);
}

void CReplaceTexDlg::OnUpdateReplace() 
{
	// get texture window and set texture in there
	CString strTex;
	m_cReplace.GetWindowText(strTex);
	IEditorTexture *pTex = g_Textures.FindActiveTexture(strTex);
	m_cReplacePic.SetTexture(pTex);
}

BOOL CReplaceTexDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	if(!m_nSelected)
	{
		CWnd *pWnd = GetDlgItem(IDC_INMARKED);
		pWnd->EnableWindow(FALSE);
	}

	OnUpdateFind();

	return TRUE;
}


void CReplaceTexDlg::DoReplaceTextures()
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( pDoc )
	{
		pDoc->ReplaceTextures( 
			m_strFind, 
			m_strReplace, 
			m_iSearchAll, 
			m_iAction | ( m_bMarkOnly ? 0x100 : 0 ), 
			m_bHidden,
			(m_bRescaleTextureCoordinates != 0)
			);
	}
}

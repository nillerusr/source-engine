//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// EditPrefabDlg.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "EditPrefabDlg.h"
#include "Prefabs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// CEditPrefabDlg dialog


CEditPrefabDlg::CEditPrefabDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CEditPrefabDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CEditPrefabDlg)
	m_strDescript = _T("");
	m_strName = _T("");
	//}}AFX_DATA_INIT

	iMaxDescriptChars = 80;
	iMaxNameChars = 30;
	m_bEnableLibrary = FALSE;
}


void CEditPrefabDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEditPrefabDlg)
	DDX_Control(pDX, IDC_CREATEIN, m_CreateIn);
	DDX_Control(pDX, IDC_NAME, m_Name);
	DDX_Control(pDX, IDC_DESCRIPT, m_Descript);
	DDX_Text(pDX, IDC_DESCRIPT, m_strDescript);
	DDX_Text(pDX, IDC_NAME, m_strName);
	//}}AFX_DATA_MAP
	DDV_MaxChars(pDX, m_strDescript, iMaxDescriptChars);
	DDV_MaxChars(pDX, m_strName, iMaxNameChars);
}


BEGIN_MESSAGE_MAP(CEditPrefabDlg, CDialog)
	//{{AFX_MSG_MAP(CEditPrefabDlg)
	ON_CBN_SELCHANGE(IDC_CREATEIN, OnSelchangeCreatein)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEditPrefabDlg message handlers

void CEditPrefabDlg::SetRanges(int iMaxDescript, int iMaxName)
{
	if(iMaxDescript != -1)
		iMaxDescriptChars = iMaxDescript;
	if(iMaxName != -1)
		iMaxNameChars = iMaxName;
}

void CEditPrefabDlg::EnableLibrary(BOOL b)
{
	m_bEnableLibrary = b;
}

BOOL CEditPrefabDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	if(!m_bEnableLibrary)
	{
		GetDlgItem(IDC_CREATEINPROMPT)->SetRedraw(FALSE);
		m_CreateIn.SetRedraw(FALSE);
	}
	else
	{
		// set title
		SetWindowText("Create Prefab");

		// add all the prefab libraries to it
		POSITION p = ENUM_START;
		CPrefabLibrary *pLibrary = CPrefabLibrary::EnumLibraries(p);
		while(pLibrary)
		{
			int iIndex = m_CreateIn.AddString(pLibrary->GetName());
			m_CreateIn.SetItemData(iIndex, pLibrary->GetID());
			pLibrary = CPrefabLibrary::EnumLibraries(p);
		}
		m_CreateIn.SetCurSel(0);
		OnSelchangeCreatein();
	}

	return TRUE;
}

void CEditPrefabDlg::OnSelchangeCreatein() 
{
	m_dwLibraryID = m_CreateIn.GetItemData(m_CreateIn.GetCurSel());	
}

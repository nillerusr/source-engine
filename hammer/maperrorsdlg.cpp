//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// MapErrorsDlg.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "MapErrorsDlg.h"
#include "Error3d.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// CMapErrorsDlg dialog


CMapErrorsDlg::CMapErrorsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CMapErrorsDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CMapErrorsDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CMapErrorsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMapErrorsDlg)
	DDX_Control(pDX, IDC_ERRORS, m_cErrors);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CMapErrorsDlg, CDialog)
	//{{AFX_MSG_MAP(CMapErrorsDlg)
	ON_BN_CLICKED(IDC_CLEAR, OnClear)
	ON_LBN_DBLCLK(IDC_ERRORS, OnDblclkErrors)
	ON_BN_CLICKED(IDC_VIEW, OnView)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMapErrorsDlg message handlers

void CMapErrorsDlg::OnClear() 
{
}

void CMapErrorsDlg::OnDblclkErrors() 
{
}

void CMapErrorsDlg::OnView() 
{
}

BOOL CMapErrorsDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	// fill list with errors
	error3d * pError = Enum3dErrors(TRUE);
	while(pError)
	{
		m_cErrors.AddString(pError->pszReason);
		m_cErrors.SetItemDataPtr(m_cErrors.GetCount()-1, PVOID(pError));
		pError = Enum3dErrors();
	}
	
	return TRUE;
}

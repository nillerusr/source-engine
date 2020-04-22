//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// UndoWarningDlg.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "UndoWarningDlg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// CUndoWarningDlg dialog


CUndoWarningDlg::CUndoWarningDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CUndoWarningDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CUndoWarningDlg)
	m_bNoShow = FALSE;
	//}}AFX_DATA_INIT
}


void CUndoWarningDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUndoWarningDlg)
	DDX_Check(pDX, IDC_NOSHOW, m_bNoShow);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CUndoWarningDlg, CDialog)
	//{{AFX_MSG_MAP(CUndoWarningDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CUndoWarningDlg message handlers

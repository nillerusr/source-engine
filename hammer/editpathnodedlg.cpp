//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// EditPathNodeDlg.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "EditPathNodeDlg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// CEditPathNodeDlg dialog


CEditPathNodeDlg::CEditPathNodeDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CEditPathNodeDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CEditPathNodeDlg)
	m_bRetrigger = FALSE;
	m_iSpeed = 0;
	m_iWait = 0;
	m_iYawSpeed = 0;
	m_strName = _T("");
	//}}AFX_DATA_INIT
}


void CEditPathNodeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEditPathNodeDlg)
	DDX_Check(pDX, IDC_RETRIGGER, m_bRetrigger);
	DDX_Text(pDX, IDC_SPEED, m_iSpeed);
	DDX_Text(pDX, IDC_PCWAIT, m_iWait);
	DDX_Text(pDX, IDC_YAWSPEED, m_iYawSpeed);
	DDX_Text(pDX, IDC_NAME, m_strName);
	DDV_MaxChars(pDX, m_strName, 64);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CEditPathNodeDlg, CDialog)
	//{{AFX_MSG_MAP(CEditPathNodeDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEditPathNodeDlg message handlers

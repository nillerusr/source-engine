//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// TransformDlg.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "TransformDlg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// CTransformDlg dialog


CTransformDlg::CTransformDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CTransformDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CTransformDlg)
	m_iMode = -1;
	m_X = 0.0f;
	m_Y = 0.0f;
	m_Z = 0.0f;
	//}}AFX_DATA_INIT
}


void CTransformDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTransformDlg)
	DDX_Radio(pDX, IDC_MODE, m_iMode);
	DDX_Text(pDX, IDC_X, m_X);
	DDX_Text(pDX, IDC_Y, m_Y);
	DDX_Text(pDX, IDC_Z, m_Z);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTransformDlg, CDialog)
	//{{AFX_MSG_MAP(CTransformDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTransformDlg message handlers

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// NewDocType.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "NewDocType.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// CNewDocType dialog


CNewDocType::CNewDocType(CWnd* pParent /*=NULL*/)
	: CDialog(CNewDocType::IDD, pParent)
{
	//{{AFX_DATA_INIT(CNewDocType)
	m_iNewType = -1;
	//}}AFX_DATA_INIT
}


void CNewDocType::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNewDocType)
	DDX_Radio(pDX, IDC_NEWTYPE, m_iNewType);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CNewDocType, CDialog)
	//{{AFX_MSG_MAP(CNewDocType)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNewDocType message handlers

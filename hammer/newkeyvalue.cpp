//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// NewKeyValue.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "NewKeyValue.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// CNewKeyValue dialog


CNewKeyValue::CNewKeyValue(CWnd* pParent /*=NULL*/)
	: CDialog(CNewKeyValue::IDD, pParent)
{
	//{{AFX_DATA_INIT(CNewKeyValue)
	m_Key = _T("");
	m_Value = _T("");
	//}}AFX_DATA_INIT
}


void CNewKeyValue::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNewKeyValue)
	DDX_Text(pDX, IDC_KEY, m_Key);
	DDV_MaxChars(pDX, m_Key, 31);
	DDX_Text(pDX, IDC_VALUE, m_Value);
	DDV_MaxChars(pDX, m_Value, 80);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CNewKeyValue, CDialog)
	//{{AFX_MSG_MAP(CNewKeyValue)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNewKeyValue message handlers

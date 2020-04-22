//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef PROPSHT_H
#define PROPSHT_H
#ifdef WIN32
#pragma once
#endif

// AllControlsSheet.h : header file
//

// This is a part of the Microsoft Foundation Classes C++ library.
// Copyright (C) 1992-1998 Microsoft Corporation
// All rights reserved.
//
// This source code is only intended as a supplement to the
// Microsoft Foundation Classes Reference and related
// electronic documentation provided with the library.
// See these sources for detailed information regarding the
// Microsoft Foundation Classes product.

/////////////////////////////////////////////////////////////////////////////
// CAllControlsSheet

#include "FolderSelectProps.h"
#include "LogSelectProps.h"
#include "SwitchProps.h"



class CAllControlsSheet : public CPropertySheet
{
	DECLARE_DYNAMIC(CAllControlsSheet)

// Construction
public:
	CAllControlsSheet(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	CAllControlsSheet(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);

protected:
	void AddControlPages(void);

// Attributes
public:

	CFolderSelectProps m_FolderPage;
	CLogSelectProps m_LogPage;
	CSwitchProps m_SwitchPage;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAllControlsSheet)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CAllControlsSheet();
	virtual BOOL OnInitDialog();

	// Generated message map functions
protected:

	HICON m_hIcon;

	//{{AFX_MSG(CAllControlsSheet)
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

#endif // PROPSHT_H

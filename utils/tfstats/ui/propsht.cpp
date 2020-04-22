//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// AllControlsSheet.cpp : implementation file
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

#include "stdafx.h"
//#include "CmnCtrl2.h"
#include "propsht.h"
//#include "progctrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


class CAboutDlg : public CPropertyPage
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CPropertyPage(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
	m_psp.dwFlags &= ~PSP_HASHELP;
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CPropertyPage)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAllControlsSheet

IMPLEMENT_DYNAMIC(CAllControlsSheet, CPropertySheet)

CAllControlsSheet::CAllControlsSheet(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
	AddControlPages();
	
	//CButton* okbut=(CButton*)this->GetDlgItem(IDC_OK);
	// TODO :: Add the pages for the rest of the controls here.
}

CAllControlsSheet::CAllControlsSheet(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
	AddControlPages();
	//CButton* okbut=(CButton*)this->GetDlgItem(IDC_OK);
}

CAllControlsSheet::~CAllControlsSheet()
{
}

void CAllControlsSheet::AddControlPages()
{
	m_psh.dwFlags|=PSH_NOAPPLYNOW;
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_psh.dwFlags |= PSH_USEHICON;
	m_psh.dwFlags &= ~PSH_HASHELP;  // Lose the Help button
	//m_psh.dwFlags |= PSH_WIZARD;
	m_psh.hIcon = m_hIcon;

	AddPage(&m_LogPage);
	AddPage(&m_SwitchPage);
	AddPage(&m_FolderPage);
	AddPage(new CAboutDlg);

}

BEGIN_MESSAGE_MAP(CAllControlsSheet, CPropertySheet)
	//{{AFX_MSG_MAP(CAllControlsSheet)
	ON_WM_QUERYDRAGICON()
	ON_WM_SYSCOMMAND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAllControlsSheet message handlers

BOOL CAllControlsSheet::OnInitDialog()
{
	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	BOOL result= CPropertySheet::OnInitDialog();

	this->SetActivePage(1);
	this->SetActivePage(2);
	this->SetActivePage(3);
	this->SetActivePage(0);

	return result;

}
HCURSOR CAllControlsSheet::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

BOOL CAllControlsSheet::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext)
{
	//removing the default DS_CONTEXT_HELP style
//  dwStyle= WS_SYSMENU | WS_POPUP | WS_CAPTION | DS_MODALFRAME | WS_VISIBLE;
	return CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext);
}

void CAllControlsSheet::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CPropertySheet::OnSysCommand(nID, lParam);
	}
}

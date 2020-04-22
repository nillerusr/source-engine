//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// PakViewFiles.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "PakDoc.h"
#include "PakViewFiles.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPakViewFiles

IMPLEMENT_DYNCREATE(CPakViewFiles, CListView)

CPakViewFiles::CPakViewFiles()
{
}

CPakViewFiles::~CPakViewFiles()
{
}


BEGIN_MESSAGE_MAP(CPakViewFiles, CListView)
	//{{AFX_MSG_MAP(CPakViewFiles)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPakViewFiles drawing

void CPakViewFiles::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: add draw code here
}

/////////////////////////////////////////////////////////////////////////////
// CPakViewFiles diagnostics

#ifdef _DEBUG
void CPakViewFiles::AssertValid() const
{
	CListView::AssertValid();
}

void CPakViewFiles::Dump(CDumpContext& dc) const
{
	CListView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CPakViewFiles message handlers

void CPakViewFiles::OnInitialUpdate() 
{
	CListView::OnInitialUpdate();
	CListCtrl& lc = GetListCtrl();

	// modify the list control's attributes
	DWORD dwStyle = GetWindowLong(lc.m_hWnd, GWL_STYLE);
	SetWindowLong(lc.m_hWnd, GWL_STYLE, (dwStyle & ~LVS_TYPEMASK) |
		LVS_ALIGNLEFT | LVS_AUTOARRANGE | LVS_REPORT | // LVS_ICON |
  		// LVS_NOITEMDATA | 
		 LVS_SORTASCENDING);

	// add some headers
	
	// 1. name of entry
	lc.InsertColumn(0, "Name", LVCFMT_LEFT, 150, colName);
	// 2. size of entry
	lc.InsertColumn(1, "Size", LVCFMT_RIGHT, 80, colSize);
	// 3. type of entry
	lc.InsertColumn(2, "Type", LVCFMT_LEFT, 180, colType);
}

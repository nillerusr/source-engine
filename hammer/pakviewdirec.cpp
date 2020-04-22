//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// PakViewDirec.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "PakDoc.h"
#include "PakViewDirec.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPakViewDirec

IMPLEMENT_DYNCREATE(CPakViewDirec, CTreeView)

CPakViewDirec::CPakViewDirec()
{
}

CPakViewDirec::~CPakViewDirec()
{
}


BEGIN_MESSAGE_MAP(CPakViewDirec, CTreeView)
	//{{AFX_MSG_MAP(CPakViewDirec)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPakViewDirec drawing

void CPakViewDirec::OnDraw(CDC* pDC)
{
	CPakDoc* pDoc = GetDocument();
	// TODO: add draw code here
}

/////////////////////////////////////////////////////////////////////////////
// CPakViewDirec diagnostics

#ifdef _DEBUG
void CPakViewDirec::AssertValid() const
{
	CTreeView::AssertValid();
}

void CPakViewDirec::Dump(CDumpContext& dc) const
{
	CTreeView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CPakViewDirec message handlers

void CPakViewDirec::OnInitialUpdate() 
{
	CTreeView::OnInitialUpdate();
	CTreeCtrl &tc = GetTreeCtrl();

	// modify the tree control's attributes
	DWORD dwStyle = GetWindowLong(tc.m_hWnd, GWL_STYLE);
	SetWindowLong(tc.m_hWnd, GWL_STYLE, dwStyle | TVS_HASLINES);

	// set the image list
	
}

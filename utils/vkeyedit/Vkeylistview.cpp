//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// Vkeylistview.cpp : implementation file
//

#include "stdafx.h"
#include "vkeyedit.h"
#include "Vkeylistview.h"

#include <KeyValues.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CVkeylistview

IMPLEMENT_DYNCREATE(CVkeylistview, CListView)

CVkeylistview::CVkeylistview()
{
}

CVkeylistview::~CVkeylistview()
{
}


BEGIN_MESSAGE_MAP(CVkeylistview, CListView)
	//{{AFX_MSG_MAP(CVkeylistview)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVkeylistview drawing

void CVkeylistview::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: add draw code here
}

/////////////////////////////////////////////////////////////////////////////
// CVkeylistview diagnostics

#ifdef _DEBUG
void CVkeylistview::AssertValid() const
{
	CListView::AssertValid();
}

void CVkeylistview::Dump(CDumpContext& dc) const
{
	CListView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CVkeylistview message handlers

void CVkeylistview::OnInitialUpdate() 
{
	CListView::OnInitialUpdate();
	
	CListCtrl &theList = GetListCtrl();

	theList.DeleteColumn( 0 );
	theList.DeleteColumn( 0 );
	
	theList.InsertColumn( 0, _T("Name"), LVCFMT_LEFT, 200 );
	theList.InsertColumn( 1, _T("Value"), LVCFMT_LEFT, 800 );

	theList.SetExtendedStyle( LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES );
}

void CVkeylistview::CalcWindowRect(LPRECT lpClientRect, UINT nAdjustType) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	CListView::CalcWindowRect(lpClientRect, nAdjustType);
}

BOOL CVkeylistview::PreCreateWindow(CREATESTRUCT& cs) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CListView::PreCreateWindow(cs);
}

BOOL CVkeylistview::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	dwStyle |= LVS_REPORT|LVS_SINGLESEL|LVS_EDITLABELS|LVS_AUTOARRANGE;
	
	return CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext);
}

void CVkeylistview::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	KeyValues *kv = (KeyValues *)pHint;

	if ( !kv || lHint != 2 )
		return;

	CListCtrl &theList = GetListCtrl();

	theList.DeleteAllItems();

	KeyValues *subkey = kv->GetFirstValue();

	LVITEM lvi;

	int i = 0;

	while ( subkey )
	{
		lvi.mask =  LVIF_TEXT;
		lvi.iItem = i;

		lvi.iSubItem = 0;
		lvi.pszText = (char*)subkey->GetName();
		theList.InsertItem(&lvi);

		lvi.iSubItem =1;
		lvi.pszText = (char*)subkey->GetString();
		theList.SetItem(&lvi);

		i++;

		subkey = subkey->GetNextValue();
	}
}

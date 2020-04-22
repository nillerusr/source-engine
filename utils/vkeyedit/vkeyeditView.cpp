//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// vkeyeditView.cpp : implementation of the CVkeyeditView class
//

#include "stdafx.h"
#include "vkeyedit.h"

#include "vkeyeditDoc.h"
#include "vkeyeditView.h"
#include <COMMCTRL.H>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CVkeyeditView

IMPLEMENT_DYNCREATE(CVkeyeditView, CTreeView)

BEGIN_MESSAGE_MAP(CVkeyeditView, CTreeView)
	//{{AFX_MSG_MAP(CVkeyeditView)
	ON_NOTIFY_REFLECT(TVN_SELCHANGED, OnSelchanged)
	//}}AFX_MSG_MAP
	// Standard printing commands
	ON_COMMAND(ID_FILE_PRINT, CTreeView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, CTreeView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, CTreeView::OnFilePrintPreview)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVkeyeditView construction/destruction

CVkeyeditView::CVkeyeditView()
{
	// TODO: add construction code here

}

CVkeyeditView::~CVkeyeditView()
{
}

BOOL CVkeyeditView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	cs.style |= TVS_HASLINES|TVS_EDITLABELS|TVS_HASBUTTONS|TVS_LINESATROOT;

	return CTreeView::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CVkeyeditView drawing

void CVkeyeditView::OnDraw(CDC* pDC)
{
	CVkeyeditDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	// TODO: add draw code for native data here
}

//DEL void CVkeyeditView::OnInitialUpdate()
//DEL {
//DEL 	CTreeView::OnInitialUpdate();
//DEL 
//DEL 	CTreeCtrl &tree = GetTreeCtrl();
//DEL 
//DEL //	tree.InsertItem( _T("Test") );
//DEL 
//DEL 
//DEL 	// TODO: You may populate your TreeView with items by directly accessing
//DEL 	//  its tree control through a call to GetTreeCtrl().
//DEL }

/////////////////////////////////////////////////////////////////////////////
// CVkeyeditView printing

BOOL CVkeyeditView::OnPreparePrinting(CPrintInfo* pInfo)
{
	// default preparation
	return DoPreparePrinting(pInfo);
}

void CVkeyeditView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: add extra initialization before printing
}

void CVkeyeditView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: add cleanup after printing
}

/////////////////////////////////////////////////////////////////////////////
// CVkeyeditView diagnostics

#ifdef _DEBUG
void CVkeyeditView::AssertValid() const
{
	CTreeView::AssertValid();
}

void CVkeyeditView::Dump(CDumpContext& dc) const
{
	CTreeView::Dump(dc);
}

CVkeyeditDoc* CVkeyeditView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CVkeyeditDoc)));
	return (CVkeyeditDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CVkeyeditView message handlers

void CVkeyeditView::CalcWindowRect(LPRECT lpClientRect, UINT nAdjustType) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	CTreeView::CalcWindowRect(lpClientRect, nAdjustType);
}

// Sort the item in reverse alphabetical order.
static int CALLBACK MyCompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
   return strcmp( ((KeyValues*)(lParam1))->GetName(), ((KeyValues*)(lParam2))->GetName()  );
}

void CVkeyeditView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	// TODO: Add your specialized code here and/or call the base class
	CTreeCtrl &theTree = GetTreeCtrl();

	KeyValues *kv = (KeyValues *)pHint;

	if ( !kv || lHint != 1 )
		return;

	theTree.DeleteAllItems();

	while ( kv )
	{
		InsertKeyValues( kv, TVI_ROOT );

		kv = kv->GetNextKey();
	}

   // The pointer to my tree control.
   TVSORTCB tvs;
   // Sort the tree control's items using my
   // callback procedure.
   tvs.hParent = TVI_ROOT;
   tvs.lpfnCompare = MyCompareProc;
   tvs.lParam = (LPARAM) &theTree;

   theTree.SortChildrenCB(&tvs);

}

bool CVkeyeditView::InsertKeyValues(KeyValues *kv, HTREEITEM hParent)
{
	CTreeCtrl &theTree = GetTreeCtrl();

	TVINSERTSTRUCT tvInsert;
	tvInsert.hParent = hParent;
	tvInsert.hInsertAfter = TVI_LAST;
	tvInsert.item.mask = TVIF_TEXT;
	tvInsert.item.lParam = (LPARAM)kv;
	tvInsert.item.pszText = (char*)kv->GetName();

	HTREEITEM hItem = theTree.InsertItem( &tvInsert );

	theTree.SetItemData(hItem, (DWORD) kv );

	KeyValues * subkey = kv->GetFirstTrueSubKey();

	while ( subkey )
	{
		InsertKeyValues( subkey, hItem );
		subkey = subkey->GetNextKey();
	}

	 // The pointer to my tree control.
   TVSORTCB tvs;
   // Sort the tree control's items using my
   // callback procedure.
   tvs.hParent = hParent;
   tvs.lpfnCompare = MyCompareProc;
   tvs.lParam = (LPARAM) &theTree;

   theTree.SortChildrenCB(&tvs);

	return true;
}

void CVkeyeditView::OnSelchanged(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_TREEVIEW *pNMTreeView = (NM_TREEVIEW*) pNMHDR;
    CTreeCtrl   &tTree = this->GetTreeCtrl ();
	
	 CTreeCtrl &theTree = this->GetTreeCtrl ();

	 HTREEITEM hItem = pNMTreeView->itemNew.hItem;

	GetDocument()->UpdateAllViews ( this, 2, (CObject*)theTree.GetItemData(hItem) );

    *pResult = 0;

}

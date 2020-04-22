//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// vkeyeditView.h : interface of the CVkeyeditView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_VKEYEDITVIEW_H__18F868AE_3796_409E_9C61_25660D4FB286__INCLUDED_)
#define AFX_VKEYEDITVIEW_H__18F868AE_3796_409E_9C61_25660D4FB286__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CVkeyeditDoc;
class KeyValues;

class CVkeyeditView : public CTreeView
{
protected: // create from serialization only
	CVkeyeditView();
	DECLARE_DYNCREATE(CVkeyeditView)

// Attributes
public:
	CVkeyeditDoc* GetDocument();

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVkeyeditView)
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	protected:
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void CalcWindowRect(LPRECT lpClientRect, UINT nAdjustType = adjustBorder);
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	//}}AFX_VIRTUAL

// Implementation
public:
	bool InsertKeyValues(KeyValues *kv, HTREEITEM hParent);
	virtual ~CVkeyeditView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CVkeyeditView)
	afx_msg void OnSelchanged(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in vkeyeditView.cpp
inline CVkeyeditDoc* CVkeyeditView::GetDocument()
   { return (CVkeyeditDoc*)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_VKEYEDITVIEW_H__18F868AE_3796_409E_9C61_25660D4FB286__INCLUDED_)

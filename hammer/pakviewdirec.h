//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// PakViewDirec.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CPakViewDirec view

#include <afxcview.h>

class CPakViewDirec : public CTreeView
{
protected:
	CPakViewDirec();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CPakViewDirec)

// Attributes
public:
	CPakDoc* GetDocument()
	{ return (CPakDoc*)m_pDocument; }

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPakViewDirec)
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CPakViewDirec();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CPakViewDirec)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

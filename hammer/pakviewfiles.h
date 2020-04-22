//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// PakViewFilesFiles.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CPakViewFiles view

#include <afxcview.h>		// MFC clistview

class CPakViewFiles : public CListView
{
protected:
	CPakViewFiles();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CPakViewFiles)

	enum
	{
		colName,
		colSize,
		colType
	};

// Attributes
public:
	CPakDoc* GetDocument()
	{ return (CPakDoc*)m_pDocument; }

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPakViewFiles)
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CPakViewFiles();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CPakViewFiles)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

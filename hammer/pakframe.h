//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// PakFrame.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CPakFrame frame

class CPakFrame : public CMDIChildWnd
{
	DECLARE_DYNCREATE(CPakFrame)
protected:
	CPakFrame();           // protected constructor used by dynamic creation

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPakFrame)
	protected:
	virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
	//}}AFX_VIRTUAL

// Implementation
protected:
	CSplitterWnd SplitWnd;
	virtual ~CPakFrame();

	// Generated message map functions
	//{{AFX_MSG(CPakFrame)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

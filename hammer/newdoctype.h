//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined(AFX_NEWDOCTYPE_H__FACD93D9_0A9D_11D1_AFAA_0060979D2F4E__INCLUDED_)
#define AFX_NEWDOCTYPE_H__FACD93D9_0A9D_11D1_AFAA_0060979D2F4E__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// NewDocType.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CNewDocType dialog

class CNewDocType : public CDialog
{
// Construction
public:
	CNewDocType(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CNewDocType)
	enum { IDD = IDD_NEWDOCTYPE };
	int		m_iNewType;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNewDocType)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CNewDocType)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NEWDOCTYPE_H__FACD93D9_0A9D_11D1_AFAA_0060979D2F4E__INCLUDED_)

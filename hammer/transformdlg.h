//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined(AFX_TRANSFORMDLG_H__B21B769F_F316_11D0_AFA6_0060979D2F4E__INCLUDED_)
#define AFX_TRANSFORMDLG_H__B21B769F_F316_11D0_AFA6_0060979D2F4E__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// TransformDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CTransformDlg dialog

class CTransformDlg : public CDialog
{
// Construction
public:
	CTransformDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CTransformDlg)
	enum { IDD = IDD_TRANSFORM };
	int		m_iMode;
	float	m_X;
	float	m_Y;
	float	m_Z;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTransformDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CTransformDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TRANSFORMDLG_H__B21B769F_F316_11D0_AFA6_0060979D2F4E__INCLUDED_)

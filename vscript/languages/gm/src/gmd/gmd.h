// gmd.h : main header file for the GMD application
//
//  See Copyright Notice in gmMachine.h

#if !defined(AFX_GMD_H__F4DE5F2C_627C_41AD_BBDC_F26D7E07343D__INCLUDED_)
#define AFX_GMD_H__F4DE5F2C_627C_41AD_BBDC_F26D7E07343D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CGmdApp:
// See gmd.cpp for the implementation of this class
//

class CGmdApp : public CWinApp
{
public:
	CGmdApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGmdApp)
	public:
	virtual BOOL InitInstance();
	virtual BOOL OnIdle(LONG lCount);
	//}}AFX_VIRTUAL

// Implementation

public:
	//{{AFX_MSG(CGmdApp)
	afx_msg void OnAppAbout();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GMD_H__F4DE5F2C_627C_41AD_BBDC_F26D7E07343D__INCLUDED_)

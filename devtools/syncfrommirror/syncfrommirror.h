//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// syncfrommirror.h : main header file for the SYNCFROMMIRROR application
//

#if !defined(AFX_SYNCFROMMIRROR_H__1E25CDDD_8382_4580_81AC_CFB68FECBE4A__INCLUDED_)
#define AFX_SYNCFROMMIRROR_H__1E25CDDD_8382_4580_81AC_CFB68FECBE4A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CSyncfrommirrorApp:
// See syncfrommirror.cpp for the implementation of this class
//

class CSyncfrommirrorApp : public CWinApp
{
public:
	CSyncfrommirrorApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSyncfrommirrorApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CSyncfrommirrorApp)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SYNCFROMMIRROR_H__1E25CDDD_8382_4580_81AC_CFB68FECBE4A__INCLUDED_)

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// vkeyedit.h : main header file for the VKEYEDIT application
//

#if !defined(AFX_VKEYEDIT_H__E87E87D1_270E_4DB9_92BB_C4FA27B4369C__INCLUDED_)
#define AFX_VKEYEDIT_H__E87E87D1_270E_4DB9_92BB_C4FA27B4369C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols
#include <filesystem.h>

/////////////////////////////////////////////////////////////////////////////
// CVkeyeditApp:
// See vkeyedit.cpp for the implementation of this class
//

extern IFileSystem *g_pFileSystem;

class CVkeyeditApp : public CWinApp
{
public:
	CVkeyeditApp();
	~CVkeyeditApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVkeyeditApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

// Implementation
	//{{AFX_MSG(CVkeyeditApp)
	afx_msg void OnAppAbout();
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_VKEYEDIT_H__E87E87D1_270E_4DB9_92BB_C4FA27B4369C__INCLUDED_)

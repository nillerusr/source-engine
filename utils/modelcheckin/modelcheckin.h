//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// ModelCheckIn.h : main header file for the MODELCHECKIN application
//

#if !defined(AFX_MODELCHECKIN_H__05EE4104_1C0C_46E2_B5A2_C005630DCD19__INCLUDED_)
#define AFX_MODELCHECKIN_H__05EE4104_1C0C_46E2_B5A2_C005630DCD19__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CModelCheckInApp:
// See ModelCheckIn.cpp for the implementation of this class
//

class CModelCheckInApp : public CWinApp
{
public:
	CModelCheckInApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CModelCheckInApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CModelCheckInApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MODELCHECKIN_H__05EE4104_1C0C_46E2_B5A2_C005630DCD19__INCLUDED_)

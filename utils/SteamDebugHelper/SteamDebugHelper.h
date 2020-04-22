//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// SteamDebugHelper.h : main header file for the STEAMDEBUGHELPER application
//

#if !defined(AFX_STEAMDEBUGHELPER_H__C6F4FF3C_B13B_4294_A881_57BC35FC16AF__INCLUDED_)
#define AFX_STEAMDEBUGHELPER_H__C6F4FF3C_B13B_4294_A881_57BC35FC16AF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CSteamDebugHelperApp:
// See SteamDebugHelper.cpp for the implementation of this class
//

class CSteamDebugHelperApp : public CWinApp
{
public:
	CSteamDebugHelperApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSteamDebugHelperApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CSteamDebugHelperApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STEAMDEBUGHELPER_H__C6F4FF3C_B13B_4294_A881_57BC35FC16AF__INCLUDED_)

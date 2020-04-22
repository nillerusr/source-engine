//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// SteamFileDialog.h : main header file for the STEAMFILEDIALOG DLL
//

#if !defined(AFX_STEAMFILEDIALOG_H__BF4B825D_4E34_443E_84D2_6212C043388D__INCLUDED_)
#define AFX_STEAMFILEDIALOG_H__BF4B825D_4E34_443E_84D2_6212C043388D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CSteamFileDialogApp
// See SteamFileDialog.cpp for the implementation of this class
//

class CSteamFileDialogApp : public CWinApp
{
public:
	CSteamFileDialogApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSteamFileDialogApp)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CSteamFileDialogApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STEAMFILEDIALOG_H__BF4B825D_4E34_443E_84D2_6212C043388D__INCLUDED_)

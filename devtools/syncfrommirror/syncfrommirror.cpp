//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// syncfrommirror.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "syncfrommirror.h"
#include "syncfrommirrorDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSyncfrommirrorApp

BEGIN_MESSAGE_MAP(CSyncfrommirrorApp, CWinApp)
	//{{AFX_MSG_MAP(CSyncfrommirrorApp)
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSyncfrommirrorApp construction

CSyncfrommirrorApp::CSyncfrommirrorApp()
{
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CSyncfrommirrorApp object

CSyncfrommirrorApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CSyncfrommirrorApp initialization

BOOL CSyncfrommirrorApp::InitInstance()
{
	// Standard initialization

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif
	AfxInitRichEdit();
	SetRegistryKey( "Valve" );

	CSyncfrommirrorDlg dlg;
	m_pMainWnd = &dlg;
	int nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
	}
	else if (nResponse == IDCANCEL)
	{
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

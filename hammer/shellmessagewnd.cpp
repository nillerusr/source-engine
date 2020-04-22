//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Receives shell commands from other applications and forwards them
//			to the shell command handler.
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "Shell.h"
#include "ShellMessageWnd.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


static const char *g_pszClassName = "Worldcraft_ShellMessageWnd";


BEGIN_MESSAGE_MAP(CShellMessageWnd, CWnd)
	//{{AFX_MSG_MAP(CShellMessageWnd)
	ON_WM_COPYDATA()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Creates the hidden shell message window.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CShellMessageWnd::Create(void)
{
	WNDCLASS wndcls;
	memset(&wndcls, 0, sizeof(WNDCLASS));
    wndcls.style         = 0;
    wndcls.lpfnWndProc   = AfxWndProc;
    wndcls.hInstance     = AfxGetInstanceHandle();
    wndcls.hIcon         = NULL;
    wndcls.hCursor       = NULL;
    wndcls.hbrBackground = NULL;
    wndcls.lpszMenuName  = NULL;
	wndcls.cbWndExtra    = 0;
    wndcls.lpszClassName = g_pszClassName;

	if (!AfxRegisterClass(&wndcls))
	{
		AfxMessageBox("Could not register the Hammer shell message window class.");
		return(false);
	}

	return(CWnd::CreateEx(0, g_pszClassName, g_pszClassName, 0, CRect(0, 0, 10, 10), NULL, 0) == TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Attaches a shell command handler to this message window. All commands
//			received by this message window will be sent to the command handler.
// Input  : pShell - Shell command handler. NULL disables command processing.
//-----------------------------------------------------------------------------
void CShellMessageWnd::SetShell(CShell *pShell)
{
	Assert(pShell != NULL);
	m_pShell = pShell;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the WM_COPYDATA message containing the shell command from
//			another application.
// Input  : pWnd - Temporary CWnd object of the sending window.
//			pCopyData - Copy data struct with shell command in the lpData field.
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CShellMessageWnd::OnCopyData(CWnd *pWnd, COPYDATASTRUCT *pCopyData)
{
	if (m_pShell != NULL)
	{
		if (pCopyData->lpData != NULL)
		{
			return(m_pShell->RunCommand((const char *)pCopyData->lpData) ? TRUE : FALSE);
		}
	}

	return(FALSE);
}


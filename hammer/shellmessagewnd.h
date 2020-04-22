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

#ifndef SHELLMESSAGEWND_H
#define SHELLMESSAGEWND_H
#pragma once


class CShell;


class CShellMessageWnd : public CWnd
{
	public:

		bool Create(void);
		void SetShell(CShell *pShell);

	protected:

		CShell *m_pShell;

		//{{AFX_MSG_MAP(CShellMessageWnd)
		afx_msg BOOL OnCopyData(CWnd *pWnd, COPYDATASTRUCT *pCopyData);
		//}}AFX_MSG
	
		DECLARE_MESSAGE_MAP()
};


#endif // SHELLMESSAGEWND_H

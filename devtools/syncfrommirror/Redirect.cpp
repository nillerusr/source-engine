//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//------------------------------------------------------------------------------
// Redirect.cpp : implementation file
//
// Creates a child process that runs a user-specified command and redirects its
// standard output and standard error to a CEdit control.
//
// Written by Matt Brunk (brunk@gorge.net)
// Copyright (C) 1999 Matt Brunk
// All rights reserved.
//
// This code may be used in compiled form in any way. This file may be
// distributed by any means providing it is not sold for profit without
// the written consent of the author, and providing that this notice and the
// author's name is included in the distribution. If the compiled form of the
// source code in this file is used in a commercial application, an e-mail to
// the author would be appreciated.
//
// Thanks to Dima Shamroni (dima@abirnet.co.il) for providing the essential
// code for this class.
//
// Thanks to Chris Maunder (chrismaunder@codeguru.com) for the PeekAndPump()
// function (from his CProgressWnd class).
//
// Initial Release Feb 8, 1999
//------------------------------------------------------------------------------


#include "stdafx.h"
#include <string.h>
#include <string>
#include "Redirect.h"

const int BUF_SIZE = 8192;

CRedirect::~CRedirect()
{
}

void CRedirect::Run( LPCTSTR szCommand, CEdit *pEdit, LPCTSTR pszCurrentDirectory )
{
	m_pEdit = pEdit;
	m_bStopped = false;

	if ( !Open( szCommand, pszCurrentDirectory ) )
	{
		AppendText("\nFAILED TO SPAWN PROCESS\n");
		return;
	}

	for (;;)
	{ 
		//------------------------------------------------------------------
		//	If the child process has completed, break out.
		//------------------------------------------------------------------
		if ( !m_hThread || MsgWaitForMultipleObjects( 1, &m_hThread, FALSE, 20, QS_ALLINPUT) == WAIT_OBJECT_0 )
		{
			break;
		}

		//------------------------------------------------------------------
		//	Peek and pump messages.
		//------------------------------------------------------------------
		PeekAndPump();

		//------------------------------------------------------------------
		//	If the user cancelled the operation, terminate the process.
		//------------------------------------------------------------------
		if ( m_bStopped )
		{
			Close();
		}
	}
}


void CRedirect::PeekAndPump()
{
    MSG Msg;
    while (::PeekMessage(&Msg, NULL, 0, 0, PM_NOREMOVE)) 
    {
        (void)AfxGetApp()->PumpMessage(); //lint !e1924 (warning about C-style cast)
    }
}

void CRedirect::Stop()
{
	m_bStopped = true;
}

void CRedirect::AppendText(LPCTSTR Text)
{
	char *pszCurrent = const_cast<char *>(Text);
	int Length;

	Length = m_pEdit->GetWindowTextLength();
	if ( Length + strlen(Text) > m_pEdit->GetLimitText() )
		m_pEdit->SetLimitText( m_pEdit->GetLimitText() * 2 );

	if ( *pszCurrent == '\n' )
		pszCurrent++;

	while ( pszCurrent && *pszCurrent )
	{
		char *pszLineEnd = strchr( pszCurrent, '\r' );
		if ( pszLineEnd )
			*pszLineEnd = 0;

		Length = m_pEdit->GetWindowTextLength();
		m_pEdit->SetSel(Length, Length);
		m_pEdit->ReplaceSel(pszCurrent);
		//OutputDebugString(pszCurrent);

		if ( pszLineEnd )
		{
			Length = m_pEdit->GetWindowTextLength();
			m_pEdit->SetSel(Length, Length);
			m_pEdit->ReplaceSel("\r\n");
			//OutputDebugString("\r\n");
		}

		m_pEdit->LineScroll( m_pEdit->GetLineCount() );
		PeekAndPump();

		if ( pszLineEnd )
		{
			*pszLineEnd = '\r';
			pszCurrent = pszLineEnd + 1;
			if ( *pszCurrent == '\n' )
				pszCurrent++;
		}
		else
			pszCurrent = NULL;
	};
}


void CRedirect::WriteStdOut(LPCSTR pszOutput)
{
	AppendText( pszOutput );
}

void CRedirect::WriteStdError(LPCSTR pszError)
{
	AppendText( CString(pszError) );
}

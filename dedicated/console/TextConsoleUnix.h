//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// TextConsoleUnix.h: Unix interface for the TextConsole class.
//
//////////////////////////////////////////////////////////////////////

#if !defined TEXTCONSOLE_UNIX_H
#define TEXTCONSOLE_UNIX_H

#ifndef _WIN32

#include <stdio.h>
#include "textconsole.h"

class CTextConsoleUnix : public CTextConsole
{
public:
	virtual ~CTextConsoleUnix() { }

	// CTextConsole
	bool		Init();
	void		ShutDown();
	void		Print( char * pszMsg );

	void		SetTitle( char *pszTitle );
	void		SetStatusLine( char *pszStatus );
	void		UpdateStatus();

	char *		GetLine( int index, char *buf, int buflen );
	int			GetWidth();

private:
	bool m_bConDebug;
	FILE *m_tty;
};

#endif // _ndef WIN32

#endif // !defined TEXTCONSOLE_UNIX_H

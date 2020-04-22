//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#if !defined TEXTCONSOLE_H
#define TEXTCONSOLE_H
#pragma once

class CTextConsole
{
public:
	CTextConsole() : m_ConsoleVisible( true ) {}
	virtual ~CTextConsole() {};

	virtual bool		Init ();
	virtual void		ShutDown() = 0;
	virtual void		Print( char * pszMsg ) = 0;

	virtual void		SetTitle( char * pszTitle ) = 0;
	virtual void		SetStatusLine( char * pszStatus ) = 0;
	virtual void		UpdateStatus() = 0;

	// Must be provided by children
	virtual char *		GetLine( int index, char *buf, int buflen ) = 0;
	virtual int			GetWidth() = 0;

	virtual void		SetVisible( bool visible ) { m_ConsoleVisible = visible; }
	virtual bool		IsVisible() { return m_ConsoleVisible; }

protected:
	bool m_ConsoleVisible;
};

#endif // !defined TEXTCONSOLE_H

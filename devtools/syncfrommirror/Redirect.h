//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "redir.h"

#ifndef REDIRECT_H_INCLUDED__
#define REDIRECT_H_INCLUDED__

class CRedirect : public CRedirector
{
public:

	//--------------------------------------------------------------------------
	//	constructor
	//--------------------------------------------------------------------------
	CRedirect()
	 :	m_pEdit(NULL),
		m_bStopped(false)
	{
	}

	//--------------------------------------------------------------------------
	//	destructor
	//--------------------------------------------------------------------------
	virtual ~CRedirect();
	//--------------------------------------------------------------------------
	//	public member functions
	//--------------------------------------------------------------------------
	virtual void		Run(LPCTSTR szCommand, CEdit *pEdit, LPCTSTR pszCurrentDirectory = NULL);
	virtual	void		Stop();

protected:

	//--------------------------------------------------------------------------
	//	member functions
	//--------------------------------------------------------------------------
	virtual void WriteStdOut(LPCSTR pszOutput);
	virtual void WriteStdError(LPCSTR pszError);

	void				AppendText(LPCTSTR Text);
	void				PeekAndPump();

	//--------------------------------------------------------------------------
	//	member data
	//--------------------------------------------------------------------------
	CEdit *	m_pEdit;
	bool	m_bStopped;

};

#endif	// REDIRECT_H_INCLUDED__

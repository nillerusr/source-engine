//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H
#ifdef _WIN32
#pragma once
#endif

class IProgressDialog
{
public:

	virtual void Start( char const *pchTitle, char const *pchText, bool bShowCancel ) = 0;
	virtual void Update( float flZeroToOneFraction ) = 0;
	virtual void UpdateText( char const *pchFmt, ... ) = 0;
	virtual bool IsCancelled() = 0;
	virtual void Finish() = 0;
};

extern IProgressDialog *g_pProgressDialog;

#endif // PROGRESSDIALOG_H

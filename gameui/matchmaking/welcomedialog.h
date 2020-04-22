//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Matchmaking's "main menu"
//
//=============================================================================//

#ifndef WELCOMEDIALOG_H
#define WELCOMEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialog.h"

//-----------------------------------------------------------------------------
// Purpose: Main matchmaking menu
//-----------------------------------------------------------------------------
class CWelcomeDialog : public CBaseDialog
{
	DECLARE_CLASS_SIMPLE( CWelcomeDialog, CBaseDialog ); 

public:
	CWelcomeDialog(vgui::Panel *parent);

	virtual void	PerformLayout( void );
	virtual void	OnCommand( const char *pCommand );
	virtual void	OnKeyCodePressed( vgui::KeyCode code );

private:
	bool	m_bOnlineEnabled;
};


#endif // WELCOMEDIALOG_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef LOADGAMEDIALOG_H
#define LOADGAMEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "BaseSaveGameDialog.h"
#include "SaveGameDialog.h"
#include "SaveGameBrowserDialog.h"
#include "BasePanel.h"
#include "vgui_controls/KeyRepeat.h"

//-----------------------------------------------------------------------------
// Purpose: Displays game loading options
//-----------------------------------------------------------------------------
class CLoadGameDialog : public CBaseSaveGameDialog
{
	DECLARE_CLASS_SIMPLE( CLoadGameDialog, CBaseSaveGameDialog );

public:
	CLoadGameDialog(vgui::Panel *parent);
	~CLoadGameDialog();

	virtual void OnCommand( const char *command );
};

//
//
//

class CLoadGameDialogXbox : public CSaveGameBrowserDialog
{
	DECLARE_CLASS_SIMPLE( CLoadGameDialogXbox, CSaveGameBrowserDialog );

public:
					CLoadGameDialogXbox( vgui::Panel *parent );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	OnCommand(const char *command);
	virtual void	PerformSelectedAction( void );
	virtual void	PerformDeletion( void );
	virtual void	UpdateFooterOptions( void );

private:
	void			DeleteSaveGame( const SaveGameDescription_t *pSaveDesc );
};

#endif // LOADGAMEDIALOG_H

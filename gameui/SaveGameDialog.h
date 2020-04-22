//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SAVEGAMEDIALOG_H
#define SAVEGAMEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "BaseSaveGameDialog.h"
#include "SaveGameBrowserDialog.h"
#include "vgui_controls/KeyRepeat.h"

//-----------------------------------------------------------------------------
// Purpose: Save game dialog
//-----------------------------------------------------------------------------
class CSaveGameDialog : public CBaseSaveGameDialog
{
	DECLARE_CLASS_SIMPLE( CSaveGameDialog, CBaseSaveGameDialog );

public:
	CSaveGameDialog( vgui::Panel *parent );
	~CSaveGameDialog();

	virtual void Activate();
	static void FindSaveSlot( char *buffer, int bufsize );

protected:
	virtual void OnCommand( const char *command );
	virtual void OnScanningSaveGames();
};

#define SAVE_NUM_ITEMS 4

//
//
//
class CAsyncCtxSaveGame;

class CSaveGameDialogXbox : public CSaveGameBrowserDialog
{
	DECLARE_CLASS_SIMPLE( CSaveGameDialogXbox, CSaveGameBrowserDialog );

public:
					CSaveGameDialogXbox( vgui::Panel *parent );
	virtual void	PerformSelectedAction( void );
	virtual void	UpdateFooterOptions( void );
	virtual void	OnCommand( const char *command );
	virtual void	OnDoneScanningSaveGames( void );

private:
	friend class CAsyncCtxSaveGame;
	void			InitiateSaving();
	void			SaveCompleted( CAsyncCtxSaveGame *pCtx );

private:
	bool					m_bGameSaving;
	bool					m_bNewSaveAvailable;
	SaveGameDescription_t	m_NewSaveDesc;
};

#endif // SAVEGAMEDIALOG_H

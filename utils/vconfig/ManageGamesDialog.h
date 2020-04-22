//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef MANAGEGAMESDIALOG_H
#define MANAGEGAMESDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/PHandle.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Main dialog for media browser
//-----------------------------------------------------------------------------
class CManageGamesDialog : public Frame
{
	DECLARE_CLASS_SIMPLE( CManageGamesDialog, Frame );

public:

	CManageGamesDialog( Panel *parent, const char *name, int configID );
	virtual ~CManageGamesDialog();

	void	SetGameDir( const char *szDir );
	void	SetGameName( const char *szDir );

protected:
	
	virtual void OnCommand( const char *command );
	
	bool	IsGameNameUnique( const char *name );

private:

	int					m_nConfigID;
	
	vgui::TextEntry		*m_pGameNameEntry;
	vgui::TextEntry		*m_pGameDirEntry;

	MESSAGE_FUNC_CHARPTR( OnChooseDirectory, "DirectorySelected", dir );
};


extern CManageGamesDialog *g_pManageGamesDialog;


#endif // MANAGEGAMESDIALOG_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Standard file menu
//
//=============================================================================


#ifndef TOOLFILEMENUBUTTON_H
#define TOOLFILEMENUBUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include "toolutils/toolmenubutton.h"


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
class Panel;
class Menu;
}

class CToolMenuButton;


//-----------------------------------------------------------------------------
// Called back by the file menu 
//-----------------------------------------------------------------------------
class IFileMenuCallbacks
{
public:
	enum MenuItems_t
	{
		FILE_NEW	= 0x01,
		FILE_OPEN	= 0x02,
		FILE_SAVE	= 0x04,
		FILE_SAVEAS = 0x08,
		FILE_CLOSE	= 0x10,
		FILE_RECENT	= 0x20,
		FILE_CLEAR_RECENT	= 0x40,
		FILE_EXIT	= 0x80,

		FILE_ALL = 0xFFFFFFFF
	};

	// Logically OR together all items that should be enabled
	virtual int	 GetFileMenuItemsEnabled( ) = 0;

	// Add recent files to the menu passed in
	virtual void AddRecentFilesToMenu( vgui::Menu *menu ) = 0;

	// Get the perforce file name (to set the various perforce menu options)
	virtual bool GetPerforceFileName( char *pFileName, int nMaxLen ) = 0;

	// Gets the root vgui panel
	virtual vgui::Panel *GetRootPanel() = 0;
};


//-----------------------------------------------------------------------------
// Standard file menu
//-----------------------------------------------------------------------------
class CToolFileMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CToolFileMenuButton, CToolMenuButton );
public:

	CToolFileMenuButton( vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionTarget, IFileMenuCallbacks *pFileMenuCallback );
	virtual void OnShowMenu( vgui::Menu *menu );

private:
	MESSAGE_FUNC( OnPerforceAdd, "OnPerforceAdd" );
	MESSAGE_FUNC( OnPerforceOpen, "OnPerforceOpen" );
	MESSAGE_FUNC( OnPerforceRevert, "OnPerforceRevert" );
	MESSAGE_FUNC( OnPerforceSubmit, "OnPerforceSubmit" );
	MESSAGE_FUNC( OnPerforceP4Win, "OnPerforceP4Win" );
	MESSAGE_FUNC( OnPerforceListOpenFiles, "OnPerforceListOpenFiles" );

	vgui::Menu			*m_pRecentFiles;
	vgui::Menu			*m_pPerforce;
	int					m_nRecentFiles;
	IFileMenuCallbacks *m_pFileMenuCallback;
	int					m_nPerforceAdd;
	int					m_nPerforceOpen;
	int					m_nPerforceRevert;
	int					m_nPerforceSubmit;
	int					m_nPerforceP4Win;
	int					m_nPerforceListOpenFiles;
};


//-----------------------------------------------------------------------------
// Global function to create the switch menu
//-----------------------------------------------------------------------------
CToolMenuButton* CreateToolFileMenuButton( vgui::Panel *parent, const char *panelName, 
	const char *text, vgui::Panel *pActionTarget, IFileMenuCallbacks *pCallbacks );


#endif // TOOLFILEMENUBUTTON_H


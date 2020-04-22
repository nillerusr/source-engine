//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MEDIABROWSERDIALOG_H
#define MEDIABROWSERDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/ImageList.h>
#include <vgui_controls/SectionedListPanel.h>
#include <vgui_controls/PHandle.h>
#include <FileSystem.h>
#include "vgui/mousecode.h"
#include "vgui/IScheme.h"
#include "configs.h"

//-----------------------------------------------------------------------------
// Purpose: Main dialog for media browser
//-----------------------------------------------------------------------------
class CSDKLauncherDialog : public vgui::Frame
{
	typedef vgui::Frame BaseClass;

public:
	DECLARE_CLASS_SIMPLE( CSDKLauncherDialog, vgui::Frame );

	CSDKLauncherDialog(vgui::Panel *parent, const char *name);
	virtual ~CSDKLauncherDialog();

	void PopulateCurrentGameCombo( bool bSelectLast );
	void PopulateCurrentEngineCombo( bool bSelectLast );
	void Launch( int hActiveListItem, bool bForce );
	void RefreshConfigs( void );
	void SetCurrentGame( const char* pcCurrentGame );

protected:

	virtual void OnClose();
	virtual void OnCommand( const char *command );

private:

	void ResetConfigs( void );
	bool ParseConfigs( CUtlVector<CGameConfig*> &configs );
	void PopulateMediaList();
	void GetEngineVersion(char* pcEngineVer, int nSize);
	void SetEngineVersion(const char *pcEngineVer);

	MESSAGE_FUNC_INT( OnItemDoubleLeftClick, "ItemDoubleLeftClick", itemID );
	MESSAGE_FUNC_INT( OnItemContextMenu, "ItemContextMenu", itemID );
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", pkv );

private:

	vgui::ImageList *m_pImageList;
	vgui::SectionedListPanel *m_pMediaList;
	vgui::Menu *m_pContextMenu;
	vgui::ComboBox *m_pCurrentGameCombo;
	vgui::ComboBox *m_pCurrentEngineCombo;
};


extern CSDKLauncherDialog *g_pSDKLauncherDialog;


#endif // MEDIABROWSERDIALOG_H

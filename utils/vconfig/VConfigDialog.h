//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef VCONFIGDIALOG_H
#define VCONFIGDIALOG_H
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

#include "ConfigManager.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Main dialog for media browser
//-----------------------------------------------------------------------------
class CVConfigDialog : public Frame
{
	DECLARE_CLASS_SIMPLE( CVConfigDialog, Frame );

public:

	CVConfigDialog(Panel *parent, const char *name);
	virtual ~CVConfigDialog();

	void		PopulateConfigList( bool bSelectActiveConfig = true );

protected:
	
	virtual void OnClose();
	virtual void OnCommand( const char *command );

private:

	void		SetGlobalConfig( const char *modDir );

	vgui::ComboBox	*m_pConfigCombo;
	bool			m_bChanged;

	MESSAGE_FUNC( OnManageSelect,	"ManageSelect" );
	MESSAGE_FUNC( OnAddSelect,		"AddSelect" );
};


extern CVConfigDialog *g_pVConfigDialog;


#endif // VCONFIGDIALOG_H

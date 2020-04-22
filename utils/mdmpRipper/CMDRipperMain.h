//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MDRIPPERMAIN_H
#define MDRIPPERMAIN_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/ImageList.h>
#include <vgui_controls/SectionedListPanel.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/MenuBar.h>
#include <FileSystem.h>
#include "vgui/mousecode.h"
#include "vgui/IScheme.h"
#include "CMDErrorPanel.h"
#include "CMDModulePanel.h"
#include "CMDDetailPanel.h"
#include "isqlwrapper.h"

using namespace vgui;


void VGUIMessageBox( vgui::Panel *pParent, const char *pTitle, PRINTF_FORMAT_STRING const char *pMsg, ... );

//-----------------------------------------------------------------------------
// Purpose: Main dialog for media browser
//-----------------------------------------------------------------------------
class CMDRipperMain : public Frame
{
	DECLARE_CLASS_SIMPLE( CMDRipperMain, Frame );

public:

	CMDRipperMain(Panel *parent, const char *name);
	virtual ~CMDRipperMain();
	ISQLWrapper *GetSqlWrapper() { return sqlWrapper; }
	
protected:
	
	virtual void OnClose();
	virtual void OnCommand( const char *command );
	virtual bool RequestInfo( KeyValues *outputData );

private:

	void		SetGlobalConfig( const char *modDir );

	vgui::ComboBox	*m_pConfigCombo;
	bool			m_bChanged;
	vgui::MenuBar	*m_pMenuBar;
	vgui::Panel		*m_pClientArea;
	CMDErrorPanel	*m_pErrorPanel;
	CMDModulePanel	*m_pModulePanel;
	CMDDetailPanel	*m_pDetailPanel;

	MESSAGE_FUNC( OnOpen, "Open" );
	MESSAGE_FUNC( OnError, "Error" );
	MESSAGE_FUNC( OnRefresh, "refresh" );
	MESSAGE_FUNC_PARAMS( OnDetail, "detail", data );	
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );	
	MESSAGE_FUNC_PARAMS( OnLookUp, "ModuleLookUp", url );
	MESSAGE_FUNC_PARAMS( OnDragDrop, "DragDrop", pData );

	CSysModule	*hSQLWrapper;
	ISQLWrapperFactory	*sqlWrapperFactory;
	ISQLWrapper *sqlWrapper;
		

//	MESSAGE_FUNC( OnManageSelect,	"ManageSelect" );
//	MESSAGE_FUNC( OnAddSelect,		"AddSelect" );
};


extern CMDRipperMain *g_pCMDRipperMain;


#endif // MDRIPPERMAIN_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef FILELISTMANAGER_H
#define FILELISTMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include "datamodel/idatamodel.h"
#include "vgui_controls/listpanel.h"
#include "vgui_controls/Frame.h"
#include "vgui/KeyCode.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class CheckButtonList;
}

//-----------------------------------------------------------------------------
// CFileListManager 
//-----------------------------------------------------------------------------
class CFileListManager : public vgui::ListPanel
{
	DECLARE_CLASS_SIMPLE( CFileListManager , vgui::ListPanel );

public:
	CFileListManager( vgui::Panel *parent );

	virtual void Refresh();
	virtual void OnCommand( const char *cmd );
	virtual void OnThink();
	virtual void OnMousePressed( vgui::MouseCode code );

protected:
	MESSAGE_FUNC_PARAMS( OnOpenContextMenu, "OpenContextMenu", pParams );
	MESSAGE_FUNC_PARAMS( OnOpenFile, "open", pParams );
	MESSAGE_FUNC_PARAMS( OnLoadFiles, "load", pParams );
	MESSAGE_FUNC_PARAMS( OnUnloadFiles, "unload", pParams );
	MESSAGE_FUNC_PARAMS( OnSaveFiles, "save", pParams );
	MESSAGE_FUNC_PARAMS( OnSaveFileAs, "saveas", pParams );
	MESSAGE_FUNC_PARAMS( OnAddToPerforce, "p4add", pParams );
	MESSAGE_FUNC_PARAMS( OnOpenForEdit, "p4edit", pParams );
	MESSAGE_FUNC_PARAMS( OnFileSelected, "FileSelected", pParams );
	MESSAGE_FUNC_PARAMS( OnDataChanged, "DataChanged", pParams );

	int AddItem( DmFileId_t fileid, const char *pFilename, const char *pPath, bool bLoaded, int nElements, bool bChanged, bool bInPerforce, bool bOpenForEdit );
	void SetLoaded( DmFileId_t fileid, bool bLoaded );

	vgui::CheckButtonList *m_pFileList;
	bool m_bRefreshRequired;

	vgui::DHANDLE< vgui::Menu >	m_hContextMenu;
};


//-----------------------------------------------------------------------------
// CFileListManagerFrame 
//-----------------------------------------------------------------------------
class CFileManagerFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CFileManagerFrame, vgui::Frame );

public:
	CFileManagerFrame( vgui::Panel *parent );

	virtual void Refresh();
	virtual void OnCommand( const char *cmd );
	virtual void PerformLayout();

protected:
	CFileListManager *m_pFileListManager;
};

#endif // FILELISTMANAGER_H

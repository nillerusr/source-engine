//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef VP4DIALOG_H
#define VP4DIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "vgui_controls/ImageList.h"

//-----------------------------------------------------------------------------
// Purpose: Main app window
//-----------------------------------------------------------------------------
class CVP4Dialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CVP4Dialog, vgui::Frame );
public:
	CVP4Dialog();
	~CVP4Dialog();

	// overridden frame functions
	virtual void Activate();
	virtual void OnClose();

private:
	void RefreshFileList();
	void RefreshClientList();
	void RefreshChangesList();

	MESSAGE_FUNC( OnFileSelected, "TreeViewItemSelected" );
	MESSAGE_FUNC( OnTextChanged, "TextChanged" );

	// changes
	MESSAGE_FUNC_INT( CloakFolder, "CloakFolder", item );
	MESSAGE_FUNC_INT( OpenFileForEdit, "EditFile", item );
	MESSAGE_FUNC_INT( OpenFileForDelete, "DeleteFile", item );

	vgui::ComboBox *m_pClientCombo;
	vgui::TreeView *m_pFileTree;
	vgui::ImageList m_Images;

	vgui::PropertySheet *m_pViewsSheet;
	vgui::PropertyPage *m_pRevisionsPage;
	vgui::PropertyPage *m_pChangesPage;

	vgui::ListPanel *m_pRevisionList;
	vgui::SectionedListPanel *m_pChangesList;

};


#endif // VP4DIALOG_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef DMESOURCESKINPANEL_H
#define DMESOURCESKINPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"
#include "datamodel/dmehandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class TextEntry;
	class CheckButton;
}

class CDmeSourceSkin;


//-----------------------------------------------------------------------------
// Purpose: Asset builder
//-----------------------------------------------------------------------------
class CDmeSourceSkinPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CDmeSourceSkinPanel, EditablePanel );

public:
	CDmeSourceSkinPanel( vgui::Panel *pParent, const char *pPanelName );
	virtual ~CDmeSourceSkinPanel();

	void SetDmeElement( CDmeSourceSkin *pSourceSkin );

	/*
	messages sent:
		"DmeElementChanged"	The element has been changed
	*/

private:
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", kv );
	MESSAGE_FUNC_INT( OnCheckButtonChecked, "CheckButtonChecked", state );

	// Marks the file as dirty
	void SetDirty( );

	vgui::TextEntry *m_pSkinName;
	vgui::TextEntry *m_pScale;
	vgui::CheckButton *m_pFlipTriangles;

	CDmeHandle< CDmeSourceSkin > m_hSourceSkin;
};


#endif // DMESOURCESKINPANEL_H

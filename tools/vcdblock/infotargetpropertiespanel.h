//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef INFOTARGETPROPERTIESPANEL_H
#define INFOTARGETPROPERTIESPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/editablepanel.h"
#include "tier1/utlstring.h"
#include "datamodel/dmehandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CVcdBlockDoc;
class CDmeVMFEntity;
class CScrollableEditablePanel;

namespace vgui
{
	class ComboBox;
	class Button;
	class TextEntry;
	class ListPanel;
	class CheckButton;
	class RadioButton;
}


//-----------------------------------------------------------------------------
// Panel that shows all entities in the level
//-----------------------------------------------------------------------------
class CInfoTargetPropertiesPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CInfoTargetPropertiesPanel, vgui::EditablePanel );

public:
	CInfoTargetPropertiesPanel( CVcdBlockDoc *pDoc, vgui::Panel* pParent );   // standard constructor

	// Sets the object to look at
	void SetObject( CDmeVMFEntity *pEntity );

private:
	// Populates the info_target fields
	void PopulateInfoTargetFields();

	// Text to attribute...
	void TextEntryToAttribute( vgui::TextEntry *pEntry, const char *pAttributeName );
	void TextEntriesToVector( vgui::TextEntry *pEntry[3], const char *pAttributeName );

	// Updates entity state when text fields change
	void UpdateInfoTarget();

	// Messages handled
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", kv );

	CVcdBlockDoc *m_pDoc;

	vgui::EditablePanel *m_pInfoTargetScroll;
	vgui::EditablePanel *m_pInfoTarget;

	vgui::TextEntry *m_pTargetName;
	vgui::TextEntry *m_pTargetPosition[3];
	vgui::TextEntry *m_pTargetOrientation[3];

	CDmeHandle< CDmeVMFEntity > m_hEntity;
};


#endif // INFOTARGETPROPERTIESPANEL_H

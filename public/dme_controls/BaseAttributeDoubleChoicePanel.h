//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef BASEATTRIBUTEDOUBLECHOICEPANEL_H
#define BASEATTRIBUTEDOUBLECHOICEPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/BaseAttributePanel.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
struct AttributeWidgetInfo_t;

namespace vgui
{
	class IScheme;
	class Panel;
	class Label;
	class ComboBox;
}

//-----------------------------------------------------------------------------
// Purpose: Helper to horizontally lay out the two child combo boxes used by
//  CBaseAttributeDoubleChoicePanel below
//-----------------------------------------------------------------------------
class CDoubleComboBoxContainerPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CDoubleComboBoxContainerPanel, vgui::Panel );
public:
	CDoubleComboBoxContainerPanel( vgui::Panel *parent, char const *name );
	void AddComboBox( int slot, vgui::ComboBox *box );

private:

	virtual void PerformLayout();

	vgui::ComboBox	*m_pBoxes[ 2 ];
};

//-----------------------------------------------------------------------------
// CBaseAttributeDoubleChoicePanel (similar to CBaseAttributeChoicePanel, but with side by side combo boxes)
//-----------------------------------------------------------------------------
class CBaseAttributeDoubleChoicePanel : public CBaseAttributePanel
{
	DECLARE_CLASS_SIMPLE( CBaseAttributeDoubleChoicePanel, CBaseAttributePanel );

public:
	CBaseAttributeDoubleChoicePanel( vgui::Panel *parent,	const AttributeWidgetInfo_t &info );

	virtual void PostConstructor();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

protected:
	virtual void Refresh();

private:
	// Derived classes can re-implement this to fill the combo box however they like
	virtual void PopulateComboBoxes( vgui::ComboBox *pComboBox[2] ) = 0;
	virtual void SetAttributeFromComboBoxes( vgui::ComboBox *pComboBox[2], KeyValues *pKeyValues[ 2 ] ) = 0;
	virtual void SetComboBoxesFromAttribute( vgui::ComboBox *pComboBox[2] ) = 0;

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );

	virtual void Apply();
	virtual vgui::Panel *GetDataPanel();

	CDoubleComboBoxContainerPanel		*m_pContainerPanel;
	vgui::ComboBox	*m_pData[2];
};


#endif // BASEATTRIBUTEDOUBLECHOICEPANEL_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef BASEATTRIBUTECHOICEPANEL_h
#define BASEATTRIBUTECHOICEPANEL_h

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
// CBaseAttributeChoicePanel
//-----------------------------------------------------------------------------
class CBaseAttributeChoicePanel : public CBaseAttributePanel
{
	DECLARE_CLASS_SIMPLE( CBaseAttributeChoicePanel, CBaseAttributePanel );

public:
	CBaseAttributeChoicePanel( vgui::Panel *parent,	const AttributeWidgetInfo_t &info );

	virtual void PostConstructor();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

protected:
	virtual void Refresh();

private:
	// Derived classes can re-implement this to fill the combo box however they like
	virtual void PopulateComboBox( vgui::ComboBox *pComboBox ) = 0;
	virtual void SetAttributeFromComboBox( vgui::ComboBox *pComboBox, KeyValues *pKeyValues ) = 0;
	virtual void SetComboBoxFromAttribute( vgui::ComboBox *pComboBox ) = 0;

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );

	virtual void Apply();
	virtual vgui::Panel *GetDataPanel();

	vgui::ComboBox	*m_pData;
};


#endif // BASEATTRIBUTECHOICEPANEL_h

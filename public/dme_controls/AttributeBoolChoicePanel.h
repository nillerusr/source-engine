//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTEBOOLCHOICEPANEL_h
#define ATTRIBUTEBOOLCHOICEPANEL_h

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/BaseAttributeChoicePanel.h"
#include "movieobjects/dmeeditortypedictionary.h"
#include "vgui_controls/MessageMap.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct AttributeWidgetInfo_t;

namespace vgui
{
	class Panel;
	class ComboBox;
}


//-----------------------------------------------------------------------------
// Configuration for integer choices
//-----------------------------------------------------------------------------
class CDmeEditorBoolChoicesInfo : public CDmeEditorChoicesInfo
{
	DEFINE_ELEMENT( CDmeEditorBoolChoicesInfo, CDmeEditorChoicesInfo );

public:
	// Add a choice
	void SetFalseChoice( const char *pChoiceString );
	void SetTrueChoice( const char *pChoiceString );

	// Gets the choices
	const char *GetFalseChoiceString( ) const;
	const char *GetTrueChoiceString( ) const;
};


//-----------------------------------------------------------------------------
// CAttributeBoolChoicePanel
//-----------------------------------------------------------------------------
class CAttributeBoolChoicePanel : public CBaseAttributeChoicePanel
{
	DECLARE_CLASS_SIMPLE( CAttributeBoolChoicePanel, CBaseAttributeChoicePanel );

public:
	CAttributeBoolChoicePanel( vgui::Panel *parent,	const AttributeWidgetInfo_t &info );

private:
	// Derived classes can re-implement this to fill the combo box however they like
	virtual void PopulateComboBox( vgui::ComboBox *pComboBox );
	virtual void SetAttributeFromComboBox( vgui::ComboBox *pComboBox, KeyValues *pKeyValues );
	virtual void SetComboBoxFromAttribute( vgui::ComboBox *pComboBox );
};


#endif // ATTRIBUTEBOOLCHOICEPANEL_h

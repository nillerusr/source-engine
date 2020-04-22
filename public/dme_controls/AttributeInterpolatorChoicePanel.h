//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTEINTERPOLATORTYPECHOICEPANEL_h
#define ATTRIBUTEINTERPOLATORTYPECHOICEPANEL_h

#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/BaseAttributeDoubleChoicePanel.h"
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
// CAttributeInterpolatorChoicePanel
//-----------------------------------------------------------------------------
class CAttributeInterpolatorChoicePanel : public CBaseAttributeDoubleChoicePanel
{
	DECLARE_CLASS_SIMPLE( CAttributeInterpolatorChoicePanel, CBaseAttributeDoubleChoicePanel );

public:
	CAttributeInterpolatorChoicePanel( vgui::Panel *parent,	const AttributeWidgetInfo_t &info );

private:
	virtual void PopulateComboBoxes( vgui::ComboBox *pComboBox[2] );
	virtual void SetAttributeFromComboBoxes( vgui::ComboBox *pComboBox[2], KeyValues *pKeyValues[ 2 ] );
	virtual void SetComboBoxesFromAttribute( vgui::ComboBox *pComboBox[2] );
};


#endif // ATTRIBUTEINTERPOLATORTYPECHOICEPANEL_h

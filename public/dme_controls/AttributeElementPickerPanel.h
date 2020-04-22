//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ATTRIBUTEELEMENTPICKERPANEL_H
#define ATTRIBUTEELEMENTPICKERPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "dme_controls/BaseAttributeChoicePanel.h"
#include "vgui_controls/PHandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
class CAttributeTextEntry;

namespace vgui
{
	class Label;
}


//-----------------------------------------------------------------------------
// CAttributeElementPickerPanel
//-----------------------------------------------------------------------------
class CAttributeElementPickerPanel : public CBaseAttributePanel
{
	DECLARE_CLASS_SIMPLE( CAttributeElementPickerPanel, CBaseAttributePanel );

public:
	CAttributeElementPickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info );

	virtual void OnCommand( const char *cmd );
	virtual void PerformLayout();

	virtual void PostConstructor();
	virtual void Apply();

private:
	// Inherited classes must implement this
	virtual	Panel *GetDataPanel();
	virtual void Refresh();

	MESSAGE_FUNC_PARAMS( OnDmeSelected, "DmeSelected", kv );
	virtual void ShowPickerDialog();

	vgui::DHANDLE< vgui::Button	>	m_hEdit;
	CAttributeTextEntry				*m_pData;
	bool							m_bShowMemoryUsage;
};


#endif // ATTRIBUTEELEMENTPICKERPANEL_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef ENTITYREPORTPANEL_H
#define ENTITYREPORTPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/editablepanel.h"
#include "tier1/utlstring.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CFoundryDoc;
class CDmeVMFEntity;

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
class CEntityReportPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CEntityReportPanel, vgui::EditablePanel );

public:
	CEntityReportPanel( CFoundryDoc *pDoc, vgui::Panel* pParent, const char *pName );   // standard constructor

// Inherited from Panel
	virtual void OnTick();
	virtual void OnCommand( const char *pCommand );

private:
	enum FilterType_t
	{
		FILTER_SHOW_EVERYTHING = 0,
		FILTER_SHOW_POINT_ENTITIES = 1,
		FILTER_SHOW_BRUSH_ENTITIES = 2
	};

	// Messages handled
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", kv );
	MESSAGE_FUNC_PARAMS( OnButtonToggled, "ButtonToggled", kv );
	MESSAGE_FUNC( OnDeleteEntities, "DeleteEntities" );

	// FIXME: Necessary because SetSelected doesn't cause a ButtonToggled message to trigger
	MESSAGE_FUNC_PARAMS( OnCheckButtonChecked, "CheckButtonChecked", kv );
	MESSAGE_FUNC_PARAMS( OnRadioButtonChecked, "RadioButtonChecked", kv );

	// Methods related to filtering
	void OnFilterByHidden( bool bState );
	void OnFilterByKeyvalue( bool bState );
	void OnFilterByClass( bool bState );
	void OnFilterKeyValueExact( bool bState );
	void OnFilterByType( FilterType_t type );
	void OnChangeFilterkey( const char *pText ); 
	void OnChangeFiltervalue( const char *pText ); 
	void OnChangeFilterclass( const char *pText );

	// Methods related to updating the listpanel
	void UpdateEntityList();
	bool ShouldAddEntityToList( CDmeVMFEntity *pEntity );

	// Methods related to saving settings 
	void ReadSettingsFromRegistry();
	void SaveSettingsToRegistry();

	// Call this when our settings are dirty
	void MarkDirty( bool bFilterDirty );

	// Shows the most recent selected object in properties window
	void OnProperties();

	CFoundryDoc *m_pDoc;
	FilterType_t m_iFilterByType;
	bool m_bFilterByClass;
	bool m_bFilterByHidden;
	bool m_bFilterByKeyvalue;
	bool m_bExact;
	bool m_bSuppressEntityListUpdate;

	CUtlString m_szFilterKey;
	CUtlString m_szFilterValue;
	CUtlString m_szFilterClass;

	bool m_bFilterTextChanged;
	float m_flFilterTime;

	bool m_bRegistrySettingsChanged;
	float m_flRegistryTime;

	vgui::CheckButton	*m_pExact;
	vgui::ComboBox		*m_pFilterClass;
	vgui::CheckButton	*m_pFilterByClass;
	vgui::ListPanel		*m_pEntities;
	vgui::TextEntry		*m_pFilterKey;
	vgui::TextEntry		*m_pFilterValue;
	vgui::CheckButton	*m_pFilterByKeyvalue;
	vgui::CheckButton	*m_pFilterByHidden;

	vgui::RadioButton	*m_pFilterEverything;
	vgui::RadioButton	*m_pFilterPointEntities;
	vgui::RadioButton	*m_pFilterBrushModels;
};


#endif // ENTITYREPORTPANEL_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef DMECOMBINATIONSYSTEMEDITORPANEL_H
#define DMECOMBINATIONSYSTEMEDITORPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "vgui_controls/Frame.h"
#include "datamodel/dmehandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeCombinationControlsPanel;
class CDmeCombinationDominationRulesPanel;
class CDmeCombinationOperator;
class CDmeElementPanel;

namespace vgui
{
	class PropertySheet;
	class PropertyPage;
	class Button;
}


//-----------------------------------------------------------------------------
// Dag editor panel
//-----------------------------------------------------------------------------
class CDmeCombinationSystemEditorPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CDmeCombinationSystemEditorPanel, vgui::EditablePanel );

public:
	// constructor, destructor
	CDmeCombinationSystemEditorPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CDmeCombinationSystemEditorPanel();

	// Sets the current scene + animation list
	void SetDmeElement( CDmeCombinationOperator *pComboSystem );
	CDmeCombinationOperator *GetDmeElement();

private:
	// Called when the selection changes moves
	MESSAGE_FUNC( OnPageChanged, "PageChanged" );
    MESSAGE_FUNC_PARAMS( OnDmeElementChanged, "DmeElementChanged", kv );

	vgui::PropertySheet *m_pEditorSheet;
	vgui::PropertyPage *m_pControlsPage;
	vgui::PropertyPage *m_pDominationRulesPage;
	vgui::PropertyPage *m_pPropertiesPage;
	CDmeCombinationControlsPanel *m_pControlsPanel;
	CDmeCombinationDominationRulesPanel *m_pDominationRulesPanel;
	CDmeElementPanel *m_pPropertiesPanel;
};


//-----------------------------------------------------------------------------
// Frame for combination system
//-----------------------------------------------------------------------------
class CDmeCombinationSystemEditorFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CDmeCombinationSystemEditorFrame, vgui::Frame );

public:
	CDmeCombinationSystemEditorFrame( vgui::Panel *pParent, const char *pTitle );
	~CDmeCombinationSystemEditorFrame();

	// Sets the current scene + animation list
	void SetCombinationOperator( CDmeCombinationOperator *pComboSystem );

	// Inherited from Frame
	virtual void OnCommand( const char *pCommand );

private:
    MESSAGE_FUNC( OnDmeElementChanged, "DmeElementChanged" );

	CDmeCombinationSystemEditorPanel *m_pEditor;
	vgui::Button *m_pOpenButton;
	vgui::Button *m_pCancelButton;
};


#endif // DMECOMBINATIONSYSTEMEDITORPANEL_H
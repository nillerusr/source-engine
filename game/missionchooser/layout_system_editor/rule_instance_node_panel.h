//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Panel to edit "rule_instance" nodes in layout system definitions.
//
//===============================================================================

#ifndef RULE_INSTANCE_NODE_H
#define RULE_INSTANCE_NODE_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "vgui_controls/Panel.h"
#include "vgui/MouseCode.h"
#include "layout_system/tilegen_rule.h"
#include "node_panel.h"

class KeyValues;
class CTilegenMissionPreprocessor;
class CTilegenRule;
namespace vgui
{
	class Button;
	class PanelListPanel;
}

class CRuleInstanceNodePanel : public CNodePanel
{
	DECLARE_CLASS_SIMPLE( CRuleInstanceNodePanel, CNodePanel );

public:
	CRuleInstanceNodePanel( Panel *pParent, const char *pName );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnCommand( const char *pCommand );

	MESSAGE_FUNC_PARAMS( OnRuleChanged, "RuleChanged", pKeyValues );

	void AddAllowableRuleType( const char *pType );

protected:
	virtual void CreatePanelContents();

private:
	// Indicates whether this rule instance is being wrapped by a dummy container node.
	// There are some cases where a rule_instance must be contained within a grouping key-values node,
	// as opposed to directly beneath the parent.  In those cases, we need to 
	// specially handle the extra layer of indirection.
	bool HasDummyContainerNode();

	vgui::Button *m_pChangeRuleButton;
	vgui::Button *m_pDisableRuleButton;

	bool m_bDisabled;

	// The set of allowed rule types for this node
	CUtlVector< RuleType_t > m_AllowableRuleTypes;
};

class CRulePickerDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CRulePickerDialog, vgui::Frame );

public:
	CRulePickerDialog( 
		vgui::Panel *pParent, 
		const char *pName, 
		const CTilegenMissionPreprocessor *pRulePreprocessor,
		RuleType_t *pAllowableRuleTypes,
		int nNumAllowableRuleTypes );
	
	MESSAGE_FUNC_PTR( OnRuleDetailsClicked, "RuleDetailsClicked", panel );

private:
	const CTilegenMissionPreprocessor *m_pRulePreprocessor;
	vgui::PanelListPanel *m_pRulePanelList;
};

class CRuleDetailsPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CRuleDetailsPanel, vgui::Panel );

public:
	CRuleDetailsPanel( vgui::Panel *pParent, const char *pName, const CTilegenRule *pRule );

	virtual void OnMouseReleased( vgui::MouseCode code );

	const CTilegenRule *GetRule() { return m_pRule; }

private:
	vgui::Label *m_pRuleNameLabel;

	const CTilegenRule *m_pRule;
};

#endif // RULE_INSTANCE_NODE_H
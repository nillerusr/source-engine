//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Panel to edit leafy data (e.g. ints, strings).
//
//===============================================================================

#ifndef TEXT_ENTRY_PANEL_H
#define TEXT_ENTRY_PANEL_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "node_panel.h"

namespace vgui
{
	class TextEntry;
	class Label;
	class Button;
}

class CTilegenRule;

class CRuleInstanceParameterPanel : public CNodePanel
{
	DECLARE_CLASS_SIMPLE( CRuleInstanceParameterPanel, CNodePanel );

public:
	CRuleInstanceParameterPanel( Panel *pParent, const char *pName, const CTilegenRule *pRule, int nRuleParameterIndex );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnCommand( const char *pCommand );

	virtual void UpdateState();

protected:
	virtual void CreatePanelContents();
	
private:
	vgui::Button *m_pAddButton;
	vgui::Button *m_pDeleteButton;
	const CTilegenRule *m_pRule;
	int m_nRuleParameterIndex;
};

#endif // TEXT_ENTRY_PANEL_H
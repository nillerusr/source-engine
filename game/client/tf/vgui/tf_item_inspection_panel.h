//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_ITEM_INSPECTION_PANEL_H
#define TF_ITEM_INSPECTION_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/EditablePanel.h>
#include "tf_controls.h"

using namespace vgui;

class CEconItemView;
class CEmbeddedItemModelPanel;
class CNavigationPanel;
class CItemModelPanel;

class CTFItemInspectionPanel : public EditablePanel
{
	DECLARE_CLASS_SIMPLE( CTFItemInspectionPanel, EditablePanel )
public:
	CTFItemInspectionPanel( Panel* pPanel, const char *pszName );

	virtual void ApplySchemeSettings( IScheme *pScheme ) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;
	virtual void OnCommand( const char *command ) OVERRIDE;
	void SetItemCopy ( CEconItemView *pItem );
	void Reset();
	void SetSpecialAttributesOnly( bool bSpecialOnly );

private:
	// we always want to copy item with SetItemCopy to make sure that we use high res skin
	void SetItem( CEconItemView *pItem );
	void RecompositeItem();
	CNavigationPanel* m_pTeamColorNavPanel;

	MESSAGE_FUNC_PARAMS( OnNavButtonSelected, "NavButtonSelected", pData );
	MESSAGE_FUNC_PTR( OnRadioButtonChecked, "RadioButtonChecked", panel );

	CEmbeddedItemModelPanel *m_pModelInspectPanel;
	CItemModelPanel *m_pItemNamePanel;

	CEconItemView *m_pItemViewData;
	CEconItem *m_pSOEconItemData;
};

#endif // TF_ITEM_INSPECTION_PANEL_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIONS_SUB_MODIFICATION_H
#define OPTIONS_SUB_MODIFICATION_H
#include "vgui_controls/Button.h"
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>

class CCvarNegateCheckButton;
class CKeyToggleCheckButton;
class CCvarToggleCheckButton;
class CCvarSlider;


namespace vgui
{
	class Label;
	class Panel;
}
class VControlsListPanel;

//-----------------------------------------------------------------------------
// Purpose: Touch Details, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubModification : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE(COptionsSubModification, vgui::PropertyPage);

public:
	COptionsSubModification(vgui::Panel *parent);
	~COptionsSubModification();

	virtual void OnResetData();
	virtual void OnApplyChanges();
	virtual void OnCommand(const char *command);

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	MESSAGE_FUNC_PTR(OnControlModified, "ControlModified", panel);
	MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);
	MESSAGE_FUNC_PTR(OnCheckButtonChecked, "CheckButtonChecked", panel)
	{
		OnControlModified(panel);
	}

	void UpdateLabel(CCvarSlider *slider, vgui::TextEntry *label);

private:
	vgui::TextEntry *m_pAspectRatioLabel;
	vgui::Button *m_pGiveWeaponButton;
	vgui::Button *m_pGiveHealthKitButton;
	

	CCvarToggleCheckButton *m_pChangeCheatFlag;
	CCvarSlider *m_pAspectRatioSlider;
};

#endif // OPTIONS_SUB_MODIFICATION_H
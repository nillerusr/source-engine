//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIONS_SUB_MOUSE_H
#define OPTIONS_SUB_MOUSE_H
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

//-----------------------------------------------------------------------------
// Purpose: Mouse Details, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubMouse : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubMouse, vgui::PropertyPage );

public:
	COptionsSubMouse(vgui::Panel *parent);
	~COptionsSubMouse();

	virtual void OnResetData();
	virtual void OnApplyChanges();

protected:
    virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	MESSAGE_FUNC_PTR( OnControlModified, "ControlModified", panel );
    MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC_PTR( OnCheckButtonChecked, "CheckButtonChecked", panel )
	{
		OnControlModified( panel );
	}

    void UpdateSensitivityLabel();
	void UpdateJoystickPanels();

	CCvarNegateCheckButton		*m_pReverseMouseCheckBox;
	CCvarToggleCheckButton		*m_pMouseFilterCheckBox;
	CCvarToggleCheckButton		*m_pJoystickCheckBox;
	CCvarToggleCheckButton		*m_pJoystickSouthpawCheckBox;
	CCvarToggleCheckButton		*m_pQuickInfoCheckBox;
	CCvarToggleCheckButton		*m_pReverseJoystickCheckBox;

	CCvarSlider					*m_pMouseSensitivitySlider;
    vgui::TextEntry             *m_pMouseSensitivityLabel;

	CCvarSlider					*m_pJoyYawSensitivitySlider;
	vgui::Label					*m_pJoyYawSensitivityPreLabel;
	CCvarSlider					*m_pJoyPitchSensitivitySlider;
	vgui::Label					*m_pJoyPitchSensitivityPreLabel;
};



#endif // OPTIONS_SUB_MOUSE_H
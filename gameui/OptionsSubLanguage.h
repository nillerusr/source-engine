//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Language selection options page
//
//=============================================================================//

#ifndef OPTIONS_SUB_LANGUAGE_H
#define OPTIONS_SUB_LANGUAGE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>

namespace vgui
{
	class Label;
	class ComboBox;
	class Panel;
}

//-----------------------------------------------------------------------------
// Purpose: Language selection, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubLanguage : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubLanguage, vgui::PropertyPage );

public:
	COptionsSubLanguage(vgui::Panel *parent);
	~COptionsSubLanguage();

	virtual void OnResetData();
	virtual void OnApplyChanges();

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	MESSAGE_FUNC_PTR( OnControlModified, "ControlModified", panel );
	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );

private:
	vgui::ComboBox	*m_pLanguageCombo;
	vgui::Label		*m_pLanguageLabel;
	int				m_nCurrentLanguage;
};

#endif // OPTIONS_SUB_LANGUAGE_H

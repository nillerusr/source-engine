//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIONS_SUB_PORTAL_H
#define OPTIONS_SUB_PORTAL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>

class CLabeledCommandComboBox;
class CCvarToggleCheckButton;

namespace vgui
{
	class Label;
	class Panel;
}

//-----------------------------------------------------------------------------
// Purpose: Mouse Details, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubPortal : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubPortal, vgui::PropertyPage );

public:
	COptionsSubPortal(vgui::Panel *parent);
	~COptionsSubPortal();

	virtual void OnResetData();
	virtual void OnApplyChanges();

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	MESSAGE_FUNC( OnCheckButtonChecked, "CheckButtonChecked" )
	{
		OnControlModified();
	}
	MESSAGE_FUNC( OnControlModified, "ControlModified" );
	MESSAGE_FUNC( OnTextChanged, "TextChanged" )
	{
		OnControlModified();
	}

	CCvarToggleCheckButton		*m_pPortalFunnelCheckBox;
	vgui::ComboBox				*m_pPortalDepthCombo;
};



#endif // OPTIONS_SUB_MOUSE_H
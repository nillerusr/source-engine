#ifndef BRIEFINGPROPERTYSHEET_H
#define BRIEFINGPROPERTYSHEET_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/PropertySheet.h>

// main property sheet used in the briefing (has tabs for mission, roster, equip, etc.)

class BriefingPropertySheet : public vgui::PropertySheet
{
	DECLARE_CLASS_SIMPLE( BriefingPropertySheet, vgui::PropertySheet );
public:
	BriefingPropertySheet(vgui::Panel *parent, const char *name);
	virtual ~BriefingPropertySheet();
	virtual void AddPageCustomButton(vgui::Panel *page, const char *title, vgui::HScheme scheme);
	virtual void ChangeActiveTab( int index );

	virtual void PerformLayout();

	void EnableTabSounds() { m_bPlayTabSounds = true; }

	bool m_bPlayTabSounds;
};

#endif // BRIEFINGPROPERTYSHEET_H
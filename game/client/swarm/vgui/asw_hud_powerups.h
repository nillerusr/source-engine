#ifndef _INCLUDED_ASW_HUD_POWERUPS_H
#define _INCLUDED_ASW_HUD_POWERUPS_H

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include "c_asw_marine.h"

namespace vgui
{
	class Label;
	class ImagePanel;
};
class C_ASW_Weapon;

// panel to show fast reload timing (this was just a test, fast reloading is currently disabled)

class CASW_Hud_Powerups : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASW_Hud_Powerups, vgui::Panel );

	CASW_Hud_Powerups( vgui::Panel *pParent, const char *pElementName );
	virtual ~CASW_Hud_Powerups();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();
	virtual void PerformLayout();
	virtual void UpdatePowerupIcon( C_ASW_Marine *pMarine );

	vgui::ImagePanel *m_pIconImage;
	vgui::Label *m_pStringLabel;
};

#endif /* _INCLUDED_ASW_HUD_POWERUPS_H */
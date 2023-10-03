#ifndef _INCLUDED_ASW_VGUI_QUEEN_HEALTH_H
#define _INCLUDED_ASW_VGUI_QUEEN_HEALTH_H
#ifdef _WIN32
#pragma once
#endif


#include <vgui_controls/Frame.h>
#include <vgui/IScheme.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui/IScheme.h"
#include "hudelement.h"

// This panel shows the remaining health of the queen (broken into 5 bars to show each of her summoning waves)

namespace vgui
{
	class Label;
};
class C_ASW_Queen;

#define ASW_QUEEN_HEALTH_BARS 5

class CASW_VGUI_Queen_Health_Panel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Queen_Health_Panel, vgui::Panel );

	CASW_VGUI_Queen_Health_Panel( vgui::Panel *pParent, const char *pElementName, C_ASW_Queen *pQueen );
	virtual ~CASW_VGUI_Queen_Health_Panel();
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible
	virtual void PaintBackground();
	virtual void PerformLayout();
	virtual void UpdateBars();
	virtual void Paint();
	void PositionAroundQueen();
	C_ASW_Queen* GetQueen() { return m_hQueen.Get(); }
	
	vgui::Panel* m_pBackdrop;
	vgui::Panel* m_pHealthBar[ASW_QUEEN_HEALTH_BARS];

	CHandle<C_ASW_Queen> m_hQueen;
};

#endif /* _INCLUDED_ASW_VGUI_QUEEN_HEALTH_H */
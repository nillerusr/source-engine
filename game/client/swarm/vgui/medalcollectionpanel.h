#ifndef _INCLUDED_MEDAL_COLLETION_PANEL_H
#define _INCLUDED_MEDAL_COLLETION_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

namespace vgui
{
	class Label;
	class ImagePanel;
	class ImagePanel;
};

class MedalPanel;
class BriefingTooltip;

#define ASW_MEDAL_COLLECTION_PORTRAITS 9
#define ASW_MEDAL_COLLECTION_MEDALS 61		// have to go 1 up, since the indices start at 1

// shows all the medals this player has collected so far (from his client store)
class MedalCollectionPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( MedalCollectionPanel, vgui::Panel );
public:
	MedalCollectionPanel(vgui::Panel *parent, const char *name);
	
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();
	virtual void OnThink();
	void UpdateMedals();
	virtual void OnMouseReleased(vgui::MouseCode code);
	void LayoutMedal(MedalPanel* pMedal, int &mx, int &my, int iSpacing = 0);
	
	vgui::Label *m_pTitle;
	vgui::Label *m_pCollectionLabel;
	vgui::Label *m_pNCOLabel;
	vgui::Label *m_pSWLabel;
	vgui::Label *m_pMedicLabel;
	vgui::Label *m_pTechLabel;
	vgui::Label *m_pTotalLabel;

	vgui::ImagePanel *m_pPortrait[ASW_MEDAL_COLLECTION_PORTRAITS];

	vgui::Panel *m_pPlayerArea;
	vgui::Panel *m_pNCOArea;
	vgui::Panel *m_pSpecialWeaponsArea;
	vgui::Panel *m_pMedicArea;
	vgui::Panel *m_pTechArea;
	vgui::Panel *m_pGenArea;
	vgui::ImagePanel *m_pBackDrop;

	vgui::Label *m_pOnlineLabel;
	vgui::ImagePanel *m_pOnlineImage;

	int m_iCursorOver;

	bool m_bOffline;
	int m_iShowOnlyMarine;

	MedalPanel* m_pMedals[ASW_MEDAL_COLLECTION_MEDALS];
	BriefingTooltip* m_pTooltip;
	char m_szTotalString[128];
	char m_szNameString[32];

	float m_fScale;
};

#endif // _INCLUDED_MEDAL_COLLETION_PANEL_H

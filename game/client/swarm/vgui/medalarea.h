#ifndef _INCLUDED_MEDAL_AREA_H
#define _INCLUDED_MEDAL_AREA_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

namespace vgui
{
	class ImagePanel;
	class Label;
};

class MedalPanel;
class C_ASW_Marine_Resource;

// shows medals earned by a particular marine
class MedalArea : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( MedalArea, vgui::Panel );
public:
	MedalArea(vgui::Panel *parent, const char *name, int iMedalsAcross);

	virtual void PerformLayout();
	virtual void OnThink();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	virtual const char * GetMedalString();
	virtual void OnMedalStringChanged();
	virtual void SetProfileIndex(int i) { m_iProfileIndex = i; }
	virtual void SetMarineResource(C_ASW_Marine_Resource *pMR) { m_hMI = pMR; }

	virtual void HideMedals();
	virtual void StartShowingMedals(float time);

	CUtlVector<MedalPanel*> m_Medals;
	int m_iProfileIndex;	
	CHandle<C_ASW_Marine_Resource> m_hMI;
	int m_iMedalsAcross;
	
	float m_fStartShowingMedalsTime;
	int m_iLastFullyShown;

	bool m_bRightAlign;

	char m_szMedalString[255];

	static float s_fLastMedalSound;
};

// a version of the above, but shows:
//  -medals earned previously in this campaign (i.e. checking the campaign save rather than the marine resource for the medal string)
class MedalAreaCampaign : public MedalArea
{
	DECLARE_CLASS_SIMPLE( MedalAreaCampaign, MedalArea );
public:
	MedalAreaCampaign(vgui::Panel *parent, const char *name, int iMedalsAcross);

	virtual const char * GetMedalString();
	virtual void OnMedalStringChanged();
};

// a version of the above, but shows:
//  -medals earned by the player this mission, rather than a marine
class MedalAreaPlayer : public MedalArea
{
	DECLARE_CLASS_SIMPLE( MedalAreaPlayer, MedalArea );
public:
	MedalAreaPlayer(vgui::Panel *parent, const char *name, int iMedalsAcross);

	virtual const char * GetMedalString();
	virtual void OnMedalStringChanged();
	// NOTE: Use SetProfileIndex to set the player's index (players start from 0 up)
};


#endif // _INCLUDED_MEDAL_AREA_H
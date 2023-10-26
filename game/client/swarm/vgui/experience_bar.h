#ifndef _INCLUDED_EXPERIENCE_BAR_H
#define _INCLUDED_EXPERIENCE_BAR_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>
#include "steam/steam_api.h"

namespace vgui
{
	class Label;
	class ImagePanel;
};
class StatsBar;
class C_ASW_Player;
class CAvatarImagePanel;

// this class shows the player's name, level and experience
// also handles animating the experience bar at the end of a mission

class ExperienceBar : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( ExperienceBar, vgui::EditablePanel );
public:
	ExperienceBar( vgui::Panel *parent, const char *name );
	
	virtual void OnTick();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	void InitFor( C_ASW_Player *pPlayer );
	void UpdateLevelLabel();
	void UpdateMinMaxes( int nPromotion );

	bool IsDoneAnimating();

	CHandle<C_ASW_Player> m_hPlayer;

	vgui::Label* m_pPlayerNameLabel;
	vgui::Label* m_pPlayerLevelLabel;
	StatsBar* m_pExperienceBar;
	vgui::Label* m_pExperienceCounter;
	vgui::Label* m_pLevelUpLabel;
	vgui::Panel *m_pAvatarBackground;
	CAvatarImagePanel	*m_pAvatarImage;
	vgui::ImagePanel *m_pPromotionIcon;

	float m_flOldBarMin;
	bool m_bOldCapped;
	int m_iPlayerLevel;
	int m_nOldPlayerXP;
	CSteamID m_lastSteamID;
	int m_nLastPromotion;
};

class ExperienceBarSmall : public ExperienceBar
{
	DECLARE_CLASS_SIMPLE( ExperienceBarSmall, ExperienceBar );
public:
	ExperienceBarSmall( vgui::Panel *parent, const char *name );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
};

#endif // _INCLUDED_EXPERIENCE_BAR_H
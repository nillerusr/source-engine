//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef CS_HUD_ACHIVEMENT_ANNOUNCE_H
#define CS_HUD_ACHIVEMENT_ANNOUNCE_H
#ifdef _WIN32
#pragma once
#endif

#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ScalableImagePanel.h>
#include "vgui/ILocalize.h"
#include "vgui_avatarimage.h"
#include "hud.h"
#include "hudelement.h"
#include "cs_hud_playerhealth.h"

#include "cs_shareddefs.h"

using namespace vgui;

class IAchievement;


class CCSAchivementInfoPanel : public vgui::EditablePanel
{
    DECLARE_CLASS_SIMPLE( CCSAchivementInfoPanel, vgui::EditablePanel );

public:
    CCSAchivementInfoPanel( vgui::Panel *parent, const char* name);
    ~CCSAchivementInfoPanel();
    virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
    void SetAchievement(IAchievement* pAchievement);    

private:
    vgui::Label                     *m_pAchievementNameLabel;
    vgui::Label                     *m_pAchievementDescLabel;
    vgui::ScalableImagePanel        *m_pAchievementIcon;    
};



class CCSAchievementAnnouncePanel: public EditablePanel, public CHudElement
{
private:
	DECLARE_CLASS_SIMPLE( CCSAchievementAnnouncePanel, EditablePanel );

public:
	CCSAchievementAnnouncePanel( const char *pElementName );
    ~CCSAchievementAnnouncePanel();

	virtual void Reset();
	virtual void Init();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void FireGameEvent( IGameEvent * event );

	void Show();
	void Hide();

	virtual bool ShouldDraw( void );
	virtual void Paint( void );
	void OnThink( void );
	bool GetAlphaFromTime(float elapsedTime, float delay, float fadeInTime, float holdTime, float fadeOutTime, float&alpha);

	int	HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

protected:
    bool GetGlowAlpha (float time, float& alpha);
    bool GetAchievementPanelAlpha (float time, float& alpha);

private:
	
    CUtlQueue<eCSAchievementType>   m_achievementQueue;
    eCSAchievementType              m_currentDisplayedAchievement;  
    float                           m_displayStartTime;
	
	
    vgui::EditablePanel		*m_pGlowPanel;
    CCSAchivementInfoPanel  *m_pAchievementInfoPanel;

	bool					m_bShouldBeVisible;
};

#endif //CS_HUD_ACHIVEMENT_ANNOUNCE_H
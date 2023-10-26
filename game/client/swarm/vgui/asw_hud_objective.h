#ifndef _INCLUDED_ASW_HUD_OBJECTIVE_H
#define _INCLUDED_ASW_HUD_OBJECTIVE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_hudelement.h"
#include <vgui_controls/Panel.h>
#include "asw_shareddefs.h"

namespace vgui
{
	class IScheme;
	class ImagePanel;
	class Label;
};
class C_ASW_Objective;

extern ConVar asw_draw_hud;
// shows the summary of the objectives in the top left of the screen during the mission
class CASWHudObjective : public CASW_HudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASWHudObjective, vgui::Panel );
public:
	CASWHudObjective( const char *pElementName );
	virtual void OnThink();
	virtual void OnTick();
	virtual void Init();
	virtual void PerformLayout();		
	virtual void LevelShutdown();
	virtual bool ShouldDraw( void ) { return asw_draw_hud.GetBool() && CASW_HudElement::ShouldDraw(); }
	void ShowObjectives( float fDuration );

	void LayoutObjectives();
	void UpdateObjectiveList();

	void SetDefaultHUDFonts();
	vgui::HFont m_hDefaultFont;
	vgui::HFont m_hDefaultGlowFont;
	vgui::HFont m_hSmallFont;	
	vgui::HFont m_hSmallGlowFont;
	int m_iLastHeight;
	bool IsSpectating();

	C_ASW_Objective* GetObjective( int i );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void Paint();

	int m_iNumLetters, m_iLastNumLetters;
	float m_fNextLetterTime;
	bool m_bPlayMissionCompleteSequence;
	int m_iObjectiveCompleteSequencePos;
	int m_iNumObjectivesListed;
	bool m_bShowObjectives;
	vgui::Label* m_pCompleteLabel;
	vgui::Label* m_pCompleteLabelBD;
	CPanelAnimationVar( vgui::HFont, m_font, "ObjectiveFont", "Default" );
	CPanelAnimationVar( vgui::HFont, m_hGlowFont, "ObjectiveGlowFont", "DefaultBlur" );	

	vgui::Label* m_pHeaderLabel;
	vgui::Label* m_pHeaderGlowLabel;
	vgui::Label* m_pObjectiveGlowLabel;
	// show a list of objectives
	CHandle<C_ASW_Objective> m_hObjectives[ASW_MAX_OBJECTIVES];
	CHandle<C_ASW_Objective> m_hObjectiveComplete;		// the objective we're currently highlighting as complete
	vgui::ImagePanel* m_pTickBox[ASW_MAX_OBJECTIVES];
	vgui::ImagePanel* m_pObjectiveIcon[ASW_MAX_OBJECTIVES];
	vgui::Label* m_pObjectiveLabel[ASW_MAX_OBJECTIVES];
	bool m_bObjectiveComplete[ASW_MAX_OBJECTIVES];
	bool m_bObjectiveTitleEmpty[ASW_MAX_OBJECTIVES];
	float m_flCurrentAlpha;
	float m_fLastVisibleTime;
	float m_flShowObjectivesTime;
	bool m_bLastSpectating;

	CPanelAnimationVarAliasType( int, m_nBlackBarTexture, "BlackBarTexture", "vgui/swarm/HUD/ASWHUDBlackBar", "textureid" );
};

// standard font sizes used by other HUD elements
vgui::HFont ASW_GetDefaultFont(bool bGlow=false);
vgui::HFont ASW_GetSmallFont(bool bGlow=false);

#endif // _INCLUDED_ASW_HUD_OBJECTIVE_H

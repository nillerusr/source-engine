#ifndef _INCLUDED_ASW_HUD_PORTRAITS_H
#define _INCLUDED_ASW_HUD_PORTRAITS_H
#ifdef _WIN32
#pragma once
#endif

#define ASW_MAX_PORTRAITS 8

#include "asw_hudelement.h"
#include "hud_numericdisplay.h"
#include "asw_shareddefs.h"

class C_ASW_Marine_Resource;
class CASW_HUD_Marine_Portrait;

//-----------------------------------------------------------------------------
// Shows all the marine portrait panels down the top right of the screen
//-----------------------------------------------------------------------------
class CASWHudPortraits : public CASW_HudElement, public CHudNumericDisplay
{
	DECLARE_CLASS_SIMPLE( CASWHudPortraits, CHudNumericDisplay );

public:
	CASWHudPortraits( const char *pElementName );
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void OnThink();
	void UpdatePortraits(bool bResize);
	virtual void Paint();
	virtual bool ShouldDraw( void );

	//virtual void DrawTexturedBox(int x, int y, int wide, int tall, Color color, float normalizedAlpha );
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();

	CPanelAnimationVarAliasType( int, m_nBlackBarTexture, "BlackBarTexture", "vgui/swarm/HUD/ASWHUDBlackBar", "textureid" );
	CPanelAnimationVarAliasType( int, m_nInfestedTexture, "InfestedHUDIcon", "vgui/swarm/HUD/InfestedHUDIcon", "textureid" );
	CPanelAnimationVarAliasType( int, m_nFiringTexture, "FiringHUDIcon", "vgui/swarm/HUD/FiringHUDIcon", "textureid" );
	CPanelAnimationVarAliasType( int, m_nWhiteTexture, "WhiteTexture", "vgui/white", "textureid" );


	// storing marine resources to draw
	int m_iNumMyMarines, m_iNumOtherMarines;
	CHandle<C_ASW_Marine_Resource> m_hMyMarine[ASW_MAX_MARINE_RESOURCES];
	CHandle<C_ASW_Marine_Resource> m_hOtherMarine[ASW_MAX_MARINE_RESOURCES];
	CHandle<C_ASW_Marine_Resource> m_hCurrentlySelectedMarineResource;

	// vars for drawing the marine panels
	int m_CurrentY;		// Y coord to draw the next frame at
	int m_iCurrentMarine;
	int m_FrameWidth;	// how wide our panel is

	int m_iFiringMod;	// used to strobe the firing texture
	float m_fFiringFade, m_fInfestedFade;

	bool m_bLastJoypadMode;

	// marine's name
	CPanelAnimationVar( vgui::HFont, m_hMarineNameFont, "MarineNameFont", "Default" );
	CPanelAnimationVar( vgui::HFont, m_hMarineNumberFont, "MarineNumberFont", "Default" );

	CPanelAnimationVar( bool, m_bHorizontalLayout, "horizontal", "1" );

	CASW_HUD_Marine_Portrait* m_pPortraits[ASW_MAX_PORTRAITS];

	void OnPortraitSizeChanged( CASW_HUD_Marine_Portrait *pPortrait );

protected:
	enum
	{
		PB_HEALTH,
		PB_AMMO,
		PB_CLIPS,
	};
	// dimensions of the bars, in pixels
	struct PortraitBar_t
	{
		int CornerX, CornerY;
		int Width, Height;
	};
	PortraitBar_t m_PortraitBar[3];
};	

#endif // _INCLUDED_ASW_HUD_PORTRAITS_H
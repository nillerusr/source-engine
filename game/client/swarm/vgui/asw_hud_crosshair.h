#ifndef _INCLUDED_ASW_HUD_CROSSHAIR_H
#define _INCLUDED_ASW_HUD_CROSSHAIR_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_hudelement.h"
#include <vgui_controls/Panel.h>
#include "asw_circularprogressbar.h"

namespace vgui
{
	class IScheme;
};

#define ASW_MAX_TURRET_TARGETS 16

extern ConVar asw_draw_hud;
// shows the main crosshair, pen crosshair when over the minimap and various brackets when using a remote turret
class CASWHudCrosshair : public CASW_HudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASWHudCrosshair, vgui::Panel );
public:
	CASWHudCrosshair( const char *pElementName );

	void				SetCrosshairAngle( const QAngle& angle );
	void				SetCrosshair( CHudTexture *texture, Color& clr );
	void				DrawCrosshair( void );
	void				DrawDirectionalCrosshair( int x, int y, int iSize );
	void				GetCurrentPos( int &x, int &y );
	int					GetCurrentCrosshair( int x, int y );
	void				SetShowGiveAmmo (bool b, int iAmmoType ) { m_bShowGiveAmmo = b; m_iShowGiveAmmoType = iAmmoType; }
	void				SetShowGiveHealth(bool b) { m_bShowGiveHealth = b; }
	virtual bool		ShouldDraw( void ) { return asw_draw_hud.GetBool() && CASW_HudElement::ShouldDraw(); }

	void				DrawSniperScope( int x, int y );

	CPanelAnimationVarAliasType( int, m_nCrosshairTexture, "CrosshairTexture", "vgui/swarm/hud/ASWCrosshairDefaultCenter", "textureid" );//"vgui/hud/as_crosshair_01"
	CPanelAnimationVarAliasType( int, m_nDirectCrosshairTexture, "DirectCrosshairTexture", "vgui/hud/as_directional_crosshair_01", "textureid" );
	CPanelAnimationVarAliasType( int, m_nDirectCrosshairTextureX, "DirectCrosshairTextureX", "vgui/hud/as_directional_crosshair_01_X", "textureid" );
	CPanelAnimationVarAliasType( int, m_nDirectCrosshairTexture2, "DirectCrosshairTexture2", "vgui/swarm/hud/ASWCrosshairDefaultEdge", "textureid" );
	CPanelAnimationVarAliasType( int, m_nMinimapDrawCrosshairTexture, "DrawCrosshairTexture", "vgui/swarm/HUD/ASWDrawcrosshair", "textureid" );
	CPanelAnimationVarAliasType( int, m_nGiveAmmoTexture, "GiveAmmoTexture", "vgui/swarm/HUD/GiveAmmoCrosshair", "textureid" );
	CPanelAnimationVarAliasType( int, m_nGiveHealthTexture, "GiveHealthTexture", "vgui/swarm/HUD/GiveHealthCrosshair", "textureid" );
	CPanelAnimationVarAliasType( int, m_nHackCrosshairTexture, "HackCrosshairTexture", "vgui/swarm/HUD/ASWHackCrosshair", "textureid" );
	CPanelAnimationVarAliasType( int, m_nHackActiveCrosshairTexture, "HackActiveCrosshairTexture", "vgui/swarm/HUD/ASWHackActiveCrosshair", "textureid" );
	CPanelAnimationVarAliasType( int, m_nTurretTexture, "TurretTexture", "vgui/swarm/Computer/TurretOverlay3", "textureid" );

	CPanelAnimationVarAliasType( int, m_nLeftBracketTexture, "LeftBracket", "vgui/swarm/Computer/TurretLeftBracket", "textureid" );
	CPanelAnimationVarAliasType( int, m_nRightBracketTexture, "RightBracket", "vgui/swarm/Computer/TurretRightBracket", "textureid" );
	CPanelAnimationVarAliasType( int, m_nLowerBracketTexture, "LowerBracket", "vgui/swarm/Computer/TurretLowerBracket", "textureid" );
	CPanelAnimationVarAliasType( int, m_nTurretCrosshair, "TurretCrosshair", "vgui/swarm/Computer/TurretCrosshair", "textureid" );
	
	CPanelAnimationVarAliasType( int, m_nBlackBarTexture, "BlackBarTexture", "vgui/swarm/HUD/ASWHUDBlackBar", "textureid" );
	CPanelAnimationVar( vgui::HFont, m_hGiveAmmoFont, "GiveAmmoFont", "Default" );
	CPanelAnimationVar( vgui::HFont, m_hTurretFont, "TurretFont", "Default" );
	CPanelAnimationVarAliasType( int, m_nSniperMagnifyTexture, "MagnifyTexture", "effects/magnifyinglens", "textureid" );
	
	const char * GetAmmoName(int iAmmoType);
	int m_iShowGiveAmmoType;

	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void	Paint();
	virtual void	PaintTurretTextures();
	void			PaintReloadProgressBar();
	float			RescaleProgessForArt( float flProgress = 1.0f );
	void DrawAttachedIcon(int iTexture, int &x, int &y, const wchar_t *text = 0);

	virtual void OnThink();
	virtual void PerformLayout();

private:
	// Crosshair sprite and colors
	Color				m_clrCrosshair;
	QAngle				m_vecCrossHairOffsetAngle;

	QAngle				m_curViewAngles;
	Vector				m_curViewOrigin;	

	// for showing attached icons
	bool		m_bShowGiveAmmo, m_bShowGiveHealth;
	bool				m_bIsReloading;

	vgui::ASWCircularProgressBar *m_pAmmoProgress;
	vgui::ASWCircularProgressBar *m_pFastReloadBar;

	vgui::Label *m_pTurretTextTopLeft;
	vgui::Label *m_pTurretTextTopRight;
	vgui::Label *m_pTurretTextTopLeftGlow;
	vgui::Label *m_pTurretTextTopRightGlow;

	// for fancy brackets that close in on possible targets while in turret view mode
	EHANDLE m_TurretTarget[ASW_MAX_TURRET_TARGETS];
	float m_fTurretTargetLock[ASW_MAX_TURRET_TARGETS];
};


// Enable/disable crosshair rendering.
extern ConVar crosshair;


#endif // _INCLUDED_ASW_HUD_CROSSHAIR_H

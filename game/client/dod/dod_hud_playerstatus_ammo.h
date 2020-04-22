//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_HUD_PLAYERSTATUS_AMMO_H
#define DOD_HUD_PLAYERSTATUS_AMMO_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Displays current ammunition level
//-----------------------------------------------------------------------------
class CDoDHudAmmo : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CDoDHudAmmo, vgui::Panel );

public:
	CDoDHudAmmo( vgui::Panel *parent, const char *name );
	void Init( void );

	void SetAmmo( int ammo, bool playAnimation );
	void SetAmmo2( int ammo2, bool playAnimation );
		
	virtual void OnThink();
	virtual void Paint( void );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	
private:
	void DrawAmmoCount( int count );
	void DrawText( char *text, int x, int y, Color clrText );
	void DrawNumbers( int num, int x, int y );

	void PaintGrenadeAmmo( CWeaponDODBase *pWpn );
	void PaintBazookaAmmo( CWeaponDODBase *pWpn );
	void PaintMGAmmo( CWeaponDODBase *pWpn );
	void PaintGunAmmo( CWeaponDODBase *pWpn );
	void PaintRifleGrenadeAmmo( CWeaponDODBase *pWpn );

	CHandle< C_BaseCombatWeapon > m_hCurrentActiveWeapon;
	int		m_iAmmo;
	int		m_iAmmo2;

	bool	m_bUsesClips;

	int		m_iAdditiveWhiteID;

	Color	m_clrTextColor;
	Color	m_clrTextXColor;

	CPanelAnimationVarAliasType( float, clip_count_text_xpos, "clip_count_text_xpos", "54", "proportional_float" );
	CPanelAnimationVarAliasType( float, clip_count_text_ypos, "clip_count_text_ypos", "16", "proportional_float" );

	CPanelAnimationVarAliasType( float, large_icon_xpos, "large_icon_xpos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, large_icon_ypos, "large_icon_ypos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, large_icon_width, "large_icon_width", "32", "proportional_float" );
	CPanelAnimationVarAliasType( float, large_icon_height, "large_icon_height", "51", "proportional_float" );

	CPanelAnimationVarAliasType( float, small_icon_xpos, "small_icon_xpos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, small_icon_ypos, "small_icon_ypos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, small_icon_width, "small_icon_width", "2", "proportional_float" );
	CPanelAnimationVarAliasType( float, small_icon_height, "small_icon_height", "2", "proportional_float" );

	CPanelAnimationVarAliasType( float, extra_clip_xpos, "extra_clip_xpos", "35", "proportional_float" );
	CPanelAnimationVarAliasType( float, extra_clip_ypos, "extra_clip_ypos", "20", "proportional_float" );
	CPanelAnimationVarAliasType( float, extra_clip_width, "extra_clip_width", "16", "proportional_float" );
	CPanelAnimationVarAliasType( float, extra_clip_height, "extra_clip_height", "16", "proportional_float" );

	CPanelAnimationVar( vgui::HFont, m_hNumberFont, "NumberFont", "HudSelectionNumbers" );

	Color m_clrIcon;

	CHudTexture *m_pMGNumbers[10];
};

#endif // DOD_HUD_PLAYERSTATUS_AMMO_H

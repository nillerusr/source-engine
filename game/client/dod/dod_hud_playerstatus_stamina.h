//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_HUD_PLAYERSTATUS_STAMINA_H
#define DOD_HUD_PLAYERSTATUS_STAMINA_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Stamina progress bar
//-----------------------------------------------------------------------------
class CDoDHudStaminaProgressBar : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CDoDHudStaminaProgressBar, vgui::Panel );

public:
	CDoDHudStaminaProgressBar( vgui::Panel *parent, const char *name ) : vgui::Panel( parent, name ){}

	virtual void Paint();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	void SetPercentage( float flPercentage ){ m_flPercentage = flPercentage; }

private:

	float			m_flPercentage;

	Color			m_clrActive;
	Color			m_clrActiveLow;
	Color			m_clrInactive;
	Color			m_clrInactiveLow;

	CPanelAnimationVarAliasType( float, m_flSliceWidth, "slice_width", "5", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flSliceSpacer, "slice_spacer", "2", "proportional_float" );
	CPanelAnimationVar( float, m_flWarningLevel, "warning_level", "0.25" );
};

//-----------------------------------------------------------------------------
// Purpose: Stamina icon
//-----------------------------------------------------------------------------
class CDoDHudStaminaIcon : public vgui::ImagePanel
{
	DECLARE_CLASS_SIMPLE( CDoDHudStaminaIcon, vgui::ImagePanel );

public:
	CDoDHudStaminaIcon( vgui::Panel *parent, const char *name );

	virtual void Paint();
	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	void SetPercentage( float flPercentage ){ m_flPercentage = flPercentage; }

private:

	float			m_flPercentage;
	CHudTexture		*m_icon;

	CPanelAnimationVar( float, m_flWarningLevel, "warning_level", "0.25" );

	char			m_szIcon[128];

	Color			m_clrActive;
	Color			m_clrActiveLow;
};

//-----------------------------------------------------------------------------
// Purpose: Stamina panel
//-----------------------------------------------------------------------------
class CDoDHudStamina : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CDoDHudStamina, vgui::EditablePanel );

public:
	CDoDHudStamina( vgui::Panel *parent, const char *name );

	virtual void OnThink();
	virtual void OnScreenSizeChanged( int iOldWide, int iOldTall );

private:

	CDoDCutEditablePanel		*m_pBackground;
	CDoDHudStaminaIcon			*m_pIcon;
	CDoDHudStaminaProgressBar	*m_pProgressBar;
};	

#endif // DOD_HUD_PLAYERSTATUS_STAMINA_H
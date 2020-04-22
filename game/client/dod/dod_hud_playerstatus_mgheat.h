//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_HUD_PLAYERSTATUS_MGHEAT_H
#define DOD_HUD_PLAYERSTATUS_MGHEAT_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: MGHeat progress bar
//-----------------------------------------------------------------------------
class CDoDHudMGHeatProgressBar : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CDoDHudMGHeatProgressBar, vgui::Panel );

public:
	CDoDHudMGHeatProgressBar( vgui::Panel *parent, const char *name ) : vgui::Panel( parent, name ){}

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
	CPanelAnimationVar( float, m_flWarningLevel, "warning_level", "0.75" );
};

//-----------------------------------------------------------------------------
// Purpose: MGHeat icon
//-----------------------------------------------------------------------------
class CDoDHudMGHeatIcon : public vgui::ImagePanel
{
	DECLARE_CLASS_SIMPLE( CDoDHudMGHeatIcon, vgui::ImagePanel );

public:
	CDoDHudMGHeatIcon( vgui::Panel *parent, const char *name ) : vgui::ImagePanel( parent, name ){};

	void Init( void );
	virtual void Paint();
	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	void SetType( int nType );
	void SetPercentage( float flPercentage ){ m_flPercentage = flPercentage; }

private:

	float			m_flPercentage;
	CHudTexture		*m_icon;

	CPanelAnimationVar( float, m_flWarningLevel, "warning_level", "0.75" );

//	char			m_szIcon30cal[128];
	char			m_szIconMg42[128];

	int				m_nType;
	Color			m_clrActive;
	Color			m_clrActiveLow;
};

//-----------------------------------------------------------------------------
// Purpose: MGHeat panel
//-----------------------------------------------------------------------------
class CDoDHudMGHeat : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CDoDHudMGHeat, vgui::EditablePanel );

public:
	CDoDHudMGHeat( vgui::Panel *parent, const char *name );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnThink();
	virtual void SetVisible( bool state );

private:

	CDoDCutEditablePanel		*m_pBackground;
	CDoDHudMGHeatIcon			*m_pIcon;
	CDoDHudMGHeatProgressBar	*m_pProgressBar;
};	

#endif // DOD_HUD_PLAYERSTATUS_MGHEAT_H
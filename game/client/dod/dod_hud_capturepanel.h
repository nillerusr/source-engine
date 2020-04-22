//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_HUD_CAPTUREPANEL_H
#define DOD_HUD_CAPTUREPANEL_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Draws the progress bar
//-----------------------------------------------------------------------------
class CDoDCapturePanelProgressBar : public vgui::ImagePanel
{
public:
	DECLARE_CLASS_SIMPLE( CDoDCapturePanelProgressBar, vgui::ImagePanel );

	CDoDCapturePanelProgressBar( vgui::Panel *parent, const char *name );

	virtual void Paint();

	void SetPercentage( float flPercentage ){ m_flPercent = flPercentage; }

private:

	float	m_flPercent;
	int		m_iTexture;

	CPanelAnimationVar( Color, m_clrActive, "color_active", "HudCaptureProgressBar.Active" );
	CPanelAnimationVar( Color, m_clrInActive, "color_inactive", "HudCaptureProgressBar.InActive" );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDoDCapturePanelIcon : public vgui::ImagePanel
{
public:
	DECLARE_CLASS_SIMPLE( CDoDCapturePanelIcon, vgui::ImagePanel );

	CDoDCapturePanelIcon( vgui::Panel *parent, const char *name );

	virtual void Paint();

	void SetActive( bool state ){ m_bActive = state; }

private:

	bool	m_bActive;
	int		m_iTexture;

	CPanelAnimationVar( Color, m_clrActive, "color_active", "HudCaptureIcon.Active" );
	CPanelAnimationVar( Color, m_clrInActive, "color_inactive", "HudCaptureIcon.InActive" );
};

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDoDHudCapturePanel : public CHudElement, public vgui::EditablePanel
{
public:
	DECLARE_CLASS_SIMPLE( CDoDHudCapturePanel, vgui::EditablePanel );

	CDoDHudCapturePanel( const char *pElementName );

	virtual void OnThink();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnScreenSizeChanged( int iOldWide, int iOldTall );

private:
    
	CDoDCapturePanelProgressBar			*m_pProgressBar;

	CUtlVector<CDoDCapturePanelIcon *>	m_PlayerIcons;

	vgui::ImagePanel					*m_pAlliesFlag;
	vgui::ImagePanel					*m_pAxisFlag;
	vgui::ImagePanel					*m_pNeutralFlag;

	vgui::Label							*m_pMessage;

	vgui::Panel							*m_pBackground;

	CPanelAnimationVarAliasType( float, m_nSpaceBetweenIcons, "icon_space", "2", "proportional_float" );
};

#endif // DOD_HUD_CAPTUREPANEL_H
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_HUD_PLAYERSTATUS_HEALTH_H
#define DOD_HUD_PLAYERSTATUS_HEALTH_H
#ifdef _WIN32
#pragma once
#endif

class C_DODPlayer;

//-----------------------------------------------------------------------------
// Purpose: Health playerclass image (with red transparency)
//-----------------------------------------------------------------------------
class CDoDHudHealthBar : public vgui::ImagePanel
{
	DECLARE_CLASS_SIMPLE( CDoDHudHealthBar, vgui::ImagePanel );

public:
	CDoDHudHealthBar( vgui::Panel *parent, const char *name );
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	virtual void OnThink();
	virtual void Paint( void );

	void SetHealthDelegatePlayer( C_DODPlayer *pPlayer );
	C_DODPlayer *GetHealthDelegatePlayer( void );

private:
	float				m_flPercentage;

	int					m_iMaterialIndex;
	Color				m_clrHealthHigh;
	Color 				m_clrHealthMed;
	Color 				m_clrHealthLow;
	Color				m_clrBackground;
	Color				m_clrBorder;

	EHANDLE	m_hHealthDelegatePlayer;
	
	CPanelAnimationVar( float, m_flFirstWarningLevel, "FirstWarning", "0.50" );
	CPanelAnimationVar( float, m_flSecondWarningLevel, "SecondWarning", "0.25" );
};

//-----------------------------------------------------------------------------
// Purpose: Health panel
//-----------------------------------------------------------------------------
class CDoDHudHealth : public vgui::EditablePanel 
{
	DECLARE_CLASS_SIMPLE( CDoDHudHealth, vgui::EditablePanel );

public:
	CDoDHudHealth( vgui::Panel *parent, const char *name );

	virtual void OnThink();
	virtual void OnScreenSizeChanged(int iOldWide, int iOldTall);

	void SetHealthDelegatePlayer( C_DODPlayer *pPlayer );
	C_DODPlayer *GetHealthDelegatePlayer( void );

private:
	int					m_nPrevClass;		// used to store the player's class so we don't have to keep setting the image
	int					m_nPrevTeam;

	CDoDHudHealthBar	*m_pHealthBar;

	vgui::ImagePanel	*m_pClassImage;		// draws the class image and the red "damage taken" part
	vgui::ImagePanel	*m_pClassImageBG;	// draws the class image outline

	EHANDLE	m_hHealthDelegatePlayer;
};	

#endif // DOD_HUD_PLAYERSTATUS_HEALTH_H

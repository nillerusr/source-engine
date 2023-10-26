#ifndef _INCLUDED_ASW_HUD_USE_AREA_H
#define _INCLUDED_ASW_HUD_USE_AREA_H
#ifdef _WIN32
#pragma once
#endif

class CASWHudUseArea;
class CASW_HUD_Use_Icon;

class CASWHudCustomPaintPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASWHudCustomPaintPanel, public vgui::Panel );
public:
	CASWHudCustomPaintPanel( vgui::Panel* pParent, const char *pElementName );
	virtual void Paint();

	virtual void PerformLayout();

	EHANDLE m_hUsable;
	CASWHudUseArea *m_pHudParent;
};

//-----------------------------------------------------------------------------
// Purpose: Shows the icons to interact with things in the game world (pick up stuff, push buttons, etc)
//-----------------------------------------------------------------------------
class CASWHudUseArea : public vgui::Panel, public CASW_HudElement
{
	DECLARE_CLASS_SIMPLE( CASWHudUseArea, public vgui::Panel );

public:
	CASWHudUseArea( const char *pElementName );
	~CASWHudUseArea();
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void Paint();	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme);
	virtual bool ShouldDraw( void );
	virtual void FireGameEvent( IGameEvent *event );

	virtual void				LevelInit( void );
	virtual void				LevelShutdown( void );

	bool AddUseIconsFor(C_BaseEntity* pEnt);

	// given a texture and some text, this adds the icon to the HUD
	void AddUseIcon(int iUseIconTexture, const char *pText,
		EHANDLE UseTarget, float fProgress=-1);

	CPanelAnimationVar( vgui::HFont, m_hUseAreaFont, "UseAreaFont", "Default" );

	int m_iNumUseIcons;
	int m_iFrameWidth, m_iFrameHeight;
	//int m_iProgressBarTexture;
	void LoadUseTextures();
	int GetUseIconAlpha();

	CASWHudCustomPaintPanel *m_pCustomPaintPanel;
	CASW_HUD_Use_Icon *m_pUseIcon;
private:
};	

#endif // _INCLUDED_ASW_HUD_USE_AREA_H
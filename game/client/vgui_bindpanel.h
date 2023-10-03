#ifndef BINDING_PANEL_H
#define BINDING_PANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/panel.h>

//-----------------------------------------------------------------------------
// Purpose: Panel that displays the key/button for a particular binding
//-----------------------------------------------------------------------------
class CBindPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CBindPanel, vgui::Panel );

public:
	CBindPanel(Panel *parent, const char *panelName);
	virtual ~CBindPanel();

	virtual void PerformLayout();
	//virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint();
	virtual void OnThink();
	virtual void ApplySettings( KeyValues *inResourceData );

	const char *GetBind() { return m_szBind; }
	void SetBind( const char* szBind, int iSlot = -1 );

	void SetScale( float fScale );
	float GetScale() { return m_fScale; }

protected:
	void UpdateBackgroundImage();
	void UpdateKey();

	void DrawBackgroundIcon();
	void DrawBindingName();	
	
	int	 GetScreenWidthForCaption( const wchar *pString, vgui::HFont hFont );

private:
	char m_szBind[256];
	char m_szKey[256];
	char m_szLastKey[256];

	int m_iSlot;

	char m_szBackgroundTextureName[MAX_PATH];
	float m_fWidthScale;
	bool m_bController;
	bool m_bDrawKeyText;
	float m_fUpdateTime;

	CPanelAnimationVar( vgui::HFont, m_hKeysFont, "font", "InstructorKeyBindings" );
	CPanelAnimationVar( vgui::HFont, m_hButtonFont, "font", "InstructorButtons" );
	CPanelAnimationVar( float, m_fScale, "scale", "1.0" );
	CPanelAnimationVarAliasType( int, m_iIconTall, "icontall", "25", "proportional_int" );

	CHudTexture	*m_pBackground;
};

#endif // BINDING_PANEL_H
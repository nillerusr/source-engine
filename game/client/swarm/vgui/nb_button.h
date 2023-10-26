#ifndef _INCLUDED_NB_BUTTON_H
#define _INCLUDED_NB_BUTTON_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Button.h>

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
// == MANAGED_CLASS_DECLARATIONS_END ==

class CNB_Button : public vgui::Button
{
	DECLARE_CLASS_SIMPLE( CNB_Button, vgui::Button );
public:
	CNB_Button(Panel *parent, const char *panelName, const char *text, Panel *pActionSignalTarget=NULL, const char *pCmd=NULL);
	CNB_Button(Panel *parent, const char *panelName, const wchar_t *text, Panel *pActionSignalTarget=NULL, const char *pCmd=NULL);
	virtual ~CNB_Button();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );
	virtual void Paint();
	virtual void PaintBackground();
	virtual void OnCursorEntered();

	void DrawRoundedBox( int x, int y, int wide, int tall, Color color, float normalizedAlpha, bool bHighlightGradient, Color highlightCenterColor );
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	// == MANAGED_MEMBER_POINTERS_END ==

	CPanelAnimationVarAliasType( int, m_nNBBgTextureId1, "NBTexture1", "vgui/hud/800corner1", "textureid" );
	CPanelAnimationVarAliasType( int, m_nNBBgTextureId2, "NBTexture2", "vgui/hud/800corner2", "textureid" );
	CPanelAnimationVarAliasType( int, m_nNBBgTextureId3, "NBTexture3", "vgui/hud/800corner3", "textureid" );
	CPanelAnimationVarAliasType( int, m_nNBBgTextureId4, "NBTexture4", "vgui/hud/800corner4", "textureid" );
};

#endif // _INCLUDED_NB_BUTTON_H

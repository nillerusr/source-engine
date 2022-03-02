//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef CS_HUD_FREEZEPANEL_H
#define CS_HUD_FREEZEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/EditablePanel.h>
#include "vgui/ILocalize.h"
#include "vgui_avatarimage.h"
#include "hud.h"
#include "hudelement.h"
#include "cs_hud_playerhealth.h"

#include "cs_shareddefs.h"

using namespace vgui;

class HorizontalGauge;
class BorderedPanel;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCSFreezePanel : public EditablePanel, public CHudElement
{
private:
	DECLARE_CLASS_SIMPLE( CCSFreezePanel, EditablePanel );

public:
	CCSFreezePanel( const char *pElementName );

	virtual void Reset();
	virtual void Init();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void FireGameEvent( IGameEvent * event );
	virtual bool ShouldDraw();
	virtual void OnScreenSizeChanged(int nOldWide, int nOldTall);

	virtual void SetActive( bool bActive );

	void InitLayout();
	void Show();
	void Hide();

	int	HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

protected:

private:
	BorderedPanel*			m_pBackgroundPanel;
	HorizontalGauge*		m_pKillerHealth;
	CAvatarImagePanel*		m_pAvatar;
	ImagePanel*				m_pDominationIcon;

	bool					m_bShouldBeVisible;
};

#endif //CS_HUD_FREEZEPANEL_H
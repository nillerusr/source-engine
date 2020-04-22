//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Create and display a win panel at the end of a round displaying interesting stats and info about the round.
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSWINPANEL_ROUND_H
#define CSWINPANEL_ROUND_H
#ifdef _WIN32
#pragma once
#endif

#include "VGUI/bordered_panel.h"
#include <game/client/iviewport.h>
#include <vgui/IScheme.h>
#include "hud.h"
#include "hudelement.h"
#include "c_cs_player.h"
#include "vgui_avatarimage.h"

#include "cs_shareddefs.h"

using namespace vgui;

class WinPanel_Round : public BorderedPanel, public CHudElement
{
private:
	DECLARE_CLASS_SIMPLE( WinPanel_Round, BorderedPanel );

public:
	WinPanel_Round(const char *pElementName);
    ~WinPanel_Round();

	virtual void Reset();
	virtual void Init();
	virtual void VidInit();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void FireGameEvent( IGameEvent * event );
	virtual bool ShouldDraw( void );
	virtual void Paint( void ) {};
	virtual void OnScreenSizeChanged(int nOldWide, int nOldTall);

	virtual void OnThink();

	void InitLayout();
	void Show();
	void Hide();

protected:
	void SetMVP( C_CSPlayer* pPlayer, CSMvpReason_t reason );
	void SetFunFactLabel( const wchar *szFunFact );

private:
	bool m_bShowTimerDefend;
	bool m_bShowTimerAttack;

	bool m_bShouldBeVisible;

	// fade tracking
	bool m_bIsFading;
	float m_fFadeBeginTime;
};

#endif //CSWINPANEL_ROUND_H
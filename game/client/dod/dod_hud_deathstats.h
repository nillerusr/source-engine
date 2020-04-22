//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_HUD_DEATHSTATS_H
#define DOD_HUD_DEATHSTATS_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include <game/client/iviewport.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Label.h>
#include "hud.h"
#include "hudelement.h"
#include "dod_shareddefs.h"

using namespace vgui;

#define MAX_DEATHSTATS_NAME_LENGTH		128	// to hold multiple player cappers

// Player entries in a death notice
struct DeathStatsPlayer
{
	char		szName[MAX_DEATHSTATS_NAME_LENGTH];
	int			iEntIndex;
};

// Contents of each entry in our list of death notices
struct DeathStatsRecord
{
	DeathStatsPlayer	Killer;
	DeathStatsPlayer   Victim;
	CHudTexture *iconDeath;
	bool		bSuicide;
};

class CDODDeathStatsPanel : public EditablePanel, public CHudElement
{
private:
	DECLARE_CLASS_SIMPLE( CDODDeathStatsPanel, EditablePanel );

public:
	CDODDeathStatsPanel( const char *pElementName );

	virtual void Reset();
	virtual void Init();
	virtual void VidInit();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnScreenSizeChanged( int iOldWide, int iOldTall );

	virtual void FireGameEvent( IGameEvent * event);
	virtual void Paint( void );
	int DrawDeathNoticeItem( int xRight, int y );

	void Show() { SetAlpha( 255 ); }
	void Hide() { SetAlpha( 0 ); }

protected:

	vgui::Label *m_pSummaryLabel;
	vgui::Label *m_pAttackerHistoryLabel;

	DeathStatsRecord m_DeathRecord;

	int m_iMaterialTexture;

	// Special death notice icons
	CHudTexture		*m_iconD_skull;

	// Icons for stats
	CHudTexture		*m_pIconKill;		
	CHudTexture		*m_pIconWounded;
	CHudTexture		*m_pIconCap;
	CHudTexture		*m_pIconDefended;	


	CPanelAnimationVar( vgui::HFont, m_hTextFont, "TextFont", "HudNumbersTimer" );
	CPanelAnimationVarAliasType( float, m_flLineHeight, "LineHeight", "15", "proportional_float" );
	CPanelAnimationVar( Color, m_ActiveBackgroundColor, "ActiveBackgroundColor", "255 255 255 140" );

	CPanelAnimationVarAliasType( int, m_iDeathNoticeX, "DeathNoticeX", "5", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iDeathNoticeY, "DeathNoticeY", "5", "proportional_int" );
};

#endif //DOD_HUD_DEATHSTATS_H
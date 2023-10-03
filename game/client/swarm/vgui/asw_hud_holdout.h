#ifndef _INCLUDED_ASW_HUD_HOLDOUT_H
#define _INCLUDED_ASW_HUD_HOLDOUT_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_hudelement.h"
#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

class Holdout_Hud_Wave_Panel;

//-----------------------------------------------------------------------------
// Purpose: Shows holdout mode scores and messages
//-----------------------------------------------------------------------------
class CASW_Hud_Holdout : public CASW_HudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASW_Hud_Holdout, vgui::Panel );

public:
	CASW_Hud_Holdout( const char *pElementName );
	virtual ~CASW_Hud_Holdout();
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual bool ShouldDraw( void );

	void AnnounceNewWave( int nWave, float flDuration );
	void ShowWaveScores( int nWave, float flDuration );

	Holdout_Hud_Wave_Panel* m_pWavePanel;
};


#endif // _INCLUDED_ASW_HUD_HOLDOUT_H
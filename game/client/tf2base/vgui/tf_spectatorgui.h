//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef TF_SPECTATORGUI_H
#define TF_SPECTATORGUI_H
#ifdef _WIN32
#pragma once
#endif

#include <spectatorgui.h>
#include "hudelement.h"
#include "tf_hud_playerstatus.h"

//-----------------------------------------------------------------------------
// Purpose: Custom health panel used to show spectator target's health
//-----------------------------------------------------------------------------
class CTFSpectatorGUIHealth : public CTFHudPlayerHealth
{
public:
	CTFSpectatorGUIHealth( Panel *parent, const char *name ) : CTFHudPlayerHealth( parent, name )
	{
	}

	virtual const char *GetResFilename( void ) 
	{ 
		return "resource/UI/SpectatorGUIHealth.res"; 
	}
	virtual void OnThink()
	{
		// Do nothing. We're just preventing the base health panel from updating.
	}
};

//-----------------------------------------------------------------------------
// Purpose: TF Spectator UI
//-----------------------------------------------------------------------------
class CTFSpectatorGUI : public CSpectatorGUI
{
private:
	DECLARE_CLASS_SIMPLE( CTFSpectatorGUI, CSpectatorGUI );

public:
	CTFSpectatorGUI( IViewPort *pViewPort );
		
	virtual void Reset( void );
	virtual void PerformLayout( void );
	virtual void Update( void );
	virtual bool NeedsUpdate( void );
	virtual bool ShouldShowPlayerLabel( int specmode ) { return false; }
	void		 UpdateReinforcements( void );
	virtual void ShowPanel(bool bShow);
	virtual Color GetBlackBarColor( void ) { return Color(52,48,45, 255); }

	void		UpdateKeyLabels( void );
protected:	
	int		m_nLastSpecMode;
	CBaseEntity	*m_nLastSpecTarget;
	float	m_flNextTipChangeTime;		// time at which to next change the tip
	int		m_iTipClass;				// class that current tip is for

	// used to store the x and y position of the Engy and Spy build panels so we can reset them when the spec panel goes away
	int		m_nEngBuilds_xpos;
	int		m_nEngBuilds_ypos;
	int		m_nSpyBuilds_xpos;
	int		m_nSpyBuilds_ypos;

	vgui::Label				*m_pReinforcementsLabel;
	vgui::Label				*m_pClassOrTeamLabel;
	vgui::Label				*m_pSwitchCamModeKeyLabel;
	vgui::Label				*m_pCycleTargetFwdKeyLabel;
	vgui::Label				*m_pCycleTargetRevKeyLabel;
	vgui::Label				*m_pMapLabel;
};

#endif // TF_SPECTATORGUI_H

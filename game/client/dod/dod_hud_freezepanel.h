//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef DOD_HUD_FREEZEPANEL_H
#define DOD_HUD_FREEZEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include <game/client/iviewport.h>
#include <vgui/IScheme.h>
#include "hud.h"
#include "hudelement.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_avatarimage.h"
#include "dod_hud_playerstatus_health.h"

using namespace vgui;

bool IsTakingAFreezecamScreenshot( void );

/*
//-----------------------------------------------------------------------------
// Purpose: Custom health panel used in the freeze panel to show killer's health
//-----------------------------------------------------------------------------
class CDODFreezePanelHealth : public CTFHudPlayerHealth
{
public:
	CTFFreezePanelHealth( Panel *parent, const char *name ) : CTFHudPlayerHealth( parent, name )
	{
	}

	virtual const char *GetResFilename( void ) { return "resource/UI/FreezePanelKillerHealth.res"; }
	virtual void OnThink()
	{
		// Do nothing. We're just preventing the base health panel from updating.
	}
};
*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDODFreezePanelCallout : public EditablePanel
{
	DECLARE_CLASS_SIMPLE( CDODFreezePanelCallout, EditablePanel );
public:
	CDODFreezePanelCallout( Panel *parent, const char *name );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDODFreezePanel : public EditablePanel, public CHudElement
{
private:
	DECLARE_CLASS_SIMPLE( CDODFreezePanel, EditablePanel );

public:
	CDODFreezePanel( const char *pElementName );

	virtual void Reset();
	virtual void Init();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void FireGameEvent( IGameEvent * event );

	void ShowSnapshotPanel( bool bShow );
	void UpdateCallout( void );
	void ShowCalloutsIn( float flTime );
	void ShowSnapshotPanelIn( float flTime );
	void Show();
	void Hide();
	virtual bool ShouldDraw( void );
	void OnThink( void );

	int	HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

	bool IsHoldingAfterScreenShot( void ) { return m_bHoldingAfterScreenshot; }

protected:
	CDODFreezePanelCallout *TestAndAddCallout( Vector &origin, Vector &vMins, Vector &vMaxs, CUtlVector<Vector> *vecCalloutsTL, 
		CUtlVector<Vector> *vecCalloutsBR, Vector &vecFreezeTL, Vector &vecFreezeBR, Vector &vecStatTL, Vector &vecStatBR, int *iX, int *iY );

private:
	void ShowNemesisPanel( bool bShow );

	int						m_iYBase;
	int						m_iKillerIndex;
	//CTFHudPlayerHealth		*m_pKillerHealth;
	int						m_iShowNemesisPanel;
	CUtlVector<CDODFreezePanelCallout*>	m_pCalloutPanels;
	float					m_flShowCalloutsAt;
	float					m_flShowSnapshotReminderAt;
	EditablePanel			*m_pNemesisSubPanel;
	vgui::Label				*m_pFreezeLabel;
	vgui::Panel				*m_pFreezePanelBG;
	CAvatarImagePanel		*m_pAvatar;
	vgui::EditablePanel		*m_pScreenshotPanel;
	vgui::EditablePanel		*m_pBasePanel;

	int 					m_iBasePanelOriginalX;
	int 					m_iBasePanelOriginalY;

	bool					m_bHoldingAfterScreenshot;

	CDoDHudHealth	*m_pHealthStatus;

	enum 
	{
		SHOW_NO_NEMESIS = 0,
		SHOW_NEW_NEMESIS,
		SHOW_REVENGE
	};
};

#endif // DOD_HUD_FREEZEPANEL_H

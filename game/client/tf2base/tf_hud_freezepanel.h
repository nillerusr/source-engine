//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef TF_HUD_FREEZEPANEL_H
#define TF_HUD_FREEZEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include <game/client/iviewport.h>
#include <vgui/IScheme.h>
#include "hud.h"
#include "hudelement.h"
#include "tf_imagepanel.h"
#include "tf_hud_playerstatus.h"
#include "vgui_avatarimage.h"

using namespace vgui;

bool IsTakingAFreezecamScreenshot( void );

//-----------------------------------------------------------------------------
// Purpose: Custom health panel used in the freeze panel to show killer's health
//-----------------------------------------------------------------------------
class CTFFreezePanelHealth : public CTFHudPlayerHealth
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

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTFFreezePanelCallout : public EditablePanel
{
	DECLARE_CLASS_SIMPLE( CTFFreezePanelCallout, EditablePanel );
public:
	CTFFreezePanelCallout( Panel *parent, const char *name );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	void	UpdateForGib( int iGib, int iCount );
	void	UpdateForRagdoll( void );

private:
	vgui::Label	*m_pGibLabel;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTFFreezePanel : public EditablePanel, public CHudElement
{
private:
	DECLARE_CLASS_SIMPLE( CTFFreezePanel, EditablePanel );

public:
	CTFFreezePanel( const char *pElementName );

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
	CTFFreezePanelCallout *TestAndAddCallout( Vector &origin, Vector &vMins, Vector &vMaxs, CUtlVector<Vector> *vecCalloutsTL, 
		CUtlVector<Vector> *vecCalloutsBR, Vector &vecFreezeTL, Vector &vecFreezeBR, Vector &vecStatTL, Vector &vecStatBR, int *iX, int *iY );

private:
	void ShowNemesisPanel( bool bShow );

	int						m_iYBase;
	int						m_iKillerIndex;
	CTFHudPlayerHealth		*m_pKillerHealth;
	int						m_iShowNemesisPanel;
	CUtlVector<CTFFreezePanelCallout*>	m_pCalloutPanels;
	float					m_flShowCalloutsAt;
	float					m_flShowSnapshotReminderAt;
	EditablePanel			*m_pNemesisSubPanel;
	vgui::Label				*m_pFreezeLabel;
	CTFImagePanel			*m_pFreezePanelBG;
	CAvatarImagePanel		*m_pAvatar;
	vgui::EditablePanel		*m_pScreenshotPanel;
	vgui::EditablePanel		*m_pBasePanel;

	int 					m_iBasePanelOriginalX;
	int 					m_iBasePanelOriginalY;

	bool					m_bHoldingAfterScreenshot;

	enum 
	{
		SHOW_NO_NEMESIS = 0,
		SHOW_NEW_NEMESIS,
		SHOW_REVENGE
	};
};

#endif // TF_HUD_FREEZEPANEL_H

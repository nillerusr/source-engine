//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TFWINPANEL_H
#define TFWINPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include <game/client/iviewport.h>
#include <vgui/IScheme.h>
#include "hud.h"
#include "hudelement.h"

using namespace vgui;

#define NUM_NOTABLE_PLAYERS	3
#define NUM_CATEGORIES	2
#define NUM_ITEMS_PER_CATEGORY	3

class CTFWinPanel : public EditablePanel, public CHudElement
{
private:
	DECLARE_CLASS_SIMPLE( CTFWinPanel, EditablePanel );

public:
	CTFWinPanel( const char *pElementName );

	virtual void Reset();
	virtual void Init();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void FireGameEvent( IGameEvent * event );
	virtual void OnThink();
	virtual bool ShouldDraw( void );
	virtual void SetVisible( bool state );

	virtual int GetRenderGroupPriority() { return 70; }

private:
	EditablePanel *m_pTeamScorePanel;

	float	m_flTimeUpdateTeamScore;
	int		m_iBlueTeamScore;
	int		m_iRedTeamScore;

	bool	m_bShouldBeVisible;
};

#endif //TFWINPANEL_H
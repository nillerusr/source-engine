//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_IMAGEPANEL_H
#define TF_IMAGEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_shareddefs.h"
#include <vgui/IScheme.h>
#include <vgui_controls/ImagePanel.h>
#include "GameEventListener.h"

#define MAX_BG_LENGTH		128

class CTFImagePanel : public vgui::ImagePanel, public CGameEventListener
{
public:
	DECLARE_CLASS_SIMPLE( CTFImagePanel, vgui::ImagePanel );

	CTFImagePanel( vgui::Panel *parent, const char *name );

	virtual void ApplySettings( KeyValues *inResourceData );
	void UpdateBGImage( void );

	virtual Color GetDrawColor( void );

public: // IGameEventListener Interface
	virtual void FireGameEvent( IGameEvent * event );

public:
	char	m_szTeamBG[TF_TEAM_COUNT][MAX_BG_LENGTH];
	int		m_iBGTeam;
};


#endif // TF_IMAGEPANEL_H

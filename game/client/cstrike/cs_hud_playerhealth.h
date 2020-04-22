//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: To display the player's health with the use of one graphic over another.  A cross in this case
//			Currently this is only used on the freeze cam panel
//
// $NoKeywords: $
//=============================================================================//


#ifndef CS_HUD_HEALTHPANEL_H
#define CS_HUD_HEALTHPANEL_H
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
#include "hud.h"
#include "hudelement.h"

//-----------------------------------------------------------------------------
// Purpose:  Clips the health image to the appropriate percentage
//-----------------------------------------------------------------------------
class CCSHealthPanel : public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CCSHealthPanel, vgui::Panel );

	CCSHealthPanel( vgui::Panel *parent, const char *name );
	virtual void Paint();
	void SetHealth( float flHealth ){ m_flHealth = ( flHealth <= 1.0 ) ? flHealth : 1.0f; }

private:

	float	m_flHealth; // percentage from 0.0 -> 1.0
	int		m_iMaterialIndex;
	int		m_iDeadMaterialIndex;
};

//-----------------------------------------------------------------------------
// Purpose:  Displays player health data
//-----------------------------------------------------------------------------
class CCSHudPlayerHealth : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CCSHudPlayerHealth, EditablePanel );

public:

	CCSHudPlayerHealth( Panel *parent, const char *name );

	virtual const char *GetResFilename( void ) { return "resource/UI/FreezePanelKillerHealth.res"; }
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Reset();

	void	SetHealth( int iNewHealth, int iMaxHealth, int iMaxBuffedHealth );
	void	HideHealthBonusImage( void );

protected:
	//virtual void OnThink();

protected:
	float				m_flNextThink;

private:
	CCSHealthPanel		*m_pHealthImage;
	vgui::ImagePanel	*m_pHealthImageBG;

	int					m_nHealth;
	int					m_nMaxHealth;
};

#endif
//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_HUD_MENU_ENGY_DESTROY_H
#define TF_HUD_MENU_ENGY_DESTROY_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Label.h>

using namespace vgui;

#define ALL_BUILDINGS	-1

class CEngyDestroyMenuItem : public EditablePanel
{
	DECLARE_CLASS_SIMPLE( CEngyDestroyMenuItem, EditablePanel );

public:

	CEngyDestroyMenuItem( Panel *parent, const char *panelName ) : EditablePanel(parent, panelName) 
	{
	}

private:
};

class CHudMenuEngyDestroy : public CHudElement, public EditablePanel
{
	DECLARE_CLASS_SIMPLE( CHudMenuEngyDestroy, EditablePanel );

public:
	CHudMenuEngyDestroy( const char *pElementName );

	virtual void	LevelInit( void );
	virtual void	ApplySchemeSettings( IScheme *scheme );
	virtual bool	ShouldDraw( void );

	virtual void	SetVisible( bool state );

	int	HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

	//void RecalculateBuildingItemState( int iBuildingType = ALL_BUILDINGS );
	virtual void	OnTick( void );

	void ErrorSound( void );

	int MapIndexToObjectID( int index );

	virtual int GetRenderGroupPriority() { return 50; }

private:
	CEngyDestroyMenuItem *m_pActiveItems[4];
	CEngyDestroyMenuItem *m_pInactiveItems[4];
};

#endif	// TF_HUD_MENU_ENGY_DESTROY_H
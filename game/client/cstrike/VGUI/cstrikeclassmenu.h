//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSCLASSMENU_H
#define CSCLASSMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <classmenu.h>
#include <vgui_controls/EditablePanel.h>
#include <filesystem.h>
#include <cs_shareddefs.h>
#include "cbase.h"
#include "cs_gamerules.h"
#include "vgui_controls/ImagePanel.h"
#include "backgroundpanel.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// These are maintained in a list so the renderer can draw a 3D character
// model on top of them.
//-----------------------------------------------------------------------------

class CCSClassImagePanel : public vgui::ImagePanel
{
public:

	typedef vgui::ImagePanel BaseClass;

	CCSClassImagePanel( vgui::Panel *pParent, const char *pName );
	virtual ~CCSClassImagePanel();
	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void Paint();


public:
	char m_ModelName[128];
};

extern CUtlVector<CCSClassImagePanel*> g_ClassImagePanels;


//-----------------------------------------------------------------------------
// Purpose: Draws the Terrorist class menu
//-----------------------------------------------------------------------------

class CClassMenu_TER : public CClassMenu
{
private:
	DECLARE_CLASS_SIMPLE( CClassMenu_TER, CClassMenu );

	// Background panel -------------------------------------------------------

public:
	virtual void PaintBackground();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	bool m_backgroundLayoutFinished;

	// End background panel ---------------------------------------------------
	
public:
	CClassMenu_TER(IViewPort *pViewPort);
	virtual Panel* CreateControlByName(const char *controlName);
	const char *GetName( void );
	void ShowPanel(bool bShow);
	void Update();
	virtual void SetVisible(bool state);
};


//-----------------------------------------------------------------------------
// Purpose: Draws the Counter-Terrorist class menu
//-----------------------------------------------------------------------------

class CClassMenu_CT : public CClassMenu
{
private:
	DECLARE_CLASS_SIMPLE( CClassMenu_CT, CClassMenu );

	// Background panel -------------------------------------------------------

public:
	virtual void PaintBackground();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	bool m_backgroundLayoutFinished;

	// End background panel ---------------------------------------------------
	
public:
	CClassMenu_CT(IViewPort *pViewPort);
	virtual Panel *CreateControlByName(const char *controlName);
	const char *GetName( void );
	void ShowPanel(bool bShow);
	void Update();
	virtual void SetVisible(bool state);
};

#endif // CSCLASSMENU_H

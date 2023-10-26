//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#if !defined( IVIEWPORT_H )
#define IVIEWPORT_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>

#include "viewport_panel_names.h"

class KeyValues;

abstract_class IViewPortPanel
{
	
public:
	virtual	~IViewPortPanel() {};

	virtual const char *GetName( void ) = 0;// return identifer name
	virtual void SetData(KeyValues *data) = 0; // set ViewPortPanel data
	virtual void Reset( void ) = 0;		// clears internal state, deactivates it
	virtual void Update( void ) = 0;	// updates all (size, position, content, etc)
	virtual bool NeedsUpdate( void ) = 0; // query panel if content needs to be updated
	virtual bool HasInputElements( void ) = 0;	// true if panel contains elments which accepts input
	virtual void ReloadScheme( void ) {}
	virtual bool CanReplace( const char *panelName ) const { return true; } // returns true if this panel can appear on top of the given panel
	virtual bool CanBeReopened( void ) const { return true; } // returns true if this panel can be re-opened after being hidden by another panel

	virtual void ShowPanel( bool state ) = 0; // activate VGUI Frame
		
	// VGUI functions:
	virtual vgui::VPANEL GetVPanel( void ) = 0; // returns VGUI panel handle
	virtual bool IsVisible() = 0;  // true if panel is visible
	virtual void SetParent( vgui::VPANEL parent ) = 0;

	virtual bool WantsBackgroundBlurred( void ) = 0;
};

abstract_class IViewPort
{
public:
	virtual void UpdateAllPanels( void ) = 0;
	virtual void ShowPanel( const char *pName, bool state, KeyValues *data, bool autoDeleteData = true ) = 0;
	virtual void ShowPanel( const char *pName, bool state ) = 0;	
	virtual void ShowPanel( IViewPortPanel* pPanel, bool state ) = 0;	
	virtual void ShowBackGround(bool bShow) = 0;
	virtual IViewPortPanel* FindPanelByName(const char *szPanelName) = 0;
	virtual IViewPortPanel* GetActivePanel( void ) = 0;
	virtual void RecreatePanel( const char *szPanelName ) = 0;
	virtual void PostMessageToPanel( const char *pName, KeyValues *pKeyValues ) = 0;
};

extern IViewPort *GetViewPortInterface();
extern IViewPort *GetFullscreenViewPortInterface();

#endif // IVIEWPORT_H
//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef COMMENTARY_MODELVIEWER_H
#define COMMENTARY_MODELVIEWER_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <game/client/iviewport.h>

// FIXME!!!!!!
//#include "basemodelpanel.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

/*
// FIXME!!!!!!
// FIXME!!!!!!
// FIXME!!!!!!

This should be using the NEW model panels... "CBaseModelPanel" in basemodel_panel.h/.cpp
Need to comment this out so we can remove the dependence on the old CModelPanel from all but TF.
This will make it so people don't accidently use it or get confused by its similar structure when
working with the new class.

class CCommentaryModelPanel : public CModelPanel
{
public:
	DECLARE_CLASS_SIMPLE( CCommentaryModelPanel, CModelPanel );

	CCommentaryModelPanel( vgui::Panel *parent, const char *name );
};

*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCommentaryModelViewer : public vgui::Frame, public IViewPortPanel
{
	DECLARE_CLASS_SIMPLE( CCommentaryModelViewer, vgui::Frame );
public:
	CCommentaryModelViewer(IViewPort *pViewPort);
	virtual ~CCommentaryModelViewer();

	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	PerformLayout( void );
	virtual void	OnCommand( const char *command );
	virtual void	OnKeyCodePressed( vgui::KeyCode code );
	virtual void	OnThink( void );

	void			SetModel( const char *pszName, const char *pszAttached );

	void			HandleMovementInput( void );

	// IViewPortPanel
public:
	virtual const char *GetName( void ) { return PANEL_COMMENTARY_MODELVIEWER; }
	virtual void SetData(KeyValues *data) {};
	virtual void Reset() {};
	virtual void Update() {};
	virtual bool NeedsUpdate( void ) { return false; }
	virtual bool HasInputElements( void ) { return true; }
	virtual void ShowPanel( bool bShow );

	// both vgui::Frame and IViewPortPanel define these, so explicitly define them here as passthroughs to vgui
	vgui::VPANEL GetVPanel( void ) { return BaseClass::GetVPanel(); }
	virtual bool IsVisible() { return BaseClass::IsVisible(); }
	virtual void SetParent( vgui::VPANEL parent ) { BaseClass::SetParent( parent ); }
	virtual bool WantsBackgroundBlurred( void ) { return false; }

private:
	IViewPort				*m_pViewPort;

	// FIXME!!!!!!
	//CCommentaryModelPanel	*m_pModelPanel;

	Vector					m_vecResetPos;
	Vector					m_vecResetAngles;
	bool					m_bTranslating;
	float					m_flYawSpeed;
	float					m_flZoomSpeed;
};

#endif // COMMENTARY_MODELVIEWER_H

//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_HUD_FLAGSTATUS_H
#define TF_HUD_FLAGSTATUS_H
#ifdef _WIN32
#pragma once
#endif

#include "entity_capture_flag.h"
#include "tf_controls.h"
#include "tf_imagepanel.h"
#include "GameEventListener.h"

//-----------------------------------------------------------------------------
// Purpose:  Draws the rotated arrow panels
//-----------------------------------------------------------------------------
class CTFArrowPanel : public CTFImagePanel
{
public:
	DECLARE_CLASS_SIMPLE( CTFArrowPanel, CTFImagePanel );

	CTFArrowPanel( vgui::Panel *parent, const char *name );
	virtual void Paint();
	virtual bool IsVisible( void );
	void SetEntity( EHANDLE hEntity ){ m_hEntity = hEntity; }
	float GetAngleRotation( void );

private:

	EHANDLE				m_hEntity;	

	CMaterialReference	m_RedMaterial;
	CMaterialReference	m_BlueMaterial;
	CMaterialReference	m_NeutralMaterial;

	CMaterialReference	m_RedMaterialNoArrow;
	CMaterialReference	m_BlueMaterialNoArrow;
};

//-----------------------------------------------------------------------------
// Purpose:  
//-----------------------------------------------------------------------------
class CTFFlagStatus : public vgui::EditablePanel
{
public:
	DECLARE_CLASS_SIMPLE( CTFFlagStatus, vgui::EditablePanel );

	CTFFlagStatus( vgui::Panel *parent, const char *name );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual bool IsVisible( void );
	void UpdateStatus( void );

	void SetEntity( EHANDLE hEntity )
	{ 
		m_hEntity = hEntity;

		if ( m_pArrow )
		{
			m_pArrow->SetEntity( hEntity );
		}
	}

private:

	EHANDLE			m_hEntity;

	CTFArrowPanel	*m_pArrow;
	CTFImagePanel	*m_pStatusIcon;
	CTFImagePanel	*m_pBriefcase;
};


//-----------------------------------------------------------------------------
// Purpose:  
//-----------------------------------------------------------------------------
class CTFHudFlagObjectives : public vgui::EditablePanel, public CGameEventListener
{
	DECLARE_CLASS_SIMPLE( CTFHudFlagObjectives, vgui::EditablePanel );

public:

	CTFHudFlagObjectives( vgui::Panel *parent, const char *name );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual bool IsVisible( void );
	virtual void Reset();
	void OnTick();

public: // IGameEventListener:
	virtual void FireGameEvent( IGameEvent *event );

private:
	
	void UpdateStatus( void );
	void SetPlayingToLabelVisible( bool bVisible );

private:

	CTFImagePanel			*m_pCarriedImage;

	CTFLabel				*m_pPlayingTo;
	CTFImagePanel			*m_pPlayingToBG;

	CTFFlagStatus			*m_pRedFlag;
	CTFFlagStatus			*m_pBlueFlag;
	CTFArrowPanel			*m_pCapturePoint;

	bool					m_bFlagAnimationPlayed;
	bool					m_bCarryingFlag;

	vgui::ImagePanel		*m_pSpecCarriedImage;
};

#endif	// TF_HUD_FLAGSTATUS_H

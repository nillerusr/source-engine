//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef IUSESMORTARPANEL_H
#define IUSESMORTARPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "hud_minimap.h"

// Derive from this if your entity wants to use the mortar firing panel
class IUsesMortarPanel
{
public:
	// Get the data from this mortar needed by the panel
	virtual void	GetMortarData( float *flClientMortarYaw, bool *bAllowedToFire, float *flPower, float *flFiringPower, float *flFiringAccuracy, int *iFiringState ) = 0;

	// Tell the server the mortar's been rotated
	virtual void	SendYawCommand( void ) = 0;

	// Panel's overriding client yaw
	virtual void	ForceClientYawCountdown( float flTime ) = 0;

	// Start firing
	virtual void	ClickFire( void ) = 0;
};

// Mortar firing panel
class CMortarMinimapPanel : public CMinimapPanel
{
public:

	DECLARE_CLASS( CMortarMinimapPanel, CMinimapPanel );

	CMortarMinimapPanel( vgui::Panel *pParent, const char *pElementName );
	virtual ~CMortarMinimapPanel();
	
	void InitMortarMinimap( C_BaseEntity *pMortar );
	C_BaseEntity *GetMortar() const;

	virtual void Paint();
	
	void OnMousePressed( vgui::MouseCode code );
	void OnCursorMoved( int x, int y );
	void OnMouseReleased( vgui::MouseCode code );


public:

	EHANDLE		m_hMortar;
	BitmapImage m_MortarButtonUp;
	BitmapImage m_MortarButtonDown;
	BitmapImage m_MortarButtonCantFire;
	BitmapImage m_MortarDirectionImage;
	
	bool m_bMouseDown;
	bool m_bFireButtonDown;
	int m_LastX, m_LastY;
	
	// The red-black fade material for the slider.
	int m_nTextureId;
	int m_nTextureId_CantFire;
};

#endif // IUSESMORTARPANEL_H

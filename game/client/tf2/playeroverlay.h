//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#if !defined( PLAYEROVERLAY_H )
#define PLAYEROVERLAY_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>

#include "vgui_entitypanel.h"

class CHudPlayerOverlayName;
class CHudPlayerOverlayHealth;
class CHudPlayerOverlayClass;
class CHudPlayerOverlaySquad;
class CHudPlayerOverlaySelected;
class KeyValues;
class C_BaseTFPlayer;
//-----------------------------------------------------------------------------
// Purpose: Container for overlay elements
//-----------------------------------------------------------------------------

class CHudPlayerOverlay : public CEntityPanel
{
public:
	DECLARE_CLASS( CHudPlayerOverlay, CEntityPanel );

					CHudPlayerOverlay( vgui::Panel *parent, const char *panelName );
	virtual			~CHudPlayerOverlay( void );

	virtual void	Clear( void );

	virtual bool	Init( KeyValues* pInitData, C_BaseEntity* pEntity );
	virtual void	OnTick( );

	virtual void	Hide( void );

	virtual void	Paint();

	// Global fade amount
	float			GetAlphaFrac( void );
	void			SetColorLevel( vgui::Panel *panel, Color& fg, Color& bg );

private:
	CHudPlayerOverlayName		*m_pName;
	CHudPlayerOverlayHealth		*m_pHealth;
	CHudPlayerOverlayClass		*m_pClass;
	CHudPlayerOverlaySquad		*m_pSquad;
	CHudPlayerOverlaySelected	*m_pSelected;

	int m_OffsetX, m_OffsetY;
	int m_PlayerNum;

	Color				m_fgColor;
	Color				m_bgColor;

	// These are only associated with tf players
	CHandle<C_BaseTFPlayer>	m_hPlayer;
};


#endif // PLAYEROVERLAY_H
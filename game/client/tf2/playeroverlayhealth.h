//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( PLAYEROVERLAYHEALTH_H )
#define PLAYEROVERLAYHEALTH_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_basepanel.h"

class KeyValues;

class CHudPlayerOverlay;

class CHudPlayerOverlayHealth : public CBasePanel
{
public:
	DECLARE_CLASS( CHudPlayerOverlayHealth, CBasePanel );

	CHudPlayerOverlayHealth( CHudPlayerOverlay *baseOverlay );
	virtual ~CHudPlayerOverlayHealth( void );

	bool Init( KeyValues* pInitData );
	void SetHealth( float health );

	virtual void Paint( void );

	virtual void OnCursorEntered();
	virtual void OnCursorExited();

private:
	float	m_Health;

	Color m_fgColor;
	Color m_bgColor;

	CHudPlayerOverlay *m_pBaseOverlay;

};

#endif // PLAYEROVERLAYHEALTH_H
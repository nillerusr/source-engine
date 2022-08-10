//======= Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: CTF Flag Capture Zone.
//
//=============================================================================//
#ifndef FUNC_CAPTURE_ZONE_H
#define FUNC_CAPTURE_ZONE_H
#ifdef _WIN32
#pragma once
#endif

#include "triggers.h"

//=============================================================================
//
// CTF Flag Capture Zone class.
//
class CCaptureZone : public CBaseTrigger
{
	DECLARE_CLASS( CCaptureZone, CBaseTrigger );

public:
	DECLARE_SERVERCLASS();

	void	Spawn();
	void	Touch( CBaseEntity *pOther );

	bool	IsDisabled( void );
	void	SetDisabled( bool bDisabled );

	// Input handlers
	void	InputEnable( inputdata_t &inputdata );
	void	InputDisable( inputdata_t &inputdata );

	int		UpdateTransmitState( void );

private:

	bool			m_bDisabled;		// Enabled/Disabled?
	
	int				m_nCapturePoint;	// Used in non-CTF maps to identify this capture point

	COutputEvent	m_outputOnCapture;	// Fired a flag is captured on this point.

	DECLARE_DATADESC();

	float			m_flNextTouchingEnemyZoneWarning;	// don't spew warnings to the player who is touching the wrong cap
};

#endif // FUNC_CAPTURE_ZONE_H

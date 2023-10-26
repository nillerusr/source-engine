//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines the player specific data that is sent only to the player
//			to whom it belongs.
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_PLAYERLOCALDATA_H
#define C_PLAYERLOCALDATA_H
#ifdef _WIN32
#pragma once
#endif

#include "basetypes.h"
#include "mathlib/vector.h"
#include "playernet_vars.h"

#ifdef CLIENT_DLL
#define CPostProcessController C_PostProcessController
#define CColorCorrection C_ColorCorrection
#endif

class CPostProcessController;
class CColorCorrection;

//-----------------------------------------------------------------------------
// Purpose: Player specific data ( sent only to local player, too )
//-----------------------------------------------------------------------------
class CPlayerLocalData
{
public:
	DECLARE_PREDICTABLE();
	DECLARE_CLASS_NOBASE( CPlayerLocalData );
	DECLARE_EMBEDDED_NETWORKVAR();

	CPlayerLocalData() :
		m_iv_vecPunchAngle( "CPlayerLocalData::m_iv_vecPunchAngle" ),
		m_iv_vecPunchAngleVel( "CPlayerLocalData::m_iv_vecPunchAngleVel" )
	{
		m_iv_vecPunchAngle.Setup( &m_vecPunchAngle, LATCH_SIMULATION_VAR );
		m_iv_vecPunchAngleVel.Setup( &m_vecPunchAngleVel, LATCH_SIMULATION_VAR );
		m_flFOVRate = 0;
	}

	unsigned char			m_chAreaBits[MAX_AREA_STATE_BYTES];				// Area visibility flags.
	unsigned char			m_chAreaPortalBits[MAX_AREA_PORTAL_STATE_BYTES];// Area portal visibility flags.

// BEGIN PREDICTION DATA COMPACTION (these fields are together to allow for faster copying in prediction system)
	int						m_nStepside;
	int						m_nOldButtons;
	float					m_flFOVRate;		// rate at which the FOV changes

	int						m_iHideHUD;			// bitfields containing sections of the HUD to hide
	int						m_nDuckTimeMsecs;
	int						m_nDuckJumpTimeMsecs;
	int						m_nJumpTimeMsecs;

	float					m_flFallVelocity;
	float					m_flStepSize;

	CNetworkQAngle( m_vecPunchAngle );		// auto-decaying view angle adjustment
	CNetworkQAngle( m_vecPunchAngleVel );		// velocity of auto-decaying view angle adjustment

	bool					m_bDucked;
	bool					m_bDucking;
	bool					m_bInDuckJump;
	bool					m_bDrawViewmodel;
	bool					m_bWearingSuit;
	bool					m_bPoisoned;
	bool					m_bAllowAutoMovement;
// END PREDICTION DATA COMPACTION

	bool					m_bInLanding;
	float					m_flLandingTime;

	// Base velocity that was passed in to server physics so 
	//  client can predict conveyors correctly.  Server zeroes it, so we need to store here, too.
	Vector					m_vecClientBaseVelocity;  
	CInterpolatedVar< QAngle >	m_iv_vecPunchAngle;
	CInterpolatedVar< QAngle >	m_iv_vecPunchAngleVel;

	// Autoaim
	bool					m_bAutoAimTarget;

	// 3d skybox
	sky3dparams_t			m_skybox3d;
	// audio environment
	audioparams_t			m_audio;

	bool					m_bSlowMovement;

};

#endif // C_PLAYERLOCALDATA_H

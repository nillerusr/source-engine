//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASEPLAYER_SHARED_H
#define BASEPLAYER_SHARED_H
#ifdef _WIN32
#pragma once
#endif

// PlayerUse defines

#define	PLAYER_USE_RADIUS	80.f


#define CONE_45_DEGREES		0.707f
#define CONE_15_DEGREES		0.9659258f
#define CONE_90_DEGREES		0

#define TRAIN_ACTIVE	0x80 
#define TRAIN_NEW		0xc0
#define TRAIN_OFF		0x00
#define TRAIN_NEUTRAL	0x01
#define TRAIN_SLOW		0x02
#define TRAIN_MEDIUM	0x03
#define TRAIN_FAST		0x04 
#define TRAIN_BACK		0x05

// entity messages
#define PLAY_PLAYER_JINGLE	1
#define UPDATE_PLAYER_RADAR	2

#define DEATH_ANIMATION_TIME	3.0f

// multiplayer only
#define NOINTERP_PARITY_MAX			4
#define NOINTERP_PARITY_MAX_BITS	2

struct autoaim_params_t
{
	autoaim_params_t()
	{
		m_fScale = 0;
		m_fMaxDist = 0;
		m_fMaxDeflection = -1.0f;
		m_bOnTargetQueryOnly = false;
	}

	Vector		m_vecAutoAimDir;		// Output: The direction autoaim wishes to point.
	Vector		m_vecAutoAimPoint;		// Output: The point (world space) that autoaim is aiming at.
	EHANDLE		m_hAutoAimEntity;		// Output: The entity that autoaim is aiming at.
	float		m_fScale;				// Input:
	float		m_fMaxDist;				// Input:
	float		m_fMaxDeflection;		// Input:
	bool		m_bOnTargetQueryOnly;	// Input: Don't do expensive assistance, just resolve m_bOnTargetNatural
	bool		m_bAutoAimAssisting;	// Output: If this is true, autoaim is aiming at the target.
	bool		m_bOnTargetNatural;		// Output: If true, the player is on target without assistance.
};

enum stepsoundtimes_t
{
	STEPSOUNDTIME_NORMAL = 0,
	STEPSOUNDTIME_ON_LADDER,
	STEPSOUNDTIME_WATER_KNEE,
	STEPSOUNDTIME_WATER_FOOT,
};

// Shared header file for players
#if defined( CLIENT_DLL )
#define CBasePlayer C_BasePlayer
#include "c_baseplayer.h"
#else
#include "player.h"
#endif

#endif // BASEPLAYER_SHARED_H

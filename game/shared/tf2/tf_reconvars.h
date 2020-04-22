//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_RECONVARS_H
#define TF_RECONVARS_H
#pragma once


#include "techtree.h"


// Jetpack vars for each tech level.
class CReconJetpackLevel
{
public:
	float m_RechargeRate;	// How fast the jetpack recharges.
	float m_DepleteRate;	// How fast the jetpack depletes.
	float m_NegSnap;		// When the jetpack is fully depleted, it snaps to this so the pilot sputters.
	float m_MaxVerticalVel;	// Fastest you can go upwards.
	float m_AccelRate;		// How fast it accelerates.
};


extern CReconJetpackLevel g_ReconJetpackLevels[MAX_TF_TECHLEVELS];


#endif // TF_RECONVARS_H

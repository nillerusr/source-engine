//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_reconvars.h"

// FIXME: put these in a config file.
CReconJetpackLevel g_ReconJetpackLevels[MAX_TF_TECHLEVELS] =
{
	{
	0.35f,	// How fast the jetpack recharges.
	1.0f,	// How fast the jetpack depletes.
	-0.1f,	// When the jetpack is fully depleted, it snaps to this so the pilot sputters.
	150,	// Fastest you can go upwards.
	15		// How fast it accelerates.
	},

	{
	0.55f,	// How fast the jetpack recharges.
	0.75f,	// How fast the jetpack depletes.
	-0.1f,	// When the jetpack is fully depleted, it snaps to this so the pilot sputters.
	150,	// Fastest you can go upwards.
	15		// How fast it accelerates.
	},

	{
	0.55f,	// How fast the jetpack recharges.
	0.55f,	// How fast the jetpack depletes.
	-0.1f,	// When the jetpack is fully depleted, it snaps to this so the pilot sputters.
	200,	// Fastest you can go upwards.
	15		// How fast it accelerates.
	},

	{
	0.75f,	// How fast the jetpack recharges.
	0.35f,	// How fast the jetpack depletes.
	-0.1f,	// When the jetpack is fully depleted, it snaps to this so the pilot sputters.
	200,	// Fastest you can go upwards.
	15		// How fast it accelerates.
	},

	// Not used

	{
	0.7f,	// How fast the jetpack recharges.
	1.0f,	// How fast the jetpack depletes.
	-0.1f,	// When the jetpack is fully depleted, it snaps to this so the pilot sputters.
	130,	// Fastest you can go upwards.
	15		// How fast it accelerates.
	},

	{
	0.7f,	// How fast the jetpack recharges.
	1.0f,	// How fast the jetpack depletes.
	-0.1f,	// When the jetpack is fully depleted, it snaps to this so the pilot sputters.
	130,	// Fastest you can go upwards.
	15		// How fast it accelerates.
	},
};



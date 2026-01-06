//=============================================================================//
//
// Purpose:		Base husk values.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef NPC_HUSK_BASE_SHARED_H
#define NPC_HUSK_BASE_SHARED_H
#ifdef _WIN32
#pragma once
#endif

enum HuskAggressionLevel_t
{
	HUSK_AGGRESSION_LEVEL_CALM,			// Ignore would-be enemies
	HUSK_AGGRESSION_LEVEL_SUSPICIOUS,	// Stare/aim at an enemy which startled me or did something suspicious
	HUSK_AGGRESSION_LEVEL_ANGRY,		// Attack all enemies

	NUM_HUSK_AGGRESSION_LEVELS,
	LAST_HUSK_AGGRESSION_LEVEL = NUM_HUSK_AGGRESSION_LEVELS-1,
};

enum HuskCognitionFlags_t
{
	bits_HUSK_COGNITION_BLIND			= (1 << 0), // Can't see unless really close
	bits_HUSK_COGNITION_DEAF			= (1 << 1), // Can't hear sounds unless really close
	bits_HUSK_COGNITION_HYPERFOCUSED	= (1 << 2), // Short attention span (only chase one enemy)
	bits_HUSK_COGNITION_SHORT_MEMORY	= (1 << 3), // Forget about enemies quickly
	bits_HUSK_COGNITION_MISCOUNT_AMMO	= (1 << 4), // UNIMPLEMENTED: Have trouble keeping track of ammo
	bits_HUSK_COGNITION_BROKEN_RADIO	= (1 << 5), // Can't communicate with squad members I can't see (could perhaps use a broken vocoder in responses)
};

#endif // NPC_HUSK_BASE_H

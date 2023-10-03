//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
//=============================================================================//

#ifndef AI_HULL_H
#define AI_HULL_H
#pragma once

class Vector;
//=========================================================
// Link Properties. These hulls must correspond to the hulls
// in AI_Hull.cpp!
//=========================================================
enum Hull_t
{
	HULL_HUMAN,				// Combine, Stalker, Zombie...
	HULL_SMALL_CENTERED,	// Scanner
	HULL_WIDE_HUMAN,		// Vortigaunt
	HULL_TINY,				// Headcrab
	HULL_WIDE_SHORT,		// Bullsquid
	HULL_MEDIUM,			// Cremator
	HULL_TINY_CENTERED,		// Manhack 
	HULL_LARGE,				// Antlion Guard
	HULL_LARGE_CENTERED,	// Mortar Synth
//--DONT_DROP flag splits hulls here
	HULL_MEDIUM_TALL,		// Hunter
	HULL_TINY_FLUID,		// Blob
	HULL_MEDIUMBIG,			// Infested drone
//--------------------------------------------
	NUM_HULLS,
	HULL_NONE				// No Hull (appears after num hulls as we don't want to count it)
};

enum Hull_Bits_t
{
	bits_HUMAN_HULL				=	0x00000001,
	bits_SMALL_CENTERED_HULL	=	0x00000002,
	bits_WIDE_HUMAN_HULL		=	0x00000004,
	bits_TINY_HULL				=	0x00000008,
	bits_WIDE_SHORT_HULL		=	0x00000010,
	bits_MEDIUM_HULL			=	0x00000020,
	bits_TINY_CENTERED_HULL		=	0x00000040,
	bits_LARGE_HULL				=	0x00000080,
	bits_LARGE_CENTERED_HULL	=	0x00000100,
	bits_DONT_DROP_PLACEHOLDER	=	0x00000200,
	bits_MEDIUM_TALL_HULL		=	0x00000400,
	bits_TINY_FLUID_HULL		=	0x00000800,
	bits_MEDIUMBIG_HULL			=   0x00001000,
	bits_HULL_BITS_MASK			=	0x00001fff,		// infested change from 1ff to fff
};

inline int HullToBit( Hull_t hull )
{
	if ( hull < HULL_MEDIUM_TALL )
	{
		// Hull is before where don't drop flag splits the hull flags
		return ( 1 << hull );
	}
	else
	{
		// Skip over the extra flag taken by don't drop
		return ( 1 << ( hull + 1 ) );
	}
}



//=============================================================================
//	>> CAI_Hull
//=============================================================================
namespace NAI_Hull
{
	const Vector &Mins(int id);
	const Vector &Maxs(int id);

	const Vector &SmallMins(int id);
	const Vector &SmallMaxs(int id);

	float		Length(int id);
	float		Width(int id);
	float		Height(int id);

	int			Bits(int id);
 
	const char*	Name(int id);

	unsigned int TraceMask(int id);

	Hull_t		LookupId(const char *szName);
};

#endif // AI_HULL_H

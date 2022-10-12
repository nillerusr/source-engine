//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef SND_ENV_FX_H
#define SND_ENV_FX_H

#if defined( _WIN32 )
#pragma once
#endif

//=====================================================================
// FX presets
//=====================================================================

#define SXROOM_OFF			0		

#define SXROOM_GENERIC		1		// general, low reflective, diffuse room

#define SXROOM_METALIC_S	2		// highly reflective, parallel surfaces
#define SXROOM_METALIC_M	3
#define SXROOM_METALIC_L	4

#define SXROOM_TUNNEL_S		5		// resonant reflective, long surfaces
#define SXROOM_TUNNEL_M		6
#define SXROOM_TUNNEL_L		7

#define SXROOM_CHAMBER_S	8		// diffuse, moderately reflective surfaces
#define SXROOM_CHAMBER_M	9
#define SXROOM_CHAMBER_L	10

#define SXROOM_BRITE_S		11		// diffuse, highly reflective
#define SXROOM_BRITE_M		12
#define SXROOM_BRITE_L		13

#define SXROOM_WATER1		14		// underwater fx
#define SXROOM_WATER2		15
#define SXROOM_WATER3		16

#define SXROOM_CONCRETE_S	17		// bare, reflective, parallel surfaces
#define SXROOM_CONCRETE_M	18
#define SXROOM_CONCRETE_L	19

#define SXROOM_OUTSIDE1		20		// echoing, moderately reflective
#define SXROOM_OUTSIDE2		21		// echoing, dull
#define SXROOM_OUTSIDE3		22		// echoing, very dull

#define SXROOM_CAVERN_S		23		// large, echoing area
#define SXROOM_CAVERN_M		24
#define SXROOM_CAVERN_L		25

#define SXROOM_WEIRDO1		26		
#define SXROOM_WEIRDO2		27
#define SXROOM_WEIRDO3		28

#define CSXROOM				29

#endif // SND_ENV_FX_H

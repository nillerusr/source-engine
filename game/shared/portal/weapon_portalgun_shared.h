//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_PORTALGUN_SHARED_H
#define WEAPON_PORTALGUN_SHARED_H

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"

#ifdef CLIENT_DLL
#include "c_weapon_portalgun.h"
#else
#include "weapon_portalgun.h"
#endif

#define PORTALGUN_BEAM_SPRITE "sprites/grav_beam.vmt"
#define PORTALGUN_BEAM_SPRITE_NOZ "sprites/grav_beam_noz.vmt"
#define PORTALGUN_GLOW_SPRITE "sprites/glow04_noz"
#define PORTALGUN_ENDCAP_SPRITE "sprites/grav_flare"
#define PORTALGUN_GRAV_ACTIVE_GLOW "sprites/grav_light"
#define PORTALGUN_PORTAL1_FIRED_LAST_GLOW "sprites/bluelight"
#define PORTALGUN_PORTAL2_FIRED_LAST_GLOW "sprites/orangelight"
#define PORTALGUN_PORTAL_MUZZLE_GLOW_SPRITE "sprites/portalgun_effects"
#define PORTALGUN_PORTAL_TUBE_BEAM_SPRITE "sprites/portalgun_effects"

enum
{
	EFFECT_NONE,
	EFFECT_READY,
	EFFECT_HOLDING,
};

extern ConVar sk_auto_reload_time;

#endif // WEAPON_PORTALGUN_SHARED_H

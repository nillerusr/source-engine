#ifndef _INCLUDE_ASW_FX_SHARED_H
#define _INCLUDE_ASW_FX_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
#include "c_asw_marine.h"
#else
#include "asw_marine.h"
#endif

void UTIL_ASW_BloodImpact( const Vector &pos, const Vector &dir, int color, int amount );
void UTIL_ASW_BloodDrips( const Vector &origin, const Vector &direction, int color, int amount );
void UTIL_ASW_MarineTakeDamage( const Vector &origin, const Vector &direction, int color, int amount, CASW_Marine *pMarine, bool bFriendly = false );
void UTIL_ASW_DroneBleed( const Vector &pos, const Vector &dir, int amount );
void UTIL_ASW_EggGibs( const Vector &pos, int iFlags, int iEntIndex );
void UTIL_ASW_BuzzerDeath( const Vector &pos );
void UTIL_ASW_GrenadeExplosion( const Vector &vecPos, float flRadius );
void UTIL_ASW_EnvExplosionFX( const Vector &vecPos, float flRadius, bool bOnGround );

#endif // _INCLUDE_ASW_FX_SHARED_H
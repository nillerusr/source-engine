//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_SHIELDGRENADE_H
#define TF_SHIELDGRENADE_H
#pragma once


class CBaseEntity;
struct edict_t;

CBaseEntity *CreateShieldGrenade( const Vector& position, const QAngle &angles, 
	const Vector& velocity, const QAngle &angVelocity, CBaseEntity *pOwner, float timer );

#endif // TF_SHIELDGRENADE_H

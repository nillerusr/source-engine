//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ILAGCOMPENSATIONMANAGER_H
#define ILAGCOMPENSATIONMANAGER_H
#ifdef _WIN32
#pragma once
#endif

class CBasePlayer;
class CUserCmd;

enum LagCompensationType
{
	LAG_COMPENSATE_BOUNDS,
	LAG_COMPENSATE_HITBOXES,
	LAG_COMPENSATE_HITBOXES_ALONG_RAY,
};

//-----------------------------------------------------------------------------
// Purpose: This is also an IServerSystem
//-----------------------------------------------------------------------------
abstract_class ILagCompensationManager
{
public:
	// Called during player movement to set up/restore after lag compensation
	virtual void	StartLagCompensation(
		CBasePlayer *player,
		LagCompensationType lagCompensationType,
		const Vector& weaponPos = vec3_origin,
		const QAngle &weaponAngles = vec3_angle,
		float weaponRange = 0.0f ) = 0;
	virtual void	FinishLagCompensation( CBasePlayer *player ) = 0;

	// Mappers can flag certain additional entities to lag compensate, this handles them
	virtual void	AddAdditionalEntity( CBaseEntity *pEntity ) = 0;
	virtual void	RemoveAdditionalEntity( CBaseEntity *pEntity ) = 0;
};

extern ILagCompensationManager *lagcompensation;

#endif // ILAGCOMPENSATIONMANAGER_H

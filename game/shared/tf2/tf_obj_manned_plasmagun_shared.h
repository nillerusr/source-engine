//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A stationary gun that players can man
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_OBJ_MANNED_PLASMAGUN_SHARED_H
#define TF_OBJ_MANNED_PLASMAGUN_SHARED_H

#ifdef _WIN32
#pragma once
#endif

#include "basetfvehicle.h"

class CMoveData;

enum MovementStyle_t
{
	MOVEMENT_STYLE_STANDARD = 0,
	MOVEMENT_STYLE_BARREL_PIVOT = 1,
	MOVEMENT_STYLE_SIMPLE = 2,
};

struct MannedPlasmagunData_t : public VehicleBaseMoveData_t
{
	float	m_flGunYaw;
	float	m_flGunPitch;
	float	m_flBarrelPitch;
	float	m_flBarrelHeight;
	int		m_nBarrelPivotAttachment;
	int		m_nBarrelAttachment;
	int		m_nStandAttachment;
	MovementStyle_t	m_nMoveStyle;
};


class CObjectMannedPlasmagunMovement
{
public:
	void ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove );
	float GetMaxYaw() const;
	float GetMinYaw() const;
	float GetMaxPitch() const;
};



#endif // TF_OBJ_MANNED_PLASMAGUN_SHARED_H

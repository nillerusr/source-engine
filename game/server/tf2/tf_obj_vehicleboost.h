//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Upgrade that boosts vehicle speeds for short periods of time.
//
//=============================================================================//

#ifndef TF_OBJ_VEHICLEBOOST_H
#define TF_OBJ_VEHICLEBOOST_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_obj_baseupgrade_shared.h"

//=============================================================================
//
// Vehicle Boost Upgrade
//
class CObjectVehicleBoost : public CBaseObjectUpgrade
{

	DECLARE_CLASS( CObjectVehicleBoost, CBaseObjectUpgrade );

public:

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CObjectVehicleBoost();

	void	Spawn( void );
	void	Precache( void );
	bool	CanTakeEMPDamage( void ) { return true; }
	void	FinishedBuilding( void );
};

#endif // TF_OBJ_VEHICLEBOOST_H

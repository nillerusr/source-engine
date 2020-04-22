//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TF_VEHICLE_FLATBED_H
#define TF_VEHICLE_FLATBED_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_basefourwheelvehicle.h"
#include "vphysics/vehicles.h"


//-----------------------------------------------------------------------------
// Purpose: Flatbed truck that can carry multiple stationary objects
//-----------------------------------------------------------------------------
class CVehicleFlatbed: public CBaseTFFourWheelVehicle
{
	DECLARE_CLASS( CVehicleFlatbed, CBaseTFFourWheelVehicle );

public:
	DECLARE_SERVERCLASS();

	static CVehicleFlatbed *Create(const Vector &vOrigin, const QAngle &vAngles);

	CVehicleFlatbed();

	virtual void	Spawn();
	virtual void	Precache();
	virtual void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );

	// Vehicle overrides
	virtual bool	IsPassengerVisible( int nRole ) { return true; }
};

#endif // TF_VEHICLE_FLATBED_H

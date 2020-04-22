//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A stationary gun that players can man
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_VEHICLE_WAGON_H
#define TF_VEHICLE_WAGON_H

#ifdef _WIN32
#pragma once
#endif

#include "tf_basefourwheelvehicle.h"
#include "vphysics/vehicles.h"


// ------------------------------------------------------------------------ //
// A wagon that players can build one object on the back of
// ------------------------------------------------------------------------ //
class CVehicleWagon: public CBaseTFFourWheelVehicle
{
	DECLARE_CLASS( CVehicleWagon, CBaseTFFourWheelVehicle );

public:
	DECLARE_SERVERCLASS();

	static CVehicleWagon* Create(const Vector &vOrigin, const QAngle &vAngles);

					CVehicleWagon();

	virtual void	Spawn();
	virtual void	Precache();
	virtual void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	virtual bool	CanTakeEMPDamage( void ) { return true; }

	// Vehicle overrides
	virtual bool	IsPassengerVisible( int nRole ) { return true; }
};

#endif // TF_VEHICLE_WAGON_H

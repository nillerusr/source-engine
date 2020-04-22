//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_VEHICLE_TELEPORT_STATION_H
#define C_VEHICLE_TELEPORT_STATION_H
#ifdef _WIN32
#pragma once
#endif


#include "c_basefourwheelvehicle.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_VehicleTeleportStation : public C_BaseTFFourWheelVehicle
{
	DECLARE_CLASS( C_VehicleTeleportStation, C_BaseTFFourWheelVehicle );
public:
	DECLARE_CLIENTCLASS();

	C_VehicleTeleportStation();

private:
	C_VehicleTeleportStation( const C_VehicleTeleportStation & );		// not defined, not accessible
};


#endif // C_VEHICLE_TELEPORT_STATION_H

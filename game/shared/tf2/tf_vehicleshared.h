//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_VEHICLESHARED_H
#define TF_VEHICLESHARED_H
#ifdef _WIN32
#pragma once
#endif


enum VehicleModeDeploy_e
{
	VEHICLE_MODE_NORMAL = 0,
	VEHICLE_MODE_DEPLOYING,
	VEHICLE_MODE_UNDEPLOYING,
	VEHICLE_MODE_DEPLOYED
};
#define NUM_VEHICLE_DEPLOYMODE_BITS	2


// Attachment indices.
#define TANK_ATTACHMENT_TURRET_FIREPOS	1
#define TANK_ATTACHMENT_TURRET_BASE		2
#define TANK_ATTACHMENT_PLAYER_WAIST	3

// Tread indices.
enum TreadIndex
{
	TREAD_LEFT=0, 
	TREAD_RIGHT=1
};

// Tread states (send across the wire).
enum TreadState
{
	TREAD_NOTMOVING=0,
	TREAD_FORWARD=1,
	TREAD_BACKWARD=2
};



#endif // TF_VEHICLESHARED_H

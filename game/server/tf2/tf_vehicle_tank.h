//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TF_VEHICLE_TANK_H
#define TF_VEHICLE_TANK_H
#ifdef _WIN32
#pragma once
#endif


#include "tf_basefourwheelvehicle.h"
#include "vphysics/vehicles.h"


// ------------------------------------------------------------------------ //
// A wagon that players can build one object on the back of
// ------------------------------------------------------------------------ //
class CVehicleTank : public CBaseTFFourWheelVehicle
{
	DECLARE_CLASS( CVehicleTank, CBaseTFFourWheelVehicle );

public:
	DECLARE_SERVERCLASS();

	static CVehicleTank* Create(const Vector &vOrigin, const QAngle &vAngles);

	CVehicleTank();
	virtual ~CVehicleTank();

	virtual void	Spawn();
	virtual void	Precache();
	virtual void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	virtual bool	CanTakeEMPDamage( void ) { return true; }
	virtual bool	ClientCommand( CBaseTFPlayer *pPlayer, const CCommand &args );

	// Vehicle overrides
	virtual bool	IsPassengerVisible( int nRole ) { return true; }
	virtual bool	IsPassengerUsingStandardWeapons( int nRole );

protected:
	// Here's where we deal with weapons
	virtual void	OnItemPostFrame( CBaseTFPlayer *pDriver );


private:
	
	void TurretThink();
	void Fire();


	// These are the angles where the client currently wants the tank to look.
	// The server smoothly interpolates to these.
	// Pitch is 0 when facing straight ahead and 90 when facing straight up.
	// Yaw is 0 when facing straight ahead and 90 when facing to the tank's left.
	float m_flClientYaw;

	float m_flClientPitch;	// This is the pitch that the client sends - we use it directly.

	// This is the real yaw (which smoothly interpolates towards m_flClientYaw).
	CNetworkVar( float, m_flTurretYaw );
	CNetworkVar( float, m_flTurretPitch );

	// Tracks when we can fire the cannon.
	float m_flNextFireTime;
};



#endif // TF_VEHICLE_TANK_H

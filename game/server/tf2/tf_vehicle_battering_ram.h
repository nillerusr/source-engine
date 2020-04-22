//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A stationary gun that players can man
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_VEHICLE_BATTERING_RAM_H
#define TF_VEHICLE_BATTERING_RAM_H

#ifdef _WIN32
#pragma once
#endif

#include "tf_basefourwheelvehicle.h"
#include "vphysics/vehicles.h"


// ------------------------------------------------------------------------ //
// A stationary gun that players can man that's built by the player
// ------------------------------------------------------------------------ //
class CVehicleBatteringRam : public CBaseTFFourWheelVehicle
{
	DECLARE_CLASS( CVehicleBatteringRam, CBaseTFFourWheelVehicle );

public:
	DECLARE_SERVERCLASS();

	static CVehicleBatteringRam* Create(const Vector &vOrigin, const QAngle &vAngles);

					CVehicleBatteringRam();

	virtual void	Spawn();
	virtual void	Precache();
	virtual void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	virtual bool	CanTakeEMPDamage( void ) { return true; }

	void			OnItemPostFrame( CBaseTFPlayer *pDriver );

	// Can a given passenger take damage?
	virtual bool	IsPassengerDamagable( int nRole ) { return (nRole > 1); }

	// Vehicle overrides
	virtual bool	IsPassengerVisible( int nRole ) { return true; }

	// Does the player use his normal weapons while in this mode?
	virtual bool	IsPassengerUsingStandardWeapons( int nRole );

	// Used to bash things it touches
	virtual void	VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );

private:

	int		m_nBarrelAttachment;
	float	m_flNextBashTime;
};

#endif // TF_VEHICLE_BATTERING_RAM_H

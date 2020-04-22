//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A stationary gun that players can man
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_VEHICLE_MORTAR_H
#define TF_VEHICLE_MORTAR_H

#ifdef _WIN32
#pragma once
#endif

#include "tf_basefourwheelvehicle.h"
#include "vphysics/vehicles.h"


// ------------------------------------------------------------------------ //
// A wagon that players can build one object on the back of
// ------------------------------------------------------------------------ //
class CVehicleMortar : public CBaseTFFourWheelVehicle
{
	DECLARE_CLASS( CVehicleMortar, CBaseTFFourWheelVehicle );

public:
	DECLARE_SERVERCLASS();

	static CVehicleMortar* Create(const Vector &vOrigin, const QAngle &vAngles);

					CVehicleMortar();

	virtual void	Spawn();
	virtual void	Precache();
	virtual void	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	virtual bool	CanTakeEMPDamage( void ) { return true; }
	virtual bool	ClientCommand( CBaseTFPlayer *pPlayer, const CCommand &args );

	// Vehicle overrides
	virtual bool	IsPassengerVisible( int nRole ) { return true; }
	virtual void	OnFinishedDeploy( void );
	virtual void	OnFinishedUnDeploy( void );
	virtual void SetPassenger( int nRole, CBasePlayer *pEnt );

protected:
	virtual bool CanGetInVehicle( CBaseTFPlayer *pPlayer );

	// Here's where we deal with weapons
	virtual void	OnItemPostFrame( CBaseTFPlayer *pDriver );

	// Called when we are deployed.
	void ElevationThink();

	void UpdateElevation( const Vector &vecTargetVel );

	bool CalcFireInfo( 
		float flFiringPower, 
		float flFiringAccuracy, 
		bool bRangeUpgraded, 
		bool bAccuracyUpgraded,
		Vector &vStartPt,
		Vector &vecTargetVel,
		float &fallTime
		);

	bool FireMortar( float flFiringPower, float flFiringAccuracy, bool bRangeUpgraded, bool bAccuracyUpgraded );

private:
	void NextFireThink();
	
	// Start/end the attack
	void StartAttack();
	void FinishAttack();

	void AttackThink();

	int	m_nBuildPoint;

	CNetworkVar( float, m_flMortarYaw );
	CNetworkVar( float, m_flMortarPitch );	// Elevation angle, recalculated periodically.

	CNetworkVar( bool, m_bAllowedToFire );
};

#endif // TF_VEHICLE_MORTAR_H

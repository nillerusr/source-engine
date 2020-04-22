//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Teleport Station Vehicle
//
//=============================================================================//

#ifndef TF_VEHICLE_TELEPORT_STATION_H
#define TF_VEHICLE_TELEPORT_STATION_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_basefourwheelvehicle.h"
#include "vphysics/vehicles.h"
#include "Sprite.h"
#include "tf_func_mass_teleport.h"


//-----------------------------------------------------------------------------
// Purpose: Teleport Station Vehicle
//-----------------------------------------------------------------------------
class CVehicleTeleportStation : public CBaseTFFourWheelVehicle
{
	DECLARE_CLASS( CVehicleTeleportStation, CBaseTFFourWheelVehicle );

public:

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	static CVehicleTeleportStation *Create( const Vector &vOrigin, const QAngle &vAngles );

	CVehicleTeleportStation();
	virtual ~CVehicleTeleportStation();

	void				Spawn();
	void				Precache();
	void				UpdateOnRemove( void );
	void				GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	void				GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );
	bool				CanTakeEMPDamage( void ) { return true; }
	virtual float		GetMaxSnapDistance( int iPoint ) { return 192; }
	virtual void		FinishedBuilding( void );

	bool				ValidDeployPosition( void );
	void				OnFinishedDeploy( void );
	void OnFinishedUnDeploy( void );
	void				DoTeleport( void );
	void				PostTeleportThink( void );
	void				RemoveCornerSprites( void );

	// Vehicle overrides
	bool				IsPassengerVisible( int nRole ) { return true; }

	static int GetNumDeployedTeleportStations();
	static CVehicleTeleportStation* GetDeployedTeleportStation( int i );

	// Returns INVALID_MCV_ID if there are no deployed MCVs.
	static CVehicleTeleportStation* GetFirstDeployedMCV( int iTeam );


protected:

	// Here's where we deal with weapons
	void				OnItemPostFrame( CBaseTFPlayer *pDriver );

private:
	
	static CUtlVector<CVehicleTeleportStation*> s_DeployedTeleportStations;

	Vector				m_vecTeleporterMins;
	Vector				m_vecTeleporterMaxs;
	Vector				m_vecTeleporterCenter;

	CHandle<CFuncMassTeleport> m_hTeleportExit;
	CHandle<CSprite>	m_hTeleportCornerSprites[5];
};




#endif // TF_VEHICLE_TELEPORT_STATION_H

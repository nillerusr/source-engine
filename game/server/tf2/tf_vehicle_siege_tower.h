//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Siege Tower Vehicle
//
//=============================================================================//

#ifndef TF_VEHICLE_SIEGE_TOWER_H
#define TF_VEHICLE_SIEGE_TOWER_H

#ifdef _WIN32
#pragma once
#endif

#include "tf_basefourwheelvehicle.h"
#include "vphysics/vehicles.h"

//=============================================================================
//
// Siege Ladder (Object)
//
class CObjectSiegeLadder : public CBaseAnimating
{
	DECLARE_CLASS( CObjectSiegeLadder, CBaseAnimating );

public:

	DECLARE_SERVERCLASS();

	static CObjectSiegeLadder* Create( const Vector &vOrigin, const QAngle &vAngles, CBaseEntity *pParent );

	CObjectSiegeLadder();

	void	Spawn();
	void	Precache();
	virtual int	OnTakeDamage( const CTakeDamageInfo &info );

public:
	EHANDLE		m_hTower;
};

//=============================================================================
//
// Siege Platform (Object)
//
class CObjectSiegePlatform : public CBaseAnimating
{
	DECLARE_CLASS( CObjectSiegePlatform, CBaseAnimating );

public:

	DECLARE_SERVERCLASS();

	static CObjectSiegePlatform* Create( const Vector &vOrigin, const QAngle &vAngles, CBaseEntity *pParent );

	CObjectSiegePlatform();

	void	Spawn();
	void	Precache();
	virtual int	OnTakeDamage( const CTakeDamageInfo &info );

public:
	EHANDLE		m_hTower;
};

//=============================================================================
//
// Siege Tower Vehicle
//
class CVehicleSiegeTower : public CBaseTFFourWheelVehicle
{
	DECLARE_CLASS( CVehicleSiegeTower, CBaseTFFourWheelVehicle );

public:

	DECLARE_SERVERCLASS();

	static CVehicleSiegeTower* Create( const Vector &vOrigin, const QAngle &vAngles );

						CVehicleSiegeTower();

	void				Spawn();
	void				Precache();
	void				Killed(void );
	void				GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	bool				CanTakeEMPDamage( void ) { return true; }

	// Vehicle overrides
	bool				IsPassengerVisible( int nRole ) { return true; }

protected:

	// Here's where we deal with weapons
	void				OnItemPostFrame( CBaseTFPlayer *pDriver );

private:
	
	// Deploy.
	void				InternalDeploy( void );
	void				InternalUnDeploy( void );
	void				CreateLadder( const Vector &vecOrigin, const QAngle &vecAngles );
	void				DestroyLadder( void );

private:

	CHandle<CObjectSiegeLadder>		m_hLadder;
	CHandle<CObjectSiegePlatform>	m_hPlatform;
};

#endif // TF_VEHICLE_SIEGE_TOWER_H

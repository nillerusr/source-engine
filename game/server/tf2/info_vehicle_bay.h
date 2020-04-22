//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef INFO_VEHICLE_BAY_H
#define INFO_VEHICLE_BAY_H
#ifdef _WIN32
#pragma once
#endif

#include "ihasbuildpoints.h"
#include "vguiscreen.h"

//-----------------------------------------------------------------------------
// Purpose: Entity that provides a place to build a vehicle
//-----------------------------------------------------------------------------
class CInfoVehicleBay : public CBaseEntity, public IHasBuildPoints
{
	DECLARE_CLASS( CInfoVehicleBay, CBaseEntity );
public:
	DECLARE_DATADESC();

	void Spawn( void );

// IHasBuildPoints
public:
	virtual	int		GetNumBuildPoints( void ) const;
	virtual bool	CanBuildObjectOnBuildPoint( int iPoint, int iObjectType );
	virtual bool	GetBuildPoint( int iPoint, Vector &vecOrigin, QAngle &vecAngles );
	virtual int		GetBuildPointAttachmentIndex( int iPoint ) const;
	virtual void	SetObjectOnBuildPoint( int iPoint, CBaseObject *pObject );
	virtual int		GetNumObjectsOnMe( void );
	virtual CBaseEntity	*GetFirstObjectOnMe( void );
	virtual CBaseObject *GetObjectOfTypeOnMe( int iObjectType );
	virtual int		FindObjectOnBuildPoint( CBaseObject *pObject );
	virtual void	GetExitPoint( CBaseEntity *pPlayer, int iPoint, Vector *pAbsOrigin, QAngle *pAbsAngles );
	virtual void	RemoveAllObjects( void );
	virtual float	GetMaxSnapDistance( int iPoint ) { return 128; }
	virtual bool	ShouldCheckForMovement( void ) { return false; }
};

//-----------------------------------------------------------------------------
// Purpose: Vgui screen for vehicle buying
//-----------------------------------------------------------------------------
class CVGuiScreenVehicleBay : public CVGuiScreen
{
	DECLARE_CLASS( CVGuiScreenVehicleBay, CVGuiScreen );
public:
	DECLARE_DATADESC();

	virtual void Activate( void );

	void	BuildVehicle( CBaseTFPlayer *pPlayer, int iObjectType );
	void	FinishedBuildVehicle( CBaseObject *pObject );
	void	SetBuildPoint( Vector &vecOrigin, QAngle &vecAngles );

	void	BayThink( void );

private:
	bool		m_bBayIsClear;
	Vector		m_vecBuildPointOrigin;
	QAngle		m_vecBuildPointAngles;

	// Outputs
	COutputEvent	m_OnStartedBuild;
	COutputEvent	m_OnFinishedBuild;
	COutputEvent	m_OnReadyToBuildAgain;
};

#endif // INFO_VEHICLE_BAY_H

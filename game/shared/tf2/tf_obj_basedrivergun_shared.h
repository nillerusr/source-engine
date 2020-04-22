//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base class for object upgrading objects
//
//=============================================================================//

#ifndef TF_OBJ_BASEDRIVERGUN_H
#define TF_OBJ_BASEDRIVERGUN_H
#ifdef _WIN32
#pragma once
#endif

#if defined( CLIENT_DLL )
#define CBaseObjectDriverGun C_BaseObjectDriverGun
#endif

#include "tf_obj_baseupgrade_shared.h"

// ------------------------------------------------------------------------ 
// Base class for objects that, when built on a vehicle, become under control of the driver
// ------------------------------------------------------------------------ 
class CBaseObjectDriverGun : public CBaseObjectUpgrade
{
DECLARE_CLASS( CBaseObjectDriverGun, CBaseObjectUpgrade );

public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CBaseObjectDriverGun();

	virtual void	Spawn( void );
	virtual void	FinishedBuilding( void );

	// Firing
	virtual bool	CanFireNow( void ) { return false; }
	virtual void	Fire( CBaseTFPlayer *pDriver ) { return; }

	// Turning
	virtual void			SetTargetAngles( const QAngle &vecAngles );
	virtual const QAngle	&GetCurrentAngles( void );
	virtual Vector			GetFireOrigin( void );

	// HUD
	virtual void			DrawHudElements( void ) { return; }

#ifdef CLIENT_DLL
	// All predicted weapons need to implement and return true
	virtual bool	IsPredicted( void ) const
	{ 
		return true;
	}

	virtual bool	ShouldPredict( void );
#endif

protected:
	CNetworkQAngle( m_vecGunAngles );

private:
	CBaseObjectDriverGun( const CBaseObjectDriverGun & ); // not defined, not accessible
};

#endif // TF_OBJ_BASEDRIVERGUN_H

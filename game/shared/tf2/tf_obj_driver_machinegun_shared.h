//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Vehicle mounted machinegun that the driver controls
//
//=============================================================================//

#ifndef TF_OBJ_DRIVER_MACHINEGUN_H
#define TF_OBJ_DRIVER_MACHINEGUN_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_obj_basedrivergun_shared.h"

#if defined( CLIENT_DLL )
#define CObjectDriverMachinegun C_ObjectDriverMachinegun
#endif

// ------------------------------------------------------------------------ //
// Mounted machinegun
// ------------------------------------------------------------------------ //
class CObjectDriverMachinegun : public CBaseObjectDriverGun
{
DECLARE_CLASS( CObjectDriverMachinegun, CBaseObjectDriverGun );

public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CObjectDriverMachinegun();

	virtual void	Spawn();
	virtual void	Precache();

	// Firing
	virtual bool	CanFireNow( void );
	virtual void	Fire( CBaseTFPlayer *pDriver );

	// Turning
	virtual Vector	GetFireOrigin( void );
	virtual void	SetTargetAngles( const QAngle &vecAngles );

#if defined( CLIENT_DLL )
	void			GetBoneControllers( float controllers[MAXSTUDIOBONECTRLS] );
	virtual void	DrawHudElements( void );
#endif

	// Ammo
	void			RechargeThink( void );

private:
	float	m_flNextAttack;
	int		m_nBarrelAttachment;
	int		m_nAmmoType;
	CNetworkVar( int, m_nAmmoCount );
	int		m_nMaxAmmoCount;

private:
	CObjectDriverMachinegun( const CObjectDriverMachinegun & ); // not defined, not accessible
};

#endif // TF_OBJ_DRIVER_MACHINEGUN_H

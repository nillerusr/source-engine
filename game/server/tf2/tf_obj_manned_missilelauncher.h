//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TF_OBJ_MANNED_MISSILELAUNCHER_H
#define TF_OBJ_MANNED_MISSILELAUNCHER_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_obj_base_manned_gun.h"

// ------------------------------------------------------------------------ //
// A stationary gun that players can man that's built by the player
// ------------------------------------------------------------------------ //
class CObjectMannedMissileLauncher : public CObjectBaseMannedGun
{
	DECLARE_CLASS( CObjectMannedMissileLauncher, CObjectBaseMannedGun );

public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	static CObjectMannedMissileLauncher* Create(const Vector &vOrigin, const QAngle &vAngles);

	CObjectMannedMissileLauncher();

	virtual void	Spawn();
	virtual void	Precache();
	virtual void	FinishedBuilding( void );
	virtual void	SetupTeamModel( void );
	void			MissileRechargeThink( void );

#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	virtual void	PreDataUpdate( DataUpdateType_t updateType );
	virtual void	PostDataUpdate( DataUpdateType_t updateType );
#endif

protected:
	// Fire the weapon
	virtual void Fire( void );

	int		m_nPreviousTeam;
	int		m_nMaxAmmoCount;
};

#endif // TF_OBJ_MANNED_MISSILELAUNCHER_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A stationary gun that players can man
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_OBJ_MANNED_PLASMAGUN_H
#define TF_OBJ_MANNED_PLASMAGUN_H

#ifdef _WIN32
#pragma once
#endif

#include "tf_obj_base_manned_gun.h"

#if defined( CLIENT_DLL )

#define CObjectMannedPlasmagun C_ObjectMannedPlasmagun

#endif

// ------------------------------------------------------------------------ //
// A stationary gun that players can man that's built by the player
// ------------------------------------------------------------------------ //
class CObjectMannedPlasmagun : public CObjectBaseMannedGun
{
public:
#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_CLASS( CObjectMannedPlasmagun, CObjectBaseMannedGun );

	static CObjectMannedPlasmagun* Create(const Vector &vOrigin, const QAngle &vAngles);

					CObjectMannedPlasmagun();

	virtual void	Spawn();
	virtual void	Precache();
	virtual void	SetupTeamModel( void );
	virtual void	FinishedBuilding( void );

	// All predicted weapons need to implement and return true
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}


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
	// Think function
	void RechargeThink();

	// Fire the weapon
	virtual void Fire( void );
	
protected:
	int		m_nMaxAmmoCount;
	CNetworkVar( float, m_flNextIdleTime );

	// Handling for the multiple barrels
	int		m_nRightBarrelAttachment;
	CNetworkVar( bool, m_bFiringLeft );

private:
	CObjectMannedPlasmagun( const CObjectMannedPlasmagun& src );

	int		m_nPreviousTeam;
};


#endif // TF_OBJ_MANNED_PLASMAGUN_H

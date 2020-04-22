//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_LIMPETMINE_H
#define WEAPON_LIMPETMINE_H
#ifdef _WIN32
#pragma once
#endif

class CBeam;

#if defined( CLIENT_DLL )
#define CWeaponLimpetmine C_WeaponLimpetmine
#else
#include "env_laserdesignation.h"
#endif

//-----------------------------------------------------------------------------
// Purpose: Combination limpet mine & laser designator weapon
//-----------------------------------------------------------------------------
class CWeaponLimpetmine : public CBaseTFCombatWeapon
{
	DECLARE_CLASS( CWeaponLimpetmine, CBaseTFCombatWeapon );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponLimpetmine( void );

	virtual void	UpdateOnRemove( void );

	virtual void	Precache( void );
	virtual float	GetFireRate( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack( void );
	virtual void	WeaponIdle( void );
	virtual bool	HasAnyAmmo( void );
	virtual void	CleanupOnActStart( void );

	// Designation
	void			StartDesignating( void );
	void			StopDesignating( void );
	void			UpdateBeamTarget( );

	// Limpet counting
	void			DecrementLimpets( void );

	// All predicted weapons need to implement and return true
	virtual bool	IsPredicted( void ) const
	{ 
		return true;
	}

// Server DLL only
#if !defined( CLIENT_DLL )
	void			DetonateDeployedLimpets( void );
	void			RemoveDeployedLimpets( void );
	void			ThrowLimpet( CBasePlayer *pPlayer, Vector vecSrc, Vector vecAiming );
	void			ActivateBeam();
	void			DesignateSentriesToAttack( CBaseEntity *pEntity );
	bool			ValidDesignationTarget( CBaseEntity *pEntity );
	CBaseEntity		*GetDesignatedEntity( CBaseTFPlayer *pPlayer );	// Visually highlight those limpets in player's view cone

public:
	CHandle<CEnvLaserDesignation>	m_hLaserDesignation;
#endif

// Client DLL only
#if defined( CLIENT_DLL )
public:
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	virtual bool CanBeSelected( void );
	virtual void PreDataUpdate( DataUpdateType_t updateType );
	virtual void PostDataUpdate( DataUpdateType_t updateType );
#endif

protected:
	// Designation
	bool		m_bDesignating;
	CNetworkHandle( CBaseEntity, m_hDesignatedEntity );
	EHANDLE		m_hOldDesignatedEntity;
	int			m_eDesignatedEntityOriginalRenderFx;
	int			m_eDesignatedEntityOriginalRenderMode;

	// Beam
	CBeam		*m_pBeam;

	// Limpets
	float		m_flStartedLaunchingAt;
	CNetworkVar( int, m_iDeployedLimpets );
	int			m_flNextBuzzTime;

private:
	CWeaponLimpetmine( const CWeaponLimpetmine & );
};

#endif // WEAPON_LIMPETMINE_H

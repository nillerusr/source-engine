//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef TF_WEAPON_MECHANICAL_ARM_H
#define TF_WEAPON_MECHANICAL_ARM_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_gun.h"
#include "tf_shareddefs.h"
#include "tf_viewmodel.h"

#ifdef CLIENT_DLL
#define CTFMechanicalArm C_TFMechanicalArm
#endif


//=============================================================================
//
// Mechanical Arm class.
//
class CTFMechanicalArm : public CTFWeaponBaseGun
{
public:
	DECLARE_CLASS( CTFMechanicalArm, CTFWeaponBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CTFMechanicalArm();
	~CTFMechanicalArm();

	virtual void	Precache();

	virtual void	PrimaryAttack();
	virtual void	SecondaryAttack( void );
	virtual int		GetWeaponID( void ) const			{ return TF_WEAPON_MECHANICAL_ARM; }
	virtual int		GetCustomDamageType( ) const		{ return TF_DMG_CUSTOM_PLASMA; }

	virtual int		GetAmmoPerShot( void );

	virtual bool	UpdateBodygroups( CBaseCombatCharacter* pOwner, int iState );

#ifdef CLIENT_DLL
	virtual void	OnDataChanged( DataUpdateType_t updateType );
#endif // CLIENT_DLL

private:

	bool ShockAttack( void );
#ifdef GAME_DLL
	bool IsValidVictim( CTFPlayer *pOwner, CBaseEntity *pTarget );
	void ShockVictim( CTFPlayer *pOwner, CBaseEntity *pTarget );
#else
	void StopParticleBeam( );
	void UpdateParticleBeam();
	HPARTICLEFFECT m_pParticleBeamEffect;
	HPARTICLEFFECT m_pParticleBeamSpark;
	C_BaseEntity  *m_pEffectOwner;
#endif // CLIENT_DLL
};

#endif // TF_WEAPON_MECHANICAL_ARM_H

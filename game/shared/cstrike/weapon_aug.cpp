//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbasegun.h"


#if defined( CLIENT_DLL )

	#define CWeaponAug C_WeaponAug
	#include "c_cs_player.h"

#else

	#include "cs_player.h"

#endif


class CWeaponAug : public CWeaponCSBaseGun
{
public:
	DECLARE_CLASS( CWeaponAug, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponAug();

	virtual void SecondaryAttack();
	virtual void PrimaryAttack();

 	virtual float GetInaccuracy() const;
	virtual bool Reload();
	virtual bool Deploy();

	virtual CSWeaponID GetWeaponID( void ) const		{ return WEAPON_AUG; }

#ifdef CLIENT_DLL
	virtual bool	HideViewModelWhenZoomed( void ) { return false; }
#endif

private:

	void AUGFire( float flSpread, bool bZoomed );
	
	CWeaponAug( const CWeaponAug & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponAug, DT_WeaponAug )

BEGIN_NETWORK_TABLE( CWeaponAug, DT_WeaponAug )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponAug )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_aug, CWeaponAug );
PRECACHE_WEAPON_REGISTER( weapon_aug );



CWeaponAug::CWeaponAug()
{
}

void CWeaponAug::SecondaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( pPlayer->GetFOV() == pPlayer->GetDefaultFOV() )
	{
		pPlayer->SetFOV( pPlayer, 55, 0.2f );
		m_weaponMode = Secondary_Mode;
	}
	else if ( pPlayer->GetFOV() == 55 )
	{
		pPlayer->SetFOV( pPlayer, pPlayer->GetDefaultFOV(), 0.15f );
		m_weaponMode = Primary_Mode;
	}
	else 
	{
		pPlayer->SetFOV( pPlayer, pPlayer->GetDefaultFOV() );
		m_weaponMode = Primary_Mode;
	}

	m_flNextSecondaryAttack = gpGlobals->curtime + 0.3;
}


float CWeaponAug::GetInaccuracy() const
{
	if ( weapon_accuracy_model.GetInt() == 1 )
	{
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer )
			return 0.0f;
	
		if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
			return 0.035f + 0.4f * m_flAccuracy;
	
		else if ( pPlayer->GetAbsVelocity().Length2D() > 140 )
			return 0.035f + 0.07f * m_flAccuracy;
		else
			return 0.02f * m_flAccuracy;
	}
	else
		return BaseClass::GetInaccuracy();
}

void CWeaponAug::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	bool bZoomed = pPlayer->GetFOV() < pPlayer->GetDefaultFOV();

	float flCycleTime = GetCSWpnData().m_flCycleTime;

	if ( bZoomed )
		flCycleTime = 0.135f;

	if ( !CSBaseGunFire( flCycleTime, m_weaponMode ) )
		return;

	// CSBaseGunFire can kill us, forcing us to drop our weapon, if we shoot something that explodes
	pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( pPlayer->GetAbsVelocity().Length2D() > 5 )
		 pPlayer->KickBack ( 1, 0.45, 0.275, 0.05, 4, 2.5, 7 );
	
	else if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
		pPlayer->KickBack ( 1.25, 0.45, 0.22, 0.18, 5.5, 4, 5 );
	
	else if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )
		pPlayer->KickBack ( 0.575, 0.325, 0.2, 0.011, 3.25, 2, 8 );
	
	else
		pPlayer->KickBack ( 0.625, 0.375, 0.25, 0.0125, 3.5, 2.25, 8 );
}


bool CWeaponAug::Reload()
{
	m_weaponMode = Primary_Mode;
	return BaseClass::Reload();
}

bool CWeaponAug::Deploy()
{
	m_weaponMode = Primary_Mode;
	return BaseClass::Deploy();
}

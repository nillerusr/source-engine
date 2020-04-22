//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbasegun.h"


#if defined( CLIENT_DLL )

	#define CWeaponUMP45 C_WeaponUMP45
	#include "c_cs_player.h"

#else

	#include "cs_player.h"

#endif


class CWeaponUMP45 : public CWeaponCSBaseGun
{
public:
	DECLARE_CLASS( CWeaponUMP45, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponUMP45();

	virtual void Spawn();
	virtual void PrimaryAttack();
	virtual bool Deploy();
	virtual bool Reload();

 	virtual float GetInaccuracy() const;

	virtual CSWeaponID GetWeaponID( void ) const		{ return WEAPON_UMP45; }


private:

	CWeaponUMP45( const CWeaponUMP45 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponUMP45, DT_WeaponUMP45 )

BEGIN_NETWORK_TABLE( CWeaponUMP45, DT_WeaponUMP45 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponUMP45 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_ump45, CWeaponUMP45 );
PRECACHE_WEAPON_REGISTER( weapon_ump45 );



CWeaponUMP45::CWeaponUMP45()
{
}


void CWeaponUMP45::Spawn()
{
	BaseClass::Spawn();

	m_flAccuracy = 0.0;
}


bool CWeaponUMP45::Deploy()
{
	bool ret = BaseClass::Deploy();

	m_flAccuracy = 0.0;

	return ret;
}

bool CWeaponUMP45::Reload()
{
	bool ret = BaseClass::Reload();

	m_flAccuracy = 0.0;

	return ret;
}

float CWeaponUMP45::GetInaccuracy() const
{
	if ( weapon_accuracy_model.GetInt() == 1 )
	{
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer )
			return 0.0f;
	
		if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
			return 0.24f * m_flAccuracy;
		else
			return 0.04f * m_flAccuracy;
	}
	else
		return BaseClass::GetInaccuracy();
}

void CWeaponUMP45::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( !CSBaseGunFire( GetCSWpnData().m_flCycleTime, Primary_Mode ) )
		return;

	// CSBaseGunFire can kill us, forcing us to drop our weapon, if we shoot something that explodes
	pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	// Kick the gun based on the state of the player.
	if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
		pPlayer->KickBack (0.125, 0.65, 0.55, 0.0475, 5.5, 4, 10);
	else if (pPlayer->GetAbsVelocity().Length2D() > 5)
		pPlayer->KickBack (0.55, 0.3, 0.225, 0.03, 3.5, 2.5, 10);
	else if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )
		pPlayer->KickBack (0.25, 0.175, 0.125, 0.02, 2.25, 1.25, 10);
	else
		pPlayer->KickBack (0.275, 0.2, 0.15, 0.0225, 2.5, 1.5, 10);
}


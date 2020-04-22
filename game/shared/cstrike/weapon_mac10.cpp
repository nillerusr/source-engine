//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbasegun.h"


#if defined( CLIENT_DLL )

	#define CWeaponMAC10 C_WeaponMAC10
	#include "c_cs_player.h"

#else

	#include "cs_player.h"

#endif


class CWeaponMAC10 : public CWeaponCSBaseGun
{
public:
	DECLARE_CLASS( CWeaponMAC10, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponMAC10();

	virtual void Spawn();
	virtual void PrimaryAttack();
	virtual bool Deploy();
	virtual bool Reload();

 	virtual float GetInaccuracy() const;

	virtual CSWeaponID GetWeaponID( void ) const		{ return WEAPON_MAC10; }


private:
	CWeaponMAC10( const CWeaponMAC10 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponMAC10, DT_WeaponMAC10 )

BEGIN_NETWORK_TABLE( CWeaponMAC10, DT_WeaponMAC10 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponMAC10 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_mac10, CWeaponMAC10 );
PRECACHE_WEAPON_REGISTER( weapon_mac10 );



CWeaponMAC10::CWeaponMAC10()
{
}


void CWeaponMAC10::Spawn( )
{
	BaseClass::Spawn();

	m_flAccuracy = 0.15;
}


bool CWeaponMAC10::Deploy()
{
	bool ret = BaseClass::Deploy();
	
	m_flAccuracy = 0.15;

	return ret;
}

bool CWeaponMAC10::Reload()
{
	bool ret = BaseClass::Reload();
	
	m_flAccuracy = 0.15;

	return ret;
}


float CWeaponMAC10::GetInaccuracy() const
{
	if ( weapon_accuracy_model.GetInt() == 1 )
	{
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer )
			return 0.0f;

		if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
			return 0.375f * m_flAccuracy;
		else
			return 0.03f * m_flAccuracy;
	}
	else
		return BaseClass::GetInaccuracy();
}

void CWeaponMAC10::PrimaryAttack()
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

	if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )	// jumping
		pPlayer->KickBack (1.3, 0.55, 0.4, 0.05, 4.75, 3.75, 5);
	else if (pPlayer->GetAbsVelocity().Length2D() > 5)				// running
		pPlayer->KickBack (0.9, 0.45, 0.25, 0.035, 3.5, 2.75, 7);
	else if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )	// ducking
		pPlayer->KickBack (0.75, 0.4, 0.175, 0.03, 2.75, 2.5, 10);
	else														// standing
		pPlayer->KickBack (0.775, 0.425, 0.2, 0.03, 3, 2.75, 9);
}

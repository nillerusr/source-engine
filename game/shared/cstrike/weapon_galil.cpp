//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbasegun.h"


#if defined( CLIENT_DLL )

	#define CWeaponGalil C_WeaponGalil
	#include "c_cs_player.h"

#else

	#include "cs_player.h"

#endif


class CWeaponGalil : public CWeaponCSBaseGun
{
public:
	DECLARE_CLASS( CWeaponGalil, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponGalil();

	virtual void PrimaryAttack();

 	virtual float GetInaccuracy() const;

	virtual CSWeaponID GetWeaponID( void ) const		{ return WEAPON_GALIL; }

private:

	CWeaponGalil( const CWeaponGalil & );

	void GalilFire( float flSpread );

};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponGalil, DT_WeaponGalil )

BEGIN_NETWORK_TABLE( CWeaponGalil, DT_WeaponGalil )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponGalil )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_galil, CWeaponGalil );
PRECACHE_WEAPON_REGISTER( weapon_galil );



CWeaponGalil::CWeaponGalil()
{
}

float CWeaponGalil::GetInaccuracy() const
{
	if ( weapon_accuracy_model.GetInt() == 1 )
	{
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer )
			return 0.0f;
	
		if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
			return 0.04f + 0.3f * m_flAccuracy;
		else if (pPlayer->GetAbsVelocity().Length2D() > 140)
			return 0.04f + 0.07f * m_flAccuracy;
		else
			return 0.0375f * m_flAccuracy;
	}
	else
		return BaseClass::GetInaccuracy();
}

void CWeaponGalil::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	// don't fire underwater
	if (pPlayer->GetWaterLevel() == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.15;
		return;
	}
	
	if ( !CSBaseGunFire( GetCSWpnData().m_flCycleTime, Primary_Mode ) )
		return;

	// CSBaseGunFire can kill us, forcing us to drop our weapon, if we shoot something that explodes
	pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if (pPlayer->GetAbsVelocity().Length2D() > 5)
		pPlayer->KickBack (1.0, 0.45, 0.28, 0.045, 3.75, 3, 7);
	else if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
		pPlayer->KickBack (1.2, 0.5, 0.23, 0.15, 5.5, 3.5, 6);
	else if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )
		pPlayer->KickBack (0.6, 0.3, 0.2, 0.0125, 3.25, 2, 7);
	else
		pPlayer->KickBack (0.65, 0.35, 0.25, 0.015, 3.5, 2.25, 7);
}



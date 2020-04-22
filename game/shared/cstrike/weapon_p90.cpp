//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbasegun.h"


#if defined( CLIENT_DLL )

	#define CWeaponP90 C_WeaponP90
	#include "c_cs_player.h"

#else

	#include "cs_player.h"

#endif


class CWeaponP90 : public CWeaponCSBaseGun
{
public:
	DECLARE_CLASS( CWeaponP90, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponP90();

	virtual void PrimaryAttack();

 	virtual float GetInaccuracy() const;

	virtual CSWeaponID GetWeaponID( void ) const		{ return WEAPON_P90; }


private:
	CWeaponP90( const CWeaponP90 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponP90, DT_WeaponP90 )

BEGIN_NETWORK_TABLE( CWeaponP90, DT_WeaponP90 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponP90 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_p90, CWeaponP90 );
PRECACHE_WEAPON_REGISTER( weapon_p90 );



CWeaponP90::CWeaponP90()
{
}

float CWeaponP90::GetInaccuracy() const
{
	if ( weapon_accuracy_model.GetInt() == 1 )
	{
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer )
			return 0.0f;
	
		if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
			return 0.3f * m_flAccuracy;
		else if (pPlayer->GetAbsVelocity().Length2D() > 170)
			return 0.115f * m_flAccuracy;
		else
			return 0.045f * m_flAccuracy;
	}
	else
		return BaseClass::GetInaccuracy();
}

void CWeaponP90::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;
	
	if ( !CSBaseGunFire( GetCSWpnData().m_flCycleTime, Primary_Mode ) )
		return;

	// Kick the gun based on the state of the player.
	if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
		pPlayer->KickBack (0.9, 0.45, 0.35, 0.04, 5.25, 3.5, 4);
	else if (pPlayer->GetAbsVelocity().Length2D() > 5)
		pPlayer->KickBack (0.45, 0.3, 0.2, 0.0275, 4, 2.25, 7);
	else if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )
		pPlayer->KickBack (0.275, 0.2, 0.125, 0.02, 3, 1, 9);
	else
		pPlayer->KickBack (0.3, 0.225, 0.125, 0.02, 3.25, 1.25, 8);
}

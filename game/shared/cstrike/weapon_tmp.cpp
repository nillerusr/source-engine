//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbasegun.h"


#if defined( CLIENT_DLL )

	#define CWeaponTMP C_WeaponTMP
	#include "c_cs_player.h"

#else

	#include "cs_player.h"

#endif


class CWeaponTMP : public CWeaponCSBaseGun
{
public:
	DECLARE_CLASS( CWeaponTMP, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponTMP();

	virtual void PrimaryAttack();
	virtual CSWeaponID GetWeaponID( void ) const		{ return WEAPON_TMP; }
	virtual bool IsSilenced( void ) const				{ return true; }

 	virtual float GetInaccuracy() const;

private:

	CWeaponTMP( const CWeaponTMP & );

	void DoFireEffects( void );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponTMP, DT_WeaponTMP )

BEGIN_NETWORK_TABLE( CWeaponTMP, DT_WeaponTMP )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponTMP )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_tmp, CWeaponTMP );
PRECACHE_WEAPON_REGISTER( weapon_tmp );


CWeaponTMP::CWeaponTMP()
{
}


float CWeaponTMP::GetInaccuracy() const
{
	if ( weapon_accuracy_model.GetInt() == 1 )
	{
		CCSPlayer *pPlayer = GetPlayerOwner();
		if ( !pPlayer )
			return 0.0f;
	
		if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
			return 0.25f * m_flAccuracy;
		else
			return 0.03f * m_flAccuracy;
	}
	else
		return BaseClass::GetInaccuracy();
}

void CWeaponTMP::PrimaryAttack( void )
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

	if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
		pPlayer->KickBack (1.1, 0.5, 0.35, 0.045, 4.5, 3.5, 6);
	else if (pPlayer->GetAbsVelocity().Length2D() > 5)
		pPlayer->KickBack (0.8, 0.4, 0.2, 0.03, 3, 2.5, 7);
	else if ( FBitSet( pPlayer->GetFlags(), FL_DUCKING ) )
		pPlayer->KickBack (0.7, 0.35, 0.125, 0.025, 2.5, 2, 10);
	else
		pPlayer->KickBack (0.725, 0.375, 0.15, 0.025, 2.75, 2.25, 9);
}

void CWeaponTMP::DoFireEffects( void )
{
	// TMP is silenced, so do nothing
}

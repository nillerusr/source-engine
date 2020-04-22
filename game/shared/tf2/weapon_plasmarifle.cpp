//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Medic's plasma rifle
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "in_buttons.h"
#include "gamerules.h"
#include "ammodef.h"
#include "entitylist.h"
#include "plasmaprojectile.h"
#include "tf_shareddefs.h"
#include "basegrenade_shared.h"
#include "tf_basecombatweapon.h"

// Damage CVars
ConVar	weapon_plasmarifle_damage( "weapon_plasmarifle_damage","0", FCVAR_NONE, "Plasma Rifle maximum damage" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWeaponPlasmaRifle : public CTFMachineGun
{
	DECLARE_CLASS( CWeaponPlasmaRifle, CTFMachineGun );
public:
	virtual void	FireBullets( CBaseTFCombatWeapon *pWeapon, int cShots, const Vector &vecSrc, const Vector &vecDirShooting, const Vector &vecSpread, float flDistance, int iBulletType, int iTracerFreq);
	virtual const Vector& GetBulletSpread( void ); 
	virtual float	GetFireRate( void );
};

LINK_ENTITY_TO_CLASS( weapon_plasmarifle, CWeaponPlasmaRifle );
PRECACHE_WEAPON_REGISTER(weapon_plasmarifle);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPlasmaRifle::FireBullets( CBaseTFCombatWeapon *pWeapon, int cShots, const Vector &vecSrc, const Vector &vecDirShooting, const Vector &vecSpread, float flDistance, int iBulletType, int iTracerFreq )
{
	CBaseTFPlayer *pPlayer = static_cast< CBaseTFPlayer * >( GetOwner() );
	if ( !pPlayer )
		return;

	Vector vecForward;
	pPlayer->EyeVectors( &vecForward );
	Vector vecOrigin = pPlayer->EyePosition();
	trace_t tr;

	Vector vecTracerSrc = pWeapon->GetTracerSrc( (Vector&)vecSrc, (Vector&)vecDirShooting );

	// Fire the emp projectile
	CBasePlasmaProjectile *pPlasma = CBasePlasmaProjectile::Create( vecTracerSrc, vecDirShooting, DMG_ENERGYBEAM, pPlayer );
	pPlasma->SetDamage( weapon_plasmarifle_damage.GetFloat() );
	pPlasma->m_hOwner = pPlayer;
}

//-----------------------------------------------------------------------------
// Purpose: Get the accuracy derived from weapon and player, and return it
//-----------------------------------------------------------------------------
const Vector& CWeaponPlasmaRifle::GetBulletSpread( void )
{
	static Vector cone = VECTOR_CONE_4DEGREES;
	return cone;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponPlasmaRifle::GetFireRate( void )
{	
	return 0.2; 
}


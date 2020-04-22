//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base TF Combat weapon
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "animation.h"
#include "tf_player.h"
#include "tf_basecombatweapon.h"
#include "soundent.h"
#include "weapon_twohandedcontainer.h"
#include "tf_gamerules.h"
#include "tf_obj.h"

//====================================================================================================
// BASE TF MACHINEGUN
//====================================================================================================
IMPLEMENT_SERVERCLASS_ST(CTFMachineGun, DT_TFMachineGun )
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFMachineGun::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (!pPlayer)
		return;
	
	// Abort here to handle burst and auto fire modes
	if ( (GetMaxClip1() != -1 && m_iClip1 == 0) || (GetMaxClip1() == -1 && !pPlayer->GetAmmoCount(m_iPrimaryAmmoType) ) )
		return;

	pPlayer->DoMuzzleFlash();


	// To make the firing framerate independent, we may have to fire more than one bullet here on low-framerate systems, 
	// especially if the weapon we're firing has a really fast rate of fire.
	if ( GetSequence() != SelectWeightedSequence( ACT_VM_PRIMARYATTACK ))
	{
		m_flNextPrimaryAttack = gpGlobals->curtime;
	}
	int iBulletsToFire = 0;
	float fireRate = GetFireRate();

	while ( m_flNextPrimaryAttack <= gpGlobals->curtime )
	{
		// MUST call sound before removing a round from the clip of a CMachineGun
		WeaponSound(SINGLE, m_flNextPrimaryAttack);
		m_flNextPrimaryAttack = m_flNextPrimaryAttack + fireRate;
		iBulletsToFire++;
	}

	// Make sure we don't fire more than the amount in the clip, if this weapon uses clips
	if ( GetMaxClip1() != -1 )
	{
		if ( iBulletsToFire > m_iClip1 )
			iBulletsToFire = m_iClip1;
		m_iClip1 -= iBulletsToFire;
	}
	else
	{
		if ( iBulletsToFire > pPlayer->GetAmmoCount(m_iPrimaryAmmoType) )
			iBulletsToFire = pPlayer->GetAmmoCount(m_iPrimaryAmmoType);
		pPlayer->RemoveAmmo( iBulletsToFire, m_iPrimaryAmmoType );
	}

	// Not time to fire any bullets yet?
	if ( !iBulletsToFire )
		return;

	// Fire the bullets
	Vector vecSrc	 = pPlayer->Weapon_ShootPosition( );
	Vector vecAiming = pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	// Factor in the view kick
	AddViewKick();

	float range = m_pRangeCVar ? m_pRangeCVar->GetFloat() : 1024.0f;

	if ( !m_pRangeCVar )
	{
		Msg( "Weapon missing m_pRangeCVar!!!\n" );
	}

	FireBullets( this, iBulletsToFire, vecSrc, vecAiming, GetBulletSpread(), range, m_iPrimaryAmmoType, 2 );

	if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}

	PlayAttackAnimation( GetPrimaryAttackActivity() );

	// Register a muzzleflash for the AI
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );

	CheckRemoveDisguise();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const Vector& CTFMachineGun::GetBulletSpread( void )
{
	static Vector cone = VECTOR_CONE_3DEGREES;
	return cone;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFMachineGun::FireBullets( CBaseTFCombatWeapon *pWeapon, int cShots, const Vector &vecSrc, const Vector &vecDirShooting, const Vector &vecSpread, float flDistance, int iBulletType, int iTracerFreq)
{
	if ( CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)GetOwner() )
	{
		float damage = m_pDamageCVar ? m_pDamageCVar->GetFloat() : 1;

		if ( !m_pDamageCVar )
		{
			Msg( "Weapon missing m_pDamageCVar!!!!\n" );
		}

		TFGameRules()->FireBullets( CTakeDamageInfo( this, pPlayer, damage, DMG_BULLET ), cShots, vecSrc, vecDirShooting, vecSpread, flDistance, iBulletType, 4, entindex(), 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFMachineGun::GetFireRate( void )
{
	return 1.0;
}

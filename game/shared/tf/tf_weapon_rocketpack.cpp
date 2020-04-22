//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tf_weapon_rocketpack.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
// Server specific.
#else
#include "tf_player.h"
#include "tf_ammo_pack.h"
#include "ilagcompensationmanager.h"
#include "tf_gamerules.h"
#endif

// TFRocketPack --
IMPLEMENT_NETWORKCLASS_ALIASED( TFRocketPack, DT_TFWeaponRocketPack )

BEGIN_NETWORK_TABLE( CTFRocketPack, DT_TFWeaponRocketPack )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFRocketPack )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_rocketpack, CTFRocketPack );
PRECACHE_WEAPON_REGISTER( tf_weapon_rocketpack );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFRocketPack::CTFRocketPack()
{
	m_flRefireTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRocketPack::PreCache( void )
{
	PrecacheParticleSystem( "rocketbackblast" );

	PrecacheScriptSound( "Weapon_Liberty_Launcher.Single" );

	BaseClass::Precache();
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFRocketPack::CanFire( void )
{
	CTFPlayer *pOwner = ToTFPlayer( GetOwner() );
	if ( !pOwner )
		return false;

	if ( !TFGameRules()->IsMannVsMachineMode() )
		return false;

	if ( TFGameRules() && (TFGameRules()->State_Get() == GR_STATE_PREROUND) )
		return false;

	if ( pOwner->m_Shared.IsLoser() )
		return false;

	if ( pOwner->m_Shared.InCond( TF_COND_STUNNED ) )
		return false;

	if ( pOwner->IsTaunting() )
		return false;

	if ( m_flRefireTime > gpGlobals->curtime )
		return false;

	if ( pOwner->GetAmmoCount( TF_AMMO_GRENADES1 ) > 0)
		return true;

	return false;
}
#endif // GAME_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRocketPack::PrimaryAttack( void )
{
	CTFPlayer *pOwner = ToTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

	if ( !CanAttack() )
		return;

#ifdef GAME_DLL
	if ( !CanFire() )
		return;

	// Launch
	if ( !pOwner->m_Shared.InCond( TF_COND_ROCKETPACK ) )
	{
		pOwner->m_Shared.AddCond( TF_COND_ROCKETPACK );
		pOwner->m_Shared.StunPlayer( 0.5f, 1.0f, TF_STUN_MOVEMENT );
	}

	Vector vecDir;
	pOwner->EyeVectors( &vecDir );
	pOwner->SetAbsVelocity( vec3_origin );
	Vector vecFlightDir = -vecDir;
	VectorNormalize( vecFlightDir );
	float flForce = 450.f;

	const float flPushScale = ( pOwner->GetFlags() & FL_ONGROUND ) ? 1.2f : 1.8f;		// Greater force while airborne
	const float flVertPushScale = ( pOwner->GetFlags() & FL_ONGROUND ) ? 1.2f : 0.25f;	// Less vertical force while airborne
	Vector vecForce = vecFlightDir * -flForce * flPushScale;
	vecForce.z += 1.f * flForce * flVertPushScale;
	pOwner->RemoveFlag( FL_ONGROUND );

	pOwner->ApplyAbsVelocityImpulse( vecForce );
	pOwner->RemoveAmmo( m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_iAmmoPerShot, m_iPrimaryAmmoType );
	pOwner->EmitSound( "Weapon_Liberty_Launcher.Single" );

	m_flRefireTime = gpGlobals->curtime + 0.25f;
#endif // GAME_DLL

	StartEffectBarRegen();

	if ( pOwner->GetAmmoCount( TF_AMMO_GRENADES1 ) == 0 )
	{
		g_pGameRules->SwitchToNextBestWeapon( pOwner, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFRocketPack::Deploy( void )
{
	CTFPlayer *pOwner = ToTFPlayer( GetOwner() );
	if ( pOwner )
	{
		pOwner->m_Shared.SetRocketPackEquipped( true );
	}

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFRocketPack::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	CTFPlayer *pOwner = ToTFPlayer( GetOwner() );
	if ( pOwner )
	{
		pOwner->m_Shared.SetRocketPackEquipped( false );
	}

	return BaseClass::Holster( pSwitchingTo );
}


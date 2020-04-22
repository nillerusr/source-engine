//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Burst rifle & Shield combo
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "weapon_combatshield.h"
#include "weapon_combat_usedwithshieldbase.h"
#include "in_buttons.h"
#include "plasmaprojectile.h"
#include "IEffects.h"

#if defined( CLIENT_DLL )
#include "fx.h"

#define CWeaponCombatBurstRifle C_WeaponCombatBurstRifle

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// Damage CVars
ConVar	weapon_combat_burstrifle_damage( "weapon_combat_burstrifle_damage","10", FCVAR_REPLICATED, "Burst Rifle damage" );
ConVar	weapon_combat_burstrifle_range( "weapon_combat_burstrifle_range","500", FCVAR_REPLICATED, "Burst Rifle maximum range" );
ConVar	weapon_combat_burstrifle_ducking_mod( "weapon_combat_burstrifle_ducking_mod", "0.75", FCVAR_REPLICATED, "Burst Rifle ducking speed modifier" );

#define MAX_RIFLE_POWER					3.0
#define RIFLE_CHARGE_TIME				2.0
#define BURSTRIFLE_BOOSTED_FIRERATE		0.015f


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWeaponCombatBurstRifle : public CWeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( CWeaponCombatBurstRifle, CWeaponCombatUsedWithShieldBase );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponCombatBurstRifle( void );

	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack( void );
	virtual float 	GetFireRate( void );
	virtual float	GetDefaultAnimSpeed( void );
	virtual const Vector& GetBulletSpread( void );

	// All predicted weapons need to implement and return true
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}
private:
	CWeaponCombatBurstRifle( const CWeaponCombatBurstRifle & );

public:
#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwner() && 
			GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	void GetViewmodelBoneControllers( C_BaseViewModel *pViewModel, float controllers[MAXSTUDIOBONECTRLS]);
	void ViewModelDrawn( C_BaseViewModel *pViewModel );

private:

	void BoostedMuzzleFlash( C_BaseViewModel *pViewModel, const Vector &vecOrigin, const QAngle &angle, float flScale );

	struct model_t			*m_pSpriteBurstRifleFlash[5];

#endif
};

CWeaponCombatBurstRifle::CWeaponCombatBurstRifle( void )
{
	SetPredictionEligible( true );
}

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCombatBurstRifle, DT_WeaponCombatBurstRifle )

BEGIN_NETWORK_TABLE( CWeaponCombatBurstRifle, DT_WeaponCombatBurstRifle )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponCombatBurstRifle )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_combat_burstrifle, CWeaponCombatBurstRifle );
PRECACHE_WEAPON_REGISTER(weapon_combat_burstrifle);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatBurstRifle::ItemPostFrame( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if (!pOwner)
		return;

	if ( UsesClipsForAmmo1() )
	{
		CheckReload();
	}

	// Handle firing
	if ( GetShieldState() == SS_DOWN && !m_bInReload )
	{
		if ( (pOwner->m_nButtons & IN_ATTACK ) && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
		{
			if ( m_iClip1 > 0 )
			{
				// Fire the plasma shot
				PrimaryAttack();
			}
			else
			{
				Reload();
			}
		}

		// Reload button (or fire button when we're out of ammo)
		if ( m_flNextPrimaryAttack <= gpGlobals->curtime ) 
		{
			if ( pOwner->m_nButtons & IN_RELOAD ) 
			{
				Reload();
			}
			else if ( !((pOwner->m_nButtons & IN_ATTACK) || (pOwner->m_nButtons & IN_ATTACK2) || (pOwner->m_nButtons & IN_RELOAD)) )
			{
				if ( !m_iClip1 && HasPrimaryAmmo() )
				{
					Reload();
				}
			}
		}
	}

	// Prevent shield post frame if we're not ready to attack, or we're charging
	AllowShieldPostFrame( m_flNextPrimaryAttack <= gpGlobals->curtime || m_bInReload );
}

//-----------------------------------------------------------------------------
// Purpose: Get the accuracy derived from weapon and player, and return it
//-----------------------------------------------------------------------------
const Vector& CWeaponCombatBurstRifle::GetBulletSpread( void )
{
	static Vector cone = VECTOR_CONE_5DEGREES;
	return cone;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatBurstRifle::PrimaryAttack( void )
{
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)GetOwner();
	if (!pPlayer)
		return;
	
	WeaponSound(SINGLE);

	// Fire the bullets
	Vector vecSrc = pPlayer->Weapon_ShootPosition( );
	Vector vecSpread = GetBulletSpread();
	Vector vecAiming, vecRight, vecUp;
	pPlayer->EyeVectors( &vecAiming, &vecRight, &vecUp );

	// Add some inaccuracy
	int seed = 0;
	float x, y, z;
	do 
	{
		float x1, x2, y1, y2;

		// Note the additional seed because otherwise we get the same set of random #'s and will get stuck
		//  in an infinite loop here potentially
		// FIXME:  Can we use a gaussian random # function instead?  ywb
		x1 = SHARED_RANDOMFLOAT_SEED( -0.5, 0.5, ++seed );
		x2 = SHARED_RANDOMFLOAT_SEED( -0.5, 0.5, ++seed );
		y1 = SHARED_RANDOMFLOAT_SEED( -0.5, 0.5, ++seed );
		y2 = SHARED_RANDOMFLOAT_SEED( -0.5, 0.5, ++seed );

		x = x1 + x2;
		y = y1 + y2;

		z = x*x+y*y;
	} while (z > 1);
	Vector vecDir = vecAiming + x * vecSpread.x * vecRight + y * vecSpread.y * vecUp;

	PlayAttackAnimation( GetPrimaryAttackActivity() );

	// Shift it down a bit so the firer can see it
	Vector right, forward;
	AngleVectors( pPlayer->EyeAngles() + pPlayer->m_Local.m_vecPunchAngle, &forward, &right, NULL );
	Vector vecStartSpot = vecSrc;

	// Get the firing position
#ifdef CLIENT_DLL
	// On our client, grab the viewmodel's firing position
	Vector vecWorldOffset = vecStartSpot + Vector(0,0,-8) + right * 12 + forward * 16;
#else
	// For everyone else, grab the weapon model's position
	/*
	Vector vecWorldOffset;
	QAngle angIgnore;
	GetAttachment( LookupAttachment( "muzzle" ), vecWorldOffset, angIgnore );
	*/

	Vector vecWorldOffset = vecStartSpot + Vector(0,0,-8) + right * 12 + forward * 16;
#endif
	Vector gunOffset = vecWorldOffset - vecStartSpot;

	CPowerPlasmaProjectile *pPlasma = CPowerPlasmaProjectile::CreatePredicted( vecStartSpot, vecDir, gunOffset, DMG_ENERGYBEAM, pPlayer );
	if ( pPlasma )
	{
		pPlasma->SetDamage( weapon_combat_burstrifle_damage.GetFloat() );
		pPlasma->m_hOwner = pPlayer;
		pPlasma->SetPower( 2.0 );
		pPlasma->SetMaxRange( weapon_combat_burstrifle_range.GetFloat() );
		pPlasma->Activate();
	}

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_iClip1 = m_iClip1 - 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponCombatBurstRifle::GetFireRate( void )
{	
	if ( !inv_demo.GetFloat() )
	{
		float flFireRate = ( SequenceDuration() * 0.6f ) + SHARED_RANDOMFLOAT( 0.0, 0.035f );

		CBaseTFPlayer *pPlayer = static_cast<CBaseTFPlayer*>( GetOwner() );
		if ( pPlayer )
		{
			// Ducking players should fire more rapidly.
			if ( pPlayer->GetFlags() & FL_DUCKING )
			{
				flFireRate *= weapon_combat_burstrifle_ducking_mod.GetFloat();
			}
		}
		
		return flFireRate;
	}

	// Get the player and check to see if we are powered up.
	CBaseTFPlayer *pPlayer = ( CBaseTFPlayer* )GetOwner();
	if ( pPlayer && pPlayer->HasPowerup( POWERUP_BOOST ) )
	{
		return BURSTRIFLE_BOOSTED_FIRERATE;
	}

	return SHARED_RANDOMFLOAT( 0.075f, 0.15f );
}

//-----------------------------------------------------------------------------
// Purpose: Match the anim speed to the weapon speed while crouching
//-----------------------------------------------------------------------------
float CWeaponCombatBurstRifle::GetDefaultAnimSpeed( void )
{
	if ( GetOwner() && GetOwner()->IsPlayer() )
	{
		if ( GetOwner()->GetFlags() & FL_DUCKING )
			return (1.0 + (1.0 - weapon_combat_burstrifle_ducking_mod.GetFloat()) );
	}

	return 1.0;
}

#if defined ( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatBurstRifle::GetViewmodelBoneControllers( C_BaseViewModel *pViewModel, 
														   float controllers[MAXSTUDIOBONECTRLS])
{
	float flAmmoCount;
	C_BaseTFPlayer *pPlayer = ( C_BaseTFPlayer* )GetOwner();
	if ( pPlayer && pPlayer->IsDamageBoosted() )
	{
		flAmmoCount = random->RandomFloat( 0.0f, 1.0f );
	}
	else
	{
		// Dial shows ammo count!
		flAmmoCount = ( float )m_iClip1 / ( float )GetMaxClip1();
	}

	// Add some shake
	flAmmoCount += RandomFloat( -0.02, 0.02 );
	controllers[0] = flAmmoCount;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatBurstRifle::ViewModelDrawn( C_BaseViewModel *pViewModel )
{
	C_BaseTFPlayer *pPlayer = ( C_BaseTFPlayer* )GetOwner();
	if ( pPlayer && pPlayer->IsDamageBoosted() )
	{			
		Vector vecBarrelPos;
		QAngle angMuzzle;
		int iAttachment = pViewModel->LookupAttachment( "muzzle" );
		pViewModel->GetAttachment( iAttachment, vecBarrelPos, angMuzzle );

		unsigned char color[3];
		color[0] = 50;
		color[1] = 128;
		color[2] = 50;
		FX_Smoke( vecBarrelPos, angMuzzle, 0.5, 1, &color[0], 192 );
	}
}
#endif

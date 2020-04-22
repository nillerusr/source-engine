//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shotgun & Shield combo
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "weapon_combatshield.h"
#include "weapon_combat_usedwithshieldbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "tf_gamerules.h"

// Damage CVars
ConVar	weapon_combat_shotgun_damage( "weapon_combat_shotgun_damage","5", FCVAR_REPLICATED, "Shotgun damage per pellet" );
ConVar	weapon_combat_shotgun_powered_damage( "weapon_combat_shotgun_powered_damage","10", FCVAR_REPLICATED, "Shotgun damage per pellet when powered" );
ConVar	weapon_combat_shotgun_range( "weapon_combat_shotgun_range","900", FCVAR_REPLICATED, "Shotgun maximum range" );
ConVar	weapon_combat_shotgun_pellets( "weapon_combat_shotgun_pellets","8", FCVAR_REPLICATED, "Shotgun pellets per fire" );
ConVar	weapon_combat_shotgun_ducking_mod( "weapon_combat_shotgun_ducking_mod", "0.75", FCVAR_REPLICATED, "Shotgun ducking speed modifier" );
ConVar	weapon_combat_shotgun_energy_cost( "weapon_combat_shotgun_energy_cost", "0.1", FCVAR_REPLICATED, "Sapper's energy cost to fire a powered shotgun round" );


#if defined( CLIENT_DLL )
#include "fx.h"
#include "c_tf_class_sapper.h"
#define CWeaponCombatShotgun C_WeaponCombatShotgun
#define CPlayerClassSapper C_PlayerClassSapper
#else
#include "tf_class_sapper.h"
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWeaponCombatShotgun : public CWeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( CWeaponCombatShotgun, CWeaponCombatUsedWithShieldBase );
public:
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponCombatShotgun( void );

	virtual const Vector& GetBulletSpread( void );
	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack( void );
	virtual float 	GetFireRate( void );
	virtual float	GetDefaultAnimSpeed( void );

	// All predicted weapons need to implement and return true
	virtual bool	IsPredicted( void ) const
	{ 
		return true;
	}
private:
	CWeaponCombatShotgun( const CWeaponCombatShotgun & );

public:
#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwner() && 
			GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	bool OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );
	void ViewModelDrawn( C_BaseViewModel *pViewModel );
#endif

private:
	// If true, the current shot being fired is a powered shot, using some of the Sapper's energy
	bool		m_bPoweredShot;
	float		m_flOwnersEnergyLevel;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponCombatShotgun::CWeaponCombatShotgun( void )
{
	SetPredictionEligible( true );
	m_bReloadsSingly = true;
	m_flOwnersEnergyLevel = 0;
}

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCombatShotgun, DT_WeaponCombatShotgun )

BEGIN_NETWORK_TABLE( CWeaponCombatShotgun, DT_WeaponCombatShotgun )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponCombatShotgun )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_combat_shotgun, CWeaponCombatShotgun );
PRECACHE_WEAPON_REGISTER(weapon_combat_shotgun);

//-----------------------------------------------------------------------------
// Purpose: Get the accuracy derived from weapon and player, and return it
//-----------------------------------------------------------------------------
const Vector& CWeaponCombatShotgun::GetBulletSpread( void )
{
	static Vector cone = VECTOR_CONE_20DEGREES;
	static Vector powered_cone = VECTOR_CONE_10DEGREES;

	if ( m_bPoweredShot )
		return powered_cone;

	return cone;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatShotgun::ItemPostFrame( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if (!pOwner)
		return;

	// Get our player's energy level
	if ( pOwner->PlayerClass() == TFCLASS_SAPPER )
	{
		CPlayerClassSapper *pSapper = static_cast<CPlayerClassSapper*>( pOwner->GetPlayerClass() );
		if ( pSapper )
		{
			m_flOwnersEnergyLevel = pSapper->GetDrainedEnergy();
		}
	}

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

	WeaponIdle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatShotgun::PrimaryAttack( void )
{
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)GetOwner();
	if (!pPlayer)
		return;
	
	WeaponSound(SINGLE);

	// Fire the bullets
	Vector vecSrc = pPlayer->Weapon_ShootPosition( );
	Vector vecAiming;
	pPlayer->EyeVectors( &vecAiming );

	PlayAttackAnimation( GetPrimaryAttackActivity() );

	float flDamage = weapon_combat_shotgun_damage.GetFloat();

	m_bPoweredShot = false;
	// Always allow a powered shot, even if they have less than the actual cost.
	// Prevents them having to examine the energy bar carefully to know if they have enough.
	if ( m_flOwnersEnergyLevel || pPlayer->HasPowerup(POWERUP_BOOST) )
	{
		if ( m_flOwnersEnergyLevel )
		{
			CPlayerClassSapper *pSapper = static_cast<CPlayerClassSapper*>( pPlayer->GetPlayerClass() );
			pSapper->DeductDrainedEnergy( weapon_combat_shotgun_energy_cost.GetFloat() );
		}

		m_bPoweredShot = true;

		flDamage = weapon_combat_shotgun_powered_damage.GetFloat();
	}

	// Make a satisfying shotgun force, and knock them into the air
	float flForceScale = (100) * 75;
	Vector vecForce = vecAiming;
	vecForce.z += 0.7;
	vecForce *= flForceScale;
	
	CTakeDamageInfo info( this, pPlayer, vecForce, vec3_origin, flDamage, DMG_BULLET | DMG_BUCKSHOT);
	TFGameRules()->FireBullets( info, weapon_combat_shotgun_pellets.GetFloat(), vecSrc, vecAiming, GetBulletSpread(), weapon_combat_shotgun_range.GetFloat(), m_iPrimaryAmmoType, 2, entindex(), 0 );

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_iClip1 = m_iClip1 - 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponCombatShotgun::GetFireRate( void )
{	
	float flFireRate = ( SequenceDuration() * 2) + SHARED_RANDOMFLOAT( 0.0, 0.035f );

	CBaseTFPlayer *pPlayer = static_cast<CBaseTFPlayer*>( GetOwner() );
	if ( pPlayer )
	{
		// Ducking players should fire more rapidly.
		if ( pPlayer->GetFlags() & FL_DUCKING )
		{
			flFireRate *= weapon_combat_shotgun_ducking_mod.GetFloat();
		}
	}
	
	return flFireRate;
}

//-----------------------------------------------------------------------------
// Purpose: Match the anim speed to the weapon speed while crouching
//-----------------------------------------------------------------------------
float CWeaponCombatShotgun::GetDefaultAnimSpeed( void )
{
	if ( GetOwner() && GetOwner()->IsPlayer() )
	{
		if ( GetOwner()->GetFlags() & FL_DUCKING )
			return (1.0 + (1.0 - weapon_combat_shotgun_ducking_mod.GetFloat()) );
	}

	return 1.0;
}

#if defined ( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponCombatShotgun::OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
{
	CBaseTFPlayer *pPlayer = static_cast<CBaseTFPlayer*>( GetOwner() );
	if ( !pPlayer )
		return true;

	if ( m_flOwnersEnergyLevel || pPlayer->HasPowerup(POWERUP_BOOST) )
	{
		Vector vecMuzzlePos, vecBarrelPos;
		QAngle angMuzzle;
		int iAttachment = pViewModel->LookupAttachment( "0" );
		pViewModel->GetAttachment( iAttachment, vecBarrelPos, angMuzzle );
		//pViewModel->UncorrectViewModelAttachment( vecBarrelPos );	
		iAttachment = pViewModel->LookupAttachment( "muzzle" );
		pViewModel->GetAttachment( 0, vecMuzzlePos, angMuzzle );
		
		unsigned char color[3];
		color[0] = 50;
		color[1] = 255;
		color[2] = 50;
		FX_MuzzleEffect( vecBarrelPos, angMuzzle, 1.0, GetRefEHandle(), &color[0] );
	
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatShotgun::ViewModelDrawn( C_BaseViewModel *pViewModel )
{
	CBaseTFPlayer *pPlayer = static_cast<CBaseTFPlayer*>( GetOwner() );
	if ( !pPlayer )
		return;

	if ( m_flOwnersEnergyLevel )
	{			
		Vector vecMuzzlePos, vecBarrelPos;
		QAngle angMuzzle;
		int iAttachment = pViewModel->LookupAttachment( "0" );
		pViewModel->GetAttachment( iAttachment, vecBarrelPos, angMuzzle );
		//pViewModel->UncorrectViewModelAttachment( vecBarrelPos );	
		iAttachment = pViewModel->LookupAttachment( "muzzle" );
		pViewModel->GetAttachment( 0, vecMuzzlePos, angMuzzle );

		unsigned char color[3];
		color[0] = 50;
		color[1] = 128;
		color[2] = 50;
		FX_Smoke( vecBarrelPos, angMuzzle, MAX(0.3,m_flOwnersEnergyLevel), 1, &color[0], 255 );
	}
}
#endif
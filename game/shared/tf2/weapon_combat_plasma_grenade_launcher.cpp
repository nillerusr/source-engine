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

#if !defined( CLIENT_DLL )

#include "grenade_antipersonnel.h"

#else

#define CWeaponCombatPlasmaGrenadeLauncher C_WeaponCombatPlasmaGrenadeLauncher

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// Damage CVars
ConVar	weapon_combat_plasmagrenadelauncher_damage( "weapon_combat_plasmagrenadelauncher_damage","40", FCVAR_REPLICATED, "Burst Rifle damage" );
ConVar	weapon_combat_plasmagrenadelauncher_radius( "weapon_combat_plasmagrenadelauncher_radius","100", FCVAR_REPLICATED, "Burst Rifle maximum range" );


#define MAX_RIFLE_POWER		3.0
#define RIFLE_CHARGE_TIME	2.0

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWeaponCombatPlasmaGrenadeLauncher : public CWeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( CWeaponCombatPlasmaGrenadeLauncher, CWeaponCombatUsedWithShieldBase );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponCombatPlasmaGrenadeLauncher();

	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack( void );
	virtual float 	GetFireRate( void );
	virtual void	Precache( void );
	// All predicted weapons need to implement and return true
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}
#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwner() && 
			GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}
#endif
private:
	CWeaponCombatPlasmaGrenadeLauncher( const CWeaponCombatPlasmaGrenadeLauncher & );

};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCombatPlasmaGrenadeLauncher, DT_WeaponCombatPlasmaGrenadeLauncher )

BEGIN_NETWORK_TABLE( CWeaponCombatPlasmaGrenadeLauncher, DT_WeaponCombatPlasmaGrenadeLauncher )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponCombatPlasmaGrenadeLauncher )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_combat_plasmagrenadelauncher, CWeaponCombatPlasmaGrenadeLauncher );
PRECACHE_WEAPON_REGISTER(weapon_combat_plasmagrenadelauncher);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponCombatPlasmaGrenadeLauncher::CWeaponCombatPlasmaGrenadeLauncher()
{
	m_bReloadsSingly = true;
	SetPredictionEligible( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatPlasmaGrenadeLauncher::Precache( void )
{
	BaseClass::Precache();
#if !defined( CLIENT_DLL )
	UTIL_PrecacheOther( "grenade_antipersonnel" );
#endif
	PrecacheModel( "models/weapons/w_grenade.mdl" );
}

void CWeaponCombatPlasmaGrenadeLauncher::ItemPostFrame( void )
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
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCombatPlasmaGrenadeLauncher::PrimaryAttack( void )
{
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)GetOwner();
	if (!pPlayer)
		return;
	
	WeaponSound(SINGLE);

	// Fire the bullets
	Vector vecSrc = pPlayer->Weapon_ShootPosition( );

	PlayAttackAnimation( GetPrimaryAttackActivity() );

	// Launch the grenade
	Vector vecForward;
	pPlayer->EyeVectors( &vecForward );
	Vector vecOrigin = pPlayer->EyePosition();
	vecOrigin += (vecForward);

#if !defined( CLIENT_DLL )
	float flSpeed = 1200;

	CGrenadeAntiPersonnel* pGrenade = CGrenadeAntiPersonnel::Create(vecOrigin, vecForward * flSpeed, pPlayer );
	pGrenade->SetModel( "models/weapons/w_grenade.mdl" );
	pGrenade->SetBounceSound( "PlasmaGrenade.Bounce" );
	pGrenade->SetDamage( weapon_combat_plasmagrenadelauncher_damage.GetFloat() );
	pGrenade->SetDamageRadius( weapon_combat_plasmagrenadelauncher_radius.GetFloat() );
	pGrenade->SetExplodeOnContact( true );
#endif

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_iClip1 = m_iClip1 - 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponCombatPlasmaGrenadeLauncher::GetFireRate( void )
{	
	return SequenceDuration() * 3;
}

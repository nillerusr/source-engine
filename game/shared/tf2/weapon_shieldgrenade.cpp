//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "weapon_combat_usedwithshieldbase.h"
#include "weapon_combat_basegrenade.h"
#include "weapon_combatshield.h"
#include "in_buttons.h"

#if !defined( CLIENT_DLL )
#include "tf_shieldgrenade.h"
#else

#define CWeaponShieldGrenade C_WeaponShieldGrenade

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define SHIELD_GRENADE_LIFETIME	10.0f		// 10 seconds

//-----------------------------------------------------------------------------
// The shield grenade weapon
//-----------------------------------------------------------------------------

class CWeaponShieldGrenade: public CWeaponCombatUsedWithShieldBase
{
	DECLARE_CLASS( CWeaponShieldGrenade, CWeaponCombatUsedWithShieldBase );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponShieldGrenade();

	void	Spawn( void );
	void	ItemPostFrame( void );
	void	PrimaryAttack( void );
	void	ThrowGrenade( void );

	// All predicted weapons need to implement and return true
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}

#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwner() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}
#endif
private:
	// A hack to work around missing animation feature
	float	m_flStartedThrowAt;

private:														
	CWeaponShieldGrenade( const CWeaponShieldGrenade & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponShieldGrenade, DT_WeaponShieldGrenade )

BEGIN_NETWORK_TABLE( CWeaponShieldGrenade, DT_WeaponShieldGrenade )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_shield_grenade, CWeaponShieldGrenade );
PRECACHE_WEAPON_REGISTER(weapon_shield_grenade);

BEGIN_PREDICTION_DATA( CWeaponShieldGrenade )
	DEFINE_FIELD( m_flStartedThrowAt, FIELD_FLOAT ),
END_PREDICTION_DATA();

CWeaponShieldGrenade::CWeaponShieldGrenade( void )
{
	SetPredictionEligible( true );
}

//-----------------------------------------------------------------------------
// Yeehaw
//-----------------------------------------------------------------------------
void CWeaponShieldGrenade::Spawn( void )
{
	BaseClass::Spawn();
    m_flStartedThrowAt = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponShieldGrenade::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if (!pOwner)
		return;

	// Look for button downs
	if ( (pOwner->m_nButtons & IN_ATTACK) && !m_flStartedThrowAt && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
	{
		m_flStartedThrowAt = gpGlobals->curtime;

		SendWeaponAnim( ACT_VM_DRAW );
	}

	// Look for button ups
	if ( (pOwner->m_afButtonReleased & IN_ATTACK) && (m_flNextPrimaryAttack <= gpGlobals->curtime) && m_flStartedThrowAt )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime;
		PrimaryAttack();
		m_flStartedThrowAt = 0;
	}

	//  No buttons down?
	if ( !((pOwner->m_nButtons & IN_ATTACK) || (pOwner->m_nButtons & IN_ATTACK2) || (pOwner->m_nButtons & IN_RELOAD)) )
	{
		WeaponIdle( );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponShieldGrenade::PrimaryAttack( void )
{
	CBasePlayer *pPlayer = dynamic_cast<CBasePlayer*>( GetOwner() );
	if ( !pPlayer )
		return;

	if ( !ComputeEMPFireState() )
		return;

	if ( !GetPrimaryAmmo() )
		return;

	// player "shoot" animation
	PlayAttackAnimation( ACT_VM_THROW );

	ThrowGrenade();

	// Setup for refire
	m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
	CheckRemoveDisguise();

	// If I'm now out of ammo, switch away
	if ( !HasPrimaryAmmo() )
	{
		g_pGameRules->GetNextBestWeapon( pPlayer, NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponShieldGrenade::ThrowGrenade( void )
{
	CBasePlayer *pPlayer = dynamic_cast<CBasePlayer*>( GetOwner() );
	if ( !pPlayer )
		return;

	BaseClass::WeaponSound(WPN_DOUBLE);

	// Calculate launch velocity (3 seconds for max distance)
	float flThrowTime = MIN( (gpGlobals->curtime - m_flStartedThrowAt), 3.0 );
	float flSpeed = 800 + (200 * flThrowTime);

	// If the player's crouched, roll the grenade
	if ( pPlayer->GetFlags() & FL_DUCKING )
	{
		// Launch the grenade
		Vector vecForward;
		QAngle vecAngles = pPlayer->EyeAngles();
		// Throw it up just a tad
		vecAngles.x = -1;
		AngleVectors( vecAngles, &vecForward, NULL, NULL);
		Vector vecOrigin;
		VectorLerp( pPlayer->EyePosition(), pPlayer->GetAbsOrigin(), 0.25f, vecOrigin );
		vecOrigin += (vecForward * 16);
		vecForward = vecForward * flSpeed;

		QAngle vecGrenAngles;
		vecGrenAngles.Init( 0, vecAngles.y, 0 );
#if !defined( CLIENT_DLL )
		CreateShieldGrenade( vecOrigin, vecGrenAngles, vecForward, vec3_angle, pPlayer, SHIELD_GRENADE_LIFETIME );
#endif
	}
	else
	{
		// Launch the grenade
		Vector vecForward;
		QAngle vecAngles = pPlayer->EyeAngles();
		AngleVectors( vecAngles, &vecForward, NULL, NULL);
		Vector vecOrigin = pPlayer->EyePosition();
		vecOrigin += (vecForward * 16);
		vecForward = vecForward * flSpeed;

		QAngle vecGrenAngles;
		vecGrenAngles.Init( 0, vecAngles.y, 0 );
#if !defined( CLIENT_DLL )
		CreateShieldGrenade( vecOrigin, vecGrenAngles, vecForward, vec3_angle, pPlayer, SHIELD_GRENADE_LIFETIME );
#endif
	}

	pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );
}

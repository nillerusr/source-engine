//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:	Recon's dual semi-auto pistols
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_basecombatweapon.h"
#include "in_buttons.h"
#include "gamerules.h"
#include "ammodef.h"
#include "basegrenade_shared.h"

ConVar	weapon_pistols_damage( "weapon_pistols_damage","0", FCVAR_NONE, "Recon pistols maximum damage" );
ConVar	weapon_pistols_range( "weapon_pistols_range","0", FCVAR_NONE, "Recon pistols maximum range" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWeaponPistols : public CTFMachineGun
{
	DECLARE_CLASS( CWeaponPistols, CTFMachineGun );
public:
	virtual float			GetFireRate( void );
	virtual const Vector&	GetBulletSpread( void ); 
	virtual bool			Deploy( void );
	virtual void			PrimaryAttack( void );
	virtual void			SecondaryAttack( void );
	virtual void			ItemPostFrame( void );

private:
	float		m_flSoonestPrimaryAttack;
	float		m_flLastPrimaryAttack;
};

LINK_ENTITY_TO_CLASS( weapon_pistols, CWeaponPistols );
PRECACHE_WEAPON_REGISTER(weapon_pistols);

//-----------------------------------------------------------------------------
// Purpose: Get the accuracy derived from weapon and player, and return it
//-----------------------------------------------------------------------------
const Vector& CWeaponPistols::GetBulletSpread( void )
{
	static Vector cone;
	cone = VECTOR_CONE_10DEGREES;

	// Modify accuracy based upon firing rate
	// Maximum accuracy's used if you're firing at the standard rate of the gun
	float flModifier = MIN( GetFireRate(), gpGlobals->curtime - m_flLastPrimaryAttack );
	flModifier = 1.0 - RemapVal( flModifier, 0, GetFireRate(), 0, 1.0 );
	cone *= flModifier;

	return cone;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponPistols::GetFireRate( void )
{	
	return 0.5; 
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
bool CWeaponPistols::Deploy( void )
{
	m_flSoonestPrimaryAttack = gpGlobals->curtime;
	m_flLastPrimaryAttack = gpGlobals->curtime;
	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponPistols::PrimaryAttack( void )
{
	m_flSoonestPrimaryAttack = gpGlobals->curtime + 0.1;
	BaseClass::PrimaryAttack();
	m_flLastPrimaryAttack = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Throw out a sticky bomb
//-----------------------------------------------------------------------------
void CWeaponPistols::SecondaryAttack( void )
{
	/* FIXME: Temporarily disabled
	CBasePlayer *pPlayer = GetOwner();
	if ( pPlayer == NULL )
		return;

	// Calculate position & velocity
	Vector vecForward;
	pPlayer->EyeVectors( &vecForward );
	Vector vecOrigin = pPlayer->EyePosition();
	vecOrigin += (vecForward * 16);

	// Create the stickybomb
	CBaseGrenade *pGrenade = (CBaseGrenade*)CreateEntityByName("grenade_stickybomb");
	UTIL_SetOrigin( pGrenade->pev, vecOrigin );
	pGrenade->Spawn();
	pGrenade->SetOwnerEntity( pPlayer );
	pGrenade->m_hOwner = pPlayer;
	pGrenade->m_vecVelocity = vecForward * 1200;
	VectorAngles( vecForward, pGrenade->GetAngles() );

	// Reduce ammo, setup for refire
	pPlayer->m_iAmmo[m_iSecondaryAmmoType]--;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5;
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistols::ItemPostFrame( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer == NULL )
		return;

	// Allow a refire as fast as the player can click
	if ( ( ( pPlayer->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime )
		/* && ( m_flNextSecondaryAttack < gpGlobals->curtime )*/ )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	}

	BaseClass::ItemPostFrame();
}


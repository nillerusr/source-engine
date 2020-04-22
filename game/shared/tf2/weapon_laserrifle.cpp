//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Medic's Laser rifle
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "NPCEvent.h"
#include "tf_basecombatweapon.h"
#include "basecombatcharacter.h"
#include "smoke_trail.h"
#include "tf_player.h"
#include "in_buttons.h"
#include "tf_gamerules.h"
#include "ammodef.h"
#include "IEffects.h"
#include "vstdlib/random.h"

// Damage CVars
ConVar	weapon_laserrifle_damage( "weapon_laserrifle_damage","0", FCVAR_NONE, "Laser Rifle maximum damage" );
ConVar	weapon_laserrifle_range( "weapon_laserrifle_range","0", FCVAR_NONE, "Laser Rifle maximum range" );

// ------------------------------------------------------------------------ //
// CWeaponLaserRifle
// ------------------------------------------------------------------------ //
class CWeaponLaserRifle : public CBaseTFCombatWeapon
{
	DECLARE_CLASS( CWeaponLaserRifle, CBaseTFCombatWeapon );
public:	
	virtual void	Precache( void );
	virtual float	GetFireRate( void );
	virtual void	PrimaryAttack( void );
	virtual bool	ComputeEMPFireState( void );
	
	// Beam
	int		m_iSpriteTexture;
};

LINK_ENTITY_TO_CLASS( weapon_laserrifle, CWeaponLaserRifle );
PRECACHE_WEAPON_REGISTER(weapon_laserrifle);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLaserRifle::Precache( void )
{
	BaseClass::Precache();

	m_iSpriteTexture = PrecacheModel( "sprites/physbeam.vmt" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponLaserRifle::GetFireRate()
{
	return 0.2;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponLaserRifle::ComputeEMPFireState( void )
{
	if (IsOwnerEMPed())
	{
		// FIXME: Need a sound
		//UTIL_EmitSound( pPlayer->pev, CHAN_WEAPON, g_pszEMPGatlingFizzle, 1.0, ATTN_NORM );
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLaserRifle::PrimaryAttack( void )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( m_hOwner );
	if ( !pPlayer )
		return;
	
	if ( !ComputeEMPFireState() )
		return;

	WeaponSound(SINGLE);

	PlayAttackAnimation( GetPrimaryAttackActivity() );

	pPlayer->AddEffects( EF_MUZZLEFLASH );

	// Fire the beam: BLOW OFF AUTOAIM
	Vector vecSrc	 = pPlayer->Weapon_ShootPosition( pPlayer->GetOrigin() );
	Vector vecAiming, right, up;
	pPlayer->EyeVectors( &vecAiming, &right, &up);

	Vector vecSpread = VECTOR_CONE_4DEGREES;

	// Get endpoint
	float x, y, z;
	do {
		x = random->RandomFloat(-0.5,0.5) + random->RandomFloat(-0.5,0.5);
		y = random->RandomFloat(-0.5,0.5) + random->RandomFloat(-0.5,0.5);
		z = x*x+y*y;
	} while (z > 1);
	Vector vecDir = vecAiming + x * vecSpread.x * right + y * vecSpread.y * up;
	Vector vecEnd = vecSrc + vecDir * weapon_laserrifle_range.GetFloat();

	trace_t tr;
	float damagefactor = TFGameRules()->WeaponTraceLine(vecSrc, vecEnd, MASK_SHOT, pPlayer, DMG_ENERGYBEAM, &tr);

	// Hit target?
	if (tr.fraction != 1.0)
	{
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.u.ent);
		if ( pEntity )
		{
			ClearMultiDamage();
			float flDamage = GetDamage( (tr.endpos - vecSrc).Length(), tr.hitgroup );
			flDamage *= damagefactor;  
			pEntity->TraceAttack( CTakeDamageInfo( pPlayer, pPlayer, flDamage, DMG_ENERGYBEAM ), vecDir, &tr );
			ApplyMultiDamage( pPlayer, pPlayer );
		}

		g_pEffects->EnergySplash( tr.endpos, tr.plane.normal );
	}

	// Get hacked gun position
	AngleVectors( pPlayer->EyeAngles() + pPlayer->m_Local.m_vecPunchAngle, NULL, &right, NULL );
	Vector vecTracerSrc = vecSrc + Vector (0,0,-8) + right * 12 + vecDir * 16;

	// Laser beam
	CBroadcastRecipientFilter filter;
	te->BeamPoints( filter, 0.0,
					&vecTracerSrc, 
					&tr.endpos, 
					m_iSpriteTexture, 
					0,		// Halo index
					0,		// Start frame
					0,		// Frame rate
					0.2,	// Life
					15,		// Width
					15,		// EndWidth
					0,		// FadeLength
					0,		// Amplitude
					200,	// r
					200,	// g
					255,	// b
					255,	// a
					255	);	// speed

	pPlayer->m_iAmmo[m_iPrimaryAmmoType]--;
	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();

	CheckRemoveDisguise();
}

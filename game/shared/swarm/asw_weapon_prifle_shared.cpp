#include "cbase.h"
#include "asw_weapon_prifle_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#define CASW_Marine C_ASW_Marine
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "asw_grenade_prifle.h"
#include "asw_fail_advice.h"
#include "effect_dispatch_data.h"
#endif
#include "asw_marine_skills.h"
#include "asw_weapon_parse.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_PRifle, DT_ASW_Weapon_PRifle )

BEGIN_NETWORK_TABLE( CASW_Weapon_PRifle, DT_ASW_Weapon_PRifle )
#ifdef CLIENT_DLL
	// recvprops
#else
	// sendprops
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_PRifle )
	
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_prifle, CASW_Weapon_PRifle );
PRECACHE_WEAPON_REGISTER(asw_weapon_prifle);

#ifndef CLIENT_DLL
extern ConVar asw_debug_marine_damage;
//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_PRifle )
	
END_DATADESC()

#endif /* not client */

CASW_Weapon_PRifle::CASW_Weapon_PRifle()
{

}


CASW_Weapon_PRifle::~CASW_Weapon_PRifle()
{

}

void CASW_Weapon_PRifle::Precache()
{
	// precache the weapon model here?	
	PrecacheScriptSound("ASW_Weapon_PRifle.StunGrenadeExplosion");	
	PrecacheScriptSound("ASW_PRifle.ReloadA");
	PrecacheScriptSound("ASW_PRifle.ReloadB");
	PrecacheScriptSound("ASW_PRifle.ReloadC");
	PrecacheModel( "swarm/effects/electrostunbeam.vmt" );
	PrecacheModel( "swarm/effects/bluemuzzle_nocull.vmt" );
	PrecacheModel( "effects/bluelaser2.vmt" );

	BaseClass::Precache();
}


float CASW_Weapon_PRifle::GetWeaponDamage()
{
	//float flDamage = 7.0f;
	float flDamage = GetWeaponInfo()->m_flBaseDamage;

	if ( GetMarine() )
	{
		flDamage += MarineSkills()->GetSkillBasedValueByMarine(GetMarine(), ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_PRIFLE_DMG);
	}

	//CALL_ATTRIB_HOOK_FLOAT( flDamage, mod_damage_done );

	return flDamage;
}

void CASW_Weapon_PRifle::SecondaryAttack()
{
	// Only the player fires this way so we can cast
	CASW_Player *pPlayer = GetCommander();

	if (!pPlayer)
		return;

	CASW_Marine *pMarine = GetMarine();

	if (!pMarine)
		return;

	//Must have ammo
	bool bUsesSecondary = UsesSecondaryAmmo();
	bool bUsesClips = UsesClipsForAmmo2();
	int iAmmoCount = pMarine->GetAmmoCount(m_iSecondaryAmmoType);
	bool bInWater = ( pMarine->GetWaterLevel() == 3 );
	if ( (bUsesSecondary &&  
			(   ( bUsesClips && m_iClip2 <= 0) ||
			    ( !bUsesClips && iAmmoCount<=0 )
				) )
				 || bInWater || m_bInReload )

	{
		SendWeaponAnim( ACT_VM_DRYFIRE );
		BaseClass::WeaponSound( EMPTY );
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
		return;
	}


	// MUST call sound before removing a round from the clip of a CMachineGun
	BaseClass::WeaponSound( SPECIAL1 );

	Vector vecSrc = pMarine->Weapon_ShootPosition();
	Vector	vecThrow;
	// Don't autoaim on grenade tosses
	vecThrow = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
	QAngle angGrenFacing;
	VectorAngles(vecThrow, angGrenFacing);
	VectorScale( vecThrow, 1000.0f, vecThrow );
	
#ifndef CLIENT_DLL
	//Create the grenade
	float fGrenadeDamage = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_DMG);
	float fGrenadeRadius = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_RADIUS);
	if (asw_debug_marine_damage.GetBool())
	{
		Msg("Grenade damage = %f radius = %f\n", fGrenadeDamage, fGrenadeRadius);
	}
	CASW_Grenade_PRifle::PRifle_Grenade_Create( 
		fGrenadeDamage,
		fGrenadeRadius,
		vecSrc, angGrenFacing, vecThrow, AngularImpulse(0,0,0), pMarine, this );
#endif

	SendWeaponAnim( GetPrimaryAttackActivity() );

	pMarine->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );

	if ( bUsesClips )
	{
		m_iClip2 -= 1;
	}
	else
	{
		pMarine->RemoveAmmo( 1, m_iSecondaryAmmoType );
	}

#ifndef CLIENT_DLL
	ASWFailAdvice()->OnMarineUsedSecondary();

	CEffectData	data;
	data.m_vOrigin = GetAbsOrigin();
	//data.m_vNormal = dir;
	//data.m_flScale = (float)amount;
	CPASFilter filter( data.m_vOrigin );
	filter.SetIgnorePredictionCull(true);
	DispatchParticleEffect( "muzzleflash_grenadelauncher_main", PATTACH_POINT_FOLLOW, this, "muzzle", false, -1, &filter );

	pMarine->OnWeaponFired( this, 1 );
#endif

	// Can shoot again immediately
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;

	// Can blow up after a short delay (so have time to release mouse button)
	m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;
}


#ifdef CLIENT_DLL
const char* CASW_Weapon_PRifle::GetPartialReloadSound(int iPart)
{
	switch (iPart)
	{
		case 1: return "ASW_PRifle.ReloadB"; break;
		case 2: return "ASW_PRifle.ReloadC"; break;
		default: break;
	};
	return "ASW_PRifle.ReloadA";
}

#endif

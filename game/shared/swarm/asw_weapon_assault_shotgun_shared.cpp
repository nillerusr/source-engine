#include "cbase.h"
#include "asw_weapon_assault_shotgun_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#else
#include "asw_lag_compensation.h"
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "asw_shotgun_pellet.h"
#include "asw_grenade_vindicator.h"
#include "asw_marine_speech.h"
#include "asw_fail_advice.h"
#include "effect_dispatch_data.h"
#endif
#include "asw_marine_skills.h"
#include "asw_weapon_parse.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Assault_Shotgun, DT_ASW_Weapon_Assault_Shotgun )

BEGIN_NETWORK_TABLE( CASW_Weapon_Assault_Shotgun, DT_ASW_Weapon_Assault_Shotgun )
#ifdef CLIENT_DLL
	// recvprops
#else
	// sendprops
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Assault_Shotgun )
	
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_vindicator, CASW_Weapon_Assault_Shotgun );
PRECACHE_WEAPON_REGISTER(asw_weapon_vindicator);

#ifndef CLIENT_DLL
extern ConVar asw_debug_marine_damage;
ConVar asw_vindicator_grenade_velocity("asw_vindicator_grenade_velocity", "3.0", FCVAR_CHEAT, "Scale to the vindicator grenade initial velocity");

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Assault_Shotgun )
	
END_DATADESC()

#endif /* not client */

CASW_Weapon_Assault_Shotgun::CASW_Weapon_Assault_Shotgun()
{
	m_fMinRange1	= 0;
	m_fMaxRange1	= 320;

	m_fMinRange2	= 256;
	m_fMaxRange2	= 256;

	m_fSlowTime = 0;
}


CASW_Weapon_Assault_Shotgun::~CASW_Weapon_Assault_Shotgun()
{

}

void CASW_Weapon_Assault_Shotgun::Precache()
{
	// precache the weapon model here?
	PrecacheScriptSound("ASW_Vindicator.ReloadA");
	PrecacheScriptSound("ASW_Vindicator.ReloadB");
	PrecacheScriptSound("ASW_Vindicator.ReloadC");

	BaseClass::Precache();
}

bool CASW_Weapon_Assault_Shotgun::ShouldMarineMoveSlow()
{
	return m_fSlowTime > gpGlobals->curtime || IsReloading();
}


void CASW_Weapon_Assault_Shotgun::SecondaryAttack()
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

	BaseClass::WeaponSound( SPECIAL1 );		
			
#ifndef CLIENT_DLL
	pMarine->GetMarineSpeech()->Chatter(CHATTER_GRENADE);
	Vector vecSrc = pMarine->Weapon_ShootPosition();
	Vector	vecThrow;
	// check for turning on lag compensation
	if (pPlayer && pMarine->IsInhabited())
	{
		CASW_Lag_Compensation::RequestLagCompensation( pPlayer, pPlayer->GetCurrentUserCommand() );
	}

	// TODO: Fix for AI
	vecThrow = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
	QAngle angAiming = pPlayer->EyeAnglesWithCursorRoll();
	float dist = tan(DEG2RAD(90 - angAiming.z)) * 50.0f;

	VectorScale( vecThrow, dist * asw_vindicator_grenade_velocity.GetFloat(), vecThrow );
	float fGrenadeDamage = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_INCENDIARY_DMG);
	float fGrenadeRadius = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_RADIUS);
	if (asw_debug_marine_damage.GetBool())
	{
		Msg("Grenade damage = %f radius = %f\n", fGrenadeDamage, fGrenadeRadius);
	}
	// check the grenade fits where we want to spawn it
	Ray_t ray;
	trace_t pm;
	ray.Init( pMarine->WorldSpaceCenter(), vecSrc, -Vector(4,4,4), Vector(4,4,4) );
	UTIL_TraceRay( ray, MASK_SOLID, pMarine, COLLISION_GROUP_PROJECTILE, &pm );
	if (pm.fraction < 1.0f)
		vecSrc = pm.endpos;

	CASW_Grenade_Vindicator::Vindicator_Grenade_Create( 
		fGrenadeDamage,
		fGrenadeRadius,
		vecSrc, angAiming, vecThrow, AngularImpulse(0,0,0), pMarine, this );

	pMarine->OnWeaponFired( this, 1, true );
#endif

	SendWeaponAnim( GetPrimaryAttackActivity() );

	pMarine->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );

	// Decrease ammo
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
#endif

	// Can shoot again immediately
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;

	// Can blow up after a short delay (so have time to release mouse button)
	m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;
}

float CASW_Weapon_Assault_Shotgun::GetWeaponDamage()
{
	//float flDamage = 7.0f;
	float flDamage = GetWeaponInfo()->m_flBaseDamage;

	if ( GetMarine() )
	{
		flDamage += MarineSkills()->GetSkillBasedValueByMarine(GetMarine(), ASW_MARINE_SKILL_VINDICATOR, ASW_MARINE_SUBSKILL_VINDICATOR_DAMAGE);
	}

	//CALL_ATTRIB_HOOK_FLOAT( flDamage, mod_damage_done );

	return flDamage;
}

int CASW_Weapon_Assault_Shotgun::GetNumPellets()
{
	if (GetMarine())
		return MarineSkills()->GetSkillBasedValueByMarine(GetMarine(), ASW_MARINE_SKILL_VINDICATOR, ASW_MARINE_SUBSKILL_VINDICATOR_PELLETS);

	return GetWeaponInfo()->m_iNumPellets;
}

#ifdef CLIENT_DLL
const char* CASW_Weapon_Assault_Shotgun::GetPartialReloadSound(int iPart)
{
	switch (iPart)
	{
		case 1: return "ASW_Vindicator.ReloadB"; break;
		case 2: return "ASW_Vindicator.ReloadC"; break;
		default: break;
	};
	return "ASW_Vindicator.ReloadA";
}

float CASW_Weapon_Assault_Shotgun::GetMuzzleFlashScale( void )
{
	// if we haven't calculated the muzzle scale based on the carrying marine's skill yet, then do so
	if (m_fMuzzleFlashScale == -1)
	{
		C_ASW_Marine *pMarine = GetMarine();
		if (pMarine)
			m_fMuzzleFlashScale = 1.5f * MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_VINDICATOR, ASW_MARINE_SUBSKILL_VINDICATOR_MUZZLE);
		else
			return 1.5f;
	}
	return m_fMuzzleFlashScale;
}
#endif

#include "cbase.h"
#include "asw_weapon_rifle.h"

// from ar2:
#include "basecombatweapon.h"
#include "npcevent.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "player.h"
#include "grenade_ar2.h"
#include "gamerules.h"
#include "game.h"
#include "in_buttons.h"
#include "ai_memory.h"
#include "soundent.h"
#include "hl2_player.h"
#include "EntityFlame.h"
#include "weapon_flaregun.h"
#include "te_effect_dispatch.h"
#include "prop_combine_ball.h"
#include "beam_shared.h"
#include "npc_combine.h"
#include "asw_rifle_grenade.h"
#include "asw_marine.h"
#include "asw_marine_resource.h"
#include "asw_player.h"
#include "asw_marine_skills.h"
#include "asw_marine_speech.h"
#include "asw_fail_advice.h"
#include "fmtstr.h"
#include "asw_weapon_parse.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_debug_marine_damage;

LINK_ENTITY_TO_CLASS( asw_weapon_rifle, CASW_Weapon_Rifle );

//---------------------------------------------------------
// 
//---------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST(CASW_Weapon_Rifle, DT_ASW_Weapon_Rifle)
	
END_SEND_TABLE()

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Rifle )
END_DATADESC()

PRECACHE_WEAPON_REGISTER(asw_weapon_rifle);

CASW_Weapon_Rifle::CASW_Weapon_Rifle()
{
}


CASW_Weapon_Rifle::~CASW_Weapon_Rifle()
{

}

void CASW_Weapon_Rifle::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound("ASW_Rifle.ReloadA");
	PrecacheScriptSound("ASW_Rifle.ReloadB");
	PrecacheScriptSound("ASW_Rifle.ReloadC");

	UTIL_PrecacheOther( "env_entity_dissolver" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &tr - 
//			nDamageType - 
//-----------------------------------------------------------------------------
void CASW_Weapon_Rifle::DoImpactEffect( trace_t &tr, int nDamageType )
{
	CEffectData data;

	data.m_vOrigin = tr.endpos + ( tr.plane.normal * 1.0f );
	data.m_vNormal = tr.plane.normal;

	DispatchEffect( "AR2Impact", data );

	BaseClass::DoImpactEffect( tr, nDamageType );
}

void CASW_Weapon_Rifle::SecondaryAttack( void )
{
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
	//BaseClass::WeaponSound( WPN_DOUBLE );
	BaseClass::WeaponSound( SPECIAL1 );

	Vector vecSrc = pMarine->Weapon_ShootPosition();
	Vector	vecThrow;
	// Don't autoaim on grenade tosses
	QAngle angGrenFacing;
	// TODO: Fix for AI
	vecThrow = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
	VectorAngles(vecThrow, angGrenFacing);
	VectorScale( vecThrow, 1000.0f, vecThrow );
	
#ifndef CLIENT_DLL
	pMarine->GetMarineSpeech()->Chatter(CHATTER_GRENADE);
	//Create the grenade
	float fGrenadeDamage = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_DMG);
	float fGrenadeRadius = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_RADIUS);
	if (asw_debug_marine_damage.GetBool())
	{
		Msg("Grenade damage = %f radius = %f\n", fGrenadeDamage, fGrenadeRadius);
	}
	CASW_Rifle_Grenade::Rifle_Grenade_Create( 
		fGrenadeDamage,
		fGrenadeRadius,
		vecSrc, angGrenFacing, vecThrow, AngularImpulse(0,0,0), pMarine, this );
		pMarine->OnWeaponFired( this, 1, true );
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

	ASWFailAdvice()->OnMarineUsedSecondary();

	CEffectData	data;
	data.m_vOrigin = GetAbsOrigin();
	//data.m_vNormal = dir;
	//data.m_flScale = (float)amount;
	CPASFilter filter( data.m_vOrigin );
	filter.SetIgnorePredictionCull(true);
	DispatchParticleEffect( "muzzleflash_grenadelauncher_main", PATTACH_POINT_FOLLOW, this, "muzzle", false, -1, &filter );

	// Can shoot again immediately
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;

	// Can blow up after a short delay (so have time to release mouse button)
	m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;
}

float CASW_Weapon_Rifle::GetFireRate()
{
	//float flRate = 0.07f;
	float flRate = GetWeaponInfo()->m_flFireRate;

	//CALL_ATTRIB_HOOK_FLOAT( flRate, mod_fire_rate );

	return flRate;
}

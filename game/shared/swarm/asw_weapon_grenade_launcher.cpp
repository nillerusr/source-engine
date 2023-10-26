#include "cbase.h"
#include "asw_weapon_grenade_launcher.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "prediction.h"
#include "c_te_effect_dispatch.h"
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "asw_grenade_cluster.h"
#include "asw_marine_speech.h"
#include "asw_gamerules.h"
#endif
#include "asw_marine_skills.h"
#include "particle_parse.h"
#include "asw_util_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_FLARES_FASTEST_REFIRE_TIME		0.1f

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Grenade_Launcher, DT_ASW_Weapon_Grenade_Launcher )

BEGIN_NETWORK_TABLE( CASW_Weapon_Grenade_Launcher, DT_ASW_Weapon_Grenade_Launcher )
#ifdef CLIENT_DLL
	// recvprops
#else
	// sendprops
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Grenade_Launcher )
	
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_grenade_launcher, CASW_Weapon_Grenade_Launcher );
PRECACHE_WEAPON_REGISTER( asw_weapon_grenade_launcher );

#ifndef CLIENT_DLL
extern ConVar asw_debug_marine_damage;
//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Grenade_Launcher )
	DEFINE_FIELD( m_flSoonestPrimaryAttack, FIELD_TIME ),	
END_DATADESC()

#endif /* not client */

CASW_Weapon_Grenade_Launcher::CASW_Weapon_Grenade_Launcher()
{
	m_flSoonestPrimaryAttack = gpGlobals->curtime;
}


CASW_Weapon_Grenade_Launcher::~CASW_Weapon_Grenade_Launcher()
{

}

#ifdef GAME_DLL
ConVar asw_grenade_launcher_speed( "asw_grenade_launcher_speed", "2.4f", FCVAR_CHEAT, "Scale speed of grenade launcher grenades" );
ConVar asw_grenade_launcher_gravity( "asw_grenade_launcher_gravity", "2.4f", FCVAR_CHEAT, "Gravity of grenade launcher grenades" );
#endif

void CASW_Weapon_Grenade_Launcher::PrimaryAttack( void )
{	
	CASW_Player *pPlayer = GetCommander();
	if ( !pPlayer )
		return;

	// NOTE: this class is now a grenade launcher, do we want to rename it at some point?
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine || pMarine->GetWaterLevel() == 3 )
		return;

#ifndef CLIENT_DLL
	pMarine->GetMarineSpeech()->Chatter(CHATTER_GRENADE);

	Vector	vecSrc		= pMarine->Weapon_ShootPosition( );
	// check it fits where we want to spawn it
	Ray_t ray;
	trace_t pm;
	ray.Init( pMarine->WorldSpaceCenter(), vecSrc, -Vector(4,4,4), Vector(4,4,4) );
	UTIL_TraceRay( ray, MASK_SOLID, pMarine, COLLISION_GROUP_PROJECTILE, &pm );
	if (pm.fraction < 1.0f)
		vecSrc = pm.endpos;

	Vector vecDest = pPlayer->GetCrosshairTracePos();
	Vector newVel = UTIL_LaunchVector( vecSrc, vecDest, asw_grenade_launcher_gravity.GetFloat() ) * 28.0f;

	const float &fGrenadeDamage = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_CLUSTER_DMG);
	const float &fGrenadeRadius = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_RADIUS);
	int iClusters = 0; //MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_CLUSTERS);
	if (asw_debug_marine_damage.GetBool())
	{
		Msg("Grenade launcher damage = %f radius = %f clusters = %d\n", fGrenadeDamage, fGrenadeRadius, iClusters);
	}

	if (ASWGameRules())
		ASWGameRules()->m_fLastFireTime = gpGlobals->curtime;

	CASW_Grenade_Cluster *pGrenade = 
		CASW_Grenade_Cluster::Cluster_Grenade_Create(
			fGrenadeDamage,
			fGrenadeRadius,
			iClusters,
			vecSrc, pMarine->EyeAngles(), newVel, AngularImpulse(0,0,0), pMarine, this );
	if ( pGrenade )
	{
		pGrenade->SetGravity( asw_grenade_launcher_gravity.GetFloat() );
		pGrenade->SetExplodeOnWorldContact( true );

		pMarine->OnWeaponFired( this, 1 );
	}

#else
	DispatchParticleEffect( "muzzleflash_grenadelauncher_main", PATTACH_POINT_FOLLOW, this, "muzzle" );
#endif

	WeaponSound(SINGLE);

	//pMarine->DoMuzzleFlash();
	// sets the animation on the weapon model iteself
	SendWeaponAnim( GetPrimaryAttackActivity() );

	pMarine->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );

	// decrement ammo
	m_iClip1 -= 1;

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + GetFireRate();
}


void CASW_Weapon_Grenade_Launcher::Precache()
{	
	BaseClass::Precache();	

#ifndef CLIENT_DLL
	//UTIL_PrecacheOther( "asw_flare_projectile" );
#endif
}

// flares don't reload
bool CASW_Weapon_Grenade_Launcher::Reload()
{
	return BaseClass::Reload();
}

void CASW_Weapon_Grenade_Launcher::ItemPostFrame( void )
{	
	BaseClass::ItemPostFrame();
}
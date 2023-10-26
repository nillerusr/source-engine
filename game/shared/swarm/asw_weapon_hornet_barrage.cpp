#include "cbase.h"
#include "asw_weapon_hornet_barrage.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_alien.h"
#include "asw_input.h"
#include "prediction.h"
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_alien.h"
#include "particle_parse.h"
#include "te_effect_dispatch.h"
#include "asw_rocket.h"
#include "asw_gamerules.h"
#endif
#include "asw_marine_skills.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Hornet_Barrage, DT_ASW_Weapon_Hornet_Barrage )

BEGIN_NETWORK_TABLE( CASW_Weapon_Hornet_Barrage, DT_ASW_Weapon_Hornet_Barrage )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_flNextLaunchTime ) ),
	RecvPropFloat( RECVINFO( m_flFireInterval ) ),
	RecvPropInt( RECVINFO( m_iRocketsToFire ) ),
#else
	SendPropFloat( SENDINFO( m_flNextLaunchTime ) ),
	SendPropFloat( SENDINFO( m_flFireInterval ) ),	
	SendPropInt( SENDINFO( m_iRocketsToFire ), 8 ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CASW_Weapon_Hornet_Barrage )
	DEFINE_PRED_FIELD_TOL( m_flNextLaunchTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),			
	DEFINE_PRED_FIELD( m_iRocketsToFire, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( asw_weapon_hornet_barrage, CASW_Weapon_Hornet_Barrage );
PRECACHE_WEAPON_REGISTER( asw_weapon_hornet_barrage );

#ifndef CLIENT_DLL
//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Hornet_Barrage )

END_DATADESC()

#endif /* not client */

CASW_Weapon_Hornet_Barrage::CASW_Weapon_Hornet_Barrage()
{

}

void CASW_Weapon_Hornet_Barrage::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "ASW_Hornet_Barrage.Fire" );
}

bool CASW_Weapon_Hornet_Barrage::OffhandActivate()
{
	if (!GetMarine() || GetMarine()->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
		return false;
	PrimaryAttack();

	return true;
}

void CASW_Weapon_Hornet_Barrage::PrimaryAttack()
{	
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return;

	CASW_Player *pPlayer = GetCommander();
	if ( !pPlayer )
		return;

	if ( m_iRocketsToFire.Get() > 0 )
		return;	

#ifndef CLIENT_DLL
	bool bThisActive = (pMarine && pMarine->GetActiveWeapon() == this);
#endif

	// mine weapon is lost when all mines are gone
	if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	{
		//Reload();
#ifndef CLIENT_DLL
		if (pMarine)
		{
			pMarine->Weapon_Detach(this);
			if (bThisActive)
				pMarine->SwitchToNextBestWeapon(NULL);
		}
		Kill();
#endif
		return;
	}

	SetRocketsToFire();
	m_flFireInterval = GetRocketFireInterval();
	m_flNextLaunchTime = gpGlobals->curtime;
		
	const char *pszSound = "ASW_Hornet_Barrage.Fire";
	CPASAttenuationFilter filter( this, pszSound );
	if ( IsPredicted() && CBaseEntity::GetPredictionPlayer() )
	{
		filter.UsePredictionRules();
	}
	EmitSound( filter, entindex(), pszSound );

	// decrement ammo
	m_iClip1 -= 1;

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
}

void CASW_Weapon_Hornet_Barrage::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( GetRocketsToFire() > 0 && GetNextLaunchTime() <= gpGlobals->curtime )
	{
		FireRocket();

#ifndef CLIENT_DLL
		if ( GetRocketsToFire() <= 0 )
		{
			DestroyIfEmpty( true );
		}
#endif
	}
}

void CASW_Weapon_Hornet_Barrage::SetRocketsToFire()
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return;

	m_iRocketsToFire = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_HORNET_COUNT );
}

float CASW_Weapon_Hornet_Barrage::GetRocketFireInterval()
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return 0.5f;

	return MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_HORNET_INTERVAL );
}

void CASW_Weapon_Hornet_Barrage::FireRocket()
{
	CASW_Player *pPlayer = GetCommander();
	CASW_Marine *pMarine = GetMarine();
	if ( !pPlayer || !pMarine || pMarine->GetHealth() <= 0 )
	{
		m_iRocketsToFire = 0;
		return;
	}

	WeaponSound(SINGLE);

	// tell the marine to tell its weapon to draw the muzzle flash
	pMarine->DoMuzzleFlash();

	pMarine->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );

	Vector vecSrc	 = GetRocketFiringPosition();
	m_iRocketsToFire = m_iRocketsToFire.Get() - 1;
	m_flNextLaunchTime = gpGlobals->curtime + m_flFireInterval.Get();

#ifndef CLIENT_DLL
	float fGrenadeDamage = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_HORNET_DMG );

	CASW_Rocket::Create( fGrenadeDamage, vecSrc, GetRocketAngle(), pMarine, this );

	if ( ASWGameRules() )
	{
		ASWGameRules()->m_fLastFireTime = gpGlobals->curtime;
	}

	pMarine->OnWeaponFired( this, 1 );

#endif
}

const QAngle& CASW_Weapon_Hornet_Barrage::GetRocketAngle()
{
	static QAngle angRocket = vec3_angle;
	CASW_Player *pPlayer = GetCommander();
	CASW_Marine *pMarine = GetMarine();
	if ( !pPlayer || !pMarine || pMarine->GetHealth() <= 0 )
	{
		return angRocket;
	}

	Vector vecDir = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
	VectorAngles( vecDir, angRocket );
	angRocket[ YAW ] += random->RandomFloat( -35, 35 );
	return angRocket;
}


const Vector& CASW_Weapon_Hornet_Barrage::GetRocketFiringPosition()
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return vec3_origin;

	static Vector vecSrc;
	vecSrc = pMarine->Weapon_ShootPosition();

	return vecSrc;
}
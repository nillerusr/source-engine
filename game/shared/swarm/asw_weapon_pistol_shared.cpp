#include "cbase.h"
#include "asw_weapon_pistol_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#else
#include "asw_lag_compensation.h"
#include "asw_marine.h"
#include "asw_marine_resource.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "asw_shotgun_pellet.h"
#include "asw_marine_speech.h"
#include "asw_weapon_ammo_bag_shared.h"
#endif
#include "asw_marine_skills.h"
#include "asw_weapon_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_weapon_max_shooting_distance;
extern ConVar sk_plr_dmg_asw_sg; 
extern ConVar asw_weapon_force_scale;

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Pistol, DT_ASW_Weapon_Pistol )

BEGIN_NETWORK_TABLE( CASW_Weapon_Pistol, DT_ASW_Weapon_Pistol )
#ifdef CLIENT_DLL
	// recvprops
	RecvPropTime( RECVINFO( m_fSlowTime ) ),
#else
	// sendprops
	SendPropTime( SENDINFO( m_fSlowTime ) ),
#endif
END_NETWORK_TABLE()


#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CASW_Weapon_Pistol )
	DEFINE_PRED_FIELD_TOL( m_fSlowTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),	
END_PREDICTION_DATA()

#else

//-----------------------------------------------------------------------------
// Purpose: Save Data
//-----------------------------------------------------------------------------// 
BEGIN_DATADESC( CASW_Weapon_Pistol )
	DEFINE_FIELD( m_fSlowTime, FIELD_TIME ),
END_DATADESC()
#endif

LINK_ENTITY_TO_CLASS( asw_weapon_pistol, CASW_Weapon_Pistol );
PRECACHE_WEAPON_REGISTER(asw_weapon_pistol);

#ifndef CLIENT_DLL
extern ConVar asw_debug_marine_damage;

#endif /* not client */

CASW_Weapon_Pistol::CASW_Weapon_Pistol()
{
	m_fMinRange1	= 0;
	m_fMaxRange1	= 512;

	m_fMinRange2	= 256;
	m_fMaxRange2	= 1024;

	m_fSlowTime = 0;
}


CASW_Weapon_Pistol::~CASW_Weapon_Pistol()
{

}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CASW_Weapon_Pistol::GetPrimaryAttackActivity( void )
{
	return ACT_VM_PRIMARYATTACK;
}

#define PELLET_AIR_VELOCITY	3500
#define PELLET_WATER_VELOCITY	1500

void CASW_Weapon_Pistol::PrimaryAttack( void )
{
	// If my clip is empty (and I use clips) start reload
	if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	{
		Reload();
		return;
	}

	CASW_Player *pPlayer = GetCommander();
	CASW_Marine *pMarine = GetMarine();

	if (pMarine)		// firing from a marine
	{
		// MUST call sound before removing a round from the clip of a CMachineGun
		WeaponSound(SINGLE);
		if (m_iClip1 <= AmmoClickPoint())
			BaseClass::WeaponSound( EMPTY );

		m_bIsFiring = true;

		// tell the marine to tell its weapon to draw the muzzle flash
		pMarine->DoMuzzleFlash();

		// sets the animation on the weapon model iteself
		SendWeaponAnim( GetPrimaryAttackActivity() );

		// sets the animation on the marine holding this weapon
		//pMarine->SetAnimation( PLAYER_ATTACK1 );

#ifdef GAME_DLL	// check for turning on lag compensation
		if (pPlayer && pMarine->IsInhabited())
		{
			CASW_Lag_Compensation::RequestLagCompensation( pPlayer, pPlayer->GetCurrentUserCommand() );
		}
#endif

		//	if (asw_pistol_hitscan.GetBool())
		if (true)
		{
			FireBulletsInfo_t info;
			info.m_vecSrc = pMarine->Weapon_ShootPosition( );
			if ( pPlayer && pMarine->IsInhabited() )
			{
				info.m_vecDirShooting = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
			}
			else
			{
#ifdef CLIENT_DLL
				Msg("Error, clientside firing of a weapon that's being controlled by an AI marine\n");
#else
				info.m_vecDirShooting = pMarine->GetActualShootTrajectory( info.m_vecSrc );
#endif
			}
			
			info.m_iShots = 1;			

			// Make sure we don't fire more than the amount in the clip
			if ( UsesClipsForAmmo1() )
			{
				info.m_iShots = MIN( info.m_iShots, m_iClip1 );
				m_iClip1 -= info.m_iShots;
#ifdef GAME_DLL
				CASW_Marine *pMarine = GetMarine();
				if (pMarine && m_iClip1 <= 0 && pMarine->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
				{
					// check he doesn't have ammo in an ammo bay
					CASW_Weapon_Ammo_Bag* pAmmoBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pMarine->GetASWWeapon(0));
					if (!pAmmoBag)
						pAmmoBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pMarine->GetASWWeapon(1));
					if (!pAmmoBag || !pAmmoBag->CanGiveAmmoToWeapon(this))
						pMarine->OnWeaponOutOfAmmo(true);
				}
#endif
			}
			else
			{
				info.m_iShots = MIN( info.m_iShots, pMarine->GetAmmoCount( m_iPrimaryAmmoType ) );
				pMarine->RemoveAmmo( info.m_iShots, m_iPrimaryAmmoType );
			}

			info.m_flDistance = asw_weapon_max_shooting_distance.GetFloat();
			info.m_iAmmoType = m_iPrimaryAmmoType;
			info.m_iTracerFreq = 1;
			info.m_flDamageForceScale = asw_weapon_force_scale.GetFloat();
		
			info.m_vecSpread = GetBulletSpread();
			info.m_flDamage = GetWeaponDamage();
#ifndef CLIENT_DLL
			if (asw_debug_marine_damage.GetBool())
				Msg("Weapon dmg = %f\n", info.m_flDamage);
			info.m_flDamage *= pMarine->GetMarineResource()->OnFired_GetDamageScale();
#endif

			pMarine->FireBullets( info );

			//FireBulletsInfo_t info( 1, vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType );
			//info.m_pAttacker = pMarine;

			// Fire the bullets, and force the first shot to be perfectly accuracy
			//pMarine->FireBullets( info );
		}
		else	// projectile pistol
		{
		
#ifndef CLIENT_DLL
			Vector vecSrc = pMarine->Weapon_ShootPosition( );
			Vector vecAiming = vec3_origin;
			if ( pPlayer && pMarine->IsInhabited() )
			{
				vecAiming = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
			}
			else
			{
				vecAiming = pMarine->GetActualShootTrajectory( vecSrc );
			}

			CShotManipulator Manipulator( vecAiming );
			AngularImpulse rotSpeed(0,0,720);
					
			Vector newVel = Manipulator.ApplySpread(GetBulletSpread());
			if ( pMarine->GetWaterLevel() == 3 )
				newVel *= PELLET_WATER_VELOCITY;
			else
				newVel *= PELLET_AIR_VELOCITY;
			newVel *= (1.0 + (0.1 * random->RandomFloat(-1,1)));
			CASW_Shotgun_Pellet::Shotgun_Pellet_Create( vecSrc, QAngle(0,0,0),
					newVel, rotSpeed, pMarine, GetWeaponDamage() );

			// decrement ammo
			m_iClip1 -= 1;
			CASW_Marine *pMarine = GetMarine();
			if (pMarine && m_iClip1 <= 0 && pMarine->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
			{
				// check he doesn't have ammo in an ammo bay
				CASW_Weapon_Ammo_Bag* pAmmoBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pMarine->GetASWWeapon(0));
				if (!pAmmoBag)
					pAmmoBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pMarine->GetASWWeapon(1));
				if (!pAmmoBag || !pAmmoBag->CanGiveAmmoToWeapon(this))
					pMarine->OnWeaponOutOfAmmo(true);
			}
#endif
		}

		// increment shooting stats
#ifndef CLIENT_DLL
		if (pMarine && pMarine->GetMarineResource())
		{
			pMarine->GetMarineResource()->UsedWeapon(this, 1);
			pMarine->OnWeaponFired( this, 1 );
		}
#endif		
	}
	
	if (m_iClip1 > 0)		// only force the fire wait time if we have ammo for another shot
		m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	else
		m_flNextPrimaryAttack = gpGlobals->curtime;
	m_fSlowTime = gpGlobals->curtime + 0.03f;

	if ( m_currentPistol == ASW_WEAPON_PISTOL_LEFT )
	{
		m_currentPistol = ASW_WEAPON_PISTOL_RIGHT;
	}
	else
	{
		m_currentPistol = ASW_WEAPON_PISTOL_LEFT;
	}
}

void CASW_Weapon_Pistol::Precache()
{
	PrecacheModel( "swarm/sprites/whiteglow1.vmt" );
	PrecacheModel( "swarm/sprites/greylaser1.vmt" );
	PrecacheScriptSound("ASW_Pistol.ReloadA");
	PrecacheScriptSound("ASW_Pistol.ReloadB");

	BaseClass::Precache();
}

bool CASW_Weapon_Pistol::ShouldMarineMoveSlow()
{
	return (gpGlobals->curtime < m_fSlowTime);
}

void CASW_Weapon_Pistol::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( m_bInReload )
		return;
	
	CBasePlayer *pOwner = GetCommander();

	if ( pOwner == NULL )
		return;

	//Allow a refire as fast as the player can click
	if ( ( ( pOwner->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime ) )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	}
}

float	CASW_Weapon_Pistol::GetFireRate( void )
{
	CASW_Marine *pMarine = GetMarine();

	float flRate = GetWeaponInfo()->m_flFireRate;

	// player firing rate
	if (!pMarine || pMarine->IsInhabited())
	{
		return flRate;
	}

#ifdef CLIENT_DLL
	return flRate;

#else
	float randomness = 0.1f * random->RandomFloat() - 0.05f;

	// AI firing rate: depends on distance to enemy
	if (!pMarine->GetEnemy())
		return 0.3f + randomness;

	float dist = pMarine->GetAbsOrigin().DistTo(pMarine->GetEnemy()->GetAbsOrigin());
	if (dist > 500)
		return 0.3f + randomness;

	if (dist < 100)
		return 0.14f + randomness;

	float factor = (dist - 100) / 400.0f;
	return 0.14f + factor * 0.16f + randomness;
#endif
}

/*
int CASW_Weapon_Pistol::ASW_SelectWeaponActivity(int idealActivity)
{
	switch( idealActivity )
	{
		//case ACT_IDLE:			idealActivity = ACT_DOD_STAND_IDLE; break;
		//case ACT_CROUCHIDLE:	idealActivity = ACT_DOD_CROUCH_IDLE; break;
		//case ACT_RUN_CROUCH:	idealActivity = ACT_DOD_CROUCHWALK_IDLE; break;
		case ACT_WALK:			idealActivity = ACT_WALK_AIM_PISTOL; break;
		case ACT_RUN:			idealActivity = ACT_RUN_AIM_PISTOL; break;
		case ACT_IDLE:			idealActivity = ACT_IDLE_PISTOL; break;
		case ACT_RELOAD:		idealActivity = ACT_RELOAD_PISTOL; break;
		case ACT_RANGE_ATTACK1:		idealActivity = ACT_RANGE_ATTACK_PISTOL; break;
		default: break;
	}
	return idealActivity;
}
*/

int CASW_Weapon_Pistol::ASW_SelectWeaponActivity(int idealActivity)
{
	switch( idealActivity )
	{
	case ACT_WALK:			idealActivity = ACT_MP_WALK_ITEM1; break;
	case ACT_RUN:			idealActivity = ACT_MP_RUN_ITEM1; break;
	case ACT_IDLE:			idealActivity = ACT_MP_STAND_ITEM1; break;
	case ACT_RELOAD:		idealActivity = ACT_RELOAD_PISTOL; break;	// short (pistol) reload
	case ACT_RANGE_ATTACK1:		idealActivity = ACT_MP_ATTACK_STAND_ITEM1; break;
	case ACT_CROUCHIDLE:		idealActivity = ACT_MP_CROUCHWALK_ITEM1; break;			
	case ACT_JUMP:		idealActivity = ACT_MP_JUMP_ITEM1; break;
	default:
		return BaseClass::ASW_SelectWeaponActivity(idealActivity);
	}
	return idealActivity;
}

float CASW_Weapon_Pistol::GetWeaponDamage()
{
	//float flDamage = 18.0f;
	float flDamage = GetWeaponInfo()->m_flBaseDamage;

	if ( GetMarine() )
	{
		flDamage += MarineSkills()->GetSkillBasedValueByMarine(GetMarine(), ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_PISTOL_DMG);
	}

	//CALL_ATTRIB_HOOK_FLOAT( flDamage, mod_damage_done );

	return flDamage;
}


#ifdef CLIENT_DLL
const char* CASW_Weapon_Pistol::GetPartialReloadSound(int iPart)
{
	switch (iPart)
	{
		case 2: return "ASW_Pistol.ReloadB"; break;
		case 1: return NULL;	// no sound in the middle
		default: break;
	};
	return "ASW_Pistol.ReloadA";
}
#endif

// user message based tracer type
const char* CASW_Weapon_Pistol::GetUTracerType()
{
	//return "ASWUTracerDual";
	if ( m_currentPistol == ASW_WEAPON_PISTOL_RIGHT )
	{
		return "ASWUTracerDualRight";
	}
	else
	{
		return "ASWUTracerDualLeft";
	}
}
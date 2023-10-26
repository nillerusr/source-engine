#include "cbase.h"
#include "asw_weapon_fire_extinguisher_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "asw_extinguisher_projectile.h"
#include "EntityFlame.h"
#include "te_effect_dispatch.h"
#include "IEffects.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_FireExtinguisher, DT_ASW_Weapon_FireExtinguisher )

BEGIN_NETWORK_TABLE( CASW_Weapon_FireExtinguisher, DT_ASW_Weapon_FireExtinguisher )
#ifdef CLIENT_DLL
	// recvprops
#else
	// sendprops
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_FireExtinguisher )
	
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_fire_extinguisher, CASW_Weapon_FireExtinguisher );
PRECACHE_WEAPON_REGISTER(asw_weapon_fire_extinguisher);

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_FireExtinguisher )
	
END_DATADESC()

#endif /* not client */

CASW_Weapon_FireExtinguisher::CASW_Weapon_FireExtinguisher()
{
	m_fMinRange1	= 0;
	m_fMaxRange1	= 128;

	m_fMinRange2	= 256;
	m_fMaxRange2	= 512;

	m_flLastFireTime = 0;
}


CASW_Weapon_FireExtinguisher::~CASW_Weapon_FireExtinguisher()
{

}

void CASW_Weapon_FireExtinguisher::Spawn()
{
	BaseClass::Spawn();

#ifdef CLIENT_DLL
	SetNextClientThink( CLIENT_THINK_ALWAYS );
#endif
}

#ifdef CLIENT_DLL
void CASW_Weapon_FireExtinguisher::ClientThink()
{
	BaseClass::ClientThink();

	if ( m_bIsFiring )
	{
		if( !pEffect )
		{
			pEffect = ParticleProp()->Create( "asw_fireextinguisher", PATTACH_POINT_FOLLOW, "muzzle" ); 
			//EmitSound( "ASW_Weapon_Freezer.OnLoop" );
		}
	}
	else
	{
		if ( pEffect )
		{
			pEffect->StopEmission();
			pEffect = NULL;
			//StopSound( "ASW_Weapon_Freezer.OnLoop" );
			//EmitSound( "ASW_Weapon_Freezer.Stop" );
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CASW_Weapon_FireExtinguisher::GetPrimaryAttackActivity( void )
{
	return ACT_VM_PRIMARYATTACK;
}

void CASW_Weapon_FireExtinguisher::PrimaryAttack( void )
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
		//WeaponSound(SINGLE);

		m_bIsFiring = true;

		// tell the marine to tell its weapon to draw the muzzle flash
		pMarine->DoMuzzleFlash();

		// sets the animation on the weapon model iteself
		SendWeaponAnim( GetPrimaryAttackActivity() );

		// sets the animation on the marine holding this weapon
		//pMarine->SetAnimation( PLAYER_ATTACK1 );
#ifndef CLIENT_DLL
		Vector	vecSrc		= pMarine->Weapon_ShootPosition( );
		Vector vecAiming = vec3_origin;
		if ( pPlayer && pMarine->IsInhabited() )
		{
			vecAiming = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
		}
		else
		{
			vecAiming = pMarine->GetActualShootTrajectory( vecSrc );
		}

		//FireBulletsInfo_t info( 7, vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType );
		//info.m_pAttacker = pMarine;

		// Fire the bullets, and force the first shot to be perfectly accuracy
		//pMarine->FireBullets( info );
		CShotManipulator Manipulator( vecAiming );
		AngularImpulse rotSpeed(0,0,720);
		
		// create a pellet at some random spread direction
		//CASW_Shotgun_Pellet *pPellet = 			
		Vector newVel = Manipulator.ApplySpread(GetBulletSpread());
		if ( pMarine->GetWaterLevel() != 3 )
		{
			newVel *= EXTINGUISHER_PROJECTILE_AIR_VELOCITY;
			newVel *= (1.0 + (0.1 * random->RandomFloat(-1,1)));
			// aim it downwards a bit
			newVel.z -= 40.0f;
			CASW_Extinguisher_Projectile::Extinguisher_Projectile_Create( vecSrc, QAngle(0,0,0),
					newVel, rotSpeed, pMarine );

			// check for putting outselves out
			if (pMarine->IsOnFire())
			{
				CEntityFlame *pFireChild = dynamic_cast<CEntityFlame *>( pMarine->GetEffectEntity() );
				if ( pFireChild )
				{
					pMarine->SetEffectEntity( NULL );
					UTIL_Remove( pFireChild );
				}
				pMarine->Extinguish();
				// spawn a cloud effect on this marine
				CEffectData	data;
				data.m_vOrigin = pMarine->GetAbsOrigin();
				//data.m_nEntIndex = pMarine->entindex();
				CPASFilter filter( data.m_vOrigin );
				filter.SetIgnorePredictionCull(true);
				DispatchEffect( filter, 0.0, "ExtinguisherCloud", data );		
			}
		}
#endif
		// decrement ammo
		m_iClip1 -= 1;

		if (!m_iClip1 && pMarine->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		{
			// HEV suit - indicate out of ammo condition
			if (pPlayer)
				pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
		}
	}
	else if (pPlayer)		// firing from a player
	{
		// MUST call sound before removing a round from the clip of a CMachineGun
		//WeaponSound(SINGLE);

		pPlayer->DoMuzzleFlash();

		SendWeaponAnim( GetPrimaryAttackActivity() );

		// player "shoot" animation
		pPlayer->SetAnimation( PLAYER_ATTACK1 );

		Vector	vecSrc		= pPlayer->Weapon_ShootPosition( );
		CBasePlayer* pBasePlayer = dynamic_cast<CBasePlayer*>(pPlayer);
		Vector	vecAiming	= pBasePlayer->GetAutoaimVector( GetAutoAimAmount() );

		FireBulletsInfo_t info( 7, vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType );
		info.m_pAttacker = pPlayer;

		// Fire the bullets, and force the first shot to be perfectly accuracy
		pPlayer->FireBullets( info );

		if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		{
			// HEV suit - indicate out of ammo condition
			pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
		}

		//Add our view kick in
		AddViewKick();
	}
	if (m_iClip1 > 0)		// only force the fire wait time if we have ammo for another shot
		m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	else
		m_flNextPrimaryAttack = gpGlobals->curtime;

	m_flLastFireTime = gpGlobals->curtime;
}

void CASW_Weapon_FireExtinguisher::Precache()
{
	PrecacheModel( "swarm/sprites/whiteglow1.vmt" );
	PrecacheModel( "swarm/sprites/greylaser1.vmt" );
	PrecacheParticleSystem( "asw_fireextinguisher" );

	BaseClass::Precache();
}

#ifdef CLIENT_DLL
 // if true, the marine emits fire extinguisher smoke from his emitter
bool CASW_Weapon_FireExtinguisher::ShouldMarineFireExtinguish()
{
	return IsFiring();
}
#endif
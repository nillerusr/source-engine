#include "cbase.h"
#include "asw_weapon_flamer_shared.h"
#include "in_buttons.h"

#include "asw_marine_skills.h"
#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "c_asw_generic_emitter_entity.h"
#include "idebugoverlaypanel.h"
#include "engine/IVDebugOverlay.h"
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "asw_marine_resource.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "asw_flamer_projectile.h"
#include "asw_fire.h"
#include "asw_marine_speech.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "asw_gamerules.h"
#include "asw_extinguisher_projectile.h"
#include "EntityFlame.h"
#include "te_effect_dispatch.h"
#include "IEffects.h"
#endif
#include "asw_weapon_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Flamer, DT_ASW_Weapon_Flamer )

BEGIN_NETWORK_TABLE( CASW_Weapon_Flamer, DT_ASW_Weapon_Flamer )
#ifdef CLIENT_DLL
	// recvprops
	RecvPropBool(RECVINFO(m_bBulletMod)),
	RecvPropBool(RECVINFO(m_bIsSecondaryFiring)),
#else
	// sendprops
	SendPropBool(SENDINFO(m_bBulletMod)),
	SendPropBool(SENDINFO(m_bIsSecondaryFiring)),
	SendPropExclude( "DT_BaseAnimating", "m_nSkin" ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Flamer )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_bBulletMod, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bIsSecondaryFiring, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_flamer, CASW_Weapon_Flamer );
PRECACHE_WEAPON_REGISTER(asw_weapon_flamer);

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Flamer )
	
END_DATADESC()

#endif /* not client */

CASW_Weapon_Flamer::CASW_Weapon_Flamer()
{
	m_fMinRange1	= 0;
	m_fMaxRange1	= 320;

	m_fMinRange2	= 256;
	m_fMaxRange2	= 1024;

	m_flLastFireTime = 0;
	m_bBulletMod = false;
}


CASW_Weapon_Flamer::~CASW_Weapon_Flamer()
{
#ifdef CLIENT_DLL
	if ( m_hPilotLight )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_hPilotLight );
		m_hPilotLight = NULL;
	}
#endif
}

void CASW_Weapon_Flamer::Spawn()
{
	BaseClass::Spawn();
}

#ifdef CLIENT_DLL
void CASW_Weapon_Flamer::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );

		if ( !m_hPilotLight )
		{
			m_hPilotLight = ParticleProp()->Create( "asw_flamethrower_pilot_sml", PATTACH_POINT_FOLLOW, "flame" );

			if ( m_hPilotLight )
				m_hPilotLight->SetControlPoint( 1, Vector( 1, 0, 0 ) );
		}
	}

	ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( GET_ACTIVE_SPLITSCREEN_SLOT() );

	if ( m_hPilotLight )
	{
		int iPilot = (m_bSwitchingWeapons || !IsVisible() || !GetMarine() || m_bIsFiring || m_bIsSecondaryFiring || IsReloading() || (Clip1() <= 0)) ? 0 : 1;

		m_hPilotLight->SetControlPoint( 1, Vector( iPilot, 0, 0 ) );
	}
}

void CASW_Weapon_Flamer::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();

	if ( m_hPilotLight )
	{
		ParticleProp()->StopEmissionAndDestroyImmediately( m_hPilotLight );
		m_hPilotLight = NULL;
	}
}

void CASW_Weapon_Flamer::ClientThink()
{
	BaseClass::ClientThink();

	if ( m_bIsFiring )
	{
		if( !pEffect )
		{
			pEffect = ParticleProp()->Create( "asw_flamethrower", PATTACH_POINT_FOLLOW, "muzzle" ); 
		}
	}
	else
	{
		if ( pEffect )
		{
			pEffect->StopEmission();
			pEffect = NULL;
		}
	}

	if ( m_bIsSecondaryFiring )
	{
		if( !pExtinguishEffect )
		{
			pExtinguishEffect = ParticleProp()->Create( "asw_fireextinguisher", PATTACH_POINT_FOLLOW, "muzzle" ); 
		}
	}
	else
	{
		if ( pExtinguishEffect )
		{
			pExtinguishEffect->StopEmission();
			pExtinguishEffect = NULL;
		}
	}
}
#endif

void CASW_Weapon_Flamer::ClearIsFiring()
{
	BaseClass::ClearIsFiring();

	m_bIsSecondaryFiring = false;
}

void CASW_Weapon_Flamer::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	CASW_Marine* pOwner = GetMarine();

	if (!pOwner)
		return;

	bool bAttack1, bAttack2, bReload, bOldReload, bOldAttack1;
	GetButtons(bAttack1, bAttack2, bReload, bOldReload, bOldAttack1 );

	if ( !bAttack2 )
	{
		m_bIsSecondaryFiring = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CASW_Weapon_Flamer::GetPrimaryAttackActivity( void )
{
	return ACT_VM_PRIMARYATTACK;
}


void CASW_Weapon_Flamer::PrimaryAttack( void )
{
	// If my clip is empty (and I use clips) start reload
	if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	{
		Reload();
		return;
	}

	if ( m_bIsSecondaryFiring.Get() )
	{
		m_bIsFiring = false;
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

		// Fire the bullets, and force the first shot to be perfectly accuracy
		CShotManipulator Manipulator( vecAiming );
		AngularImpulse rotSpeed(0,0,720);
		
		// create a pellet at some random spread direction		
		Vector newVel = Manipulator.ApplySpread(GetBulletSpread());
		if ( pMarine->GetWaterLevel() != 3 )
		{
			newVel *= FLAMER_PROJECTILE_AIR_VELOCITY;
			newVel *= (1.0 + (0.1 * random->RandomFloat(-1,1)));
			CASW_Flamer_Projectile::Flamer_Projectile_Create( GetWeaponDamage(), vecSrc, QAngle(0,0,0),
					newVel, rotSpeed, pMarine, pMarine, this );

			if (ASWGameRules())
				ASWGameRules()->m_fLastFireTime = gpGlobals->curtime;

			pMarine->OnWeaponFired( this, 1 );
		}
#endif

		if (!m_bBulletMod)
		{
			// decrement ammo
			m_iClip1 -= 1;
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
		m_bBulletMod = !m_bBulletMod;

		if (!m_iClip1 && pMarine->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		{
			// HEV suit - indicate out of ammo condition
			if (pPlayer)
				pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
		}
	}
	
	if (m_iClip1 > 0)		// only force the fire wait time if we have ammo for another shot
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
		m_flNextSecondaryAttack = gpGlobals->curtime + GetFireRate();
	}
	else
	{
		m_flNextPrimaryAttack = gpGlobals->curtime;
	}

	m_flLastFireTime = gpGlobals->curtime;
}

void CASW_Weapon_Flamer::SecondaryAttack( void )
{
	// If my clip is empty (and I use clips) start reload
	if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	{
		Reload();
		return;
	}

	// clear primary fire if we're secondary firing
	if ( m_bIsFiring )
	{
		m_bIsFiring = false;
		return;
	}

	CASW_Player *pPlayer = GetCommander();
	CASW_Marine *pMarine = GetMarine();

	if (pMarine)		// firing from a marine
	{
		// MUST call sound before removing a round from the clip of a CMachineGun
		//WeaponSound(SINGLE);

		m_bIsSecondaryFiring = true;

		// tell the marine to tell its weapon to draw the muzzle flash
		pMarine->DoMuzzleFlash();

		// sets the animation on the weapon model iteself
		SendWeaponAnim( GetPrimaryAttackActivity() );

		// sets the animation on the marine holding this weapon
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

		CShotManipulator Manipulator( vecAiming );
		AngularImpulse rotSpeed(0,0,720);

		// create a pellet at some random spread direction			
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

			pMarine->OnWeaponFired( this, 1, true );
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
	if (m_iClip1 > 0)		// only force the fire wait time if we have ammo for another shot
	{
		m_flNextSecondaryAttack = gpGlobals->curtime + GetFireRate();
		m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	}
	else
	{
		m_flNextSecondaryAttack = gpGlobals->curtime;
	}

	m_flLastFireTime = gpGlobals->curtime;
}

void CASW_Weapon_Flamer::Precache()
{
	PrecacheModel( "swarm/sprites/whiteglow1.vmt" );
	PrecacheModel( "swarm/sprites/greylaser1.vmt" );
	PrecacheScriptSound("ASW_Flamer.ReloadA");
	PrecacheScriptSound("ASW_Flamer.ReloadB");
	PrecacheScriptSound("ASW_Flamer.ReloadC");
	PrecacheParticleSystem( "asw_flamethrower" );
	PrecacheParticleSystem( "asw_fireextinguisher" );

	BaseClass::Precache();
}

#ifdef CLIENT_DLL
 // if true, the marine emits flames from his flame emitter
bool CASW_Weapon_Flamer::ShouldMarineFlame()
{
	return IsFiring();
}
#endif

bool CASW_Weapon_Flamer::SupportsBayonet()
{
	return true;
}

float CASW_Weapon_Flamer::GetWeaponDamage()
{
	//float flDamage = 35.0f;
	float flDamage = GetWeaponInfo()->m_flBaseDamage;

	if ( GetMarine() )
	{
		flDamage += MarineSkills()->GetSkillBasedValueByMarine(GetMarine(), ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_FLAMER_DMG);
	}

	//CALL_ATTRIB_HOOK_FLOAT( flDamage, mod_damage_done );

	return flDamage;
}

#ifdef CLIENT_DLL
const char* CASW_Weapon_Flamer::GetPartialReloadSound(int iPart)
{
	switch (iPart)
	{
		case 1: return "ASW_Flamer.ReloadB"; break;
		case 2: return "ASW_Flamer.ReloadC"; break;
		default: break;
	};
	return "ASW_Flamer.ReloadA";
}

bool CASW_Weapon_Flamer::Simulate()
{
	BaseClass::Simulate();

	// hide our blue flame tip when appropriate
	m_nSkin = (IsFiring() || m_bIsSecondaryFiring || IsReloading() || (Clip1() <= 0)) ? 0 : 1;

	return true;
}

// if true, the marine emits fire extinguisher smoke from his emitter
bool CASW_Weapon_Flamer::ShouldMarineFireExtinguish()
{
	return m_bIsSecondaryFiring;
}
#endif

float CASW_Weapon_Flamer::GetFireRate()
{
	//float flRate = 0.1f;
	float flRate = GetWeaponInfo()->m_flFireRate;

	//CALL_ATTRIB_HOOK_FLOAT( flRate, mod_fire_rate );

	return flRate;
}

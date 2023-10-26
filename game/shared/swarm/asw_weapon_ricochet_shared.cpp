#include "cbase.h"
#include "asw_weapon_ricochet_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#define CASW_Weapon C_ASW_Weapon
#define CASW_Marine C_ASW_Marine
#define CBasePlayer C_BasePlayer
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "c_asw_marine_resource.h"
#include "precache_register.h"
#include "c_te_effect_dispatch.h"
#include "iviewrender_beams.h"			// laser beam
#else
#include "asw_lag_compensation.h"
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "te_effect_dispatch.h"
#include "asw_marine_resource.h"
#include "asw_marine_speech.h"
#include "decals.h"
#include "ammodef.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "asw_rocket.h"
#endif
#include "asw_bouncing_pellet_shared.h"
#include "shot_manipulator.h"
#include "asw_marine_skills.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Ricochet, DT_ASW_Weapon_Ricochet )

BEGIN_NETWORK_TABLE( CASW_Weapon_Ricochet, DT_ASW_Weapon_Ricochet )
#ifdef CLIENT_DLL
	// recvprops
	RecvPropBool	(RECVINFO(m_bBounceLaser)),
#else
	// sendprops	
	SendPropBool	(SENDINFO(m_bBounceLaser)),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CASW_Weapon_Ricochet )	
	DEFINE_PRED_FIELD( m_bBounceLaser, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( asw_weapon_ricochet, CASW_Weapon_Ricochet );
PRECACHE_WEAPON_REGISTER( asw_weapon_ricochet );

extern ConVar asw_weapon_max_shooting_distance;

#ifndef CLIENT_DLL
extern ConVar asw_debug_marine_damage;

#endif

#define PELLET_AIR_VELOCITY	2500

CASW_Weapon_Ricochet::CASW_Weapon_Ricochet()
{
#ifdef CLIENT_DLL
	for (int i=0;i<ASW_NUM_LASER_BEAMS;i++)
	{
		m_pLaserBeam[i] = NULL;
	}
#endif
}


CASW_Weapon_Ricochet::~CASW_Weapon_Ricochet()
{

}

void CASW_Weapon_Ricochet::Precache()
{	
	BaseClass::Precache();
}

void CASW_Weapon_Ricochet::PrimaryAttack()
{
	// If my clip is empty (and I use clips) start reload
	if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	{		
		Reload();
		return;
	}

	CASW_Player *pPlayer = GetCommander();
	CASW_Marine *pMarine = GetMarine();

	bool bAttack1, bAttack2, bReload, bOldReload, bOldAttack1;
	GetButtons(bAttack1, bAttack2, bReload, bOldReload, bOldAttack1 );

	if (pMarine)		// firing from a marine
	{
		m_bIsFiring = true;

		// MUST call sound before removing a round from the clip of a CMachineGun
		WeaponSound(SINGLE);

		if (m_iClip1 <= AmmoClickPoint())
		{
			LowAmmoSound();
		}

		// tell the marine to tell its weapon to draw the muzzle flash
		pMarine->DoMuzzleFlash();

		// sets the animation on the weapon model iteself
		SendWeaponAnim( GetPrimaryAttackActivity() );

#ifdef GAME_DLL	// check for turning on lag compensation
		if (pPlayer && pMarine->IsInhabited())
		{
			CASW_Lag_Compensation::RequestLagCompensation( pPlayer, pPlayer->GetCurrentUserCommand() );
		}
#endif

#ifdef RICOCHET_SHOTGUN
		//if (bAttack2)
		{
			int iPellets = 7;
			Vector vecSrc = pMarine->Weapon_ShootPosition( );
			Vector vecAiming;
			if ( pPlayer && pMarine->IsInhabited() )
			{
				vecAiming = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
			}
			else
			{
	#ifdef CLIENT_DLL
				Msg("Error, clientside firing of a weapon that's being controlled by an AI marine\n");
	#else
				vecAiming = pMarine->GetActualShootTrajectory( vecSrc );
	#endif
			}
			
			static Vector cone(12,12,12);
			for (int i=0;i<iPellets;i++)
			{
				FireBulletsInfo_t info( 1, vecSrc, vecAiming, cone, asw_weapon_max_shooting_distance.GetFloat(), m_iPrimaryAmmoType );
				info.m_pAttacker = pMarine;
				info.m_iTracerFreq = 1;
				info.m_nFlags = FIRE_BULLETS_NO_PIERCING_SPARK | FIRE_BULLETS_HULL | FIRE_BULLETS_ANGULAR_SPREAD;
				info.m_flDamage = GetWeaponDamage();
	#ifndef CLIENT_DLL
				if (asw_debug_marine_damage.GetBool())
					Msg("Weapon dmg = %f\n", info.m_flDamage);
				info.m_flDamage *= pMarine->GetMarineResource()->OnFired_GetDamageScale();
	#endif
				// shotgun bullets have a base 50% chance of piercing
				//float fPiercingChance = 0.5f;
				//if (pMarine->GetMarineResource() && pMarine->GetMarineProfile() && pMarine->GetMarineProfile()->GetMarineClass() == MARINE_CLASS_SPECIAL_WEAPONS)
					//fPiercingChance += MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_PIERCING);
				
				//pMarine->FirePenetratingBullets(info, 5, fPiercingChance);
				pMarine->FireBouncingBullets(info, 1, i);
			}
			// increment shooting stats
			

			// pellet based
//#ifndef CLIENT_DLL
			/*
			CShotManipulator Manipulator( vecAiming );
					
			for (int i=0;i<iPellets;i++)
			{
				// create a pellet at some random spread direction
				//CASW_Shotgun_Pellet *pPellet = 			
				Vector newVel = Manipulator.ApplySpread(VECTOR_CONE_20DEGREES);
				//Vector newVel = ApplySpread( vecAiming, GetBulletSpread() );
				newVel *= PELLET_AIR_VELOCITY;
				newVel *= (1.0 + (0.1 * random->RandomFloat(-1,1)));
				CreatePellet(vecSrc, newVel, pMarine);

				if (i == 0)
				{
					Msg("[%s] New pellet, vel = %s\n", IsServer() ? "s" : "c", VecToString(newVel));
				}
				
				//CASW_Shotgun_Pellet_Predicted::CreatePellet(vecSrc, newVel, pPlayer, pMarine);
			}
			*/
//#endif

#ifndef CLIENT_DLL
			if (pMarine && pMarine->GetMarineResource())
			{
				pMarine->GetMarineResource()->UsedWeapon(this, 1);
				pMarine->OnWeaponFired( this );
			}
#endif
			m_flNextPrimaryAttack = gpGlobals->curtime + 1.0f;
			return;
		}

#endif		// RICOCHET SHOTGUN

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

		// To make the firing framerate independent, we may have to fire more than one bullet here on low-framerate systems, 
		// especially if the weapon we're firing has a really fast rate of fire.
		info.m_iShots = 0;
		float fireRate = GetFireRate();
		while ( m_flNextPrimaryAttack <= gpGlobals->curtime )
		{
			m_flNextPrimaryAttack = m_flNextPrimaryAttack + fireRate;
			info.m_iShots++;
			if ( !fireRate )
				break;
		}

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
		info.m_iTracerFreq = 1;  // asw tracer test everytime
		info.m_vecSpread = pMarine->GetActiveWeapon()->GetBulletSpread();
		info.m_flDamage = GetWeaponDamage();
#ifndef CLIENT_DLL
		if (asw_debug_marine_damage.GetBool())
			Msg("Weapon dmg = %f\n", info.m_flDamage);
		info.m_flDamage *= pMarine->GetMarineResource()->OnFired_GetDamageScale();
#endif

		// make bullets bounce if secondary fire is held down
		if (bAttack2)
			pMarine->FireBouncingBullets( info, 2 );
		else
			pMarine->FireBullets( info );

		// increment shooting stats
#ifndef CLIENT_DLL
		if (pMarine && pMarine->GetMarineResource())
		{
			pMarine->GetMarineResource()->UsedWeapon(this, info.m_iShots);
			pMarine->OnWeaponFired( this, info.m_iShots );
		}
#endif
	}	
}

void CASW_Weapon_Ricochet::SecondaryAttack()
{
	CASW_Player *pPlayer = GetCommander();
	if (!pPlayer)
		return;

	CASW_Marine *pMarine = GetMarine();
	if (!pMarine)
		return;


	// dry fire
	SendWeaponAnim( ACT_VM_DRYFIRE );
	BaseClass::WeaponSound( EMPTY );
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
}

void CASW_Weapon_Ricochet::ItemPostFrame()
{
	CASW_Marine* pOwner = GetMarine();
	
	if (!pOwner)
		return;

	bool bAttack1, bAttack2, bReload, bOldReload, bOldAttack1;
	GetButtons(bAttack1, bAttack2, bReload, bOldReload, bOldAttack1 );

	// check for clearing our firing bool from reloading
	if (m_bInReload && gpGlobals->curtime > m_fReloadClearFiringTime)
		m_bIsFiring = false;

	if ( m_bShotDelayed && gpGlobals->curtime > m_flDelayedFire )
	{
		DelayedAttack();
	}

	m_fFireDuration = bAttack1 ? ( m_fFireDuration + gpGlobals->frametime ) : 0.0f;

	if ( UsesClipsForAmmo1() )
	{
		CheckReload();
	}

	bool bFired = false;

	// secondary fire makes our bounce targeting laser come on
	m_bBounceLaser = bAttack2;
	
	if ( !bFired && bAttack1 && (m_flNextPrimaryAttack <= gpGlobals->curtime) && gpGlobals->curtime > pOwner->m_fFFGuardTime)
	{
		// Clip empty? Or out of ammo on a no-clip weapon?
		if ( !IsMeleeWeapon() &&  
			(( UsesClipsForAmmo1() && m_iClip1 <= 0) || ( !UsesClipsForAmmo1() && pOwner->GetAmmoCount(m_iPrimaryAmmoType)<=0 )) )
		{
			HandleFireOnEmpty();
		}
		else if (pOwner->GetWaterLevel() == 3 && m_bFiresUnderwater == false)
		{
			// This weapon doesn't fire underwater
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
			return;
		}
		else
		{
			// If the firing button was just pressed, reset the firing time
			if ( pOwner && bAttack1 )
			{
				 m_flNextPrimaryAttack = gpGlobals->curtime;
			}

			PrimaryAttack();
		}
	}

	if (!bAttack1)	// clear our firing var if we don't have the attack button held down (not totally accurate since firing could continue for some time after pulling the trigger, but it's good enough for our purposes)
		m_bIsFiring = false;

	if ( bReload && UsesClipsForAmmo1())	
	{
		if ( m_bInReload ) 
		{
			// todo: check for a fast reload
			Msg("Check for fast reload\n");
		}
		else
		{
			// reload when reload is pressed, or if no buttons are down and weapon is empty.
			Reload();
			m_fFireDuration = 0.0f;
		}
	}

	//  No buttons down
	if (!(bAttack1 || bAttack2 || bReload))
	{
		// no fire buttons down or reloading
		if ( !ReloadOrSwitchWeapons() && ( m_bInReload == false ) )
		{
			WeaponIdle();
		}
	}
}

#ifdef CLIENT_DLL
int CASW_Weapon_Ricochet::DrawModel( int flags, const RenderableInstance_t &instance )
{
	return BaseClass::DrawModel(flags, instance);
}

ConVar asw_ricochet_laser_width("asw_ricochet_laser_width", "1.5f", FCVAR_CHEAT, "Width of the ricochet targeting laser");
ConVar asw_ricochet_laser_fade("asw_ricochet_laser_fade", "0.5f", FCVAR_CHEAT, "Width of the ricochet targeting laser");
ConVar asw_ricochet_laser_brightness("asw_ricochet_laser_brightness", "255.0f", FCVAR_CHEAT, "Width of the ricochet targeting laser");
ConVar asw_ricochet_laser_r("asw_ricochet_laser_r", "255", 0, "Red component of ricochet rifle bounce laser");
ConVar asw_ricochet_laser_g("asw_ricochet_laser_g", "0", 0, "Red component of ricochet rifle bounce laser");
ConVar asw_ricochet_laser_b("asw_ricochet_laser_b", "0", 0, "Red component of ricochet rifle bounce laser");


void CASW_Weapon_Ricochet::UpdateBounceLaser()
{
	CASW_Player *pPlayer = GetCommander();
	CASW_Marine *pMarine = GetMarine();
	if (!pPlayer || !pMarine)
		return;
	
	if (!pMarine->IsInhabited())
	{
		ReleaseLaserBeam();
		return;
	}

	Vector vecSrc = pMarine->Weapon_ShootPosition( );
	Vector vecDir = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());
	trace_t tr;
	UTIL_TraceLine(vecSrc, vecSrc + vecDir * asw_weapon_max_shooting_distance.GetFloat(), MASK_SHOT, pMarine, COLLISION_GROUP_NONE, &tr);

	//static Vector s_vecLastSrc = vec3_origin;
	//Msg("lasersrc = %s", VecToString(vecSrc));
	//if (s_vecLastSrc != vecSrc)
	//{
		//Msg(" source changed");
		//s_vecLastSrc = vecSrc;
	//}
	//Msg("\n");

	for (int i=0;i<ASW_NUM_LASER_BEAMS;i++)
	{
		if (m_pLaserBeam[i] == NULL)
		{
			BeamInfo_t beamInfo;
			beamInfo.m_nType = TE_BEAMPOINTS;
			beamInfo.m_vecStart = tr.startpos;
			beamInfo.m_vecEnd = tr.endpos;
			beamInfo.m_pszModelName = "sprites/glow01.vmt";
			beamInfo.m_pszHaloName = "sprites/glow01.vmt";
			beamInfo.m_flHaloScale = 3.0;
			beamInfo.m_flWidth =  asw_ricochet_laser_width.GetFloat();
			beamInfo.m_flEndWidth =  asw_ricochet_laser_width.GetFloat();
			beamInfo.m_flFadeLength = asw_ricochet_laser_fade.GetFloat();
			beamInfo.m_flAmplitude = 0;
			beamInfo.m_flBrightness = asw_ricochet_laser_brightness.GetFloat();
			beamInfo.m_flSpeed = 0.0f;
			beamInfo.m_nStartFrame = 0.0;
			beamInfo.m_flFrameRate = 0.0;
			beamInfo.m_flRed = asw_ricochet_laser_r.GetFloat();
			beamInfo.m_flGreen = asw_ricochet_laser_g.GetFloat();
			beamInfo.m_flBlue = asw_ricochet_laser_b.GetFloat();
			beamInfo.m_nSegments = 8;
			beamInfo.m_bRenderable = true;
			beamInfo.m_flLife = 0.5;
			beamInfo.m_nFlags = FBEAM_FOREVER | FBEAM_ONLYNOISEONCE | FBEAM_NOTILE | FBEAM_HALOBEAM;
			
			m_pLaserBeam[i] = beams->CreateBeamPoints( beamInfo );
		}
		else
		{
			BeamInfo_t beamInfo;
			beamInfo.m_vecStart = tr.startpos;
			beamInfo.m_vecEnd = tr.endpos;
			beamInfo.m_flRed = asw_ricochet_laser_r.GetFloat();
			beamInfo.m_flGreen = asw_ricochet_laser_g.GetFloat();
			beamInfo.m_flBlue = asw_ricochet_laser_b.GetFloat();

			beams->UpdateBeamInfo( m_pLaserBeam[i], beamInfo );
		}

		vecSrc = tr.endpos;
		if (tr.DidHit())
		{
			Vector vecNewDir = vecDir;
			// reflect the X+Y off the surface (leave Z intact so the shot is more likely to stay flat and hit enemies)
			float proj = (vecNewDir).Dot( tr.plane.normal );
			VectorMA( vecNewDir, -proj*2, tr.plane.normal, vecNewDir );
			vecDir.x = vecNewDir.x;
			vecDir.y = vecNewDir.y;
		}
		UTIL_TraceLine(vecSrc, vecSrc + vecDir * asw_weapon_max_shooting_distance.GetFloat(), MASK_SHOT, pMarine, COLLISION_GROUP_NONE, &tr);
	}
}

void CASW_Weapon_Ricochet::ReleaseLaserBeam()
{
	for (int i=0;i<ASW_NUM_LASER_BEAMS;i++)
	{
		if( m_pLaserBeam[i] )
		{
			m_pLaserBeam[i]->flags = 0;
			m_pLaserBeam[i]->die = gpGlobals->curtime - 1;

			m_pLaserBeam[i] = NULL;
		}
	}
}

void CASW_Weapon_Ricochet::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	// Remove all addon models if we go out of the PVS.
	if ( state == SHOULDTRANSMIT_END )
	{
		ReleaseLaserBeam();
	}

	BaseClass::NotifyShouldTransmit( state );
}

bool CASW_Weapon_Ricochet::Simulate()
{
	// calculate the bounce targeting beam if it's active
	if (m_bBounceLaser.Get() && !IsFiring())
	{
		UpdateBounceLaser();
	}
	else
	{
		ReleaseLaserBeam();
	}
	
	BaseClass::Simulate();

	return true;
}
#endif


CASW_Bouncing_Pellet*  CASW_Weapon_Ricochet::CreatePellet(Vector vecSrc, Vector newVel, CASW_Marine *pMarine)
{
	if (!pMarine)
		return NULL;
	AngularImpulse rotSpeed(0,0,720);
	float flDamage = GetWeaponDamage();
#ifdef GAME_DLL
	if (asw_debug_marine_damage.GetBool())
		Msg("Weapon dmg = %f\n", flDamage);

	flDamage *= pMarine->GetMarineResource()->OnFired_GetDamageScale();
#endif
	return CASW_Bouncing_Pellet::CreatePellet( vecSrc, newVel, pMarine, flDamage);
}
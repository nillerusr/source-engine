#include "cbase.h"
#include "asw_weapon_flechette_shared.h"
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
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "te_effect_dispatch.h"
#include "asw_marine_resource.h"
#include "asw_marine_speech.h"
#include "decals.h"
#include "ammodef.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "asw_rocket.h"
#include "asw_gamerules.h"
#endif
#include "asw_marine_skills.h"
#include "asw_weapon_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Flechette, DT_ASW_Weapon_Flechette )

BEGIN_NETWORK_TABLE( CASW_Weapon_Flechette, DT_ASW_Weapon_Flechette )
#ifdef CLIENT_DLL
	// recvprops
#else
	// sendprops	
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CASW_Weapon_Flechette )	
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( asw_weapon_flechette, CASW_Weapon_Flechette );
PRECACHE_WEAPON_REGISTER( asw_weapon_flechette );

extern ConVar asw_weapon_max_shooting_distance;

#ifndef CLIENT_DLL
extern ConVar asw_debug_marine_damage;

#endif

CASW_Weapon_Flechette::CASW_Weapon_Flechette()
{
}


CASW_Weapon_Flechette::~CASW_Weapon_Flechette()
{

}

void CASW_Weapon_Flechette::Precache()
{	
	BaseClass::Precache();
	PrecacheScriptSound( "ASWRocket.Explosion" );
	PrecacheParticleSystem( "small_fire_burst" );
}

void CASW_Weapon_Flechette::PrimaryAttack()
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

		Vector vecDir;
		Vector vecSrc	 = pMarine->Weapon_ShootPosition( );
		if ( pPlayer && pMarine->IsInhabited() )
		{
			vecDir = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
		}
		else
		{
#ifdef CLIENT_DLL
			Msg("Error, clientside firing of a weapon that's being controlled by an AI marine\n");
#else
			vecDir = pMarine->GetActualShootTrajectory( vecSrc );
#endif
		}
		
		int iShots = 1;

		// Make sure we don't fire more than the amount in the clip
		if ( UsesClipsForAmmo1() )
		{
			iShots = MIN( iShots, m_iClip1 );
			m_iClip1 -= iShots;

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
			iShots = MIN( iShots, pMarine->GetAmmoCount( m_iPrimaryAmmoType ) );
			pMarine->RemoveAmmo( iShots, m_iPrimaryAmmoType );
		}

/*#ifndef CLIENT_DLL
		if (asw_debug_marine_damage.GetBool())
			Msg("Weapon dmg = %d\n", info.m_flDamage);
		info.m_flDamage *= pMarine->GetMarineResource()->OnFired_GetDamageScale();
#endif*/
				
		// increment shooting stats
#ifndef CLIENT_DLL
		float fGrenadeDamage = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_FLECHETTE_DMG);

		QAngle vecRocketAngle;
		VectorAngles(vecDir, vecRocketAngle);
		vecRocketAngle[YAW] += random->RandomFloat(-10, 10);
		CASW_Rocket::Create(fGrenadeDamage, vecSrc, vecRocketAngle, GetMarine());

		if (pMarine && pMarine->GetMarineResource())
		{
			pMarine->GetMarineResource()->UsedWeapon(this, iShots);
			pMarine->OnWeaponFired( this, iShots );
		}

		if (ASWGameRules())
			ASWGameRules()->m_fLastFireTime = gpGlobals->curtime;
#endif
		m_flNextPrimaryAttack = m_flNextPrimaryAttack + GetFireRate();
	}	
}

void CASW_Weapon_Flechette::SecondaryAttack()
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

float CASW_Weapon_Flechette::GetFireRate()
{
	//float flRate = 0.125f;
	float flRate = GetWeaponInfo()->m_flFireRate;

	//CALL_ATTRIB_HOOK_FLOAT( flRate, mod_fire_rate );

	return flRate;
}
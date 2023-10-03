#include "cbase.h"
#include "asw_weapon_pdw_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "c_te_legacytempents.h"
#include "c_breakableprop.h"
#include "fx.h"
#include "c_asw_fx.h"
#define CASW_Marine C_ASW_Marine
#else
#include "asw_lag_compensation.h"
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "asw_marine_resource.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "asw_marine_speech.h"
#include "asw_weapon_ammo_bag_shared.h"
#endif
#include "asw_marine_skills.h"
#include "asw_weapon_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_PDW, DT_ASW_Weapon_PDW )

BEGIN_NETWORK_TABLE( CASW_Weapon_PDW, DT_ASW_Weapon_PDW )
#ifdef CLIENT_DLL
	// recvprops
	RecvPropBool(RECVINFO(m_bBulletMod)),
#else
	// sendprops
	SendPropBool(SENDINFO(m_bBulletMod)),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_PDW )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_bBulletMod, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_pdw, CASW_Weapon_PDW );
PRECACHE_WEAPON_REGISTER(asw_weapon_pdw);

ConVar asw_pdw_max_shooting_distance( "asw_pdw_max_shooting_distance", "400", FCVAR_REPLICATED, "Maximum distance of the hitscan weapons." );
extern ConVar asw_weapon_max_shooting_distance;
extern ConVar asw_weapon_force_scale;

CASW_Weapon_PDW::CASW_Weapon_PDW()
{
	m_bBulletMod = false;
}


CASW_Weapon_PDW::~CASW_Weapon_PDW()
{

}

#ifndef CLIENT_DLL
extern ConVar asw_debug_marine_damage;
//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_PDW )
	
END_DATADESC()

void CASW_Weapon_PDW::Spawn()
{
	BaseClass::Spawn();
}

// just dry fire by default
void CASW_Weapon_PDW::SecondaryAttack( void )
{
	// Only the player fires this way so we can cast
	CASW_Player *pPlayer = GetCommander();
	if (!pPlayer)
		return;

	CASW_Marine *pMarine = GetMarine();
	if (!pMarine)
		return;

	SendWeaponAnim( ACT_VM_DRYFIRE );
	BaseClass::WeaponSound( EMPTY );
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
}

#endif /* not client */

void CASW_Weapon_PDW::Precache()
{
	// precache the weapon model here?

	BaseClass::Precache();
}

void CASW_Weapon_PDW::PrimaryAttack()
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

		// sets the animation on the marine holding this weapon
		//pMarine->SetAnimation( PLAYER_ATTACK1 );

#ifdef GAME_DLL	// check for turning on lag compensation
		if (pPlayer && pMarine->IsInhabited())
		{
			CASW_Lag_Compensation::RequestLagCompensation( pPlayer, pPlayer->GetCurrentUserCommand() );
		}
#endif

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

			for (int k=0;k<info.m_iShots;k++)
			{
				//if (m_bBulletMod)
				{
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
				//m_bBulletMod = !m_bBulletMod;
			}
		}
		else
		{
			info.m_iShots = MIN( info.m_iShots, pMarine->GetAmmoCount( m_iPrimaryAmmoType ) );
			for (int k=0;k<info.m_iShots;k++)
			{
				if (m_bBulletMod)
					pMarine->RemoveAmmo( info.m_iShots, m_iPrimaryAmmoType );;
				m_bBulletMod = !m_bBulletMod;
			}
		}

		info.m_flDistance = asw_weapon_max_shooting_distance.GetFloat();
		info.m_iAmmoType = m_iPrimaryAmmoType;
		info.m_iTracerFreq = 1;   // asw tracer test everytime
		info.m_vecSpread = pMarine->GetActiveWeapon()->GetBulletSpread();
		info.m_flDamage = GetWeaponDamage();
		info.m_flDamageForceScale = asw_weapon_force_scale.GetFloat();
#ifndef CLIENT_DLL
		if (asw_debug_marine_damage.GetBool())
			Msg("Weapon dmg = %f\n", info.m_flDamage);
		info.m_flDamage *= pMarine->GetMarineResource()->OnFired_GetDamageScale();
#endif

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

int CASW_Weapon_PDW::ASW_SelectWeaponActivity(int idealActivity)
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



float CASW_Weapon_PDW::GetWeaponDamage()
{
	//float flDamage = 18.0f;
	float flDamage = GetWeaponInfo()->m_flBaseDamage;

	if ( GetMarine() )
	{
		flDamage += MarineSkills()->GetSkillBasedValueByMarine(GetMarine(), ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_PDW_DMG);
	}

	//CALL_ATTRIB_HOOK_FLOAT( flDamage, mod_damage_done );

	return flDamage;
}

#ifdef CLIENT_DLL
void CASW_Weapon_PDW::OnMuzzleFlashed()
{
	BaseClass::OnMuzzleFlashed();

	// UNDONE: flash both guns from ASWUTracerDual 
// 	int iAttachment = LookupAttachment( "muzzle_flash_l" );	
// 	if ( iAttachment > 0 )
// 	{
// 		float flScale = GetMuzzleFlashScale();		
// 		if (GetMuzzleFlashRed())
// 		{			
// 			FX_ASW_RedMuzzleEffectAttached( flScale, GetRefEHandle(), iAttachment, NULL, false );
// 		}
// 		else
// 		{
// 			FX_ASW_MuzzleEffectAttached( flScale, GetRefEHandle(), iAttachment, NULL, false );
// 		}
// 	}
}

float CASW_Weapon_PDW::GetMuzzleFlashScale( void )
{
	return BaseClass::GetMuzzleFlashScale();
	/*
	// if we haven't calculated the muzzle scale based on the carrying marine's skill yet, then do so
	if (m_fMuzzleFlashScale == -1)
	{
		C_ASW_Marine *pMarine = GetMarine();
		if (pMarine)
			m_fMuzzleFlashScale = 2.0f * MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_MUZZLE);
		else
			return 2.0f;
	}

	return m_fMuzzleFlashScale;
	*/
}

bool CASW_Weapon_PDW::GetMuzzleFlashRed()
{
	return BaseClass::GetMuzzleFlashRed();
	//return ((GetMuzzleFlashScale() / 2.0f) >= 1.15f);	// red if our muzzle flash is the biggest size based on our skill
}
#endif

// user message based tracer type
const char* CASW_Weapon_PDW::GetUTracerType()
{
	return "ASWUTracerDual";
}

float CASW_Weapon_PDW::GetFireRate()
{
	//float flRate = 0.035f;
	float flRate = GetWeaponInfo()->m_flFireRate;

	//CALL_ATTRIB_HOOK_FLOAT( flRate, mod_fire_rate );

	return flRate;
}
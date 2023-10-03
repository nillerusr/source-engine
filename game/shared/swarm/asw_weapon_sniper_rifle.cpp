#include "cbase.h"
#include "asw_weapon_sniper_rifle.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "dlight.h"
#include "asw_input.h"
#include "iefx.h"
#include "engine/ivdebugoverlay.h"
#include "c_asw_fx.h"
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

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Sniper_Rifle, DT_ASW_Weapon_Sniper_Rifle )

BEGIN_NETWORK_TABLE( CASW_Weapon_Sniper_Rifle, DT_ASW_Weapon_Sniper_Rifle )
#ifdef CLIENT_DLL
	// recvprops
	RecvPropTime( RECVINFO( m_fSlowTime ) ),
	RecvPropBool( RECVINFO( m_bZoomed ) ),
#else
	// sendprops
	SendPropTime( SENDINFO( m_fSlowTime ) ),
	SendPropBool( SENDINFO( m_bZoomed ) ),
#endif
END_NETWORK_TABLE()


#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CASW_Weapon_Sniper_Rifle )
	DEFINE_PRED_FIELD_TOL( m_fSlowTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),	
END_PREDICTION_DATA()

#else

//-----------------------------------------------------------------------------
// Purpose: Save Data
//-----------------------------------------------------------------------------// 
BEGIN_DATADESC( CASW_Weapon_Sniper_Rifle )
	DEFINE_FIELD( m_fSlowTime, FIELD_TIME ),
END_DATADESC()
#endif

LINK_ENTITY_TO_CLASS( asw_weapon_sniper_rifle, CASW_Weapon_Sniper_Rifle );
PRECACHE_WEAPON_REGISTER( asw_weapon_sniper_rifle );

#ifndef CLIENT_DLL
extern ConVar asw_debug_marine_damage;

#endif /* not client */

CASW_Weapon_Sniper_Rifle::CASW_Weapon_Sniper_Rifle()
{
	m_fMinRange1	= 0;
	m_fMaxRange1	= 512;

	m_fMinRange2	= 256;
	m_fMaxRange2	= 1024;

	m_fSlowTime = 0;

#ifdef CLIENT_DLL
	m_pSniperDynamicLight = NULL;
#endif
}


CASW_Weapon_Sniper_Rifle::~CASW_Weapon_Sniper_Rifle()
{

}

#define PELLET_AIR_VELOCITY	3500
#define PELLET_WATER_VELOCITY	1500

void CASW_Weapon_Sniper_Rifle::PrimaryAttack( void )
{
	// If my clip is empty (and I use clips) start reload
	if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	{
		Reload();
		return;
	}

	CASW_Player *pPlayer = GetCommander();
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return;

	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound(SINGLE);
	if (m_iClip1 <= AmmoClickPoint())
		BaseClass::WeaponSound( EMPTY );

	m_bIsFiring = true;

	// tell the marine to tell its weapon to draw the muzzle flash
	pMarine->DoMuzzleFlash();

	// sets the animation on the weapon model iteself
	//SendWeaponAnim( GetPrimaryAttackActivity() );

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

	int iPenetration = 1;
	if ( pMarine->GetDamageBuffEndTime() > gpGlobals->curtime )		// sniper rifle penetrates more targets when marine is in a damage amp
	{
		iPenetration = 3;
	}
	pMarine->FirePenetratingBullets( info, iPenetration, 3.5f, 0, true, NULL, false  );

	// increment shooting stats
#ifndef CLIENT_DLL
	if (pMarine && pMarine->GetMarineResource())
	{
		pMarine->GetMarineResource()->UsedWeapon(this, 1);
		pMarine->OnWeaponFired( this, 1 );
	}
#endif
	
	if (m_iClip1 > 0)		// only force the fire wait time if we have ammo for another shot
		m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	else
		m_flNextPrimaryAttack = gpGlobals->curtime;
	m_fSlowTime = gpGlobals->curtime + 0.03f;
}

void CASW_Weapon_Sniper_Rifle::Precache()
{
	PrecacheModel( "swarm/sprites/whiteglow1.vmt" );
	PrecacheModel( "swarm/sprites/greylaser1.vmt" );
	PrecacheScriptSound("ASW_Pistol.ReloadA");
	PrecacheScriptSound("ASW_Pistol.ReloadB");

	BaseClass::Precache();
}

bool CASW_Weapon_Sniper_Rifle::ShouldMarineMoveSlow()
{
	return (gpGlobals->curtime < m_fSlowTime) || IsZoomed();
}

void CASW_Weapon_Sniper_Rifle::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return;

	// AIs switch out of zoom mode
	if ( !pMarine->IsInhabited() && IsZoomed() )
	{
		m_bZoomed = false;
	}

	bool bAttack1, bAttack2, bReload, bOldReload, bOldAttack1;
	GetButtons( bAttack1, bAttack2, bReload, bOldReload, bOldAttack1 );

	bool bOldAttack2 = false;
	if ( pMarine->IsInhabited() && pMarine->GetCommander() )
	{
		bOldAttack2 = !!(pMarine->m_nOldButtons & IN_ATTACK2);
	}

	if ( bAttack2 && !bOldAttack2 )
	{
		m_bZoomed = !IsZoomed();
	}
}

float CASW_Weapon_Sniper_Rifle::GetWeaponDamage()
{
	float flDamage = GetWeaponInfo()->m_flBaseDamage;

	if ( GetMarine() )
	{
		flDamage += MarineSkills()->GetSkillBasedValueByMarine(GetMarine(), ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_SNIPER_RIFLE_DMG);
	}

	return flDamage;
}

#ifdef CLIENT_DLL

ConVar asw_sniper_dlight_radius("asw_sniper_dlight_radius", "100", FCVAR_CHEAT, "Radius of the light around the cursor.");
ConVar asw_sniper_dlight_r("asw_sniper_dlight_r", "250", FCVAR_CHEAT, "Red component of flashlight colour");
ConVar asw_sniper_dlight_g("asw_sniper_dlight_g", "250", FCVAR_CHEAT, "Green component of flashlight colour");
ConVar asw_sniper_dlight_b("asw_sniper_dlight_b", "250", FCVAR_CHEAT, "Blue component of flashlight colour");
ConVar asw_sniper_dlight_exponent( "asw_sniper_dlight_exponent", "1", FCVAR_CHEAT );

void CASW_Weapon_Sniper_Rifle::UpdateDynamicLight()
{
	// DLIGHT disabled, since it looks bad
	return;

	C_ASW_Marine *pMarine =GetMarine();
	C_ASW_Player *pPlayer = pMarine ? pMarine->GetCommander() : NULL;
	
	if ( !pMarine || pMarine->GetActiveWeapon() != this || !pPlayer || !pMarine->IsInhabited() || !pPlayer->IsLocalPlayer() )
	{
		if (m_pSniperDynamicLight)
		{			
			m_pSniperDynamicLight->die = gpGlobals->curtime + 0.001;
			m_pSniperDynamicLight = NULL;
		}
		return;
	}

	if ( !m_pSniperDynamicLight || (m_pSniperDynamicLight->key != index) )
	{
		m_pSniperDynamicLight = effects->CL_AllocDlight ( index );
	}

	//m_fAmbientLight = asw_flashlight_marine_ambient.GetFloat();
	//m_fLightingScale = asw_flashlight_marine_lightscale.GetFloat();

	Vector vecForward, vecRight, vecUp;

	if (m_pSniperDynamicLight)
	{
		AngleVectors( GetLocalAngles(), &vecForward, &vecRight, &vecUp );
		m_pSniperDynamicLight->origin = pPlayer->GetCrosshairTracePos() + Vector( 0, 0, 10 );	
		Msg( "crosshair trace pos is %f %f %f\n", VectorExpand( pPlayer->GetCrosshairTracePos() ) );
		debugoverlay->AddTextOverlay( m_pSniperDynamicLight->origin, 0.01f, "Light" );
		m_pSniperDynamicLight->color.r = asw_sniper_dlight_r.GetInt();
		m_pSniperDynamicLight->color.g = asw_sniper_dlight_g.GetInt();
		m_pSniperDynamicLight->color.b = asw_sniper_dlight_b.GetInt();
		m_pSniperDynamicLight->radius = asw_sniper_dlight_radius.GetFloat();
		m_pSniperDynamicLight->color.exponent = asw_sniper_dlight_exponent.GetFloat();
		//m_pSniperDynamicLight->decay = 0;
		m_pSniperDynamicLight->die = gpGlobals->curtime + 30.0f;
	}
}

void CASW_Weapon_Sniper_Rifle::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

void CASW_Weapon_Sniper_Rifle::ClientThink()
{
	BaseClass::ClientThink();

	UpdateDynamicLight();

	if ( m_nEjectBrassCount > 0 && gpGlobals->curtime >= m_flEjectBrassTime )
	{
		int iAttachment = LookupAttachment( "eject1" );
		if( iAttachment != -1 )
		{
			for ( int i = 0; i < m_nEjectBrassCount; i++ )
			{
				EjectParticleBrass( "weapon_shell_casing_rifle", iAttachment );
			}
		}
		m_nEjectBrassCount = 0;
	}
}

ConVar asw_sniper_shell_delay( "asw_sniper_shell_delay", "0.7", FCVAR_NONE, "Delay on shell casing after firing rifle" );

void CASW_Weapon_Sniper_Rifle::OnMuzzleFlashed()
{
	BaseClass::OnMuzzleFlashed();

	Vector attachOrigin;
	QAngle attachAngles;

	m_nEjectBrassCount++;
	m_flEjectBrassTime = gpGlobals->curtime + asw_sniper_shell_delay.GetFloat();
	if( GetAttachment( LookupAttachment("muzzle"), attachOrigin, attachAngles ) )
	{
		FX_ASW_ShotgunSmoke(attachOrigin, attachAngles);
	}
}
#endif

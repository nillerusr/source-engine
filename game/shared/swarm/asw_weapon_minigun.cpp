#include "cbase.h"
#include "asw_weapon_minigun.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "c_asw_marine_resource.h"
#include "c_te_legacytempents.h"
#include "c_asw_gun_smoke_emitter.h"
#include "soundenvelope.h"
#define CASW_Marine_Resource C_ASW_Marine_Resource
#define CASW_Marine C_ASW_Marine
#else
#include "asw_lag_compensation.h"
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "asw_marine_resource.h"
#endif
#include "asw_marine_skills.h"
#include "asw_weapon_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar asw_minigun_spin_rate_threshold( "asw_minigun_spin_rate_threshold", "0.75f", FCVAR_CHEAT | FCVAR_REPLICATED, "Minimum barrel spin rate before minigun will fire" );
ConVar asw_minigun_spin_up_rate( "asw_minigun_spin_up_rate", "1.0", FCVAR_CHEAT | FCVAR_REPLICATED, "Spin up speed of minigun" );
ConVar asw_minigun_spin_down_rate( "asw_minigun_spin_down_rate", "0.4", FCVAR_CHEAT | FCVAR_REPLICATED, "Spin down speed of minigun" );
extern ConVar asw_weapon_max_shooting_distance;
extern ConVar asw_weapon_force_scale;
extern ConVar asw_DebugAutoAim;


IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Minigun, DT_ASW_Weapon_Minigun )

BEGIN_NETWORK_TABLE( CASW_Weapon_Minigun, DT_ASW_Weapon_Minigun )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_flSpinRate ) ),
	RecvPropFloat( RECVINFO( m_flPartialBullets ) ),
#else
	SendPropFloat( SENDINFO( m_flSpinRate ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flPartialBullets ), 0, SPROP_NOSCALE, 0.0f, 1.0f ),
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),	
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),	
	SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
	SendPropExclude( "DT_BaseAnimating", "m_nNewSequenceParity" ),
	SendPropExclude( "DT_BaseAnimating", "m_nResetEventsParity" ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CASW_Weapon_Minigun )
	DEFINE_PRED_FIELD_TOL( m_flSpinRate, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD( m_flPartialBullets, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flCycle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_nSequence, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),	
	DEFINE_PRED_FIELD( m_nNewSequenceParity, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_nResetEventsParity, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( asw_weapon_minigun, CASW_Weapon_Minigun );
PRECACHE_WEAPON_REGISTER( asw_weapon_minigun );


#ifndef CLIENT_DLL
extern ConVar asw_debug_marine_damage;
//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Minigun )
	
END_DATADESC()

void CASW_Weapon_Minigun::Spawn()
{
	BaseClass::Spawn();

	UseClientSideAnimation();
}

void CASW_Weapon_Minigun::SecondaryAttack( void )
{
	CASW_Player *pPlayer = GetCommander();
	if (!pPlayer)
		return;

	CASW_Marine *pMarine = GetMarine();
	if (!pMarine)
		return;

	// dry fire
	//SendWeaponAnim( ACT_VM_DRYFIRE );
	//BaseClass::WeaponSound( EMPTY );
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
}

#endif /* not client */

CASW_Weapon_Minigun::CASW_Weapon_Minigun()
{
#ifdef CLIENT_DLL
	m_flLastMuzzleFlashTime = 0;
#endif
}

bool CASW_Weapon_Minigun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	// stop the barrel spinning
	m_flSpinRate = 0.0f;

	return BaseClass::Holster( pSwitchingTo );
}


CASW_Weapon_Minigun::~CASW_Weapon_Minigun()
{

}

void CASW_Weapon_Minigun::Precache()
{
	PrecacheScriptSound("ASW_Autogun.ReloadA");
	PrecacheScriptSound("ASW_Autogun.ReloadB");
	PrecacheScriptSound("ASW_Autogun.ReloadC");
	PrecacheScriptSound( "ASW_Minigun.Spin" );

	BaseClass::Precache();
}

void CASW_Weapon_Minigun::PrimaryAttack()
{
	// can't attack until the minigun barrel has spun up
	if ( GetSpinRate() < asw_minigun_spin_rate_threshold.GetFloat() )
		return;
	
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

	m_bIsFiring = true;
	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound(SINGLE);

	if (m_iClip1 <= AmmoClickPoint())
	{
		LowAmmoSound();
	}

	// tell the marine to tell its weapon to draw the muzzle flash
	pMarine->DoMuzzleFlash();

	// sets the animation on the weapon model itself
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
	info.m_vecSrc	 = pMarine->Weapon_ShootPosition( );
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
	float fireRate = GetFireRate() * ( 1.0f / MAX( GetSpinRate(), asw_minigun_spin_rate_threshold.GetFloat() ) );
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
		m_flPartialBullets += static_cast<float>( info.m_iShots ) * 0.5f;

		if ( m_flPartialBullets >= 1.0f )
		{
			// Subtract ammo if we've counted up a whole bullet
			int nBullets = m_flPartialBullets;
			m_iClip1 -= nBullets;
			m_flPartialBullets -= nBullets;
		}

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
	info.m_flDamageForceScale = asw_weapon_force_scale.GetFloat();

	info.m_vecSpread = pMarine->GetActiveWeapon()->GetBulletSpread();
	info.m_flDamage = GetWeaponDamage();
#ifndef CLIENT_DLL
	if (asw_debug_marine_damage.GetBool())
		Msg("Weapon dmg = %f\n", info.m_flDamage);
	info.m_flDamage *= pMarine->GetMarineResource()->OnFired_GetDamageScale();
	if (asw_DebugAutoAim.GetBool())
	{
		NDebugOverlay::Line(info.m_vecSrc, info.m_vecSrc + info.m_vecDirShooting * info.m_flDistance, 64, 0, 64, true, 1.0);
	}
#endif

	// fire extra shots per ammo from the minigun, so we get a nice solid spray of bullets
	//info.m_iShots = 2;
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

void CASW_Weapon_Minigun::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	UpdateSpinRate();
}

void CASW_Weapon_Minigun::ItemBusyFrame( void )
{
	BaseClass::ItemBusyFrame();

	UpdateSpinRate();
}

void CASW_Weapon_Minigun::UpdateSpinRate()
{
	bool bAttack1, bAttack2, bReload, bOldReload, bOldAttack1;
	GetButtons(bAttack1, bAttack2, bReload, bOldReload, bOldAttack1 );

	CASW_Marine *pMarine = GetMarine();
	bool bMeleeing = pMarine && ( pMarine->GetCurrentMeleeAttack() != NULL );
	bool bWalking = pMarine && pMarine->m_bWalking.Get();

	bool bSpinUp = !m_bInReload && !bMeleeing && ( bAttack1 || bAttack2 || bWalking );
	if ( bSpinUp )
	{
		m_flSpinRate = MIN( 1.0f, GetSpinRate() + gpGlobals->frametime * asw_minigun_spin_up_rate.GetFloat() );
	}
	else
	{
		m_flSpinRate = MAX( 0.0f, GetSpinRate() - gpGlobals->frametime * asw_minigun_spin_down_rate.GetFloat() * ( ( m_bInReload || bMeleeing ) ? 3.0f : 1.0f ) );
	}
}

#ifdef CLIENT_DLL

void CASW_Weapon_Minigun::OnMuzzleFlashed()
{
	BaseClass::OnMuzzleFlashed();
	
	//m_flPlaybackRate = 1.0f;
	m_flLastMuzzleFlashTime = gpGlobals->curtime;

	//Vector attachOrigin;
	//QAngle attachAngles;
	
	int iAttachment = LookupAttachment( "eject1" );
	if( iAttachment != -1 )
	{				
		//		tempents->EjectBrass( attachOrigin, attachAngles, GetAbsAngles(), 1 );		// 0 = brass shells, 2 = grey shells, 3 = plastic shotgun shell casing
		EjectParticleBrass( "weapon_shell_casing_rifle", iAttachment );
	}
	if ( m_hGunSmoke.IsValid() )
	{
		m_hGunSmoke->OnFired();
	}
}


void CASW_Weapon_Minigun::ReachedEndOfSequence()
{
	BaseClass::ReachedEndOfSequence();
	//if (gpGlobals->curtime - m_flLastMuzzleFlashTime > 1.0)	// 0.3 is the real firing time, but spin for a bit longer
	//{
		//if (GetSequenceActivity(GetSequence()) != ACT_VM_IDLE)
			//SetActivity(ACT_VM_IDLE, 0);
	//}
}

float CASW_Weapon_Minigun::GetMuzzleFlashScale( void )
{
	// if we haven't calculated the muzzle scale based on the carrying marine's skill yet, then do so
	if (m_fMuzzleFlashScale == -1)
	{
		C_ASW_Marine *pMarine = GetMarine();
		if (pMarine)
			m_fMuzzleFlashScale = 2.0f * MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_AUTOGUN, ASW_MARINE_SUBSKILL_AUTOGUN_MUZZLE);
		else
			return 2.0f;
	}
	return m_fMuzzleFlashScale;
}

bool CASW_Weapon_Minigun::GetMuzzleFlashRed()
{
	return ((GetMuzzleFlashScale() / 2.0f) >= 1.15f);	// red if our muzzle flash is the biggest size based on our skill
}

#endif

float CASW_Weapon_Minigun::GetMovementScale()
{
	return ShouldMarineMoveSlow() ? 0.4f : 0.95f;
}

bool CASW_Weapon_Minigun::ShouldMarineMoveSlow()
{
	bool bAttack1, bAttack2, bReload, bOldReload, bOldAttack1;
	GetButtons(bAttack1, bAttack2, bReload, bOldReload, bOldAttack1 );

	return ( BaseClass::ShouldMarineMoveSlow() || bAttack2 || bAttack1 || GetSpinRate() >= 0.99f );
}

float CASW_Weapon_Minigun::GetWeaponDamage()
{
	//float flDamage = 7.0f;
	float flDamage = GetWeaponInfo()->m_flBaseDamage;

	if ( GetMarine() )
	{
		flDamage += MarineSkills()->GetSkillBasedValueByMarine(GetMarine(), ASW_MARINE_SKILL_AUTOGUN, ASW_MARINE_SUBSKILL_AUTOGUN_DMG);
	}

	//CALL_ATTRIB_HOOK_FLOAT( flDamage, mod_damage_done );

	return flDamage;
}

const Vector& CASW_Weapon_Minigun::GetBulletSpread( void )
{
	static Vector cone = Vector( 0.13053, 0.13053, 0.02 );	// VECTOR_CONE_15DEGREES with flattened Z
	return cone;
}

#ifdef CLIENT_DLL
const char* CASW_Weapon_Minigun::GetPartialReloadSound(int iPart)
{
	switch (iPart)
	{
		case 1: return "ASW_Autogun.ReloadB"; break;
		case 2: return "ASW_Autogun.ReloadC"; break;
		default: break;
	};
	return "ASW_Autogun.ReloadA";
}

void CASW_Weapon_Minigun::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		CreateGunSmoke();

		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

void CASW_Weapon_Minigun::ClientThink()
{
	BaseClass::ClientThink();

	UpdateSpinningBarrel();
}

ConVar asw_minigun_pitch_min( "asw_minigun_pitch_min", "50", FCVAR_CHEAT | FCVAR_REPLICATED, "Pitch of barrel spin sound" );
ConVar asw_minigun_pitch_max( "asw_minigun_pitch_max", "150", FCVAR_CHEAT | FCVAR_REPLICATED, "Pitch of barrel spin sound" );

void CASW_Weapon_Minigun::UpdateSpinningBarrel()
{
	if (GetSequenceActivity(GetSequence()) != ACT_VM_PRIMARYATTACK)
	{
		SetActivity(ACT_VM_PRIMARYATTACK, 0);
	}
	m_flPlaybackRate = GetSpinRate();

	if ( GetSpinRate() > 0.0f )
	{
		if( !m_pBarrelSpinSound )
		{
			CPASAttenuationFilter filter( this );
			m_pBarrelSpinSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "ASW_Minigun.Spin" );
			CSoundEnvelopeController::GetController().Play( m_pBarrelSpinSound, 0.0, 100 );
		}
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pBarrelSpinSound, MIN( 1.0f, m_flPlaybackRate * 3.0f ), 0.0f );
		CSoundEnvelopeController::GetController().SoundChangePitch( m_pBarrelSpinSound, asw_minigun_pitch_min.GetFloat() + ( GetSpinRate() * ( asw_minigun_pitch_max.GetFloat() - asw_minigun_pitch_min.GetFloat() ) ), 0.0f );
	}
	else
	{
		if ( m_pBarrelSpinSound )
		{
			CSoundEnvelopeController::GetController().SoundDestroy( m_pBarrelSpinSound );
			m_pBarrelSpinSound = NULL;
		}
	}
}

void CASW_Weapon_Minigun::CreateGunSmoke()
{	
	int iAttachment = LookupAttachment( "muzzle" );
	if ( iAttachment <= 0 )
	{
		Msg("error, couldn't find muzzle attachment for flamer tip\n");
		return;
	}

	C_ASW_Gun_Smoke_Emitter *pEnt = new C_ASW_Gun_Smoke_Emitter;
	if (!pEnt)
	{
		Msg("Error, couldn't create new C_ASW_Gun_Smoke_Emitter for autogun smoke\n");
		return;
	}
	if ( !pEnt->InitializeAsClientEntity( NULL, false ) )
	{
		Msg("Error, couldn't InitializeAsClientEntity for autogun smoke\n");
		UTIL_Remove( pEnt );
		return;
	}
	
	Vector vecMuzzle;
	QAngle angMuzzle;
	GetAttachment( iAttachment, vecMuzzle, angMuzzle );

	Q_snprintf(pEnt->m_szTemplateName, sizeof(pEnt->m_szTemplateName), "autogunsmoke");
	pEnt->m_fScale = 1.0f;
	pEnt->m_bEmit = false;
	pEnt->SetAbsOrigin(vecMuzzle);
	pEnt->CreateEmitter();
	pEnt->SetAbsOrigin(vecMuzzle);
	pEnt->SetAbsAngles(angMuzzle);

	pEnt->ClientAttach(this, "muzzle");

	m_hGunSmoke = pEnt;
}

void CASW_Weapon_Minigun::SetDormant( bool bDormant )
{
	if ( bDormant )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pBarrelSpinSound );
		m_pBarrelSpinSound = NULL;
	}
	BaseClass::SetDormant( bDormant );
}

void CASW_Weapon_Minigun::UpdateOnRemove()
{
	if ( m_hGunSmoke.IsValid() )
	{
		UTIL_Remove( m_hGunSmoke.Get() );
	}

	if ( m_pBarrelSpinSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pBarrelSpinSound );
		m_pBarrelSpinSound = NULL;
	}

	BaseClass::UpdateOnRemove();
}
#else

void CASW_Weapon_Minigun::Drop( const Vector &vecVelocity )
{
	// stop the barrel spinning
	m_flSpinRate = 0.0f;

	BaseClass::Drop( vecVelocity );
}
#endif
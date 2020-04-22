//========= Copyright Valve Corporation, All rights reserved. ============//
//
// TF Flame Thrower
//
//=============================================================================
#include "cbase.h"
#include "tf_weapon_flamethrower.h"
#include "tf_fx_shared.h"
#include "in_buttons.h"
#include "ammodef.h"
#include "tf_gamerules.h"

#if defined( CLIENT_DLL )

	#include "c_tf_player.h"
	#include "vstdlib/random.h"
	#include "engine/IEngineSound.h"
	#include "soundenvelope.h"
	#include "prediction.h"
	#include "haptics/ihaptics.h"
	#include "c_tf_gamestats.h"
#else

	#include "explode.h"
	#include "tf_player.h"
	#include "tf_gamestats.h"
	#include "ilagcompensationmanager.h"
	#include "collisionutils.h"
	#include "tf_team.h"
	#include "tf_obj.h"
	#include "tf_weapon_grenade_pipebomb.h"
	#include "particle_parse.h"
	#include "tf_weaponbase_grenadeproj.h"
	#include "tf_weapon_compound_bow.h"
	#include "tf_projectile_arrow.h"
	#include "tf_gamestats.h"
	#include "NextBot/NextBotManager.h"
	#include "halloween/merasmus/merasmus_trick_or_treat_prop.h"
	#include "tf_logic_robot_destruction.h"
#ifdef STAGING_ONLY
	#include "tf_fx.h"
#endif // STAGING_ONLY
	#include "tf_passtime_logic.h"

	ConVar	tf_debug_flamethrower("tf_debug_flamethrower", "0", FCVAR_CHEAT , "Visualize the flamethrower damage." );
	ConVar  tf_flamethrower_velocity( "tf_flamethrower_velocity", "2300.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Initial velocity of flame damage entities." );
	ConVar	tf_flamethrower_drag("tf_flamethrower_drag", "0.87", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Air drag of flame damage entities." );
	ConVar	tf_flamethrower_float("tf_flamethrower_float", "50.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Upward float velocity of flame damage entities." );
	ConVar  tf_flamethrower_vecrand("tf_flamethrower_vecrand", "0.05", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Random vector added to initial velocity of flame damage entities." );
	ConVar  tf_flamethrower_boxsize("tf_flamethrower_boxsize", "12.0", FCVAR_CHEAT , "Size of flame damage entities." );
	ConVar  tf_flamethrower_maxdamagedist("tf_flamethrower_maxdamagedist", "350.0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Maximum damage distance for flamethrower." );
	ConVar  tf_flamethrower_shortrangedamagemultiplier("tf_flamethrower_shortrangedamagemultiplier", "1.2", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Damage multiplier for close-in flamethrower damage." );
	ConVar  tf_flamethrower_velocityfadestart("tf_flamethrower_velocityfadestart", ".3", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Time at which attacker's velocity contribution starts to fade." );
	ConVar  tf_flamethrower_velocityfadeend("tf_flamethrower_velocityfadeend", ".5", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Time at which attacker's velocity contribution finishes fading." );
	ConVar	tf_flamethrower_burst_zvelocity( "tf_flamethrower_burst_zvelocity", "350", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

	static const char *s_pszFlameThrowerHitTargetThink = "FlameThrowerHitTargetThink";

	extern ConVar tf_player_movement_stun_time;

#endif

#include "tf_pumpkin_bomb.h"

ConVar  tf_flamethrower_burstammo("tf_flamethrower_burstammo", "20", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "How much ammo does the air burst uses per shot." );
ConVar  tf_flamethrower_flametime("tf_flamethrower_flametime", "0.5", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Time to live of flame damage entities." );

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// position of end of muzzle relative to shoot position
#define TF_FLAMETHROWER_MUZZLEPOS_FORWARD		70.0f
#define TF_FLAMETHROWER_MUZZLEPOS_RIGHT			12.0f
#define TF_FLAMETHROWER_MUZZLEPOS_UP			-12.0f

#define TF_FLAMETHROWER_AMMO_PER_SECOND_PRIMARY_ATTACK		14.0f

#define TF_FLAMETHROWER_HITACCURACY_MED			40.0f
#define TF_FLAMETHROWER_HITACCURACY_HIGH		60.0f

//-----------------------------------------------------------------------------

#define TF_WEAPON_BUBBLE_WAND_MODEL		"models/player/items/pyro/mtp_bubble_wand.mdl"

//-----------------------------------------------------------------------------

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Only send to local player
//-----------------------------------------------------------------------------
void* SendProxy_SendLocalFlameThrowerDataTable( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
{
	// Get the weapon entity
	CBaseCombatWeapon *pWeapon = (CBaseCombatWeapon*)pVarData;
	if ( pWeapon )
	{
		// Only send this chunk of data to the player carrying this weapon
		CBasePlayer *pPlayer = ToBasePlayer( pWeapon->GetOwner() );
		if ( pPlayer )
		{
			pRecipients->SetOnly( pPlayer->GetClientIndex() );
			return (void*)pVarData;
		}
	}

	return NULL;
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_SendLocalFlameThrowerDataTable );
#endif	// CLIENT_DLL

IMPLEMENT_NETWORKCLASS_ALIASED( TFFlameThrower, DT_WeaponFlameThrower )

//-----------------------------------------------------------------------------
// Purpose: Only sent to the local player
//-----------------------------------------------------------------------------
BEGIN_NETWORK_TABLE_NOBASE( CTFFlameThrower, DT_LocalFlameThrower )
	#if defined( CLIENT_DLL )
		RecvPropInt( RECVINFO( m_iActiveFlames ) ),
		RecvPropInt( RECVINFO( m_iDamagingFlames ) ),
		RecvPropBool( RECVINFO( m_bHasHalloweenSpell ) ),
	#else
		SendPropInt( SENDINFO( m_iActiveFlames ), 5, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
		SendPropInt( SENDINFO( m_iDamagingFlames ), 10, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
		SendPropBool( SENDINFO( m_bHasHalloweenSpell ) ),
	#endif
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
BEGIN_NETWORK_TABLE( CTFFlameThrower, DT_WeaponFlameThrower )
	#if defined( CLIENT_DLL )
		RecvPropInt( RECVINFO( m_iWeaponState ) ),
		RecvPropBool( RECVINFO( m_bCritFire ) ),
		RecvPropBool( RECVINFO( m_bHitTarget ) ),
		RecvPropFloat( RECVINFO( m_flChargeBeginTime ) ),
		RecvPropDataTable("LocalFlameThrowerData", 0, 0, &REFERENCE_RECV_TABLE( DT_LocalFlameThrower ) ),
	#else
		SendPropInt( SENDINFO( m_iWeaponState ), 4, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
		SendPropBool( SENDINFO( m_bCritFire ) ),
		SendPropBool( SENDINFO( m_bHitTarget ) ),
		SendPropFloat( SENDINFO( m_flChargeBeginTime ) ),
		SendPropDataTable("LocalFlameThrowerData", 0, &REFERENCE_SEND_TABLE( DT_LocalFlameThrower ), SendProxy_SendLocalFlameThrowerDataTable ),
	#endif
END_NETWORK_TABLE()

#if defined( CLIENT_DLL )
BEGIN_PREDICTION_DATA( CTFFlameThrower )
	DEFINE_PRED_FIELD( m_iWeaponState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bCritFire, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_FIELD(  m_flChargeBeginTime, FIELD_FLOAT ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( tf_weapon_flamethrower, CTFFlameThrower );
PRECACHE_WEAPON_REGISTER( tf_weapon_flamethrower );

BEGIN_DATADESC( CTFFlameThrower )
END_DATADESC()

// ------------------------------------------------------------------------------------------------ //
// CTFFlameThrower implementation.
// ------------------------------------------------------------------------------------------------ //
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFFlameThrower::CTFFlameThrower()
#if defined( CLIENT_DLL )
: 	m_FlameEffects( this )
,	m_MmmmphEffect( this )
#endif
{
	WeaponReset();

#if defined( CLIENT_DLL )
	m_pFiringStartSound = NULL;
	m_pFiringLoop = NULL;
	m_pFiringAccuracyLoop = NULL;
	m_pFiringHitLoop = NULL;
	m_bFiringLoopCritical = false;
	m_pPilotLightSound = NULL;
	m_pSpinUpSound = NULL;
	m_szAccuracySound = NULL;
	m_bEffectsThinking = false;
	m_bFullRageEffect = false;
#else
	m_flTimeToStopHitSound = 0;
#endif
	m_bHasHalloweenSpell.Set( false );

	ListenForGameEvent( "recalculate_holidays" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFFlameThrower::~CTFFlameThrower()
{
	DestroySounds();
#if defined( CLIENT_DLL )
	StopFullCritEffect();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::Precache( void )
{
	BaseClass::Precache();

	int iModelIndex = PrecacheModel( TF_WEAPON_BUBBLE_WAND_MODEL );
	PrecacheGibsForModel( iModelIndex );

	PrecacheParticleSystem( "pyro_blast" );
	PrecacheScriptSound( "Weapon_FlameThrower.AirBurstAttack" );
	PrecacheScriptSound( "TFPlayer.AirBlastImpact" );
	PrecacheScriptSound( "Weapon_FlameThrower.AirBurstAttackDeflect" );
	PrecacheParticleSystem( "deflect_fx" );
	PrecacheParticleSystem( "drg_bison_idle" );
	PrecacheParticleSystem( "medicgun_invulnstatus_fullcharge_blue" );
	PrecacheParticleSystem( "medicgun_invulnstatus_fullcharge_red" );
	PrecacheParticleSystem( "halloween_burningplayer_flyingbits");

#ifdef STAGING_ONLY
	PrecacheScriptSound( "Equipment.RocketPack_Activate" );
	PrecacheParticleSystem( "muzzle_bignasty" );
#endif // STAGING_ONLY
}

bool CTFFlameThrower::CanAirBlast() const
{
	int iAirblastDisabled = 0;
	CALL_ATTRIB_HOOK_INT( iAirblastDisabled, airblast_disabled );

	bool bAllowed = ( iAirblastDisabled == 0 );

#ifdef STAGING_ONLY
	int nRocketPack = 0;
	CALL_ATTRIB_HOOK_INT_ON_OTHER( GetOwner(), nRocketPack, rocket_pack );
	if ( nRocketPack )
	{
		bAllowed = false;
	}
#endif // STAGING_ONLY

	return bAllowed;
}

void CTFFlameThrower::DestroySounds( void )
{
#if defined( CLIENT_DLL )
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	if ( m_pFiringStartSound )
	{
		controller.SoundDestroy( m_pFiringStartSound );
		m_pFiringStartSound = NULL;
	}
	if ( m_pFiringLoop )
	{
		controller.SoundDestroy( m_pFiringLoop );
		m_pFiringLoop = NULL;
	}
	if ( m_pPilotLightSound )
	{
		controller.SoundDestroy( m_pPilotLightSound );
		m_pPilotLightSound = NULL;
	}
	if ( m_pSpinUpSound )
	{
		controller.SoundDestroy( m_pSpinUpSound );
		m_pSpinUpSound = NULL;
	}
	if ( m_pFiringAccuracyLoop )
	{
		controller.SoundDestroy( m_pFiringAccuracyLoop );
		m_pFiringAccuracyLoop = NULL;
	}

	StopHitSound();
#endif

}
void CTFFlameThrower::WeaponReset( void )
{
	BaseClass::WeaponReset();

	SetWeaponState( FT_STATE_IDLE );
	m_bCritFire = false;
	m_bHitTarget = false;
	m_flStartFiringTime = 0;
	m_flAmmoUseRemainder = 0;
	m_flChargeBeginTime = 0;
	m_flSpinupBeginTime = 0;
	ResetFlameHitCount();

	DestroySounds();

#if defined( CLIENT_DLL )
	StopFullCritEffect();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::Spawn( void )
{
	m_iAltFireHint = HINT_ALTFIRE_FLAMETHROWER;
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFFlameThrower::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	SetWeaponState( FT_STATE_IDLE );
	m_bCritFire = false;
	m_bHitTarget = false;
	m_flChargeBeginTime = 0;

#if defined ( CLIENT_DLL )
	StopFlame();
	StopPilotLight();
	StopFullCritEffect();

	m_bEffectsThinking = false;
#endif

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::ItemPostFrame()
{
	if ( m_bLowered )
		return;

	// Get the player owning the weapon.
	CTFPlayer *pOwner = GetTFPlayerOwner();
	if ( !pOwner )
		return;

#ifdef CLIENT_DLL
	if ( !m_bEffectsThinking )
	{
		m_bEffectsThinking = true;
		SetContextThink( &CTFFlameThrower::ClientEffectsThink, gpGlobals->curtime, "EFFECTS_THINK" );
	}
#endif

	int iAmmo = pOwner->GetAmmoCount( m_iPrimaryAmmoType );

	m_bFiredSecondary = false;
	if ( pOwner->IsAlive() && ( pOwner->m_nButtons & IN_ATTACK2 ) )
	{
		SecondaryAttack();
	}

	// Fixes an exploit where the airblast effect repeats while +attack is active
	if ( m_bFiredBothAttacks )
	{
		if ( pOwner->m_nButtons & IN_ATTACK && !( pOwner->m_nButtons & IN_ATTACK2 ) )
		{
			pOwner->m_nButtons &= ~IN_ATTACK;
		}
		m_bFiredBothAttacks = false;
	}

	if ( pOwner->m_nButtons & IN_ATTACK && pOwner->m_nButtons & IN_ATTACK2 )
	{
		m_bFiredBothAttacks = true;
	}

	if ( !m_bFiredSecondary )
	{
		bool bSpinDown = m_flSpinupBeginTime > 0.0f;

		if ( pOwner->IsAlive() && ( pOwner->m_nButtons & IN_ATTACK ) && iAmmo > 0 )
		{
			PrimaryAttack();
			bSpinDown = false;
		}
		else if ( m_iWeaponState > FT_STATE_IDLE )
		{
			SendWeaponAnim( ACT_MP_ATTACK_STAND_POSTFIRE );
			pOwner->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_POST );
			SetWeaponState( FT_STATE_IDLE );
			m_bCritFire = false;
			m_bHitTarget = false;
		}

		if ( bSpinDown )
		{
			m_flSpinupBeginTime = 0.0f;

#if defined( CLIENT_DLL )
			if ( m_pSpinUpSound )
			{
				float flSpinUpTime = GetSpinUpTime();

				CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
				controller.SoundChangePitch( m_pSpinUpSound, 40, flSpinUpTime * 0.5f );
				controller.SoundChangeVolume( m_pSpinUpSound, 0.0f, flSpinUpTime * 2.0f );
			}
#endif
		}
	}

	if (!((pOwner->m_nButtons & IN_ATTACK) || (pOwner->m_nButtons & IN_RELOAD)) || (!(pOwner->m_nButtons & IN_ATTACK2) || !m_bFiredSecondary))
	{
		// no fire buttons down or reloading
		if ( !ReloadOrSwitchWeapons() && ( m_bInReload == false ) )
		{
			WeaponIdle();
		}
	}

	// charged airblast
	int iChargedAirblast = 0;
	CALL_ATTRIB_HOOK_INT( iChargedAirblast, set_charged_airblast );
	if ( iChargedAirblast != 0 )
	{
		if ( m_flChargeBeginTime > 0 )
		{
			CTFPlayer *pPlayer = GetTFPlayerOwner();
			if ( !pPlayer )
				return;

			// If we're not holding down the attack button, launch the flame rocket
			if ( !(pPlayer->m_nButtons & IN_ATTACK2) )
			{
				//FireProjectile( pOwner );
				float flMultAmmoPerShot = 1.0f;
				CALL_ATTRIB_HOOK_FLOAT( flMultAmmoPerShot, mult_airblast_cost );
				int iAmmoPerShot = tf_flamethrower_burstammo.GetInt() * flMultAmmoPerShot;
				FireAirBlast( iAmmoPerShot );
			}
		}
	}
}

class CTraceFilterIgnoreObjects : public CTraceFilterSimple
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS( CTraceFilterIgnoreObjects, CTraceFilterSimple );

	CTraceFilterIgnoreObjects( const IHandleEntity *passentity, int collisionGroup )
		: CTraceFilterSimple( passentity, collisionGroup )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pServerEntity );

		if ( pEntity && pEntity->IsBaseObject() )
			return false;

		return BaseClass::ShouldHitEntity( pServerEntity, contentsMask );
	}
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::PrimaryAttack()
{
	float flSpinUpTime = GetSpinUpTime();

	if ( flSpinUpTime > 0.0f )
	{
		if ( m_flSpinupBeginTime > 0.0f )
		{
			if ( gpGlobals->curtime - m_flSpinupBeginTime < flSpinUpTime )
			{
				return;
			}
		}
		else
		{
			m_flSpinupBeginTime = gpGlobals->curtime;

#if defined( CLIENT_DLL )
			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
			if ( !m_pSpinUpSound )
			{
				// Create the looping pilot light sound
				const char *pchSpinUpSound = GetShootSound( RELOAD );
				CLocalPlayerFilter filter;
				m_pSpinUpSound = controller.SoundCreate( filter, entindex(), pchSpinUpSound );

				controller.Play( m_pSpinUpSound, 0.0f, 40 );
			}

			if ( m_pSpinUpSound )
			{
				controller.SoundChangePitch( m_pSpinUpSound, 100, flSpinUpTime );
				controller.SoundChangeVolume( m_pSpinUpSound, 1.0f, flSpinUpTime * 0.1f );
			}
#endif
			return;
		}
	}

	// Are we capable of firing again?
	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return;

	// Get the player owning the weapon.
	CTFPlayer *pOwner = GetTFPlayerOwner();
	if ( !pOwner )
		return;

	if ( !CanAttack() )
	{
#if defined ( CLIENT_DLL )
		StopFlame();
#endif
		SetWeaponState( FT_STATE_IDLE );
		return;
	}

	m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;

	CalcIsAttackCritical();

	// Because the muzzle is so long, it can stick through a wall if the player is right up against it.
	// Make sure the weapon can't fire in this condition by tracing a line between the eye point and the end of the muzzle.
	trace_t trace;	
	Vector vecEye = pOwner->EyePosition();
	Vector vecMuzzlePos = GetVisualMuzzlePos();
	CTraceFilterIgnoreObjects traceFilter( this, COLLISION_GROUP_NONE );
	UTIL_TraceLine( vecEye, vecMuzzlePos, MASK_SOLID, &traceFilter, &trace );
	if ( trace.fraction < 1.0 && ( !trace.m_pEnt || trace.m_pEnt->m_takedamage == DAMAGE_NO ) )
	{
		// there is something between the eye and the end of the muzzle, most likely a wall, don't fire, and stop firing if we already are
		if ( m_iWeaponState > FT_STATE_IDLE )
		{
#if defined ( CLIENT_DLL )
			StopFlame();
#endif
			SetWeaponState( FT_STATE_IDLE );
		}
		return;
	}

	switch ( m_iWeaponState )
	{
	case FT_STATE_IDLE:
		{
			// Just started, play PRE and start looping view model anim

			pOwner->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRE );

			SendWeaponAnim( ACT_VM_PRIMARYATTACK );

			m_flStartFiringTime = gpGlobals->curtime + 0.16;	// 5 frames at 30 fps

			SetWeaponState( FT_STATE_STARTFIRING );
		}
		break;
	case FT_STATE_STARTFIRING:
		{
			// if some time has elapsed, start playing the looping third person anim
			if ( gpGlobals->curtime > m_flStartFiringTime )
			{
				SetWeaponState( FT_STATE_FIRING );
				m_flNextPrimaryAttackAnim = gpGlobals->curtime;
			}
		}
		break;
	case FT_STATE_FIRING:
		{
			if ( gpGlobals->curtime >= m_flNextPrimaryAttackAnim )
			{
				pOwner->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY );
				m_flNextPrimaryAttackAnim = gpGlobals->curtime + 1.4;		// fewer than 45 frames!
			}
		}
		break;

	default:
		break;
	}

#ifdef CLIENT_DLL
	// Restart our particle effect if we've transitioned across water boundaries
	if ( m_iParticleWaterLevel != -1 && pOwner->GetWaterLevel() != m_iParticleWaterLevel )
	{
		if ( m_iParticleWaterLevel == WL_Eyes || pOwner->GetWaterLevel() == WL_Eyes )
		{
			RestartParticleEffect();
		}
	}
#endif

#if !defined (CLIENT_DLL)
	// Let the player remember the usercmd he fired a weapon on. Assists in making decisions about lag compensation.
	pOwner->NoteWeaponFired();

	pOwner->SpeakWeaponFire();
	CTF_GameStats.Event_PlayerFiredWeapon( pOwner, m_bCritFire );

	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation( pOwner, pOwner->GetCurrentCommand() );

	// PASSTIME custom lag compensation for the ball; see also tf_fx_shared.cpp
	// it would be better if all entities could opt-in to this, or a way for lagcompensation to handle non-players automatically
	if ( g_pPasstimeLogic && g_pPasstimeLogic->GetBall() )
	{
		g_pPasstimeLogic->GetBall()->StartLagCompensation( pOwner, pOwner->GetCurrentCommand() );
	}

#endif
#ifdef CLIENT_DLL
	C_CTF_GameStats.Event_PlayerFiredWeapon( pOwner, IsCurrentAttackACrit() );
#endif

	float flFiringInterval = m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flTimeFireDelay;

#ifdef STAGING_ONLY
	if ( ShootsNapalm() )
	{
		flFiringInterval *= 4.f;
	}
#endif // STAGING_ONLY

	// Don't attack if we're underwater
	if ( pOwner->GetWaterLevel() != WL_Eyes )
	{
		// Find eligible entities in a cone in front of us.
		// Vector vOrigin = pOwner->Weapon_ShootPosition();
		Vector vForward, vRight, vUp;
		QAngle vAngles = pOwner->EyeAngles() + pOwner->GetPunchAngle();
		AngleVectors( vAngles, &vForward, &vRight, &vUp );

		#define NUM_TEST_VECTORS	30

#ifdef CLIENT_DLL
		bool bWasCritical = m_bCritFire;
#endif

		// Burn & Ignite 'em
		int iDmgType = g_aWeaponDamageTypes[ GetWeaponID() ];
		m_bCritFire = IsCurrentAttackACrit();
		if ( m_bCritFire )
		{
			iDmgType |= DMG_CRITICAL;
		}

#ifdef CLIENT_DLL
		if ( bWasCritical != m_bCritFire )
		{
			RestartParticleEffect();
		}
#endif


#ifdef GAME_DLL
		// create the flame entity
		int iDamagePerSec = m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_nDamage;
		float flDamage = (float)iDamagePerSec * flFiringInterval;
		CALL_ATTRIB_HOOK_FLOAT( flDamage, mult_dmg );

		int iCritFromBehind = 0;
		CALL_ATTRIB_HOOK_INT( iCritFromBehind, set_flamethrower_back_crit );

#ifdef STAGING_ONLY
		if ( ShootsNapalm() )
		{
			CTFProjectile_Napalm::Create( pOwner, this );
		}
		else
#endif // STAGING_ONLY
		{
			CTFFlameEntity::Create( GetFlameOriginPos(), pOwner->EyeAngles(), this, tf_flamethrower_velocity.GetFloat(), iDmgType, flDamage, iCritFromBehind == 1 );
		}

		// Pyros can become invis in some game modes.  Hitting fire normally handles this,
		// but in the case of flamethrowers it's likely that stealth will be applied while
		// the fire button is down, so we have to call into RemoveInvisibility here, too.
		if ( pOwner->m_Shared.IsStealthed() )
		{
			pOwner->RemoveInvisibility();
		}
#endif
	}

#ifdef GAME_DLL
	// Figure how much ammo we're using per shot and add it to our remainder to subtract.  (We may be using less than 1.0 ammo units
	// per frame, depending on how constants are tuned, so keep an accumulator so we can expend fractional amounts of ammo per shot.)
	// Note we do this only on server and network it to client.  If we predict it on client, it can get slightly out of sync w/server
	// and cause ammo pickup indicators to appear
	float flAmmoPerSecond = TF_FLAMETHROWER_AMMO_PER_SECOND_PRIMARY_ATTACK;
	CALL_ATTRIB_HOOK_FLOAT( flAmmoPerSecond, mult_flame_ammopersec );
	m_flAmmoUseRemainder += flAmmoPerSecond * flFiringInterval;
	// take the integer portion of the ammo use accumulator and subtract it from player's ammo count; any fractional amount of ammo use
	// remains and will get used in the next shot
	int iAmmoToSubtract = (int) m_flAmmoUseRemainder;
	if ( iAmmoToSubtract > 0 )
	{
		pOwner->RemoveAmmo( iAmmoToSubtract, m_iPrimaryAmmoType );
		m_flAmmoUseRemainder -= iAmmoToSubtract;
		// round to 2 digits of precision
		m_flAmmoUseRemainder = (float) ( (int) (m_flAmmoUseRemainder * 100) ) / 100.0f;
	}
#endif

	m_flNextPrimaryAttack = gpGlobals->curtime + flFiringInterval;
	m_flTimeWeaponIdle = gpGlobals->curtime + flFiringInterval;

#if !defined (CLIENT_DLL)
	lagcompensation->FinishLagCompensation( pOwner );

	// PASSTIME custom lag compensation for the ball; see also tf_fx_shared.cpp
	// it would be better if all entities could opt-in to this, or a way for lagcompensation to handle non-players automatically
	if ( g_pPasstimeLogic && g_pPasstimeLogic->GetBall() )
	{
		g_pPasstimeLogic->GetBall()->FinishLagCompensation( pOwner );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float AirBurstDamageForce( const Vector &size, float damage, float scale )
{ 
	float force = damage * ((48 * 48 * 82.0) / (size.x * size.y * size.z)) * scale;

	if ( force > 1000.0) 
	{
		force = 1000.0;
	}

	return force;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFFlameThrower::SupportsAirBlastFunction( EFlameThrowerAirblastFunction eFunction ) const
{
	int iSupportedAirBlastFunctions = 0;
	CALL_ATTRIB_HOOK_INT( iSupportedAirBlastFunctions, airblast_functionality_flags );

	// If we don't have this attribute specified, or it is set to the value 0, we interpret
	// that as "I can do everything!".
	if ( iSupportedAirBlastFunctions == 0 )
	{
		// They can do everything unless airblast is disabled, in which case they can do nothing
		return CanAirBlast();
	}

	return (iSupportedAirBlastFunctions & eFunction) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::FireAirBlast( int iAmmoPerShot )
{
	CTFPlayer *pOwner = GetTFPlayerOwner();
	if ( !pOwner )
		return;

	m_bFiredSecondary = true;

#ifdef CLIENT_DLL
	// Stop the flame if we're currently firing
	StopFlame( false );
#endif

	SetWeaponState( FT_STATE_SECONDARY );

#ifdef GAME_DLL
	SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	pOwner->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_SECONDARY );

	int nDash = 0;
	CALL_ATTRIB_HOOK_INT( nDash, airblast_dashes );

	if ( !nDash )
	{
		DeflectProjectiles();
	}
	else
	{
#ifdef STAGING_ONLY
		Vector vDashDir;
		AngleVectors( pOwner->EyeAngles() + QAngle( 0.0f, 180.0f, 0.0f ), &vDashDir );
#else
		Vector vDashDir = pOwner->GetAbsVelocity();
		if ( !pOwner->GetGroundEntity() || vDashDir.Length() == 0.0f )
		{
			AngleVectors( pOwner->EyeAngles(), &vDashDir );
		}
#endif
		vDashDir.z = 0.0f;
		VectorNormalize( vDashDir );

		Vector vCenter = pOwner->WorldSpaceCenter();
		Vector vSize = GetDeflectionSize();
		DeflectPlayer( pOwner, pOwner, vDashDir, vCenter, vSize );
	}

	// for charged airblast
	int iChargedAirblast = 0;
	CALL_ATTRIB_HOOK_INT( iChargedAirblast, set_charged_airblast );
	if ( iChargedAirblast != 0 )
	{
		m_flChargeBeginTime = 0;
	}

	// compression blast doesn't go through the normal "weapon fired" code path
	TheNextBots().OnWeaponFired( pOwner, this );
#endif

#ifdef CLIENT_DLL
	if ( prediction->IsFirstTimePredicted() == true )
	{
		StartFlame();
	}
#endif

	float fAirblastRefireTimeScale = 1.0f;
	CALL_ATTRIB_HOOK_FLOAT( fAirblastRefireTimeScale, mult_airblast_refire_time );
	if ( fAirblastRefireTimeScale <= 0.0f  )
	{
		fAirblastRefireTimeScale = 1.0f;
	}

	float fAirblastPrimaryRefireTimeScale = 1.0f;
	CALL_ATTRIB_HOOK_FLOAT( fAirblastPrimaryRefireTimeScale, mult_airblast_primary_refire_time );
	if ( fAirblastPrimaryRefireTimeScale <= 0.0f )
	{
		fAirblastPrimaryRefireTimeScale = 1.0f;
	}

	// Haste Powerup Rune adds multiplier to fire delay time
	if ( pOwner->m_Shared.GetCarryingRuneType() == RUNE_HASTE )
	{
		fAirblastRefireTimeScale *= 0.5f;
	}

	m_flNextSecondaryAttack = gpGlobals->curtime + (0.75f * fAirblastRefireTimeScale);	
	m_flNextPrimaryAttack = gpGlobals->curtime + (1.0f * fAirblastRefireTimeScale * fAirblastPrimaryRefireTimeScale);
	m_flResetBurstEffect = gpGlobals->curtime + 0.05f;

	pOwner->RemoveAmmo( iAmmoPerShot, m_iPrimaryAmmoType );
}

float CTFFlameThrower::GetSpinUpTime( void ) const
{
	float flSpinUpTime = 0.0f;
	CALL_ATTRIB_HOOK_FLOAT( flSpinUpTime, mod_flamethrower_spinup_time );

	return flSpinUpTime;
}

void CTFFlameThrower::SetWeaponState( int nWeaponState )
{
	if ( m_iWeaponState == nWeaponState )
		return;

	CTFPlayer *pOwner = GetTFPlayerOwner();

	switch ( nWeaponState )
	{
	case FT_STATE_IDLE:
		if ( pOwner )
		{
			float flFiringForwardPull = 0.0f;
			CALL_ATTRIB_HOOK_FLOAT( flFiringForwardPull, firing_forward_pull );
			if ( flFiringForwardPull )
			{
				pOwner->m_Shared.RemoveCond( TF_COND_SPEED_BOOST );
			}
		}
		break;

	case FT_STATE_STARTFIRING:
		if ( pOwner )
		{
			float flFiringForwardPull = 0.0f;
			CALL_ATTRIB_HOOK_FLOAT( flFiringForwardPull, firing_forward_pull );
			if ( flFiringForwardPull )
			{
				pOwner->m_Shared.AddCond( TF_COND_SPEED_BOOST );
			}
		}
		break;
	}

	m_iWeaponState = nWeaponState;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::UseRage( void )
{
	if ( !IsRageFull() )
		return;

	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return;

	if ( !pPlayer->IsAllowedToTaunt() )
		return;

	float flNextAttack = m_flNextSecondaryAttack;

#if GAME_DLL
	// Do a taunt so everyone has a chance to run
	pPlayer->Taunt( TAUNT_BASE_WEAPON );
	if ( pPlayer->m_Shared.IsRageDraining() )
	{
		// taunt succeeded
		flNextAttack = gpGlobals->curtime + 1.0f;
	}
#else
	flNextAttack = gpGlobals->curtime + 1.0f;
#endif

	m_flNextSecondaryAttack = flNextAttack;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::SecondaryAttack()
{
	if ( m_flChargeBeginTime > 0 )
	{
		m_bFiredSecondary = true;
		return;
	}

	if ( m_flNextSecondaryAttack > gpGlobals->curtime )
	{
#ifndef CLIENT_DLL
		if ( m_flResetBurstEffect <= gpGlobals->curtime )
		{
			SetWeaponState( FT_STATE_IDLE );
		}
#endif
		return;
	}

	CTFPlayer *pOwner = GetTFPlayerOwner();
	if ( !pOwner )
		return;

	if ( pOwner->GetWaterLevel() == WL_Eyes )
		return;

	if ( !CanAttack() )
	{
		SetWeaponState( FT_STATE_IDLE );
		return;
	}

	int iAmmo = pOwner->GetAmmoCount( m_iPrimaryAmmoType );

	// charged airblast
	int iChargedAirblast = 0;
	CALL_ATTRIB_HOOK_INT( iChargedAirblast, set_charged_airblast );
	int iBuffType = 0;
	CALL_ATTRIB_HOOK_INT( iBuffType, set_buff_type );
	float flMultAmmoPerShot = 1.0f;
	CALL_ATTRIB_HOOK_FLOAT( flMultAmmoPerShot, mult_airblast_cost );
	int iAmmoPerShot = tf_flamethrower_burstammo.GetInt() * flMultAmmoPerShot;

	if ( iBuffType != 0 )
	{
		UseRage();
		return;
	}

	if ( iAmmo < iAmmoPerShot )
		return;

	// normal air blast?
	if ( iChargedAirblast == 0 && CanAirBlast() )
	{
		FireAirBlast( iAmmoPerShot );
		return;
	}

#ifdef CLIENT_DLL
	// Stop the flame if we're currently firing
	StopFlame( false );
#endif

	SetWeaponState( FT_STATE_SECONDARY );

#ifdef STAGING_ONLY
	if ( RocketPackCanActivate( iAmmoPerShot ) )
	{
		RocketPackLaunch( iAmmoPerShot );
		return;
	}
#endif // STAGING_ONLY

#ifdef GAME_DLL
	m_iWeaponMode = TF_WEAPON_SECONDARY_MODE;
	m_flChargeBeginTime = gpGlobals->curtime;
	SendWeaponAnim( ACT_VM_PULLBACK );
	// @todo replace with the correct one
	WeaponSound( SINGLE );
#endif
}

#ifdef GAME_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector CTFFlameThrower::GetDeflectionSize()
{ 
	const Vector vecBaseDeflectionSize = BaseClass::GetDeflectionSize();
	float fMultiplier = 1.0f;

	// int iChargedAirblast = 0;
	// CALL_ATTRIB_HOOK_INT( iChargedAirblast, set_charged_airblast );
	// if ( iChargedAirblast != 0 )
	// {
	//	 fMultiplier *= RemapValClamped( ( gpGlobals->curtime - m_flChargeBeginTime ),
	// 										  0.0f,
	// 										  GetChargeMaxTime(),
	// 										  AIRBLAST_CHARGE_MULT_MIN,
	// 										  AIRBLAST_CHARGE_MULT_MAX );
	// }

	// Allow custom attributes to scale the deflection size.
	CALL_ATTRIB_HOOK_FLOAT( fMultiplier, deflection_size_multiplier );

	return vecBaseDeflectionSize * fMultiplier;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
#ifdef _DEBUG
ConVar tf_pushbackscalescale( "tf_pushbackscalescale", "1.0" );
ConVar tf_pushbackscalescale_vertical( "tf_pushbackscalescale_vertical", "1.0" );
#endif

void ExtinguishPlayer( CEconEntity *pExtinguisher, CTFPlayer *pOwner, CTFPlayer *pTarget, const char *pExtinguisherName )
{
	pTarget->EmitSound( "TFPlayer.FlameOut" );

	pTarget->m_Shared.RemoveCond( TF_COND_BURNING );

	// we're going to limit the number of times you can be awarded bonus points to prevent exploits
	if ( pOwner->ShouldGetBonusPointsForExtinguishEvent( pTarget->GetUserID() ) )
	{
		CTF_GameStats.Event_PlayerAwardBonusPoints( pOwner, pTarget, 10 );
	}

	CRecipientFilter involved_filter;
	involved_filter.AddRecipient( pOwner );
	involved_filter.AddRecipient( pTarget );

	UserMessageBegin( involved_filter, "PlayerExtinguished" );
	WRITE_BYTE( pOwner->entindex() );
	WRITE_BYTE( pTarget->entindex() );
	MessageEnd();

	IGameEvent *event = gameeventmanager->CreateEvent( "player_extinguished" );
	if ( event )
	{
		event->SetInt( "victim", pTarget->entindex() );
		event->SetInt( "healer", pOwner->entindex() );

		gameeventmanager->FireEvent( event, true );
	}

	// stats
	EconEntity_OnOwnerKillEaterEvent( pExtinguisher, pOwner, pTarget, kKillEaterEvent_BurningAllyExtinguished );

	UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"player_extinguished\" against \"%s<%i><%s><%s>\" with \"%s\" (attacker_position \"%d %d %d\") (victim_position \"%d %d %d\")\n",    
				pOwner->GetPlayerName(), pOwner->GetUserID(), pOwner->GetNetworkIDString(), pOwner->GetTeam()->GetName(),
				pTarget->GetPlayerName(), pTarget->GetUserID(), pTarget->GetNetworkIDString(), pTarget->GetTeam()->GetName(),
				pExtinguisherName, (int)pOwner->GetAbsOrigin().x, (int)pOwner->GetAbsOrigin().y, (int)pOwner->GetAbsOrigin().z,
				(int)pTarget->GetAbsOrigin().x, (int)pTarget->GetAbsOrigin().y, (int)pTarget->GetAbsOrigin().z );
}

bool CTFFlameThrower::DeflectPlayer( CTFPlayer *pTarget, CTFPlayer *pOwner, Vector &vecForward, Vector &vecCenter, Vector &vecSize )
{
	if ( pTarget->GetTeamNumber() == pOwner->GetTeamNumber() && pTarget != pOwner )
	{
		if ( pTarget->m_Shared.InCond( TF_COND_BURNING ) && SupportsAirBlastFunction( TF_FUNCTION_AIRBLAST_PUT_OUT_TEAMMATES ) )
		{
			ExtinguishPlayer( this, pOwner, pTarget, "tf_weapon_flamethrower" );

			// Return health to the Pyro. 
			// We may want to cap the amount of health per extinguish but for now lets test this
			int iRestoreHealthOnExtinguish = 0;
			CALL_ATTRIB_HOOK_INT( iRestoreHealthOnExtinguish, extinguish_restores_health );
			if ( iRestoreHealthOnExtinguish > 0 )
			{
				pOwner->TakeHealth( iRestoreHealthOnExtinguish, DMG_GENERIC );
				IGameEvent *healevent = gameeventmanager->CreateEvent( "player_healonhit" );
				if ( healevent )
				{
					healevent->SetInt( "amount", iRestoreHealthOnExtinguish );
					healevent->SetInt( "entindex", pOwner->entindex() );
					item_definition_index_t healingItemDef = INVALID_ITEM_DEF_INDEX;
					if ( GetAttributeContainer() && GetAttributeContainer()->GetItem() )
					{
						healingItemDef = GetAttributeContainer()->GetItem()->GetItemDefIndex();
					}
					healevent->SetInt( "weapon_def_index", healingItemDef );

					gameeventmanager->FireEvent( healevent ); 
				}
			}
		}

		return false;
	}
	
	if ( SupportsAirBlastFunction( TF_FUNCTION_AIRBLAST_PUSHBACK ) )
	{
		int iReverseBlast = 0;
		CALL_ATTRIB_HOOK_INT( iReverseBlast, reverse_airblast );

		// Against players, let's force the pyro to be actually looking at them.
		// We'll be a bit more laxed when it comes to aiming at rockets and grenades.
		Vector vecToTarget;

		if ( pTarget == pOwner )
		{
			vecToTarget = vecForward;
		}
		else
		{
			vecToTarget = pTarget->WorldSpaceCenter() - pOwner->WorldSpaceCenter();
			VectorNormalize( vecToTarget );
		}

		// Quick Fix Uber is immune
		if ( pTarget->m_Shared.InCond( TF_COND_MEGAHEAL )) 
			return false;


		// Require our target be in a cone in front of us. Default threshold is the dot-product needs to be at least 0.8 = 1 - 0.2. 
		float flDot = DotProduct( vecForward, vecToTarget );
		float flAirblastConeScale = 0.2f;
		CALL_ATTRIB_HOOK_FLOAT( flAirblastConeScale, mult_airblast_cone_scale );
		float flAirblastConeThreshold = Clamp(1.0f - flAirblastConeScale, 0.0f, 1.0f);
		if (flDot < flAirblastConeThreshold)
		{
			return false;
		}

		if ( pTarget != pOwner )
		{
			pTarget->SetAbsVelocity( vec3_origin );

			if ( SupportsAirBlastFunction( TF_FUNCTION_AIRBLAST_PUSHBACK__STUN ) )
			{
				if ( !pTarget->m_Shared.InCond( TF_COND_KNOCKED_INTO_AIR ) )
				{
					pTarget->m_Shared.StunPlayer( tf_player_movement_stun_time.GetFloat(), 1.f, TF_STUN_MOVEMENT, pOwner );
				}
			}

			if ( SupportsAirBlastFunction( TF_FUNCTION_AIRBLAST_PUSHBACK__VIEW_PUNCH ) )
			{
				pTarget->ApplyPunchImpulseX( RandomInt( 10, 15 ) );
			}
		}

		pTarget->SpeakConceptIfAllowed( MP_CONCEPT_DEFLECTED, "projectile:0,victim:1" );

		float flForce = AirBurstDamageForce( pTarget->WorldAlignSize(), 60, 6.f );

		CALL_ATTRIB_HOOK_FLOAT( flForce, airblast_pushback_scale );

#ifdef _DEBUG
		Vector vecForce = vecToTarget * flForce * tf_pushbackscalescale.GetFloat();
#else
		Vector vecForce = vecToTarget * flForce;	
#endif

		if ( iReverseBlast )
		{
			vecForce = -vecForce;
		}

		float flVerticalPushbackScale = tf_flamethrower_burst_zvelocity.GetFloat();
		if ( iReverseBlast )
		{
			// Don't give quite so big a vertical kick if we're sucking rather than blowing...
			flVerticalPushbackScale *= 0.75f;
		}

#ifdef STAGING_ONLY
		if ( !( pTarget == pOwner && pOwner->GetGroundEntity() ) )
#endif		
		{
			CALL_ATTRIB_HOOK_FLOAT( flVerticalPushbackScale, airblast_vertical_pushback_scale );
		}

#ifdef _DEBUG
		vecForce.z += flVerticalPushbackScale * tf_pushbackscalescale_vertical.GetFloat();

		/*
		// Kyle says: this will force players off the ground for at least one frame.
		//			  This is disabled on purpose right now to match previous flamethrower functionality.
		if ( pTarget->GetFlags() & FL_ONGROUND )
		{
			vecForce.z += 268.3281572999747f;
		}
		*/
#else
		vecForce.z += flVerticalPushbackScale;
#endif

		// Apply AirBlastImpulse
		pTarget->ApplyAirBlastImpulse( vecForce );
		
		// Make sure we get credit for the airblast if the target falls to its death
		pTarget->m_AchievementData.AddDamagerToHistory( pOwner );

		SendObjectDeflectedEvent( pOwner, pTarget, TF_WEAPON_NONE, pTarget ); // TF_WEAPON_NONE means the player got pushed

		// If the target is charging, stop the charge and keep the charge meter where it is.
		pTarget->m_Shared.InterruptCharge();

		// Track for achievements
		pTarget->m_AchievementData.AddPusherToHistory( pOwner );

		// Give bonus points whenever a pyro pushes high-value targets back
		if ( TFGameRules() && ( pTarget->IsMiniBoss() || pTarget->m_Shared.IsInvulnerable() ) )
		{
			int nAmount = pTarget->IsMiniBoss() ? 10 : 5;
			CTF_GameStats.Event_PlayerAwardBonusPoints( pOwner, pTarget, nAmount );
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::PlayDeflectionSound( bool bPlayer )
{
	if ( bPlayer )
	{
		EmitSound( "TFPlayer.AirBlastImpact" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFFlameThrower::DeflectEntity( CBaseEntity *pTarget, CTFPlayer *pOwner, Vector &vecForward, Vector &vecCenter, Vector &vecSize )
{
	Assert( pTarget );
	Assert( pOwner );

	if ( !SupportsAirBlastFunction( TF_FUNCTION_AIRBLAST_REFLECT_PROJECTILES ) )
		return false;

	// can't deflect things on our own team
	// except the passtime ball when in passtime mode
	if ( (pTarget->GetTeamNumber() == pOwner->GetTeamNumber()) 
		&& !(g_pPasstimeLogic && (g_pPasstimeLogic->GetBall() == pTarget)) )
	{
		return false;
	}

	// Grab the owner of the projectile *before* we reflect it.
	CTFPlayer *pTFPlayerVictim = dynamic_cast<CTFPlayer *>( pTarget );
	if ( !pTFPlayerVictim )
	{
		pTFPlayerVictim = dynamic_cast<CTFPlayer *>( pTarget->GetOwnerEntity() );
	}

	if ( !pTFPlayerVictim )
	{
		// We can't use OwnerEntity for grenades, because then the owner can't shoot them with his hitscan weapons (due to collide rules)
		// Thrower is used to store the person who threw the grenade, for damage purposes.
		CBaseGrenade *pBaseGrenade = dynamic_cast< CBaseGrenade*>( pTarget );
		if ( pBaseGrenade )
		{
			pTFPlayerVictim = dynamic_cast<CTFPlayer *>( pBaseGrenade->GetThrower() );
		}
	}

	if ( !pTFPlayerVictim )
	{
		// Is the OwnerEntity() a base object, like a sentry gun shooting rockets at us?
		if ( pTarget->GetOwnerEntity() && pTarget->GetOwnerEntity()->IsBaseObject() )
		{
			CBaseObject *pObj = dynamic_cast<CBaseObject *>( pTarget->GetOwnerEntity() );
			if ( pObj )
			{
				pTFPlayerVictim = dynamic_cast<CTFPlayer *>( pObj->GetOwner() );
			}
		}
	}

	bool bDeflected = BaseClass::DeflectEntity( pTarget, pOwner, vecForward, vecCenter, vecSize );
	if ( bDeflected )
	{
		pTarget->EmitSound( "Weapon_FlameThrower.AirBurstAttackDeflect" );

		EconEntity_OnOwnerKillEaterEvent( this, pOwner, pTFPlayerVictim, kKillEaterEvent_ProjectileReflect );
	}
	return bDeflected;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFFlameThrower::Lower( void )
{
	if ( BaseClass::Lower() )
	{
		// If we were firing, stop
		if ( m_iWeaponState > FT_STATE_IDLE )
		{
			SendWeaponAnim( ACT_MP_ATTACK_STAND_POSTFIRE );
			SetWeaponState( FT_STATE_IDLE );
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the position of the tip of the muzzle at it appears visually
//-----------------------------------------------------------------------------
Vector CTFFlameThrower::GetVisualMuzzlePos()
{
	return GetMuzzlePosHelper( true );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the position at which to spawn flame damage entities
//-----------------------------------------------------------------------------
Vector CTFFlameThrower::GetFlameOriginPos()
{
	return GetMuzzlePosHelper( false );
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFFlameThrower::GetFlameHitRatio( void )
{
	// Safety net to avoid divide by zero
	if ( m_iActiveFlames == 0 )
		return 0.1f;

	float flRatio = ( ( (float)m_iDamagingFlames ) / ( (float)m_iActiveFlames ) );
	//Msg( "Act:  %d  Dmg:  %d\n", m_iActiveFlames, m_iDamagingFlames );

	return flRatio;
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFFlameThrower::IncrementFlameDamageCount( void )
{
	m_iDamagingFlames++;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFFlameThrower::DecrementFlameDamageCount( void )
{
	if ( m_iDamagingFlames <= 0 )
		return;

	m_iDamagingFlames--;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFFlameThrower::IncrementActiveFlameCount( void )
{
	m_iActiveFlames++;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFFlameThrower::DecrementActiveFlameCount( void )
{
	if ( m_iActiveFlames <= 0 )
		return;

	m_iActiveFlames--;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFFlameThrower::ResetFlameHitCount( void )
{
	m_iDamagingFlames = 0;
	m_iActiveFlames = 0;
}


//-----------------------------------------------------------------------------
// Purpose: UI Progress
//-----------------------------------------------------------------------------
float CTFFlameThrower::GetProgress( void )
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return 0.f;

	return pPlayer->m_Shared.GetRageMeter() / 100.0f;
}


//-----------------------------------------------------------------------------
// Purpose: UI Progress (same as GetProgress() without the division by 100.0f)
//-----------------------------------------------------------------------------
bool CTFFlameThrower::IsRageFull( void )
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return false;

	return ( pPlayer->m_Shared.GetRageMeter() >= 100.0f );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFFlameThrower::EffectMeterShouldFlash( void )
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( pPlayer && (IsRageFull() || pPlayer->m_Shared.IsRageDraining()) )
		return true;
	else
		return false;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the position of the tip of the muzzle
//-----------------------------------------------------------------------------
Vector CTFFlameThrower::GetMuzzlePosHelper( bool bVisualPos )
{
	Vector vecMuzzlePos;
	CTFPlayer *pOwner = GetTFPlayerOwner();
	if ( pOwner ) 
	{
		Vector vecForward, vecRight, vecUp;
		AngleVectors( pOwner->GetAbsAngles(), &vecForward, &vecRight, &vecUp );
		vecMuzzlePos = pOwner->Weapon_ShootPosition();
		vecMuzzlePos +=  vecRight * TF_FLAMETHROWER_MUZZLEPOS_RIGHT;
		// if asking for visual position of muzzle, include the forward component
		if ( bVisualPos )
		{
			vecMuzzlePos +=  vecForward * TF_FLAMETHROWER_MUZZLEPOS_FORWARD;
		}
	}
	return vecMuzzlePos;
}

void CTFFlameThrower::CalculateHalloweenSpell( void )
{
	m_bHasHalloweenSpell.Set( false );
	if ( TF_IsHolidayActive( kHoliday_HalloweenOrFullMoon ) )
	{
		int iHalloweenSpell = 0;
		CALL_ATTRIB_HOOK_INT_ON_OTHER( this, iHalloweenSpell, halloween_green_flames );
		m_bHasHalloweenSpell.Set( iHalloweenSpell > 0 );
	}
}

bool CTFFlameThrower::Deploy( void )
{
#if defined( CLIENT_DLL )
	StartPilotLight();
	m_flFlameHitRatio = 0;
	m_flPrevFlameHitRatio = -1;
	m_flChargeBeginTime = 0;

	m_bEffectsThinking = true;
	SetContextThink( &CTFFlameThrower::ClientEffectsThink, gpGlobals->curtime, "EFFECTS_THINK" );

	StopFullCritEffect();
#endif // CLIENT_DLL

	CalculateHalloweenSpell();

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFFlameThrower::FireGameEvent( IGameEvent *event )
{
	if ( FStrEq( event->GetName(), "recalculate_holidays" ) )
	{
		CalculateHalloweenSpell();
	}
}

#if defined( CLIENT_DLL )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged(updateType);

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	C_TFPlayer *pPlayerOwner = GetTFPlayerOwner();

	//
	bool bLocalPlayerAmmo = true;

	if ( pPlayerOwner == pLocalPlayer )
	{
		bLocalPlayerAmmo = GetPlayerOwner()->GetAmmoCount( m_iPrimaryAmmoType ) > 0;
	}

	if ( IsCarrierAlive() && ( WeaponState() == WEAPON_IS_ACTIVE ) && bLocalPlayerAmmo == true )
	{
		if ( m_iWeaponState > FT_STATE_IDLE )
		{
			if ( ( m_iWeaponState == FT_STATE_SECONDARY && GetPlayerOwner() != C_BasePlayer::GetLocalPlayer() ) || m_iWeaponState != FT_STATE_SECONDARY )
			{
				StartFlame();

#ifdef STAGING_ONLY
				if ( ShootsNapalm() )
				{
					RestartParticleEffect();
				}
#endif // STAGING_ONLY
			}
		}
		else
		{
			StartPilotLight();
		}
	}
	else 
	{
		StopFlame();
		StopPilotLight();
		StopFullCritEffect();
		m_bEffectsThinking = false;
	}
	
	if ( pPlayerOwner == pLocalPlayer )
	{
		if ( m_pFiringLoop )
		{
 			m_flFlameHitRatio = GetFlameHitRatio();
 			m_flFlameHitRatio = RemapValClamped( m_flFlameHitRatio, 0.0f, 1.0f, 1.0f, 100.f );

			//Msg ( "%f\n", m_flFlameHitRatio );

			if ( m_flFlameHitRatio != m_flPrevFlameHitRatio )
			{
				m_flPrevFlameHitRatio = m_flFlameHitRatio;

				CLocalPlayerFilter filter;
				CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

				// We play accent sounds based on accuracy
				if ( m_flFlameHitRatio >= TF_FLAMETHROWER_HITACCURACY_HIGH )
				{
					controller.SoundChangePitch( m_pFiringLoop, 140, 0.1 );	
					m_szAccuracySound = "Weapon_FlameThrower.FireHitHard";
				}
				else
				{
					controller.SoundChangePitch( m_pFiringLoop, 100, 0.1 );	

					// If our accuracy is too low
					if ( m_pFiringAccuracyLoop )
					{
						controller.SoundDestroy( m_pFiringAccuracyLoop );
						m_pFiringAccuracyLoop = NULL;					
					}

					return;
				}

				// Only start a new sound if there's been a change
				if ( !m_pFiringAccuracyLoop )
				{
					m_pFiringAccuracyLoop = controller.SoundCreate( filter, entindex(), m_szAccuracySound );
					controller.Play( m_pFiringAccuracyLoop, 1.0, 100 );
				}

			}
		}
		else if ( m_pFiringAccuracyLoop )
		{
			CSoundEnvelopeController::GetController().SoundDestroy( m_pFiringAccuracyLoop );
			m_pFiringAccuracyLoop = NULL;
		}

		if ( GetBuffType() > 0 )
		{
			if ( !m_bFullRageEffect && pPlayerOwner && pPlayerOwner->m_Shared.GetRageMeter() >= 100.0f )
			{
				m_bFullRageEffect = true;
				m_MmmmphEffect.StartEffects( FullCritChargedEffectName() );
			}
			else if ( m_bFullRageEffect && pPlayerOwner && pPlayerOwner->m_Shared.GetRageMeter() < 100.0f )
			{
				StopFullCritEffect();
				m_MmmmphEffect.StopEffects();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::UpdateOnRemove( void )
{
	m_FlameEffects.StopEffects();
	m_MmmmphEffect.StopEffects();
	StopPilotLight();
	StopFullCritEffect();
	m_bEffectsThinking = false;

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::SetDormant( bool bDormant )
{
	// If I'm going from active to dormant and I'm carried by another player, stop our firing sound.
	if ( !IsCarriedByLocalPlayer() )
	{
		if ( !IsDormant() && bDormant )
		{
			StopFlame();
			StopPilotLight();
			StopFullCritEffect();
			m_bEffectsThinking = false;
		}
	}

	// Deliberately skip base combat weapon to avoid being holstered
	C_BaseEntity::SetDormant( bDormant );
}

int CTFFlameThrower::GetWorldModelIndex( void )
{
	int iParticleEffectIndex = 0;
	CALL_ATTRIB_HOOK_INT( iParticleEffectIndex, set_weapon_mode );

	// Pyro bubble wand support.
	if ( iParticleEffectIndex == 3 )
	{
		CTFPlayer *pPlayer = ToTFPlayer( GetOwner() );
		if ( pPlayer && pPlayer->m_Shared.InCond( TF_COND_TAUNTING ) && pPlayer->m_Shared.GetTauntIndex() == TAUNT_BASE_WEAPON )
		{
			// While we are taunting, replace our normal world model with the bubble wand.
			m_iWorldModelIndex = modelinfo->GetModelIndex( TF_WEAPON_BUBBLE_WAND_MODEL );
			return m_iWorldModelIndex;
		}
	}

	return BaseClass::GetWorldModelIndex();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::StartFlame()
{
	if ( m_iWeaponState == FT_STATE_SECONDARY )
	{
		GetAppropriateWorldOrViewModel()->ParticleProp()->Create( "pyro_blast", PATTACH_POINT_FOLLOW, "muzzle" );
		CLocalPlayerFilter filter;
		const char *shootsound = GetShootSound( WPN_DOUBLE );
		EmitSound( filter, entindex(), shootsound );

		return;
	}

	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	// normally, crossfade between start sound & firing loop in 3.5 sec
	float flCrossfadeTime = 3.5;

	if ( m_pFiringLoop && ( m_bCritFire != m_bFiringLoopCritical ) )
	{
		// If we're firing and changing between critical & noncritical, just need to change the firing loop.
		// Set crossfade time to zero so we skip the start sound and go to the loop immediately.

		flCrossfadeTime = 0;
		StopFlame( true );
	}

	StopPilotLight();

	if ( !m_pFiringStartSound && !m_pFiringLoop )
	{
		// NVNT if the local player is owning this weapon, process the start event
		if ( C_BasePlayer::GetLocalPlayer() == GetOwner() && haptics )
			haptics->ProcessHapticEvent(2,"Weapons","flamer_start");

		RestartParticleEffect();
		CLocalPlayerFilter filter;

		// Play the fire start sound
		const char *shootsound = GetShootSound( SINGLE );
		if ( flCrossfadeTime > 0.0 )
		{
			// play the firing start sound and fade it out
			m_pFiringStartSound = controller.SoundCreate( filter, entindex(), shootsound );		
			controller.Play( m_pFiringStartSound, 1.0, 100 );
			controller.SoundChangeVolume( m_pFiringStartSound, 0.0, flCrossfadeTime );
		}

		// Start the fire sound loop and fade it in
		if ( m_bCritFire )
		{
			shootsound = GetShootSound( BURST );
		}
		else
		{
			shootsound = GetShootSound( SPECIAL1 );
		}
		m_pFiringLoop = controller.SoundCreate( filter, entindex(), shootsound );
		m_bFiringLoopCritical = m_bCritFire;

		// play the firing loop sound and fade it in
		if ( flCrossfadeTime > 0.0 )
		{
			controller.Play( m_pFiringLoop, 0.0, 100 );
			controller.SoundChangeVolume( m_pFiringLoop, 1.0, flCrossfadeTime );
		}
		else
		{
			controller.Play( m_pFiringLoop, 1.0, 100 );
		}
	}

	// check our "hit" sound
	if ( m_bHitTarget != m_bFiringHitTarget )
	{
		if ( m_bHitTarget == false )
		{
			StopHitSound();
		}
		else
		{
			char *pchFireHitSound = "Weapon_FlameThrower.FireHit";

			int iParticleEffectIndex = 0;
			CALL_ATTRIB_HOOK_INT( iParticleEffectIndex, set_weapon_mode );

			if ( iParticleEffectIndex == 3 )
			{
				pchFireHitSound = "Weapon_Rainblower.FireHit";
			}

			CLocalPlayerFilter filter;
			m_pFiringHitLoop = controller.SoundCreate( filter, entindex(), pchFireHitSound );	
			controller.Play( m_pFiringHitLoop, 1.0, 100 );
		}

		m_bFiringHitTarget = m_bHitTarget;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::StopHitSound()
{
	if ( m_pFiringHitLoop )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pFiringHitLoop );
		m_pFiringHitLoop = NULL;
	}

	m_bHitTarget = m_bFiringHitTarget = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::StopFlame( bool bAbrupt /* = false */ )
{
	if ( ( m_pFiringLoop || m_pFiringStartSound ) && !bAbrupt )
	{
		// play a quick wind-down poof when the flame stops
		CLocalPlayerFilter filter;
		const char *shootsound = GetShootSound( SPECIAL3 );
		EmitSound( filter, entindex(), shootsound );
	}

	if ( m_pFiringLoop )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pFiringLoop );
		m_pFiringLoop = NULL;
	}

	if ( m_pFiringStartSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pFiringStartSound );
		m_pFiringStartSound = NULL;
	}

	if ( m_FlameEffects.StopEffects() )
	{
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pLocalPlayer && pLocalPlayer == GetOwner() )
		{
			// NVNT local player is finished firing. send the stop event.
			if ( haptics )
				haptics->ProcessHapticEvent(2,"Weapons","flamer_stop");
		}
	}

	if ( !bAbrupt )
	{
		StopHitSound();
	}

	m_iParticleWaterLevel = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::StartPilotLight()
{
	if ( !m_pPilotLightSound )
	{
		StopFlame();

		// Create the looping pilot light sound
		const char *pilotlightsound = GetShootSound( SPECIAL2 );
		CLocalPlayerFilter filter;

		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		m_pPilotLightSound = controller.SoundCreate( filter, entindex(), pilotlightsound );

		controller.Play( m_pPilotLightSound, 1.0, 100 );
	}	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::StopPilotLight()
{
	if ( m_pPilotLightSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pPilotLightSound );
		m_pPilotLightSound = NULL;
	}
}

void CTFFlameThrower::StopFullCritEffect()
{
	m_bFullRageEffect = false;

	m_MmmmphEffect.StopEffects();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::RestartParticleEffect( void )
{
	CTFPlayer *pOwner = GetTFPlayerOwner();
	if ( !pOwner )
		return;

	if ( m_iWeaponState != FT_STATE_FIRING && m_iWeaponState != FT_STATE_STARTFIRING )
	{
		return;
	}

	m_iParticleWaterLevel = pOwner->GetWaterLevel();

	bool bIsFirstPersonView = IsFirstPersonView();

	// Start the appropriate particle effect
	const char *pszParticleEffect;
	if ( pOwner->GetWaterLevel() == WL_Eyes )
	{
		pszParticleEffect = "flamethrower_underwater";
	}
	else
	{
		if ( m_bCritFire )
		{
			pszParticleEffect = FlameCritEffectName( bIsFirstPersonView );
		}
		else 
		{
			pszParticleEffect = FlameEffectName( bIsFirstPersonView );
		}
	}

	m_FlameEffects.StartEffects( pszParticleEffect );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char* CTFFlameThrower::FlameEffectName( bool bIsFirstPersonView )
{
	CTFPlayer *pOwner = GetTFPlayerOwner();
	if ( !pOwner )
		return NULL;

#ifdef STAGING_ONLY
	if ( ShootsNapalm() )
	{
		return "muzzle_bignasty";
	}
#endif // STAGING_ONLY

	// Halloween Spell
	if ( m_bHasHalloweenSpell )
	{
		return "flamethrower_halloween";
	}

	int iParticleEffectIndex = 0;
	CALL_ATTRIB_HOOK_INT( iParticleEffectIndex, set_weapon_mode );

	switch ( iParticleEffectIndex )
	{
	case 1:		return "drg_phlo_stream";
	case 2:		return "flamethrower_giant_mvm";
	case 3:		return ( bIsFirstPersonView ? "flamethrower_rainbow_FP" : "flamethrower_rainbow" );
	default:	return ( pOwner->GetTeamNumber() == TF_TEAM_BLUE ? "flamethrower_blue" : "flamethrower" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char* CTFFlameThrower::FlameCritEffectName( bool bIsFirstPersonView )
{
	CTFPlayer *pOwner = GetTFPlayerOwner();
	if ( !pOwner )
		return NULL;

#ifdef STAGING_ONLY
	if ( ShootsNapalm() )
	{
		return "muzzle_bignasty";
	}
#endif // STAGING_ONLY

	// Halloween Spell
	if ( m_bHasHalloweenSpell )
	{
		return ( pOwner->GetTeamNumber() == TF_TEAM_BLUE ? "flamethrower_halloween_crit_blue" : "flamethrower_halloween_crit_red" );
	}

	int iParticleEffectIndex = 0;
	CALL_ATTRIB_HOOK_INT( iParticleEffectIndex, set_weapon_mode );

	switch ( iParticleEffectIndex )
	{
	case 1:		return "drg_phlo_stream_crit";
	case 2:		return "flamethrower_crit_giant_mvm";
	case 3:		return ( bIsFirstPersonView ? "flamethrower_rainbow_FP" : "flamethrower_rainbow" );
	default:	return ( pOwner->GetTeamNumber() == TF_TEAM_BLUE ? "flamethrower_crit_blue" : "flamethrower_crit_red" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char* CTFFlameThrower::FullCritChargedEffectName( void )
{
	switch( GetTeamNumber() )
	{
	case TF_TEAM_BLUE:	return "medicgun_invulnstatus_fullcharge_blue";
	case TF_TEAM_RED:	return "medicgun_invulnstatus_fullcharge_red";
	default:			return "";
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::ClientEffectsThink( void )
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return;

	if ( !pPlayer->IsLocalPlayer() )
		return;

	if ( !pPlayer->GetViewModel() )
		return;

	if ( !m_bEffectsThinking )
		return;

	float flRageInverse = 1.f;

	if ( GetBuffType() > 0 )
	{
		flRageInverse = 1.0f - ( pPlayer->m_Shared.GetRageMeter() / 100.0f );
		if ( flRageInverse < 1.0f )
		{
			// We have some rage, let's spark!
			ParticleProp()->Init( this );
			CNewParticleEffect* pEffect = ParticleProp()->Create( "drg_bison_idle", PATTACH_POINT_FOLLOW, "muzzle" );
			if ( pEffect )
			{
				pEffect->SetControlPoint( CUSTOM_COLOR_CP1, GetParticleColor( 1 ) );
				pEffect->SetControlPoint( CUSTOM_COLOR_CP2, GetParticleColor( 2 ) );
			}
		}
	}

	SetContextThink( &CTFFlameThrower::ClientEffectsThink, gpGlobals->curtime + 0.1f + RandomFloat( 1.0f, 5.0f ) * flRageInverse, "EFFECTS_THINK" );
}

void CTFFlameThrower::FlameEffect_t::StartEffects( const char* pszEffectName )
{
	// Stop any old flame effects
	StopEffects();

	// Figure out which weapon this flame effect is to be attached to.  Store this for
	// later so we know which weapon to deactivate the effect on
	m_hEffectWeapon = m_pOwner->GetWeaponForEffect();

	if( m_hEffectWeapon )
	{
		CParticleProperty* pParticleProp = m_hEffectWeapon->ParticleProp();
		if( pParticleProp )
		{
			// Flame on
			m_pFlameEffect = pParticleProp->Create( pszEffectName, PATTACH_POINT_FOLLOW, "muzzle" );
		}
	}
}

bool CTFFlameThrower::FlameEffect_t::StopEffects()
{
	bool bStopped = false;
	// Stop any old flame effects
	if ( m_pFlameEffect && m_hEffectWeapon )
	{
		m_hEffectWeapon->ParticleProp()->StopEmission( m_pFlameEffect );
		bStopped = true;
	}

	m_pFlameEffect = NULL;
	m_hEffectWeapon = NULL;

	return bStopped;
}

#else

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::HitTargetThink( void )
{
	if ( ( m_flTimeToStopHitSound > 0 ) && ( m_flTimeToStopHitSound < gpGlobals->curtime ) )
	{
		m_bHitTarget = false;
		m_flTimeToStopHitSound = 0;
		SetContextThink( NULL, 0, s_pszFlameThrowerHitTargetThink );
		return;
	}

	SetNextThink( gpGlobals->curtime + 0.1f, s_pszFlameThrowerHitTargetThink );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameThrower::SetHitTarget( void )
{ 
	if ( m_iWeaponState > FT_STATE_IDLE )
	{
		m_bHitTarget = true;
		m_flTimeToStopHitSound = gpGlobals->curtime + 0.2;

		// Start the hit target thinking
		SetContextThink( &CTFFlameThrower::HitTargetThink, gpGlobals->curtime + 0.1f, s_pszFlameThrowerHitTargetThink );
	}
}

#endif

#ifdef STAGING_ONLY
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFFlameThrower::RocketPackCanActivate( int nAmmoCost )
{
	CTFPlayer *pOwner = GetTFPlayerOwner();
	if ( !pOwner )
		return false;

	int nRocketPack = 0;
	CALL_ATTRIB_HOOK_INT_ON_OTHER( pOwner, nRocketPack, rocket_pack );
	if ( !nRocketPack )
		return false;

	if ( pOwner->m_Shared.IsLoser() )
		return false;

	if ( pOwner->m_Shared.InCond( TF_COND_STUNNED ) )
		return false;

	if ( pOwner->IsTaunting() )
		return false;

// 	if ( pOwner->m_Shared.GetChargeMeter() < 100.f )
// 		return false;

	if ( pOwner->GetAmmoCount( TF_AMMO_PRIMARY ) < nAmmoCost )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFFlameThrower::RocketPackLaunch( int nAmmoCost )
{
	CTFPlayer *pOwner = GetTFPlayerOwner();
	if ( !pOwner )
		return false;

#ifdef CLIENT_DLL
	StopFlame( false );
#endif // CLIENT_DLL

#ifdef GAME_DLL
	// Launch
	if ( !pOwner->m_Shared.InCond( TF_COND_ROCKETPACK ) )
	{
		pOwner->m_Shared.AddCond( TF_COND_ROCKETPACK );
		pOwner->m_Shared.StunPlayer( 0.5f, 1.0f, TF_STUN_MOVEMENT );
	}

	Vector vecDir;
	pOwner->EyeVectors( &vecDir );
	pOwner->SetAbsVelocity( vec3_origin );
	Vector vecFlightDir = -vecDir;
	VectorNormalize( vecFlightDir );
	float flForce = 450.f;

	const float flPushScale = ( pOwner->GetFlags() & FL_ONGROUND ) ? 1.2f : 1.8f;		// Greater force while airborne
	const float flVertPushScale = ( pOwner->GetFlags() & FL_ONGROUND ) ? 1.2f : 0.25f;	// Less vertical force while airborne
	Vector vecForce = vecFlightDir * -flForce * flPushScale;
	vecForce.z += 1.f * flForce * flVertPushScale;
	pOwner->RemoveFlag( FL_ONGROUND );
	pOwner->ApplyAbsVelocityImpulse( vecForce );

	m_flNextSecondaryAttack = gpGlobals->curtime + 0.75f;
	m_flNextPrimaryAttack = gpGlobals->curtime + 1.f;
	m_flResetBurstEffect = gpGlobals->curtime + 0.05f;
	m_bFiredSecondary = true;
	m_flChargeBeginTime = 0;

	pOwner->RemoveAmmo( nAmmoCost, m_iPrimaryAmmoType );
	pOwner->EmitSound( "Equipment.RocketPack_Activate" );
#endif // GAME_DLL

#ifdef CLIENT_DLL
	if ( prediction->IsFirstTimePredicted() == true )
	{
		StartFlame();
	}
#endif // CLIENT_DLL

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFFlameThrower::ShootsNapalm( void )
{
	int iNapalm = 0;
	CALL_ATTRIB_HOOK_INT_ON_OTHER( GetOwnerEntity(), iNapalm, mod_flamethrower_napalm );
	return ( iNapalm > 0 );
}
#endif // STAGING_ONLY

IMPLEMENT_NETWORKCLASS_ALIASED( TFFlameRocket, DT_TFFlameRocket )
BEGIN_NETWORK_TABLE( CTFFlameRocket, DT_TFFlameRocket )
END_NETWORK_TABLE()

#ifdef GAME_DLL
LINK_ENTITY_TO_CLASS( tf_flame, CTFFlameEntity );
IMPLEMENT_AUTO_LIST( ITFFlameEntityAutoList );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFFlameEntity::CTFFlameEntity()
{}

//-----------------------------------------------------------------------------
// Purpose: Spawns this entity
//-----------------------------------------------------------------------------
void CTFFlameEntity::Spawn( void )
{
	BaseClass::Spawn();

	// don't collide with anything, we do our own collision detection in our think method
	SetSolid( SOLID_NONE );
	SetSolidFlags( FSOLID_NOT_SOLID );
	SetCollisionGroup( COLLISION_GROUP_NONE );
	// move noclip: update position from velocity, that's it
	SetMoveType( MOVETYPE_NOCLIP, MOVECOLLIDE_DEFAULT );
	AddEFlags( EFL_NO_WATER_VELOCITY_CHANGE );

	float iBoxSize = tf_flamethrower_boxsize.GetFloat();
	CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( GetOwnerEntity(), iBoxSize, mult_flame_size );
	UTIL_SetSize( this, -Vector( iBoxSize, iBoxSize, iBoxSize ), Vector( iBoxSize, iBoxSize, iBoxSize ) );

	// Setup attributes.
	m_takedamage = DAMAGE_NO;
	m_vecInitialPos = GetAbsOrigin();
	m_vecPrevPos = m_vecInitialPos;

	// Track total active flame entities
	m_hFlameThrower = dynamic_cast< CTFFlameThrower* >( GetOwnerEntity() );
	if ( m_hFlameThrower )
	{
		m_hFlameThrower->IncrementActiveFlameCount();
		m_bBurnedEnemy = false;

		float flFlameLife = tf_flamethrower_flametime.GetFloat();
		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( GetOwnerEntity(), flFlameLife, mult_flame_life );
		m_flTimeRemove = gpGlobals->curtime + ( flFlameLife * random->RandomFloat( 0.9f, 1.1f ) );
	}
	else
	{
		m_flTimeRemove = gpGlobals->curtime + 3.f;
	}

	// Setup the think function.
	SetThink( &CTFFlameEntity::FlameThink );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: Creates an instance of this entity
//-----------------------------------------------------------------------------
CTFFlameEntity *CTFFlameEntity::Create( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flSpeed, int iDmgType, float flDmgAmount, bool bAlwaysCritFromBehind, bool bRandomize )
{
	CTFFlameEntity *pFlame = static_cast<CTFFlameEntity*>( CBaseEntity::Create( "tf_flame", vecOrigin, vecAngles, pOwner ) );
	if ( !pFlame )
		return NULL;

	// Initialize the owner.
	pFlame->SetOwnerEntity( pOwner );
	if ( pOwner->GetOwnerEntity() )
		pFlame->m_hAttacker = pOwner->GetOwnerEntity();
	else
		pFlame->m_hAttacker = pOwner;
	CBaseEntity *pAttacker = (CBaseEntity *) pFlame->m_hAttacker;
	if ( pAttacker )
	{
		pFlame->m_iAttackerTeam = pAttacker->GetTeamNumber();
	}

	// Set team.
	pFlame->ChangeTeam( pOwner->GetTeamNumber() );
	pFlame->m_iDmgType = iDmgType;
	pFlame->m_flDmgAmount = flDmgAmount;

	// Setup the initial velocity.
	Vector vecForward, vecRight, vecUp;
	AngleVectors( vecAngles, &vecForward, &vecRight, &vecUp );

	float flFlameLifeMult = 1.0f;
	CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pFlame->m_hAttacker, flFlameLifeMult, mult_flame_life );
	float velocity = flFlameLifeMult * flSpeed;
	pFlame->m_vecBaseVelocity = vecForward * velocity;
	float iFlameSizeMult = 1.0f;
	CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pFlame->m_hAttacker, iFlameSizeMult, mult_flame_size );
	if ( bRandomize )
	{
		pFlame->m_vecBaseVelocity += RandomVector( -velocity * iFlameSizeMult * tf_flamethrower_vecrand.GetFloat(), velocity * iFlameSizeMult * tf_flamethrower_vecrand.GetFloat() );
	}
	if ( pOwner->GetOwnerEntity() )
	{
		pFlame->m_vecAttackerVelocity = pOwner->GetOwnerEntity()->GetAbsVelocity();
	}
	pFlame->SetAbsVelocity( pFlame->m_vecBaseVelocity );	
	// Setup the initial angles.
	pFlame->SetAbsAngles( vecAngles );
	pFlame->SetCritFromBehind( bAlwaysCritFromBehind );

	return pFlame;
}

//-----------------------------------------------------------------------------
class CFlameEntityEnum : public IEntityEnumerator
{
public:
	CFlameEntityEnum( CBaseEntity *pShooter )
	{
		m_pShooter = pShooter;
	}

	virtual bool EnumEntity( IHandleEntity *pHandleEntity )
	{
		CBaseEntity *pEnt = static_cast<CBaseEntity*>( pHandleEntity );

		// Ignore collisions with the shooter
		if ( pEnt == m_pShooter )
			return true;

		if ( pEnt->IsPlayer() && pEnt->IsAlive() )
		{
			m_Targets.AddToTail( pEnt );
		}
		else if ( pEnt->MyNextBotPointer() && pEnt->IsAlive() )
		{
			// add non-player bots
			m_Targets.AddToTail( pEnt );
		}
		else if ( pEnt->IsBaseObject() && m_pShooter->GetTeamNumber() != pEnt->GetTeamNumber() )
		{
			// only add enemy objects
			m_Targets.AddToTail( pEnt );
		}
		else if ( CTFRobotDestructionLogic::GetRobotDestructionLogic() && m_pShooter->GetTeamNumber() != pEnt->GetTeamNumber() && FClassnameIs( pEnt, "tf_robot_destruction_robot" ) )
		{
			// only add enemy robots
			m_Targets.AddToTail( pEnt );
		}
		else if ( FClassnameIs( pEnt, "func_breakable" ) || FClassnameIs( pEnt, "tf_pumpkin_bomb" ) || FClassnameIs( pEnt, "tf_merasmus_trick_or_treat_prop" ) )
		{
			m_Targets.AddToTail( pEnt );
		}

		return true;
	}

	const CUtlVector< CBaseEntity* >& GetTargets() { return m_Targets; }

public:
	Ray_t		*m_pRay;
	CBaseEntity	*m_pShooter;
	CUtlVector< CBaseEntity* > m_Targets;
};

//-----------------------------------------------------------------------------
// Purpose: Think method
//-----------------------------------------------------------------------------
void CTFFlameEntity::FlameThink( void )
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 )
	// if we've expired, remove ourselves
	if ( gpGlobals->curtime >= m_flTimeRemove )
	{
		RemoveFlame();
		return;
	}
	else
	{
		// Always think, if we haven't died due to our timeout.
		SetNextThink( gpGlobals->curtime );
	}

	// Did we move? should we check collision?
	if ( GetAbsOrigin() != m_vecPrevPos )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s Collision", __FUNCTION__ );
		CTFPlayer *pAttacker = dynamic_cast<CTFPlayer *>( (CBaseEntity *) m_hAttacker );
		if ( !pAttacker )
			return;

		// Create a ray for flame entity to trace
		Ray_t rayWorld;
		rayWorld.Init( m_vecInitialPos, GetAbsOrigin(), WorldAlignMins(), WorldAlignMaxs() );

		// check against world first
		// if we collide with world, just destroy the flame
		trace_t trWorld;
		UTIL_TraceRay( rayWorld, MASK_SOLID, this, COLLISION_GROUP_DEBRIS, &trWorld );

		bool bHitWorld = trWorld.startsolid || trWorld.fraction < 1.f;

		// update the ray
		Ray_t rayEnt;
		rayEnt.Init( m_vecPrevPos, GetAbsOrigin(), WorldAlignMins(), WorldAlignMaxs() );

		// burn all entities that we should collide with
		CFlameEntityEnum eFlameEnum( pAttacker );
		enginetrace->EnumerateEntities( rayEnt, false, &eFlameEnum );

		bool bHitSomething = false;
		FOR_EACH_VEC( eFlameEnum.GetTargets(), i )
		{
			CBaseEntity *pEnt = eFlameEnum.GetTargets()[i];

			// skip ent that's already burnt by this flame
			int iIndex = m_hEntitiesBurnt.Find( pEnt );
			if ( iIndex != m_hEntitiesBurnt.InvalidIndex() )
				continue;

			// if we're removing the flame this frame from hitting world, check if we hit this ent before hitting the world
			if ( bHitWorld )
			{
				trace_t trEnt;
				enginetrace->ClipRayToEntity( rayWorld, MASK_SOLID | CONTENTS_HITBOX, pEnt, &trEnt );
				// hit world before this ent, skip it
				if ( trEnt.fraction >= trWorld.fraction )
					continue;
			}

			// burn them all!
			if ( pEnt->IsPlayer() && pEnt->InSameTeam( pAttacker ) )
			{
				OnCollideWithTeammate( ToTFPlayer( pEnt ) );
			}
			else
			{
				OnCollide( pEnt );
			}

			bHitSomething = true;
		}

		// now, let's see if the flame visual could have actually hit this player.  Trace backward from the
		// point of impact to where the flame was fired, see if we hit anything.
		if ( bHitSomething && tf_debug_flamethrower.GetBool() )
		{
			NDebugOverlay::SweptBox( m_vecPrevPos, GetAbsOrigin(), WorldAlignMins(), WorldAlignMaxs(), vec3_angle, 255, 255, 0, 100, 5.0 );
			NDebugOverlay::EntityBounds( this, 255, 255, 0, 100, 5.0 );
		}

		// remove the flame if it hits the world
		if ( bHitWorld )
		{
			if ( tf_debug_flamethrower.GetInt() )
			{
				NDebugOverlay::SweptBox( m_vecInitialPos, GetAbsOrigin(), WorldAlignMins(), WorldAlignMaxs(), vec3_angle, 255, 0, 0, 100, 3.0 );
			}

			RemoveFlame();
		}
	}

	// Reduce our base velocity by the air drag constant
	m_vecBaseVelocity *= GetFlameDrag();

	// Add our float upward velocity
	Vector vecVelocity = m_vecBaseVelocity + Vector( 0, 0, GetFlameFloat() ) + m_vecAttackerVelocity;

	// Update our velocity
	SetAbsVelocity( vecVelocity );
	
	// Render debug visualization if convar on
	if ( tf_debug_flamethrower.GetInt() )
	{
		if ( m_hEntitiesBurnt.Count() > 0 )
		{
			int val = ( (int) ( gpGlobals->curtime * 10 ) ) % 255;
			NDebugOverlay::EntityBounds(this, val, 255, val, 0 ,0 );
		} 
		else 
		{
			NDebugOverlay::EntityBounds(this, 0, 100, 255, 0 ,0) ;
		}
	}

	m_vecPrevPos = GetAbsOrigin();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameEntity::SetHitTarget( void )
{
	if ( !m_hFlameThrower )
		return;

	m_hFlameThrower->SetHitTarget();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameEntity::RemoveFlame()
{
	UpdateFlameThrowerHitRatio();

	UTIL_Remove( this );
}


//-----------------------------------------------------------------------------
// Purpose: Called when we've collided with another entity
//-----------------------------------------------------------------------------
void CTFFlameEntity::OnCollide( CBaseEntity *pOther )
{
	int nContents = UTIL_PointContents( GetAbsOrigin() );
	if ( (nContents & MASK_WATER) )
	{
		RemoveFlame();
		return;
	}

	// remember that we've burnt this player
	m_hEntitiesBurnt.AddToTail( pOther );
	
	float flDistance = GetAbsOrigin().DistTo( m_vecInitialPos );
	float flDamage = m_flDmgAmount * RemapValClamped( flDistance, tf_flamethrower_maxdamagedist.GetFloat()/2, tf_flamethrower_maxdamagedist.GetFloat(), 1.0f, 0.70f );

	flDamage = MAX( flDamage, 1.0 );
	if ( tf_debug_flamethrower.GetInt() )
	{
		Msg( "Flame touch dmg: %.1f\n", flDamage );
	}

	CBaseEntity *pAttacker = m_hAttacker;
	if ( !pAttacker )
		return;

	SetHitTarget();

	int iDamageType = m_iDmgType;

	if ( pOther && pOther->IsPlayer() )
	{
		CTFPlayer *pVictim = ToTFPlayer( pOther );
		if ( IsBehindTarget( pOther ) )
		{
			if ( m_bCritFromBehind == true )
			{
				iDamageType |= DMG_CRITICAL;
			}

			if ( pVictim )
			{
				pVictim->HandleAchievement_Pyro_BurnFromBehind( ToTFPlayer( pAttacker ) );
			}
		}

		// Pyro-specific
		if ( pAttacker->IsPlayer() && pVictim )
		{
			CTFPlayer *pPlayerAttacker = ToTFPlayer( pAttacker );
			if ( pPlayerAttacker && pPlayerAttacker->IsPlayerClass( TF_CLASS_PYRO ) )
			{
				// burn the victim while taunting?
				if ( pVictim->m_Shared.InCond( TF_COND_TAUNTING ) )
				{
					static CSchemaItemDefHandle flipTaunt( "Flippin' Awesome Taunt" );
					// if I'm the one being flipped, and getting lit on fire
					if ( !pVictim->IsTauntInitiator() && pVictim->GetTauntEconItemView() && pVictim->GetTauntEconItemView()->GetItemDefinition() == flipTaunt )
					{
						pPlayerAttacker->AwardAchievement( ACHIEVEMENT_TF_PYRO_IGNITE_PLAYER_BEING_FLIPPED );
					}
				}

				pVictim->m_Shared.AddCond( TF_COND_HEALING_DEBUFF, 2.f, pAttacker );
			}
		}
	}

	CTakeDamageInfo info( GetOwnerEntity(), pAttacker, GetOwnerEntity(), flDamage, iDamageType, TF_DMG_CUSTOM_BURNING );
	info.SetReportedPosition( pAttacker->GetAbsOrigin() );

	if ( info.GetDamageType() & DMG_CRITICAL )
	{
		info.SetCritType( CTakeDamageInfo::CRIT_FULL );
	}

	// terrible hack for flames hitting the Merasmus props to get the particle effect in the correct position
	if ( TFGameRules() && TFGameRules()->GetActiveBoss() && ( TFGameRules()->GetActiveBoss()->GetBossType() == HALLOWEEN_BOSS_MERASMUS ) )
	{
		info.SetDamagePosition( GetAbsOrigin() );
	}

	// Track hits for the Flamethrower, which is used to change the weapon sound based on hit ratio
	if ( m_hFlameThrower )
	{
		m_bBurnedEnemy = true;
		m_hFlameThrower->IncrementFlameDamageCount();
	}

	// We collided with pOther, so try to find a place on their surface to show blood
	trace_t pTrace;
	UTIL_TraceLine( WorldSpaceCenter(), pOther->WorldSpaceCenter(), MASK_SOLID|CONTENTS_HITBOX, this, COLLISION_GROUP_NONE, &pTrace );

	pOther->DispatchTraceAttack( info, GetAbsVelocity(), &pTrace );
	ApplyMultiDamage();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameEntity::OnCollideWithTeammate( CTFPlayer *pPlayer )
{
	// Only care about Snipers
	if ( !pPlayer->IsPlayerClass(TF_CLASS_SNIPER) )
		return;

	int iIndex = m_hEntitiesBurnt.Find( pPlayer );
	if ( iIndex != m_hEntitiesBurnt.InvalidIndex() )
		return;

	m_hEntitiesBurnt.AddToTail( pPlayer );

	// Does he have the bow?
	CTFWeaponBase *pWpn = pPlayer->GetActiveTFWeapon();
	if ( pWpn && pWpn->GetWeaponID() == TF_WEAPON_COMPOUND_BOW )
	{
		CTFCompoundBow *pBow = static_cast<CTFCompoundBow*>( pWpn );
		pBow->SetArrowAlight( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFFlameEntity::IsBehindTarget( CBaseEntity *pTarget )
{
	return ( DotProductToTarget( pTarget ) > 0.8 );
}

//-----------------------------------------------------------------------------
// Purpose: Utility to calculate dot product between facing angles of flame and target
//-----------------------------------------------------------------------------
float CTFFlameEntity::DotProductToTarget( CBaseEntity *pTarget )
{
	Assert( pTarget );

	// Get the forward view vector of the target, ignore Z
	Vector vecVictimForward;
	AngleVectors( pTarget->EyeAngles(), &vecVictimForward, NULL, NULL );
	vecVictimForward.z = 0.0f;
	vecVictimForward.NormalizeInPlace();

	Vector vecTraveling = m_vecBaseVelocity;
	vecTraveling.z = 0.0f;
	vecTraveling.NormalizeInPlace();

	return DotProduct( vecVictimForward, vecTraveling );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFFlameEntity::UpdateFlameThrowerHitRatio( void )
{
	if ( !m_hFlameThrower )
		return;
	
	if ( m_bBurnedEnemy )
	{
		m_hFlameThrower->DecrementFlameDamageCount();
	}

	m_hFlameThrower->DecrementActiveFlameCount();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFFlameEntity::GetFlameFloat( void )
{
	return tf_flamethrower_float.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFFlameEntity::GetFlameDrag( void )
{
	return tf_flamethrower_drag.GetFloat();
}

#endif // GAME_DLL

#ifdef STAGING_ONLY
//-----------------------------------------------------------------------------
// Purpose: Napalm
//-----------------------------------------------------------------------------
#ifdef GAME_DLL
#define NAPALM_THINK_CONTEXT	"CTFMedigunShield_ShieldThink"
#endif // GAME_DLL

LINK_ENTITY_TO_CLASS( tf_projectile_napalm, CTFProjectile_Napalm );

IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_Napalm, DT_TFProjectile_Napalm )

BEGIN_NETWORK_TABLE( CTFProjectile_Napalm, DT_TFProjectile_Napalm )
#ifdef GAME_DLL
	SendPropEHandle( SENDINFO( m_hFlameThrower ) ),
#else
	RecvPropEHandle( RECVINFO( m_hFlameThrower ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFProjectile_Napalm )
END_PREDICTION_DATA()

// Data
BEGIN_DATADESC( CTFProjectile_Napalm )
#ifdef GAME_DLL
DEFINE_THINKFUNC( NapalmThink ),
#endif // GAME_DLL
END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFProjectile_Napalm::CTFProjectile_Napalm()
{
#ifdef GAME_DLL
	m_flRemoveTime = 0.f;
	m_nHitCount = 0;
	m_flLastBurnTime = 0.f;
#endif // GAME_DLL
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFProjectile_Napalm::~CTFProjectile_Napalm()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_Napalm::Precache()
{
	// PrecacheModel( TF_MODEL_NAPALM );
	PrecacheParticleSystem( "burninggibs" );
	PrecacheParticleSystem( "flaming_arrow" );
	
	PrecacheScriptSound( "Player.PlasmaDamage" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_Napalm::Spawn()
{
	Precache();

#ifdef GAME_DLL
	m_flRemoveTime = gpGlobals->curtime + 2.2f;
	SetContextThink( &CTFProjectile_Napalm::NapalmThink, gpGlobals->curtime, NAPALM_THINK_CONTEXT );
#endif // GAME_DLL

	BaseClass::Spawn();

#ifdef GAME_DLL
	SetDetonateTimerLength( FLT_MAX );
#endif // GAME_DLL
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFProjectile_Napalm *CTFProjectile_Napalm::Create( CBaseCombatCharacter *pOwner, CTFFlameThrower *pLauncher )
{
	if ( pOwner )
	{
		Vector vecForward;
		AngleVectors( pOwner->EyeAngles(), &vecForward, NULL, NULL );
		CTFProjectile_Napalm *pProjectile = static_cast< CTFProjectile_Napalm* >( CBaseEntity::Create( "tf_projectile_napalm", pLauncher->GetVisualMuzzlePos(), pOwner->EyeAngles() ) );
		if ( pProjectile )
		{
			pProjectile->ChangeTeam( pOwner->GetTeamNumber() );

			// Setup the initial velocity.
			float flVelocity = 1100.f;
			pProjectile->m_vecBaseVelocity = vecForward * flVelocity;
			pProjectile->m_vecBaseVelocity += RandomVector( -flVelocity * tf_flamethrower_vecrand.GetFloat(), flVelocity * tf_flamethrower_vecrand.GetFloat() );
			pProjectile->InitGrenade( pProjectile->m_vecBaseVelocity, vec3_origin, pOwner, pLauncher->GetTFWpnData() );
			pProjectile->m_hFlameThrower = pLauncher;

			return pProjectile;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CTFProjectile_Napalm::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_PVSCHECK );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_Napalm::NapalmThink( void )
{
	if ( gpGlobals->curtime > m_flRemoveTime )
	{
		SetContextThink( &CBaseGrenade::SUB_Remove, gpGlobals->curtime, "RemoveThink" );
		SetTouch( NULL );
		return;
	}

	SetContextThink( &CTFProjectile_Napalm::NapalmThink, gpGlobals->curtime + 0.1f, NAPALM_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_Napalm::Explode( trace_t *pTrace, int bitsDamageType )
{
	if ( !m_nHitCount )
	{
		SetModelName( NULL_STRING );
		AddSolidFlags( FSOLID_TRIGGER );

		m_takedamage = DAMAGE_NO;

		// Pull out of the wall a bit.
		if ( pTrace->fraction != 1.f )
		{
			SetAbsOrigin( pTrace->endpos + ( pTrace->plane.normal * 1.0f ) );
		}

		CTFPlayer *pThrower = ToTFPlayer( GetThrower() );
		if ( pThrower )
		{
			const Vector& vecOrigin = GetAbsOrigin();

			// Any effects from the initial explosion
			if ( InitialExplodeEffects( pThrower, pTrace ) )
			{
				// Particle
				if ( GetImpactEffect() )
				{	
					CPVSFilter filter( vecOrigin );

					// Stick effect on the player
					CBaseEntity *pEnt = pTrace->m_pEnt;
					if ( pEnt && pEnt->IsPlayer() && !pThrower->InSameTeam( pEnt ) )
					{
						// TE_TFParticleEffect( filter, 0.f, GetImpactEffect(), pEnt->GetAbsOrigin(), vec3_angle, pEnt, PATTACH_ABSORIGIN_FOLLOW );
						m_flRemoveTime = gpGlobals->curtime;
					}
					// World
					else
					{
						TE_TFParticleEffect( filter, 0.0, GetImpactEffect(), vecOrigin, vec3_angle );
					}
				}

				// Sounds
				// EmitSound( "Player.PlasmaDamage" );

				// Treat this trace exactly like radius damage
				CTraceFilterIgnorePlayers traceFilter( pThrower, COLLISION_GROUP_PROJECTILE );

				// Burn players in impact range
				CBaseEntity *pListOfEntities[32];
				int iEntities = UTIL_EntitiesInSphere( pListOfEntities, 32, vecOrigin, GetDamageRadius(), FL_CLIENT | FL_FAKECLIENT | FL_NPC );
				for ( int i = 0; i < iEntities; ++i )
				{
					if ( pThrower->InSameTeam( pListOfEntities[i] ) )
						continue;

					CBaseCombatCharacter *pBaseCombatCharacter = NULL;
					CTFPlayer *pPlayer = ToTFPlayer( pListOfEntities[i] );
					if ( !pPlayer )
					{
						pBaseCombatCharacter = dynamic_cast< CBaseCombatCharacter* >( pListOfEntities[i] );
					}
					else
					{
						pBaseCombatCharacter = pPlayer;
					}

					if ( !pBaseCombatCharacter || !pBaseCombatCharacter->IsAlive() )
						continue;

					// Do a quick trace to see if there's any geometry in the way.
					trace_t pImpactTrace;
					UTIL_TraceLine( GetAbsOrigin(), pBaseCombatCharacter->GetAbsOrigin(), ( MASK_SHOT & ~( CONTENTS_HITBOX ) ), &traceFilter, &pImpactTrace );
					if ( pImpactTrace.DidHitWorld() )
						continue;

					// Effects on the individual players
					ExplodeEffectOnTarget( pThrower, pPlayer, pBaseCombatCharacter );
				}

				ApplyBlastDamage( pThrower, vecOrigin );
			}
		}

		AddEffects( EF_NODRAW );
		SetAbsVelocity( vec3_origin );
		SetMoveType( MOVETYPE_NONE );
	}

	m_nHitCount++;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFProjectile_Napalm::PipebombTouch( CBaseEntity *pOther )
{
	if ( gpGlobals->curtime - m_flLastBurnTime < 0.15f )
		return;

	if ( InSameTeam( pOther ) )
		return;

	if ( !m_hFlameThrower || !m_hFlameThrower->GetOwnerEntity() )
		return;

	CTakeDamageInfo info;
	info.SetAttacker( m_hFlameThrower->GetOwnerEntity() );
	info.SetInflictor( m_hFlameThrower ); 
	info.SetWeapon( m_hFlameThrower );
	info.SetDamage( 2.f );
	info.SetDamageCustom( GetCustomDamageType() );
	info.SetDamagePosition( GetAbsOrigin() );
	info.SetDamageType( DMG_BURN );
	pOther->TakeDamage( info );

	m_flLastBurnTime = gpGlobals->curtime;

	BaseClass::PipebombTouch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: Radius damage
//-----------------------------------------------------------------------------
void CTFProjectile_Napalm::ApplyBlastDamage( CTFPlayer *pThrower, Vector vecOrigin )
{
	CTakeDamageInfo info;
	info.SetAttacker( pThrower );
	info.SetInflictor( this ); 
	info.SetWeapon( m_hFlameThrower );
	info.SetDamage( 25.f );
	info.SetDamageCustom( GetCustomDamageType() );
	info.SetDamagePosition( vecOrigin );
	info.SetDamageType( DMG_BURN );

	CTFRadiusDamageInfo radiusinfo( &info, vecOrigin, 100.f, pThrower );
	TFGameRules()->RadiusDamage( radiusinfo );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFProjectile_Napalm::InitialExplodeEffects( CTFPlayer *pThrower, const trace_t *pTrace )
{
	// Added Particle
	Vector vecOrigin = GetAbsOrigin();
	// Particle
	CPVSFilter filter( vecOrigin );
	TE_TFExplosion( filter, 0.0f, vecOrigin, pTrace->plane.normal, TF_WEAPON_FLAMETHROWER, kInvalidEHandleExplosion, -1, SPECIAL1, INVALID_STRING_INDEX );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Direct hit
//-----------------------------------------------------------------------------
void CTFProjectile_Napalm::ExplodeEffectOnTarget( CTFPlayer *pThrower, CTFPlayer *pTarget, CBaseCombatCharacter *pBaseTarget )
{
	if ( pBaseTarget->GetTeamNumber() == GetTeamNumber() )
		return;

	if ( pTarget )
	{
		if ( pTarget->m_Shared.IsInvulnerable() )
			return;

		if ( pTarget->m_Shared.InCond( TF_COND_PHASE ) || pTarget->m_Shared.InCond( TF_COND_PASSTIME_INTERCEPTION ) )
			return;
	}

	Vector vecDir = pBaseTarget->WorldSpaceCenter() - GetAbsOrigin();
	VectorNormalize( vecDir );

	const trace_t *pTrace = &CBaseEntity::GetTouchTrace();
	trace_t *pNewTrace = const_cast<trace_t*>( pTrace );

	CBaseEntity *pInflictor = GetLauncher();
	CTakeDamageInfo info;
	info.SetAttacker( pThrower );
	info.SetInflictor( this ); 
	info.SetWeapon( pInflictor );
	info.SetDamage( 50 );
	info.SetDamageCustom( GetCustomDamageType() );
	info.SetDamagePosition( GetAbsOrigin() );
	info.SetDamageType( DMG_IGNITE );

	// Hurt 'em.
	Vector dir;
	AngleVectors( GetAbsAngles(), &dir );
	pBaseTarget->DispatchTraceAttack( info, dir, pNewTrace );
	ApplyMultiDamage();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CTFProjectile_Napalm::GetImpactEffect( void )
{ 
	return "burninggibs";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFProjectile_Napalm::SetCustomPipebombModel( void )
{ 
	SetModel( "models/weapons/w_models/w_flaregun_shell.mdl" );
}
#endif // GAME_DLL

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFProjectile_Napalm::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CTFProjectile_Napalm::GetTrailParticleName( void )
{
	if ( GetTeamNumber() == TF_TEAM_BLUE )
	{
		return "flaming_arrow";
	}
	else
	{
		return "flaming_arrow";
	}
}
#endif // CLIENT_DLL
#endif // STAGING_ONLY

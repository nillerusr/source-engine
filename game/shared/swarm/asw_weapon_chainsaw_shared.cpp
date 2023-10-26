#include "cbase.h"
#include "npcevent.h"
#include "Sprite.h"
#include "beam_shared.h"
#include "takedamageinfo.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "soundenvelope.h"

#ifdef CLIENT_DLL
#include "c_baseplayer.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "ivieweffects.h"
#include "c_te_effect_dispatch.h"
#include "prediction.h"
#include "fx.h"
#define CASW_Player C_ASW_Player
#define CASW_Marine C_ASW_Marine
#else
#include "asw_lag_compensation.h"
#include "player.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "soundent.h"
#include "game.h"
#include "te_effect_dispatch.h"
#include "asw_marine_speech.h"
#include "asw_weapon_ammo_bag_shared.h"
#endif

#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "IEffects.h"

#include "asw_marine_skills.h"
#include "shake.h"
#include "asw_util_shared.h"
#include "asw_weapon_chainsaw_shared.h"
#include "asw_weapon_parse.h"

#define ASW_CHAINSAW_CHARGE_UP_TIME  1.0
#define ASW_CHAINSAW_PULSE_INTERVAL			0.1
#define ASW_CHAINSAW_DISCHARGE_INTERVAL		0.1
#define ASW_CHAINSAW_RANGE 30

#define ASW_CHAINSAW_BEAM_SPRITE		"swarm/sprites/mlaserbeam.vmt"
#define ASW_CHAINSAW_FLARE_SPRITE		"swarm/sprites/mlaserspark.vmt"

extern ConVar sk_plr_dmg_asw_cs;
extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Chainsaw, DT_ASW_Weapon_Chainsaw );

BEGIN_NETWORK_TABLE( CASW_Weapon_Chainsaw, DT_ASW_Weapon_Chainsaw )
END_NETWORK_TABLE()    

#if defined( CLIENT_DLL )
BEGIN_PREDICTION_DATA( CASW_Weapon_Chainsaw )
	DEFINE_FIELD( m_fireState, FIELD_INTEGER ),
	DEFINE_FIELD( m_flAmmoUseTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flShakeTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flStartFireTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDmgTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flFireAnimTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flLastHitTime, FIELD_FLOAT ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( asw_weapon_chainsaw, CASW_Weapon_Chainsaw );
PRECACHE_WEAPON_REGISTER( asw_weapon_chainsaw );

#ifndef CLIENT_DLL
BEGIN_DATADESC( CASW_Weapon_Chainsaw )
	DEFINE_FIELD( m_fireState, FIELD_INTEGER ),
	DEFINE_FIELD( m_flAmmoUseTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flShakeTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flStartFireTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDmgTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flFireAnimTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_hBeam, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hNoise, FIELD_EHANDLE ),
END_DATADESC()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CASW_Weapon_Chainsaw::CASW_Weapon_Chainsaw( void )
{
	m_flFireAnimTime = 0.0f;
	m_flTargetChainsawPitch = 0.0f;
	m_flDmgTime = 0.0f;
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= true;
	m_bPlayedIdleSound = false;
	m_pChainsawAttackSound = NULL;
	m_pChainsawAttackOffSound = NULL;
#ifdef GAME_DLL
	m_fLastForcedFireTime = 0;	// last time we forced an AI marine to push its fire button
#endif
}

CASW_Weapon_Chainsaw::~CASW_Weapon_Chainsaw()
{
	if (m_fireState != FIRE_OFF)
	{
		EndAttack();
	}
	else
	{
		if (m_bPlayedIdleSound)
		{
			StopSound( "ASW_Chainsaw.Start" );
			if ( CBaseEntity::IsAbsQueriesValid() )
				EmitSound( "ASW_Chainsaw.Stop" );
		}
	}
	StopChainsawSound();
	StopAttackOffSound();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Weapon_Chainsaw::Precache( void )
{
    PrecacheScriptSound( "ASW_Chainsaw.Start" );
	PrecacheScriptSound( "ASW_Chainsaw.Stop" );
	PrecacheScriptSound( "ASW_Chainsaw.attackOn" );
	PrecacheScriptSound( "ASW_Chainsaw.attackOff" );

	PrecacheModel( ASW_CHAINSAW_BEAM_SPRITE );
	PrecacheModel( ASW_CHAINSAW_FLARE_SPRITE );
	PrecacheParticleSystem( "mining_laser_exhaust" );

	BaseClass::Precache();
}

bool CASW_Weapon_Chainsaw::HasAmmo()
{
	return true;
	//return m_iClip1 > 0;
}

bool CASW_Weapon_Chainsaw::Deploy( void )
{
	SetFiringState(FIRE_OFF);

	m_bPlayedIdleSound = true;
	EmitSound( "ASW_Chainsaw.Start" );
	SetWeaponIdleTime( gpGlobals->curtime + 0.7f );

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Weapon_Chainsaw::PrimaryAttack( void )
{
	// If my clip is empty (and I use clips) start reload
	//if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	//{
		//Reload();
		//return;
	//}

	CASW_Player *pPlayer = GetCommander();
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine || !pMarine->IsAlive())
	{
		EndAttack();
		return;
	}

	// don't fire underwater
	if ( pMarine->GetWaterLevel() == 3 )
	{
		if ( m_fireState != FIRE_OFF || m_hBeam )
		{
			EndAttack();
		}
		else
		{
			WeaponSound( EMPTY );
		}

		m_flNextPrimaryAttack = gpGlobals->curtime + GetWeaponInfo()->m_flFireRate;
		m_flNextSecondaryAttack = gpGlobals->curtime + GetWeaponInfo()->m_flFireRate;
		return;
	}

#ifdef GAME_DLL	// check for turning on lag compensation
	if (pPlayer && pMarine->IsInhabited())
	{
		CASW_Lag_Compensation::RequestLagCompensation( pPlayer, pPlayer->GetCurrentUserCommand() );
	}
#endif

	Vector vecSrc		= pMarine->Weapon_ShootPosition( );
	Vector	vecAiming = vec3_origin;
	if ( pPlayer && pMarine->IsInhabited() )
	{
		vecAiming = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
	}
	else
	{
#ifndef CLIENT_DLL
		vecAiming = pMarine->GetActualShootTrajectory( vecSrc );
#endif
	}

	// make vecaiming go down at 45 degrees
	vecAiming.z = 0;
	VectorNormalize(vecAiming);
	vecAiming.z = -1.0f;
	VectorNormalize(vecAiming);

	switch( m_fireState )
	{
		case FIRE_OFF:
		{
			//if ( !HasAmmo() )
			//{
				//m_flNextPrimaryAttack = gpGlobals->curtime + 0.25;
				//m_flNextSecondaryAttack = gpGlobals->curtime + 0.25;
				//WeaponSound( EMPTY );
				//return;
			//}

			m_flAmmoUseTime = gpGlobals->curtime;// start using ammo ASAP.

			SendWeaponAnim( ACT_VM_PRIMARYATTACK );
						
			m_flShakeTime = 0;
			m_flStartFireTime = gpGlobals->curtime;

			SetWeaponIdleTime( gpGlobals->curtime + 0.1 );

			m_flDmgTime = gpGlobals->curtime + ASW_CHAINSAW_PULSE_INTERVAL;			
			SetFiringState(FIRE_STARTUP);
		}
		break;

		case FIRE_STARTUP:
		{
			Fire( vecSrc, vecAiming );
#ifndef CLIENT_DLL
			pMarine->OnWeaponFired( this, 1 );
#endif
		
			if ( gpGlobals->curtime >= ( m_flStartFireTime + ASW_CHAINSAW_CHARGE_UP_TIME ) )
			{
                //EmitSound( "ASW_Chainsaw.AttackLoop" );
				
				SetFiringState(FIRE_CHARGE);
			}

			if ( !HasAmmo() )
			{
				EndAttack();
				m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
				m_flNextSecondaryAttack = gpGlobals->curtime + 1.0;
			}
		}
		break;

		case FIRE_CHARGE:
		{
			Fire( vecSrc, vecAiming );
#ifndef CLIENT_DLL
			pMarine->OnWeaponFired( this, 1 );
#endif
		
			if ( !HasAmmo() )
			{
				EndAttack();
				m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
				m_flNextSecondaryAttack = gpGlobals->curtime + 1.0;
			}
		}
		break;
	}
}

void CASW_Weapon_Chainsaw::Fire( const Vector &vecOrigSrc, const Vector &vecDir )
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
	{
		return;
	}

	StopAttackOffSound();
	StartChainsawSound();

	if ( m_flFireAnimTime < gpGlobals->curtime )
	{
		pMarine->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );

		m_flFireAnimTime = gpGlobals->curtime + 0.1f;
	}

	Vector vecDest = vecOrigSrc + (vecDir * ASW_CHAINSAW_RANGE);

	bool bDamageTime = m_flDmgTime < gpGlobals->curtime;
	bool bHit = false;

	Ray_t ray;
	ray.Init( vecOrigSrc, vecDest, Vector( -5, -5, -5 ), Vector( 5, 5, 25 ) );

	CBaseEntity *(pEntities[ 8 ]);

	CHurtableEntitiesEnum hurtableEntities( pEntities, 8 );
	partition->EnumerateElementsAlongRay( PARTITION_ENGINE_NON_STATIC_EDICTS | PARTITION_ENGINE_SOLID_EDICTS, ray, false, &hurtableEntities );

	trace_t	tr;

	for ( int i = 0; i < hurtableEntities.GetCount(); ++i )
	{
		CBaseEntity *pEntity = pEntities[ i ];
		if ( pEntity == NULL || pEntity == pMarine )
			continue;

		bHit = true;

		if ( bDamageTime )
		{
			// wide mode does damage to the ent, and radius damage
			if ( pEntity->m_takedamage != DAMAGE_NO )
			{
				CTraceFilterOnlyHitThis filter( pEntity );
				UTIL_TraceHull( vecOrigSrc, vecDest, Vector( -5, -5, -2 ), Vector( 5, 5, 25 ), MASK_SHOT, &filter, &tr );

				ClearMultiDamage();
				float fDamage = 0.5f * GetWeaponInfo()->m_flBaseDamage + MarineSkills()->GetSkillBasedValueByMarine( pMarine, ASW_MARINE_SKILL_MELEE, ASW_MARINE_SUBSKILL_MELEE_DMG );
				CTakeDamageInfo info( this, pMarine, fDamage * g_pGameRules->GetDamageMultiplier(), DMG_SLASH );
				info.SetWeapon( this );
				CalculateMeleeDamageForce( &info, vecDir, tr.endpos );
				pEntity->DispatchTraceAttack( info, vecDir, &tr );
				ApplyMultiDamage();
			}

			// radius damage a little more potent in multiplayer.
#ifndef CLIENT_DLL
			//RadiusDamage( CTakeDamageInfo( this, pMarine, sk_plr_dmg_asw_ml.GetFloat() * g_pGameRules->GetDamageMultiplier() / 4, DMG_ENERGYBEAM | DMG_BLAST | DMG_ALWAYSGIB ), tr.endpos, 128, CLASS_NONE, NULL );
#endif		
		}
	}

	if ( bHit )
	{
		if ( bDamageTime )
		{
			m_bIsFiring = true;

			m_flLastHitTime = gpGlobals->curtime;
			m_flTargetChainsawPitch = 0.0f;

#ifndef CLIENT_DLL
			pMarine->OnWeaponFired( this, 1 );
#endif

			if ( !pMarine->IsAlive() )
				return;

			// uses 5 ammo/second
			if ( gpGlobals->curtime >= m_flAmmoUseTime )
			{
				// chainsaw no longer uses ammo

				m_flAmmoUseTime = gpGlobals->curtime + 0.2;

				/*
				// decrement ammo
				//m_iClip1 -= 1;	

				#ifdef GAME_DLL
				CASW_Marine *pMarine = GetMarine();
				if (pMarine && m_iClip1 <= 0 && pMarine->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
				{
				// check he doesn't have ammo in an ammo bay
				CASW_Weapon_Ammo_Bag* pAmmoBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pMarine->GetASWWeapon(0));
				if (!pAmmoBag)
				pAmmoBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pMarine->GetASWWeapon(1));
				if (!pAmmoBag || !pAmmoBag->CanGiveAmmoToWeapon(this))
				pMarine->GetMarineSpeech()->Chatter(CHATTER_NO_AMMO);
				}
				#endif
				*/
			}

			m_flDmgTime = gpGlobals->curtime + ASW_CHAINSAW_DISCHARGE_INTERVAL;
			if ( m_flShakeTime < gpGlobals->curtime )
			{
				CASW_Player *pPlayer = pMarine->GetCommander();
				if (pPlayer && pMarine->IsInhabited())
				{
					// #ifndef CLIENT_DLL
					// 				if (gpGlobals->maxClients == 1)	// only shake the screen from the server in singleplayer (in multi it'll be predicted)
					// 				{
					// 					ASW_TransmitShakeEvent( pPlayer, 5.0f, 100.0f, 1.75f, SHAKE_START );
					// 				}				
					// #else
					// 				ASW_TransmitShakeEvent( pPlayer, 5.0f, 100.0f, 1.75f, SHAKE_START );
					// #endif
				}

				m_flShakeTime = gpGlobals->curtime + 0.5;
			}
		}

		Vector vecUp, vecRight;
		QAngle angDir;

		VectorAngles( vecDir, angDir );
		AngleVectors( angDir, NULL, &vecRight, &vecUp );

		Vector tmpSrc = vecOrigSrc + (vecUp * -8) + (vecRight * 3);
		//UTIL_ImpactTrace( &tr, DMG_SLASH );
		UpdateEffect( tmpSrc, tr.endpos );
	}
}

void CASW_Weapon_Chainsaw::UpdateEffect( const Vector &startPoint, const Vector &endPoint )
{
	//if ( !m_hBeam )
	//{
		//CreateEffect();
	//}

	//if ( m_hBeam )
	//{
		//m_hBeam->SetStartPos( endPoint );
	//}

	CASW_Marine* pOwner = GetMarine();
	
	if (!pOwner)
		return;

	// make sparks come out at end point
	Vector vecNormal = startPoint - endPoint;
	vecNormal.NormalizeInPlace();
#ifndef CLIENT_DLL
	CEffectData data;
	data.m_vOrigin = endPoint;	
	data.m_vNormal = vecNormal;
	CPASFilter filter( data.m_vOrigin );
	filter.SetIgnorePredictionCull(true);
	
	if (gpGlobals->maxClients > 1 && pOwner->IsInhabited() && pOwner->GetCommander())
	{
		// multiplayer game, where this marine is currently being controlled by a client, who will spawn his own effect
		// so just make the beam effect for all other clients		
		filter.RemoveRecipient(pOwner->GetCommander());
	}
	
	DispatchEffect( filter, 0.0, "ASWWelderSparks", data );
#else
	FX_Sparks( endPoint, 1, 2, vecNormal, 5, 32, 160 );
#endif

	if ( m_hNoise )
	{
		m_hNoise->SetStartPos( endPoint );
	}
}

void CASW_Weapon_Chainsaw::CreateEffect( void )
{
#ifndef CLIENT_DLL    
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
	{
		return;
	}

	DestroyEffect();

	m_hBeam = CBeam::BeamCreate( ASW_CHAINSAW_BEAM_SPRITE, 1.75 );
	m_hBeam->PointEntInit( GetAbsOrigin(), this );
	m_hBeam->SetBeamFlags( FBEAM_SINENOISE );
	m_hBeam->SetEndAttachment( 1 );
	m_hBeam->AddSpawnFlags( SF_BEAM_TEMPORARY );	// Flag these to be destroyed on save/restore or level transition
	m_hBeam->SetOwnerEntity( pMarine );
	m_hBeam->SetScrollRate( 10 );
	m_hBeam->SetBrightness( 200 );
	m_hBeam->SetColor( 255, 255, 255 );
	m_hBeam->SetNoise( 0.025 );

	m_hNoise = CBeam::BeamCreate( ASW_CHAINSAW_BEAM_SPRITE, 2.5 );
	m_hNoise->PointEntInit( GetAbsOrigin(), this );
	m_hNoise->SetEndAttachment( 1 );
	m_hNoise->AddSpawnFlags( SF_BEAM_TEMPORARY );
	m_hNoise->SetOwnerEntity( pMarine );
	m_hNoise->SetScrollRate( 25 );
	m_hNoise->SetBrightness( 200 );
	m_hNoise->SetColor( 255, 255, 255 );
	m_hNoise->SetNoise( 0.8 );
#endif    
}


void CASW_Weapon_Chainsaw::DestroyEffect( void )
{
#ifndef CLIENT_DLL    
	if ( m_hBeam )
	{
		UTIL_Remove( m_hBeam );
		m_hBeam = NULL;
	}
	if ( m_hNoise )
	{
		UTIL_Remove( m_hNoise );
		m_hNoise = NULL;
	}
#endif
}

void CASW_Weapon_Chainsaw::EndAttack( void )
{
	
	
	if ( m_fireState != FIRE_OFF )
	{
		StartAttackOffSound();

#ifdef CLIENT_DLL
		DispatchParticleEffect( "mining_laser_exhaust", PATTACH_POINT_FOLLOW, this, "eject1" );
#endif
	}

	StopChainsawSound();
	
	SetWeaponIdleTime( gpGlobals->curtime + 2.0 );
	m_flNextPrimaryAttack = gpGlobals->curtime + GetWeaponInfo()->m_flFireRate;
	m_flNextSecondaryAttack = gpGlobals->curtime + GetWeaponInfo()->m_flFireRate;

	SetFiringState(FIRE_OFF);

	ClearIsFiring();

	DestroyEffect();
}

void CASW_Weapon_Chainsaw::Drop( const Vector &vecVelocity )
{
	if (m_fireState != FIRE_OFF)
	{
		EndAttack();
	}

	StopChainsawSound( true );
	StopAttackOffSound();
	StopSound( "ASW_Chainsaw.Start" );
	EmitSound( "ASW_Chainsaw.Stop" );

	BaseClass::Drop( vecVelocity );
}

bool CASW_Weapon_Chainsaw::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if (m_fireState != FIRE_OFF)
	{
		EndAttack();
	}

	StopChainsawSound( true );
	StopAttackOffSound();
	StopSound( "ASW_Chainsaw.Start" );
	EmitSound( "ASW_Chainsaw.Stop" );

	return BaseClass::Holster( pSwitchingTo );
}

void CASW_Weapon_Chainsaw::WeaponIdle( void )
{
	if ( !HasWeaponIdleTimeElapsed() )
		return;
	
	//Msg("%f CASW_Weapon_Chainsaw::WeaponIdle\n", gpGlobals->curtime);

	float flIdleTime;
	if ( m_fireState != FIRE_OFF )
	{
		//Msg("  ending attack\n", gpGlobals->curtime);
		EndAttack();
		flIdleTime = gpGlobals->curtime + 1.4f;
	}
	else
	{
		//Msg("  idle looping\n", gpGlobals->curtime);
		//EmitSound( "ASW_Chainsaw.IdleLoop" );
		flIdleTime = gpGlobals->curtime + 1.7f;
	}
	
	//int iAnim;

	//float flRand = random->RandomFloat( 0,1 );
	
	//if ( flRand <= 0.5 )
	//{
		//iAnim = ACT_VM_IDLE;
		//flIdleTime = gpGlobals->curtime + 1.4f;
	//}
	//else 
	//{
		//iAnim = ACT_VM_FIDGET;
		//flIdleTime = gpGlobals->curtime + 3.0;
	//}

	//SendWeaponAnim( iAnim );

	SetWeaponIdleTime( flIdleTime );
}

void CASW_Weapon_Chainsaw::SetFiringState(CHAINSAW_FIRE_STATE state)
{
#ifdef CLIENT_DLL
	//Msg("[C] SetFiringState %d\n", state);
#else
	//Msg("[C] SetFiringState %d\n", state);
#endif
	m_fireState = state;
}

bool CASW_Weapon_Chainsaw::ShouldMarineMoveSlow()
{
	return m_fireState != FIRE_OFF;
}

#ifdef GAME_DLL
void CASW_Weapon_Chainsaw::GetButtons(bool& bAttack1, bool& bAttack2, bool& bReload, bool& bOldReload, bool& bOldAttack1 )
{
	CASW_Marine *pMarine = GetMarine();

	// make AI fire this weapon whenever they have an enemy within a certain range
	if (pMarine && !pMarine->IsInhabited())
	{
		bool bHasEnemy = (pMarine->GetEnemy() && pMarine->GetEnemy()->GetAbsOrigin().DistTo(pMarine->GetAbsOrigin()) < 250.0f);

		if (bHasEnemy)
			m_fLastForcedFireTime = gpGlobals->curtime;

		bAttack1 = (bHasEnemy || gpGlobals->curtime < m_fLastForcedFireTime + 1.0f);	// fire for 2 seconds after killing our enemy
		bAttack2 = false;
		bReload = false;
		bOldReload = false;


		return;
	}

	BaseClass::GetButtons( bAttack1, bAttack2, bReload, bOldReload, bOldAttack1 );
}
#endif

void CASW_Weapon_Chainsaw::ItemPostFrame()
{
	BaseClass::ItemPostFrame();

	AdjustChainsawPitch();
	if ( m_fireState == FIRE_OFF )
	{
		StopChainsawSound();
	}
}

ConVar asw_chainsaw_pitch_bite_rate( "asw_chainsaw_pitch_bite_rate", "50.0", FCVAR_CHEAT | FCVAR_REPLICATED, "How quickly the chainsaw pitch changes when attack hits somethings" );
ConVar asw_chainsaw_pitch_return_rate( "asw_chainsaw_pitch_return_rate", "50.0", FCVAR_CHEAT | FCVAR_REPLICATED, "How quickly the pitch returns to normal when attacking chainsaw doesn't hit anything" );
ConVar asw_chainsaw_pitch_return_delay( "asw_chainsaw_pitch_return_delay", "1.0", FCVAR_CHEAT | FCVAR_REPLICATED, "How long after attacking something before the chainsaw pitch returns to normal" );
ConVar asw_chainsaw_pitch_target( "asw_chainsaw_pitch_target", "95.0", FCVAR_CHEAT | FCVAR_REPLICATED, "Target pitch when chainsaw starts to attack something" );
ConVar asw_chainsaw_pitch_range( "asw_chainsaw_pitch_range", "5.0", FCVAR_CHEAT | FCVAR_REPLICATED, "Random variation applied above and below target pitch" );
ConVar asw_chainsaw_attack_fade_time( "asw_chainsaw_attack_fade_time", "0.5", FCVAR_CHEAT | FCVAR_REPLICATED, "Time for chainsaw attack sound to fade out" );

ConVar asw_chainsaw_shake_amplitude( "asw_chainsaw_shake_amplitude", "12.5", FCVAR_CHEAT | FCVAR_REPLICATED );
ConVar asw_chainsaw_shake_frequency( "asw_chainsaw_shake_frequency", "100.0", FCVAR_CHEAT | FCVAR_REPLICATED );
ConVar asw_chainsaw_debug( "asw_chainsaw_debug", "0", FCVAR_CHEAT | FCVAR_REPLICATED );

#ifdef CLIENT_DLL
ConVar asw_chainsaw_shake_duration( "asw_chainsaw_shake_duration", "1.0", FCVAR_CHEAT );
#endif

void CASW_Weapon_Chainsaw::AdjustChainsawPitch()
{
	if ( !m_pChainsawAttackSound )
	{
		if ( asw_chainsaw_debug.GetBool() )
		{
			Msg( "No chainsaw attack sound, aborting\n" );
		}
		return;
	}

	// Translated into the pitch values as represented in the soundscripts that would mean roughly that you
	// change the pitch value to randomly land between 85 and 90. (Pitch value 100 equals no change. Pitch value 50 is one octave lower.)
	// The pitch down should happen over 100 milliseconds. 400 ms later (half as second after the impact triggered the pitch down) 
	// reset to the original pitch value over 250 ms. 
	
	float flTransitionRate = 50.0f;

	// check for returning to normal pitch
	if ( ( gpGlobals->curtime - m_flLastHitTime ) > asw_chainsaw_pitch_return_delay.GetFloat() )
	{
		m_flTargetChainsawPitch = 100.0f;	
		flTransitionRate = asw_chainsaw_pitch_return_rate.GetFloat();
	}
	else if ( m_flTargetChainsawPitch == 0.0f )	// just started attacking something
	{
		float flPitchMin = asw_chainsaw_pitch_target.GetFloat() - asw_chainsaw_pitch_range.GetFloat();
		float flPitchMax = asw_chainsaw_pitch_target.GetFloat() + asw_chainsaw_pitch_range.GetFloat();
		if ( asw_chainsaw_debug.GetBool() )
		{
			Msg( "pitch min/max( %f %f )\n", flPitchMin, flPitchMax );
		}
		m_flTargetChainsawPitch = RandomFloat( flPitchMin, flPitchMax );
		flTransitionRate = asw_chainsaw_pitch_bite_rate.GetFloat();

#ifdef CLIENT_DLL
		if ( !( prediction && prediction->InPrediction() && !prediction->IsFirstTimePredicted() ) )
		{
			// move this somewhere else....
			ScreenShake_t shake;
			shake.command = SHAKE_STOP;
			shake.amplitude = 0;

			HACK_GETLOCALPLAYER_GUARD( "ASW_AdjustChainsawPitch" );
			GetViewEffects()->Shake( shake );

			shake.command	= SHAKE_START;
			shake.amplitude = asw_chainsaw_shake_amplitude.GetFloat();
			shake.frequency = asw_chainsaw_shake_frequency.GetFloat();
			shake.duration	= asw_chainsaw_shake_duration.GetFloat();

			GetViewEffects()->Shake( shake );
		}
#endif
	}

	float flCurPitch = CSoundEnvelopeController::GetController().SoundGetPitch( m_pChainsawAttackSound );
	if ( flCurPitch != m_flTargetChainsawPitch )
	{
		if ( asw_chainsaw_debug.GetBool() )
		{
			Msg( "Changing chainsaw pitch to %f transition rate %f\n", m_flTargetChainsawPitch, flTransitionRate );
		}

		flCurPitch = Approach( m_flTargetChainsawPitch, flCurPitch, flTransitionRate * gpGlobals->curtime );
		CSoundEnvelopeController::GetController().SoundChangePitch( m_pChainsawAttackSound, flCurPitch, 0.0f );
	}
}

void CASW_Weapon_Chainsaw::StartChainsawSound()
{
	if ( !m_pChainsawAttackSound )
	{
		if ( asw_chainsaw_debug.GetBool() )
		{
			DevMsg( "Start Chainsaw Attach Sound\n" );
		}

		StopSound( "ASW_Chainsaw.Start" );

		CPASAttenuationFilter filter( this );
		m_pChainsawAttackSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "ASW_Chainsaw.attackOn" );
		CSoundEnvelopeController::GetController().Play( m_pChainsawAttackSound, 1.0, 100 );
	}
	else
	{
		float fVolume = CSoundEnvelopeController::GetController().SoundGetVolume( m_pChainsawAttackSound );
		if ( fVolume < 1.0f )
		{
			CSoundEnvelopeController::GetController().SoundChangeVolume( m_pChainsawAttackSound, Approach( 1.0f, fVolume, ( 1.0f / asw_chainsaw_attack_fade_time.GetFloat() ) * gpGlobals->frametime ), 0.0f );
		}
	}
}

void CASW_Weapon_Chainsaw::StopChainsawSound( bool bForce /*= false*/ )
{
	if ( m_pChainsawAttackSound )
	{
		if ( asw_chainsaw_debug.GetBool() )
		{
			DevMsg( "Stop Chainsaw Attach Sound\n" );
		}

		float fVolume = bForce ? 0.0f : CSoundEnvelopeController::GetController().SoundGetVolume( m_pChainsawAttackSound );
		if ( fVolume == 0.0f )
		{
			CSoundEnvelopeController::GetController().SoundDestroy( m_pChainsawAttackSound ); 
			m_pChainsawAttackSound = NULL;
		}
		else
		{
			CSoundEnvelopeController::GetController().SoundChangeVolume( m_pChainsawAttackSound, Approach( 0.0f, fVolume, ( 1.0f / asw_chainsaw_attack_fade_time.GetFloat() ) * gpGlobals->frametime ), 0.0f );
		}
	}
}

void CASW_Weapon_Chainsaw::StartAttackOffSound()
{
	float flCurPitch = 100;
	
	if ( m_pChainsawAttackSound )
	{
		if ( asw_chainsaw_debug.GetBool() )
		{
			DevMsg( "Start Chainsaw Off Sound\n" );
		}

		flCurPitch = CSoundEnvelopeController::GetController().SoundGetPitch( m_pChainsawAttackSound );
	}

	if ( !m_pChainsawAttackOffSound )
	{
		CPASAttenuationFilter filter( this );
		m_pChainsawAttackOffSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "ASW_Chainsaw.attackOff" );
		CSoundEnvelopeController::GetController().Play( m_pChainsawAttackOffSound, 0.0, flCurPitch );
	}

	CSoundEnvelopeController::GetController().SoundChangeVolume( m_pChainsawAttackOffSound, 1.0, asw_chainsaw_attack_fade_time.GetFloat() );
	CSoundEnvelopeController::GetController().SoundChangePitch( m_pChainsawAttackOffSound, 100, 1.0f / asw_chainsaw_pitch_return_rate.GetFloat() );
}

void CASW_Weapon_Chainsaw::StopAttackOffSound()
{
	if ( m_pChainsawAttackOffSound )
	{
		if ( asw_chainsaw_debug.GetBool() )
		{
			DevMsg( "Stop Chainsaw Off Sound\n" );
		}

		CSoundEnvelopeController::GetController().SoundDestroy( m_pChainsawAttackOffSound );
		m_pChainsawAttackOffSound = NULL;
	}
}

#define ASW_CHAINSAW_SYNC_KILL_RANGE 60

bool CASW_Weapon_Chainsaw::CheckSyncKill( byte &forced_action, short &sync_kill_ent )
{
	// sync kills disabled
	return false;
	/*
	// If not firing, don't do anything
	if ( !IsFiring() ) //m_fireState <= FIRE_OFF
		return false;

	// check for enemies in front of the player
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return false;

	if ( pMarine->GetCurrentMeleeAttack() )
		return false;

	Vector vecSrc = pMarine->Weapon_ShootPosition();
	Vector vecDir;
	AngleVectors( pMarine->EyeAngles(), &vecDir );
	Vector vecDest	= vecSrc + ( vecDir * ASW_CHAINSAW_SYNC_KILL_RANGE );

	trace_t	tr;
	UTIL_TraceHull( vecSrc, vecDest, Vector( -5, -5, -5 ), Vector( 5, 5, 5 ), MASK_SHOT, pMarine, COLLISION_GROUP_NONE, &tr );

	if ( tr.fraction < 1.0f && tr.m_pEnt && tr.m_pEnt->Classify() == CLASS_ASW_DRONE )
	{
		forced_action = FORCED_ACTION_CHAINSAW_SYNC_KILL;
		sync_kill_ent = tr.m_pEnt->entindex();
		return true;
	}

	return false;
	*/
}
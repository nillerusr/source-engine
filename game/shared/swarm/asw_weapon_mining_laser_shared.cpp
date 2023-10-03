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
#include "c_te_effect_dispatch.h"
#include "fx.h"
#define CASW_Player C_ASW_Player
#define CASW_Marine C_ASW_Marine
#else
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

#include "asw_weapon_mining_laser_shared.h"
#include "asw_weapon_parse.h"

#define ASW_MINING_LASER_CHARGE_UP_TIME  1.6
#define ASW_MINING_LASER_PULSE_INTERVAL			0.1
#define ASW_MINING_LASER_DISCHARGE_INTERVAL		0.1


ConVar asw_mininglaser_damage_snd_interval("asw_mininglaser_damage_snd_interval", "1.0", FCVAR_CHEAT | FCVAR_REPLICATED, "How often to play the damage sound when the laser beam is on");
extern ConVar sk_plr_dmg_asw_ml;
extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Mining_Laser, DT_ASW_Weapon_Mining_Laser );

BEGIN_NETWORK_TABLE( CASW_Weapon_Mining_Laser, DT_ASW_Weapon_Mining_Laser )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_fireState ) ),
	RecvPropBool( RECVINFO( m_bCutting ) ),
	RecvPropFloat( RECVINFO( m_flStartFireTime ) )
#else
	SendPropInt( SENDINFO( m_fireState ) ),
	SendPropBool( SENDINFO( m_bCutting ) ),
	SendPropFloat( SENDINFO( m_flStartFireTime ) )
#endif
END_NETWORK_TABLE()    

LINK_ENTITY_TO_CLASS( asw_weapon_mining_laser, CASW_Weapon_Mining_Laser );
PRECACHE_WEAPON_REGISTER( asw_weapon_mining_laser );

#if defined( CLIENT_DLL )
BEGIN_PREDICTION_DATA( CASW_Weapon_Mining_Laser )
	DEFINE_FIELD( m_fireState, FIELD_INTEGER ),
	DEFINE_FIELD( m_flAmmoUseTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flShakeTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flStartFireTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDmgTime, FIELD_FLOAT ),
	//DEFINE_FIELD( m_hSprite, FIELD_EHANDLE ),
	//DEFINE_FIELD( m_hBeam, FIELD_EHANDLE ),
	//DEFINE_FIELD( m_hNoise, FIELD_EHANDLE ),
END_PREDICTION_DATA()
#endif

    /*
BEGIN_DATADESC( CASW_Weapon_Mining_Laser )
	DEFINE_FIELD( m_fireState, FIELD_INTEGER ),
	DEFINE_FIELD( m_flAmmoUseTime, FIELD_TIME ),
	DEFINE_FIELD( m_flShakeTime, FIELD_TIME ),
	DEFINE_FIELD( m_flStartFireTime, FIELD_TIME ),
	DEFINE_FIELD( m_flDmgTime, FIELD_TIME ),
	DEFINE_FIELD( m_hSprite, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hBeam, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hNoise, FIELD_EHANDLE ),
END_DATADESC()
    */

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CASW_Weapon_Mining_Laser::CASW_Weapon_Mining_Laser( void )
{
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= true;
	m_bPlayingCuttingSound = false;
	m_fLastDamageSoundTime = 0;
	m_fLastAttackTime = 0;
	m_bCutting = false;
	m_pChargeSound = NULL;
	m_pRunSound = NULL;
#ifdef GAME_DLL
	m_fLastForcedFireTime = 0;	// last time we forced an AI marine to push its fire button
#endif

#ifdef CLIENT_DLL
	m_bLastHadTarget = false;
#endif
}

CASW_Weapon_Mining_Laser::~CASW_Weapon_Mining_Laser()
{
	EndAttack();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Weapon_Mining_Laser::Precache( void )
{
    PrecacheScriptSound( "ASW_Mining_Laser.Start" );
    PrecacheScriptSound( "ASW_Mining_Laser.Run" );
	PrecacheScriptSound( "ASW_Mining_Laser.Cut" );
    PrecacheScriptSound( "ASW_Mining_Laser.Off" );
	PrecacheScriptSound("ASW_MiningLaser.ReloadA");
	PrecacheScriptSound("ASW_MiningLaser.ReloadB");
	PrecacheScriptSound("ASW_MiningLaser.ReloadC");
	PrecacheScriptSound("ASW_Mining_Laser.Damage");
	PrecacheParticleSystem( "mining_laser_beam" );
	PrecacheParticleSystem( "mining_laser_charging" );
	PrecacheParticleSystem( "mining_laser_exhaust" );

	BaseClass::Precache();
}

void CASW_Weapon_Mining_Laser::Spawn()
{
	BaseClass::Spawn();

#ifdef CLIENT_DLL
	SetNextClientThink( CLIENT_THINK_ALWAYS );
#endif
}

#ifdef CLIENT_DLL
void CASW_Weapon_Mining_Laser::ClientThink()
{
	BaseClass::ClientThink();

	C_ASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
	{
		if ( m_pLaserEffect )
		{
			m_pLaserEffect->StopEmission(false, true, false);
			m_pLaserEffect = NULL;
		}	
		if ( m_pChargeEffect )
		{
			m_pChargeEffect->StopEmission(false, true, false);
			m_pChargeEffect = NULL;
		}
		StopRunSound();
		return;
	}

	if ( m_bIsFiring )
	{
		int iAttachment = GetMuzzleAttachment();
		if ( iAttachment <= 0 )
			return;

		float flCharging = 0;

		switch( m_fireState )
		{
		case FIRE_STARTUP:
			{
				if ( m_pLaserEffect )
				{
					m_pLaserEffect->StopEmission(false, true, false);
					m_pLaserEffect = NULL;
					DispatchParticleEffect( "mining_laser_exhaust", PATTACH_POINT_FOLLOW, this, "eject1" );
				}

				if ( !m_pChargeEffect )
				{
					m_pChargeEffect = ParticleProp()->Create( "mining_laser_charging", PATTACH_POINT_FOLLOW, iAttachment );
				}

				flCharging = 1;
			}
			break;

		case FIRE_LASER:
			{
				if ( m_bCutting )
				{
					if ( m_pChargeEffect )
					{
						m_pChargeEffect->StopEmission(false, true, false);
						m_pChargeEffect = NULL;
					}

					if ( !m_pLaserEffect )
					{
						m_pLaserEffect = ParticleProp()->Create( "mining_laser_beam", PATTACH_POINT_FOLLOW, iAttachment );
					}
				}
				else
				{
					if ( m_pLaserEffect )
					{
						m_pLaserEffect->StopEmission(false, true, false);
						m_pLaserEffect = NULL;
						DispatchParticleEffect( "mining_laser_exhaust", PATTACH_POINT_FOLLOW, this, "eject1" );
					}

					if ( !m_pChargeEffect )
					{
						m_pChargeEffect = ParticleProp()->Create( "mining_laser_charging", PATTACH_POINT_FOLLOW, iAttachment );
					}
				}

				flCharging = 0;
			}
			break;
		}

		bool bLocalPlayer = false;
		C_ASW_Player *pPlayer = GetCommander();
		C_ASW_Player *pLocalPlayer = C_ASW_Player::GetLocalASWPlayer();
		if ( pPlayer == pLocalPlayer )
			bLocalPlayer = true;

		QAngle angWeapon;

		Vector vecShootPos	 = pMarine->Weapon_ShootPosition();
		Vector vecDirShooting;
		if ( bLocalPlayer )
		{
			vecDirShooting = pLocalPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
		}
		else
		{
			QAngle angMarine = pMarine->EyeAngles();

			Vector vRight;
			Vector vUp;
			AngleVectors( angMarine, &vecDirShooting, &vRight, &vUp );
		}

		Vector vecEnd;
		GetLaserEndPosition( vecShootPos, vecDirShooting, &vecEnd );

		trace_t	tr;
		UTIL_TraceLine( vecShootPos, vecEnd, MASK_SHOT, pMarine, COLLISION_GROUP_NONE, &tr );

		if ( m_pChargeEffect )
		{		
			float flTotalCharge = (gpGlobals->curtime - m_flStartFireTime) + ASW_MINING_LASER_CHARGE_UP_TIME;
			float flChargeAmt = 1 - (ASW_MINING_LASER_CHARGE_UP_TIME / flTotalCharge);
			//m_pChargeEffect->SetControlPoint( 0, startPoint );
			m_pChargeEffect->SetControlPoint( 1, tr.endpos );
			m_pChargeEffect->SetControlPoint( 2, Vector( flChargeAmt, flChargeAmt * flCharging, 0 ) );
		}

		if ( m_pLaserEffect )
		{		
			//m_pLaserEffect->SetControlPoint( 0, startPoint );
			m_pLaserEffect->SetControlPoint( 1, tr.endpos );
			QAngle	vecAngles;
			VectorAngles( vecDirShooting, vecAngles );
			Vector vecForward, vecRight, vecUp;
			AngleVectors( vecAngles, &vecForward, &vecRight, &vecUp );
			m_pLaserEffect->SetControlPointOrientation( 1, vecForward, vecRight, vecUp );
		}
	}
	else
	{
		if ( m_pChargeEffect )
		{
			m_pChargeEffect->StopEmission(false, true, false);
			m_pChargeEffect = NULL;
		}

		if ( m_pLaserEffect )
		{
			m_pLaserEffect->StopEmission(false, true, false);
			m_pLaserEffect = NULL;
			DispatchParticleEffect( "mining_laser_exhaust", PATTACH_POINT_FOLLOW, this, "eject1" );
		}
	}
}
#endif

bool CASW_Weapon_Mining_Laser::HasAmmo()
{
	return m_iClip1 > 0;
}

bool CASW_Weapon_Mining_Laser::Deploy( void )
{
	SetFiringState(FIRE_OFF);

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Weapon_Mining_Laser::PrimaryAttack( void )
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
		if (m_fireState != FIRE_OFF)
		{
			EndAttack();
		}
		return;
	}

	// don't fire underwater
	if ( pMarine->GetWaterLevel() == 3 )
	{
		if ( m_fireState != FIRE_OFF )
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

	Vector vecSrc		= pMarine->Weapon_ShootPosition( );
	Vector	vecAiming = vec3_origin;
	if ( pPlayer && pMarine->IsInhabited() )
	{
		vecAiming	= pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
	}
	else
	{
#ifndef CLIENT_DLL
		vecAiming	= pMarine->GetActualShootTrajectory( vecSrc );
#endif
	}

	switch( m_fireState )
	{
		case FIRE_OFF:
		{
			m_bCutting = false;

			//if ( !HasAmmo() )
			//{
				//m_flNextPrimaryAttack = gpGlobals->curtime + 0.25;
				//m_flNextSecondaryAttack = gpGlobals->curtime + 0.25;
				//WeaponSound( EMPTY );
				//return;
			//}

			m_flAmmoUseTime = gpGlobals->curtime;// start using ammo ASAP.

			StartChargingSound();

			SendWeaponAnim( ACT_VM_PRIMARYATTACK );
						
			m_flShakeTime = 0;
			m_flStartFireTime = gpGlobals->curtime;

			SetWeaponIdleTime( gpGlobals->curtime + 0.1 );

			m_flDmgTime = gpGlobals->curtime + ASW_MINING_LASER_PULSE_INTERVAL;			
			SetFiringState(FIRE_STARTUP);
		}
		break;

		case FIRE_STARTUP:
		{
			m_bCutting = false;
			m_bIsFiring = true;

			if ( gpGlobals->curtime >= ( m_flStartFireTime + ASW_MINING_LASER_CHARGE_UP_TIME ) )
			{
				m_bPlayingCuttingSound = false;	
				StartRunSound();
				
				SetFiringState(FIRE_LASER);
			}

			if ( !HasAmmo() )
			{
				EndAttack();
				m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
				m_flNextSecondaryAttack = gpGlobals->curtime + 1.0;
			}
		}
		break;

		case FIRE_LASER:
		{
			m_bCutting = Fire( vecSrc, vecAiming );
			m_bIsFiring = true;

			bool bFiredRecently = m_fLastAttackTime != 0 && ((gpGlobals->curtime - m_fLastAttackTime) < 0.12f);

			Vector vecDest;
			GetLaserEndPosition( vecSrc, vecAiming, &vecDest );

			if ( m_bCutting && (gpGlobals->curtime - m_fLastDamageSoundTime) > asw_mininglaser_damage_snd_interval.GetFloat() )
			{
				CPASFilter filter( vecDest );
				EmitSound( filter, SOUND_FROM_WORLD, "ASW_Mining_Laser.Damage", &vecDest );
				m_fLastDamageSoundTime = gpGlobals->curtime;
				//Msg( "%f - m_fLastDamageSoundTime\n", m_fLastDamageSoundTime );
			}

			if (bFiredRecently && !m_bPlayingCuttingSound)
			{
				StopRunSound();
				EmitSound("ASW_Mining_Laser.Cut");

				m_bPlayingCuttingSound = true;
			}
			else if (!bFiredRecently && m_bPlayingCuttingSound)
			{
				StopSound("ASW_Mining_Laser.Cut");				
				StartRunSound();
				m_bPlayingCuttingSound = false;
			}
		
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

bool CASW_Weapon_Mining_Laser::Fire( const Vector &vecOrigSrc, const Vector &vecDir )
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
	{
		return false;
	}

	//CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 450, 0.1 );
    //WeaponSound( SINGLE );
	
	Vector vecDest;
	GetLaserEndPosition( vecOrigSrc, vecDir, &vecDest );

	trace_t	tr;
	UTIL_TraceLine( vecOrigSrc, vecDest, MASK_SHOT, pMarine, COLLISION_GROUP_NONE, &tr );

	//if ( tr.allsolid )
	//	return;

	Vector vecUp, vecRight;
	QAngle angDir;

	VectorAngles( vecDir, angDir );
	AngleVectors( angDir, NULL, &vecRight, &vecUp );

	Vector tmpSrc = vecOrigSrc + (vecUp * -8) + (vecRight * 3);

	CBaseEntity *pEntity = tr.m_pEnt;
	if ( pEntity == NULL )
		return false;

	if ( m_flDmgTime < gpGlobals->curtime )
	{
		m_fLastAttackTime = gpGlobals->curtime;

		// wide mode does damage to the ent, and radius damage
		if ( pEntity->m_takedamage != DAMAGE_NO )
		{
			ClearMultiDamage();
			float fDamage = GetWeaponInfo()->m_flBaseDamage;  //sk_plr_dmg_asw_ml.GetFloat();
			CTakeDamageInfo info( this, pMarine, fDamage * g_pGameRules->GetDamageMultiplier(), DMG_ENERGYBEAM | DMG_ALWAYSGIB );
			info.SetWeapon( this );
			CalculateMeleeDamageForce( &info, vecDir, tr.endpos );
			pEntity->DispatchTraceAttack( info, vecDir, &tr );
			ApplyMultiDamage();
		}

		// radius damage a little more potent in multiplayer.
#ifndef CLIENT_DLL
		//RadiusDamage( CTakeDamageInfo( this, pMarine, sk_plr_dmg_asw_ml.GetFloat() * g_pGameRules->GetDamageMultiplier() / 4, DMG_ENERGYBEAM | DMG_BLAST | DMG_ALWAYSGIB ), tr.endpos, 128, CLASS_NONE, NULL );
#endif		

		if ( !pMarine->IsAlive() )
			return false;

		// uses 5 ammo/second
		if ( gpGlobals->curtime >= m_flAmmoUseTime )
		{
			// decrement ammo
			m_iClip1 -= 1;
			m_flAmmoUseTime = gpGlobals->curtime + 0.2;
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

			pMarine->OnWeaponFired( this, 1 );
#endif
		}
		

		m_flDmgTime = gpGlobals->curtime + ASW_MINING_LASER_DISCHARGE_INTERVAL;
		if ( m_flShakeTime < gpGlobals->curtime )
		{
#ifndef CLIENT_DLL
			UTIL_ScreenShake( tr.endpos, 5.0, 150.0, 0.75, 250.0, SHAKE_START );
#endif
			m_flShakeTime = gpGlobals->curtime + 1.5;
		}
	}

	return true;
}

void CASW_Weapon_Mining_Laser::GetLaserEndPosition( Vector vecStart, Vector vecDir, Vector *pVecEnd )
{
	*pVecEnd = vecStart + (vecDir * ASW_MINING_LASER_RANGE);
}

void CASW_Weapon_Mining_Laser::EndAttack( void )
{
	m_bIsFiring = false;

	StopRunSound();
	StopSound( "ASW_Mining_Laser.Cut" );
	
	if ( m_fireState != FIRE_OFF )
	{
        EmitSound( "ASW_Mining_Laser.Off" );

		/*
		Vector vecEjectPos, vecEjectPos2;
		QAngle angEject, angEject2;
		int iAttachment = LookupAttachment( "eject1" );
		GetAttachment( iAttachment, vecEjectPos, angEject );

		unsigned char color[3];
		color[0] = 50;
		color[1] = 128;
		color[2] = 50;		

		iAttachment = LookupAttachment( "eject2" );
		GetAttachment( iAttachment, vecEjectPos2, angEject2 );
		*/

#ifdef CLIENT_DLL
		//FX_Smoke( vecEjectPos, angEject, 0.5, 5, &color[0], 192 );
		//FX_Smoke( vecEjectPos2, angEject2, 0.5, 5, &color[0], 192 );
#else
		//CPVSFilter filter( vecEjectPos );
		//te->Smoke( filter, 0.0, &vecEjectPos, g_sModelIndexSmoke, 0.5f, 10 );
		//te->Smoke( filter, 0.0, &vecEjectPos2, g_sModelIndexSmoke, 0.5f, 10 );
#endif
	}

	SetWeaponIdleTime( gpGlobals->curtime + 2.0 );
	m_flNextPrimaryAttack = gpGlobals->curtime + GetWeaponInfo()->m_flFireRate;
	m_flNextSecondaryAttack = gpGlobals->curtime + GetWeaponInfo()->m_flFireRate;

	SetFiringState(FIRE_OFF);	

	ClearIsFiring();
}

void CASW_Weapon_Mining_Laser::Drop( const Vector &vecVelocity )
{	
	EndAttack();

	BaseClass::Drop( vecVelocity );
}

bool CASW_Weapon_Mining_Laser::Holster( CBaseCombatWeapon *pSwitchingTo )
{
    EndAttack();

	return BaseClass::Holster( pSwitchingTo );
}

void CASW_Weapon_Mining_Laser::WeaponIdle( void )
{
	if ( !HasWeaponIdleTimeElapsed() )
		return;

	if ( m_fireState != FIRE_OFF )
		 EndAttack();
	
	float flIdleTime = gpGlobals->curtime + 5.0;

	SetWeaponIdleTime( flIdleTime );
}

void CASW_Weapon_Mining_Laser::SetFiringState(MININGLASER_FIRE_STATE state)
{
#ifdef CLIENT_DLL
	//Msg("[C] SetFiringState %d\n", state);
#else
	//Msg("[C] SetFiringState %d\n", state);
#endif
	m_fireState = state;
}

bool CASW_Weapon_Mining_Laser::ShouldMarineMoveSlow()
{
	return m_fireState != FIRE_OFF;
}

#ifdef CLIENT_DLL
const char* CASW_Weapon_Mining_Laser::GetPartialReloadSound(int iPart)
{
	switch (iPart)
	{
		case 1: return "ASW_MiningLaser.ReloadB"; break;
		case 2: return "ASW_MiningLaser.ReloadC"; break;
		default: break;
	};
	return "ASW_MiningLaser.ReloadA";
}
#endif

#ifdef GAME_DLL
void CASW_Weapon_Mining_Laser::GetButtons(bool& bAttack1, bool& bAttack2, bool& bReload, bool& bOldReload, bool& bOldAttack1 )
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

ConVar asw_mining_laser_start_run_fade( "asw_mining_laser_start_run_fade", "0.1", FCVAR_CHEAT | FCVAR_REPLICATED, "Crossfade time between mining laser's charge and run sound" );
ConVar asw_mining_laser_run_fade( "asw_mining_laser_run_fade", "0.05", FCVAR_CHEAT | FCVAR_REPLICATED, "Fade time for mining laser run sound" );


void CASW_Weapon_Mining_Laser::StartChargingSound()
{
	if ( !m_pChargeSound )
	{
		CPASAttenuationFilter filter( this );
		m_pChargeSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "ASW_Mining_Laser.Start" );
	}
	CSoundEnvelopeController::GetController().Play( m_pChargeSound, 1.0, 100 );
}

void CASW_Weapon_Mining_Laser::StartRunSound()
{
	if ( m_pChargeSound )
	{
		CSoundEnvelopeController::GetController().SoundFadeOut( m_pChargeSound, asw_mining_laser_start_run_fade.GetFloat(), true );
		m_pChargeSound = NULL;
	}
	if ( !m_pRunSound )
	{
		CPASAttenuationFilter filter( this );
		m_pRunSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "ASW_Mining_Laser.Run" );
	}
	CSoundEnvelopeController::GetController().Play( m_pRunSound, 0.0, 100 );
	CSoundEnvelopeController::GetController().SoundChangeVolume( m_pRunSound, 1.0, asw_mining_laser_start_run_fade.GetFloat() );
}

void CASW_Weapon_Mining_Laser::StopRunSound()
{
	if ( m_pChargeSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pChargeSound );
		m_pChargeSound = NULL;
	}
	if ( m_pRunSound )
	{
		CSoundEnvelopeController::GetController().SoundFadeOut( m_pRunSound, asw_mining_laser_run_fade.GetFloat(), true );
		m_pRunSound = NULL;
	}
}

void CASW_Weapon_Mining_Laser::UpdateOnRemove()
{
	if ( m_pChargeSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pChargeSound );
		m_pChargeSound = NULL;
	}
	if ( m_pRunSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pRunSound );
		m_pRunSound = NULL;
	}
	BaseClass::UpdateOnRemove();
}
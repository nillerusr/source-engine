#include "cbase.h"
#include "takedamageinfo.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_baseplayer.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_te_effect_dispatch.h"
#include "c_asw_marine_resource.h"
#include "c_asw_game_resource.h"
#include "asw_hud_crosshair.h"
#include "asw_input.h"
#else
#include "asw_player.h"
#include "asw_marine.h"
#include "asw_marine_speech.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "asw_marine_resource.h"
#include "asw_game_resource.h"
#include "ndebugoverlay.h"
#include "asw_fail_advice.h"
#include "asw_marine_speech.h"
#include "asw_marine_profile.h"
#include "asw_triggers.h"
#endif

#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "asw_trace_filter_shot.h"
#include "particle_parse.h"
#include "asw_marine_skills.h"
#include "soundenvelope.h"
#include "asw_weapon_medical_satchel_shared.h"
#include "asw_gamerules.h"

#include "asw_weapon_heal_gun_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//static const float ASW_HG_HEAL_RATE = 0.25f; // Rate at which we heal entities we're latched on
static const float ASW_HG_SEARCH_DIAMETER = 48.0f; // How far past the range to search for enemies to latch on to
static const float SQRT3 = 1.732050807569; // for computing max extents inside a box

extern ConVar asw_laser_sight;
extern ConVar asw_marine_special_heal_chatter_chance;

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Heal_Gun, DT_ASW_Weapon_Heal_Gun );

BEGIN_NETWORK_TABLE( CASW_Weapon_Heal_Gun, DT_ASW_Weapon_Heal_Gun )
#ifdef CLIENT_DLL
// recvprops
RecvPropTime( RECVINFO( m_fSlowTime ) ),
RecvPropInt( RECVINFO( m_FireState ) ),
RecvPropEHandle( RECVINFO( m_hHealEntity ) ),
RecvPropVector( RECVINFO( m_vecHealPos ) ),
#else
// sendprops
SendPropTime( SENDINFO( m_fSlowTime ) ),
SendPropInt( SENDINFO( m_FireState ), Q_log2(ASW_HG_NUM_FIRE_STATES)+1, SPROP_UNSIGNED ),
SendPropEHandle( SENDINFO( m_hHealEntity ) ),
SendPropVector( SENDINFO( m_vecHealPos ) ),
#endif
END_NETWORK_TABLE()    

LINK_ENTITY_TO_CLASS( asw_weapon_heal_gun, CASW_Weapon_Heal_Gun );
PRECACHE_WEAPON_REGISTER( asw_weapon_heal_gun );

#if defined( CLIENT_DLL )
BEGIN_PREDICTION_DATA( CASW_Weapon_Heal_Gun )
	DEFINE_PRED_FIELD_TOL( m_fSlowTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD( m_vecHealPos, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CASW_Weapon_Heal_Gun::CASW_Weapon_Heal_Gun( void )
{
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= true;
	m_flLastHealTime = 0;
	m_flNextHealMessageTime = 0.0f;
	m_flTotalAmountHealed = 0;
#ifdef GAME_DLL
	m_fLastForcedFireTime = 0;	// last time we forced an AI marine to push its fire button
#endif

	m_pSearchSound = NULL;
	m_pHealSound = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Weapon_Heal_Gun::Precache( void )
{
	PrecacheScriptSound("ASW_MiningLaser.ReloadA");
	PrecacheScriptSound("ASW_MiningLaser.ReloadB");
	PrecacheScriptSound("ASW_MiningLaser.ReloadC");

	PrecacheScriptSound( "ASW_HealGun.SearchLoop" );
	PrecacheScriptSound( "ASW_HealGun.HealLoop" );
	PrecacheScriptSound( "ASW_HealGun.StartHeal" );
	PrecacheScriptSound( "ASW_HealGun.StartSearch" );
	PrecacheScriptSound( "ASW_HealGun.Off" );

	PrecacheParticleSystem( "heal_gun_noconnect" );
	PrecacheParticleSystem( "heal_gun_attach" );
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Weapon_Heal_Gun::PrimaryAttack( void )
{
	CASW_Player *pPlayer = GetCommander();
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine || !pMarine->IsAlive() )
	{
		EndAttack();
		return;
	}

	// don't fire underwater
	if ( pMarine->GetWaterLevel() == 3 )
	{
		WeaponSound( EMPTY );

		m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5;
		return;
	}

	Vector vecSrc = pMarine->Weapon_ShootPosition( );
	Vector vecAiming = vec3_origin;

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

#ifdef GAME_DLL
	m_bIsFiring = true;

	switch ( m_FireState )
	{
		case ASW_HG_FIRE_OFF:
		{
			Fire( vecSrc, vecAiming );
			m_flNextPrimaryAttack = gpGlobals->curtime;
			break;
		}

		case ASW_HG_FIRE_HEALSELF:
		{
			EHANDLE hHealEntity = m_hHealEntity.Get();

			if ( !hHealEntity || !TargetCanBeHealed( hHealEntity.Get() ) )
			{
				HealDetach();
				SetFiringState( ASW_HG_FIRE_DISCHARGE );
				break;
			}

			if ( hHealEntity.Get() == GetMarine() )
			{
				HealSelf();
				break;
			}
		}

		case ASW_HG_FIRE_DISCHARGE:
		{
			Fire( vecSrc, vecAiming );

			EHANDLE hHealEntity = m_hHealEntity.Get();

			// Search for nearby entities to heal
			if ( !hHealEntity )
			{
				if ( pPlayer->GetHighlightEntity() && pPlayer->GetHighlightEntity()->Classify() == CLASS_ASW_MARINE )
				{
					CASW_Marine* pTargetMarine = static_cast< CASW_Marine* >( pPlayer->GetHighlightEntity() );
					if ( pTargetMarine )
					{
						// healing self
						if ( pTargetMarine == GetMarine() && TargetCanBeHealed( pTargetMarine ) )
						{
							HealSelf();
							break;
						}
						else if ( HealAttach( pTargetMarine ) )
						{
							SetFiringState( ASW_HG_FIRE_ATTACHED );
							m_flLastHealTime = gpGlobals->curtime;
							break;
						}
					}
				}
			}
			
			m_flNextPrimaryAttack = gpGlobals->curtime;
			m_flNextSecondaryAttack = m_flNextPrimaryAttack;
			StartSearchSound();
			break;
		}

		case ASW_HG_FIRE_ATTACHED:
		{
			Fire( vecSrc, vecAiming );

			EHANDLE hHealEntity = m_hHealEntity.Get();
			// detach if...
			// full health, dead or not here
			if ( !hHealEntity || !TargetCanBeHealed( hHealEntity.Get() ) )
			{
				HealDetach();
				SetFiringState( ASW_HG_FIRE_DISCHARGE );
				break;
			}
			
			// too far
			float flHealDistance = (hHealEntity->GetAbsOrigin() - pMarine->GetAbsOrigin()).LengthSqr();
			if ( flHealDistance > (GetWeaponRange() + SQRT3*ASW_HG_SEARCH_DIAMETER)*(GetWeaponRange() + SQRT3*ASW_HG_SEARCH_DIAMETER) )
			{
				HealDetach();
				SetFiringState( ASW_HG_FIRE_DISCHARGE );
				break;
			}

			// facing another direction
			Vector vecAttach = hHealEntity->GetAbsOrigin() - pMarine->GetAbsOrigin();
			Vector vecForward;
			QAngle vecEyeAngles;
			CASW_Player *pPlayer = pMarine->GetCommander();
			if ( pMarine->IsInhabited() && pPlayer )
				vecEyeAngles = pPlayer->EyeAngles();
			else
				vecEyeAngles = pMarine->ASWEyeAngles();

			AngleVectors( vecEyeAngles, &vecForward );
			if ( DotProduct( vecForward, vecAttach ) < 0.0f )
			{
				HealDetach();
				SetFiringState( ASW_HG_FIRE_DISCHARGE );
				break;
			}

			if ( gpGlobals->curtime > m_flLastHealTime + GetFireRate() )
			{
				HealEntity();
				m_flLastHealTime = gpGlobals->curtime;
			}

			m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
			m_flNextSecondaryAttack = m_flNextPrimaryAttack;

			//StartHealSound();
			break;
		}
	}

	SendWeaponAnim( GetPrimaryAttackActivity() );			

	SetWeaponIdleTime( gpGlobals->curtime + GetFireRate() );
#endif

	m_fSlowTime = gpGlobals->curtime + GetFireRate();
}

void CASW_Weapon_Heal_Gun::SecondaryAttack()
{
	HealSelf();
}

bool CASW_Weapon_Heal_Gun::Reload( void )
{
	EndAttack();

	return BaseClass::Reload();
}

bool CASW_Weapon_Heal_Gun::HasAmmo()
{
	return m_iClip1 > 0;
}

void CASW_Weapon_Heal_Gun::WeaponIdle( void )
{
	if ( !HasWeaponIdleTimeElapsed() )
		return;

	if ( m_FireState != ASW_HG_FIRE_OFF )
	{
		EndAttack();
	}

	float flIdleTime = gpGlobals->curtime + 5.0;

	SetWeaponIdleTime( flIdleTime );
}

void CASW_Weapon_Heal_Gun::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	CheckEndFiringState();
}

void CASW_Weapon_Heal_Gun::ItemBusyFrame( void )
{
	BaseClass::ItemBusyFrame();

	CheckEndFiringState();
}

void CASW_Weapon_Heal_Gun::CheckEndFiringState( void )
{
	bool bAttack1, bAttack2, bReload, bOldReload, bOldAttack1;
	GetButtons(bAttack1, bAttack2, bReload, bOldReload, bOldAttack1 );

	bool bNotAttacking = !bAttack1 && !bAttack2;
	if ( (m_bIsFiring || m_FireState != ASW_HG_FIRE_OFF) && bNotAttacking )
	{
		EndAttack();
	}
}

void CASW_Weapon_Heal_Gun::UpdateOnRemove()
{
	if ( m_pSearchSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pSearchSound );
		m_pSearchSound = NULL;
	}
	if ( m_pHealSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pHealSound );
		m_pHealSound = NULL;
	}
	BaseClass::UpdateOnRemove();
}


void CASW_Weapon_Heal_Gun::Drop( const Vector &vecVelocity )
{	
	EndAttack();

	BaseClass::Drop( vecVelocity );
}

bool CASW_Weapon_Heal_Gun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	EndAttack();

	return BaseClass::Holster( pSwitchingTo );
}

bool CASW_Weapon_Heal_Gun::ShouldMarineMoveSlow()
{
	return m_fSlowTime > gpGlobals->curtime || IsReloading();
}

#ifdef GAME_DLL
void CASW_Weapon_Heal_Gun::GetButtons(bool& bAttack1, bool& bAttack2, bool& bReload, bool& bOldReload, bool& bOldAttack1 )
{
	CASW_Marine *pMarine = GetMarine();

	// make AI fire this weapon whenever they have a heal target
	if (pMarine && !pMarine->IsInhabited())
	{
		bAttack1 = ( m_hHealEntity->Get() != NULL );
		bAttack2 = false;
		bReload = false;
		bOldReload = false;

		return;
	}

	BaseClass::GetButtons(bAttack1, bAttack2, bReload, bOldReload, bOldAttack1);
}
#endif


void CASW_Weapon_Heal_Gun::SetFiringState(ASW_Weapon_HealGunFireState_t state)
{
#ifdef CLIENT_DLL
	//Msg("[C] SetFiringState %d\n", state);
#else
	//Msg("[C] SetFiringState %d\n", state);
#endif

	// Check for transitions
	if ( m_FireState != state )
	{	
		if ( state == ASW_HG_FIRE_DISCHARGE || state == ASW_HG_FIRE_ATTACHED )
		{
			EmitSound( "ASW_HealGun.StartSearch" );
		}
		else if ( state == ASW_HG_FIRE_OFF )
		{
			//StopSound( "ASW_HealGun.SearchLoop" );
			EmitSound( "ASW_Tesla_Laser.Stop" );

		}
	}

	m_FireState = state;
}

bool CASW_Weapon_Heal_Gun::TargetCanBeHealed( CBaseEntity* pTarget )
{	
	if ( pTarget->Classify() != CLASS_ASW_MARINE )
		return false;

	CASW_Marine *pMarine = static_cast<CASW_Marine*>( pTarget );
	if ( !pMarine )
		return false;

	if ( pMarine->GetHealth() <= 0 || ( pMarine->GetHealth() + pMarine->m_iSlowHealAmount ) >= pMarine->GetMaxHealth() )
		return false;

	return true;
}

void CASW_Weapon_Heal_Gun::HealSelf( void )
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return;

	if ( !TargetCanBeHealed( pMarine ) )
	{
		// TODO: have some better feedback here if the player is full on health?
		WeaponSound( EMPTY );
		m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
		m_flNextSecondaryAttack = m_flNextPrimaryAttack;
		return;
	}

	if ( pMarine != m_hHealEntity.Get() )
	{
		SetFiringState( ASW_HG_FIRE_OFF );
		HealDetach();
	}

	m_flLastHealTime = gpGlobals->curtime;
	m_hHealEntity.Set( pMarine );
	HealEntity();
	SetFiringState( ASW_HG_FIRE_HEALSELF );
	m_bIsFiring = true;

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_flNextSecondaryAttack = m_flNextPrimaryAttack;
	m_fSlowTime = m_flNextSecondaryAttack;

	SetWeaponIdleTime( m_flNextSecondaryAttack );
}

void CASW_Weapon_Heal_Gun::HealEntity( void )
{
	// verify target
	CASW_Marine *pMarine = GetMarine();
	EHANDLE hHealEntity = m_hHealEntity.Get();
	Assert( hHealEntity && hHealEntity->m_takedamage != DAMAGE_NO && pMarine );
	if ( !pMarine )
		return;

	if ( hHealEntity.Get()->Classify() != CLASS_ASW_MARINE )
		return;

	CASW_Marine *pTarget = static_cast<CASW_Marine*>( static_cast<CBaseEntity*>( hHealEntity.Get() ) );
	if ( !pTarget )
		return;

	// apply heal
	int iHealAmount = MIN( GetHealAmount(), pTarget->GetMaxHealth() - ( pTarget->GetHealth() + pTarget->m_iSlowHealAmount ) );

	if ( iHealAmount == 0 )
		return;

#ifdef GAME_DLL
	pTarget->AddSlowHeal( iHealAmount, 2, pMarine, this );

	m_flTotalAmountHealed += iHealAmount;
	if ( m_flTotalAmountHealed > 50 )
	{
		// TODO: only fire this off if we've healed enough
		ASWFailAdvice()->OnMarineHealed();
	}

	if ( gpGlobals->curtime > m_flNextHealMessageTime )
	{
		// Fire event
		IGameEvent * event = gameeventmanager->CreateEvent( "player_heal" );
		if ( event )
		{
			CASW_Player *pPlayer = GetCommander();
			event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
			event->SetInt( "entindex", pTarget->entindex() );
			gameeventmanager->FireEvent( event );
		}

		m_flNextHealMessageTime = gpGlobals->curtime + 2.0f;
	}

	if ( ASWGameRules()->GetInfoHeal() )
	{
		ASWGameRules()->GetInfoHeal()->OnMarineHealed( GetMarine(), pTarget, this );
	}

#endif

	// decrement ammo
	m_iClip1 -= 1;

	// emit heal sound
	StartHealSound();

#ifdef GAME_DLL
	bool bHealingSelf = (pMarine == pTarget);
	bool bInfested = pTarget->IsInfested();

	if ( bInfested )
	{
		float fCurePercent = GetInfestationCureAmount();

		// cure infestation
		if ( fCurePercent > 0.0f )
		{
			// Cure infestation on a per bullet basis (the full clip does cure relative to 9 heal grenades)
			pTarget->CureInfestation( pMarine, 1.0f - ( ( 1.0f - fCurePercent ) / ( GetMaxClip1() / 9.0f ) ) );
		}
	}

	bool bSkipChatter = bInfested;
	if ( m_iClip1 <= 0 )
	{
		// Out of ammo
		ASWFailAdvice()->OnMedSatchelEmpty();

		pMarine->GetMarineSpeech()->Chatter( CHATTER_MEDS_NONE );

		if ( pMarine )
		{
			pMarine->Weapon_Detach(this);
			if ( pMarine->GetActiveWeapon() == this )
			{
				pMarine->SwitchToNextBestWeapon( NULL );
			}
		}
		Kill();

		pMarine->GetMarineSpeech()->Chatter( CHATTER_MEDS_NONE );
		bSkipChatter = true;
	}
	else if ( m_iClip1 == 10 )
	{
		if ( pMarine->GetMarineSpeech()->Chatter( CHATTER_MEDS_LOW ) )
		{
			bSkipChatter = true;
		}
	}
	else if ( (m_flLastHealTime + 4.0f) > gpGlobals->curtime )
	{
		bSkipChatter = true;
	}
	else if ( bHealingSelf )
	{
		bSkipChatter = true;
	}

	if ( !bSkipChatter )
	{
		// try and do a special chatter?
		bool bSkipChatter = false;
		if ( pMarine->GetMarineSpeech()->AllowCalmConversations(CONV_HEALING_CRASH) )
		{
			if ( !pTarget->m_bDoneWoundedRebuke && pTarget->GetMarineResource() && pTarget->GetMarineResource()->m_bTakenWoundDamage )
			{
				// marine has been wounded and this is our first heal after the fact - good chance of the medic saying something
				if ( random->RandomFloat() < asw_marine_special_heal_chatter_chance.GetFloat() * 3 )
				{
					if ( CASW_MarineSpeech::StartConversation(CONV_SERIOUS_INJURY, pMarine, pTarget) )
					{
						bSkipChatter = true;
						pTarget->m_bDoneWoundedRebuke = true;
					}
				}
			}

			// if we didn't complaint about a serious injury, check for doing a different kind of conv
			float fRand = random->RandomFloat();
			if ( !bSkipChatter && fRand < asw_marine_special_heal_chatter_chance.GetFloat() )
			{
				if ( pTarget->GetMarineProfile() && pTarget->GetMarineProfile()->m_VoiceType == ASW_VOICE_CRASH
					&& pMarine->GetMarineProfile() && pMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_BASTILLE )	// bastille healing crash
				{
					if ( random->RandomFloat() < 0.5f )
					{
						if ( CASW_MarineSpeech::StartConversation(CONV_HEALING_CRASH, pMarine, pTarget) )
							bSkipChatter = true;						
					}
					else
					{
						if ( CASW_MarineSpeech::StartConversation(CONV_MEDIC_COMPLAIN, pMarine, pTarget) )
							bSkipChatter = true;
					}
				}
				else
				{
					if ( CASW_MarineSpeech::StartConversation(CONV_MEDIC_COMPLAIN, pMarine, pTarget) )
						bSkipChatter = true;
				}
			}
		}
		if ( !bSkipChatter )
			pMarine->GetMarineSpeech()->Chatter( CHATTER_HEALING );
	}
#endif
}

bool CASW_Weapon_Heal_Gun::HealAttach( CBaseEntity *pEntity )
{
#ifdef CLIENT_DLL
	return false;
#else

	if ( !pEntity || (pEntity->Classify() != CLASS_ASW_MARINE) ||
		(pEntity == m_hHealEntity.Get()) || !TargetCanBeHealed( pEntity ) )  // we can attach to ourselves //|| pEntity == GetMarine() 
	{
		return false;
	}

	if ( m_hHealEntity.Get() )
	{
		HealDetach();
	}

	m_hHealEntity.Set( pEntity );

	// we need a trace_t struct representing the entity we're healing, but if we 
	//  attached based on proximity to the end of the beam, we might not have actually performed a trace - for now just update m_AttackTrace
	//  but it'd be nice to decouple attribute damage from requiring a trace
	if ( m_AttackTrace.m_pEnt != pEntity )
	{
		m_AttackTrace.endpos = pEntity->GetAbsOrigin();
		m_AttackTrace.m_pEnt = pEntity;
	}

	ASW_WPN_DEV_MSG( "Attached to %d:%s\n", m_hHealEntity.Get()->entindex(), m_hHealEntity.Get()->GetClassname() );
	return true;
#endif
}

void CASW_Weapon_Heal_Gun::HealDetach()
{
#ifdef GAME_DLL
	ASW_WPN_DEV_MSG( "Detaching from %d:%s\n", m_hHealEntity.Get() ? m_hHealEntity.Get()->entindex() : -1, m_hHealEntity.Get() ? m_hHealEntity.Get()->GetClassname() : "(null)" );
	m_hHealEntity.Set( NULL );
#endif
	StopHealSound();
}

void CASW_Weapon_Heal_Gun::Fire( const Vector &vecOrigSrc, const Vector &vecDir )
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
	{
		return;
	}

	Vector vecDest	= vecOrigSrc + (vecDir * GetWeaponRange());
	trace_t	tr;
	UTIL_TraceLine( vecOrigSrc, vecDest, MASK_SHOT, pMarine, COLLISION_GROUP_NONE, &tr );

	if ( tr.allsolid )
		return;

	m_AttackTrace = tr;

	if( pMarine->IsInhabited() )
	{
		CBaseEntity *pEntity = tr.m_pEnt;
		CASW_Marine *pTargetMarine = NULL;
		if ( pEntity )
		{
			pTargetMarine = CASW_Marine::AsMarine( pEntity );
		}

		Vector vecUp, vecRight;
		QAngle angDir;

		VectorAngles( vecDir, angDir );
		AngleVectors( angDir, NULL, &vecRight, &vecUp );

		if ( HealAttach( pEntity ) )
		{
			SetFiringState( ASW_HG_FIRE_ATTACHED );
		}
	}
	else
	{
		SetFiringState( ASW_HG_FIRE_ATTACHED );
	}

	if ( !m_hHealEntity.Get() )
	{
		// If we fail to hit something, and we're not healing someone, we're just shooting out a beam
		SetFiringState( ASW_HG_FIRE_DISCHARGE );
	}

	vecDest = tr.endpos;
	m_vecHealPos = vecDest;

/*
#ifdef GAME_DLL
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
	*/
}


void CASW_Weapon_Heal_Gun::EndAttack( void )
{
	SetWeaponIdleTime( gpGlobals->curtime + 2.0 );

	if ( m_FireState != ASW_HG_FIRE_OFF )
	{
		EmitSound( "ASW_HealGun.Off" );
	}

	SetFiringState( ASW_HG_FIRE_OFF );
	
	HealDetach();

	ClearIsFiring();

	m_bIsFiring = false;
}

ConVar asw_heal_gun_start_heal_fade( "asw_heal_gun_start_heal_fade", "0.25", FCVAR_CHEAT | FCVAR_REPLICATED, "Crossfade time between heal gun's search and heal sound" );
ConVar asw_heal_gun_heal_fade( "asw_heal_gun_heal_fade", "0.05", FCVAR_CHEAT | FCVAR_REPLICATED, "Fade time for heal gun's heal sound" );

void CASW_Weapon_Heal_Gun::StartSearchSound()
{
	if ( !m_pSearchSound )
	{
		CPASAttenuationFilter filter( this );
		m_pSearchSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "ASW_HealGun.SearchLoop" );
	}
	CSoundEnvelopeController::GetController().Play( m_pSearchSound, 1.0, 100 );
}

void CASW_Weapon_Heal_Gun::StartHealSound()
{
	if ( !m_pSearchSound )
	{
		StartSearchSound();
	}

	//if ( m_pSearchSound )
	//{
	//	CSoundEnvelopeController::GetController().SoundFadeOut( m_pSearchSound, asw_heal_gun_start_heal_fade.GetFloat(), true );
	//	m_pSearchSound = NULL;
	//}

	if ( !m_pHealSound )
	{
		CPASAttenuationFilter filter( this );
		m_pHealSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "ASW_HealGun.HealLoop" );
		EmitSound( "ASW_HealGun.StartHeal" );
	}
	CSoundEnvelopeController::GetController().Play( m_pHealSound, 0.0, 100 );
	CSoundEnvelopeController::GetController().SoundChangeVolume( m_pHealSound, 1.0, asw_heal_gun_start_heal_fade.GetFloat() );
}

void CASW_Weapon_Heal_Gun::StopHealSound()
{
	if ( m_pSearchSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pSearchSound );
		m_pSearchSound = NULL;
	}
	if ( m_pHealSound )
	{
		CSoundEnvelopeController::GetController().SoundFadeOut( m_pHealSound, asw_heal_gun_heal_fade.GetFloat(), true );
		m_pHealSound = NULL;
	}
}


int CASW_Weapon_Heal_Gun::GetHealAmount()
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
	{
		return 0;
	}

	float flHealAmount = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_HEALING, ASW_MARINE_SUBSKILL_HEAL_GUN_HEAL_AMOUNT);

	return flHealAmount;
}

float CASW_Weapon_Heal_Gun::GetInfestationCureAmount()
{
	CASW_Marine *pMarine = GetMarine();
	if (!pMarine)
		return 0.0f;		

	float flCureAmount = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_XENOWOUNDS) / 100.0f;

	return flCureAmount;
}

#ifdef CLIENT_DLL
bool CASW_Weapon_Heal_Gun::ShouldShowLaserPointer()
{
	if ( !asw_laser_sight.GetBool() || IsDormant() || !GetOwner() )
	{
		return false;
	}

	C_ASW_Marine *pMarine = GetMarine();

	return ( pMarine && pMarine->GetActiveWeapon() == this );
}

// if the player has his mouse over another marine, highlight it, cos he's the one we can give health to
void CASW_Weapon_Heal_Gun::MouseOverEntity(C_BaseEntity *pEnt, Vector vecWorldCursor)
{
	C_ASW_Marine* pOtherMarine = C_ASW_Marine::AsMarine( pEnt );
	CASW_Player *pOwner = GetCommander();
	CASW_Marine *pMarine = GetMarine();
	if (!pOwner || !pMarine)
		return;
	
	if (!pOtherMarine)
	{
		C_ASW_Game_Resource *pGameResource = ASWGameResource();
		if (pGameResource)
		{
			// find marine closest to world cursor
			const float fMustBeThisClose = 70;
			const C_ASW_Game_Resource::CMarineToCrosshairInfo::tuple_t &info = pGameResource->GetMarineCrosshairCache()->GetClosestMarine();
			if ( info.m_fDistToCursor <= fMustBeThisClose )
			{
				pOtherMarine = info.m_hMarine.Get();
			}
		}
	}

	// if the marine our cursor is over is near enough, highlight him
	if (pOtherMarine)
	{
		float dist = (pMarine->GetAbsOrigin() - pOtherMarine->GetAbsOrigin()).Length2D();
		if (dist < GetWeaponRange() )
		{
			bool bCanGiveHealth = ( TargetCanBeHealed( pOtherMarine ) && m_iClip1 > 0 );
			ASWInput()->SetHighlightEntity( pOtherMarine, bCanGiveHealth );
			if ( bCanGiveHealth )		// if he needs healing, show the give health cursor
			{
				CASWHudCrosshair *pCrosshair = GET_HUDELEMENT( CASWHudCrosshair );
				if ( pCrosshair )
				{
					pCrosshair->SetShowGiveHealth(true);
				}
			}
		}
	}		
}

void CASW_Weapon_Heal_Gun::ClientThink()
{
	BaseClass::ClientThink();

	UpdateEffects();
}

const char* CASW_Weapon_Heal_Gun::GetPartialReloadSound(int iPart)
{
	switch (iPart)
	{
		case 1: return "ASW_MiningLaser.ReloadB"; break;
		case 2: return "ASW_MiningLaser.ReloadC"; break;
		default: break;
	};
	return "ASW_MiningLaser.ReloadA";
}

void CASW_Weapon_Heal_Gun::UpdateEffects()
{
	if ( !m_hHealEntity.Get() || m_hHealEntity.Get()->Classify() != CLASS_ASW_MARINE || !GetMarine() )
	{
		if ( m_pDischargeEffect )
		{
			m_pDischargeEffect->StopEmission();
			m_pDischargeEffect = NULL;
		}
		return;
	}

	C_ASW_Marine* pMarine = static_cast<C_ASW_Marine*>( static_cast<C_BaseEntity*>( m_hHealEntity.Get() ) );
	bool bHealingSelf = pMarine ? (pMarine == GetMarine()) : false;

	if ( bHealingSelf && m_pDischargeEffect )
	{
		m_pDischargeEffect->StopEmission();
		m_pDischargeEffect = NULL;
	}

	switch( m_FireState )
	{
		case ASW_HG_FIRE_OFF:
		{
			if ( m_pDischargeEffect )
			{
				m_pDischargeEffect->StopEmission();
				m_pDischargeEffect = NULL;
			}
			break;
		}
		case ASW_HG_FIRE_DISCHARGE:
		{
			if ( m_pDischargeEffect && m_pDischargeEffect->GetControlPointEntity( 1 ) != NULL )
			{
				// Still attach, detach us
				m_pDischargeEffect->StopEmission();
				m_pDischargeEffect = NULL;
			}

			// don't create the effect if you are healing yourself
			if ( bHealingSelf )
				break;

			if ( !m_pDischargeEffect )
			{
				m_pDischargeEffect = ParticleProp()->Create( "heal_gun_noconnect", PATTACH_POINT_FOLLOW, "muzzle" ); 
			}

			m_pDischargeEffect->SetControlPoint( 1, m_vecHealPos );
			m_pDischargeEffect->SetControlPointOrientation( 0, GetMarine()->Forward(), -GetMarine()->Left(), GetMarine()->Up() );
			
			break;
		}
		case ASW_HG_FIRE_ATTACHED:
		{
			if ( !pMarine )
			{
				break;
			}

			if ( m_pDischargeEffect )
			{
				if ( m_pDischargeEffect->GetControlPointEntity( 1 ) != m_hHealEntity.Get() )
				{
					m_pDischargeEffect->StopEmission();
					m_pDischargeEffect = NULL;
				}
			}

			// don't create the effect if you are healing yourself
			if ( bHealingSelf )
				break;

			if ( !m_pDischargeEffect )
			{				
				m_pDischargeEffect = ParticleProp()->Create( "heal_gun_attach", PATTACH_POINT_FOLLOW, "muzzle" ); 
			}

			Assert( m_pDischargeEffect );
	
			if ( m_pDischargeEffect->GetControlPointEntity( 1 ) == NULL )
			{
				float flHeight = pMarine->BoundingRadius();
				Vector vOffset( 0.0f, 0.0f, flHeight * 0.25 );

				ParticleProp()->AddControlPoint( m_pDischargeEffect, 1, pMarine, PATTACH_ABSORIGIN_FOLLOW, NULL, vOffset );
				m_pDischargeEffect->SetControlPointOrientation( 0, GetMarine()->Forward(), -GetMarine()->Left(), GetMarine()->Up() );
			}

			break;
		}
	}
}
#endif


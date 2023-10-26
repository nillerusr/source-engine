#include "cbase.h"
#ifdef CLIENT_DLL
	#include "c_asw_player.h"
	#include "c_asw_marine.h"
	#include "c_asw_game_resource.h"
	#include "c_asw_alien.h"
	#include "c_asw_weapon.h"
	#include "engine/IVDebugOverlay.h"
	#include "shake.h"
	#include "ivieweffects.h"
	#include "asw_input.h"
	#include "prediction.h"
	#define CASW_Player C_ASW_Player
	#define CASW_Marine C_ASW_Marine
	#define CASW_Game_Resource C_ASW_Game_Resource
#else
	#include "asw_player.h"
	#include "asw_marine.h"
	#include "asw_game_resource.h"
	#include "asw_weapon.h"
	#include "util.h"
	#include "asw_achievements.h"
	#include "asw_missile_round_shared.h"
	#include "asw_marine_resource.h"
#endif
#include "asw_gamerules.h"
#include "in_buttons.h"
#include "npcevent.h"
#include "eventlist.h"
#include "asw_shareddefs.h"
#include "asw_util_shared.h"
#include "asw_marine_profile.h"
#include "basecombatweapon_shared.h"
#include "igamemovement.h"
#include "filesystem.h"
#include "asw_melee_system.h"
#include "asw_marine_skills.h"
#include "asw_gamerules.h"
#include "asw_movedata.h"
#include "datacache/imdlcache.h"
#include "asw_weapon_blink.h"
#include "asw_weapon_jump_jet.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_DEFINE_LOGGING_CHANNEL( LOG_ASW_Melee, "ASWMelee", 0, LS_MESSAGE );
ADD_LOGGING_CHANNEL_TAG( "AlienSwarm" );
ADD_LOGGING_CHANNEL_TAG( "Melee" );
END_DEFINE_LOGGING_CHANNEL();

// TODO: Some parts of the clientside prediction are firing multiple times when you have lag.  Filter them out if it isn't the first time predicting?
//       (Sounds at least)

#define MELEE_FACING_THRESHOLD 0.7071f

// MELEE_CHARGE_SWITCH_START - time after starting a heavy attack that we begin allowing a transition to a charge attack if fire is still pressed
#define MELEE_CHARGE_SWITCH_START 0.25f

CASW_Melee_System* g_pMeleeSystem = NULL;
CASW_Melee_System* ASWMeleeSystem()
{
	if ( !g_pMeleeSystem )
	{
		g_pMeleeSystem = new CASW_Melee_System;
	}
	return g_pMeleeSystem;
}

ConVar asw_melee_debug( "asw_melee_debug", "0", FCVAR_REPLICATED, "Debugs the melee system. Set to 2 for position updates" );
ConVar asw_melee_require_contact( "asw_melee_require_contact", "0", FCVAR_REPLICATED, "Melee requires contact to transition to the next combo" );
ConVar asw_melee_lock( "asw_melee_lock", "0", FCVAR_REPLICATED, "Marine is moved to the nearest enemy when melee attacking" );
ConVar asw_melee_require_key_release( "asw_melee_require_key_release", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Melee requires key release between attacks" );
ConVar asw_melee_base_damage( "asw_melee_base_damage", "12.0", FCVAR_REPLICATED | FCVAR_CHEAT, "The melee damage that marines do at level 1 (scales up with level)" );
ConVar asw_marine_rolls( "asw_marine_rolls", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "If set, marine will do rolls when jump is pressed" );
#ifdef CLIENT_DLL
ConVar asw_melee_lock_distance( "asw_melee_lock_distance", "35", FCVAR_CHEAT, "Dist marine slides to when locked onto a melee target" );
ConVar asw_melee_lock_slide_speed( "asw_melee_lock_slide_speed", "200", FCVAR_CHEAT, "Speed at which marine slides into place when target locked in melee" );
#endif

mstudioevent_for_client_server_t *GetEventIndexForSequence( mstudioseqdesc_t &seqdesc );
void SetEventIndexForSequence( mstudioseqdesc_t &seqdesc );

int CASW_Melee_System::s_nRollAttackID = -1;
int CASW_Melee_System::s_nKnockdownForwardAttackID = -1;
int CASW_Melee_System::s_nKnockdownBackwardAttackID = -1;

CASW_Melee_System::CASW_Melee_System()
{
	LoadMeleeAttacks();
	m_bAllowNormalAnimEvents = false;
	m_bAttacksValidated = false;
}

CASW_Melee_System::~CASW_Melee_System()
{
	m_MeleeAttacks.PurgeAndDeleteElements();
}

void CASW_Melee_System::Reload()
{
	m_MeleeAttacks.PurgeAndDeleteElements();
	LoadMeleeAttacks();
	m_bAttacksValidated = false;
}

void CASW_Melee_System::LoadMeleeAttacks()
{
	KeyValues *kv = new KeyValues( "resource/melee_attacks.txt" );
	if ( kv->LoadFromFile( g_pFullFileSystem, "resource/melee_attacks.txt" ) )
	{
		KeyValues *pKeys = kv;
		while ( pKeys )
		{		
			CASW_Melee_Attack* pAttack = new CASW_Melee_Attack;
			pAttack->ApplyKeyValues( pKeys );
			m_MeleeAttacks.AddToTail( pAttack );

			pKeys = pKeys->GetNextKey();
		}							
	}
	kv->deleteThis();

	LinkUpCombos();

	CASW_Melee_System::s_nRollAttackID = GetMeleeAttackByName( "Roll" ) ? GetMeleeAttackByName( "Roll" )->m_nAttackID : -1;
	CASW_Melee_System::s_nKnockdownForwardAttackID = GetMeleeAttackByName( "KnockdownForward" ) ? GetMeleeAttackByName( "KnockdownForward" )->m_nAttackID : -1;
	CASW_Melee_System::s_nKnockdownBackwardAttackID = GetMeleeAttackByName( "KnockdownBackward" ) ? GetMeleeAttackByName( "KnockdownBackward" )->m_nAttackID : -1;
}

void CASW_Melee_System::LinkUpCombos()
{
	m_candidateMeleeAttacks.RemoveAll();
	// link up the combos
	for ( int i = 0 ; i < m_MeleeAttacks.Count() ; i++ )
	{
		m_MeleeAttacks[i]->m_nAttackID = i + 1;
		m_MeleeAttacks[i]->m_CombosFromAttacks.RemoveAll();
		m_MeleeAttacks[i]->m_pOnCollisionDoAttack = NULL;
		m_MeleeAttacks[i]->m_pForceComboAttack = NULL;

		if ( m_MeleeAttacks[i]->m_CombosFromAttackNames.Count() )
		{
			for ( int j = 0; j < m_MeleeAttacks[i]->m_CombosFromAttackNames.Count(); j++ )
			{
				for ( int k = 0 ; k < m_MeleeAttacks.Count() ; k++ )
				{
					if ( k == i )
						continue;

					if ( !Q_stricmp( m_MeleeAttacks[i]->m_CombosFromAttackNames[j], m_MeleeAttacks[k]->m_szAttackName ) )
					{
						m_MeleeAttacks[i]->m_CombosFromAttacks.AddToTail( m_MeleeAttacks[k] );
					}		
				}
			}
		}
		if ( m_MeleeAttacks[i]->m_szOnCollisionDoAttackName )
		{
			for ( int k = 0 ; k < m_MeleeAttacks.Count() ; k++ )
			{
				if ( k == i )
					continue;

				if ( !Q_stricmp( m_MeleeAttacks[i]->m_szOnCollisionDoAttackName, m_MeleeAttacks[k]->m_szAttackName ) )
				{
					m_MeleeAttacks[i]->m_pOnCollisionDoAttack = m_MeleeAttacks[k];
				}		
			}
		}
		if ( m_MeleeAttacks[i]->m_szForceComboAttackName )
		{
			for ( int k = 0; k < m_MeleeAttacks.Count() ; k++ )
			{
				if ( k == i )
					continue;

				if ( !Q_stricmp( m_MeleeAttacks[i]->m_szForceComboAttackName, m_MeleeAttacks[k]->m_szAttackName ) )
				{
					m_MeleeAttacks[i]->m_pForceComboAttack = m_MeleeAttacks[k];
				}
			}
		}

	}
}

void CASW_Melee_System::ValidateMeleeAttacks( CASW_Marine *pMarine )
{
	for ( int i = m_MeleeAttacks.Count() - 1; i >= 0; --i )
	{
		if ( pMarine->LookupSequence( m_MeleeAttacks[i]->m_szSequenceName ) == -1 )
		{
			ASW_MEL_WAR( "Sequence %s not found for melee attack %s\n", m_MeleeAttacks[i]->m_szSequenceName, m_MeleeAttacks[i]->m_szAttackName );
			
			// TODO: Fix this - not all marine models have the same attacks, so removing entries from the list when they're invalid will result in mismatched lists
			//CASW_Melee_Attack *pInvalidAttack = m_MeleeAttacks[i];
			//m_MeleeAttacks.Remove( i );
			//delete pInvalidAttack;
		}
	}

	LinkUpCombos();
}


static Animevent s_PredictedAnimEvents[]={
	AE_MELEE_DAMAGE,
	AE_MELEE_START_COLLISION_DAMAGE,
	AE_MELEE_STOP_COLLISION_DAMAGE,
	AE_START_DETECTING_COMBO,
	AE_STOP_DETECTING_COMBO,
	AE_COMBO_TRANSITION,
	AE_ALLOW_MOVEMENT,
	AE_CL_CREATE_PARTICLE_EFFECT,
	AE_CL_STOP_PARTICLE_EFFECT,
	AE_CL_ADD_PARTICLE_EFFECT_CP,
	AE_CL_PLAYSOUND,
	AE_SKILL_EVENT,
};

// player has pressed melee attack key
void CASW_Melee_System::ProcessMovement( CASW_Marine *pMarine, CMoveData *pMoveData )
{
	if ( !pMarine )
	{
		return;
	}

	CASW_MoveData *pASWMove = static_cast<CASW_MoveData*>( pMoveData );

	if ( !m_bAttacksValidated )
	{
		ValidateMeleeAttacks( pMarine );
		m_bAttacksValidated = true;
	}

	//if ( pMarine->IsMeleeInhibited() )
	//{
		//return;
	//}

	//CASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
#ifdef MELEE_CHARGE_ATTACKS
	int nMeleeButton = MELEE_BUTTON;
	int nAltPressed = nMeleeButton & ((pMoveData->m_nOldButtons ^ pMoveData->m_nButtons) & pMoveData->m_nButtons);

	if ( nAltPressed || !(pMoveData->m_nOldButtons & nMeleeButton) )
	{
		pMarine->m_flMeleeHeavyKeyHoldStart = gpGlobals->curtime;	
	}

	if ( (pMoveData->m_nButtons & nMeleeButton) &&
		gpGlobals->curtime >= (pMarine->m_flMeleeHeavyKeyHoldStart + MELEE_CHARGE_SWITCH_START) )
	{
		pMarine->m_bMeleeHeavyKeyHeld = true;
	}
	else
	{
		pMarine->m_bMeleeHeavyKeyHeld = false;
	}
	int nButtonsActivated = (pMoveData->m_nButtons ^ pMoveData->m_nOldButtons) & pMoveData->m_nOldButtons;		// in charge attack mode, we have to initiate melee on mouse up
#else
	int nButtonsActivated = (pMoveData->m_nButtons ^ pMoveData->m_nOldButtons) & pMoveData->m_nButtons;
#endif

	CASW_Melee_Attack *pAttack = pMarine->GetCurrentMeleeAttack();
	bool bStumbling = ( pAttack &&
			( !Q_stricmp( pAttack->m_szAttackName, "StumbleForward" ) ||
			  !Q_stricmp( pAttack->m_szAttackName, "StumbleRightward" ) ||
			  !Q_stricmp( pAttack->m_szAttackName, "StumbleBackward" ) ||
			  !Q_stricmp( pAttack->m_szAttackName, "StumbleLeftward" ) ||
			  !Q_stricmp( pAttack->m_szAttackName, "StumbleShortForward" ) ||
			  !Q_stricmp( pAttack->m_szAttackName, "StumbleShortRightward" ) ||
			  !Q_stricmp( pAttack->m_szAttackName, "StumbleShortLeftward" ) ||
			  !Q_stricmp( pAttack->m_szAttackName, "StumbleShortBackward" )
			)
		);
	bool bKnockedDown = ( pAttack && 
		( !Q_stricmp( pAttack->m_szAttackName, "KnockdownForward" ) ||
		!Q_stricmp( pAttack->m_szAttackName, "KnockdownBackward" ) ) );
	bool bTryForcedAction = ( !bKnockedDown && ( !bStumbling || pASWMove->m_iForcedAction == FORCED_ACTION_KNOCKDOWN_FORWARD || pASWMove->m_iForcedAction == FORCED_ACTION_KNOCKDOWN_BACKWARD ) );
	if ( bTryForcedAction && pASWMove->m_iForcedAction != 0 )
	{
		if ( pMarine->CanDoForcedAction( pASWMove->m_iForcedAction ) )
		{
			pAttack = NULL;
			switch( pASWMove->m_iForcedAction )
			{
				case FORCED_ACTION_STUMBLE_FORWARD: pAttack = GetMeleeAttackByName( "StumbleForward" ); break;
				case FORCED_ACTION_STUMBLE_RIGHT: pAttack = GetMeleeAttackByName( "StumbleRightward" ); break;
				case FORCED_ACTION_STUMBLE_LEFT: pAttack = GetMeleeAttackByName( "StumbleLeftward" ); break;
				case FORCED_ACTION_STUMBLE_BACKWARD: pAttack = GetMeleeAttackByName( "StumbleBackward" ); break;
				case FORCED_ACTION_STUMBLE_SHORT_FORWARD: pAttack = GetMeleeAttackByName( "StumbleShortForward" ); break;
				case FORCED_ACTION_STUMBLE_SHORT_RIGHT: pAttack = GetMeleeAttackByName( "StumbleShortRightward" ); break;
				case FORCED_ACTION_STUMBLE_SHORT_LEFT: pAttack = GetMeleeAttackByName( "StumbleShortLeftward" ); break;
				case FORCED_ACTION_STUMBLE_SHORT_BACKWARD: pAttack = GetMeleeAttackByName( "StumbleShortBackward" ); break;
				case FORCED_ACTION_KNOCKDOWN_FORWARD: pAttack = GetMeleeAttackByName( "KnockdownForward" ); break;
				case FORCED_ACTION_KNOCKDOWN_BACKWARD: pAttack = GetMeleeAttackByName( "KnockdownBackward" ); break;
				case FORCED_ACTION_CHAINSAW_SYNC_KILL: pAttack = GetMeleeAttackByName( "SyncKillChainsaw" ); break;
				case FORCED_ACTION_BLINK:
					{
						CASW_Weapon *pWeapon = pMarine->GetASWWeapon( ASW_INVENTORY_SLOT_EXTRA );
						if ( pWeapon && pWeapon->Classify() == CLASS_ASW_BLINK )
						{
							CASW_Weapon_Blink *pBlink = assert_cast<CASW_Weapon_Blink*>( pWeapon );
							pBlink->DoBlink();
						}
					}
					break;
				case FORCED_ACTION_JUMP_JET:
					{
						CASW_Weapon *pWeapon = pMarine->GetASWWeapon( ASW_INVENTORY_SLOT_EXTRA );
						if ( pWeapon && pWeapon->Classify() == CLASS_ASW_JUMP_JET )
						{
							CASW_Weapon_Jump_Jet *pJet = assert_cast<CASW_Weapon_Jump_Jet*>( pWeapon );
							pJet->DoJumpJet();
						}
					}
					break;
			}
			if ( pAttack )
			{
	#ifdef CLIENT_DLL
				ScreenShake_t shake;

				shake.command	= SHAKE_START;
				shake.amplitude = 20.0f;
				shake.frequency = 750.f;
				shake.duration	= 0.5f;

				if ( pASWMove->m_iForcedAction >= FORCED_ACTION_STUMBLE_SHORT_FORWARD
					&& pASWMove->m_iForcedAction <= FORCED_ACTION_STUMBLE_SHORT_BACKWARD )
				{
					shake.amplitude = 10.0f;
					shake.duration = 0.25f;
				}

				//HACK_GETLOCALPLAYER_GUARD( "ASW_ShakeAnimEvent" );

				static float s_flLastShakeTime = 0.0f;
				if ( Plat_FloatTime() > s_flLastShakeTime + 3.0f )
				{
					if ( !( prediction && prediction->InPrediction() && !prediction->IsFirstTimePredicted() ) )
					{
						GetViewEffects()->Shake( shake );
					}
					s_flLastShakeTime = Plat_FloatTime();
				}
	#endif
				StartMeleeAttack( pAttack, pMarine, pMoveData );

				if ( pASWMove->m_iForcedAction == FORCED_ACTION_KNOCKDOWN_FORWARD )
				{
					pMarine->m_flMeleeYaw = pMarine->m_flKnockdownYaw;
					pMarine->m_bFaceMeleeYaw = true;
				}
				else if ( pASWMove->m_iForcedAction == FORCED_ACTION_KNOCKDOWN_BACKWARD )
				{
					pMarine->m_flMeleeYaw = pMarine->m_flKnockdownYaw + 180;
					pMarine->m_bFaceMeleeYaw = true;
				}
			}
		}
		pMarine->ClearForcedActionRequest();
	}
	else if ( gpGlobals->curtime >= pMarine->m_fNextMeleeTime )
	{
		int nMeleeButton = MELEE_BUTTON;

		bool bMeleePressed = ( ( pMoveData->m_nButtons & nMeleeButton ) || ( nButtonsActivated & nMeleeButton ) );
		//bool bAttackPressedWithMeleeWeapon = ( ( pMoveData->m_nButtons & IN_ATTACK ) && pWeapon && pWeapon->ASWIsMeleeWeapon() && !pMarine->IsFiringInhibited() );
		bool bAttackPressedWithMeleeWeapon = false;
		if ( ( pMoveData->m_nButtons & nMeleeButton ) && !( pMoveData->m_nOldButtons & nMeleeButton ) )
		{
			CASW_Player *pPlayer = pMarine->GetCommander();
			if ( pPlayer )
			{
				pMarine->m_iUsableItemsOnMeleePress = pPlayer->GetNumUseEntities();
			}
		}

		// if we're using the USE key to melee, then don't melee when there are usable entities nearby
		if ( bMeleePressed && nMeleeButton == IN_USE )
		{
			CASW_Player *pPlayer = pMarine->GetCommander();
			if ( pPlayer && pPlayer->GetNumUseEntities() > 0 )
			{
				bMeleePressed = false;
			}
		}
		
		if ( bMeleePressed || bAttackPressedWithMeleeWeapon )
		{
			OnMeleePressed( pMarine, pMoveData );
		}
		if ( pMoveData->m_nButtons & IN_JUMP )
		{
			OnJumpPressed( pMarine, pMoveData );
		}
	}

	if ( pMarine && pMarine->GetCurrentMeleeAttack() )
	{
		SetupMeleeMovement( pMarine, pMoveData );
	}
}

void CASW_Melee_System::OnMeleePressed( CASW_Marine *pMarine, CMoveData *pMoveData )
{
	if ( !pMarine || !pMoveData || !pMarine->GetMarineProfile() )
		return;

	CASW_Melee_Attack *pCurrentAttack = pMarine->GetCurrentMeleeAttack();
	if ( pCurrentAttack )
	{
		if ( pMarine->m_bMeleeComboKeypressAllowed && ( !asw_melee_require_key_release.GetBool() || pMarine->m_bMeleeKeyReleased ) )
		{
			// the player has pressed melee attack again at the right time, when the combo transition event occurs, the next attack will start
			if ( asw_melee_debug.GetInt() == 3 )
			{
				Msg( "%s:%f m_bMeleeComboKeyPressed set to true\n", pMarine->IsServer() ? "s" : "   c", gpGlobals->curtime );
			}
			pMarine->m_bMeleeComboKeyPressed = true;
		}

		return;
	}

	// Check to see if we should transition to a charge combo
#ifdef MELEE_CHARGE_ATTACKS
	if ( pMarine->m_bMeleeHeavyKeyHeld )
	{
		if ( asw_melee_debug.GetInt() == 3 )
		{
			Msg( "%s:%f m_bMeleeChargeActivate set to true\n", pMarine->IsServer() ? "s" : "   c", gpGlobals->curtime );
		}
		pMarine->m_bMeleeChargeActivate = true;
	}
#endif

	UpdateCandidateMeleeAttacks( pMarine, pMoveData );
	if ( m_candidateMeleeAttacks.Count() <= 0 )
		return;
	
	// count how many attacks at the end of the list have the same priority
	float flHighestPriority = m_candidateMeleeAttacks.Tail()->m_flPriority;
	int iCount = 0;
	for ( int i = m_candidateMeleeAttacks.Count() - 1; i >= 0; i-- )
	{
		if ( m_candidateMeleeAttacks[i]->m_flPriority == flHighestPriority )
		{
			iCount++;
		}
	}
	StartMeleeAttack( m_candidateMeleeAttacks[ SharedRandomInt( "MeleeChoice", m_candidateMeleeAttacks.Count() - iCount, m_candidateMeleeAttacks.Count() - 1 ) ], pMarine, pMoveData );
}

// do rolls when jump is pressed
void CASW_Melee_System::OnJumpPressed( CASW_Marine *pMarine, CMoveData *pMoveData )
{
	if ( !pMarine || !pMoveData || !pMarine->GetMarineProfile() || !ASWGameRules() )
		return;

	if ( !asw_marine_rolls.GetBool() )
		return;

	// no rolling if in the middle of an attack
	if ( pMarine->GetCurrentMeleeAttack() )
		return;

	CASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
	if ( pWeapon )
	{
		pWeapon->OnStartedRoll();
	}
	
	StartMeleeAttack( GetMeleeAttackByID( CASW_Melee_System::s_nRollAttackID ), pMarine, pMoveData );
	QAngle angRollDir = vec3_angle;
	if ( pMoveData->m_flSideMove == 0.0f && pMoveData->m_flForwardMove == 0.0f )
	{
		pMarine->m_flMeleeYaw = pMarine->ASWEyeAngles()[ YAW ];
	}
	else
	{
		pMarine->m_flMeleeYaw = RAD2DEG(atan2(-pMoveData->m_flSideMove, pMoveData->m_flForwardMove)) + ASWGameRules()->GetTopDownMovementAxis()[YAW];	// assumes 45 degree cam!
	}
	pMarine->m_bFaceMeleeYaw = true;

	// see if we just dodged any ranger shots
#ifdef GAME_DLL
	CASW_Player *pPlayer = pMarine->GetCommander();
	if ( pPlayer && pMarine->IsInhabited() )
	{
		const float flNearby = 150.0f;
		const float flNearbySqr = flNearby * flNearby;
		int nCount = g_vecMissileRounds.Count();
		for ( int i = 0; i < nCount; i++ )
		{
			if ( pMarine->GetAbsOrigin().DistToSqr( g_vecMissileRounds[i]->GetAbsOrigin() ) <= flNearbySqr )
			{
				pPlayer->AwardAchievement( ACHIEVEMENT_ASW_DODGE_RANGER_SHOT );
				if ( pMarine->GetMarineResource() )
				{
					pMarine->GetMarineResource()->m_bDodgedRanger = true;
				}
			}
		}
	}
#endif
}

void CASW_Melee_System::UpdateCandidateMeleeAttacks( CASW_Marine *pMarine, CMoveData *pMoveData )
{
	// build a list of melee attacks that can be triggered given our conditions and controls
	m_candidateMeleeAttacks.RemoveAll();
	int iMeleeSkill = 0;

	CASW_Melee_Attack *pCurrentAttack = pMarine->GetCurrentMeleeAttack();
#ifdef MELEE_CHARGE_ATTACKS
	int nButtonsActivated = (pMoveData->m_nButtons ^ pMoveData->m_nOldButtons) & pMoveData->m_nOldButtons;	// in charge attack mode, we have to initiate melee on mouse up
#else
	int nButtonsActivated = (pMoveData->m_nButtons ^ pMoveData->m_nOldButtons) & pMoveData->m_nButtons;
#endif

	for ( int i = 0 ; i < m_MeleeAttacks.Count() ; i++ )
	{
		CASW_Melee_Attack *pAttack = m_MeleeAttacks[i];
		if ( pAttack->m_iMinMeleeSkill > iMeleeSkill )
			continue;
		if ( pAttack->m_iMaxMeleeSkill < iMeleeSkill )
			continue;

		// Require melee-weapon specific attacks if using a melee weapon
		//if ( pCurrentMeleeWeapon && !pAttack->m_szActiveWeapon )
			//continue;
		
#ifdef MELEE_CHARGE_ATTACKS
		// Allow heavy->charge transitions to override currently playing melee attacks
		if ( !pAttack->CanComboFrom( pCurrentAttack ) && 
			(!pMarine->m_bMeleeHeavyKeyHeld || (pCurrentAttack && pCurrentAttack->m_AttackType != ASW_MA_HEAVY)) )
			continue;
#else
		if ( !pAttack->CanComboFrom( pCurrentAttack ) )
			continue;
#endif

		if ( pAttack->m_MarineClass != MARINE_CLASS_UNDEFINED && pAttack->m_MarineClass != pMarine->GetMarineProfile()->GetMarineClass() )
			continue;

		if ( pAttack->m_AttackType == ASW_MA_LIGHT && !(pMoveData->m_nButtons & IN_ATTACK) )
			continue;

		int nMeleeButton = MELEE_BUTTON;

		if ( pAttack->m_AttackType == ASW_MA_HEAVY )
		{
			if ( !(nButtonsActivated & nMeleeButton) )
				continue;
			if ( pMarine->m_iUsableItemsOnMeleePress > 0 && nMeleeButton == IN_USE )			// don't do regular melee if there was a usable item on the ground when we pressed the key down
				continue;
		}
#ifdef MELEE_CHARGE_ATTACKS
		if ( pAttack->m_AttackType == ASW_MA_CHARGE && !pMarine->m_bMeleeChargeActivate ) 
			continue;

		// Check if charge release attacks have charged enough
		if ( pAttack->m_AttackType == ASW_MA_CHARGE_RELEASE && 
			( (gpGlobals->curtime < (pMarine->m_flMeleeHeavyKeyHoldStart + pAttack->m_flRequiredChargeTime)) || !pCurrentAttack || pCurrentAttack->m_AttackType != ASW_MA_CHARGE) )
		{
			continue;
		}
#endif

		//if ( pAttack->m_AttackType == ASW_MA_CHARGE_RELEASE && 
		//	(!pCurrentAttack || pCurrentAttack->m_AttackType != ASW_MA_CHARGE ||  
		//	(gpGlobals->curtime < (pMarine->m_flMeleeHeavyKeyHoldStart + pAttack->m_flRequiredChargeTime)) ) )
		//{
		//	continue;
		//}
				
		if ( pAttack->m_ControlDirection != ASW_CD_ANY )
		{
			if ( pAttack->m_ControlDirection == ASW_CD_NONE && ( pMoveData->m_flForwardMove != 0 || pMoveData->m_flSideMove != 0 ) )
				continue;

			// build a vector pointing in the direction of our keypresses
			Vector vecKeyDir = vec3_origin;
			vecKeyDir.y += pMoveData->m_flForwardMove;
			vecKeyDir.x += pMoveData->m_flSideMove;
			VectorNormalize( vecKeyDir );

			// find dot product of key direction and marine facing
			QAngle angFacing = vec3_angle;
			angFacing.y = pMoveData->m_vecViewAngles.y;
			Vector vecFacing;
			AngleVectors( angFacing, &vecFacing );
			float flFacingDot = vecFacing.Dot( vecKeyDir );

			if ( pAttack->m_ControlDirection == ASW_CD_FORWARD && flFacingDot <= MELEE_FACING_THRESHOLD )
				continue;
			if ( pAttack->m_ControlDirection == ASW_CD_BACK && flFacingDot >= -MELEE_FACING_THRESHOLD )
				continue;

			// find sideways dot product to check strafing
			angFacing.y -= 90;
			AngleVectors( angFacing, &vecFacing );
			float flSideDot = vecFacing.Dot( vecKeyDir );
			if ( pAttack->m_ControlDirection == ASW_CD_LEFT && flSideDot >= -MELEE_FACING_THRESHOLD )
				continue;
			if ( pAttack->m_ControlDirection == ASW_CD_RIGHT && flSideDot <= MELEE_FACING_THRESHOLD )
				continue;
		}

		if ( pAttack->m_szActiveWeapon )
		{
			CBaseCombatWeapon *pWeapon = pMarine->GetActiveWeapon();
			if ( !pWeapon )
				continue;

			if ( Q_stricmp( pWeapon->GetClassname(), pAttack->m_szActiveWeapon ) )
				continue;
		}
		
		m_candidateMeleeAttacks.Insert( pAttack );
	}

	//if ( pCurrentMeleeWeapon )
	//{
		//pCurrentMeleeWeapon->OnMeleeAttackAvailable( m_candidateMeleeAttacks );
	//}
}

void CASW_Melee_System::StartMeleeAttack( CASW_Melee_Attack *pAttack, CASW_Marine *pMarine, CMoveData *pMoveData, float flBaseMeleeDamage )
{
	if ( !pMarine )
		return;

	if ( !pAttack )
		return;
	if ( flBaseMeleeDamage == -1 )
	{
		//CASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
		bool bIsMeleeWeapon = false; //pWeapon ? pWeapon->ASWIsMeleeWeapon() : false;

		if ( !bIsMeleeWeapon )
		{
			pMarine->m_flBaseMeleeDamage = asw_melee_base_damage.GetFloat();

			CASW_Marine_Profile *pProfile = pMarine->GetMarineProfile();
			if ( pProfile )
			{
				int iMarineLevel = 1; //pProfile->GetLevel();
				pMarine->m_flBaseMeleeDamage = pMarine->m_flBaseMeleeDamage + pMarine->m_flBaseMeleeDamage * 0.15f * ( iMarineLevel - 1 );	// +15% damage for every level above 1, similar to drone health
			}
		}
		else
		{
			//pMarine->m_flBaseMeleeDamage = pWeapon->m_flDamage * g_pGameRules->GetDamageMultiplier();
		}
	}
	else
	{
		pMarine->m_flBaseMeleeDamage = flBaseMeleeDamage;
	}
//	CASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
// 	if ( pWeapon && pWeapon->GetAttributeContainer()->GetItem() )
// 	{
// 		CASWScriptCreatedItem *pItem = static_cast<CASWScriptCreatedItem*>( pWeapon->GetAttributeContainer()->GetItem() );
// 		if ( pItem->IsMeleeWeapon() )
// 		{
// 			pMarine->m_flBaseMeleeDamage += pWeapon->GetWeaponDamage();
// 		}
// 	
// 		// mod the damage if the weapon has a melee damage increasing attribute
// 		float flModdedDamage = pMarine->m_flBaseMeleeDamage;
// 		if ( asw_melee_debug.GetBool() )
// 			Msg( "%s:%f Pre-modded melee damage: %d\n", IsServerDll() ? "S" : "C", gpGlobals->curtime, flModdedDamage );
// 
// 		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flModdedDamage, mod_melee_damage );
// 		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flModdedDamage, mod_melee_damage_scale );
// 
// 		if ( asw_melee_debug.GetBool() )	
// 			Msg( "%s:%f Post-modded melee damage: %d\n", IsServerDll() ? "S" : "C", gpGlobals->curtime, flModdedDamage );
// 
// 		pMarine->m_flBaseMeleeDamage = flModdedDamage;
// 	}

	if ( asw_melee_debug.GetBool() )
	{
		Msg( "%s:%f Playing melee attack %s (seq: %s)\n", IsServerDll() ? "S" : "C", gpGlobals->curtime, pAttack->m_szAttackName, pAttack->m_szSequenceName );
	}
	int iSequence = pMarine->LookupSequence( pAttack->m_szSequenceName );
	if ( iSequence < 0 )
	{
		Assert( false );
		return;
	}

	//CASW_Melee_Attack *pPrevAttack = GetMeleeAttackByID( pMarine->m_iMeleeAttackID );

	pMarine->m_iMeleeAttackID = pAttack->m_nAttackID;
	if ( pMoveData && pMoveData->m_bFirstRunOfFunctions )
	{
		pMarine->DoAnimationEvent( (PlayerAnimEvent_t) ( PLAYERANIMEVENT_MELEE + pAttack->m_nAttackID - 1 ) );
	}
	// set m_fNextMeleeTime (prevents attacking again too soon)
	//float flDuration = pMarine->SequenceDuration( iSequence );
	//pMarine->m_fNextMeleeTime = gpGlobals->curtime + flDuration;	// TODO: Revisit to allow combos
	pMarine->m_vecMeleeStartPos = pMarine->GetAbsOrigin();
	pMarine->m_bFaceMeleeYaw = false;
	pMarine->m_flMeleeStartTime  = gpGlobals->curtime;
	pMarine->m_flMeleeYaw = pMoveData ? pMoveData->m_vecViewAngles.y : pMarine->GetAbsAngles()[ YAW ];
	pMarine->m_flMeleeLastCycle = 0;
	pMarine->m_bMeleeCollisionDamage = false;
	pMarine->m_bMeleeComboKeypressAllowed = false;
	pMarine->m_bMeleeComboKeyPressed = false;
	pMarine->m_bMeleeComboTransitionAllowed = false;
	pMarine->m_bMeleeMadeContact = false;
	pMarine->m_iMeleeAllowMovement = pAttack->m_iAllowMovement;
	pMarine->m_bMeleeKeyReleased = false;
	pMarine->m_bMeleeChargeActivate = false;
	pMarine->m_RecentMeleeHits.RemoveAll();
	pMarine->m_bPlayedMeleeHitSound = false;
	//pMarine->m_bReflectingProjectiles = true;

	// search through the chosen sequence for events we want to predict (such as combo timing and damage)
	pMarine->m_iNumPredictedEvents = 0;
	CStudioHdr *pStudioHdr = pMarine->GetModelPtr();
	if ( !pStudioHdr )
		return;
	mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( iSequence );
	if ( seqdesc.numevents > 0 )
	{
		SetEventIndexForSequence( seqdesc );
		mstudioevent_t *pEvent = GetEventIndexForSequence( seqdesc );
		int num_events = NELEMS( s_PredictedAnimEvents );
		for ( int i = 0 ; i < (int) seqdesc.numevents ; i++ )
		{
			if ( ! (pEvent[i].type & AE_TYPE_NEWEVENTSYSTEM ) )
				continue;

			int nEvent = pEvent[i].Event();

			for ( int k = 0 ; k < num_events ; k++ )
			{
				if ( nEvent == s_PredictedAnimEvents[k] )
				{
					if ( pEvent[i].cycle == 0 )		// if the animation event is on the first frame, then trigger it now
					{
						if ( pMoveData->m_bFirstRunOfFunctions ||
							nEvent == AE_START_DETECTING_COMBO ||
							nEvent == AE_STOP_DETECTING_COMBO ||
							nEvent == AE_COMBO_TRANSITION ||
							nEvent == AE_ALLOW_MOVEMENT )
						{
							m_bAllowNormalAnimEvents = true;
							pMarine->HandlePredictedAnimEvent( nEvent, pEvent[i].pszOptions() );
							m_bAllowNormalAnimEvents = false;
						}
						break;
					}
					pMarine->m_iPredictedEvent[pMarine->m_iNumPredictedEvents] = nEvent;
					pMarine->m_flPredictedEventTime[pMarine->m_iNumPredictedEvents] = pEvent[i].cycle;
					pMarine->m_szPredictedEventOptions[pMarine->m_iNumPredictedEvents] = pEvent[i].pszOptions();
					pMarine->m_iNumPredictedEvents++;
					break;
				}
			}
		}
	}

	//CASW_Weapon_Melee *pMeleeWeapon = dynamic_cast<CASW_Weapon_Melee*>(pMarine->GetActiveASWWeapon());
	//if ( pMeleeWeapon )
	//{
		//pMeleeWeapon->OnMeleeAttackBegin( pAttack, pPrevAttack );
	//}

#ifdef CLIENT_DLL
	pMarine->m_hMeleeLockTarget = NULL;
	FindMeleeLockTarget( pMarine );
#endif
}

extern ConVar sv_maxvelocity;
void CASW_Melee_System::SetupMeleeMovement( CASW_Marine *pMarine, CMoveData *pMoveData )
{
	CASW_Melee_Attack *pAttack = pMarine->GetCurrentMeleeAttack();
	if ( !pAttack )
		return;	
#ifdef CLIENT_DLL
	if ( !pMarine->m_hMeleeLockTarget.Get() )
	{
		FindMeleeLockTarget( pMarine );
	}
	if ( pMarine->m_hMeleeLockTarget.Get() && asw_melee_debug.GetBool() )
	{
		debugoverlay->AddLineOverlay( pMarine->GetAbsOrigin(), pMarine->m_hMeleeLockTarget->GetAbsOrigin(), 255, 128, 0, true, 0.05f );
	}
#endif

	int nMeleeButton = MELEE_BUTTON;

	if ( !( pMoveData->m_nButtons & nMeleeButton ) )
	{
		pMarine->m_bMeleeKeyReleased = true;
	}

	if ( !pMarine->m_bMeleeMadeContact && pMoveData->m_nButtons & IN_MELEE_CONTACT )
	{
		pMarine->m_bMeleeMadeContact = true;
	}

	// find out the position we should be in by now, relative to our melee animation
	Vector vecDeltaPos;
	QAngle vecDeltaAngles;
	int iMiscSequence = pMarine->LookupSequence( pAttack->m_szSequenceName );

	//CASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
	float flSpeedScale = pAttack->m_flSpeedScale;
// 	if ( pWeapon )
// 	{
// 		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, flSpeedScale, mod_melee_speed );
// 	}
// 	else
// 	{
// 		CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pMarine, flSpeedScale, mod_melee_speed );
// 	}

	float flMiscDuration = pMarine->SequenceDuration( iMiscSequence ) * flSpeedScale;
	float flMiscCycle = clamp( ( gpGlobals->curtime - pMarine->m_flMeleeStartTime ) / flMiscDuration, 0.0f, 1.0f );
	static int iOutputNum = 0;
	iOutputNum++;
	
	if ( pMarine->m_PlayerAnimState )
	{
		pMarine->m_PlayerAnimState->SetMiscPlaybackRate( 1.0f / pAttack->m_flSpeedScale );
	}
	
	if ( asw_melee_debug.GetInt() == 2 )
	{
		Msg( "%s %d iMiscSequence = %d %f %f yaw %f\n", pMarine->IsServer() ? "s" : "                                                     c", iOutputNum, iMiscSequence, flMiscCycle, flMiscDuration, pMarine->m_flMeleeYaw );
	}
	if ( iMiscSequence <= 0 )
	{
		Msg( "Aborting melee attack as couldn't find sequence for it\n" );
		OnMeleeAttackFinished( pMarine );
		return;
	}

	if ( pMarine->m_iMeleeAllowMovement == MELEE_MOVEMENT_ANIMATION_ONLY && gpGlobals->frametime > 0 )
	{
		pMarine->GetSequenceMovement( iMiscSequence, pMarine->m_flMeleeLastCycle, flMiscCycle, vecDeltaPos, vecDeltaAngles );
		VectorYawRotate( vecDeltaPos, pMarine->m_flMeleeYaw, vecDeltaPos );
		Vector vecTargetPos = pMarine->GetAbsOrigin();
		
		if ( asw_melee_lock.GetInt() < 2 )		// ignore animation movement when melee locked?
		{
			vecTargetPos += vecDeltaPos;
		}

		// add in target lock sliding
		if ( asw_melee_lock.GetBool() && pMoveData->m_nButtons & IN_MELEE_LOCK )
		{
			QAngle facing = vec3_angle;
			facing[ YAW ] = pMarine->ASWEyeAngles()[ YAW ]; //m_flMeleeYaw;
			Vector vecForward;
			AngleVectors( facing, &vecForward );
			vecTargetPos += vecForward * pMoveData->m_flForwardMove;
		}
		// set our velocity such that we move to the target position
		float flVerticalSpeed = pMoveData->m_vecVelocity.z;
		pMoveData->m_vecVelocity = ( vecTargetPos - pMarine->GetAbsOrigin() ) / gpGlobals->frametime;
		if ( pMoveData->m_vecVelocity.z == 0 )
		{
			pMoveData->m_vecVelocity.z = flVerticalSpeed;
		}

		if ( asw_melee_debug.GetBool() && ( fabs( pMoveData->m_vecVelocity[0] ) > sv_maxvelocity.GetFloat() || fabs( pMoveData->m_vecVelocity[1] ) > sv_maxvelocity.GetFloat() ) ) 
		{
			Msg( "%s high velocity %d.  Targetpos is %f units away\n", IsServerDll() ? "S" : "C", ( vecTargetPos - pMarine->GetAbsOrigin() ).Length() );
			Msg( "   forwardmove is %f frametime is %f\n", pMoveData->m_flForwardMove, gpGlobals->frametime );
#ifdef CLIENT_DLL
			debugoverlay->AddLineOverlay( pMarine->GetAbsOrigin(), vecTargetPos, 255, 0, 0, true, 10.0f );
			debugoverlay->AddTextOverlay( vecTargetPos, 10.0f, "vecTargetPos" );
			debugoverlay->AddTextOverlay( pMarine->GetAbsOrigin(), 10.0f, "marine origin" );
#endif
		}
		
		//pMoveData->m_vecVelocity += ( vecDeltaPos / gpGlobals->frametime );
		if ( asw_melee_debug.GetInt() == 2 )
		{
			Msg("%s %d  set velocity to %f %f %f\n", pMarine->IsServer() ? "s" : "                                                     c", iOutputNum, VectorExpand( pMoveData->m_vecVelocity ) );
			Msg("%s %d     pos = %f %f %f\n", pMarine->IsServer() ? "s" : "                                                     c", iOutputNum, VectorExpand( pMoveData->GetAbsOrigin() ) );
			Msg("%s %d   start = %f %f %f\n", pMarine->IsServer() ? "s" : "                                                     c", iOutputNum, VectorExpand( pMarine->m_vecMeleeStartPos ) );
			Msg("%s %d   delta = %f %f %f\n", pMarine->IsServer() ? "s" : "                                                     c", iOutputNum, VectorExpand( vecDeltaPos ) );
			Msg("%s %d  target = %f %f %f\n", pMarine->IsServer() ? "s" : "                                                     c", iOutputNum, VectorExpand( vecTargetPos ) );
		}
	}

	// check for any predicted anim events
	for ( int i = 0 ; i < pMarine->m_iNumPredictedEvents ; i++ )
	{
		if ( pMarine->m_iPredictedEvent[i] != -1 && pMarine->m_flPredictedEventTime[i] > pMarine->m_flMeleeLastCycle && pMarine->m_flPredictedEventTime[i] <= flMiscCycle )
		{
			if ( pMoveData->m_bFirstRunOfFunctions ||
				pMarine->m_iPredictedEvent[i] == AE_START_DETECTING_COMBO ||
				pMarine->m_iPredictedEvent[i] == AE_STOP_DETECTING_COMBO ||
				pMarine->m_iPredictedEvent[i] == AE_COMBO_TRANSITION ||
				pMarine->m_iPredictedEvent[i] == AE_ALLOW_MOVEMENT )
			{
				m_bAllowNormalAnimEvents = true;
				pMarine->HandlePredictedAnimEvent( pMarine->m_iPredictedEvent[i], pMarine->m_szPredictedEventOptions[i] );
				m_bAllowNormalAnimEvents = false;
			}
		}
	}

	// Check for a regular combo transition
	if ( pMarine->m_bMeleeComboKeyPressed && pMarine->m_bMeleeComboTransitionAllowed )
	{
		if ( asw_melee_debug.GetInt() == 3 )
		{
			Msg( "%s:%f Checking for m_bMeleeComboKeyPressed combo\n", pMarine->IsServer() ? "s" : "   c", gpGlobals->curtime );
		}
		bool bMadeContact = !asw_melee_require_contact.GetBool() || pMarine->m_bMeleeMadeContact;
		if ( bMadeContact && ComboTransition( pMarine, pMoveData ) )
			return;
	}

#ifdef MELEE_CHARGE_ATTACKS
	// check for transitions from charge to charge-released attacks
	if ( pAttack->m_AttackType == ASW_MA_CHARGE )
	{
		// See if any attacks are avaialble, to play ready effects, etc
		UpdateCandidateMeleeAttacks( pMarine, pMoveData );
		
		if ( !pMarine->m_bMeleeHeavyKeyHeld )
		{
			// actually transition to 		
			if ( asw_melee_debug.GetInt() == 3 )
			{
				Msg( "%s:%f Checking for !m_bMeleeHeavyKeyHeld combo\n", pMarine->IsServer() ? "s" : "   c", gpGlobals->curtime );
			}

			if ( ComboTransition( pMarine, pMoveData, false ) )
			{
				return;
			}
			else
			{
				// If we released our button, but didn't pick up any combo transitions, it means we didn't hold long enough,
				//  so force our cycle to end so we exit the current charge attack
				flMiscCycle = 1.0f;
			}
		}
	}



	// looping
	if ( pAttack->m_bIsLooping && flMiscCycle >= 1.00f && pMarine->m_bMeleeHeavyKeyHeld )
	{
		StartMeleeAttack( pAttack, pMarine, pMoveData );
		return;
	}
#endif
	// finished melee attack
	if ( flMiscCycle >= 1.0f )
	{
		if ( pAttack->m_pForceComboAttack )
		{
			StartMeleeAttack( pAttack->m_pForceComboAttack, pMarine, pMoveData );
			return;
		}
		OnMeleeAttackFinished( pMarine );
	}
	else
	{
		pMarine->m_flMeleeLastCycle = flMiscCycle;
	}
}

bool CASW_Melee_System::ComboTransition( CASW_Marine *pMarine, CMoveData *pMoveData, bool bUpdateCandidateAttacks /* = true */ )
{
	if ( bUpdateCandidateAttacks )
	{
		UpdateCandidateMeleeAttacks( pMarine, pMoveData );
	}

	if ( m_candidateMeleeAttacks.Count() <= 0 )
	{
		return false;
	}

	if ( asw_melee_debug.GetBool() )
	{
		Msg( "%s doing ComboTransition\n", pMarine->IsServer() ? "s" : "   c" );
	}

	// count how many attacks at the end of the list have the same priority
	float flHighestPriority = m_candidateMeleeAttacks.Tail()->m_flPriority;
	int iCount = 0;
	for ( int i = m_candidateMeleeAttacks.Count() - 1; i >= 0; i-- )
	{
		if ( m_candidateMeleeAttacks[i]->m_flPriority == flHighestPriority )
		{
			iCount++;
		}
	}

	// set the attack and play the animation for it
	StartMeleeAttack( m_candidateMeleeAttacks[ SharedRandomInt( "MeleeChoice", m_candidateMeleeAttacks.Count() - iCount, m_candidateMeleeAttacks.Count() - 1 ) ], pMarine, pMoveData );

	return true;
}

void CASW_Melee_System::OnMeleeAttackFinished( CASW_Marine *pMarine )
{
	Assert( pMarine );
	//CASW_Weapon_Melee *pWeapon = dynamic_cast<CASW_Weapon_Melee*>(pMarine->GetActiveASWWeapon());
	//if ( pWeapon )
	//{
		//pWeapon->OnMeleeAttackEnd( pMarine->GetCurrentMeleeAttack() );
	//}

	pMarine->m_bMeleeCollisionDamage = false;
	pMarine->m_bMeleeComboKeypressAllowed = false;
	pMarine->m_bMeleeComboKeyPressed = false;
	pMarine->m_bMeleeComboTransitionAllowed = false;
	pMarine->m_iMeleeAllowMovement = MELEE_MOVEMENT_ANIMATION_ONLY;
	pMarine->m_bMeleeChargeActivate = false;

#ifdef MELEE_CHARGE_ATTACKS
	if ( pMarine->m_bMeleeHeavyKeyHeld )
	{
		pMarine->m_bMeleeHeavyKeyHeld = false;
		pMarine->m_flMeleeHeavyKeyHoldStart = gpGlobals->curtime;
	}
#endif

	if ( asw_melee_debug.GetBool() )
	{
		Msg( "%s:%f OnMeleeFinished() for attack %s\n", pMarine->IsServer() ? "s" : "   c", gpGlobals->curtime, pMarine->GetCurrentMeleeAttack() ? pMarine->GetCurrentMeleeAttack()->m_szAttackName : "(null)" );
	}

	pMarine->m_iMeleeAttackID = 0;
	pMarine->m_bReflectingProjectiles = false;
}

// ==================================================================================================

CASW_Melee_Attack::CASW_Melee_Attack()
{
	m_nAttackID = 0;
	m_iAllowMovement = MELEE_MOVEMENT_ANIMATION_ONLY;
	m_pOnCollisionDoAttack = NULL;
	m_pForceComboAttack = NULL;
	m_ControlDirection = ASW_CD_ANY;
}

CASW_Melee_Attack::~CASW_Melee_Attack()
{
	m_CombosFromAttackNames.PurgeAndDeleteElements();
	m_CombosFromAttacks.Purge();
}

void CASW_Melee_Attack::ApplyKeyValues( KeyValues *pKeys )
{
	m_szAttackName = ASW_AllocString( pKeys->GetString( "name", "Unnamed Attack" ) );
	m_szSequenceName = ASW_AllocString( pKeys->GetString( "sequence", "Melee sequence not specified" ) );
	m_flPriority = pKeys->GetFloat( "Priority", 1.0f );

	m_iMinMeleeSkill = pKeys->GetInt( "MinMeleeSkill", 0 );
	m_iMaxMeleeSkill = pKeys->GetInt( "MaxMeleeSkill", 5 );
	m_flDamageScale = pKeys->GetFloat( "DamageScale", 1.0f );
	m_flForceScale = pKeys->GetFloat( "ForceScale", 1.0f );
	m_flTraceDistance = pKeys->GetFloat( "TraceDistance", 0.0f );
	m_flTraceHullSize = pKeys->GetFloat( "TraceHullSize", 0.0f );
	m_iAllowMovement = (ASW_Melee_Movement_t) pKeys->GetInt( "AllowMovement", 0 );
	m_bAllowRotation = pKeys->GetBool( "AllowRotation", true );
	m_vTraceAttackOffset.x = pKeys->GetFloat( "TraceAttackOffsetRight", 0.0f );
	m_vTraceAttackOffset.y = pKeys->GetFloat( "TraceAttackOffsetForward", 0.0f );
	m_vTraceAttackOffset.z = pKeys->GetFloat( "TraceAttackOffsetUp", 0.0f );
	m_flRequiredChargeTime = pKeys->GetFloat( "RequiredChargeTime", 0.0f );
	m_flSpeedScale = pKeys->GetFloat( "SpeedScale", 1.0f );
	m_flBlendIn = pKeys->GetFloat( "BlendIn", 0.0f );
	m_flBlendOut = pKeys->GetFloat( "BlendOut", 0.1f );
	m_flKnockbackForce = pKeys->GetFloat( "KnockbackForce", 0.0f );
	m_bHoldAtEnd = pKeys->GetBool( "HoldAtEnd", false );
	m_bAllowHitsBehindMarine = pKeys->GetBool( "AllowHitsBehindMarine", false );

	const char *szAttackType = pKeys->GetString( "AttackType", "Heavy" );
	if ( !Q_stricmp( szAttackType, "Light" ) )
	{
		m_AttackType = ASW_MA_LIGHT;
	}
	else if ( !Q_stricmp( szAttackType, "Heavy" ) )
	{
		m_AttackType = ASW_MA_HEAVY;
	}
	else if ( !Q_stricmp( szAttackType, "Charge" ) )
	{
		m_AttackType = ASW_MA_CHARGE;
	}
	else if ( !Q_stricmp( szAttackType, "ChargeRelease" ) )
	{
		m_AttackType = ASW_MA_CHARGE_RELEASE;
	}

	m_bIsLooping = pKeys->GetBool( "Loop", false );

	const char *szMarineClass = pKeys->GetString( "MarineClass" );
	m_MarineClass = MARINE_CLASS_UNDEFINED;
	if ( !Q_stricmp( szMarineClass, "Officer" ) )
	{
		m_MarineClass = MARINE_CLASS_NCO;
	}
	else if ( !Q_stricmp( szMarineClass, "SpecialWeapons" ) )
	{
		m_MarineClass = MARINE_CLASS_SPECIAL_WEAPONS;
	}
	else if ( !Q_stricmp( szMarineClass, "Medic" ) )
	{
		m_MarineClass = MARINE_CLASS_MEDIC;
	}
	else if ( !Q_stricmp( szMarineClass, "Tech" ) )
	{
		m_MarineClass = MARINE_CLASS_TECH;
	}

	for ( KeyValues *pKey = pKeys->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
	{
		if ( !Q_stricmp( pKey->GetName(), "CombosFrom" ) )
		{
			const char *szCombosFromAttackName = ASW_AllocString( pKey->GetString() );
			m_CombosFromAttackNames.AddToTail( szCombosFromAttackName );
		}
	}

	m_szOnCollisionDoAttackName = ASW_AllocString( pKeys->GetString( "OnCollisionDoAttack" ) );
	m_szForceComboAttackName = ASW_AllocString( pKeys->GetString( "ForceCombo" ) );

	const char *szControlDirection = pKeys->GetString( "ControlDirection", "any" );
	if ( !szControlDirection || !Q_stricmp( szControlDirection, "any" ) )
	{
		m_ControlDirection = ASW_CD_ANY;
	}
	else if ( !Q_stricmp( szControlDirection, "forward" ) )
	{
		m_ControlDirection = ASW_CD_FORWARD;
	}
	else if ( !Q_stricmp( szControlDirection, "back" ) )
	{
		m_ControlDirection = ASW_CD_BACK;
	}
	else if ( !Q_stricmp( szControlDirection, "left" ) )
	{
		m_ControlDirection = ASW_CD_LEFT;
	}
	else if ( !Q_stricmp( szControlDirection, "right" ) )
	{
		m_ControlDirection = ASW_CD_RIGHT;
	}
	else if ( !Q_stricmp( szControlDirection, "none" ) )
	{
		m_ControlDirection = ASW_CD_NONE;
	}
	m_szActiveWeapon = ASW_AllocString( pKeys->GetString( "ActiveWeapon" ) );
}

CASW_Melee_Attack* CASW_Melee_System::GetMeleeAttackByID( int iMeleeID )
{
	// melee attack IDs start at 1
	if ( iMeleeID <= 0 || iMeleeID > m_MeleeAttacks.Count() )
	{
		return NULL;
	}

	return m_MeleeAttacks[ iMeleeID - 1 ];
}

CASW_Melee_Attack* CASW_Melee_System::GetMeleeAttackByName( const char *szAttackName )
{
	for ( int i = 0; i < m_MeleeAttacks.Count(); i++ )
	{
		if ( !Q_stricmp( m_MeleeAttacks[i]->m_szAttackName, szAttackName ) )
		{
			return m_MeleeAttacks[i];
		}
	}
	return NULL;
}

void CASW_Melee_System::ComputeTraceOffset( CASW_Marine *pMarine, Vector &vecTraceOffset )
{
	CASW_Melee_Attack *pAttack = pMarine->GetCurrentMeleeAttack();

	if ( !pAttack )
	{
		return;
	}

	QAngle angles = pMarine->GetAbsAngles();
	Vector right, up, forward;
	AngleVectors( angles, &forward, &right, &up );

	vecTraceOffset += right * pAttack->m_vTraceAttackOffset.x;
	vecTraceOffset += forward * pAttack->m_vTraceAttackOffset.y;
	vecTraceOffset += up * pAttack->m_vTraceAttackOffset.z;
}


#ifdef CLIENT_DLL
void CASW_Melee_System::FindMeleeLockTarget( CASW_Marine *pMarine )
{
	CBaseEntity *pBest = NULL;
	float flBestScore = -1;
	for ( int i = 0; i < IASW_Client_Aim_Target::AutoList().Count(); i++ )
	{
		IASW_Client_Aim_Target *pAimTarget = static_cast< IASW_Client_Aim_Target* >( IASW_Client_Aim_Target::AutoList()[ i ] );
		C_BaseEntity *pEnt = pAimTarget->GetEntity();
		if ( !pEnt || !pAimTarget->IsAimTarget() )
			continue;
		
		Vector dir = pEnt->GetAbsOrigin() - pMarine->GetAbsOrigin();
		float flDist = dir.NormalizeInPlace();
		if ( flDist > 200.0f )
			continue;
		Vector vecForward;
		AngleVectors( pMarine->ASWEyeAngles(), &vecForward );
		float flDot = dir.Dot( vecForward );
		if ( flDot < 0 )
			continue;

		float flScore = ( 200.0f - flDist ) * flDot;
		if ( flScore > flBestScore )
		{
			flBestScore = flScore;
			pBest = pEnt;
		}
	}
	pMarine->m_hMeleeLockTarget = pBest;
}

void CASW_Melee_System::CreateMove( float flInputSampleTime, CUserCmd *pCmd, CASW_Marine *pMarine )
{
	// don't replace the move if our current melee attack lets us move about freely
	if ( pMarine->m_iMeleeAllowMovement != MELEE_MOVEMENT_ANIMATION_ONLY )
		return;

	if ( pMarine->m_bFaceMeleeYaw )
	{
		pCmd->viewangles[ YAW ] = pMarine->m_flMeleeYaw;
	}

	if ( asw_melee_lock.GetBool() )
	{
		// TODO: This breaks direction detection once we start melee'ing.
		pCmd->forwardmove = 0;
		pCmd->sidemove = 0;	

		// set forwardmove to the number of units we wish to move this movement tick
		C_BaseEntity *pEnemy = pMarine->m_hMeleeLockTarget.Get();
		if ( pEnemy )
		{
			Vector dir = pEnemy->GetAbsOrigin() - pMarine->GetAbsOrigin();
			float flDist = dir.NormalizeInPlace();
			Vector vecForward;
			AngleVectors( pMarine->ASWEyeAngles(), &vecForward );
			float flDot = dir.Dot( vecForward );
			if ( flDot > 0.7f )
			{
				float flIdealDist = pEnemy->CollisionProp()->BoundingRadius2D() + asw_melee_lock_distance.GetFloat();
				flDist -= flIdealDist;		// distance to our ideal spot
				
				float flSlideAmount = asw_melee_lock_slide_speed.GetFloat() * flInputSampleTime;
				if ( flDist > 0 )
				{
					flSlideAmount = MIN( flDist, flSlideAmount );
				}
				else
				{
					flSlideAmount = MAX( flDist, -flSlideAmount );
				}
				pCmd->forwardmove = flSlideAmount;
			}
			pCmd->buttons |= IN_MELEE_LOCK;
		}
	}
}
#endif

// ======================================================================================================

#ifdef CLIENT_DLL
// this does a trace with the current melee attack's size/dimensions and reports if we hit anything
bool CASW_Melee_Attack::CheckContact( CASW_Marine *pAttacker )
{
	Vector forward;
	AngleVectors( pAttacker->GetAbsAngles(), &forward );
	Vector vStart = pAttacker->GetAbsOrigin();

	Vector mins = -Vector( m_flTraceHullSize, m_flTraceHullSize, 32);
	Vector maxs = Vector( m_flTraceHullSize, m_flTraceHullSize, 32 );
	float flDist = m_flTraceDistance;

	// The ideal place to start the trace is in the center of the attacker's bounding box.
	// however, we need to make sure there's enough clearance. Some of the smaller monsters aren't 
	// as big as the hull we try to trace with. (SJB)
	float flVerticalOffset = pAttacker->WorldAlignSize().z * 0.5;

	if( flVerticalOffset < maxs.z )
	{
		// There isn't enough room to trace this hull, it's going to drag the ground.
		// so make the vertical offset just enough to clear the ground.
		flVerticalOffset = maxs.z + 1.0;
	}

	vStart.z += flVerticalOffset;
	Vector vEnd = vStart + (forward * flDist );

	// asw - make melee attacks trace below us too, so it's possible to hit things just below you on a slope
	Vector low_mins = mins;
	low_mins.z -= 30;

	Ray_t ray;
	ray.Init( vStart, vEnd, mins, maxs );

	trace_t tr;
	CTraceFilterSimple traceFilter( pAttacker, COLLISION_GROUP_NONE );
	enginetrace->TraceRay( ray, MASK_SHOT_HULL, &traceFilter, &tr );
	return tr.DidHit();
}
#endif

// marine skips anim events that are predicted by the melee system
bool CASW_Melee_Attack::AllowNormalAnimEvent( CASW_Marine *pMarine, int event )
{
	if ( ASWMeleeSystem()->m_bAllowNormalAnimEvents )
		return true;
#ifdef CLIENT_DLL
	// we don't predict other players' animation events, so allow them to fire the regular way
	if ( pMarine && pMarine->GetPredictionOwner() != C_ASW_Player::GetLocalASWPlayer() )
		return true;

	// in singleplayer there's no prediction, so allow events
	if ( gpGlobals->maxClients <= 1 )
		return true;
#endif

	int num_events = NELEMS( s_PredictedAnimEvents );

	for ( int k = 0 ; k < num_events ; k++ )
	{
		if ( event == s_PredictedAnimEvents[k] )	
			return false;
	}
	return true;
}

void CASW_Melee_Attack::MovementCollision( CASW_Marine *pMarine, CMoveData *pMoveData, trace_t *tr )
{
	if ( !tr->m_pEnt )
	{
		//Msg( "%s: Melee attack collided with nothing\n", IsServerDll() ? "S" : "C" );
		return;
	}
	//Msg( "%s: Melee attack collided with %d %s\n", IsServerDll() ? "S" : "C", tr->m_pEnt->entindex(), tr->m_pEnt->GetClassname() );
	if ( m_pOnCollisionDoAttack )
	{
		// jms: Temp for playtest, this assumes we're using the Charge skill.  Make this data driven
		CASW_Marine_Profile *pProfile = pMarine->GetMarineProfile();
		if ( !pProfile )
			return;

		pMarine->EmitSound( "ASW_Charge.ImpactBlast" );
		int iBaseDamage = 10; //pSkill->GetValue( CASW_Skill_Details::Damage, pProfile->GetMarineSkill( pSkill->m_iSkillIndex ) );		
		ASWMeleeSystem()->StartMeleeAttack( m_pOnCollisionDoAttack, pMarine, pMoveData, iBaseDamage );
//#ifdef GAME_DLL
		//float flRadius = pSkill->GetValue( CASW_Skill_Details::Radius, pProfile->GetMarineSkill( pSkill->m_iSkillIndex ) );
		//ASWGameRules()->StumbleAliensInRadius( pMarine, pMarine->GetAbsOrigin(), flRadius );
//#endif
	}
}

bool CASW_Melee_Attack::CanComboFrom( CASW_Melee_Attack *pAttack )
{
	return (m_CombosFromAttacks.Count() == 0 && pAttack == NULL) || (m_CombosFromAttacks.Find( pAttack ) != -1);
}

#ifdef GAME_DLL
typedef CCopyableUtlVector<CASW_Melee_Attack*> ASW_Melee_Attack_List_t;

void BuildAttackList_r( CUtlVector<ASW_Melee_Attack_List_t> &attackList, ASW_Melee_Attack_List_t &currentList, int nStartIndex )
{
	CASW_Melee_Attack *pAttack = ASWMeleeSystem()->GetMeleeAttackByID( nStartIndex );

	// Check for loops
	int nSelfIndex = currentList.Find( pAttack );
	if ( nSelfIndex != -1 )
	{
		return;
	}

	bool bIsLeaf = true;

	// Add ourselves to the list we're currently building
	currentList.AddToTail( pAttack );

	// Melee attack indexing starts at 1
	for ( int k = 1; k <= ASWMeleeSystem()->m_MeleeAttacks.Count(); k++ )
	{
		if ( nStartIndex == k )
		{
			continue;
		}

		CASW_Melee_Attack *pIterAttack = ASWMeleeSystem()->GetMeleeAttackByID( k );

		if( pIterAttack->CanComboFrom( pAttack ) )
		{
			bIsLeaf = false;
			BuildAttackList_r( attackList, currentList, k );
		}
	}

	if ( bIsLeaf )
	{
		// Once at a leaf node, actually add the complete attack list
		attackList.AddToTail( currentList );
	}

	currentList.Remove( currentList.Count() - 1 );
}

void BuildAttackList( CUtlVector<ASW_Melee_Attack_List_t> &attackList )
{
	// Melee attack indexing starts at 1
	for( int i = 1; i <= ASWMeleeSystem()->m_MeleeAttacks.Count(); i++ )
	{
		CASW_Melee_Attack *pAttack = ASWMeleeSystem()->GetMeleeAttackByID( i );

		// Recurse down starting at each root melee attack
		if ( pAttack->m_CombosFromAttacks.Count() == 0 )
		{
			ASW_Melee_Attack_List_t emptyList;
			BuildAttackList_r( attackList, emptyList, i );
		}
	}
}

int GetNumberOfAttacks( CStudioHdr *pStudioHdr, int nSequence )
{
	int nNumAttacks = 0;
	MDLCACHE_CRITICAL_SECTION();

	mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( nSequence );
	if ( seqdesc.numevents > 0 )
	{
		SetEventIndexForSequence( seqdesc );
		mstudioevent_t *pEvent = GetEventIndexForSequence( seqdesc );
		for ( int i = 0 ; i < (int) seqdesc.numevents ; i++ )
		{
			if ( pEvent[i].Event() == AE_MELEE_DAMAGE )
			{
				nNumAttacks++;
			}
		}
	}

	return nNumAttacks;
}

void cc_asw_melee_list_dps_f()
{
	CUtlVector<ASW_Melee_Attack_List_t> attackList;
	
	BuildAttackList( attackList );

	CASW_Player *pPlayer = ToASW_Player( UTIL_GetCommandClient() );

	if ( !pPlayer )
	{
		Warning( "Cannot get local player\n" );
		return;
	}

	CASW_Marine *pMarine = pPlayer->GetMarine();
	CStudioHdr *pStudioHdr = pMarine? pMarine->GetModelPtr() : NULL;

	if ( !pMarine || !pStudioHdr )
	{
		Warning( "Cannot get local marine\n" );
		return;
	}

	for ( int i = 0; i < attackList.Count(); i++ )
	{
		// This will take damage scale into account, so a melee attack that does 3 attacks with a damage scale of 0.5 will count as 1.5 attacks
		float flTotalAttacks = 0;
		float flAttackDuration = 0;

		if ( attackList[i].Count() == 0 )
		{
			continue;
		}

		ASW_MEL_MSG_SIMPLE( "Melee Attack Sequence (%s):\n", attackList[i][0]->m_szActiveWeapon );
		for( int j = 0; j < attackList[i].Count(); j++ )
		{
			if ( j == 0 )
			{
				ASW_MEL_MSG_SIMPLE( "\t\t" );	
			}
			else 
			{
				ASW_MEL_MSG_SIMPLE( " -> " );
			}
			ASW_MEL_MSG_SIMPLE( "%s", attackList[i][j]->m_szAttackName );

			flTotalAttacks += attackList[i][j]->m_flDamageScale * 
				GetNumberOfAttacks( pStudioHdr, pMarine->LookupSequence( attackList[i][j]->m_szSequenceName ) );
			flAttackDuration += pMarine->SequenceDuration( pMarine->LookupSequence( attackList[i][j]->m_szSequenceName ) );
		}
		ASW_MEL_MSG_SIMPLE( "\n\tThis sequence performs %f attacks over %f seconds, DPS modifier: %f\n\n", flTotalAttacks, flAttackDuration, flTotalAttacks / flAttackDuration );
	}
}
ConCommand cc_asw_melee_list_dps( "asw_melee_list_dps", cc_asw_melee_list_dps_f, "Lists DPS for melee weapons" );

#endif


void cc_asw_melee_reload_f()
{
	ASWMeleeSystem()->Reload();
#ifdef CLIENT_DLL
	engine->ClientCmd( "asw_melee_reload_server_only" );
#endif
}

#ifdef GAME_DLL
ConCommand cc_asw_melee_reload( "asw_melee_reload_server_only", cc_asw_melee_reload_f, "Reloads melee_attacks.txt" );
#else // #ifdef GAME_DLL
ConCommand cc_cl_asw_melee_reload( "asw_melee_reload", cc_asw_melee_reload_f, "Reloads melee_attacks.txt" );
#endif // #ifdef GAME_DLL

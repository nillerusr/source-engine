//=============================================================================//
//
// Purpose: 	Bad Cop, a former human bent on killing anyone who stands in his way.
//				His drive in this life is to pacify noncitizens, serve the Combine,
//				and use overly cheesy quips.
// 
// Author: 		Blixibon, 1upD
//
//=============================================================================//

#include "cbase.h"

#include "ez2_player.h"
#include "ai_squad.h"
#include "basegrenade_shared.h"
#include "in_buttons.h"
#include "npc_wilson.h"
#include "eventqueue.h"
#include "iservervehicle.h"
#include "ai_interactions.h"
#include "world.h"
#include "sceneentity.h"
#include "fmtstr.h"
#include "mapbase/info_remarkable.h"
#include "combine_mine.h"
#include "weapon_physcannon.h"
#include "saverestore_utlvector.h"
#include "grenade_satchel.h"
#include "npc_citizen17.h"
#include "npc_combine.h"
#include "point_bonusmaps_accessor.h"
#include "achievementmgr.h"
#include "npc_husk_base.h"
#include "ammodef.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar player_mute_responses( "player_mute_responses", "0", FCVAR_ARCHIVE, "Mutes the responsive Bad Cop." );
ConVar player_dummy_in_squad( "player_dummy_in_squad", "0", FCVAR_ARCHIVE, "Puts the player dummy in the player's squad, which means squadmates will see enemies the player sees." );
ConVar player_dummy( "player_dummy", "1", FCVAR_NONE, "Enables the player NPC dummy." );

ConVar player_use_instructor( "player_use_instructor", "1", FCVAR_NONE, "Enables game instructor hints instead of HL2 HUD hints" );

ConVar player_silent_surrender( "player_silent_surrender", "1" );

extern ConVar sv_bonus_challenge;

#if EZ2
LINK_ENTITY_TO_CLASS(player, CEZ2_Player);
PRECACHE_REGISTER(player);
#endif //EZ2

BEGIN_DATADESC(CEZ2_Player)
	DEFINE_FIELD(m_bInAScript, FIELD_BOOLEAN),

	DEFINE_FIELD(m_hStaringEntity, FIELD_EHANDLE),
	DEFINE_FIELD(m_flCurrentStaringTime, FIELD_TIME),

	//DEFINE_UTLVECTOR( m_SightEvents, FIELD_EMBEDDED ),

	DEFINE_FIELD( m_vecLastCommandGoal, FIELD_VECTOR ),

	DEFINE_FIELD(m_hNPCComponent, FIELD_EHANDLE),
	DEFINE_FIELD(m_flNextSpeechTime, FIELD_TIME),
	DEFINE_FIELD(m_hSpeechFilter, FIELD_EHANDLE),

	DEFINE_EMBEDDED( m_MemoryComponent ),

	DEFINE_FIELD(m_hSpeechTarget, FIELD_EHANDLE),

	// These don't need to be saved
	//DEFINE_FIELD(m_iVisibleEnemies, FIELD_INTEGER),
	//DEFINE_FIELD(m_iCloseEnemies, FIELD_INTEGER),
	//DEFINE_FIELD(m_iCriteriaAppended, FIELD_INTEGER),

	DEFINE_INPUTFUNC(FIELD_STRING, "AnswerConcept", InputAnswerConcept),

	DEFINE_INPUTFUNC(FIELD_VOID, "StartScripting", InputStartScripting),
	DEFINE_INPUTFUNC(FIELD_VOID, "StopScripting", InputStopScripting),
	
	DEFINE_INPUTFUNC(FIELD_VOID, "__FinishBonusChallenge", InputFinishBonusChallenge),
END_DATADESC()

BEGIN_ENT_SCRIPTDESC( CEZ2_Player, CBasePlayer, "E:Z2's player entity." )

	DEFINE_SCRIPTFUNC_NAMED( ScriptGetNPCComponent, "GetNPCComponent", "Gets the player's NPC component." )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetStaringEntity, "GetStaringEntity", "Gets the player's staring entity." )

	DEFINE_SCRIPTFUNC_NAMED( VScriptGetEnemy, "GetEnemy", "Gets the player's current enemy." )
	DEFINE_SCRIPTFUNC( GetVisibleEnemies, "Gets the player's visible enemies." )
	DEFINE_SCRIPTFUNC( GetCloseEnemies, "Gets the player's close enemies." )

	DEFINE_SCRIPTFUNC( IsInAScript, "Returns true if the player is in a script." )

	DEFINE_SCRIPTFUNC( HasCheated, "Returns true if the player has had cheats on during this map." )

	DEFINE_SCRIPTFUNC( HUDMaskInterrupt, "Disables HUD elements for mask cache scenes." )
	DEFINE_SCRIPTFUNC( HUDMaskRestore, "Restores disabled HUD elements for mask cache scenes." )

END_SCRIPTDESC();

IMPLEMENT_SERVERCLASS_ST(CEZ2_Player, DT_EZ2_Player)
	SendPropBool( SENDINFO( m_bBonusChallengeUpdate ) ),
	SendPropEHandle( SENDINFO( m_hWarningTarget ) ),
END_SEND_TABLE()

/*
BEGIN_SIMPLE_DATADESC( SightEvent_t )

	DEFINE_FIELD( flNextHintTime, FIELD_TIME ),
	DEFINE_FIELD( flLastHintTime, FIELD_TIME ),

	DEFINE_FIELD( flCooldown, FIELD_FLOAT ),

END_DATADESC()
*/

#define PLAYER_MIN_ENEMY_CONSIDER_DIST Square(4096)
#define PLAYER_MIN_MOB_DIST_SQR Square(192)

// How many close enemies there has to be before it's considered a "mob".
#define PLAYER_ENEMY_MOB_COUNT 4

#define SPEECH_AI_INTERVAL_IDLE 0.5f
#define SPEECH_AI_INTERVAL_ALERT 0.25f

//-----------------------------------------------------------------------------
// Purpose: Allow post-frame adjustments on the player
//-----------------------------------------------------------------------------
void CEZ2_Player::PostThink(void)
{
	if (m_flNextSpeechTime < gpGlobals->curtime) 
	{
		float flCooldown = SPEECH_AI_INTERVAL_IDLE;
		if (GetNPCComponent())
		{
			// Do some pre-speech setup based off of our state.
			switch (GetNPCComponent()->GetState())
			{
				// Speech AI runs more frequently if we're alert or in combat.
				case NPC_STATE_ALERT:
				{
					flCooldown = SPEECH_AI_INTERVAL_ALERT;
				} break;
				case NPC_STATE_COMBAT:
				{
					flCooldown = SPEECH_AI_INTERVAL_ALERT;

					// Measure enemies and cache them.
					// They're almost entirely used for speech anyway, so it makes sense to put them here.
					MeasureEnemies(m_iVisibleEnemies, m_iCloseEnemies);
				} break;
			}
		}

		// Some stuff in DoSpeechAI() relies on m_flNextSpeechTime.
		m_flNextSpeechTime = gpGlobals->curtime + flCooldown;

		DoSpeechAI();
	}

	if (GetNPCComponent())
	{
		AISightIter_t iter;
		CBaseEntity *pSightEnt = GetNPCComponent()->GetSenses()->GetFirstSeenEntity( &iter );
		if (pSightEnt)
		{
			// Go through sight events if we have entities in our vision
			for (int i = 0; i < m_SightEvents.Count(); i++)
			{
				if (m_SightEvents[i]->flNextHintTime < gpGlobals->curtime)
				{
					iter = AISightIter_t();
					pSightEnt = GetNPCComponent()->GetSenses()->GetFirstSeenEntity( &iter );
					while ( pSightEnt )
					{
						if (m_SightEvents[i]->Test( this, pSightEnt ))
						{
							DevMsg("Showing hint %s\n", m_SightEvents[i]->pName);
							m_SightEvents[i]->DoEvent( this, pSightEnt );
							m_SightEvents[i]->flLastHintTime = gpGlobals->curtime;
							m_SightEvents[i]->flNextHintTime = gpGlobals->curtime + m_SightEvents[i]->flCooldown;
							break;
						}

						pSightEnt = GetNPCComponent()->GetSenses()->GetNextSeenEntity( &iter );
					}

					if (m_SightEvents[i]->flNextHintTime < gpGlobals->curtime)
					{
						// Just check again in X seconds if we can't find what we're looking for
						m_SightEvents[i]->flNextHintTime = gpGlobals->curtime + m_SightEvents[i]->flFailedCooldown;
					}
				}
			}
		}
	}

	if (GetBonusChallenge() != EZ_CHALLENGE_NONE && !m_bMaskInterrupt && gpGlobals->curtime > 2.5f)
	{
		// Abort the challenge if we're cheating
		if (HasCheated() && developer.GetInt() == 0)
		{
			RunScript( "MapCheated()" );
		}
		else if (GetBonusChallenge() == EZ_CHALLENGE_TIME)
		{
			// Start and stay ahead 2.5 seconds after curtime
			SetBonusProgress( gpGlobals->curtime - 2.5f );
		}
	}

	BaseClass::PostThink();
}

void CEZ2_Player::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( "models/bad_cop.mdl" );
	PrecacheScriptSound( "Player.Detonate" );
	PrecacheScriptSound( "Player.DetonateFail" );

	// Health Vial Backpack
	PrecacheScriptSound("HealthVial.BackpackConsume");
	PrecacheScriptSound("HealthVial.BackpackDeny");

	if (GetBonusChallenge() == EZ_CHALLENGE_MASS || sv_bonus_challenge.GetInt() == EZ_CHALLENGE_MASS)
	{
		VerifyXenRecipeManager( GetClassname() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
bool SoldierHintTest( CEZ2_Player *pPlayer, CBaseEntity *pActivator )
{
	// Look for a commandable soldier
	// (must be a soldier and not, say, a commandable rollermine)
	CAI_BaseNPC *pNPC = pActivator->MyNPCPointer();
	CNPC_Combine *pCombine = dynamic_cast<CNPC_Combine *>(pNPC);
	if ( pCombine && pCombine->IsCommandable() && !pCombine->IsInPlayerSquad() && !pCombine->IsPlayerUseDisabled() )
		return (pPlayer->GetAbsOrigin().DistToSqr( pCombine->GetAbsOrigin()) <= Square(192.0f) && pPlayer->IRelationType( pCombine ) == D_LI);
	return false;
}

// Shows the hint when the player sees a soldier
SIGHT_EVENT_HINT_START( SoldierHintShow, "player_see_soldier" )

			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetInt( "soldier", pActivator->entindex() );
			gameeventmanager->FireEvent( event );

SIGHT_EVENT_HINT_END( "#EZ2_HudHint_Soldiers" )

// Hides the hint when the soldier joins the squad
SIGHT_EVENT_HINT_START( SoldierHintHide, "player_squad_join_soldier" )

			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetInt( "soldier", pActivator->entindex() );
			gameeventmanager->FireEvent( event );

SIGHT_EVENT_HINT_END( "" )

// Show a HUD hint for any nearby commandable soldiers
SightEvent_t g_SightHintSoldierSquad( "Soldier Squad", 50.0f, SoldierHintTest, SoldierHintShow );

//-----------------------------------------------------------------------------
bool CrateHintTest( CEZ2_Player *pPlayer, CBaseEntity *pActivator )
{
	// Look for an item crate
	if (pActivator->ClassMatches( "item_item_crate" ))
		return (pPlayer->GetAbsOrigin().DistToSqr(pActivator->GetAbsOrigin()) <= Square(192.0f));
	return false;
}

// Shows the hint when the player sees an item crate
SIGHT_EVENT_HINT_START( CrateHintShow, "player_see_crate" )

			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetInt( "crate", pActivator->entindex() );
			gameeventmanager->FireEvent( event );

SIGHT_EVENT_HINT_END( "#EZ2_HudHint_KickCrate" )

// Show a HUD hint for any nearby kickables
SightEvent_t g_SightHintCrateKick( "Crate Kick", 50.0f, CrateHintTest, CrateHintShow );

//-----------------------------------------------------------------------------
bool VehicleHintTest( CEZ2_Player *pPlayer, CBaseEntity *pActivator )
{
	// Look for a nearby vehicle we can enter
	// (only APCs for now)
	if (pActivator->ClassMatches("prop_vehicle_drivable_apc") && pActivator->GetServerVehicle() && pPlayer->CanEnterVehicle(pActivator->GetServerVehicle(), VEHICLE_ROLE_DRIVER) && !pPlayer->IsInAVehicle())
		return (pPlayer->GetAbsOrigin().DistToSqr(pActivator->GetAbsOrigin()) <= Square(384.0f));
	return false;
}

// Shows the hint when the player sees a vehicle they can enter
SIGHT_EVENT_HINT_START( VehicleHintShow, "player_see_vehicle" )

			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetInt( "vehicle", pActivator->entindex() );
			gameeventmanager->FireEvent( event );

SIGHT_EVENT_HINT_END( "#EZ2_HudHint_EnterAPC" )

// Hides the hint when the player enters the vehicle
SIGHT_EVENT_HINT_START( VehicleHintHide, "player_enter_vehicle" )

			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetInt( "vehicle", pActivator->entindex() );
			gameeventmanager->FireEvent( event );

SIGHT_EVENT_HINT_END( "" )

// Show a HUD hint for any nearby vehicles
SightEvent_t g_SightHintEnterVehicle( "Vehicle Enter", 200.0f, VehicleHintTest, VehicleHintShow );

//-----------------------------------------------------------------------------

ConVar player_hint_surrenderable_min_time_acquired( "player_hint_surrenderable_min_time_acquired", "3.5", FCVAR_NONE, "The minimum amount of time an enemy must've been the current enemy before the hint can appear" );
ConVar player_hint_surrenderable_min_time_begged( "player_hint_surrenderable_min_time_begged", "2.0", FCVAR_NONE, "The minimum amount of time an enemy must've last begged before the hint can appear" );
ConVar player_hint_surrenderable_max_time_last_seen( "player_hint_surrenderable_max_time_last_seen", "2.0", FCVAR_NONE, "The maximum amount of time since a NPC was last seen before the hint can appear" );
ConVar player_hint_surrenderable_max_dist( "player_hint_surrenderable_max_dist", "224", FCVAR_NONE, "The maximum distance a NPC must be from the player before the hint can appear" );

bool SurrenderableCitizenHintTest( CEZ2_Player *pPlayer, CBaseEntity *pActivator )
{
	// Don't do this if there's too many enemies around
	if (pPlayer->GetVisibleEnemies() > 2)
		return false;

	// Only our current enemy and if they've been an enemy for X seconds
	if (pPlayer->GetEnemy() != pActivator || (gpGlobals->curtime - pPlayer->GetNPCComponent()->GetTimeEnemyAcquired()) < player_hint_surrenderable_min_time_acquired.GetFloat())
		return false;

	if (pActivator->Classify() == CLASS_PLAYER_ALLY && pActivator->MyNPCPointer()->GetActiveWeapon() == NULL)
	{
		// Check our enemy memory; Make sure we see them
		CAI_Enemies *pEnemies = pPlayer->GetNPCComponent()->GetEnemies();
		AI_EnemyInfo_t *pEnemyInfo = pEnemies->Find( pActivator );
		if (!pEnemyInfo || (gpGlobals->curtime - pEnemyInfo->timeLastSeen) > player_hint_surrenderable_max_time_last_seen.GetFloat())
			return false;

		CNPC_Citizen *pCitizen = dynamic_cast<CNPC_Citizen*>(pActivator);
		if (pCitizen && !pCitizen->IsSurrendered() && pCitizen->CanSurrender() && !pCitizen->SurrenderAutomatically()
			&& pPlayer->GetAbsOrigin().DistToSqr( pActivator->GetAbsOrigin() ) <= Square( player_hint_surrenderable_max_dist.GetFloat() ))
		{
			// Make sure they're not about to pick up a weapon
			if (pCitizen->IsCurSchedule( SCHED_NEW_WEAPON, false ))
				return false;

			// Finally, check if the citizen has begged already
			float flTimeSpeakBeg = pCitizen->GetTimeSpokeConcept( TLK_BEG );
			if (flTimeSpeakBeg != -1 && gpGlobals->curtime - flTimeSpeakBeg >= player_hint_surrenderable_min_time_begged.GetFloat())
				return true;
		}
	}

	return false;
}

// Shows the hint when the player sees a surrenderable citizen
SIGHT_EVENT_HINT_START( SurrenderableCitizenHintShow, "player_see_surrenderable" )

			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetInt( "victim", pActivator->entindex() );
			gameeventmanager->FireEvent( event );

SIGHT_EVENT_HINT_END( "#EZ2_HudHint_Surrenderable" )

// Hides the hint when the player enters the vehicle
SIGHT_EVENT_HINT_START( SurrenderableCitizenHintHide, "player_did_surrender" )

			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetInt( "victim", pActivator->entindex() );
			gameeventmanager->FireEvent( event );

SIGHT_EVENT_HINT_END( "" )

// Show a HUD hint for surrenderable citizens
SightEvent_t g_SightHintSurrenderableCitizen( "Surrenderable Citizen", 100.0f, SurrenderableCitizenHintTest, SurrenderableCitizenHintShow );

//-----------------------------------------------------------------------------
bool SurrenderedMedicHintTest( CEZ2_Player *pPlayer, CBaseEntity *pActivator )
{
	// Don't do this if the player is in combat
	if (pPlayer->GetNPCComponent() && pPlayer->GetNPCComponent()->GetState() == NPC_STATE_COMBAT)
		return false;

	if (pActivator->Classify() == CLASS_CITIZEN_PASSIVE)
	{
		CNPC_Citizen *pCitizen = dynamic_cast<CNPC_Citizen*>(pActivator);
		if (pCitizen && pCitizen->IsSurrendered() && pCitizen->IsSurrenderIdle() &&
			pCitizen->IsMedic() && pPlayer->GetAbsOrigin().DistToSqr( pActivator->GetAbsOrigin() ) <= Square( 192.0f ))
		{
			// We have the medic, but make sure that we need health in the first place
			if ((pPlayer->GetMaxHealth() - pPlayer->GetHealth()) >= 15)
				return true;
		}
	}

	return false;
}

// Shows the hint when the player sees a surrendered medic
SIGHT_EVENT_HINT_START( SurrenderedMedicHintShow, "player_see_surrendered_medic" )

			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetInt( "medic", pActivator->entindex() );
			gameeventmanager->FireEvent( event );

SIGHT_EVENT_HINT_END( "#EZ2_HudHint_SurrenderedMedic" )

// Show a HUD hint for surrendered medics who can heal the player
SightEvent_t g_SightHintSurrenderedMedic( "Surrendered Medic", 100.0f, SurrenderedMedicHintTest, SurrenderedMedicHintShow );

//-----------------------------------------------------------------------------

ConVar player_smg_grenade_warning_cone( "player_smg_grenade_warning_cone", ".4" );
ConVar player_smg_grenade_warning_dist( "player_smg_grenade_warning_dist", "5000" );

bool SMGGrenadeWarnTest( CEZ2_Player *pPlayer, CBaseEntity *pActivator )
{
	// Look for an SMG grenade that isn't ours and is approaching
	if (pActivator->ClassMatches( "grenade_ar2" ) && pActivator->GetOwnerEntity() != pPlayer)
	{
		Vector2D vecMoveDir = pActivator->GetSmoothedVelocity().AsVector2D();
		vecMoveDir.NormalizeInPlace();
		Vector2D vecDirToPlayer = pPlayer->GetAbsOrigin().AsVector2D() - pActivator->GetAbsOrigin().AsVector2D();
		Vector2D vecDirToPlayerNorm = vecDirToPlayer;
		vecDirToPlayerNorm.NormalizeInPlace();

		if ( vecDirToPlayer.LengthSqr() <= Square(player_smg_grenade_warning_dist.GetFloat()) && DotProduct2D( vecMoveDir, vecDirToPlayerNorm ) > player_smg_grenade_warning_cone.GetFloat() )
		{
			return true;
		}
	}

	return false;
}

bool SMGGrenadeWarn( CEZ2_Player *pPlayer, CBaseEntity *pActivator )
{
	pPlayer->SetWarningTarget( pActivator );

	IGameEvent *event = gameeventmanager->CreateEvent( "player_target_warning" );
	if (event)
	{
		event->SetInt( "userid", pPlayer->GetUserID() );
		event->SetInt( "target", pActivator->entindex() );
		gameeventmanager->FireEvent( event );
	}

	return true;
}

// Warn
SightEvent_t g_SightIncomingSMGGrenade( "SMG Grenade Warning", 5.0f, SMGGrenadeWarnTest, SMGGrenadeWarn, 0.5f );

//-----------------------------------------------------------------------------
inline void CEZ2_Player::AddSightEvent( SightEvent_t &sightEvent )
{
	sightEvent.flLastHintTime = 0.0f;
	sightEvent.flNextHintTime = 0.0f;
	m_SightEvents.AddToTail( &sightEvent );
}

//-----------------------------------------------------------------------------
CEZ2_Player::CEZ2_Player()
{
	AddSightEvent( g_SightHintSoldierSquad );
	AddSightEvent( g_SightHintCrateKick );
	AddSightEvent( g_SightHintEnterVehicle );
	AddSightEvent( g_SightHintSurrenderableCitizen );
	AddSightEvent( g_SightHintSurrenderedMedic );
	AddSightEvent( g_SightIncomingSMGGrenade );

	if (sv_bonus_challenge.GetInt() != EZ_CHALLENGE_NONE)
	{
		// Make sure the difficulty is set to hard and cheats are off
		// (must do this before the player spawns)

		ConVarRef skill( "skill" );
		skill.SetValue("3"); // SKILL_HARD
		HL2GameRules()->SetSkillLevel( SKILL_HARD );

		if (sv_cheats)
			sv_cheats->SetValue( "0" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::Spawn( void )
{
	if (player_dummy.GetBool() && gpGlobals->eLoadType != MapLoad_Background)
	{
		// Must do this here because PostConstructor() is called before save/restore,
		// which causes duplicates to be created.
		CreateNPCComponent();
	}

	BaseClass::Spawn();

	SetModel( "models/bad_cop.mdl" );

	Activate();

	if (GetBonusChallenge() != EZ_CHALLENGE_NONE)
	{
		// Run the main script
		RunScriptFile( "ez2/bonus_challenge" );

		if (GetBonusChallenge() > EZ_CHALLENGE_SPECIAL)
		{
			// Run the challenge's script
			RunScriptFile( UTIL_VarArgs( "ez2/bonus_challenge_special_%i", GetBonusChallenge() ) );
		}

		// Run the map's script
		RunScriptFile( UTIL_VarArgs( "ez2/bonus_challenge_%s", STRING(gpGlobals->mapname) ) );

		if (gpGlobals->eLoadType != MapLoad_Transition)
		{
			// Ensure the bonus challenge state is retained if the player dies
			CBaseEntity *pAutosave = CBaseEntity::Create( "logic_autosave", vec3_origin, vec3_angle, NULL );
			if ( pAutosave )
			{
				g_EventQueue.AddEvent( pAutosave, "Save", 1.0, NULL, NULL );
				g_EventQueue.AddEvent( pAutosave, "Kill", 1.1, NULL, NULL );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::Activate( void )
{
	BaseClass::Activate();

	ListenForGameEvent( "zombie_scream" );
	ListenForGameEvent( "vehicle_overturned" );
	ListenForGameEvent( "skill_changed" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::UpdateOnRemove( void )
{
	RemoveNPCComponent();

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::CanAutoSwitchToNextBestWeapon( CBaseCombatWeapon *pWeapon )
{
	if (GetNPCComponent())
	{
		if ( GetNPCComponent()->GetState() == NPC_STATE_COMBAT )
		{
			Msg("EZ2: Not autoswitching to %s because of combat\n", pWeapon->GetDebugName());
		}
	}
	
	return BaseClass::CanAutoSwitchToNextBestWeapon( pWeapon );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::OnUseEntity( CBaseEntity *pEntity )
{
	// Continuous use = turning valves, using chargers, etc.
	if (pEntity->ObjectCaps() & FCAP_CONTINUOUS_USE)
		return;

	AI_CriteriaSet modifiers;
	bool bStealthChat = false;
	bool bOrderSurrender = false;
	ModifyOrAppendSpeechTargetCriteria( modifiers, pEntity );

	CAI_BaseNPC *pNPC = pEntity->MyNPCPointer();
	if ( pNPC )
	{
		// Is Bad Cop trying to scare a rebel that doesn't see him?
		if ( pNPC->IRelationType( this ) <= D_FR && pNPC->GetEnemy() != this && (!pNPC->GetExpresser() || !pNPC->GetExpresser()->IsSpeaking()) )
		{
			modifiers.AppendCriteria( "stealth_chat", "1" );
			bStealthChat = true;
		}

		// Is Bad Cop trying to order a rebel to surrender?
		if ( pNPC->IRelationType( this ) <= D_FR && !pNPC->GetWeapon( 0 ) && !pNPC->IsInAScript() )
		{
			modifiers.AppendCriteria( "order_surrender", "1" );
			bOrderSurrender = true;

			// See MIN_HUDHINT_DISPLAY_TIME in basecombatweapon_shared.cpp
			if (g_SightHintSurrenderableCitizen.flLastHintTime != 0 && gpGlobals->curtime - g_SightHintSurrenderableCitizen.flLastHintTime < 7.0f)
				SurrenderableCitizenHintHide( this, pNPC );
		}
	}

	bool bSpoken = SpeakIfAllowed( TLK_USE, modifiers );

	if (bSpoken)
	{
		if ( bOrderSurrender )
		{
			OnOrderSurrender( pNPC );
		}
		else if ( bStealthChat )
		{
			// "Fascinating."
			// "Holy shit!"

			if (pNPC->GetExpresser())
			{
				pNPC->GetExpresser()->Speak( TLK_USE_SCARE );

				// If the chosen response has no speech, force the NPC to be recognized as *not* speaking.
				// This lets them say their alert concept while turning to look at the player.
				if (!IsRunningScriptedSceneWithSpeech(pNPC))
					pNPC->GetExpresser()->ForceNotSpeaking();
			}
			else
			{
				pNPC->AddFacingTarget(this, 5.0f, 3.0f, 2.0f);
				pNPC->AddLookTarget(this, 5.0f, 3.0f, 2.0f);
			}

			//CSoundEnt::InsertSound( SOUND_PLAYER, EyePosition(), 128, 0.1, this );
		}
	}
	else if ( bOrderSurrender )
	{
		// The player can't speak. We may be in a scripted state.
		// If the NPC is facing us, try to order a surrender anyway.
		// (Bad Cop just has that glare, don't question it)
		if (/*GetNumSquadCommandables() <= 0 &&*/ pNPC->FInViewCone( this ) && GetActiveWeapon() && player_silent_surrender.GetBool())
		{
			//GetActiveWeapon()->SendWeaponAnim( ACT_VM_COMMAND_SEND );
			OnOrderSurrender( pNPC );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CEZ2_Player::HandleInteraction( int interactionType, void *data, CBaseCombatCharacter* sourceEnt )
{
	if ( interactionType == g_interactionBarnacleVictimGrab )
	{
		AI_CriteriaSet modifiers;

		if (sourceEnt)
			ModifyOrAppendEnemyCriteria(modifiers, sourceEnt);

		SpeakIfAllowed(TLK_SELF_IN_BARNACLE, modifiers);

		// Fall in on base
		//return true;
	}
	else if ( interactionType == g_interactionScannerInspectBegin )
	{
		AI_CriteriaSet modifiers;

		if (sourceEnt)
			ModifyOrAppendEnemyCriteria(modifiers, sourceEnt);

		SpeakIfAllowed(TLK_SCANNER_FLASH, modifiers);

		return true;
	}
	else if ( interactionType == g_interactionXenGrenadeHop )
	{
		// Drop the Xen grenade if we're holding it
		CBaseEntity *pGrenade = (CBaseEntity*)data;
		if (pGrenade)
		{
			CBaseEntity *pHeldEntity = GetPlayerHeldEntity(this);
			if ( pHeldEntity )
			{
				if (pHeldEntity == pGrenade)
					ClearUseEntity();
			}
		}

		return true;
	}

	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::OnOrderSurrender( CAI_BaseNPC *pNPC )
{
	pNPC->DispatchInteraction( g_interactionBadCopOrderSurrender, NULL, this );

	// Increment the associated global
	int iGlobal = GlobalEntity_GetIndex( GLOBAL_PLAYER_ORDER_SURRENDER );
	if ( iGlobal < 0 )
	{
		iGlobal = GlobalEntity_Add( GLOBAL_PLAYER_ORDER_SURRENDER, STRING(gpGlobals->mapname), GLOBAL_ON );
		GlobalEntity_SetCounter( iGlobal, 0 );
	}

	GlobalEntity_AddToCounter( iGlobal, 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Disposition_t CEZ2_Player::IRelationType( CBaseEntity *pTarget )
{
	Disposition_t base = BaseClass::IRelationType( pTarget );

	if (pTarget && pTarget->Classify() == CLASS_COMBINE_HUSK)
	{
		// Bad Cop likes husks which are passive towards him
		CAI_HuskSink *pHusk = dynamic_cast<CAI_HuskSink *>(pTarget);
		if (pHusk && pHusk->IsPassiveTarget( this ))
			return D_LI;
	}

	return base;
}

//-----------------------------------------------------------------------------
// Purpose: Put this player in a vehicle 
//-----------------------------------------------------------------------------
bool CEZ2_Player::GetInVehicle( IServerVehicle *pVehicle, int nRole )
{
	if (!BaseClass::GetInVehicle( pVehicle, nRole ))
		return false;

	// See MIN_HUDHINT_DISPLAY_TIME in basecombatweapon_shared.cpp
	if (g_SightHintEnterVehicle.flLastHintTime != 0 && gpGlobals->curtime - g_SightHintEnterVehicle.flLastHintTime < 7.0f)
		VehicleHintHide( this, pVehicle->GetVehicleEnt() );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iImpulse - 
//-----------------------------------------------------------------------------
void CEZ2_Player::CheatImpulseCommands( int iImpulse )
{
	switch (iImpulse)
	{
	// Use healthvial
	case 28:
	{
		UseHealthVial();
		break;
	}
	// Detonate explosives
	case 36:
	{
		DetonateExplosives();
		break;
	}

	default:
		BaseClass::CheatImpulseCommands( iImpulse );
	}
}

void CEZ2_Player::DetonateExplosives()
{
	DevMsg( "Player attempting to detonate explosives...\n" );

	bool bDidExplode = false;
	CBaseEntity * pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "npc_satchel" )) != NULL)
	{
		CSatchelCharge *pSatchel = dynamic_cast<CSatchelCharge *>(pEntity);
		if (pSatchel->m_bIsLive && pSatchel->GetThrower() && pSatchel->GetThrower() == this)
		{
			g_EventQueue.AddEvent( pSatchel, "Explode", 0.20, this, this );
			bDidExplode = true;
		}
	}

	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "point_detonatable" )) != NULL)
	{
		CPointDetonatable *pDetonatable = static_cast<CPointDetonatable *>(pEntity);
		if (!pDetonatable->m_bDisabled && pDetonatable->m_hThrower.Get() == this)
		{
			g_EventQueue.AddEvent( pDetonatable, "Detonate", 0.20, this, this );
			bDidExplode = true;
		}
	}

	if (bDidExplode)
	{
		// Play sound for pressing the detonator
		EmitSound( "Player.Detonate" );
		SpeakIfAllowed( TLK_DETONATE );
	}
	else
	{
		// Play a 'failed to detonate' sound
		EmitSound( "Player.DetonateFail" );
	}
}

extern ConVar sk_healthvial;
extern ConVar sk_healthvial_backpack;

void CEZ2_Player::UseHealthVial()
{
	DevMsg("Player attempting to use health vial...\n");


	if (sk_healthvial_backpack.GetBool() == false)
	{
		DevMsg("sk_healthvial_backpack is set to 0.\n");
		return;
	}

	int iAmmoType = GetAmmoDef()->Index("item_healthvial");
	if (GetAmmoCount(iAmmoType) > 0 && TakeHealth(sk_healthvial.GetFloat(), DMG_GENERIC))
	{
		RemoveAmmo(1, iAmmoType);

		// Play sound for using health vial
		EmitSound("HealthVial.BackpackConsume");
	}
	else
	{
		// Play a 'deny' sound
		EmitSound("HealthVial.BackpackDeny");
	}

	// send a message to the client, to notify the hud of the loss
	CSingleUserRecipientFilter user(this);
	user.MakeReliable();
	UserMessageBegin(user, "HealthVialConsumed");
	WRITE_BOOL(true);
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: For checking if the player is looking at something, but takes the pitch into account (more expensive)
//-----------------------------------------------------------------------------
bool CEZ2_Player::FInTrueViewCone( const Vector &vecSpot )
{
	Vector los = ( vecSpot - EyePosition() );

	// Same as FInViewCone(), but in 2D
	VectorNormalize( los );

	Vector facingDir = EyeDirection3D();

	float flDot = DotProduct( los, facingDir );

	if ( flDot > m_flFieldOfView )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Event fired when a living player takes damage - used to emit damage sounds
//-----------------------------------------------------------------------------
int CEZ2_Player::OnTakeDamage_Alive(const CTakeDamageInfo & info)
{
	// Record memory stuff
	if (info.GetAttacker() && info.GetAttacker() != GetWorldEntity() && info.GetAttacker() != this)
	{
		GetMemoryComponent()->RecordEngagementStart();
		GetMemoryComponent()->InitLastDamage(info);
	}

	AI_CriteriaSet modifiers;
	ModifyOrAppendDamageCriteria(modifiers, info);

	if (IsAllowedToSpeak(TLK_WOUND))
	{
		SetSpeechTarget( FindSpeechTarget() );
		if (!GetSpeechTarget())
			SetSpeechTarget( info.GetAttacker() );

		// Complain about taking damage from an enemy.
		// If that doesn't work, just do a regular wound. (we know we're allowed to speak it)
		if (!SpeakIfAllowed(TLK_WOUND_REMARK, modifiers))
			Speak(TLK_WOUND, modifiers);
	}

	int iPreHealth = GetHealth();
	int base = BaseClass::OnTakeDamage_Alive(info);

	if (base == 1 && GetHealth() != iPreHealth)
	{
		if (GetBonusChallenge() == EZ_CHALLENGE_DAMAGE)
		{
			// Add this damage to the bonus progress
			SetBonusProgress( GetBonusProgress() + (iPreHealth - GetHealth()) );
		}
	}

	return base;
}

//-----------------------------------------------------------------------------
// Purpose: give health. Returns the amount of health actually taken.
//-----------------------------------------------------------------------------
int CEZ2_Player::TakeHealth( float flHealth, int bitsDamageType )
{
	// Cache the player's original health
	int beforeHealth = m_iHealth;

	// Give health
	int returnValue = BaseClass::TakeHealth( flHealth, bitsDamageType );

	AI_CriteriaSet modifiers;
	modifiers.AppendCriteria( "health", UTIL_VarArgs( "%i", beforeHealth ) );
	modifiers.AppendCriteria( "damage", UTIL_VarArgs( "%i", returnValue ) );
	modifiers.AppendCriteria( "damage_type", UTIL_VarArgs( "%i", bitsDamageType ) );

	SpeakIfAllowed( TLK_HEAL, modifiers );

	return returnValue;
}

//-----------------------------------------------------------------------------
// Purpose: Override and copy-paste of CBasePlayer::TraceAttack(), does fake hitgroup calculations
//-----------------------------------------------------------------------------
void CEZ2_Player::TraceAttack( const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	if ( m_takedamage )
	{
		CTakeDamageInfo info = inputInfo;

		if ( info.GetAttacker() )
		{
			// --------------------------------------------------
			//  If an NPC check if friendly fire is disallowed
			// --------------------------------------------------
			CAI_BaseNPC *pNPC = info.GetAttacker()->MyNPCPointer();
			if ( pNPC && (pNPC->CapabilitiesGet() & bits_CAP_NO_HIT_PLAYER) && pNPC->IRelationType( this ) != D_HT )
				return;

			// Prevent team damage here so blood doesn't appear
			if ( info.GetAttacker()->IsPlayer() )
			{
				if ( !g_pGameRules->FPlayerCanTakeDamage( this, info.GetAttacker(), info ) )
					return;
			}
		}

		int hitgroup = ptr->hitgroup;

		if ( hitgroup == HITGROUP_GENERIC )
		{
			// Try and calculate a fake hitgroup since Bad Cop doesn't have a model.
			Vector vPlayerMins = GetPlayerMins();
			Vector vPlayerMaxs = GetPlayerMaxs();
			Vector vecDamagePos = (inputInfo.GetDamagePosition() - GetAbsOrigin());

			if (vecDamagePos.z < (vPlayerMins[2] + vPlayerMaxs[2])*0.5)
			{
				// Legs (under waist)
				// We could do either leg with matrix calculations if we want, but we don't need that right now.
				hitgroup = HITGROUP_LEFTLEG;
			}
			else if (vecDamagePos.z >= (GetViewOffset()[2] - 1.0f))
			{
				// Head
				hitgroup = HITGROUP_HEAD;
			}
			else
			{
				// Torso
				// We could do arms with matrix calculations if we want, but we don't need that right now.
				hitgroup = HITGROUP_STOMACH;
			}
		}

		SetLastHitGroup( hitgroup );


		// If this damage type makes us bleed, then do so
		bool bShouldBleed = !g_pGameRules->Damage_ShouldNotBleed( info.GetDamageType() );
		if ( bShouldBleed )
		{
			SpawnBlood(ptr->endpos, vecDir, BloodColor(), info.GetDamage());// a little surface blood.
			TraceBleed( info.GetDamage(), vecDir, ptr, info.GetDamageType() );
		}

		AddMultiDamage( info, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Execute squad command
//-----------------------------------------------------------------------------
bool CEZ2_Player::CommanderExecuteOne(CAI_BaseNPC * pNpc, const commandgoal_t & goal, CAI_BaseNPC ** Allies, int numAllies)
{
	// This is called for each NPC, so make sure we haven't already spoken yet
	if (GetExpresser()->GetTimeSpokeConcept(TLK_COMMAND_RECALL) != gpGlobals->curtime &&
		GetExpresser()->GetTimeSpokeConcept(TLK_COMMAND_SEND) != gpGlobals->curtime)
	{
		if (goal.m_pGoalEntity)
		{
			SpeakIfAllowed(TLK_COMMAND_RECALL);
			m_vecLastCommandGoal = vec3_invalid;
		}
		else if (pNpc->IsInPlayerSquad())
		{
			AI_CriteriaSet modifiers;

			modifiers.AppendCriteria("commandpoint_dist_to_player", UTIL_VarArgs("%.0f", (goal.m_vecGoalLocation - GetAbsOrigin()).Length()));
			modifiers.AppendCriteria("commandpoint_dist_to_npc", UTIL_VarArgs("%.0f", (goal.m_vecGoalLocation - pNpc->GetAbsOrigin()).Length()));
			modifiers.AppendCriteria("distancetoplayer", UTIL_VarArgs("%.0f", (GetAbsOrigin() - pNpc->GetAbsOrigin()).Length()));

			// This is the difference in between the new goal and the last goal, but it's mostly just
			// so we know whether we already had a goal before this command or not.
			if (m_vecLastCommandGoal != vec3_invalid)
				modifiers.AppendCriteria( "commandpoint_prev", UTIL_VarArgs( "%.0f", (goal.m_vecGoalLocation - m_vecLastCommandGoal).Length() ) );
			else
				modifiers.AppendCriteria( "commandpoint_prev", "-1" );

			m_vecLastCommandGoal = goal.m_vecGoalLocation;

			SpeakIfAllowed(TLK_COMMAND_SEND, modifiers);
		}
	}

	return BaseClass::CommanderExecuteOne(pNpc, goal, Allies, numAllies);
}

// Determines if this concept isn't worth saying "Shut up" over.
static inline bool WasUnremarkableConcept( AIConcept_t concept )
{
	return CompareConcepts( concept, TLK_WOUND ) ||
			CompareConcepts( concept, TLK_SHOT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::ModifyOrAppendCriteria(AI_CriteriaSet& criteriaSet)
{
	ModifyOrAppendSquadCriteria(criteriaSet); // Add player squad criteria

	if (GetNPCComponent())
	{
		CAI_PlayerNPCDummy *pAI = GetNPCComponent();

		pAI->ModifyOrAppendOuterCriteria(criteriaSet);

		// Append enemy stuff
		if (pAI->GetState() == NPC_STATE_COMBAT)
		{
			// Append criteria for our general enemy if it hasn't been filled out already
			if (!IsCriteriaModified(PLAYERCRIT_ENEMY))
				ModifyOrAppendEnemyCriteria(criteriaSet, pAI->GetEnemy());

			ModifyOrAppendAICombatCriteria(criteriaSet);
		}
	}

	if (GetMemoryComponent()->InEngagement())
	{
		criteriaSet.AppendCriteria("combat_length", UTIL_VarArgs("%f", GetMemoryComponent()->GetEngagementTime()));
	}

	if (IsInAVehicle())
		criteriaSet.AppendCriteria("in_vehicle", GetVehicleEntity()->GetClassname());
	else
		criteriaSet.AppendCriteria("in_vehicle", "0");

	// Use our speech target's criteris if we should
	if (GetSpeechTarget() && !IsCriteriaModified(PLAYERCRIT_SPEECHTARGET))
		ModifyOrAppendSpeechTargetCriteria(criteriaSet, GetSpeechTarget());

	// Use criteria from our active weapon
	if (!IsCriteriaModified(PLAYERCRIT_WEAPON) && GetActiveWeapon())
		ModifyOrAppendWeaponCriteria(criteriaSet, GetActiveWeapon());

	// Reset this now that we're appending general criteria
	ResetPlayerCriteria();

	// Look for Will-E.
	float flBestDistSqr = FLT_MAX;
	CNPC_Wilson *pWilson = CNPC_Wilson::GetBestWilson( flBestDistSqr, &GetAbsOrigin() );
	if (pWilson)
	{
		criteriaSet.AppendCriteria("wilson_distance", CFmtStrN<32>( "%f.3", sqrt(flBestDistSqr) ));

		// Record whether Wilson is speaking
		if (pWilson->IsSpeaking())
		{
			criteriaSet.AppendCriteria( "wilson_speaking", "1" );
		}
		else
		{
			criteriaSet.AppendCriteria( "wilson_speaking", "0" );
		}

		pWilson->AppendContextToCriteria( criteriaSet, "wilson_" );
	}
	else
	{
		criteriaSet.AppendCriteria("wilson_distance", "99999999999");
	}

	// Do we have a speech filter? If so, append its criteria too
	if ( GetSpeechFilter() )
	{
		GetSpeechFilter()->AppendContextToCriteria( criteriaSet );
	}

	// Find the last spoken concept.
	AIConcept_t lastSpokeConcept = GetExpresser()->GetLastSpokeConcept( "TLK_WOUND" );
	float lastSpokeTime = MAX( GetTimeSpokeConcept( lastSpokeConcept ), LastTimePlayerTalked() );
	criteriaSet.AppendCriteria( "last_spoke", UTIL_VarArgs( "%f", gpGlobals->curtime - lastSpokeTime ) );

	BaseClass::ModifyOrAppendCriteria(criteriaSet);
}

//-----------------------------------------------------------------------------
// Purpose: Appends damage criteria
//-----------------------------------------------------------------------------
void CEZ2_Player::ModifyOrAppendDamageCriteria(AI_CriteriaSet & set, const CTakeDamageInfo & info, bool bPlayer)
{
	MarkCriteria(PLAYERCRIT_DAMAGE);

	set.AppendCriteria("damage", UTIL_VarArgs("%i", (int)info.GetDamage()));
	set.AppendCriteria("damage_type", UTIL_VarArgs("%i", info.GetDamageType()));

	if (info.GetInflictor())
	{
		CBaseEntity *pInflictor = info.GetInflictor();
		set.AppendCriteria("inflictor", pInflictor->GetClassname());
		set.AppendCriteria("inflictor_is_physics", pInflictor->GetMoveType() == MOVETYPE_VPHYSICS ? "1" : "0");
	}

	// Are we the one getting damaged?
	if (bPlayer)
	{
		// This technically doesn't need damage info, but whatever.
		set.AppendCriteria("hitgroup", UTIL_VarArgs("%i", LastHitGroup()));

		if (info.GetAttacker() != this)
		{
			if (!IsCriteriaModified(PLAYERCRIT_ENEMY))
				ModifyOrAppendEnemyCriteria(set, info.GetAttacker());
		}
		else
		{
			// We're our own enemy
			set.AppendCriteria("damaged_self", "1");
		}
	}
}
//-----------------------------------------------------------------------------
// Purpose: Appends enemy criteria
//-----------------------------------------------------------------------------
void CEZ2_Player::ModifyOrAppendEnemyCriteria(AI_CriteriaSet& set, CBaseEntity *pEnemy)
{
	MarkCriteria(PLAYERCRIT_ENEMY);

	if (pEnemy)
	{
		set.AppendCriteria("enemy", pEnemy->GetClassname());
		set.AppendCriteria("enemyclass", g_pGameRules->AIClassText(pEnemy->Classify()));
		set.AppendCriteria("distancetoenemy", UTIL_VarArgs("%f", GetAbsOrigin().DistTo((pEnemy->GetAbsOrigin()))));
		set.AppendCriteria("enemy_visible", (FInViewCone(pEnemy) && FVisible(pEnemy)) ? "1" : "0");

		CAI_BaseNPC *pNPC = pEnemy->MyNPCPointer();
		if (pNPC)
		{
			set.AppendCriteria("enemy_is_npc", "1");

			if (pNPC->IsOnFire())
				set.AppendCriteria("enemy_on_fire", "1");

			// Bypass split-second reactions
			if (pNPC->GetEnemy() == this && (gpGlobals->curtime - pNPC->GetLastEnemyTime()) > 0.1f)
			{
				set.AppendCriteria( "enemy_attacking_me", "1" );
				set.AppendCriteria( "enemy_sees_me", pNPC->HasCondition( COND_SEE_ENEMY ) ? "1" : "0" );
			}
			else
			{
				// Some NPCs have tunnel vision and lose sight of their enemies often.
				// If they remember us and last saw us less than 4 seconds ago,
				// they probably know we're there.
				// Sure, this means the response system thinks a NPC "sees me" when they technically do not,
				// but it curbs false sneak attack cases.
				AI_EnemyInfo_t *EnemyInfo = pNPC->GetEnemies()->Find( this );
				if (EnemyInfo && (gpGlobals->curtime - EnemyInfo->timeLastSeen) < 4.0f)
				{
					set.AppendCriteria( "enemy_sees_me", "1" );
				}
				else
				{
					// Do a simple visibility check
					set.AppendCriteria( "enemy_sees_me", pNPC->FInViewCone( this ) && pNPC->FVisible( this ) ? "1" : "0" );
				}
			}

			if (pNPC->GetExpresser())
			{
				// That's enough outta you.
				// (IsSpeaking() accounts for the delay as well, so it lingers beyond actual speech time)
#ifdef NEW_RESPONSE_SYSTEM
				if (gpGlobals->curtime < pNPC->GetExpresser()->GetTimeSpeechCompleteWithoutDelay()
						&& !WasUnremarkableConcept(pNPC->GetExpresser()->GetLastSpokeConcept()))
#else
				if (gpGlobals->curtime < pNPC->GetExpresser()->GetRealTimeSpeechComplete()
						&& !WasUnremarkableConcept(pNPC->GetExpresser()->GetLastSpokeConcept()))
#endif
					set.AppendCriteria("enemy_is_speaking", "1");
			}

			set.AppendCriteria( "enemy_activity", CAI_BaseNPC::GetActivityName( pNPC->GetActivity() ) );

			set.AppendCriteria( "enemy_weapon", pNPC->GetActiveWeapon() ? pNPC->GetActiveWeapon()->GetClassname() : "0" );

			// NPC and class-specific criteria
			pNPC->ModifyOrAppendCriteriaForPlayer(this, set);
		}
		else
		{
			set.AppendCriteria("enemy_is_npc", "0");
		}

		// Append their contexts
		pEnemy->AppendContextToCriteria( set, "enemy_" );
	}
	else
	{
		set.AppendCriteria("distancetoenemy", "-1");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Appends squad criteria
//		1upD added this method, but the code is from Blixibon to add
//		squadmate criteria.
//-----------------------------------------------------------------------------
void CEZ2_Player::ModifyOrAppendSquadCriteria(AI_CriteriaSet& set)
{
	MarkCriteria(PLAYERCRIT_SQUAD);

	CAI_BaseNPC *pRepresentative = GetSquadCommandRepresentative();
	if (pRepresentative != NULL)
	{
		if (pRepresentative->GetCommandGoal() != vec3_invalid)
			set.AppendCriteria("squad_mode", "1"); // Send
		else
			set.AppendCriteria("squad_mode", "0"); // Follow

		// Get criteria related to individual squad members
		int iNumSquadCommandables = 0;
		bool bSquadInPVS = false;
		AISquadIter_t iter;
		for (CAI_BaseNPC *pAllyNpc = GetPlayerSquad()->GetFirstMember( &iter ); pAllyNpc; pAllyNpc = GetPlayerSquad()->GetNextMember( &iter ))
		{
			if (pAllyNpc->IsCommandable() && !pAllyNpc->IsSilentCommandable())
			{
				if (pAllyNpc->HasCondition( COND_IN_PVS ))
					bSquadInPVS = true;

				iNumSquadCommandables++;
			}
		}

		set.AppendCriteria("squadmates", UTIL_VarArgs("%i", iNumSquadCommandables));

		if (bSquadInPVS)
			set.AppendCriteria("squad_in_pvs", "1");
	}
	else
	{
		set.AppendCriteria("squadmates", "0");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Appends weapon criteria
//-----------------------------------------------------------------------------
void CEZ2_Player::ModifyOrAppendWeaponCriteria(AI_CriteriaSet& set, CBaseEntity *pWeapon)
{
	MarkCriteria(PLAYERCRIT_WEAPON);

	set.AppendCriteria( "weapon", pWeapon->GetClassname() );

	// Append its contexts
	pWeapon->AppendContextToCriteria( set, "weapon_" );
}

//-----------------------------------------------------------------------------
// Purpose: Appends speech target criteria
//-----------------------------------------------------------------------------
void CEZ2_Player::ModifyOrAppendSpeechTargetCriteria(AI_CriteriaSet &set, CBaseEntity *pTarget)
{
	MarkCriteria(PLAYERCRIT_SPEECHTARGET);

	Assert(pTarget);

	set.AppendCriteria( "speechtarget", pTarget->GetClassname() );
	set.AppendCriteria( "speechtargetname", STRING(pTarget->GetEntityName()) );

	set.AppendCriteria( "speechtarget_visible", (FInViewCone(pTarget) && FVisible(pTarget)) ? "1" : "0" );

	if (pTarget->IsNPC())
	{
		CAI_BaseNPC *pNPC = pTarget->MyNPCPointer();

		if (pNPC->GetActiveWeapon())
			set.AppendCriteria( "speechtarget_weapon", pNPC->GetActiveWeapon()->GetClassname() );

		if (pNPC->IsInPlayerSquad())
		{
			// Silent commandable NPCs are rollermines, manhacks, etc. that are in the player's squad and can be commanded,
			// but don't show up in the HUD, have primitive commanding schedules, and are meant to give priority to non-silent members.
			set.AppendCriteria( "speechtarget_inplayersquad", pNPC->IsSilentCommandable() ? "2" : "1" );
		}
		else
		{
			set.AppendCriteria( "speechtarget_inplayersquad", "0" );
		}

		// NPC and class-specific criteria
		pNPC->ModifyOrAppendCriteriaForPlayer(this, set);
	}

	// Append their contexts
	pTarget->AppendContextToCriteria( set, "speechtarget_" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CEZ2_Player::FindSpeechTarget()
{
	const Vector &	vAbsOrigin = GetAbsOrigin();
	float 			closestDistSq = FLT_MAX;
	CBaseEntity *	pNearest = NULL;
	float			distSq;

	// First, search for Will-E
	for (CNPC_Wilson *pWilson = CNPC_Wilson::GetWilson(); pWilson != NULL; pWilson = pWilson->m_pNext)
	{
		if (!pWilson->IsPlayerAlly())
			continue;

		distSq = (vAbsOrigin - pWilson->GetAbsOrigin()).LengthSqr();
		if ( (distSq > Square(TALKRANGE_MIN) || distSq > closestDistSq) && !pWilson->IsOmniscient() )
			continue;

		if (pWilson->IsAlive() && !pWilson->IsInAScript() && !pWilson->HasSpawnFlags(SF_NPC_GAG))
		{
			closestDistSq = distSq;
			pNearest = pWilson;
		}
	}

	if (pNearest)
		return pNearest;

	// Next, search our squad
	AISquadIter_t iter;
	for (CAI_BaseNPC *pAllyNpc = GetPlayerSquad()->GetFirstMember( &iter ); pAllyNpc; pAllyNpc = GetPlayerSquad()->GetNextMember( &iter ))
	{
		if (!pAllyNpc->HasCondition( COND_IN_PVS ))
			continue;

		distSq = (vAbsOrigin - pAllyNpc->GetAbsOrigin()).LengthSqr();
		if ( distSq > Square(TALKRANGE_MIN) || distSq > closestDistSq )
			continue;

		if (pAllyNpc->IsAlive() && pAllyNpc->CanBeUsedAsAFriend() && !pAllyNpc->IsInAScript() && !pAllyNpc->HasSpawnFlags(SF_NPC_GAG))
		{
			closestDistSq = distSq;
			pNearest = pAllyNpc;
		}
	}

	if (pNearest)
		return pNearest;

	// Finally, look for one of our enemies
	if (GetNPCComponent())
	{
		CAI_Enemies *pEnemies = GetNPCComponent()->GetEnemies();
		AIEnemiesIter_t iter;
		for ( AI_EnemyInfo_t *pEMemory = pEnemies->GetFirst(&iter); pEMemory != NULL; pEMemory = pEnemies->GetNext(&iter) )
		{
			distSq = (vAbsOrigin - pEMemory->hEnemy->GetAbsOrigin()).LengthSqr();
			if ( distSq > Square(TALKRANGE_MIN) || distSq > closestDistSq )
				continue;

			CAI_BaseNPC *pNPC = pEMemory->hEnemy->MyNPCPointer();
			if (!pNPC)
				continue;

			if (pNPC->IsAlive() && pNPC->CanBeUsedAsAFriend() && !pNPC->IsInAScript() && !pNPC->HasSpawnFlags( SF_NPC_GAG ))
			{
				closestDistSq = distSq;
				pNearest = pNPC;
			}
		}
	}

	return pNearest;
}

//-----------------------------------------------------------------------------
// Purpose: Check if the given concept can be spoken
//-----------------------------------------------------------------------------
bool CEZ2_Player::IsAllowedToSpeak(AIConcept_t concept)
{
	if (m_lifeState > LIFE_DYING)
		return false;

	bool bCanSpeak = GetExpresser()->CanSpeak();
	if (concept && !bCanSpeak)
	{
		ConceptInfo_t *pInfo = GetAllySpeechManager()->GetConceptInfo( concept );
		ConceptInfo_t *pPrevInfo = GetAllySpeechManager()->GetConceptInfo( GetExpresser()->GetLastSpokeConcept() );
		if (pInfo && pPrevInfo)
		{
			bCanSpeak = pInfo->category > pPrevInfo->category;
		}
	}

	if (!bCanSpeak)
		return false;

	if (!GetExpresser()->CanSpeakConcept(concept))
		return false;

	// Remove this once we've replaced gagging in all of the maps and talker files
	const char *szGag = GetContextValue( "gag" );
	if (szGag && FStrEq(szGag, "1"))
		return false;

	if (IsInAScript())
		return false;

	if (player_mute_responses.GetBool())
		return false;

	// Don't say anything if we're running a scene
	// (instanced scenes aren't ignored because they could be Wilson scenes)
	if ( IsTalkingInAScriptedScene( this, false ) )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Check if the given concept can be spoken and then speak it
//-----------------------------------------------------------------------------
bool CEZ2_Player::SpeakIfAllowed(AIConcept_t concept, AI_CriteriaSet& modifiers, char *pszOutResponseChosen, size_t bufsize, IRecipientFilter *filter)
{
	if (!IsAllowedToSpeak(concept))
		return false;

	return Speak(concept, modifiers, pszOutResponseChosen, bufsize, filter);
}

//-----------------------------------------------------------------------------
// Purpose: Alternate method signature for SpeakIfAllowed allowing no criteriaset parameter 
//-----------------------------------------------------------------------------
bool CEZ2_Player::SpeakIfAllowed(AIConcept_t concept, char *pszOutResponseChosen, size_t bufsize, IRecipientFilter *filter)
{
	AI_CriteriaSet set;
	return SpeakIfAllowed(concept, set, pszOutResponseChosen, bufsize, filter);
}

//-----------------------------------------------------------------------------
// Purpose: Find a response for the given concept
//-----------------------------------------------------------------------------
bool CEZ2_Player::SelectSpeechResponse( AIConcept_t concept, AI_CriteriaSet *modifiers, CBaseEntity *pTarget, AISpeechSelection_t *pSelection )
{
	if ( IsAllowedToSpeak( concept ) )
	{
		// If we have modifiers, send them, otherwise create a new object
#ifdef NEW_RESPONSE_SYSTEM
		bool result = FindResponse( pSelection->Response, concept, modifiers );

		if ( result )
		{
			pSelection->concept = concept;
			pSelection->hSpeechTarget = pTarget;
			return true;
		}
#else
		AI_Response *pResponse = SpeakFindResponse( concept, (modifiers != NULL ? *modifiers : AI_CriteriaSet()) );

		if ( pResponse )
		{
			pSelection->Set( concept, pResponse, pTarget );
			return true;
		}
#endif
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::PostSpeakDispatchResponse( AIConcept_t concept, AI_Response *response )
{
	CBaseEntity *pTarget = GetSpeechTarget();
	if (!pTarget || !pTarget->IsAlive() || !pTarget->IsCombatCharacter())
	{
		// Find Wilson
		float flDistSqr = Square( 4096.0f );
		pTarget = CNPC_Wilson::GetBestWilson( flDistSqr, &GetAbsOrigin() );
	}

	if (pTarget)
	{
		// Get them to look at us (at least if it's a soldier)
		if (pTarget->MyNPCPointer() && pTarget->MyNPCPointer()->CapabilitiesGet() & bits_CAP_TURN_HEAD)
		{
			CAI_BaseActor *pActor = dynamic_cast<CAI_BaseActor*>(pTarget);
			if (pActor)
				pActor->AddLookTarget( this, 0.75, (GetExpresser()->GetTimeSpeechComplete() - gpGlobals->curtime) + 3.0f );
		}

		// Notify the speech target for a response
		variant_t variant;
		variant.SetString(AllocPooledString(concept));

		char szResponse[64] = { "<null>" };
		char szRule[64] = { "<null>" };
		if (response)
		{
			response->GetName( szResponse, sizeof( szResponse ) );
			response->GetRule( szRule, sizeof( szRule ) );
		}

		float flDuration = (GetExpresser()->GetTimeSpeechComplete() - gpGlobals->curtime);
		pTarget->AddContext("speechtarget_response", szResponse, (gpGlobals->curtime + flDuration + 1.0f));
		pTarget->AddContext("speechtarget_rule", szRule, (gpGlobals->curtime + flDuration + 1.0f));

		// Delay is now based off of predelay
		g_EventQueue.AddEvent(pTarget, "AnswerConcept", variant, flDuration /*+ RandomFloat(0.25f, 0.5f)*/, this, this);
	}

	// Clear our speech target for the next concept
	SetSpeechTarget( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::InputAnswerConcept( inputdata_t &inputdata )
{
	// Complex Q&A
	ConceptResponseAnswer( inputdata.pActivator, inputdata.value.String() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::GetGameTextSpeechParams( hudtextparms_t &params )
{
	params.channel = 4;
	params.x = -1;
	params.y = 0.7;
	params.effect = 0;

	params.r1 = 255;
	params.g1 = 51;
	params.b1 = 0;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_Expresser *CEZ2_Player::CreateExpresser(void)
{
	m_pExpresser = new CAI_Expresser(this);
	if (!m_pExpresser)
		return NULL;

	m_pExpresser->Connect(this);
	return m_pExpresser;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::PostConstructor(const char *szClassname)
{
	BaseClass::PostConstructor(szClassname);
	CreateExpresser();

	GetMemoryComponent()->SetOuter(this);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::CreateNPCComponent()
{
	// Create our NPC component
	if (!m_hNPCComponent)
	{
		CBaseEntity *pEnt = CBaseEntity::CreateNoSpawn("player_npc_dummy", EyePosition(), EyeAngles(), this);
		m_hNPCComponent.Set(static_cast<CAI_PlayerNPCDummy*>(pEnt));

		if (m_hNPCComponent)
		{
			m_hNPCComponent->SetParent(this);
			m_hNPCComponent->SetOuter(this);

			DispatchSpawn( m_hNPCComponent );
		}
	}
	else
	{
		// Their outer is saved now, but double-check
		m_hNPCComponent->SetOuter(this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::RemoveNPCComponent()
{
	if ( m_hNPCComponent != NULL )
	{
		// Don't let friends call it out as a dead ally, as it overrides TLK_PLDEAD
		m_hNPCComponent->RemoveFromSquad();

		UTIL_Remove( m_hNPCComponent.Get() );
		m_hNPCComponent = NULL;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEZ2_Player::Event_Killed(const CTakeDamageInfo &info)
{
	BaseClass::Event_Killed(info);

	AI_CriteriaSet modifiers;
	ModifyOrAppendDamageCriteria(modifiers, info);
	Speak(TLK_DEATH, modifiers);

	// No speaking anymore
	m_flNextSpeechTime = FLT_MAX;
	RemoveNPCComponent();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEZ2_Player::Event_KilledOther(CBaseEntity *pVictim, const CTakeDamageInfo &info)
{
	BaseClass::Event_KilledOther(pVictim, info);

	if (pVictim->IsCombatCharacter())
	{
		// Event_KilledOther is called later than Event_NPCKilled in the NPC dying process,
		// meaning some pre-death conditions are retained in Event_NPCKilled that are not retained in Event_KilledOther,
		// such as the activity they were playing when they died.
		// As a result, NPCs always call through to Event_KilledEnemy through Event_NPCKilled and Event_KilledOther is not used.
		if (!pVictim->IsNPC())
			Event_KilledEnemy(pVictim->MyCombatCharacterPointer(), info);
	}
	else
	{
		// Killed a non-NPC (we don't do anything for this yet)
	}
}

//-----------------------------------------------------------------------------
// Purpose: Appends criteria for when we're leaving an engagement
//-----------------------------------------------------------------------------
void CEZ2_Player::ModifyOrAppendFinalEnemyCriteria(AI_CriteriaSet& set, CBaseEntity *pEnemy, const CTakeDamageInfo &info)
{
	// Append our pre-engagement conditions
	set.AppendCriteria( "prev_health_diff", UTIL_VarArgs( "%i", GetHealth() - GetMemoryComponent()->GetPrevHealth() ) );

	set.AppendCriteria( "num_enemies_historic", UTIL_VarArgs( "%i", GetMemoryComponent()->GetHistoricEnemies() ) );

	// Was this the one who made Bad Cop pissed 5 seconds ago?
	if (pEnemy == GetMemoryComponent()->GetLastDamageAttacker() && gpGlobals->curtime - GetLastDamageTime() < 5.0f)
	{
		set.AppendCriteria("enemy_revenge", "1");
		GetMemoryComponent()->AppendLastDamageCriteria( set );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Appends criteria from our AI
//-----------------------------------------------------------------------------
void CEZ2_Player::ModifyOrAppendAICombatCriteria(AI_CriteriaSet& set)
{
	// Append cached enemy numbers.
	// "num_enemies" here is just "visible" enemies and not every enemy the player saw before and knows is there, but that's good enough.
	set.AppendCriteria("num_enemies", UTIL_VarArgs("%i", m_iVisibleEnemies));
	set.AppendCriteria("close_enemies", UTIL_VarArgs("%i", m_iCloseEnemies));
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEZ2_Player::Event_KilledEnemy(CBaseCombatCharacter *pVictim, const CTakeDamageInfo &info)
{
	if (!IsAlive())
		return;

	if (GetBonusChallenge() == EZ_CHALLENGE_KILLS && pVictim->IsNPC())
	{
		SetBonusProgress( GetBonusProgress() + 1 );
	}

	if (CNPC_Wilson::GetWilson() && info.GetInflictor() == CNPC_Wilson::GetWilson())
	{
		// TODO: Achievement?

		// Wilson responds to these things himself
		info.GetInflictor()->Event_KilledOther(pVictim, info);
		return;
	}

	AI_CriteriaSet modifiers;

	ModifyOrAppendDamageCriteria(modifiers, info, false);
	ModifyOrAppendEnemyCriteria(modifiers, pVictim);

	GetMemoryComponent()->KilledEnemy();
	GetMemoryComponent()->AppendKilledEnemyCriteria(modifiers);

	Disposition_t disposition = pVictim->IRelationType(this);

	// This code used to check for CBaseCombatCharacter before,
	// but now the function specifically takes that kind of class.
	// Maybe an if statement related to optimization could be added here?
	{
		modifiers.AppendCriteria("hitgroup", UTIL_VarArgs("%i", pVictim->LastHitGroup()));

		// If the victim *just* started being afraid of us, pick it up as D_HT instead.
		// This is to prevent cases where Bad Cop taunts afraid enemies before it's apparent that they're afraid.
		if (disposition == D_FR && pVictim->JustStartedFearing(this))
			disposition = D_HT;

		modifiers.AppendCriteria("enemy_relationship", UTIL_VarArgs("%i", disposition));

		// Add our relationship to the victim as well
		modifiers.AppendCriteria("relationship", UTIL_VarArgs("%i", IRelationType(pVictim)));

		//if (CAI_BaseNPC *pNPC = pVictim->MyNPCPointer())
		//{
		//}

		// Bad Cop needs to differentiate between human-like opponents (rebels, zombies, vorts, etc.)
		// and non-human opponents (antlions, bullsquids, headcrabs, etc.) for some responses.
		if (pVictim->GetHullType() == HULL_HUMAN || pVictim->GetHullType() == HULL_WIDE_HUMAN)
			modifiers.AppendCriteria("enemy_humanoid", "1");
	}

	bool bSpoken = false;

	// Is this the last enemy we know about?
	if (m_iVisibleEnemies <= 1 && GetNPCComponent()->GetEnemy() == pVictim && GetMemoryComponent()->InEngagement())
	{
		ModifyOrAppendFinalEnemyCriteria( modifiers, pVictim, info );

		// If the enemy was targeting one of our allies, use said ally as the speech target.
		// Otherwise, just look for a random one.
		if (pVictim->GetEnemy() && pVictim->GetEnemy() != this && IRelationType(pVictim->GetEnemy()) > D_FR)
			SetSpeechTarget( pVictim->GetEnemy() );
		else
			SetSpeechTarget( FindSpeechTarget() );

		// Check if we should say something special in regards to the situation being apparently over.
		// Separate concepts are used to bypass respeak delays.
		bSpoken = SpeakIfAllowed(TLK_LAST_ENEMY_DEAD, modifiers);
	}
	else if (disposition == D_LI)
	{
		// Our "enemy" was actually our "friend"
		SetSpeechTarget( pVictim );
		bSpoken = SpeakIfAllowed(TLK_KILLED_ALLY, modifiers);
	}
	else
	{
		// Look for someone to talk to during murder
		SetSpeechTarget( FindSpeechTarget() );
	}

	if (!bSpoken)
		SpeakIfAllowed( TLK_ENEMY_DEAD, modifiers );
	else
		GetExpresser()->SetSpokeConcept( TLK_ENEMY_DEAD, NULL, false ); // Pretend we spoke TLK_ENEMY_DEAD too
}

//-----------------------------------------------------------------------------
// Purpose: Event fired by all NPCs, intended for when allies are killed, enemies are killed by allies, etc.
//-----------------------------------------------------------------------------
void CEZ2_Player::Event_NPCKilled(CAI_BaseNPC *pVictim, const CTakeDamageInfo &info)
{
	if (info.GetAttacker() == this || (GetVehicleEntity() && info.GetAttacker() == GetVehicleEntity()))
	{
		// Event_NPCKilled is called before Event_KilledOther in the NPC dying process,
		// meaning some pre-death conditions are retained in Event_NPCKilled that are not retained in Event_KilledOther,
		// such as the activity they were playing when they died.
		// As a result, NPCs always call through to Event_KilledEnemy through Event_NPCKilled and Event_KilledOther is not used.
		Event_KilledEnemy(pVictim, info);
		return;
	}

	// For now, don't care about NPCs not in our PVS.
	if (!pVictim->HasCondition(COND_IN_PVS))
		return;

	// "Mourn" dead allies
	if (pVictim->IsPlayerAlly(this))
	{
		AllyDied(pVictim, info);
		return;
	}

	if (info.GetAttacker())
	{
		// Check to see if they were killed by an ally.
		if (info.GetAttacker()->IsNPC())
		{
			if (info.GetAttacker()->MyNPCPointer()->IsPlayerAlly(this))
			{
				// Cheer them on, maybe!
				AllyKilledEnemy(info.GetAttacker(), pVictim, info);
				return;
			}
		}

		// Check to see if they were killed by a hopper mine.
		else if (FClassnameIs(info.GetAttacker(), "combine_mine"))
		{
			CBounceBomb *pBomb = static_cast<CBounceBomb*>(info.GetAttacker());
			if (pBomb)
			{
				if (pBomb->IsPlayerPlaced())
				{
					// Pretend we killed it.
					Event_KilledEnemy(pVictim, info);
					return;
				}
				else if (pBomb->IsFriend(this))
				{
					// Cheer them on, maybe!
					AllyKilledEnemy(info.GetAttacker(), pVictim, info);
					return;
				}
			}
		}
	}

	// Finally, see if they were an ignited NPC we were attacking.
	// Entity flames don't credit their igniters, so we can only guess that immediate enemies
	// dying on fire were lit up by the player.
	if (pVictim->IsOnFire() && GetNPCComponent() && GetNPCComponent()->GetEnemy() == pVictim)
	{
		// Pretend we killed it.
		Event_KilledEnemy(pVictim, info);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Event fired by killed allies
//-----------------------------------------------------------------------------
void CEZ2_Player::AllyDied( CAI_BaseNPC *pVictim, const CTakeDamageInfo &info )
{
	AI_CriteriaSet modifiers;

	ModifyOrAppendDamageCriteria(modifiers, info, false);
	ModifyOrAppendEnemyCriteria(modifiers, info.GetAttacker());
	SetSpeechTarget(pVictim);

	// Bad Cop needs to differentiate between human-like allies (soldiers, metrocops, etc.)
	// and non-human allies (hunters, manhacks, etc.) for some responses.
	if (pVictim->GetHullType() == HULL_HUMAN || pVictim->GetHullType() == HULL_WIDE_HUMAN)
		modifiers.AppendCriteria("speechtarget_humanoid", "1");

	SpeakIfAllowed(TLK_ALLY_KILLED, modifiers);
}

//-----------------------------------------------------------------------------
// Purpose: Event fired by NPCs killed by allies
//-----------------------------------------------------------------------------
void CEZ2_Player::AllyKilledEnemy( CBaseEntity *pAlly, CAI_BaseNPC *pVictim, const CTakeDamageInfo &info )
{
	AI_CriteriaSet modifiers;

	ModifyOrAppendDamageCriteria(modifiers, info, false);
	ModifyOrAppendEnemyCriteria(modifiers, pVictim);
	SetSpeechTarget(pAlly);

	if (m_iVisibleEnemies <= 1 && GetNPCComponent() && GetNPCComponent()->GetEnemy() == pVictim && GetMemoryComponent()->InEngagement())
	{
		ModifyOrAppendFinalEnemyCriteria( modifiers, pVictim, info );
	}

	SpeakIfAllowed(TLK_ALLY_KILLED_NPC, modifiers);
}

//-----------------------------------------------------------------------------
// Purpose: Event fired by NPCs when they're ignited
//-----------------------------------------------------------------------------
void CEZ2_Player::Event_NPCIgnited(CAI_BaseNPC *pVictim)
{
	if ( GetNPCComponent() && GetNPCComponent()->GetEnemy() == pVictim && (FInTrueViewCone(pVictim->WorldSpaceCenter()) && FVisible(pVictim)) )
	{
		SpeakIfAllowed( TLK_ENEMY_BURNING );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::Event_SeeEnemy( CBaseEntity *pEnemy )
{
	AI_CriteriaSet modifiers;

	ModifyOrAppendEnemyCriteria(modifiers, pEnemy);

	SpeakIfAllowed(TLK_STARTCOMBAT, modifiers);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::HandleAddToPlayerSquad( CAI_BaseNPC *pNPC )
{
	// See MIN_HUDHINT_DISPLAY_TIME in basecombatweapon_shared.cpp
	if (g_SightHintSoldierSquad.flLastHintTime != 0 && gpGlobals->curtime - g_SightHintSoldierSquad.flLastHintTime < 7.0f)
		SoldierHintHide( this, pNPC );

	SetSpeechTarget(pNPC);

	return SpeakIfAllowed(TLK_COMMAND_ADD);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::HandleRemoveFromPlayerSquad( CAI_BaseNPC *pNPC )
{
	SetSpeechTarget(pNPC);

	return SpeakIfAllowed(TLK_COMMAND_REMOVE);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::Weapon_HandleEquip( CBaseCombatWeapon *pWeapon )
{
	BaseClass::Weapon_HandleEquip( pWeapon );

	// Make sure Wilson doesn't remind us of our special weapons for at least 20 minutes
	if (FClassnameIs( pWeapon, "weapon_hopwire" ))
	{
		AddContext( "xen_grenade_thrown", "1", 1200.0f );
	}
	else if (FClassnameIs( pWeapon, "weapon_displacer_pistol" ))
	{
		AddContext( "displacer_used", "1", 1200.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Event fired upon picking up a new weapon
//-----------------------------------------------------------------------------
void CEZ2_Player::Event_FirstDrawWeapon( CBaseCombatWeapon *pWeapon )
{
	AI_CriteriaSet modifiers;
	ModifyOrAppendWeaponCriteria( modifiers, pWeapon );
	SpeakIfAllowed( TLK_NEWWEAPON, modifiers );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::Event_ThrewGrenade( CBaseCombatWeapon *pWeapon )
{
	if (FClassnameIs(pWeapon, "weapon_hopwire"))
	{
		// Take note of Xen grenade throws for 20 minutes
		AddContext("xen_grenade_thrown", "1", 1200.0f);
	}

	AI_CriteriaSet modifiers;

	ModifyOrAppendWeaponCriteria(modifiers, pWeapon);

	SpeakIfAllowed(TLK_THROWGRENADE, modifiers);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::Event_DisplacerPistolDisplace( CBaseCombatWeapon *pWeapon, CBaseEntity *pVictimEntity )
{
	AI_CriteriaSet modifiers;

	ModifyOrAppendWeaponCriteria( modifiers, pWeapon );
	ModifyOrAppendEnemyCriteria( modifiers, pVictimEntity );

	if (!SpeakIfAllowed( TLK_DISPLACER_DISPLACE, modifiers ))
	{
		CAI_PlayerAlly *pSquadRep = dynamic_cast<CAI_PlayerAlly*>(GetSquadCommandRepresentative());
		if (pSquadRep)
		{
			// Have a nearby soldier make a comment instead
			pSquadRep->SpeakIfAllowed( TLK_DISPLACER_DISPLACE, modifiers, true );
		}
	}

	// Take note of displacer usage for 20 minutes
	AddContext("displacer_used", "1", 1200.0f);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::Event_DisplacerPistolRelease( CBaseCombatWeapon *pWeapon, CBaseEntity *pReleaseEntity, CBaseEntity *pVictimEntity )
{
	AI_CriteriaSet modifiers;

	ModifyOrAppendWeaponCriteria( modifiers, pWeapon );
	ModifyOrAppendEnemyCriteria( modifiers, pVictimEntity );

	if (!SpeakIfAllowed( TLK_DISPLACER_RELEASE, modifiers ))
	{
		CAI_PlayerAlly *pSquadRep = dynamic_cast<CAI_PlayerAlly*>(GetSquadCommandRepresentative());
		if (pSquadRep)
		{
			// Have a nearby soldier make a comment instead
			pSquadRep->SpeakIfAllowed( TLK_DISPLACER_RELEASE, modifiers, true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when a game event is fired
//-----------------------------------------------------------------------------
void CEZ2_Player::FireGameEvent( IGameEvent *event )
{
	DevMsg("Player heard event\n");

	if ( FStrEq( "zombie_scream", event->GetName() ) )
	{
		CBaseEntity *pZombie = UTIL_EntityByIndex( event->GetInt("zombie") );
		CBaseEntity *pTarget = UTIL_EntityByIndex( event->GetInt("target") );

		if (pTarget == this)
		{
			// WTF IS WRONG WITH YOU ZOMBIE
			SetSpeechTarget( pZombie );
			SpeakIfAllowed( TLK_ZOMBIE_SCREAM );
		}
	}
	else if ( FStrEq( "vehicle_overturned", event->GetName() ) )
	{
		if (event->GetInt("userid") == GetUserID())
		{
			//CBaseEntity *pVehicle = UTIL_EntityByIndex( event->GetInt("vehicle") );

			SpeakIfAllowed( TLK_VEHICLE_OVERTURNED );
		}
	}
	else if ( FStrEq( "skill_changed", event->GetName() ) )
	{
		if (GetBonusChallenge() != EZ_CHALLENGE_NONE && !m_bMaskInterrupt && event->GetInt("skill_level", 0) != SKILL_HARD)
		{
			// Abort the challenge if the difficulty level is changed
			if (developer.GetInt() == 0)
			{
				RunScript( "SkillChanged()" );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::InputStartScripting( inputdata_t &inputdata )
{
	m_bInAScript = true;

	// Remove this once we've replaced gagging in all of the maps and talker files
	AddContext("gag", "1");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::InputStopScripting( inputdata_t &inputdata )
{
	m_bInAScript = false;

	// Remove this once we've replaced gagging in all of the maps and talker files
	AddContext("gag", "0");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::InputFinishBonusChallenge( inputdata_t &inputdata )
{
	// Really simple check to make sure this wasn't ent_fire'd through the basic command
	// (not that people can't cheat in other ways)
	if (inputdata.pActivator == GetWorldEntity())
		m_bBonusChallengeUpdate = true;
	else
		Warning( "Invalid activator\n" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::FireBullets( const FireBulletsInfo_t &info )
{
	BaseClass::FireBullets( info );

	if (GetBonusChallenge() == EZ_CHALLENGE_BULLETS)
	{
		SetBonusProgress( GetBonusProgress() + info.m_iShots );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::HasCheated()
{
	// Borrow the achievement manager for this
	CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>(engine->GetAchievementMgr());
	if (pAchievementMgr)
	{
		return pAchievementMgr->WereCheatsEverOn();
	}

	return false;
}

#define HIDEHUD_MASK_INTERRUPT_BITS (HIDEHUD_HEALTH | HIDEHUD_MISCSTATUS | HIDEHUD_CROSSHAIR | HIDEHUD_BONUS_PROGRESS)

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::HUDMaskInterrupt()
{
	m_Local.m_iHideHUD |= HIDEHUD_MASK_INTERRUPT_BITS;
	m_bMaskInterrupt = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::HUDMaskRestore()
{
	m_Local.m_iHideHUD &= ~(HIDEHUD_MASK_INTERRUPT_BITS);
	m_bMaskInterrupt = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::HidingBonusProgressHUD()
{
	return m_bMaskInterrupt /*|| GetBonusChallenge() >= EZ_CHALLENGE_SPECIAL*/;
}

//=============================================================================
// Bad Cop Speech System
// By Blixibon
// 
// A special speech AI system inspired by what CAI_PlayerAlly uses.
// Right now, this runs every 0.5 seconds on idle (0.25 on alert or combat) and reads our NPC component for NPC state, sensing, etc.
// This allows Bad Cop to react to danger and comment on things while he's idle.
//=============================================================================

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::DoSpeechAI( void )
{
	// First off, make sure we should be doing this AI
	if (IsInAScript())
		return;

	// If we're in notarget, don't comment on anything.
	if (GetFlags() & FL_NOTARGET)
		return;

	// Access our NPC component
	CAI_PlayerNPCDummy *pAI = GetNPCComponent();
	NPC_STATE iState = NPC_STATE_IDLE;
	if (pAI)
	{
		iState = pAI->GetState();

		// Has our NPC heard anything recently?
		AISoundIter_t iter;
		if (pAI->GetSenses()->GetFirstHeardSound( &iter ))
		{
			// Refresh sound conditions.
			pAI->OnListened();

			// First off, look for important sounds Bad Cop should react to immediately.
			// This is the "priority" version of sound sensing. Idle things like scents are handled in DoIdleSpeech().
			// Update CAI_PlayerNPCDummy::GetSoundInterests() if you want to add more.
			int iBestSound = SOUND_NONE;
			if (pAI->HasCondition(COND_HEAR_DANGER))
				iBestSound = SOUND_DANGER;
			else if (pAI->HasCondition(COND_HEAR_PHYSICS_DANGER))
				iBestSound = SOUND_PHYSICS_DANGER;

			if (iBestSound != SOUND_NONE)
			{
				CSound *pSound = pAI->GetBestSound(iBestSound);
				if (pSound)
				{
					if (ReactToSound(pSound, (GetAbsOrigin() - pSound->GetSoundReactOrigin()).Length()))
						return;
				}
			}
		}

		// Do other things if our NPC is idle
		switch (iState)
		{
			case NPC_STATE_IDLE:
			{
				if (DoIdleSpeech())
					return;
			} break;

			case NPC_STATE_COMBAT:
			{
				if (DoCombatSpeech())
					return;
			} break;
		}
	}

	float flRandomSpeechModifier = GetSpeechFilter() ? GetSpeechFilter()->GetIdleModifier() : 1.0f;

	if ( flRandomSpeechModifier > 0.0f )
	{
		int iChance = (int)(RandomFloat(0, 10) * flRandomSpeechModifier);

		// 2% chance by default
		if (iChance > RandomInt(0, 500))
		{
			// Find us a random speech target
			SetSpeechTarget( FindSpeechTarget() );

			switch (iState)
			{
				case NPC_STATE_IDLE:
				case NPC_STATE_ALERT:
					SpeakIfAllowed(TLK_IDLE); break;

				case NPC_STATE_COMBAT:
					SpeakIfAllowed(TLK_ATTACKING); break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::DoIdleSpeech()
{
	float flHealthPerc = ((float)m_iHealth / (float)m_iMaxHealth);
	if ( flHealthPerc < 0.5f )
	{
		// Find us a random speech target
		SetSpeechTarget( FindSpeechTarget() );

		// Bad Cop could be feeling pretty shit.
		if ( SpeakIfAllowed( TLK_PLHURT ) )
			return true;
	}

	// We shouldn't be this far if we don't have a NPC component
	CAI_PlayerNPCDummy *pAI = GetNPCComponent();
	Assert( pAI );

	// Has our NPC heard anything recently?
	AISoundIter_t iter;
	if (pAI->GetSenses()->GetFirstHeardSound( &iter ))
	{
		// Refresh sound conditions.
		pAI->OnListened();

		// React to the little things in life that Bad Cop cares about.
		// This is the idle version of sound sensing. Priority things like danger are handled in DoSpeechAI().
		// Update CAI_PlayerNPCDummy::GetSoundInterests() if you want to add more.
		int iBestSound = SOUND_NONE;
		if (pAI->HasCondition(COND_HEAR_SPOOKY))
			iBestSound = SOUND_COMBAT;
		else if (pAI->HasCondition(COND_SMELL))
			iBestSound = (SOUND_MEAT | SOUND_CARCASS);

		if (iBestSound != SOUND_NONE)
		{
			CSound *pSound = pAI->GetBestSound(iBestSound);
			if (pSound)
			{
				if (ReactToSound(pSound, (GetAbsOrigin() - pSound->GetSoundReactOrigin()).Length()))
					return true;
			}
		}
	}

	// Check if we're staring at something
	if (GetSmoothedVelocity().LengthSqr() < Square( 100 ) && !GetUseEntity())
	{
		// First, reset our staring entity
		CBaseEntity *pLastStaring = GetStaringEntity();
		SetStaringEntity( NULL );

		// If our eye angles haven't changed much, and we're not running a scripted scene, it could mean we're intentionally looking at something
		if (QAnglesAreEqual(EyeAngles(), m_angLastStaringEyeAngles, 5.0f) && !IsRunningScriptedSceneWithSpeechAndNotPaused( this, true ))
		{
			// Trace a line 96 units in front of ourselves to see what we're staring at
			trace_t tr;
			Vector vecEyeDirection = EyeDirection3D();
			UTIL_TraceLine(EyePosition(), EyePosition() + vecEyeDirection * 96.0f, MASK_BLOCKLOS_AND_NPCS, this, COLLISION_GROUP_NONE, &tr);

			if (tr.m_pEnt && !tr.m_pEnt->IsWorld())
			{
				// Make sure we're looking at its "face" more-so than its origin
				if (!tr.m_pEnt->IsCombatCharacter() || (tr.m_pEnt->EyePosition() - tr.endpos).LengthSqr() < (tr.m_pEnt->GetAbsOrigin() - tr.endpos).LengthSqr())
				{
					if (tr.m_pEnt != pLastStaring)
					{
						// We're staring at a new entity
						m_flCurrentStaringTime = gpGlobals->curtime;
					}

					SetStaringEntity(tr.m_pEnt);
				}
			}
		}

		m_angLastStaringEyeAngles = EyeAngles();

		if (GetStaringEntity())
		{
			// Check if we've been staring for longer than 3 seconds
			if (gpGlobals->curtime - m_flCurrentStaringTime > 3.0f)
			{
				// We're staring at something
				AI_CriteriaSet modifiers;

				SetSpeechTarget(GetStaringEntity());

				modifiers.AppendCriteria("staretime", UTIL_VarArgs("%f", gpGlobals->curtime - m_flCurrentStaringTime));

				if (SpeakIfAllowed(TLK_STARE, modifiers))
					return true;
			}
		}
		else
		{
			// Reset staring time
			m_flCurrentStaringTime = FLT_MAX;
		}
	}

	// TLK_IDLE is handled in DoSpeechAI(), so there's nothing else we could say.
	return false;

	// We could use something like this somewhere (separated since Speak() does all of this anyway)
	//AISpeechSelection_t selection;
	//if ( SelectSpeechResponse(TLK_IDLE, NULL, NULL, &selection) )
	//{
	//	if (SpeakDispatchResponse(selection.concept.c_str(), selection.pResponse))
	//		return true;
	//}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::DoCombatSpeech()
{
	// We shouldn't be this far if we don't have a NPC component
	CAI_PlayerNPCDummy *pAI = GetNPCComponent();
	Assert( pAI );

	// Comment on enemy counts
	if ( pAI->HasCondition( COND_MOBBED_BY_ENEMIES ) )
	{
		// Find us a random speech target
		SetSpeechTarget( FindSpeechTarget() );

		if (SpeakIfAllowed( TLK_MOBBED ))
			return true;
	}
	else if ( m_iVisibleEnemies >= 5 && m_iCloseEnemies < 3 )
	{
		if (GetNPCComponent()->GetEnemy() && GetNPCComponent()->GetEnemy()->GetEnemy() == this)
		{
			// Bad Cop sees 5+ enemies and they (or at least one) are targeting him in particular
			// (indicates they're not idle and not distracted)
			if (SpeakIfAllowed( TLK_MANY_ENEMIES ))
				return true;
		}
	}

#if 0 // Bad Cop doesn't announce reloading for now.
	if ( GetActiveWeapon() )
	{
		if (GetActiveWeapon()->m_bInReload && (GetActiveWeapon()->Clip1() / GetActiveWeapon()->GetMaxClip1()) < 0.5f && !IsSpeaking())
		{
			// Bad Cop announces reloading
			if (SpeakIfAllowed( TLK_HIDEANDRELOAD ))
				return true;
		}
	}
#endif

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Counts enemies from our NPC component, based off of Alyx's enemy counting/mobbing implementaton.
//-----------------------------------------------------------------------------
void CEZ2_Player::MeasureEnemies(int &iVisibleEnemies, int &iCloseEnemies)
{
	// We shouldn't be this far if we don't have a NPC component
	CAI_PlayerNPCDummy *pAI = GetNPCComponent();
	Assert( pAI );

	CAI_Enemies *pEnemies = pAI->GetEnemies();
	Assert( pEnemies );

	iVisibleEnemies = 0;
	iCloseEnemies = 0;

	// This is a simplified version of Alyx's mobbed AI found in CNPC_Alyx::DoMobbedCombatAI().
	// This isn't as expensive as it looks.
	AIEnemiesIter_t iter;
	for ( AI_EnemyInfo_t *pEMemory = pEnemies->GetFirst(&iter); pEMemory != NULL; pEMemory = pEnemies->GetNext(&iter) )
	{
		if ( IRelationType( pEMemory->hEnemy ) <= D_FR && pEMemory->hEnemy->GetAbsOrigin().DistToSqr(GetAbsOrigin()) <= PLAYER_MIN_ENEMY_CONSIDER_DIST &&
			pEMemory->hEnemy->IsAlive() && gpGlobals->curtime - pEMemory->timeLastSeen <= 5.0f )
		{
			Class_T classify = pEMemory->hEnemy->Classify();
			if (classify == CLASS_BARNACLE || classify == CLASS_BULLSEYE)
				continue;

			iVisibleEnemies += 1;

			if( pEMemory->hEnemy->GetAbsOrigin().DistToSqr(GetAbsOrigin()) <= PLAYER_MIN_MOB_DIST_SQR )
			{
				iCloseEnemies += 1;
				pEMemory->bMobbedMe = true;
			}
		}
	}

	// Set the NPC component's mob condition here.
	if( iCloseEnemies >= PLAYER_ENEMY_MOB_COUNT )
	{
		pAI->SetCondition( COND_MOBBED_BY_ENEMIES );
	}
	else
	{
		pAI->ClearCondition( COND_MOBBED_BY_ENEMIES );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Appends sound criteria
//-----------------------------------------------------------------------------
void CEZ2_Player::ModifyOrAppendSoundCriteria(AI_CriteriaSet & set, CSound *pSound, float flDist)
{
	set.AppendCriteria( "sound_distance", UTIL_VarArgs("%f", flDist ) );

	set.AppendCriteria( "sound_type", UTIL_VarArgs("%i", pSound->SoundType()) );

	if (pSound->m_hOwner)
	{
		if (pSound->SoundChannel() == SOUNDENT_CHANNEL_ZOMBINE_GRENADE)
		{
			// Pretend the owner is a grenade (the zombine will be the enemy)
			set.AppendCriteria( "sound_owner", "npc_grenade_frag" );
		}
		else
		{
			set.AppendCriteria( "sound_owner", UTIL_VarArgs("%s", pSound->m_hOwner->GetClassname()) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::ReactToSound( CSound *pSound, float flDist )
{
	// Do not react to our own sounds, or sounds produced by the vehicle we're in
	if (pSound->m_hOwner == this || pSound->m_hOwner == GetVehicleEntity())
		return false;

	AI_CriteriaSet set;
	ModifyOrAppendSoundCriteria(set, pSound, flDist);

	if (pSound->m_iType & SOUND_DANGER)
	{
		CBaseEntity *pOwner = pSound->m_hOwner.Get();
		CBaseGrenade *pGrenade = dynamic_cast<CBaseGrenade*>(pOwner);
		if (pGrenade)
			pOwner = pGrenade->GetThrower();

		// Only danger sounds with no owner or an owner we don't like are counted
		// (no reacting to danger from self or allies)
		if (!pOwner || IRelationType(pOwner) <= D_FR)
		{
			if (pOwner)
				ModifyOrAppendEnemyCriteria(set, pOwner);

			return SpeakIfAllowed(TLK_DANGER, set);
		}
	}
	else if (pSound->m_iType & (SOUND_MEAT | SOUND_CARCASS))
	{
		return SpeakIfAllowed(TLK_SMELL, set);
	}
	else if (pSound->m_iType & SOUND_COMBAT && pSound->SoundChannel() == SOUNDENT_CHANNEL_SPOOKY_NOISE)
	{
		// Bad Cop is creeped out
		return SpeakIfAllowed( TLK_DARKNESS_HEARDSOUND, set );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
HSCRIPT CEZ2_Player::ScriptGetNPCComponent()
{
	return ToHScript( GetNPCComponent() );
}

HSCRIPT CEZ2_Player::ScriptGetStaringEntity()
{
	return ToHScript( GetStaringEntity() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CEZ2_Player::GetEnemy()
{
	return GetNPCComponent() ? GetNPCComponent()->GetEnemy() : NULL;
}

NPC_STATE CEZ2_Player::GetState()
{
	return GetNPCComponent() ? GetNPCComponent()->GetState() : NPC_STATE_NONE;
}

//=============================================================================
// Bad Cop Memory Component
// By Blixibon
// 
// Carries miscellaneous "memory" information that Bad Cop should remember later on for dialogue.
// 
// For example, the player's health is recorded before they begin combat.
// When combat ends, the response system subtracts the player's current health from the health recorded here,
// creating a "health difference" that indicates how much health the player lost in the engagement.
//=============================================================================
BEGIN_SIMPLE_DATADESC( CEZ2_PlayerMemory )

	DEFINE_FIELD( m_bInEngagement, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flEngagementStartTime, FIELD_TIME ),
	DEFINE_FIELD( m_iPrevHealth, FIELD_INTEGER ),

	DEFINE_FIELD( m_iNumEnemiesHistoric, FIELD_INTEGER ),

	DEFINE_FIELD( m_iComboEnemies, FIELD_INTEGER ),
	DEFINE_FIELD( m_flLastEnemyDeadTime, FIELD_TIME ),

	DEFINE_FIELD( m_iLastDamageType, FIELD_INTEGER ),
	DEFINE_FIELD( m_iLastDamageAmount, FIELD_INTEGER ),
	DEFINE_FIELD( m_hLastDamageAttacker, FIELD_EHANDLE ),

	DEFINE_FIELD( m_hOuter, FIELD_EHANDLE ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_PlayerMemory::InitLastDamage(const CTakeDamageInfo &info)
{
	m_iLastDamageType = info.GetDamageType();

	if (info.GetAttacker() == m_hLastDamageAttacker)
	{
		// Just add it on
		m_iLastDamageAmount += (int)(info.GetDamage());
	}
	else
	{
		m_iLastDamageAmount = (int)(info.GetDamage());
		m_hLastDamageAttacker = info.GetAttacker();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_PlayerMemory::RecordEngagementStart()
{
	if (!InEngagement())
	{
		m_iPrevHealth = GetOuter()->GetHealth();
		m_bInEngagement = true;
		m_flEngagementStartTime = gpGlobals->curtime;
	}
}

void CEZ2_PlayerMemory::RecordEngagementEnd()
{
	m_bInEngagement = false;
	m_iNumEnemiesHistoric = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_PlayerMemory::KilledEnemy()
{
	if (gpGlobals->curtime - m_flLastEnemyDeadTime > 5.0f)
		m_iComboEnemies = 0;

	m_iComboEnemies++;
	m_flLastEnemyDeadTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_PlayerMemory::AppendLastDamageCriteria( AI_CriteriaSet& set )
{
	set.AppendCriteria( "lasttaken_damagetype", UTIL_VarArgs( "%i", GetLastDamageType() ) );
	set.AppendCriteria( "lasttaken_damage", UTIL_VarArgs( "%i", GetLastDamageAmount() ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_PlayerMemory::AppendKilledEnemyCriteria( AI_CriteriaSet& set )
{
	set.AppendCriteria("killcombo", CNumStr(m_iComboEnemies));
}

//=============================================================================
// Bad Cop "Dummy" NPC Template Class
// By Blixibon
// 
// So, you remember that whole "Sound Sensing System" thing that allowed Bad Cop to hear sounds?
// One of my ideas to implement it was some sort of "dummy" NPC that "heard" things for Bad Cop.
// I decided not to go for it because it added an extra entity, there weren't many reasons to add it, etc.
// Now we need Bad Cop to know when he's idle or alert, Will-E's code can be re-used and drawn from it, and I finally decided to just do it.
//
// This is a "dummy" NPC only meant to hear sounds and change states, intended to be a "component" for the player.
//=============================================================================

BEGIN_DATADESC(CAI_PlayerNPCDummy)

DEFINE_FIELD(m_hOuter, FIELD_EHANDLE),

END_DATADESC()

LINK_ENTITY_TO_CLASS(player_npc_dummy, CAI_PlayerNPCDummy);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_PlayerNPCDummy::Spawn( void )
{
	BaseClass::Spawn();

	// This is a dummy model that is never used!
	UTIL_SetSize(this, Vector(-16,-16,-16), Vector(16,16,16));

	// What the player uses by default
	m_flFieldOfView = 0.766;

	SetMoveType( MOVETYPE_NONE );
	ClearEffects();
	SetGravity( 0.0 );

	AddEFlags( EFL_NO_DISSOLVE );

	SetSolid( SOLID_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );
	m_takedamage = DAMAGE_NO;

	AddEffects( EF_NODRAW );

	if (player_dummy_in_squad.GetBool())
	{
		// Put us in the player's squad
		CapabilitiesAdd(bits_CAP_SQUAD);
		AddToSquad( AllocPooledString(PLAYER_SQUADNAME) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Higher priority for enemies the player is actually aiming at
//-----------------------------------------------------------------------------
int CAI_PlayerNPCDummy::IRelationPriority( CBaseEntity *pTarget )
{
	// Draw from our outer for the base priority
	int iPriority = GetOuter()->IRelationPriority(pTarget);

	Vector los = ( pTarget->WorldSpaceCenter() - EyePosition() );
	Vector facingDir = EyeDirection3D();
	float flDot = DotProduct( los, facingDir );

	if ( flDot > 0.75f )
		iPriority += 1;
	if ( flDot > 0.9f )
		iPriority += 1;

	return iPriority;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_PlayerNPCDummy::ModifyOrAppendOuterCriteria( AI_CriteriaSet & set )
{
	// CAI_ExpresserHost uses its own complicated take on NPC state, but
	// considering the other response enums works fine, I think it might be from another era.
	set.AppendCriteria( "npcstate", UTIL_VarArgs( "%i", m_NPCState ) );

	if ( GetLastEnemyTime() == 0.0 )
		set.AppendCriteria( "timesincecombat", "999999.0" );
	else
		set.AppendCriteria( "timesincecombat", UTIL_VarArgs( "%f", gpGlobals->curtime - GetLastEnemyTime() ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_PlayerNPCDummy::RunAI( void )
{
	if (GetOuter()->GetFlags() & FL_NOTARGET)
	{
		SetActivity( ACT_IDLE );
		return;
	}

	BaseClass::RunAI();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_PlayerNPCDummy::GatherEnemyConditions( CBaseEntity *pEnemy )
{
	BaseClass::GatherEnemyConditions( pEnemy );

	if ( GetLastEnemyTime() == 0 || gpGlobals->curtime - GetLastEnemyTime() > 30 )
	{
		if ( HasCondition( COND_SEE_ENEMY ) && pEnemy->Classify() != CLASS_BULLSEYE && !(GetOuter()->GetFlags() & FL_NOTARGET) )
		{
			GetOuter()->Event_SeeEnemy(pEnemy);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_PlayerNPCDummy::TranslateSchedule( int scheduleType ) 
{
	switch( scheduleType )
	{
	case SCHED_ALERT_STAND:
		return SCHED_PLAYERDUMMY_ALERT_STAND;
		break;
	}

	return scheduleType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_PlayerNPCDummy::OnStateChange( NPC_STATE OldState, NPC_STATE NewState )
{
	BaseClass::OnStateChange( OldState, NewState );

	if (GetOuter())
	{
		if (NewState == NPC_STATE_COMBAT && OldState < NPC_STATE_COMBAT)
		{
			// The player is entering combat
			GetOuter()->GetMemoryComponent()->RecordEngagementStart();
		}
		else if (GetOuter()->GetMemoryComponent()->InEngagement() && (NewState == NPC_STATE_ALERT || NewState == NPC_STATE_IDLE))
		{
			// The player is probably exiting combat
			GetOuter()->GetMemoryComponent()->RecordEngagementEnd();
		}

		if ( OldState == NPC_STATE_IDLE )
		{
			// Remove the player from non-idle scenes
			RemoveActorFromScriptedScenes( GetOuter(), true, true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this NPC can hear the specified sound
//-----------------------------------------------------------------------------
bool CAI_PlayerNPCDummy::QueryHearSound( CSound *pSound )
{
	if ( pSound->m_hOwner != NULL )
	{
		// We can't hear sounds emitted directly by our player.
		if ( pSound->m_hOwner.Get() == GetOuter() )
			return false;

		// We can't hear sounds emitted directly by a vehicle driven by our player or by nobody.
		IServerVehicle *pVehicle = pSound->m_hOwner->GetServerVehicle();
		if ( pVehicle && (!pVehicle->GetPassenger(VEHICLE_ROLE_DRIVER) || pVehicle->GetPassenger(VEHICLE_ROLE_DRIVER) == GetOuter()) )
			return false;
	}

	return BaseClass::QueryHearSound( pSound );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this NPC can see the specified entity
//-----------------------------------------------------------------------------
bool CAI_PlayerNPCDummy::QuerySeeEntity( CBaseEntity *pEntity, bool bOnlyHateOrFearIfNPC )
{
	if ( pEntity->IsNPC() )
	{
		// Under regular circumstances, the player should pick up on all NPCs, regardless of relationship.
		// This is so the player dummy can see commandable soldiers for things like automated HUD hints.
		if ( bOnlyHateOrFearIfNPC && (pEntity->Classify() == CLASS_BULLSEYE) )
		{
			Disposition_t disposition = IRelationType( pEntity );
			return ( disposition == D_HT || disposition == D_FR );
		}
	}
	else if ( pEntity == GetOuter() )
	{
		// Do not see our player
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Update information on my enemy
// Input  :
// Output : Returns true is new enemy, false is known enemy
//-----------------------------------------------------------------------------
bool CAI_PlayerNPCDummy::UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer )
{
	if ( BaseClass::UpdateEnemyMemory(pEnemy, position, pInformer) )
	{
		// New enemy, tell memory component
		if (GetOuter())
			GetOuter()->GetMemoryComponent()->IncrementHistoricEnemies();

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_PlayerNPCDummy::DrawDebugGeometryOverlays( void )
{
	BaseClass::DrawDebugGeometryOverlays();

	// Highlight enemies
	AIEnemiesIter_t iter;
	for ( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst(&iter); pEMemory != NULL; pEMemory = GetEnemies()->GetNext(&iter) )
	{
		if (pEMemory->hEnemy == GetEnemy())
		{
			NDebugOverlay::EntityBounds(pEMemory->hEnemy, 0, 255, 0, 15 * IRelationPriority(pEMemory->hEnemy), 0);
		}
		else
		{
			NDebugOverlay::EntityBounds(pEMemory->hEnemy, 0, 0, 255, 15 * IRelationPriority(pEMemory->hEnemy), 0);
		}
	}
}

extern ConVar ai_debug_remarkables;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_PlayerNPCDummy::OnSeeEntity( CBaseEntity * pEntity )
{
	if ( pEntity->IsRemarkable() )
	{
		CInfoRemarkable * pRemarkable = dynamic_cast<CInfoRemarkable *>(pEntity);
		if ( pRemarkable != NULL )
		{
			AI_CriteriaSet modifiers = pRemarkable->GetModifiers( GetOuter() );
			if ( GetOuter()->SpeakIfAllowed( TLK_REMARK, modifiers ) )
			{
				pRemarkable->OnRemarked();

				if (ai_debug_remarkables.GetBool())
				{
					Msg( "%s noticed remarkable %s and remarked on it!\n", GetDebugName(), pRemarkable->GetDebugName() );
					if (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT)
					{
						pRemarkable->DrawDebugGeometryOverlays();
					}
				}
			}
			else
			{
				if (ai_debug_remarkables.GetBool())
				{
					Msg( "%s noticed remarkable %s, but could not remark on it\n", GetDebugName(), pRemarkable->GetDebugName() );

					if (m_debugOverlays & OVERLAY_NPC_SELECTED_BIT)
					{
						pRemarkable->DrawDebugGeometryOverlays();
					}
				}
			}
		}
	}

	BaseClass::OnSeeEntity( pEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_PlayerNPCDummy::IsValidEnemy( CBaseEntity *pEnemy )
{
	if (!BaseClass::IsValidEnemy( pEnemy ))
		return false;

	// Just completely ignore bullseyes for now
	// (fixes an issue where the NPC component always regards any bullseye as an enemy)
	if (pEnemy->Classify() == CLASS_BULLSEYE)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( player_npc_dummy, CAI_PlayerNPCDummy )

	DEFINE_SCHEDULE
	(
		SCHED_PLAYERDUMMY_ALERT_STAND,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_REASONABLE		0"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
		"		TASK_WAIT					5" // Don't wait very long
		"		TASK_SUGGEST_STATE			STATE:IDLE"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_COMBAT"		// sound flags
		"		COND_HEAR_DANGER"
		"		COND_HEAR_BULLET_IMPACT"
		"		COND_IDLE_INTERRUPT"
	);

AI_END_CUSTOM_NPC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

BEGIN_DATADESC( CPointDetonatable )

	DEFINE_KEYFIELD( m_bDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_KEYFIELD( m_iszThrower, FIELD_STRING, "thrower" ),
	DEFINE_FIELD( m_hThrower, FIELD_EHANDLE ),
	DEFINE_KEYFIELD( m_iszGlowTarget, FIELD_STRING, "glowtarget" ),
	DEFINE_FIELD( m_hGlowTarget, FIELD_EHANDLE ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "SetGlowTarget", InputSetGlowTarget ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Detonate", InputDetonate ),

	DEFINE_OUTPUT( m_OnDetonate, "OnDetonate" ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CPointDetonatable, DT_PointDetonatable )
	SendPropBool( SENDINFO( m_bDisabled ) ),
	SendPropEHandle( SENDINFO( m_hThrower ) ),
	SendPropEHandle( SENDINFO( m_hGlowTarget ) ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( point_detonatable, CPointDetonatable );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPointDetonatable::CPointDetonatable()
{
	m_bDisabled = false;
}

CPointDetonatable::~CPointDetonatable()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointDetonatable::Spawn()
{
	BaseClass::Spawn();

	// Default to local player
	if (m_iszThrower == NULL_STRING)
	{
		m_iszThrower = AllocPooledString( "!player" );
		m_hThrower = UTIL_GetLocalPlayer();
	}
	else
		m_hThrower = dynamic_cast<CBaseCombatCharacter*>(gEntList.FindEntityByName( NULL, m_iszThrower, this ));
	
	if (!m_bDisabled)
	{
		VerifyGlowTarget( this, this );

		if (m_hThrower && m_hThrower->IsPlayer())
		{
			static_cast<CHL2_Player*>(m_hThrower.Get())->OnSetupDetonatable( this );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Transmit data
//-----------------------------------------------------------------------------
int CPointDetonatable::UpdateTransmitState( void )
{
	if ( m_hThrower && m_hThrower->IsPlayer() )
	{
		// Player detonatables need to be transmitted for HUD glow
		return SetTransmitState( FL_EDICT_ALWAYS );
	}
	else
	{
		return BaseClass::UpdateTransmitState();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointDetonatable::VerifyGlowTarget( CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	if (!m_hGlowTarget)
		m_hGlowTarget = gEntList.FindEntityByName( NULL, m_iszGlowTarget, this, pActivator, pCaller );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointDetonatable::InputEnable( inputdata_t &inputdata )
{
	if (!m_bDisabled)
		return;

	VerifyGlowTarget( inputdata.pActivator, inputdata.pCaller );
	m_bDisabled = false;
	
	if (!m_hThrower)
		m_hThrower = dynamic_cast<CBaseCombatCharacter*>(gEntList.FindEntityByName( NULL, m_iszThrower, this, inputdata.pActivator, inputdata.pCaller ));
	
	if (m_hThrower && m_hThrower->IsPlayer())
	{
		static_cast<CHL2_Player*>(m_hThrower.Get())->OnSetupDetonatable( this );
	}
}

void CPointDetonatable::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
	
	if (m_hThrower && m_hThrower->IsPlayer())
	{
		static_cast<CHL2_Player*>(m_hThrower.Get())->OnDetonatableDisabled( this );
	}
}

void CPointDetonatable::InputSetGlowTarget( inputdata_t &inputdata )
{
	m_hGlowTarget = inputdata.value.Entity();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointDetonatable::InputDetonate( inputdata_t &inputdata )
{
	if (m_bDisabled)
		return;

	if (!m_hThrower)
		m_hThrower = dynamic_cast<CBaseCombatCharacter*>(gEntList.FindEntityByName( NULL, m_iszThrower, this, inputdata.pActivator, inputdata.pCaller ));

	if (m_hThrower && m_hThrower->IsPlayer())
	{
		CHL2_Player *pHL2Player = static_cast<CHL2_Player*>(m_hThrower.Get());
		if (pHL2Player)
		{
			pHL2Player->OnDetonatableExploded( this, inputdata.pActivator );
		}
	}

	m_OnDetonate.FireOutput( inputdata.pActivator, this );

	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: Debugging commands
//-----------------------------------------------------------------------------

CON_COMMAND( player_reset_sight_events, "Resets the cooldown on all player sight events" )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if (pPlayer)
	{
		CUtlVector<SightEvent_t*> *pSightEvents = ((CEZ2_Player*)pPlayer)->GetSightEvents();
		for (int i = 0; i < pSightEvents->Count(); i++)
		{
			pSightEvents->Element(i)->flNextHintTime = gpGlobals->curtime;
		}
	}
}

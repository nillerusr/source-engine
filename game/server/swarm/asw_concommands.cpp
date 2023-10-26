#include "cbase.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "asw_marine_resource.h"
#include "asw_marine_speech.h"
#include "asw_gamerules.h"
#include "asw_door.h"
#include "asw_spawner.h"
#include "asw_weapon.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "asw_remote_turret_shared.h"
#include "physics_prop_ragdoll.h"
#include "ai_network.h"
#include "asw_util_shared.h"
#include "gib.h"
#include "missionchooser/iasw_mission_chooser.h"
#include "missionchooser/iasw_random_missions.h"
#include "datacache/imdlcache.h"
#include "asw_spawn_manager.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// This file contains various debugging and cheat concommands

void cc_CreatePredictionError_f()
{
	CBaseEntity *pEnt = CBaseEntity::Instance( 1 );
	pEnt->SetAbsOrigin( pEnt->GetAbsOrigin() + Vector( 63, 0, 0 ) );
}

ConCommand cc_CreatePredictionError( "CreatePredictionError", cc_CreatePredictionError_f, "Create a prediction error", FCVAR_CHEAT );

void MarineHealths()
{
	CBaseEntity* pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "asw_marine" )) != NULL)
	{
		CASW_Marine* marine = CASW_Marine::AsMarine( pEntity );
		if (marine)
		{
			Msg("Marine health is %d\n", marine->GetHealth());
		}
	}
}

ConCommand MarineHealthscc( "MarineHealths", MarineHealths, "List health of all marines", FCVAR_CHEAT );

void ShootMe()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if ( pPlayer->GetMarine() )
	{
		CTakeDamageInfo dmgInfo( pPlayer, pPlayer, 3, DMG_BULLET );
		Vector vecDir = RandomVector(-1, 1);
		trace_t tr;
		pPlayer->GetMarine()->DispatchTraceAttack( dmgInfo, vecDir, &tr );
	}
}

ConCommand ShootMecc( "ShootMe", ShootMe, "Does DMG_BULLET to marine", FCVAR_CHEAT );

/*
void SimpleAIPathTestf()
{
CBasePlayer *pPlayer = UTIL_GetCommandClient();
if (g_FirstSimpleAI && pPlayer)
{

//int npc_node = g_FirstSimpleAI->GetPathfinder()->NearestNodeToNPC();
//int player_node = g_FirstSimpleAI->GetPathfinder()->NearestNodeToPoint(pPlayer->GetAbsOrigin());
//Msg("Doing path test between nodes %d and %d\n", npc_node, player_node);
//AI_Waypoint_t* waypoint = g_FirstSimpleAI->GetPathfinder()->FindBestPath(npc_node, player_node);

AI_Waypoint_t* waypoint = g_FirstSimpleAI->GetPathfinder()->BuildRoute(g_FirstSimpleAI->GetAbsOrigin(),
pPlayer->GetAbsOrigin(), pPlayer, 5*12, NAV_NONE, true);
//BuildRoute( const Vector &vStart, const Vector &vEnd, CBaseEntity *pTarget, float goalTolerance, Navigation_t curNavType = NAV_NONE, bool bLocalSucceedOnWithinTolerance = false );
if (waypoint)
Msg("got a waypoint\n");
else Msg("got no waypoint\n");

Vector lastpos = g_FirstSimpleAI->GetAbsOrigin();
while (waypoint)		
{
DebugDrawLine(lastpos, waypoint->GetPos(), 255,0,255,false, 10.0f);
lastpos = waypoint->GetPos();
waypoint = waypoint->GetNext();
//Msg("got a waypoint, drawing line to it!\n");
//DebugDrawLine(pPlayer->GetAbsOrigin() + Vector(0,0,50), waypoint->GetPos() + Vector(0,0,50), 255,0,255,false, 10.0f);
}

}
else
{
Msg("Couldn't do path test.  player=%d simpleai=%d\n", pPlayer, g_FirstSimpleAI);
}
}

ConCommand SimpleAIPathTest( "SimpleAIPathTest", SimpleAIPathTestf, "makes the first simple ai path to the player", FCVAR_CHEAT );

void SimpleAIMoveTestf()
{
CBasePlayer *pPlayer = UTIL_GetCommandClient();
if (g_FirstSimpleAI && pPlayer)
{
//g_FirstSimpleAI->StartMoveTest(pPlayer->GetAbsOrigin(), pPlayer);
//g_FirstSimpleAI->SetDirectDestination(pPlayer->GetAbsOrigin());
g_FirstSimpleAI->SetSimpleEnemy(pPlayer);
g_FirstSimpleAI->SetSimpleAIState(ASAS_CHASING);
}
}
ConCommand SimpleAIMoveTest( "SimpleAIMoveTest", SimpleAIMoveTestf, "makes the first simple ai path to the player", FCVAR_CHEAT );

void ShowAntlionVelocityf()
{
CBaseEntity *pAntlion = gEntList.FindEntityByClassname( NULL, "npc_antlion" );

while( pAntlion )
{
char buffer[256];
Q_snprintf(buffer, sizeof(buffer), "x=%f y=%f z=%f", pAntlion->GetAbsVelocity().x, 
pAntlion->GetAbsVelocity().y,
pAntlion->GetAbsVelocity().z);
pAntlion->AddTimedOverlay(buffer, 3.0f);
pAntlion = gEntList.FindEntityByClassname( pAntlion, "npc_antlion" );
}
}
ConCommand ShowAntlionVelocity( "ShowAntlionVelocity", ShowAntlionVelocityf, "show velocity of antlions", FCVAR_CHEAT );
*/


void HurtMyMarinef()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());

	if (pPlayer && pPlayer->GetMarine())
	{
		CASW_Marine *marine = pPlayer->GetMarine();
		CTakeDamageInfo damageinfo( marine, marine,
			Vector(1,1,1), marine->GetAbsOrigin(), 10, DMG_BULLET );
		/*vDamagePosition, m_iDamage, DMG_SLASH );
		CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker,
		float flDamage, int bitsDamageType, int iKillType = 0 );
		CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker,
		const Vector &damageForce, const Vector &damagePosition, float flDamage,
		int bitsDamageType, int iKillType = 0, Vector *reportedPosition = NULL );*/
		pPlayer->GetMarine()->TakeDamage(damageinfo);		
	}
}
ConCommand HurtMyMarine( "HurtMyMarine", HurtMyMarinef, "Gives your marine 1-10 damage", FCVAR_CHEAT );


void asw_LeaveMarinef()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());;

	if (pPlayer && pPlayer->GetMarine())
	{
		pPlayer->LeaveMarines();
	}
}
ConCommand LeaveMarine( "asw_LeaveMarine", asw_LeaveMarinef, "Uninhabits your current marine", FCVAR_CHEAT );

void asw_StartStimf()
{
	ASWGameRules()->StartStim( 4.0f, NULL );
}
ConCommand StartStim( "asw_StartStim", asw_StartStimf, "Activates a stim pack", FCVAR_CHEAT );

void asw_ClearHousef()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if (!pPlayer)
		return;

	// remove all alien NPCs
	if ( ASWSpawnManager() )
	{
		int nCount = ASWSpawnManager()->GetNumAlienClasses();
		for ( int i = 0; i < nCount; i++ )
		{
			engine->ClientCommand( pPlayer->edict(), CFmtStr( "ent_remove_all %s", ASWSpawnManager()->GetAlienClass(i)->m_pszAlienClass ) );
		}
	}

	// remove non NPC types
	engine->ClientCommand(pPlayer->edict(), "ent_remove_all asw_alien_goo");
	engine->ClientCommand(pPlayer->edict(), "ent_remove_all asw_grub_sac");
	engine->ClientCommand(pPlayer->edict(), "ent_remove_all asw_spawner");
	engine->ClientCommand(pPlayer->edict(), "ent_remove_all asw_egg");
}
ConCommand ClearHouse( "asw_ClearHouse", asw_ClearHousef, "Removes all Swarm from the map", FCVAR_CHEAT );

void asw_MarineInvulnf()
{
	ASWGameRules()->MarineInvuln();
}
ConCommand MarineInvuln( "asw_MarineInvuln", asw_MarineInvulnf, "Makes your marines invulnerable", FCVAR_CHEAT );

void ASW_DropTest_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());;
	
	if (pPlayer && pPlayer->GetMarine())
	{
		CASW_Marine *pMarine = pPlayer->GetMarine();
		if (pMarine->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
			return;
		if (pPlayer->GetFlags() & FL_FROZEN)
			return;
		int c = pMarine->WeaponCount();
		int current = -1;
		//int target = 0;
		for (int i=0;i<c;i++)
		{
			CBaseCombatWeapon *pWeapon = pMarine->GetWeapon(i);
			if (pWeapon == pMarine->GetActiveWeapon())
			{
				current = i;
				break;
			}
		}
		if (current == -1)
			return;
		
		pMarine->DropWeapon(current);

		IGameEvent * event = gameeventmanager->CreateEvent( "player_dropped_weapon" );
		if ( event )
		{
			event->SetInt( "userid", pPlayer->GetUserID() );

			gameeventmanager->FireEvent( event );
		}
	}
}
ConCommand ASW_Drop( "ASW_Drop", ASW_DropTest_f, "Makes your marine drop his current weapon", 0 );

void ASW_AllowBriefing_f()
{
	ASWGameRules()->AllowBriefing();
}
ConCommand ASW_AllowBriefing( "ASW_AllowBriefing", ASW_AllowBriefing_f, "Let's you restart the briefing", 0 );

void ASW_PhysicsShove_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	
	if (pPlayer && pPlayer->GetMarine())
	{
		if (pPlayer->GetMarine()->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
			return;
		pPlayer->GetMarine()->PhysicsShove();
	}
}
ConCommand ASW_PhysicsShove( "ASW_PhysicsShove", ASW_PhysicsShove_f, "Shove objects in front of you", FCVAR_CHEAT );

void ASW_PermaStim_f()
{
	ASWGameRules()->StartStim( 9000, NULL );
}
ConCommand ASW_PermaStim( "ASW_PermaStim", ASW_PermaStim_f, "Free long stim",FCVAR_CHEAT );

void ASW_PermaStim_Stop_f()
{
	ASWGameRules()->StopStim();
}
ConCommand ASW_PermaStimStop( "ASW_PermaStimStop", ASW_PermaStim_Stop_f, "Free long stim", FCVAR_CHEAT );

void asw_stop_burning_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	
	if (pPlayer && pPlayer->GetMarine())
	{
		CASW_Marine *pMarine = pPlayer->GetMarine();
		pMarine->Extinguish();
	}
}
ConCommand asw_stop_burning( "asw_stop_burning", asw_stop_burning_f, "Makes your marine stop burning", FCVAR_CHEAT );

void asw_flip_door_f()
{
	CBaseEntity* pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "asw_door" )) != NULL)
	{
		CASW_Door* pDoor = dynamic_cast<CASW_Door*>(pEntity);
		if (pDoor)
		{
			pDoor->FlipDoor();
		}
	}
}

ConCommand asw_flip_door( "asw_flip_door", asw_flip_door_f, "Flip all door meshes", FCVAR_CHEAT );

void asw_spawn_alien_f(const CCommand &args)
{
	if ( args.ArgC() < 2 )
	{
		Msg( "Please supply a spawner name\n" );
	}
	CBaseEntity* pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "asw_spawner" )) != NULL)
	{
		CASW_Spawner* pSpawner = dynamic_cast<CASW_Spawner*>(pEntity);
		if (pSpawner && !stricmp(STRING(pSpawner->GetEntityName()), args[1]))
		{
			pSpawner->SpawnOneAlien();
		}
	}
}

ConCommand asw_spawn_alien( "asw_spawn_alien", asw_spawn_alien_f, "Make the named asw_spawner spit out an alien", FCVAR_CHEAT );

void asw_test_turret_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	
	if (pPlayer && pPlayer->GetMarine())
	{
		CASW_Marine *pMarine = pPlayer->GetMarine();
		if (pMarine->m_hRemoteTurret.Get())
		{
			pMarine->m_hRemoteTurret->StopUsingTurret();//m_hUser = NULL;
			pMarine->m_hRemoteTurret = NULL;
			return;
		}
		CBaseEntity* pEntity = NULL;
		while ((pEntity = gEntList.FindEntityByClassname( pEntity, "asw_remote_turret" )) != NULL)
		{
			CASW_Remote_Turret* pTurret = dynamic_cast<CASW_Remote_Turret*>(pEntity);			
			if (pTurret)
			{
				pTurret->StartedUsingTurret(pMarine);
				pMarine->m_hRemoteTurret = pTurret;
				Msg("Set turret\n");
				return;
			}
		}
	}	
	Msg("Failed to find a turret\n");
}
ConCommand asw_test_turret( "asw_test_turret", asw_test_turret_f, "Test remote turret", FCVAR_CHEAT );
/*
void asw_test_marine_glow_f(const CCommand &args)
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if ( args.ArgC() < 10 )
	{
		Msg( "Usage: asw_test_marine_glow [scale] [brightness] [r] [g] [b] [a] [r] [g] [b]\n" );
	}
	
	if (pPlayer && pPlayer->GetMarine())
	{
		CASW_Marine *pMarine = pPlayer->GetMarine();
		static CSprite* pSprite = NULL;
		if (pSprite==NULL)
		{
			pSprite = CSprite::SpriteCreate( "swarm/sprites/light_glow03_no_z.vmt", pMarine->GetLocalOrigin(), false );
			pSprite->SetAttachment( pMarine, pMarine->LookupAttachment( "head" ) );
		}


		if (!pSprite)
			return;

		
		pSprite->SetTransparency( kRenderTransAdd, atof(args[3]), atof(args[4]), atof(args[5]), atof(args[5]), kRenderFxNoDissipation );  // 128 0 0 128 kRenderTransAdd kRenderWorldGlow
		pSprite->SetBrightness( atof(args[2]), 0.1f );	// 164
		pSprite->SetScale( atof(args[1]), 0.1f );		// 0.1
		pSprite->SetColor( atof(args[7]), atof(args[8]), atof(args[9]) );		// 128 0 0
		pSprite->SetAsTemporary();
	}	
	Msg("Failed to find a marine\n");
}
ConCommand asw_test_marine_glow( "asw_test_marine_glow", asw_test_marine_glow_f, "Usage: asw_test_marine_glow [scale] [brightness] [trans r] [trans g] [trans b] [trans a] [r] [g] [b]", FCVAR_CHEAT );
*/



void asw_drones_friendly_f()
{
	CBaseCombatCharacter::SetDefaultRelationship( (Class_T) CLASS_ASW_DRONE,			(Class_T) CLASS_ASW_MARINE,D_LI, 0);
}
ConCommand asw_drones_friendly( "asw_drones_friendly", asw_drones_friendly_f, "Makes drones friendly to marines", FCVAR_CHEAT );

void asw_ai_report_specific(const char* szClass)
{
	int iNormal = 0;
	int iEfficient = 0;
	int iVeryEfficient = 0;
	int iSuperEfficient = 0;
	int iDormant = 0;

	int iMoveNormal = 0;
	int iMoveEfficient = 0;

	int iAwake = 0;
	int iWaitingThreat = 0;
	int iWaitingPVS = 0;
	int iWaitingInput = 0;

	CBaseEntity* pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, szClass )) != NULL)
	{
		CAI_BaseNPC* pAI = dynamic_cast<CAI_BaseNPC*>(pEntity);			
		if (pAI)
		{
			if (pAI->GetEfficiency() == AIE_NORMAL)
				iNormal++;
			else if (pAI->GetEfficiency() == AIE_EFFICIENT)
				iEfficient++;
			else if (pAI->GetEfficiency() == AIE_VERY_EFFICIENT)
				iVeryEfficient++;
			else if (pAI->GetEfficiency() == AIE_SUPER_EFFICIENT)
				iSuperEfficient++;
			else if (pAI->GetEfficiency() == AIE_DORMANT)
				iDormant++;
			if (pAI->GetMoveEfficiency() == AIME_NORMAL)
				iMoveNormal++;
			else if (pAI->GetMoveEfficiency() == AIME_EFFICIENT)
				iMoveEfficient++;
			if (pAI->GetSleepState() == AISS_AWAKE)
				iAwake++;
			else if (pAI->GetSleepState() == AISS_WAITING_FOR_THREAT)
				iWaitingThreat++;
			else if (pAI->GetSleepState() == AISS_WAITING_FOR_PVS)
				iWaitingPVS++;
			else if (pAI->GetSleepState() == AISS_WAITING_FOR_INPUT)
				iWaitingInput++;
		}
	}
	Msg("Alien Class: %s\n", szClass);
	Msg("Efficiency: Normal=%d Efficient=%d VeryEfficient=%d SuperEfficient=%d Dormant=%d\n",
		iNormal, iEfficient, iVeryEfficient, iSuperEfficient, iDormant);
	Msg("Movement: Normal=%d Efficient=%d\n", iMoveNormal, iMoveEfficient);
	Msg("Sleep: Awake=%d WaitingThreat=%d WaitingPVS=%d WaitingInput=%d\n\n",
		iAwake, iWaitingThreat, iWaitingPVS, iWaitingInput);
}

void asw_ai_report_f()
{
	asw_ai_report_specific("asw_drone");
	asw_ai_report_specific("asw_drone_jumper");
	asw_ai_report_specific("asw_parasite");
	asw_ai_report_specific("asw_grub");	
}
ConCommand asw_ai_report( "asw_ai_report", asw_ai_report_f, "Lists various AI and their efficiencies", FCVAR_CHEAT );

void asw_drone_cycle_f()
{
	CBaseEntity* pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "asw_drone" )) != NULL)
	{
		CAI_BaseNPC* pAI = dynamic_cast<CAI_BaseNPC*>(pEntity);			
		if (pAI)
		{
			Msg("[%d:%s] Cycle=%f", pAI->entindex(),
							pAI->GetClassname(),
							pAI->GetCycle());
			int iLayers = pAI->GetNumAnimOverlays();
			for (int i=0;i<iLayers;i++)
			{
				Msg("  L%d: Cycle=%f Weight=%f\n", i, pAI->GetLayerCycle(i), pAI->GetLayerWeight(i));
			}
				
		}
	}
}
ConCommand asw_drone_cycle( "asw_drone_cycle", asw_drone_cycle_f, "Lists drones along with cycle and weight", FCVAR_CHEAT );

void asw_suicide_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
		
	if (pPlayer && pPlayer->GetMarine() && pPlayer->GetMarine()->GetHealth() > 0)
	{
		pPlayer->GetMarine()->Suicide();
	}
}
ConCommand asw_suicide( "asw_suicide", asw_suicide_f, "Kills your current marine", 0 );

void asw_hide_marine_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
		
	if (pPlayer && pPlayer->GetMarine())
	{
		if (pPlayer->GetMarine()->IsEffectActive(EF_NODRAW))
		{
			pPlayer->GetMarine()->RemoveEffects( EF_NODRAW );
			if (pPlayer->GetMarine()->GetActiveWeapon())
				pPlayer->GetMarine()->GetActiveWeapon()->RemoveEffects( EF_NODRAW );
		}
		else
		{
			pPlayer->GetMarine()->AddEffects( EF_NODRAW );
			if (pPlayer->GetMarine()->GetActiveWeapon())
				pPlayer->GetMarine()->GetActiveWeapon()->AddEffects( EF_NODRAW );
		}
	}
}
ConCommand asw_hide_marine( "asw_hide_marine", asw_hide_marine_f, "Toggle drawing of the current marine", FCVAR_CHEAT );

void asw_ragdoll_marine_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	static CRagdollProp * s_pRagdoll = NULL;

	if (pPlayer && pPlayer->GetMarine())
	{
		CASW_Marine* pMarine = pPlayer->GetMarine();
		pMarine->SetKnockedOut(!pMarine->m_bKnockedOut);

		return;
		
		if (pMarine->IsEffectActive(EF_NODRAW) && s_pRagdoll)
		{
			//Calcs the diff between ragdoll worldspace center and victim worldspace center, moves the victim by this diff.
			//Sets the victim's angles to 0, ragdoll yaw, 0
			QAngle newAngles( 0, s_pRagdoll->GetAbsAngles()[YAW], 0 );

			Vector centerDelta = s_pRagdoll->WorldSpaceCenter() - pMarine->WorldSpaceCenter();
			centerDelta.z = 0;	// don't put us in the floor
			Vector newOrigin = pMarine->GetAbsOrigin() + centerDelta;
			pMarine->SetAbsOrigin( newOrigin );
			pMarine->SetAbsAngles( newAngles );
			//DetachAttachedRagdoll( s_pRagdoll ); 	// unnecessary since we remove it next?
			UTIL_Remove( s_pRagdoll );
			pMarine->RemoveEffects( EF_NODRAW );
			pMarine->RemoveSolidFlags( FSOLID_NOT_SOLID );
		}
		else
		{
			pMarine->InvalidateBoneCache();
			pMarine->AddSolidFlags( FSOLID_NOT_SOLID );			
			CTakeDamageInfo	info;
			info.SetDamageType( DMG_GENERIC );
			info.SetDamageForce( vec3_origin );
			info.SetDamagePosition( pMarine->WorldSpaceCenter() );
			s_pRagdoll = (CRagdollProp*) CreateServerRagdoll( pMarine, 0, info, COLLISION_GROUP_NONE );
			if ( s_pRagdoll )
			{
				s_pRagdoll->DisableAutoFade();
				s_pRagdoll->SetThink( NULL );
				s_pRagdoll->SetUnragdoll( pMarine );
			}			
			pMarine->AddEffects( EF_NODRAW );
			//pMarine->SetupBones( m_pRagdollBones, BONE_USED_BY_ANYTHING );
		}
	}
}
//ConCommand asw_ragdoll_marine( "asw_ragdoll_marine", asw_ragdoll_marine_f, "Toggle ragdolling of the current marine", FCVAR_CHEAT );

void asw_ragdoll_blend_test_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
		
	if (pPlayer)
	{
		CBaseEntity* pEntity = NULL;
		while ((pEntity = gEntList.FindEntityByClassname( pEntity, "prop_ragdoll" )) != NULL)
		{
			CRagdollProp *pRagdoll = dynamic_cast<CRagdollProp*>(pEntity);
			if (pRagdoll)
			{
				pPlayer->m_pBlendRagdoll = pRagdoll;
				pPlayer->m_fBlendAmount = 0;
				pRagdoll->SetOverlaySequence(ACT_IDLE);
				return;
			}
		}				
	}
}
ConCommand asw_ragdoll_blend_test( "asw_ragdoll_blend_test", asw_ragdoll_blend_test_f, "Ragdoll blending test", FCVAR_CHEAT );


void asw_marine_server_anim_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if (pPlayer)
	{
		CASW_Marine *pMarine = pPlayer->GetMarine();
		if (pMarine)
		{
			Msg("[S] Marine playing sequence %d (%s) A:%s\n", pMarine->GetSequence(),
				pMarine->GetSequenceName(pMarine->GetSequence()),
				pMarine->GetSequenceActivityName(pMarine->GetSequence()));
			int iLayers = pMarine->GetNumAnimOverlays();
			Msg("Layers: %d\n", iLayers);
			for (int i=0;i<iLayers;i++)
			{
				CAnimationLayer* pLayer = pMarine->GetAnimOverlay(i);
				if (!pLayer)
				{
					Msg("Layer %d is null\n", i);
				}
				else
				{
					int iSeq = pLayer->m_nSequence;
					Msg("Layer %d sequence %d (%s) A:%s W:%f C:%f\n", i, iSeq, pMarine->GetSequenceName(iSeq),
						pMarine->GetSequenceActivityName(iSeq), pLayer->m_flWeight, pLayer->m_flCycle);
				}
			}
		}
		else
		{
			Msg("No Marine to list anims on\n");
		}
	}
	else
	{
		Msg("No command player!\n");
	}
}
ConCommand asw_marine_server_anim( "asw_marine_server_anim", asw_marine_server_anim_f, "Lists animation playing on the player's current marine serverside", FCVAR_CHEAT );

void listmarineresources_server_f(void)
{
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if ( !pGameResource )
		return;

	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		if (pGameResource->GetMarineResource(i) == NULL)
			Msg("MarineResource %d = empty\n", i);
		else
		{
			Msg("MarineResource %d = present, profileindex %d, commander %d commander index %d\n",
				i, pGameResource->GetMarineResource(i)->m_MarineProfileIndex,
				pGameResource->GetMarineResource(i)->GetCommander(),
				pGameResource->GetMarineResource(i)->m_iCommanderIndex.Get());
		}
	}
}

static ConCommand listmarineresources_server("listmarineresources_server", listmarineresources_server_f, "Shows contents of the marine resources array", FCVAR_CHEAT);

void listroster_server_f(void)
{
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{
		Msg("[S]Roster %d selected=%d\n", i, pGameResource->IsRosterSelected(i));
	}
}

static ConCommand listroster_server("listroster_server", listroster_server_f, "Shows which marines in the roster are selected", FCVAR_CHEAT);


void asw_check_campaign_f(void)
{
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;
	
	Msg("IsInCampaignMap = %d\n", pGameResource->IsCampaignGame());
}

static ConCommand asw_check_campaign("asw_check_campaign", asw_check_campaign_f, "Is this map in campaign mode?", FCVAR_CHEAT);

void asw_mission_complete_f()
{
	if (!ASWGameRules())
		return;
	
	ASWGameRules()->CheatCompleteMission();
}
static ConCommand asw_mission_complete("asw_mission_complete", asw_mission_complete_f, "Cheat to complete the current mission", FCVAR_CHEAT);


void asw_marine_spectate_f(const CCommand &args)
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if ( args.ArgC() < 2 )
	{
		Msg( "Usage: asw_marine_spectate [marine_num]\n" );
		return;
	}

	CASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	int iMarine = atof(args[1]);
	if (iMarine < 0 || iMarine >= pGameResource->GetMaxMarineResources())
		return;

	CASW_Marine_Resource* pMR = pGameResource->GetMarineResource(iMarine);
	if (!pMR)
	{
		Msg("No marine resource in that index\n");
		return;
	}

	CASW_Marine *pMarine = pMR->GetMarineEntity();
	if (!pMarine)
	{
		Msg("No live marine in that slot\n");
		return;
	}
		
	pPlayer->SetSpectatingMarine(pMarine);
}
static ConCommand asw_marine_spectate("asw_marine_spectate", asw_marine_spectate_f, "Usage: asw_marine_spectate [marine_num]", FCVAR_CHEAT);

void asw_live_marines_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if (!pPlayer)
		return;

	Msg("Has live marines: %d\n", pPlayer->HasLiveMarines());
}
static ConCommand asw_live_marines("asw_live_marines", asw_live_marines_f, "Reports if you have live marines", FCVAR_CHEAT);

void asw_marine_skill_f(const CCommand &args)
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());	

	if (!ASWGameRules())
		return;
	if (!pPlayer)
		return;
	CASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	CASW_Marine_Profile *pProfile = NULL;
	CASW_Marine *pMarine = pPlayer->GetMarine();
	if (pMarine)
	{
		pProfile = pMarine->GetMarineProfile();
	}
	else
	{
		// find the first marine info that belongs to us
		for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
		{
			CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
			if (pMR && pMR->GetCommander() == pPlayer)
			{
				pProfile = pMR->GetProfile();
				break;
			}
		}
	}
	
	if ( !pProfile )
		return;

	if ( args.ArgC() < 2 )
	{
		Msg("Usage: asw_marine_skill [SkillSlot]  - reports the number of skill points of the current marine in that skill\n  asw_marine_skill [SkillSlot] [x]  - sets that skill to the specified number of skill points (0-5)\n");
		Msg("SkillSlot goes from 0 to 4 for your skills, slot 5 is spare skill points.\n");
		return;
	}

	int nSkillSlot = atoi(args[1]);
	if ( nSkillSlot < 0 || nSkillSlot >= ASW_NUM_SKILL_SLOTS )
	{
		Msg("nSkillSlot out of bounds\n");
		return;
	}

	if ( args.ArgC() < 3 )
	{
		int iSkillPoints = ASWGameResource()->GetMarineSkill( pProfile->m_ProfileIndex, nSkillSlot );
		Msg( "Marine skill[%d] is %s = %d\n", nSkillSlot, SkillToString( pProfile->GetSkillMapping( nSkillSlot ) ), iSkillPoints );
	}
	else
	{
		int iNewPoints = atoi(args[2]);
		ASWGameResource()->SetMarineSkill( pProfile->m_ProfileIndex, nSkillSlot, iNewPoints );	
		int iSkillPoints = ASWGameResource()->GetMarineSkill( pProfile->m_ProfileIndex, nSkillSlot );
		Msg( "Marine skill[%d] is now %s = %d\n", nSkillSlot, SkillToString( pProfile->GetSkillMapping( nSkillSlot ) ), iSkillPoints );
	}
}
static ConCommand asw_marine_skill("asw_marine_skill", asw_marine_skill_f, "Usage: asw_marine_skill [nSkillSlot]  - reports the number of skill points of the current marine in that skill\n  asw_marine_speed [nSkillSlot] [x]  - sets that skill to the specified number of skill points (0-5)", FCVAR_CHEAT);

extern ConVar asw_marine_nearby_angle;
void asw_test_marinenearby_f(const CCommand &args)
{
	Msg("0.5 as float = %f\n", 0.5f);
	Msg("0.5f as int = %d\n", (int) 0.5f);
	Msg("0.51f as int = %d\n", (int) 0.51f);
	Msg("0.52f as int = %d\n", (int) 0.52f);
	Msg("0.6f as int = %d\n", (int) 0.6f);
	Msg("0.56f as int = %d\n", (int) 0.56f);
	Msg("0.49f as int = %d\n", (int) 0.49f);
	Msg("1.99f as int = %d\n", (int) 1.99f);
	Msg("2.01f as int = %d\n", (int) 2.01f);
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());	

	if (!ASWGameRules())
		return;
	if (!pPlayer || !pPlayer->GetMarine())
		return;
	
	if ( args.ArgC() < 4 )
	{
		Msg("Usage: asw_test_marinenearby [0|1] [grid step] [grid count]\n");
	}
	int iWide = atoi(args[1]);
	float fGridStep = atof(args[2]);
	int iGridCount = atoi(args[3]);
	//Msg("Wide = %d, step = %f, count = %d\n", iWide, fGridStep, iGridCount);

	Vector asw_default_camera_dir_2;
	QAngle test_angle(asw_marine_nearby_angle.GetFloat(), 90, 0);
	AngleVectors(test_angle, &asw_default_camera_dir_2);
	Vector asw_default_camera_offset_2 = asw_default_camera_dir_2 * -405.0f;

	for (int x=-iGridCount;x<iGridCount;x++)
	{
		for (int y=-iGridCount;y<iGridCount*3;y++)
		{
			Vector pos = pPlayer->GetMarine()->GetAbsOrigin() + Vector(x * fGridStep, y*fGridStep, 10);
			//Msg("Testing pos %f, %f, %f\n", pos.x, pos.y, pos.z);
			bool bCorpseCanSee = false;
			UTIL_ASW_AnyMarineCanSee(pos,
				iWide, bCorpseCanSee);
		}
	}	
	Vector pos = (pPlayer->GetMarine()->GetAbsOrigin() + asw_default_camera_offset_2);
	//NDebugOverlay::Line(pos, pos + asw_default_camera_dir_2 * 410, 0,0,255,true, 30);
}
static ConCommand asw_test_marinenearby("asw_test_marinenearby", asw_test_marinenearby_f, "Tests the marine nearby util shared function", FCVAR_CHEAT);


void asw_set_drone_skin_f(const CCommand &args)
{
	if ( args.ArgC() < 2 )
	{
		Msg("Usage: asw_set_drone_skin [skin index]\n");
	}

	CBaseEntity* pEntity = NULL;
	int iSkin = atoi(args[1]);
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "asw_drone_advanced" )) != NULL)
	{
		CBaseAnimating *pAnim = dynamic_cast<CBaseAnimating*>(pEntity);
		if (pAnim)
			pAnim->m_nSkin = iSkin;
	}
	pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "asw_simple_drone" )) != NULL)
	{
		CBaseAnimating *pAnim = dynamic_cast<CBaseAnimating*>(pEntity);
		if (pAnim)
			pAnim->m_nSkin = iSkin;
	}
}
static ConCommand asw_set_drone_skin("asw_set_drone_skin", asw_set_drone_skin_f, "Sets skin index of all drones and simple drones", FCVAR_CHEAT);

void asw_report_difficulty_f()
{
	Msg("Skill level = %d\n", ASWGameRules()->GetSkillLevel());
	Msg("Mission Difficulty level = %d\n", ASWGameRules()->GetMissionDifficulty());

	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	CPASAttenuationFilter filter( pPlayer );						
	CSoundParameters params;
	if ( CBaseEntity::GetParametersForSound( "crash.selection", params, NULL ) )
	{
	
		EmitSound_t ep( params );
			
		ep.m_flVolume = 1.0f;
		ep.m_nChannel = CHAN_AUTO;
		ep.m_pOrigin = &pPlayer->GetAbsOrigin();
		ep.m_nPitch = params.pitch * random->RandomFloat(0.5, 3.0);

		CBaseEntity::EmitSound( filter, 0, ep );
	}
}
static ConCommand asw_report_difficulty("asw_report_difficulty", asw_report_difficulty_f, "Reports current skill and mission difficulty level", FCVAR_CHEAT);

void asw_conv_f(const CCommand &args)
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());	

	if (!ASWGameRules())
		return;
	if (!pPlayer || !pPlayer->GetMarine())
		return;
	CASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	CASW_Marine *pMarine = pPlayer->GetMarine();
	if (!pPlayer->GetMarine()->GetMarineProfile())
		return;

	if ( args.ArgC() < 2 )
	{
		Msg("Usage: asw_conv [conv num]	");
		return;
	}

	CASW_MarineSpeech::StartConversation(atoi(args[1]), pMarine);		
}
static ConCommand asw_conv("asw_conv", asw_conv_f, "Test a conversation", FCVAR_CHEAT);

void asw_medal_info_f(const CCommand &args)
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());	

	if (!ASWGameRules())
		return;
	if (!pPlayer || !pPlayer->GetMarine())
		return;
	CASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	if ( args.ArgC() < 2 )
	{
		Msg("Usage: asw_medal_info [marine info num from 0-3]");
		return;
	}

	int i = atoi(args[1]);
	if (pGameResource->GetMarineResource(i))
		pGameResource->GetMarineResource(i)->DebugMedalStats();
}
static ConCommand asw_medal_info("asw_medal_info", asw_medal_info_f, "Give medal info on a particular marine", FCVAR_CHEAT);


void asw_build_speech_durations_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());	
	
	if (!pPlayer || !MarineProfileList() || !pPlayer->GetMarine())
		return;
	Msg("Saving speech durations...");
	MarineProfileList()->SaveSpeechDurations(pPlayer->GetMarine());
	Msg("Done!\n");
}
static ConCommand asw_build_speech_durations("asw_build_speech_durations", asw_build_speech_durations_f, "Measures speech durations and saves to file for use in async client speech", FCVAR_CHEAT);

void asw_network_id_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());	
	
	if (!pPlayer)
		return;

	//Msg("Your network ID is %s userID is %s\n", pPlayer->GetNetworkIDString(), pPlayer->GetUserID());
	// integer tests
	Msg("1/10 = %d\n", int(1)/int(10));
	Msg("4/10 = %d\n", int(4)/int(10));
	Msg("5/10 = %d\n", int(5)/int(10));
	Msg("6/10 = %d\n", int(6)/int(10));
	Msg("9/10 = %d\n", int(9)/int(10));
	Msg("10/10 = %d\n", int(10)/int(10));
	Msg("11/10 = %d\n", int(11)/int(10));
	Msg("14/10 = %d\n", int(14)/int(10));
	Msg("15/10 = %d\n", int(15)/int(10));
	Msg("16/10 = %d\n", int(16)/int(10));
	Msg("19/10 = %d\n", int(19)/int(10));
	Msg("20/10 = %d\n", int(20)/int(10));
}
static ConCommand asw_network_id("asw_network_id", asw_network_id_f, "returns network id", FCVAR_CHEAT);


void asw_corpse_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());	

	if (!ASWGameRules())
		return;
	if (!pPlayer || !pPlayer->GetMarine())
		return;
	CASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	CASW_Marine *pMarine = pPlayer->GetMarine();
	if (!pPlayer->GetMarine()->GetMarineProfile())
		return;

	QAngle facing = pMarine->GetAbsAngles();
	Vector forward;
	AngleVectors(facing, &forward);
	Vector pos = pMarine->GetAbsOrigin() + forward * 100.0f;
	//CBaseEntity *pGib = 
	CreateRagGib( "models/swarm/colonist/male/malecolonist.mdl", pos, facing, Vector(0,0,0) );
}
static ConCommand asw_corpse("asw_corpse", asw_corpse_f, "Test create a clientside corpse", FCVAR_CHEAT);

void asw_gimme_ammo_f(void)
{
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if ( !pGameResource )
		return;

	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		if (pGameResource->GetMarineResource(i) != NULL && pGameResource->GetMarineResource(i)->GetMarineEntity())
		{
			CASW_Marine *pMarine = pGameResource->GetMarineResource(i)->GetMarineEntity();
			for (int k=0;k<ASW_MAX_MARINE_WEAPONS;k++)
			{
				CASW_Weapon *pWeapon = pMarine->GetASWWeapon(k);
				if (!pWeapon)
					continue;

				// refill bullets in the gun
				pWeapon->m_iClip1 = pWeapon->GetMaxClip1();
				pWeapon->m_iClip2 = pWeapon->GetMaxClip2();

				// give the marine a load of ammo of that type
				pMarine->GiveAmmo(10000, pWeapon->GetPrimaryAmmoType());
				pMarine->GiveAmmo(10000, pWeapon->GetSecondaryAmmoType());
			}
		}
	}
}

static ConCommand asw_gimme_ammo("asw_gimme_ammo", asw_gimme_ammo_f, "Refills all marine ammo", FCVAR_CHEAT);

void asw_drop_ammo_f(const CCommand &args)
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if (!pPlayer)
		return;
	
	CASW_Marine *pMarine = pPlayer->GetMarine();
	if (!pMarine)
		return;

	int iBagSlot = atoi(args[1]);

	CASW_Weapon_Ammo_Bag *pBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pMarine->GetWeapon(0));
	if (pBag)
	{
		if (pBag->DropAmmoPickup(iBagSlot))
		{
			return;
		}
	}

	pBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pMarine->GetWeapon(1));
	if (pBag)
	{
		if (pBag->DropAmmoPickup(iBagSlot))
		{
			return;
		}
	}
}

static ConCommand asw_drop_ammo("asw_drop_ammo", asw_drop_ammo_f, "Drops ammo from an ammo bag", 0);

void asw_conversation_f(const CCommand &args)
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if (!pPlayer)
		return;
	
	CASW_Marine *pMarine = pPlayer->GetMarine();
	if (!pMarine)
		return;

	if (args.ArgC() < 2)
	{
		Msg("Usage: asw_conversation [conversation number]\n");
	}
	int iConv = atoi(args[1]);
	CASW_MarineSpeech::StartConversation(iConv, pMarine);
}

static ConCommand asw_conversation("asw_conversation", asw_conversation_f, "Triggers a conversation", FCVAR_CHEAT);

/*
void asw_show_game_stats_f(void)
{
	if (ASWGameStats())
		ASWGameStats()->DebugContents();
}

static ConCommand asw_show_game_stats("asw_show_game_stats", asw_show_game_stats_f, "Outputs contents of the asi_gamestats.dat file for debugging", FCVAR_CHEAT);
*/

void asw_debug_spectator_server_f(void)
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if (!pPlayer)
		return;
	
	Msg("Server: m_bRequestedSpectator=%d\n", pPlayer->m_bRequestedSpectator);
}

static ConCommand asw_debug_spectator_server("asw_debug_spectator_server", asw_debug_spectator_server_f, "Prints whether a player wants to spectate", FCVAR_CHEAT);

void CC_asw_teleport( const CCommand &args )
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if ( !pPlayer )
		return;

	Vector vTargetPos = pPlayer->GetAbsOrigin();

	// fires a command from the console
	if ( args.ArgC() < 2 )
	{
		trace_t tr;

		Vector vPlayerForward;
		pPlayer->EyeVectors( &vPlayerForward, NULL, NULL );

		UTIL_TraceLine( pPlayer->GetAbsOrigin(), pPlayer->GetAbsOrigin() + vPlayerForward * 10000.0f, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
		
		if ( tr.DidHit() )
		{
			vTargetPos = tr.endpos;
		}
	}
	else
	{
		// find the named entity
		CBaseEntity *target = gEntList.FindEntityByName( NULL, args[1] );
		if ( !target )
		{
			int i = atoi( args[1] );
			if ( i != 0 )
			{
				target = CBaseEntity::Instance( i );
				if ( !target )
				{
					Msg( "Couldn't find entity!\n" );
					return;
				}
			}
			else
			{
				Msg( "Couldn't find entity!\n" );
				return;
			}
		}

		vTargetPos = target->GetAbsOrigin();
	}

	CASW_Marine *pMarine = pPlayer->GetMarine();

	if ( !pMarine )
	{
		for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); ++i )
		{
			CASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
			if ( !pMR )
				continue;

			if ( pMR->GetMarineEntity() && pMR->GetMarineEntity()->GetCommander() == pPlayer )
			{
				pMarine = pMR->GetMarineEntity();
				break;
			}
		}
	}

	if ( pMarine )
	{
		// Teleport the dude under our control
		Vector vecPos = vTargetPos;//pNearest->GetOrigin();
		pMarine->Teleport( &vecPos, NULL, NULL );
	}
}

static int CC_asw_teleport_autocomplete( char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	if ( !g_pGameRules )
	{
		return 0;
	}

	char const *cmdname = "asw_teleport";

	char *substring = (char *)partial;
	if ( Q_strstr( partial, cmdname ) )
	{
		substring = (char *)partial + strlen( cmdname ) + 1;
	}

	int checklen = Q_strlen( substring );
	CUtlSymbolTable entries( 0, 0, true );
	CUtlVector< CUtlSymbol > symbols;

	CBaseEntity *pos = NULL;
	while ( ( pos = gEntList.NextEnt( pos ) ) != NULL )
	{
		// Check target name against partial string
		if ( pos->GetEntityName() == NULL_STRING )
			continue;

		if ( Q_strnicmp( STRING( pos->GetEntityName() ), substring, checklen ) )
			continue;

		CUtlSymbol sym = entries.AddString( STRING( pos->GetEntityName() ) );

		int idx = symbols.Find( sym );
		if ( idx == symbols.InvalidIndex() )
		{
			symbols.AddToTail( sym );
		}

		// Too many
		if ( symbols.Count() >= COMMAND_COMPLETION_MAXITEMS )
			break;
	}

	// Now fill in the results
	for ( int i = 0; i < symbols.Count(); i++ )
	{
		char const *name = entries.String( symbols[ i ] );

		char buf[ 512 ];
		Q_strncpy( buf, name, sizeof( buf ) );
		Q_strlower( buf );

		Q_snprintf( commands[ i ], COMMAND_COMPLETION_ITEM_LENGTH, "%s %s",
			cmdname, buf );
	}

	return symbols.Count();
}

static ConCommand asw_teleport("asw_teleport", CC_asw_teleport, "Usage:\n   asw_teleport <target entity>\nTeleports your current marine to the named entity", FCVAR_CHEAT, CC_asw_teleport_autocomplete );

void asw_solid_info_f(void)
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());

	if (pPlayer == NULL)
	{
		Msg("Couldn't find local player.");
		return;
	}
	CASW_Marine *pMarine = pPlayer->GetMarine();
	if (!pMarine)
		return;

	int solid0 = pMarine->GetSolid();
	int nSolidFlags0 = pMarine->GetSolidFlags();
	int movetype0 = pMarine->GetMoveType();

	Msg("Marine(%d} solid=%d solidflags=%d movetype=%d\n", pMarine->entindex(), solid0, nSolidFlags0, movetype0);
}

static ConCommand asw_solid_info("asw_solid_info", asw_solid_info_f, "Shows solid status of current marine", FCVAR_CHEAT);

void asw_set_solid_f( const CCommand &args )
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());

	if (pPlayer == NULL)
	{
		Msg("Couldn't find local player.");
		return;
	}
	if (args.ArgC() != 2)
		return;

	CASW_Marine *pMarine = pPlayer->GetMarine();
	if (!pMarine)
		return;

	pMarine->SetSolid((SolidType_t) atoi(args[1]));
}

static ConCommand asw_set_solid("asw_set_solid", asw_set_solid_f, "Sets solid status of current marine", FCVAR_CHEAT);

//------------------------------------------------------------------------------
// Purpose: Create an NPC of the given type
//------------------------------------------------------------------------------
void CC_ASW_Ent_Create( const CCommand& args )
{
	MDLCACHE_CRITICAL_SECTION();

	bool allowPrecache = CBaseEntity::IsPrecacheAllowed();
	CBaseEntity::SetAllowPrecache( true );

	// Try to create entity
	CBaseEntity *entity = dynamic_cast< CBaseEntity * >( CreateEntityByName(args[1]) );
	if (entity)
	{
		entity->Precache();		

		// Now attempt to drop into the world
		CASW_Player* pPlayer = ToASW_Player( UTIL_GetCommandClient() );
		if (!pPlayer)
			return;

		trace_t tr;
		UTIL_TraceLine( pPlayer->GetCrosshairTracePos() + Vector( 0, 0, 30 ),
			pPlayer->GetCrosshairTracePos(), MASK_SOLID, 
			pPlayer, COLLISION_GROUP_NONE, &tr );

		if ( tr.fraction != 0.0 )
		{
			// Raise the end position a little up off the floor, place the npc and drop him down
			tr.endpos.z += 12;
			entity->Teleport( &tr.endpos, NULL, NULL );
			// this was causing aliens to spawn under ground 
			//UTIL_DropToFloor( entity, MASK_SOLID );
		}
		DispatchSpawn(entity);
	}
	CBaseEntity::SetAllowPrecache( allowPrecache );
}
static ConCommand asw_ent_create("asw_ent_create", CC_ASW_Ent_Create, "Creates an entity of the given type in front of the current marine.", FCVAR_GAMEDLL | FCVAR_CHEAT);


// This is used by the PlayerListPanel when the player clicks the restart mission button
void asw_restart_mission_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if (!pPlayer || !ASWGameResource())
		return;

	if (ASWGameResource()->GetLeader() != pPlayer)
		return;
	if (ASWGameRules())
	{
		if ( gpGlobals->maxClients > 1)
		{
			ASWGameRules()->RestartMissionCountdown( pPlayer );
		}
		else
		{
			// restart instantly in singleplayer
			ASWGameRules()->RestartMission( pPlayer );
		}
	}
}
ConCommand asw_restart_mission( "asw_restart_mission", asw_restart_mission_f, "Restarts the current mission", 0 );

void asw_NextMarinef()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	pPlayer->SelectNextMarine(false);
}
ConCommand ASW_NextMarine( "ASW_NextMarine", asw_NextMarinef, "Select your next marine", 0);

void asw_PrevMarinef()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	pPlayer->SelectNextMarine(true);
}
ConCommand ASW_PrevMarine( "ASW_PrevMarine", asw_PrevMarinef, "Select your previous marine", 0);


void asw_room_info_f()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if ( pPlayer && pPlayer->GetMarine() && missionchooser && missionchooser->RandomMissions() )
	{
		IASW_Room_Details *pRoom = missionchooser->RandomMissions()->GetRoomDetails( pPlayer->GetMarine()->GetAbsOrigin() );
		if ( !pRoom )
		{
			Msg(" Couldn't find room\n" );
		}
		else
		{
			char buf[MAX_PATH];
			pRoom->GetFullRoomName( buf, MAX_PATH );
			Msg( " Room name: %s\n", buf );
			Msg( " Room tags: " );
			int nNumTags = pRoom->GetNumTags();
			for ( int i = 0; i < nNumTags; ++ i )
			{
				Msg( "%s    ", pRoom->GetTag( i ) );
			}
			Msg( "\n" );
		}
	}
}
ConCommand asw_room_info( "asw_room_info", asw_room_info_f, "Shows details about the current room in a randomly generated map", 0);


void asw_debug_server_cursor_f( const CCommand& args )
{
	MDLCACHE_CRITICAL_SECTION();

	// Now attempt to drop into the world
	CASW_Player* pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if (!pPlayer)
		return;

	NDebugOverlay::Cross3D( pPlayer->GetCrosshairTracePos(), 20.0f, 255, 128, 0, true, 1.0f );
}
static ConCommand asw_debug_server_cursor("asw_debug_server_cursor", asw_debug_server_cursor_f, "Server shows a marker where it thinks your cursor is", FCVAR_CHEAT );

//------------------------------------------------------------------------------
// Purpose: Teleport a specified entity to where the player is looking
//------------------------------------------------------------------------------
bool CC_ASW_GetCommandEnt( const CCommand& args, CBaseEntity **ent, Vector *vecTargetPoint, QAngle *vecPlayerAngle )
{
	// Find the entity
	*ent = NULL;
	// First try using it as an entindex
	int iEntIndex = atoi( args[1] );
	if ( iEntIndex )
	{
		*ent = CBaseEntity::Instance( iEntIndex );
	}
	else
	{
		// Try finding it by name
		*ent = gEntList.FindEntityByName( NULL, args[1] );

		if ( !*ent )
		{
			// Finally, try finding it by classname
			*ent = gEntList.FindEntityByClassname( NULL, args[1] );
		}
	}

	if ( !*ent )
	{
		Msg( "Couldn't find any entity named '%s'\n", args[1] );
		return false;
	}

	CASW_Player *pPlayer = ToASW_Player( UTIL_GetCommandClient() );
	if ( vecTargetPoint )
	{
		trace_t tr;
		UTIL_TraceLine( pPlayer->GetCrosshairTracePos() + Vector( 0, 0, 30 ),
			pPlayer->GetCrosshairTracePos() - Vector( 0, 0, 10 ), MASK_SOLID, 
			pPlayer, COLLISION_GROUP_NONE, &tr );

		if ( tr.fraction != 1.0 )
		{
			*vecTargetPoint = tr.endpos;
		}
	}

	if ( vecPlayerAngle )
	{
		*vecPlayerAngle = pPlayer->EyeAngles();
	}

	return true;
}
//------------------------------------------------------------------------------
// Purpose: Teleport a specified entity to where the player is looking
//------------------------------------------------------------------------------
void CC_ASW_Ent_Teleport( const CCommand& args )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "Format: ent_teleport <entity name>\n" );
		return;
	}

	CBaseEntity *pEnt;
	Vector vecTargetPoint;
	if ( CC_ASW_GetCommandEnt( args, &pEnt, &vecTargetPoint, NULL ) )
	{
		pEnt->Teleport( &vecTargetPoint, NULL, NULL );
	}
}

static ConCommand asw_ent_teleport("asw_ent_teleport", CC_ASW_Ent_Teleport, "Teleport the specified entity to the crosshair location.\n\tFormat: ent_teleport <entity name>", FCVAR_CHEAT);


void cc_asw_inventory()
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if ( pPlayer->GetMarine() )
	{
		for (int i=0;i<pPlayer->GetMarine()->WeaponCount();i++)
		{
			CBaseEntity *pWeapon = pPlayer->GetMarine()->GetWeapon(i);
			if ( pWeapon )
			{
				Msg(" Inventory[%d] = %s (%d)\n", i, pWeapon->GetClassname(), pWeapon->entindex() );
			}
		}
	}
}

ConCommand asw_inventory( "asw_inventory", cc_asw_inventory, "Lists marine's inventory", FCVAR_CHEAT );

void cc_stuck( const CCommand &args )
{
	CASW_Player *pPlayer = ToASW_Player(UTIL_GetCommandClient());
	if (!pPlayer)
		return;

	CASW_Marine *pMarine = pPlayer->GetMarine();
	if (!pMarine)
		return;

	if ( !pMarine->TeleportStuckMarine() )
	{
		Msg("Error, couldn't find a valid free info_node to teleport to!\n");
	}		
}
ConCommand stuck( "stuck", cc_stuck, "Teleports marine to the nearest free node", FCVAR_CHEAT );

void asw_gimme_health_f(void)
{
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if ( !pGameResource )
		return;

	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		if (pGameResource->GetMarineResource(i) != NULL && pGameResource->GetMarineResource(i)->GetMarineEntity())
		{
			CASW_Marine *pMarine = pGameResource->GetMarineResource(i)->GetMarineEntity();
			pMarine->AddSlowHeal( pMarine->GetMaxHealth() - pMarine->GetHealth(), 3, NULL );
		}
	}
}

static ConCommand asw_gimme_health("asw_gimme_health", asw_gimme_health_f, "Refills all marine health", FCVAR_CHEAT);


void SpawnBuzzerAboveMe( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "Format: asw_spawn_buzzer <z height above marine>\n" );
		return;
	}

	MDLCACHE_CRITICAL_SECTION();

	bool allowPrecache = CBaseEntity::IsPrecacheAllowed();
	CBaseEntity::SetAllowPrecache( true );

	// Try to create entity
	CBaseEntity *entity = dynamic_cast< CBaseEntity * >( CreateEntityByName( "asw_buzzer" ) );
	if (entity)
	{
		entity->Precache();		

		// Now attempt to drop into the world
		CASW_Player* pPlayer = ToASW_Player( UTIL_GetCommandClient() );
		if (!pPlayer)
			return;

		CASW_Marine *pMarine = pPlayer->GetMarine();
		if ( !pMarine )
			return;

		Vector vecPos = pMarine->GetAbsOrigin();
		vecPos.z += atof( args[1] );
		entity->Teleport( &vecPos, NULL, NULL );

		DispatchSpawn(entity);
	}
	CBaseEntity::SetAllowPrecache( allowPrecache );
}
static ConCommand asw_spawn_buzzer("asw_spawn_buzzer", SpawnBuzzerAboveMe, "Refills all marine health", FCVAR_CHEAT);
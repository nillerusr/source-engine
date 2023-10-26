#include "cbase.h"
#include "asw_campaign_save.h"
#include "asw_campaign_info.h"
#include "asw_gamerules.h"
#include "asw_game_resource.h"
#include "asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "asw_marine.h"
#include "filesystem.h"
#include "asw_medals.h"
#include "asw_medals_shared.h"
#include "asw_player.h"
#include "asw_equipment_list.h"
#include "world.h"
#include "asw_achievements.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_medal_explosive_kills("asw_medal_explosive_kills", "6", FCVAR_CHEAT, "How many aliens have to be killed from one explosion to get the explosives merit medal");
ConVar asw_medal_reckless_explosive_kills("asw_medal_reckless_explosive_kills", "1", FCVAR_CHEAT, "How many aliens have to be killed from kicked grenades to get the reckless explosive merit");
ConVar asw_medal_lifesaver_dist("asw_medal_lifesaver_dist", "100", FCVAR_CHEAT, "How close an alien has to be a fellow marine to count for the lifesaver medal");
ConVar asw_medal_lifesaver_kills("asw_medal_lifesaver_kills", "1", FCVAR_CHEAT, "How many aliens you have to kill that were close to a teammate to get the lifesaver medal");
ConVar asw_medal_blood_heal_amount("asw_medal_blood_heal_amount", "150", FCVAR_CHEAT, "How much health must be healed to win the blood halo medal");
ConVar asw_medal_silver_heal_amount("asw_medal_silver_heal_amount", "200", FCVAR_CHEAT, "How much health must be healed to win the silver halo medal");
ConVar asw_medal_mine_burns("asw_medal_mine_burns", "60", FCVAR_CHEAT, "How many mine kills needed to get the mine medal");
ConVar asw_medal_accuracy("asw_medal_accuracy", "0.9", FCVAR_CHEAT, "Minimum accuracy needed to win the accuracy medal");
ConVar asw_medal_sentry_kills("asw_medal_sentry_kills", "60", FCVAR_CHEAT, "Minimum kills needed to win the sentry medal");
ConVar asw_medal_grub_kills("asw_medal_grub_kills", "10", FCVAR_CHEAT, "Minimum kills needed to get the grub medal, if the team has wiped out all grubs in the map");
ConVar asw_medal_egg_kills("asw_medal_egg_kills", "10", FCVAR_CHEAT, "Minimum kills needed to get the eggs medal, if the team has wiped out all grubs in the map");
ConVar asw_medal_parasite_kills("asw_medal_parasite_kills", "50", FCVAR_CHEAT, "Minimum kills needed to get the parasite medal");
ConVar asw_medal_firefighter("asw_medal_firefighter", "10", FCVAR_CHEAT, "Minimum fires needed to put out to get the firefighter medal");
//ConVar asw_medal_last_stand_kills("asw_medal_last_stand_kills", "10", FCVAR_CHEAT, "Minimum kills needed to get the last stand medal");
ConVar asw_medal_melee_hits("asw_medal_melee_hits", "20", FCVAR_CHEAT, "Minimum kicks needed to get the iron fist medal");
ConVar asw_medal_barrel_kills("asw_medal_barrel_kills", "5", FCVAR_CHEAT, "Minimum aliens killed with barrels to get the Collateral damage medal");

ConVar asw_debug_medals("asw_debug_medals", "0", 0, "Print debug info about medals");

// counts needed for persistent stats medals
//#define ASW_MISSIONS_MEDAL_1 100
//#define ASW_MISSIONS_MEDAL_2 1000
//#define ASW_CAMPAIGNS_MEDAL_1 10
//#define ASW_CAMPAIGNS_MEDAL_2 100
//#define ASW_KILLS_MEDAL_1 5000
//#define ASW_KILLS_MEDAL_2 10000
//#define ASW_KILLS_MEDAL_3 50000
//#define ASW_KILLS_MEDAL_4 100000

int s_nRepeatableMedals[] =
{
	MEDAL_CLEAR_FIRING,
	MEDAL_SHIELDBUG_ASSASSIN,
	MEDAL_EXPLOSIVES_MERIT,
	MEDAL_SHARPSHOOTER,
	MEDAL_PERFECT,
	//MEDAL_STATIC_DEFENSE,
	MEDAL_ALL_SURVIVE_A_MISSION,
	MEDAL_IRON_FIST,
	MEDAL_COLLATERAL_DAMAGE,
	MEDAL_GOLDEN_HALO,
	MEDAL_BLOOD_HALO,
	MEDAL_PEST_CONTROL,
	MEDAL_EXTERMINATOR,
	MEDAL_ELECTRICAL_SYSTEMS_EXPERT,
	MEDAL_SMALL_ARMS_SPECIALIST,
	MEDAL_GUNFIGHTER,
	MEDAL_BUGSTOMPER,
	MEDAL_RECKLESS_EXPLOSIVES_MERIT,
	MEDAL_LIFESAVER,
	MEDAL_HUNTER,
	MEDAL_SPEED_RUN_LANDING_BAY,
	MEDAL_SPEED_RUN_OUTSIDE,
	MEDAL_SPEED_RUN_PLANT,
	MEDAL_SPEED_RUN_OFFICE,
	MEDAL_SPEED_RUN_DESCENT,
	MEDAL_SPEED_RUN_SEWERS,
	MEDAL_SPEED_RUN_MINE,

};

// testing version
//#define ASW_MISSIONS_MEDAL_1 3
//#define ASW_MISSIONS_MEDAL_2 6
//#define ASW_CAMPAIGNS_MEDAL_1 1
//#define ASW_CAMPAIGNS_MEDAL_2 3
//#define ASW_KILLS_MEDAL_1 50
//#define ASW_KILLS_MEDAL_2 100
//#define ASW_KILLS_MEDAL_3 150
//#define ASW_KILLS_MEDAL_4 200

void CASW_Medals::OnStartMission()
{
	// count how many eggs there are
	CBaseEntity* pEntity = NULL;
	m_iNumEggs = 0;
	m_bAwardEggMedal = false;
	m_bAllSurvived = false;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "asw_egg" )) != NULL)
	{
		m_iNumEggs++;
	}
	if (asw_debug_medals.GetBool())
		Msg("Medals: %d eggs in the mission\n", m_iNumEggs);

	// count how many grubs there are in the mission at the start
	pEntity = NULL;
	ASWGameRules()->m_iNumGrubs = 0;
	m_bAwardGrubMedal = false;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "asw_grub" )) != NULL)
	{
		ASWGameRules()->m_iNumGrubs++;		// note: this var will get increased when grub sacs spawn grubs too
	}
	m_fStartMissionTime = gpGlobals->curtime;
}

void CASW_Medals::AwardMedals()
{
	if (!ASWGameRules() || !ASWGameResource())
		return;
	
	CASW_Game_Resource *pGameResource = ASWGameResource();

	// did the marines kill all the eggs?
	int iNumEggsKilled = 0;
	int iNumGrubsKilled = 0;
	m_bAllSurvived = true;
	int nMarineResources = 0;
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if (pMR)
		{
			iNumEggsKilled += pMR->m_iEggKills;
			iNumGrubsKilled += pMR->m_iGrubKills;

			if ( pMR->GetHealthPercent() <= 0.0f )
			{
				m_bAllSurvived = false;
			}
			nMarineResources++;
		}
	}
	m_bAwardEggMedal = iNumEggsKilled >= m_iNumEggs;
	m_bAwardGrubMedal = iNumGrubsKilled >= ASWGameRules()->m_iNumGrubs;

	if (asw_debug_medals.GetBool())
		Msg("Marines killed %d out of %d eggs\n", iNumEggsKilled, m_iNumEggs);

	// go through each marine
	bool bTechAlive = false;

	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if ( pMR )
		{
			AwardMedalsTo( pMR );

			if ( pMR->GetProfile() && pMR->IsAlive() && pMR->GetHealthPercent() > 0.0f )
			{
				if ( pMR->GetProfile()->GetMarineClass() == MARINE_CLASS_TECH )
				{
					bTechAlive = true;
				}
			}
		}
	}

	// award player medals to each player
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CASW_Player* pPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i));

		if ( pPlayer && pPlayer->IsConnected() )
		{
#ifdef USE_MEDAL_STORE
			AwardPlayerMedalsTo(pPlayer);
#endif

			if ( pPlayer->GetMarine() )
			{
				if ( ASWGameRules()->GetMissionSuccess() )
				{
					if ( bTechAlive )
					{
						pPlayer->AwardAchievement( ACHIEVEMENT_ASW_TECH_SURVIVES );
					}
				}
			}
		}
	}
}

void CASW_Medals::AwardMedalsTo(CASW_Marine_Resource *pMR)
{
	CASW_Game_Resource *pGameResource = ASWGameResource();

	if ( !pMR || !pGameResource )
		return;

	if ( pGameResource->IsOfflineGame() )
		return;

	if ( !pMR->IsInhabited() )
		return;

	CASW_Player *pPlayer = pMR->GetCommander();
	if ( !pPlayer )
		return;

	pMR->m_bAwardedMedals = true;

	//CASW_Marine *pMarine = pMR->GetMarineEntity();
	//DebugMedals(pMR);
	if (asw_debug_medals.GetBool() && pMR->GetProfile())
		Msg("Awarding medals to %s\n", pMR->GetProfile()->m_ShortName);

	if ( asw_debug_medals.GetInt() == 2 )
	{
		for ( int i = 0; i < 4; i++ )
		{
			AwardSingleMedalTo( s_nRepeatableMedals[ RandomInt( 0, NELEMS( s_nRepeatableMedals ) - 1 ) ], pMR );
		}
		//AwardSingleMedalTo( RandomInt( MEDAL_BLOOD_HEART, MEDAL_OUTSTANDING_EXECUTION_QUEEN_LAIR ), pMR );
		return;
	}

	// add achievements to the medal string
	char achievement_buffer[ 255 ];
	achievement_buffer[0] = 0;
	for ( int i = 0; i < pMR->m_aAchievementsEarned.Count(); i++ )
	{
		if ( i > 0 )
		{
			Q_strncat( achievement_buffer, UTIL_VarArgs( " %d", -pMR->m_aAchievementsEarned[i] ), sizeof( achievement_buffer ), COPY_ALL_CHARACTERS );
		}
		else
		{
			Q_strncat( achievement_buffer, UTIL_VarArgs( "%d", -pMR->m_aAchievementsEarned[i] ), sizeof( achievement_buffer ), COPY_ALL_CHARACTERS );
		}
	}
	Q_snprintf( pMR->m_MedalsAwarded.GetForModify(), 255, "%s", achievement_buffer );

	// todo: some of these medals should only be awarded if all marines are alive by the end?

	// award campaign completion medals?
	if ( ASWGameRules()->IsCampaignGame() && ASWGameRules()->GetMissionSuccess() &&
		 ASWGameRules()->CampaignMissionsLeft() <= 1 )	// 1 mission left, because medals get awarded before the save is updated with the last mission complete
	{
		int iSkill = ASWGameRules()->GetLowestSkillLevelPlayed();
		if ( ASWGameRules()->GetSkillLevel() < iSkill )	// check they didn't just complete the last mission on a low skill
			iSkill = ASWGameRules()->GetSkillLevel();

		bool bJacobCampaign = ASWGameRules()->GetCampaignInfo() && ASWGameRules()->GetCampaignInfo()->IsJacobCampaign();	
		if ( iSkill >= 2 && ASWGameRules()->GetCampaignSave() && ASWGameRules()->GetCampaignSave()->m_iNumDeaths <= 0 && ASWGameRules()->GetCampaignSave()->m_iInitialNumMissionsComplete == 0 && bJacobCampaign )
		{
			pPlayer->AwardAchievement( ACHIEVEMENT_ASW_CAMPAIGN_NO_DEATHS );
		}
	}

	if ( ASWGameRules()->GetSkillLevel() >= 2 && ASWGameRules()->GetMissionSuccess() )
	{
		if ( m_bAllSurvived && pGameResource->GetNumMarines( NULL, true ) >= 4 )
		{
			AwardSingleMedalTo( MEDAL_ALL_SURVIVE_A_MISSION, pMR );
			pPlayer->AwardAchievement( ACHIEVEMENT_ASW_MISSION_NO_DEATHS );
		}
	}
	
	// Clear Firing - awarded for having no friendly fire incidents
	if (pMR->m_fFriendlyFireDamageDealt <= 0 && ASWGameRules()->GetMissionSuccess() && pMR->m_iAliensKilled >= 25
		&& pMR->m_iPlayerShotsFired >= 25 )
	{
		if (ASWGameResource() && ASWGameResource()->GetNumMarines(NULL) >= 4)
		{
			pPlayer->AwardAchievement( ACHIEVEMENT_ASW_NO_FRIENDLY_FIRE );
			AwardSingleMedalTo(MEDAL_CLEAR_FIRING, pMR);
		}
	}

	// Shieldbug Assassin - awarded for killing a shieldbug
	if (pMR->m_iShieldbugsKilled > 0)
		AwardSingleMedalTo(MEDAL_SHIELDBUG_ASSASSIN, pMR);

	// Explosives Merit - awarded for killing a number of aliens in one explosion
	if (pMR->m_iSingleGrenadeKills >= asw_medal_explosive_kills.GetInt())
		AwardSingleMedalTo(MEDAL_EXPLOSIVES_MERIT, pMR);

	// Perfect - awarded for finishing a mission taking no damage
	if ( !pMR->m_bHurt && pMR->m_iAliensKilled >= 25 && ASWGameRules()->GetSkillLevel() >= 2 && ASWGameRules()->GetMissionSuccess() )
	{
		AwardSingleMedalTo( MEDAL_PERFECT, pMR );
		pPlayer->AwardAchievement( ACHIEVEMENT_ASW_NO_DAMAGE_TAKEN );
	}

	/////
	// Old medals! To reinable you'll need to uncomment this code as well as adding them to the repeatable list, s_nRepeatableMedals!
	/////

	// Blood heart - awarded when a marine is wounded in a mission
	/*if (pMR->m_bTakenWoundDamage)	
	AwardSingleMedalTo(MEDAL_BLOOD_HEART, pMR);*/

	// Reckless Explosives Merit - awarded for kicking a grenade into an alien
	/*if (pMR->m_iKickedGrenadeKills >= asw_medal_reckless_explosive_kills.GetInt())
	AwardSingleMedalTo(MEDAL_RECKLESS_EXPLOSIVES_MERIT, pMR);*/

	// Lifesaver - awarded for killing a bug that was close to a teammate
	/*if (pMR->m_iSavedLife >= asw_medal_lifesaver_kills.GetInt())
	AwardSingleMedalTo(MEDAL_LIFESAVER, pMR);*/

	// Killing spree - awarded for killing x bugs in y seconds (i.e. doing the mad firing speech)
	/*if (pMR->m_iMadFiring > 0)
		AwardSingleMedalTo(MEDAL_KILLING_SPREE, pMR);*/
	
	// Swarm Suppression - awarded for killing x bugs in y seconds with the autogun (i.e. doing the mad firing speech holding an autogun)
	/*if (pMR->m_iMadFiringAutogun > 0)
		AwardSingleMedalTo(MEDAL_SWARM_SUPPRESSION, pMR);*/

	// mine medal awarded for killing X bugs with incendiary mines
	/*if (pMR->m_iMineKills >= asw_medal_mine_burns.GetInt())
	AwardSingleMedalTo(MEDAL_INCENDIARY_DEFENCE, pMR);*/

	// sentry medal awarded for killing X bugs with a sentry gun
	/*if (pMR->m_iSentryKills >= asw_medal_sentry_kills.GetInt())
	AwardSingleMedalTo(MEDAL_STATIC_DEFENSE, pMR);*/

	// awarded for putting out a number of fires
	/*if (pMR->m_iFiresPutOut >= asw_medal_firefighter.GetInt() && ASWGameRules()->GetMissionSuccess())
	AwardSingleMedalTo(MEDAL_FIREFIGHTER, pMR);*/

	// awarded for killing aliens during the last stand time
	/*if ( pMR->m_iLastStandKills >= asw_medal_last_stand_kills.GetInt() && pMR->GetHealthPercent() <= 0 )
	AwardSingleMedalTo(MEDAL_LAST_STAND, pMR);*/

	// awarded for hacking a door quickly
	/*if ( pMR->m_iFastDoorHacks >= 3 )
	AwardSingleMedalTo( MEDAL_ELECTRICAL_SYSTEMS_EXPERT, pMR );	*/	

	// awarded for hacking a computer quickly
	/*if ( pMR->m_iFastComputerHacks > 0 )
	AwardSingleMedalTo( MEDAL_COMPUTER_SYSTEMS_EXPERT, pMR );	*/	

	// grub medal awarded for killing all grubs on a map, at least X
	//if (m_bAwardGrubMedal)
	//{
	//	// check there's no unpopped grub sacs left
	//	CBaseEntity* pEntity = NULL;
	//	int iNumSacs = 0;		
	//	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "asw_grub_sac" )) != NULL)
	//	{
	//		iNumSacs++;
	//	}
	//	if (iNumSacs <= 0)
	//	{
	//		if (m_bAwardGrubMedal && pMR->m_iGrubKills > asw_medal_grub_kills.GetInt() && ASWGameRules()->GetMissionSuccess())	
	//			AwardSingleMedalTo(MEDAL_BUGSTOMPER, pMR);
	//	}
	//}

	float accuracy = 0;
	if ( pMR->m_iPlayerShotsFired > 0 )
	{
		accuracy = float(pMR->m_iPlayerShotsFired - pMR->m_iPlayerShotsMissed) / float(pMR->m_iPlayerShotsFired);
	}

	// Old medals!

	// have we only fired 1 weapon type throughout the mission? award special medals if so
	// requires some minimum number of kills, mission success, 90% accuracy and hard difficulty
	//if (pMR->m_iOnlyWeaponEquipIndex >= 0 && ASWEquipmentList()
	//	&& pMR->m_iAliensKilled >= 10 && ASWGameRules()->GetMissionSuccess()
	//	&& accuracy >= 0.9f && ASWGameRules()->GetSkillLevel() >= 3)
	//{		
	//	if (!pMR->m_bOnlyWeaponExtra)
	//	{
	//		CASW_EquipItem *pItem = ASWEquipmentList()->GetRegular(pMR->m_iOnlyWeaponEquipIndex);
	//		if (pItem)
	//		{
	//			if (!Q_strcmp(STRING(pItem->m_EquipClass), "asw_weapon_rifle")
	//				|| !Q_strcmp(STRING(pItem->m_EquipClass), "asw_weapon_prifle"))				
	//				AwardSingleMedalTo(MEDAL_IRON_DAGGER, pMR);
	//			else if (!Q_strcmp(STRING(pItem->m_EquipClass), "asw_weapon_flamer"))
	//				AwardSingleMedalTo(MEDAL_PYROMANIAC, pMR);
	//			else if (!Q_strcmp(STRING(pItem->m_EquipClass), "asw_weapon_shotgun"))
	//				AwardSingleMedalTo(MEDAL_HUNTER, pMR);
	//			//else if (!Q_strcmp(STRING(pItem->m_EquipClass), "asw_weapon_railgun"))
	//				//AwardSingleMedalTo(MEDAL_HYBRID_WEAPONS_EXPERT, pMR);
	//			else if (!Q_strcmp(STRING(pItem->m_EquipClass), "asw_weapon_pdw"))
	//				AwardSingleMedalTo(MEDAL_SMALL_ARMS_SPECIALIST, pMR);
	//			else if (!Q_strcmp(STRING(pItem->m_EquipClass), "asw_weapon_autogun"))
	//				AwardSingleMedalTo(MEDAL_IRON_SWORD, pMR);
	//			else if (!Q_strcmp(STRING(pItem->m_EquipClass), "asw_weapon_vindicator"))
	//				AwardSingleMedalTo(MEDAL_IRON_HAMMER, pMR);
	//		}
	//	}
	//	else
	//	{
	//		CASW_EquipItem *pItem = ASWEquipmentList()->GetExtra(pMR->m_iOnlyWeaponEquipIndex);
	//		if (pItem)
	//		{
	//			if (!Q_strcmp(STRING(pItem->m_EquipClass), "asw_weapon_pistol"))
	//				AwardSingleMedalTo(MEDAL_GUNFIGHTER, pMR);
	//		}
	//	}
	//}

	// accuracy medal awarded for achieving X hits with shots fired from guns		
	if ( accuracy > asw_medal_accuracy.GetFloat() && pMR->m_iAliensKilled >= 10 && ASWGameRules()->GetMissionSuccess() )
	{
		AwardSingleMedalTo(MEDAL_SHARPSHOOTER, pMR);
		pPlayer->AwardAchievement( ACHIEVEMENT_ASW_ACCURACY );
	}
	
	// halo medals, awarded for different amounts of healing alien damage from marines
	bool bMedic = ( pMR->GetProfile() && pMR->GetProfile()->CanUseFirstAid() );
	if ( bMedic )
	{
		// Old medals!

		/*bool bAwardedSilverHalo = false;
		if (pMR->m_iMedicHealing >= asw_medal_silver_heal_amount.GetInt())
		{
			bAwardedSilverHalo = AwardSingleMedalTo(MEDAL_SILVER_HALO, pMR);
		}
		if ( !bAwardedSilverHalo && pMR->m_iMedicHealing >= asw_medal_blood_heal_amount.GetInt() )
			AwardSingleMedalTo(MEDAL_BLOOD_HALO, pMR);*/

		// golden halo awarded for healing a marine so he survives Infestation
		if ( pMR->m_iCuredInfestation > 0 )
		{
			AwardSingleMedalTo( MEDAL_GOLDEN_HALO, pMR );
		}

		const int iAllMedicHealAmount = 300;
		if ( pMR->m_iMedicHealing >= iAllMedicHealAmount && !pMR->m_bDealtNonMeleeDamage )
		{
			AwardSingleMedalTo( MEDAL_BLOOD_HALO, pMR );
			pPlayer->AwardAchievement( ACHIEVEMENT_ASW_ALL_HEALING );
		}
	}

	if ( ASWGameResource()->m_iStartingEggsInMap > 5 && ASWGameResource()->m_iEggsKilled >= ASWGameResource()->m_iStartingEggsInMap && ASWGameResource()->m_iEggsHatched <= 0 )
	{
		// No eggs hatched FTW!
		AwardSingleMedalTo( MEDAL_PEST_CONTROL, pMR );
	}

	if ( pMR->m_bMeleeParasiteKill )
	{
		AwardSingleMedalTo(MEDAL_EXTERMINATOR, pMR);
	}
	
	if ( pMR->m_bProtectedTech )
	{
		AwardSingleMedalTo(MEDAL_ELECTRICAL_SYSTEMS_EXPERT, pMR);
	}

	if ( pMR->m_bDidFastReloadsInARow )
	{
		AwardSingleMedalTo(MEDAL_SMALL_ARMS_SPECIALIST, pMR);
	}

	if ( pMR->m_bDamageAmpMedal )
	{
		AwardSingleMedalTo(MEDAL_GUNFIGHTER, pMR);
	}

	if ( pMR->m_bKilledBoomerEarly )
	{
		AwardSingleMedalTo(MEDAL_BUGSTOMPER, pMR);
	}

	if ( pMR->m_bStunGrenadeMedal )
	{
		AwardSingleMedalTo(MEDAL_RECKLESS_EXPLOSIVES_MERIT, pMR);
	}

	if ( pMR->m_bDodgedRanger )
	{
		AwardSingleMedalTo(MEDAL_LIFESAVER, pMR);
	}

	if ( pMR->m_bFreezeGrenadeMedal )
	{
		AwardSingleMedalTo(MEDAL_HUNTER, pMR);
	}

	// awarded for kicking X aliens
	if ( pMR->m_iAliensKicked >= asw_medal_melee_hits.GetInt() )
	{
		AwardSingleMedalTo( MEDAL_IRON_FIST, pMR );
	}

	// awarded for killing X aliens with barrels
	if ( pMR->m_iBarrelKills >= asw_medal_barrel_kills.GetInt() )
	{
		AwardSingleMedalTo( MEDAL_COLLATERAL_DAMAGE, pMR );		
	}
	
	if ( ASWGameRules()->GetMissionSuccess() )
	{
		if ( m_bAllSurvived )
		{
			// speed runs
			int iCompleteSeconds = gpGlobals->curtime - m_fStartMissionTime;
			if (asw_debug_medals.GetBool())
				Msg("Mission complete, took %d seconds\n", iCompleteSeconds);

			const char *mapName = STRING(gpGlobals->mapname);
			if (asw_debug_medals.GetBool())
				Msg("Medal checking mapname: %s\n", mapName);

			int speedrun_time = 240;
			if ( GetWorldEntity() && ASWGameRules()->GetSpeedrunTime() > 0 )
			{
				speedrun_time = ASWGameRules()->GetSpeedrunTime();
			}
			
			if ( !Q_stricmp( mapName, "ASI-Jac1-LandingBay_01" ) && iCompleteSeconds <= speedrun_time)
			{
				AwardSingleMedalTo(MEDAL_SPEED_RUN_LANDING_BAY, pMR);
				pPlayer->AwardAchievement( ACHIEVEMENT_ASW_SPEEDRUN_LANDING_BAY );
			}
			else if ( !Q_stricmp( mapName, "ASI-Jac1-LandingBay_02" ) && iCompleteSeconds <= speedrun_time)
			{
				AwardSingleMedalTo(MEDAL_SPEED_RUN_DESCENT, pMR);
				pPlayer->AwardAchievement( ACHIEVEMENT_ASW_SPEEDRUN_DESCENT );
			}
			else if ( !Q_stricmp( mapName, "ASI-Jac2-Deima" ) && iCompleteSeconds <= speedrun_time)
			{
				AwardSingleMedalTo(MEDAL_SPEED_RUN_OUTSIDE, pMR);
				pPlayer->AwardAchievement( ACHIEVEMENT_ASW_SPEEDRUN_DEIMA );
			}
			else if ( !Q_stricmp( mapName, "ASI-Jac3-Rydberg" ) && iCompleteSeconds <= speedrun_time)
			{
				AwardSingleMedalTo(MEDAL_SPEED_RUN_PLANT, pMR);
				pPlayer->AwardAchievement( ACHIEVEMENT_ASW_SPEEDRUN_RYDBERG );
			}
			else if ( !Q_stricmp( mapName, "ASI-Jac4-Residential" ) && iCompleteSeconds <= speedrun_time)
			{
				AwardSingleMedalTo(MEDAL_SPEED_RUN_OFFICE, pMR);
				pPlayer->AwardAchievement( ACHIEVEMENT_ASW_SPEEDRUN_RESIDENTIAL );
			}
 			/*else if ( !Q_stricmp( mapName, "ASI-Jac5-BioResearch" ) && iCompleteSeconds <= speedrun_time)
 			{
 				AwardSingleMedalTo(MEDAL_SPEED_RUN_LABS, pMR);
 			}*/
			else if ( !Q_stricmp( mapName, "ASI-Jac6-SewerJunction" ) && iCompleteSeconds <= speedrun_time)
			{
				AwardSingleMedalTo(MEDAL_SPEED_RUN_SEWERS, pMR);
				pPlayer->AwardAchievement( ACHIEVEMENT_ASW_SPEEDRUN_SEWER );
			}
			else if ( !Q_stricmp( mapName, "ASI-Jac7-TimorStation" ) && iCompleteSeconds <= speedrun_time)
			{
				AwardSingleMedalTo(MEDAL_SPEED_RUN_MINE, pMR);
				pPlayer->AwardAchievement( ACHIEVEMENT_ASW_SPEEDRUN_TIMOR );
			}
			/*else if ( !Q_stricmp( mapName, "ASI-Jac8-LastRites" ) && iCompleteSeconds <= speedrun_time)
			{
				AwardSingleMedalTo(MEDAL_SPEED_RUN_QUEEN_LAIR, pMR);
			}*/
		}

		// Old medals

		// outstanding execution - requires insane difficulty
		/*if (ASWGameRules()->IsCampaignGame() && ASWGameRules()->GetCampaignSave()
			 && ASWGameRules()->GetSkillLevel() >= 4)
		{
			if (ASWGameRules()->GetCampaignSave()->GetRetries() <= 1)
			{
				if ( !Q_stricmp( mapName, "ASI-Jac1-LandingBay" ))
				{
					AwardSingleMedalTo(MEDAL_OUTSTANDING_EXECUTION_LANDING_BAY, pMR);
				}
				else if ( !Q_stricmp( mapName, "ASI-Jac2-Deima" ))
				{
					AwardSingleMedalTo(MEDAL_OUTSTANDING_EXECUTION_OUTSIDE, pMR);
				}
				else if ( !Q_stricmp( mapName, "ASI-Jac3-Rydberg" ))
				{
					AwardSingleMedalTo(MEDAL_OUTSTANDING_EXECUTION_PLANT, pMR);
				}
				else if ( !Q_stricmp( mapName, "ASI-Jac4-Residential" ))
				{
					AwardSingleMedalTo(MEDAL_OUTSTANDING_EXECUTION_OFFICE, pMR);
				}
				else if ( !Q_stricmp( mapName, "ASI-Jac5-BioResearch" ))
				{
					AwardSingleMedalTo(MEDAL_OUTSTANDING_EXECUTION_LABS, pMR);
				}
				else if ( !Q_stricmp( mapName, "ASI-Jac6-SewerJunction" ))
				{
					AwardSingleMedalTo(MEDAL_OUTSTANDING_EXECUTION_SEWERS, pMR);
				}
				else if ( !Q_stricmp( mapName, "ASI-Jac7-TimorStation" ))
				{
					AwardSingleMedalTo(MEDAL_OUTSTANDING_EXECUTION_MINE, pMR);
				}
				else if ( !Q_stricmp( mapName, "ASI-Jac8-LastRites" ))
				{
					AwardSingleMedalTo(MEDAL_OUTSTANDING_EXECUTION_QUEEN_LAIR, pMR);
				}
			}
		}*/
	}
	
	//DebugMedals(pMR);
}

bool CASW_Medals::AwardSingleMedalTo(int iMedal, CASW_Marine_Resource *pMR)
{
	if (iMedal <= 0 || iMedal >= LAST_MEDAL)
		return false;

	// most medals have been replaced with achievements.  Only support a subset of repeatable ones.
	bool bSupported = false;
	for ( int i = 0; i < NELEMS( s_nRepeatableMedals ); i++ )
	{
		if ( s_nRepeatableMedals[ i ] == iMedal )
		{
			bSupported = true;
			break;
		}
	}
	if ( !bSupported )
		return false;

	// check he doesn't have it already
	if (HasMedal(iMedal, pMR))
		return false;

	// add the new medal to the end of the current medals this marine has been awarded
	char buffer[255];
	if (Q_strlen(pMR->m_MedalsAwarded.Get()) <= 0)
	{
		Q_snprintf(buffer, sizeof(buffer), "%d", iMedal);
	}
	else
		Q_snprintf(buffer, sizeof(buffer), "%s %d", pMR->m_MedalsAwarded.Get(), iMedal);

	// copy new string into the marine info
	Q_snprintf(pMR->m_MedalsAwarded.GetForModify(), 255, "%s", buffer);
	//Msg("Awarding medal, setting medal string to:%s\n", buffer);

	// update char array of medal used for uploading stats
	pMR->m_CharMedals.AddToTail(iMedal);

	return true;
}

void CASW_Medals::DebugMedals(CASW_Marine_Resource *pMR)
{
	const char	*p = pMR->m_MedalsAwarded.Get();
	char		token[128];
	
	p = nexttoken( token, p, ' ' );	
	
	Msg("Marine %s has medals (p=%s token=%s):\n", pMR->GetProfile()->m_ShortName,
		p, token);
	// list all the medals
	while ( Q_strlen( token ) > 0 )  
	{
		int iHasMedal = atoi(token);
		Msg("%d ", iHasMedal);
		if (p)
			p = nexttoken( token, p, ' ' );
		else
			token[0] = '\0';
	}
	Msg("\n");
}

bool CASW_Medals::HasMedal(int iMedal, CASW_Marine_Resource *pMR, bool bOnlyThisMission)
{
	if (!pMR)
		return false;

	// check in the medals awarded so far this mission
	const char	*p = pMR->m_MedalsAwarded.Get();
	char		token[128];
	
	p = nexttoken( token, p, ' ' );
	//p = nexttoken( token, p, ' ' );

	while ( Q_strlen( token ) > 0 ) 
	{
		int iHasMedal = atoi(token);
		if (iHasMedal == iMedal)
		{
			//Msg("Marine already has medal %d, so can't award it\n", iMedal);
			return true;
		}
		if ( iHasMedal < 0 && MedalMatchesAchievement( iMedal, -iHasMedal ) )
			return true;
		if (p)
			p = nexttoken( token, p, ' ' );
		else
			token[0] = '\0';
	}

	if (bOnlyThisMission)
		return false;

	// check in the medals awarded in previous missions
#ifdef USE_MEDAL_STORE
	if (ASWGameRules()->IsCampaignGame())
	{
		CASW_Campaign_Save* pSave = ASWGameRules()->GetCampaignSave();
		int iProfileIndex = pMR->GetProfileIndex();
		if (pSave && iProfileIndex >=0 && iProfileIndex <ASW_NUM_MARINE_PROFILES)
		{
			p = STRING(pSave->m_Medals[iProfileIndex]);
			p = nexttoken( token, p, ' ' );
			//p = nexttoken( token, p, ' ' );

			while ( Q_strlen( token ) > 0 ) 
			{
				int iHasMedal = atoi(token);
				if (iHasMedal == iMedal)
				{
					Msg("Marine already has medal %d, so can't award it\n", iMedal);
					return true;
				}
				if (p)
					p = nexttoken( token, p, ' ' );
				else
					token[0] = '\0';
			}
		}
	}
#endif
	//Msg("Marine doesn't have medal %d, so awarding it\n", iMedal);
	return false;
}

void CASW_Medals::AddMedalsToCampaignSave(CASW_Campaign_Save *pSave)
{
	if ( !ASWGameResource() )
		return;

	CASW_Game_Resource *pGameResource = ASWGameResource();
	// go through each marine
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if (!pMR)
			continue;

		int iProfileIndex = pMR->GetProfileIndex();
		if (iProfileIndex < 0 || iProfileIndex >= ASW_NUM_MARINE_PROFILES)
			continue;
		
		if (Q_strlen(pMR->m_MedalsAwarded.Get()) <= 0)
			continue;

		// add the new medal to the end of the current medals this marine has been awarded
		char buffer[255];
		if (Q_strlen(STRING(pSave->m_Medals[iProfileIndex])) <= 0)
			Q_snprintf(buffer, sizeof(buffer), "%s", pMR->m_MedalsAwarded.Get());
		else
			Q_snprintf(buffer, sizeof(buffer), "%s %s", STRING(pSave->m_Medals[iProfileIndex]), pMR->m_MedalsAwarded.Get());

		// copy new string into the campaign save		
		pSave->m_Medals.Set(iProfileIndex, AllocPooledString(buffer));
		//Q_snprintf(pMR->m_MedalsAwarded.GetForModify(), 255, "%s", buffer);
		Msg("[p%d] Added medals to campaign save:%s\n", iProfileIndex, buffer);

		// clear the medals our the marine info now
		pMR->m_MedalsAwarded.GetForModify()[0] = '\0';
	}	
}

// ================ Player Medals =======================

void CASW_Medals::AwardPlayerMedalsTo(CASW_Player *pPlayer)
{
	if (!pPlayer || !ASWGameRules())
		return;
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	// don't award player medals to spectators
	if (pGameResource->GetNumMarines(pPlayer) <= 0)
		return;

	//DebugMedals(pPlayer);
	//Msg("Awarding medals to player %d\n", pPlayer->entindex());

	// give him some random medals
	//int iNumMedals = random->RandomInt(5, 5);
	//for (int i=0;i<iNumMedals;i++)
	//{
		//AwardSinglePlayerMedalTo(random->RandomInt(MEDAL_BLOOD_HEART, MEDAL_OUTSTANDING_EXECUTION_QUEEN_LAIR), pPlayer);
	//}

	// Old medals!

	//if (ASWGameRules()->GetMissionSuccess())
	//{
	//	//char mapName[255];
	//	//Q_FileBase( engine->GetLevelName(), mapName, sizeof(mapName) );
	//	const char *mapName = STRING(gpGlobals->mapname);
	//	
	//	if ( !Q_strncmp( mapName, "tutorial", 8 ) )
	//	{
	//		AwardSinglePlayerMedalTo(MEDAL_IAF_TRAINING, pPlayer);
	//	}

	//	// check for X missions medals
	//	if (ASWGameRules()->GetSkillLevel() >= 2)	// minimum normal difficulty
	//	{
	//		int iOldMissions = pPlayer->m_iClientMissionsCompleted;
	//		if (iOldMissions == ASW_MISSIONS_MEDAL_1 -1)
	//		{
	//			AwardSinglePlayerMedalTo(MEDAL_IAF_COMBAT_HONORS, pPlayer);
	//		}
	//		if (iOldMissions == ASW_MISSIONS_MEDAL_2 - 1)
	//		{
	//			AwardSinglePlayerMedalTo(MEDAL_IAF_BATTLE_HONORS, pPlayer);
	//		}

	//		// if this is the last mission in the campaign, check if any players are 1 campaign away from a campaign medal
	//		if (ASWGameRules()->IsCampaignGame() && ASWGameRules()->CampaignMissionsLeft() == 1)
	//		{
	//			int iOldCampaigns = pPlayer->m_iClientCampaignsCompleted;
	//			if (iOldCampaigns == ASW_CAMPAIGNS_MEDAL_1 - 1)
	//			{
	//				AwardSinglePlayerMedalTo(MEDAL_IAF_CAMPAIGN_HONORS, pPlayer);
	//			}
	//			if (iOldCampaigns == ASW_CAMPAIGNS_MEDAL_2 - 1)
	//			{
	//				AwardSinglePlayerMedalTo(MEDAL_IAF_WARTIME_SERVICE, pPlayer);
	//			}
	//		}
	//	}
	//}
	
	// check for X kills medals
	/*int iOldKills = pPlayer->m_iClientKills;
	int iNewKills = iOldKills;
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if (pMR && pMR->GetCommander() == pPlayer)
			iNewKills += pMR->m_iAliensKilled;
	}
	if (iOldKills < ASW_KILLS_MEDAL_1 && iNewKills >= ASW_KILLS_MEDAL_1)
	{
		AwardSinglePlayerMedalTo(MEDAL_PROFESSIONAL, pPlayer);
	}
	if (iOldKills < ASW_KILLS_MEDAL_2 && iNewKills >= ASW_KILLS_MEDAL_2)
	{
		AwardSinglePlayerMedalTo(MEDAL_NEMESIS, pPlayer);
	}
	if (iOldKills < ASW_KILLS_MEDAL_3 && iNewKills >= ASW_KILLS_MEDAL_3)
	{
		AwardSinglePlayerMedalTo(MEDAL_RETRIBUTION, pPlayer);
	}
	if (iOldKills < ASW_KILLS_MEDAL_4 && iNewKills >= ASW_KILLS_MEDAL_4)
	{
		AwardSinglePlayerMedalTo(MEDAL_IAF_HERO, pPlayer);
	}*/

	//DebugMedals(pPlayer);
}

bool CASW_Medals::AwardSinglePlayerMedalTo(int iMedal, CASW_Player *pPlayer)
{
	if (iMedal <= 0 || iMedal >= LAST_MEDAL)
		return false;

	// most medals have been replaced with achievements.  Only support a subset of repeatable ones.
	bool bSupported = false;
	for ( int i = 0; i < NELEMS( s_nRepeatableMedals ); i++ )
	{
		if ( s_nRepeatableMedals[ i ] == iMedal )
		{
			bSupported = true;
			break;
		}
	}
	if ( !bSupported )
		return false;

	// check he doesn't have it already
	if (HasPlayerMedal(iMedal, pPlayer))
		return false;

	if ( !ASWGameResource() )
		return false;

	int iPlayerIndex = pPlayer->entindex() - 1;
	if (iPlayerIndex < 0 || iPlayerIndex >= ASW_MAX_READY_PLAYERS)
		return false;

	// add the new medal to the end of the current medals this marine has been awarded
	char buffer[255];
	if (Q_strlen(STRING(ASWGameResource()->m_iszPlayerMedals[iPlayerIndex])) <= 0)
	{
		Q_snprintf(buffer, sizeof(buffer), "%d", iMedal);
	}
	else
		Q_snprintf(buffer, sizeof(buffer), "%s %d", STRING(ASWGameResource()->m_iszPlayerMedals[iPlayerIndex]), iMedal);

	// copy new string into the game resource
	ASWGameResource()->m_iszPlayerMedals.Set(iPlayerIndex, AllocPooledString(buffer));
	//Msg("Awarding player medal, setting medal string to:%s\n", buffer);

	// update char array of medal used for uploading stats
	pPlayer->m_CharMedals.AddToTail(iMedal);

	return true;
}

void CASW_Medals::DebugMedals(CASW_Player *pPlayer)
{
	int iPlayerIndex = pPlayer->entindex() - 1;
	if (iPlayerIndex < 0 || iPlayerIndex >= ASW_MAX_READY_PLAYERS)
		return;

	const char	*p = STRING(ASWGameResource()->m_iszPlayerMedals[iPlayerIndex]);
	char		token[128];
	
	p = nexttoken( token, p, ' ' );
	//p = nexttoken( token, p, ' ' );
	
	Msg("Player %d has medals (p=%s token=%s):\n", iPlayerIndex,
		p, token);
	// list all the medals
	while ( Q_strlen( token ) > 0 )  
	{
		int iHasMedal = atoi(token);
		Msg("%d ", iHasMedal);
		if (p)
			p = nexttoken( token, p, ' ' );
		else
			token[0] = '\0';
	}
	Msg("\n");
}

bool CASW_Medals::HasPlayerMedal(int iMedal, CASW_Player *pPlayer)
{
	if (!pPlayer)
		return false;

	int iPlayerIndex = pPlayer->entindex() - 1;
	if (iPlayerIndex < 0 || iPlayerIndex >= ASW_MAX_READY_PLAYERS)
		return false;

	// check in the medals awarded so far this mission
	const char	*p = STRING(ASWGameResource()->m_iszPlayerMedals[iPlayerIndex]);
	char		token[128];
	
	p = nexttoken( token, p, ' ' );
	//p = nexttoken( token, p, ' ' );

	while ( Q_strlen( token ) > 0 ) 
	{
		int iHasMedal = atoi(token);
		if (iHasMedal == iMedal)
		{
			Msg("Player already has medal %d, so can't award it\n", iMedal);
			return true;
		}
		if (p)
			p = nexttoken( token, p, ' ' );
		else
			token[0] = '\0';
	}

	Msg("Player doesn't have medal %d, so awarding it\n", iMedal);
	return false;
}
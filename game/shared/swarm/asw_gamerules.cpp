#include "cbase.h"
#include "ammodef.h"

#ifdef CLIENT_DLL
	#include "c_asw_marine.h"
	#include "c_asw_pickup.h"
	#include "c_asw_marine_resource.h"
	#include "c_asw_flare_projectile.h"
	#include "c_asw_weapon.h"
	#include "c_asw_game_resource.h"
	#include "c_asw_player.h"
	#include "asw_shareddefs.h"
	#include "c_asw_campaign_save.h"
	#include "c_asw_ammo.h"
	#include "voice_status.h"
	#define CASW_Weapon C_ASW_Weapon
	#define CAI_BaseNPC C_AI_BaseNPC
	#define CASW_Flare_Projectile C_ASW_Flare_Projectile
	#define CASW_Campaign_Save C_ASW_Campaign_Save
	#define CASW_Ammo C_ASW_Ammo
#else
	#include "asw_marine_resource.h"
	#include "player.h"
	#include "game.h"
	#include "gamerules.h"
	#include "teamplay_gamerules.h"
	#include "asw_player.h"
	#include "voice_gamemgr.h"
	#include "globalstate.h"
	#include "ai_basenpc.h"
	#include "asw_game_resource.h"
		
	#include "asw_marine.h"
	#include "asw_spawner.h"
	#include "asw_pickup.h"
	#include "asw_flare_projectile.h"
	#include "asw_weapon.h"
	#include "asw_ammo.h"
	#include "asw_mission_manager.h"
	#include "asw_marine_speech.h"
	#include "asw_gamestats.h"
	#include "ai_networkmanager.h"
	#include "ai_initutils.h"
	#include "ai_network.h"
	#include "ai_navigator.h"
	#include "ai_node.h"
	#include "asw_campaign_save.h"
	#include "asw_egg.h"
	#include "asw_alien_goo_shared.h"
	#include "asw_parasite.h"
	#include "asw_harvester.h"
	#include "asw_drone_advanced.h"
	#include "asw_shieldbug.h"
	#include "asw_parasite.h"
	#include "asw_medals.h"
	#include "asw_mine.h"
	#include "asw_burning.h"
	#include "asw_triggers.h"
	#include "asw_use_area.h"
	#include "asw_grenade_vindicator.h"
	#include "asw_sentry_top.h"
	#include "asw_sentry_base.h"
	#include "asw_radiation_volume.h"
	#include "missionchooser/iasw_mission_chooser_source.h"
	#include "asw_objective.h"
	#include "asw_debrief_stats.h"
	#include "props.h"
	#include "vgui/ILocalize.h"
	#include "inetchannelinfo.h"
	#include "asw_util_shared.h"
	#include "filesystem.h"
	#include "asw_intro_control.h"
	#include "asw_tutorial_spawning.h"
	#include "asw_equip_req.h"
	#include "asw_map_scores.h"
	#include "world.h"
	#include "asw_bloodhound.h"
	#include "asw_fire.h"
	#include "engine/IEngineSound.h"
	#include "asw_pickup_weapon.h"
	#include "asw_fail_advice.h"
	#include "asw_spawn_manager.h"
	#include "asw_path_utils.h"
	#include "EntityFlame.h"
	#include "asw_buffgrenade_projectile.h"
	#include "asw_achievements.h"
	#include "asw_director.h"
#endif
#include "game_timescale_shared.h"
#include "asw_gamerules.h"
#include "asw_equipment_list.h"
#include "asw_marine_profile.h"
#include "asw_weapon_parse.h"
#include "asw_campaign_info.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "takedamageinfo.h"
#include "asw_holdout_mode.h"
#include "asw_powerup_shared.h"
#include "missionchooser/iasw_mission_chooser.h"
#include "missionchooser/iasw_random_missions.h"
#include "missionchooser/iasw_map_builder.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar old_radius_damage;

#define ASW_LAUNCHING_STEP 0.25f			// time between each stage of launching

#ifndef CLIENT_DLL
	extern ConVar asw_debug_alien_damage;
	extern ConVar asw_medal_lifesaver_dist;
	extern ConVar asw_medal_lifesaver_kills;
	extern ConVar asw_tutorial_save_stage;
	extern ConVar asw_horde;
	extern ConVar asw_director_controls_spawns;
	extern ConVar asw_medal_barrel_kills;
	ConVar asw_objective_slowdown_time( "asw_objective_slowdown_time", "1.8", FCVAR_CHEAT, "Length of time that the slowdown effect lasts." );
	ConVar asw_marine_explosion_protection("asw_marine_explosion_protection", "0.5", FCVAR_CHEAT, "Reduction of explosion radius against marines");
	ConVar asw_door_explosion_boost("asw_door_explosion_boost", "2.0", FCVAR_CHEAT, "Sets damage scale for grenades vs doors");
	ConVar asw_difficulty_alien_health_step("asw_difficulty_alien_health_step", "0.2", 0, "How much alien health is changed per mission difficulty level");
	ConVar asw_difficulty_alien_damage_step("asw_difficulty_alien_damage_step", "0.2", 0, "How much alien damage is changed per mission difficulty level");
	ConVar asw_ww_chatter_interval_min("asw_ww_chatter_interval_min", "200", 0, "Min time between wildcat and wolfe conversation");
	ConVar asw_ww_chatter_interval_max("asw_ww_chatter_interval_max", "260", 0, "Max time between wildcat and wolfe conversation");
	ConVar asw_compliment_chatter_interval_min("asw_compliment_chatter_interval_min", "180", 0, "Min time between kill compliments");
	ConVar asw_compliment_chatter_interval_max("asw_compliment_chatter_interval_max", "240", 0, "Max time between kill compliments");	
	ConVar asw_default_campaign("asw_default_campaign", "jacob", FCVAR_ARCHIVE, "Default campaign used when dedicated server restarts");
	ConVar asw_fair_marine_rules("asw_fair_marine_rules", "1", FCVAR_ARCHIVE, "If set, fair marine selection rules are enforced during the briefing");
	ConVar asw_override_max_marines("asw_override_max_marines", "0", FCVAR_CHEAT, "Overrides how many marines can be selected for (testing).");
	ConVar asw_last_game_variation("asw_last_game_variation", "0", FCVAR_ARCHIVE, "Which game variation was used last game");
	ConVar asw_bonus_charges("asw_bonus_charges", "0", FCVAR_CHEAT, "Bonus ammo given to starting equipment");
	ConVar asw_campaign_wounding("asw_campaign_wounding", "0", FCVAR_NONE, "Whether marines are wounded in the roster if a mission is completed with the marine having taken significant damage");
	ConVar asw_drop_powerups("asw_drop_powerups", "0", FCVAR_CHEAT, "Do aliens drop powerups?");
	ConVar asw_adjust_difficulty_by_number_of_marines( "asw_adjust_difficulty_by_number_of_marines", "1", FCVAR_CHEAT, "If enabled, difficulty will be reduced when there are only 3 or 2 marines." );
	ConVar sv_vote_kick_ban_duration("sv_vote_kick_ban_duration", "5", 0, "How long should a kick vote ban someone from the server? (in minutes)");
	ConVar sv_timeout_when_fully_connected( "sv_timeout_when_fully_connected", "30", FCVAR_NONE, "Once fully connected, player will be kicked if he doesn't send a network message within this interval." );
	ConVar mm_swarm_state( "mm_swarm_state", "ingame", FCVAR_DEVELOPMENTONLY );

	static void UpdateMatchmakingTagsCallback( IConVar *pConVar, const char *pOldValue, float flOldValue )
	{
		// update sv_tags to force an update of the matchmaking tags
		static ConVarRef sv_tags( "sv_tags" );

		if ( sv_tags.IsValid() )
		{
			char buffer[ 1024 ];
			Q_snprintf( buffer, sizeof( buffer ), "%s", sv_tags.GetString() );
			sv_tags.SetValue( buffer );
		}
	}
#else
	extern ConVar asw_controls;
#endif

ConVar asw_vote_duration("asw_vote_duration", "30", FCVAR_REPLICATED, "Time allowed to vote on a map/campaign/saved game change.");
ConVar asw_marine_death_cam("asw_marine_death_cam", "1", FCVAR_CHEAT | FCVAR_REPLICATED, "Use death cam");
ConVar asw_marine_death_cam_time_interp("asw_marine_death_cam_time_interp", "0.5", FCVAR_CHEAT | FCVAR_REPLICATED, "Time to blend into the death cam");
ConVar asw_marine_death_cam_time_interp_out("asw_marine_death_cam_time_interp_out", "0.75", FCVAR_CHEAT | FCVAR_REPLICATED, "Time to blend out of the death cam");
ConVar asw_marine_death_cam_time("asw_marine_death_cam_time", "0.4", FCVAR_CHEAT | FCVAR_REPLICATED, "Time to do the slowdown death cam");
ConVar asw_marine_death_cam_hold("asw_marine_death_cam_time_hold", "1.75", FCVAR_CHEAT | FCVAR_REPLICATED, "Time to hold on the dying marine at time ramps back up");
ConVar asw_marine_death_cam_time_local_hold("asw_marine_death_cam_time_local_hold", "5.0", FCVAR_CHEAT | FCVAR_REPLICATED, "Time to hold on the dying marine at time ramps back up if they died");
ConVar asw_marine_death_cam_time_scale("asw_marine_death_cam_time_scale", "0.035", FCVAR_CHEAT | FCVAR_REPLICATED, "Time scale during death cam");
ConVar asw_campaign_death("asw_campaign_death", "0", FCVAR_REPLICATED, "Whether marines are killed in the roster if a mission is completed with the marine dead");
ConVar asw_objective_update_time_scale("asw_objective_update_time_scale", "0.2", FCVAR_CHEAT | FCVAR_REPLICATED, "Time scale during objective updates");
ConVar asw_stim_time_scale("asw_stim_time_scale", "0.35", FCVAR_REPLICATED | FCVAR_CHEAT, "Time scale during stimpack slomo");
ConVar asw_time_scale_delay("asw_time_scale_delay", "0.15", FCVAR_REPLICATED | FCVAR_CHEAT, "Delay before timescale changes to give a chance for the client to comply and predict.");
ConVar asw_ignore_need_two_player_requirement("asw_ignore_need_two_player_requirement", "0", FCVAR_REPLICATED, "If set to 1, ignores the mission setting that states two players are needed to start the mission.");
ConVar mp_gamemode( "mp_gamemode", "campaign", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Current game mode, acceptable values are campaign and single_mission.", false, 0.0f, false, 0.0f );
ConVar mm_max_players( "mm_max_players", "4", FCVAR_REPLICATED | FCVAR_CHEAT, "Max players for matchmaking system" );
ConVar asw_sentry_friendly_fire_scale( "asw_sentry_friendly_fire_scale", "0", FCVAR_REPLICATED, "Damage scale for sentry gun friendly fire"
#ifdef GAME_DLL
	,UpdateMatchmakingTagsCallback );
#else
	 );
#endif
ConVar asw_marine_ff_absorption("asw_marine_ff_absorption", "1", FCVAR_REPLICATED, "Friendly fire absorption style (0=none 1=ramp up 2=ramp down)"
#ifdef GAME_DLL
	,UpdateMatchmakingTagsCallback );
#else
	);
#endif
ConVar asw_horde_override( "asw_horde_override", "0", FCVAR_REPLICATED, "Forces hordes to spawn"
#ifdef GAME_DLL
	  ,UpdateMatchmakingTagsCallback );
#else
	  );
#endif
ConVar asw_wanderer_override( "asw_wanderer_override", "0", FCVAR_REPLICATED, "Forces wanderers to spawn"
#ifdef GAME_DLL
	 ,UpdateMatchmakingTagsCallback );
#else
	 );
#endif

// ASW Weapons
// Rifle
ConVar	sk_plr_dmg_asw_r			( "sk_plr_dmg_asw_r","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_r			( "sk_npc_dmg_asw_r","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_r				( "sk_max_asw_r","0", FCVAR_REPLICATED);
// Rifle Grenade
ConVar	sk_plr_dmg_asw_r_g			( "sk_plr_dmg_asw_r_g","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_r_g			( "sk_npc_dmg_asw_r_g","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_r_g				( "sk_max_asw_r_g","0", FCVAR_REPLICATED);
// Autogun
ConVar	sk_plr_dmg_asw_ag			( "sk_plr_dmg_asw_ag","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_ag			( "sk_npc_dmg_asw_ag","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_ag				( "sk_max_asw_ag","0", FCVAR_REPLICATED);
// Shotgun
ConVar	sk_plr_dmg_asw_sg			( "sk_plr_dmg_asw_sg","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_sg			( "sk_npc_dmg_asw_sg","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_sg				( "sk_max_asw_sg","0", FCVAR_REPLICATED);
// Assault Shotgun
ConVar	sk_plr_dmg_asw_asg			( "sk_plr_dmg_asw_asg","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_asg			( "sk_npc_dmg_asw_asg","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_asg				( "sk_max_asw_asg","0", FCVAR_REPLICATED);
// Flamer
ConVar	sk_plr_dmg_asw_f			( "sk_plr_dmg_asw_f","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_f			( "sk_npc_dmg_asw_f","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_f				( "sk_max_asw_f","0", FCVAR_REPLICATED);
// Pistol
ConVar	sk_plr_dmg_asw_p			( "sk_plr_dmg_asw_p","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_p			( "sk_npc_dmg_asw_p","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_p				( "sk_max_asw_p","0", FCVAR_REPLICATED);
// Mining laser
ConVar	sk_plr_dmg_asw_ml			( "sk_plr_dmg_asw_ml","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_ml			( "sk_npc_dmg_asw_ml","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_ml				( "sk_max_asw_ml","0", FCVAR_REPLICATED);
// TeslaGun
ConVar	sk_plr_dmg_asw_tg			( "sk_plr_dmg_asw_tg","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_tg			( "sk_npc_dmg_asw_tg","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_tg				( "sk_max_asw_tg","0", FCVAR_REPLICATED);
// Chainsaw
ConVar	sk_plr_dmg_asw_cs			( "sk_plr_dmg_asw_cs","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_cs			( "sk_npc_dmg_asw_cs","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_cs				( "sk_max_asw_cs","0", FCVAR_REPLICATED);
// Rails
ConVar	sk_plr_dmg_asw_rg			( "sk_plr_dmg_asw_rg","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_rg			( "sk_npc_dmg_asw_rg","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_rg				( "sk_max_asw_rg","0", FCVAR_REPLICATED);
// Flares
ConVar	sk_plr_dmg_asw_flares			( "sk_plr_dmg_asw_flares","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_flares			( "sk_npc_dmg_asw_flares","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_flares				( "sk_max_asw_flares","0", FCVAR_REPLICATED);
// Medkit
ConVar	sk_plr_dmg_asw_medkit			( "sk_plr_dmg_asw_medkit","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_medkit			( "sk_npc_dmg_asw_medkit","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_medkit				( "sk_max_asw_medkit","0", FCVAR_REPLICATED);
// Med Satchel
ConVar	sk_plr_dmg_asw_medsat			( "sk_plr_dmg_asw_medsat","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_medsat			( "sk_npc_dmg_asw_medsat","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_medsat				( "sk_max_asw_medsat","0", FCVAR_REPLICATED);
// Med Satchel self heal secondary fire
ConVar	sk_plr_dmg_asw_medself			( "sk_plr_dmg_asw_medself","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_medself			( "sk_npc_dmg_asw_medself","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_medself				( "sk_max_asw_medself","0", FCVAR_REPLICATED);
// Med Stim
ConVar	sk_plr_dmg_asw_stim			( "sk_plr_dmg_asw_stim","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_stim			( "sk_npc_dmg_asw_stim","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_stim				( "sk_max_asw_stim","0", FCVAR_REPLICATED);
// Welder
ConVar	sk_plr_dmg_asw_welder			( "sk_plr_dmg_asw_welder","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_welder		( "sk_npc_dmg_asw_welder","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_welder				( "sk_max_asw_welder","0", FCVAR_REPLICATED);
// Extinguisher
ConVar	sk_plr_dmg_asw_ext			( "sk_plr_dmg_asw_ext","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_ext			( "sk_npc_dmg_asw_ext","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_ext				( "sk_max_asw_ext","0", FCVAR_REPLICATED);
// Mines
ConVar	sk_plr_dmg_asw_mines			( "sk_plr_dmg_asw_mines","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_mines			( "sk_npc_dmg_asw_mines","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_mines				( "sk_max_asw_mines","0", FCVAR_REPLICATED);
// PDW
ConVar	sk_plr_dmg_asw_pdw			( "sk_plr_dmg_asw_pdw","0", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_asw_pdw			( "sk_npc_dmg_asw_pdw","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_pdw				( "sk_max_asw_pdw","0", FCVAR_REPLICATED);
// Hand Grenades
ConVar	sk_npc_dmg_asw_hg			( "sk_npc_dmg_asw_hg","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_hg				( "sk_max_asw_hg","0", FCVAR_REPLICATED);
// Grenade launcher
ConVar	sk_npc_dmg_asw_gl			( "sk_npc_dmg_asw_gl","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_gl				( "sk_max_asw_gl","0", FCVAR_REPLICATED);
// Sniper Rifle
ConVar	sk_npc_dmg_asw_sniper			( "sk_npc_dmg_asw_sniper","0", FCVAR_REPLICATED);
ConVar	sk_max_asw_sniper				( "sk_max_asw_sniper","0", FCVAR_REPLICATED);


ConVar asw_flare_autoaim_radius("asw_flare_autoaim_radius", "250", FCVAR_REPLICATED | FCVAR_CHEAT, "Radius of autoaim effect from flares");
ConVar asw_vote_kick_fraction("asw_vote_kick_fraction", "0.6", FCVAR_REPLICATED | FCVAR_ARCHIVE, "Fraction of players needed to activate a kick vote");
ConVar asw_vote_leader_fraction("asw_vote_leader_fraction", "0.6", FCVAR_REPLICATED | FCVAR_ARCHIVE, "Fraction of players needed to activate a leader vote");
ConVar asw_vote_map_fraction("asw_vote_map_fraction", "0.6", FCVAR_REPLICATED | FCVAR_ARCHIVE, "Fraction of players needed to activate a map vote");
ConVar asw_marine_collision("asw_marine_collision", "0", FCVAR_REPLICATED, "Whether marines collide with each other or not, in a multiplayer game");
ConVar asw_skill( "asw_skill","2", FCVAR_REPLICATED | FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX, "Game skill level (1-5).", true, 1, true, 5 );
ConVar asw_money( "asw_money", "0", FCVAR_REPLICATED, "Can players collect money?" );
ConVar asw_client_build_maps("asw_client_build_maps", "0", FCVAR_REPLICATED, "Whether clients compile random maps rather than getting sent them");

REGISTER_GAMERULES_CLASS( CAlienSwarm );

BEGIN_NETWORK_TABLE_NOBASE( CAlienSwarm, DT_ASWGameRules )
	#ifdef CLIENT_DLL
		RecvPropInt(RECVINFO(m_iGameState)),
		RecvPropInt(RECVINFO(m_iSpecialMode)),
		RecvPropBool(RECVINFO(m_bMissionSuccess)),
		RecvPropBool(RECVINFO(m_bMissionFailed)),
		RecvPropInt(RECVINFO(m_nFailAdvice)),
		RecvPropInt(RECVINFO(m_iMissionDifficulty) ),
		RecvPropInt(RECVINFO(m_iSkillLevel) ),
		RecvPropInt(RECVINFO(m_iCurrentVoteYes) ),
		RecvPropInt(RECVINFO(m_iCurrentVoteNo) ),
		RecvPropInt(RECVINFO(m_iCurrentVoteType) ),
		RecvPropString(RECVINFO(m_szCurrentVoteDescription) ),
		RecvPropString(RECVINFO(m_szCurrentVoteMapName) ),
		RecvPropString(RECVINFO(m_szCurrentVoteCampaignName) ),
		RecvPropFloat(RECVINFO(m_fVoteEndTime)),
		RecvPropFloat(RECVINFO(m_fBriefingStartedTime) ),
		RecvPropBool(RECVINFO(m_bMissionRequiresTech)),
		RecvPropBool(RECVINFO(m_bCheated)),
		RecvPropInt(RECVINFO(m_iUnlockedModes)),
		RecvPropEHandle(RECVINFO(m_hStartStimPlayer)),
		RecvPropFloat(RECVINFO(m_flStimEndTime)),
		RecvPropFloat(RECVINFO(m_flStimStartTime)),
		RecvPropFloat(RECVINFO(m_flRestartingMissionTime)),
		RecvPropFloat(RECVINFO(m_fPreventStimMusicTime)),
		RecvPropBool(RECVINFO(m_bForceStylinCam)),
		RecvPropBool(RECVINFO(m_bShowCommanderFace)),
		RecvPropFloat(RECVINFO(m_fMarineDeathTime)),
		RecvPropVector(RECVINFO(m_vMarineDeathPos)),
		RecvPropInt(RECVINFO(m_nMarineForDeathCam)),
	#else
		SendPropInt(SENDINFO(m_iGameState), 8, SPROP_UNSIGNED ),
		SendPropInt(SENDINFO(m_iSpecialMode), 3, SPROP_UNSIGNED),
		SendPropBool(SENDINFO(m_bMissionSuccess)),
		SendPropBool(SENDINFO(m_bMissionFailed)),
		SendPropInt(SENDINFO(m_nFailAdvice)),
		SendPropInt(SENDINFO(m_iMissionDifficulty) ),
		SendPropInt(SENDINFO(m_iSkillLevel) ),
		SendPropInt(SENDINFO(m_iCurrentVoteYes) ),
		SendPropInt(SENDINFO(m_iCurrentVoteNo) ),
		SendPropInt(SENDINFO(m_iCurrentVoteType) ),
		SendPropString(SENDINFO(m_szCurrentVoteDescription) ),
		SendPropString(SENDINFO(m_szCurrentVoteMapName) ),
		SendPropString(SENDINFO(m_szCurrentVoteCampaignName) ),
		SendPropFloat(SENDINFO(m_fVoteEndTime) ),
		SendPropFloat(SENDINFO(m_fBriefingStartedTime) ),
		SendPropBool(SENDINFO(m_bMissionRequiresTech)),
		SendPropBool(SENDINFO(m_bCheated)),
		SendPropInt(SENDINFO(m_iUnlockedModes), 4, SPROP_UNSIGNED ),
		SendPropEHandle(SENDINFO(m_hStartStimPlayer)),
		SendPropFloat(SENDINFO(m_flStimEndTime), 0, SPROP_NOSCALE),
		SendPropFloat(SENDINFO(m_flStimStartTime), 0, SPROP_NOSCALE),
		SendPropFloat(SENDINFO(m_flRestartingMissionTime), 0, SPROP_NOSCALE),
		SendPropFloat(SENDINFO(m_fPreventStimMusicTime), 0, SPROP_NOSCALE),
		SendPropBool(SENDINFO(m_bForceStylinCam)),
		SendPropBool(SENDINFO(m_bShowCommanderFace)),
		SendPropFloat(SENDINFO(m_fMarineDeathTime), 0, SPROP_NOSCALE),
		SendPropVector(SENDINFO(m_vMarineDeathPos)),
		SendPropInt(SENDINFO(m_nMarineForDeathCam), 8),
	#endif
END_NETWORK_TABLE()


BEGIN_DATADESC( CAlienSwarmProxy )
	DEFINE_KEYFIELD( m_iSpeedrunTime, FIELD_INTEGER, "speedruntime" ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( asw_gamerules, CAlienSwarmProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( AlienSwarmProxy, DT_AlienSwarmProxy )

#ifdef CLIENT_DLL
	void CAlienSwarmProxy::OnDataChanged( DataUpdateType_t updateType )
	{
		BaseClass::OnDataChanged( updateType );
		if ( ASWGameRules() )
		{
			ASWGameRules()->OnDataChanged( updateType );
		}
	}

	void RecvProxy_ASWGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CAlienSwarm *pRules = ASWGameRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CAlienSwarmProxy, DT_AlienSwarmProxy )
		RecvPropDataTable( "asw_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_ASWGameRules ), RecvProxy_ASWGameRules )		
	END_RECV_TABLE()
#else
	void* SendProxy_ASWGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CAlienSwarm *pRules = ASWGameRules();
		Assert( pRules );
		pRecipients->SetAllRecipients();
		return pRules;
	}

	BEGIN_SEND_TABLE( CAlienSwarmProxy, DT_AlienSwarmProxy )
		SendPropDataTable( "asw_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_ASWGameRules ), SendProxy_ASWGameRules )
	END_SEND_TABLE()
#endif


#ifndef CLIENT_DLL
class CStylinCamProxy : public CBaseEntity
{
public:
	DECLARE_CLASS( CStylinCamProxy, CBaseEntity );
	DECLARE_DATADESC();

	void InputShowStylinCam( inputdata_t &inputdata );
	void InputHideStylinCam( inputdata_t &inputdata );
	void InputShowCommanderFace( inputdata_t &inputdata );
	void InputHideCommanderFace( inputdata_t &inputdata );
};

LINK_ENTITY_TO_CLASS( asw_stylincam, CStylinCamProxy );

BEGIN_DATADESC( CStylinCamProxy )
	DEFINE_INPUTFUNC( FIELD_VOID, "ShowStylinCam", InputShowStylinCam ),
	DEFINE_INPUTFUNC( FIELD_VOID, "HideStylinCam", InputHideStylinCam ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ShowCommanderFace", InputShowCommanderFace ),
	DEFINE_INPUTFUNC( FIELD_VOID, "HideCommanderFace", InputHideCommanderFace ),
END_DATADESC()

void CStylinCamProxy::InputShowStylinCam( inputdata_t &inputdata )
{
	if ( !ASWGameRules() )
		return;

	ASWGameRules()->m_bForceStylinCam = true;
}

void CStylinCamProxy::InputHideStylinCam( inputdata_t &inputdata )
{
	if ( !ASWGameRules() )
		return;

	ASWGameRules()->m_bForceStylinCam = false;
}

void CStylinCamProxy::InputShowCommanderFace( inputdata_t &inputdata )
{
	if ( !ASWGameRules() )
		return;

	ASWGameRules()->m_bShowCommanderFace = true;
}

void CStylinCamProxy::InputHideCommanderFace( inputdata_t &inputdata )
{
	if ( !ASWGameRules() )
		return;

	ASWGameRules()->m_bShowCommanderFace = false;
}
#endif


// shared ammo definition
// JAY: Trying to make a more physical bullet response
#define BULLET_MASS_GRAINS_TO_LB(grains)	(0.002285*(grains)/16.0f)
#define BULLET_MASS_GRAINS_TO_KG(grains)	lbs2kg(BULLET_MASS_GRAINS_TO_LB(grains))

// exaggerate all of the forces, but use real numbers to keep them consistent
#define BULLET_IMPULSE_EXAGGERATION			3.5
// convert a velocity in ft/sec and a mass in grains to an impulse in kg in/s
#define BULLET_IMPULSE(grains, ftpersec)	((ftpersec)*12*BULLET_MASS_GRAINS_TO_KG(grains)*BULLET_IMPULSE_EXAGGERATION)

CAmmoDef *GetAmmoDef()
{
	static CAmmoDef def;
	static bool bInitted = false;

	if ( !bInitted )
	{
		bInitted = true;

		def.AddAmmoType("AR2",				DMG_BULLET,					TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_r",			"sk_npc_dmg_asw_r",			"sk_max_asw_r",			BULLET_IMPULSE(200, 1225), 0 );
		// asw ammo
		//				name				damagetype					tracertype				player dmg					npc damage					carry					physics force impulse		flags
		// rifle  DMG_BULLET
		def.AddAmmoType("ASW_R",			DMG_BULLET,					TRACER_LINE,	"sk_plr_dmg_asw_r",			"sk_npc_dmg_asw_r",			"sk_max_asw_r",			BULLET_IMPULSE(200, 1225),	0 );
		// rifle grenades
		def.AddAmmoType("ASW_R_G",			DMG_BURN,					TRACER_NONE,	"sk_plr_dmg_asw_r_g",			"sk_npc_dmg_asw_r_g",			"sk_max_asw_r_g",			0,	0 );
		// autogun
		def.AddAmmoType("ASW_AG",			DMG_BULLET,					TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_ag",		"sk_npc_dmg_asw_ag",		"sk_max_asw_ag",		BULLET_IMPULSE(200, 1225),	0 );
		// shotgun
		def.AddAmmoType("ASW_SG",			DMG_BULLET | DMG_BUCKSHOT,	TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_sg",		"sk_npc_dmg_asw_sg",		"sk_max_asw_sg",		BULLET_IMPULSE(200, 1225),	0 );
		// assault shotgun
		def.AddAmmoType("ASW_ASG",			DMG_BULLET | DMG_BUCKSHOT,	TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_asg",		"sk_npc_dmg_asw_asg",		"sk_max_asw_asg",		BULLET_IMPULSE(200, 1225),	0 );
		// flamer
		def.AddAmmoType("ASW_F",			DMG_BURN,					TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_f",			"sk_npc_dmg_asw_f",			"sk_max_asw_f",			BULLET_IMPULSE(200, 1225),	0 );
		// pistol
		def.AddAmmoType("ASW_P",			DMG_BULLET,					TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_p",			"sk_npc_dmg_asw_p",			"sk_max_asw_p",			BULLET_IMPULSE(200, 1225),	0 );
		// mining laser
		def.AddAmmoType("ASW_ML",			DMG_ENERGYBEAM,				TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_ml",		"sk_npc_dmg_asw_ml",		"sk_max_asw_ml",		BULLET_IMPULSE(200, 1225),	0 );
		// tesla gun - happy LJ?
		def.AddAmmoType("ASW_TG",			DMG_ENERGYBEAM,				TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_tg",		"sk_npc_dmg_asw_tg",		"sk_max_asw_tg",		BULLET_IMPULSE(200, 1225),	0 );
		// railgun
		def.AddAmmoType("ASW_RG",			DMG_SONIC,					TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_rg",		"sk_npc_dmg_asw_rg",		"sk_max_asw_rg",		BULLET_IMPULSE(200, 1225),	0 );
		// chainsaw
		def.AddAmmoType("ASW_CS",			DMG_SLASH,					TRACER_NONE,			"sk_plr_dmg_asw_cs",		"sk_npc_dmg_asw_cs",		"sk_max_asw_cs",		BULLET_IMPULSE(200, 1225),	0 );
		// flares
		def.AddAmmoType("ASW_FLARES",		DMG_SONIC,					TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_flares",	"sk_npc_dmg_asw_flares",	"sk_max_asw_flares",	BULLET_IMPULSE(200, 1225),	0 );
		// medkit
		def.AddAmmoType("ASW_MEDKIT",		DMG_SONIC,					TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_medkit",	"sk_npc_dmg_asw_medkit",	"sk_max_asw_medkit",	BULLET_IMPULSE(200, 1225),	0 );
		// med satchel
		def.AddAmmoType("ASW_MEDSAT",		DMG_SONIC,					TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_medsat",	"sk_npc_dmg_asw_medsat",	"sk_max_asw_medsat",	BULLET_IMPULSE(200, 1225),	0 );
		// med satchel self heal
		def.AddAmmoType("ASW_MEDSELF",		DMG_SONIC,					TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_medself",	"sk_npc_dmg_asw_medself",	"sk_max_asw_medself",	BULLET_IMPULSE(200, 1225),	0 );
		// stim
		def.AddAmmoType("ASW_STIM",		DMG_SONIC,						TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_stim",	"sk_npc_dmg_asw_stim",	"sk_max_asw_stim",	BULLET_IMPULSE(200, 1225),	0 );
		// welder
		def.AddAmmoType("ASW_WELDER",		DMG_SONIC,					TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_welder",	"sk_npc_dmg_asw_welder",	"sk_max_asw_welder",	BULLET_IMPULSE(200, 1225),	0 );
		// fire extinguisher
		def.AddAmmoType("ASW_EXT",			DMG_SONIC,					TRACER_NONE,	"sk_plr_dmg_asw_ext",			"sk_npc_dmg_asw_ext",			"sk_max_asw_ext",		BULLET_IMPULSE(200, 1225),	0 );
		// mines
		def.AddAmmoType("ASW_MINES",		DMG_BURN,					TRACER_NONE,	"sk_plr_dmg_asw_mines",			"sk_npc_dmg_asw_mines",			"sk_max_asw_mines",		BULLET_IMPULSE(200, 1225),	0 );
		// vindicator grenades
		def.AddAmmoType("ASW_ASG_G",		DMG_BURN,					TRACER_NONE,	"sk_plr_dmg_asw_r_g",			"sk_npc_dmg_asw_r_g",			"sk_max_asw_r_g",			0,	0 );
		// PDW
		def.AddAmmoType("ASW_PDW",			DMG_BULLET,					TRACER_LINE_AND_WHIZ,	"sk_plr_dmg_asw_pdw",			"sk_npc_dmg_asw_pdw",			"sk_max_asw_pdw",		BULLET_IMPULSE(200, 1225),	0 );
		// Hand Grenades
		def.AddAmmoType("ASW_HG",			DMG_BURN,					TRACER_NONE,	"sk_npc_dmg_asw_hg",			"sk_npc_dmg_asw_hg",			"sk_max_asw_hg",		BULLET_IMPULSE(200, 1225),	0 );
		// Grenade launcher
		def.AddAmmoType("ASW_GL",			DMG_BURN,					TRACER_NONE,	"sk_npc_dmg_asw_gl",			"sk_npc_dmg_asw_gl",			"sk_max_asw_gl",		BULLET_IMPULSE(200, 1225),	0 );
		// Sniper Rifle
		def.AddAmmoType("ASW_SNIPER",		DMG_BULLET,					TRACER_LINE_AND_WHIZ,	"sk_npc_dmg_asw_sniper",		"sk_npc_dmg_asw_sniper",			"sk_max_asw_sniper",		BULLET_IMPULSE(200, 1225),	0 );
	}

	return &def;
}


#ifdef CLIENT_DLL

CAlienSwarm::CAlienSwarm()
{
	Msg("C_AlienSwarm created\n");

	if (ASWEquipmentList())
		ASWEquipmentList()->LoadTextures();

	m_nOldMarineForDeathCam = -1;
	m_fMarineDeathCamRealtime = 0.0f;
	m_nMarineDeathCamStep = 0;
	m_hMarineDeathRagdoll = NULL;
	m_fDeathCamYawAngleOffset = 0.0f;
	m_iPreviousGameState = 200;

	engine->SetPitchScale( 1.0f );

	CVoiceStatus *pVoiceMgr = GetClientVoiceMgr();
	if ( pVoiceMgr )
	{
		pVoiceMgr->SetHeadLabelsDisabled( true );
	}
}

CAlienSwarm::~CAlienSwarm()
{	
	Msg("C_AlienSwarm deleted\n");

	GameTimescale()->SetDesiredTimescale( 1.0f );
	engine->SetPitchScale( 1.0f );
}

float CAlienSwarm::GetMarineDeathCamInterp( void )
{
	if ( !asw_marine_death_cam.GetBool() || m_nMarineForDeathCam == -1 || m_fMarineDeathTime <= 0.0f )
		return 0.0f;

	bool bNewStep = false;

	if ( m_nOldMarineForDeathCam != m_nMarineForDeathCam )
	{
		m_nOldMarineForDeathCam = m_nMarineForDeathCam;
		m_hMarineDeathRagdoll = NULL;
		m_nMarineDeathCamStep = 0;
	}

	if ( m_nMarineDeathCamStep == 0 )
	{
		if ( gpGlobals->curtime > m_fMarineDeathTime + asw_time_scale_delay.GetFloat() )
		{
			m_nMarineDeathCamStep++;
			bNewStep = true;
		}
	}
	else if ( m_nMarineDeathCamStep == 1 )
	{
		if ( gpGlobals->curtime > m_fMarineDeathTime + asw_time_scale_delay.GetFloat() + asw_marine_death_cam_time.GetFloat() )
		{
			m_nMarineDeathCamStep++;
			bNewStep = true;
		}
	}

	if ( bNewStep )
	{
		m_fMarineDeathCamRealtime = gpGlobals->realtime;
	}

	C_ASW_Marine *pLocalMarine = C_ASW_Marine::GetLocalMarine();
	bool bLocal = ( !pLocalMarine || !pLocalMarine->IsAlive() );

	float flHoldTime = ( bLocal ? asw_marine_death_cam_time_local_hold.GetFloat() : asw_marine_death_cam_hold.GetFloat() );

	if ( m_nMarineDeathCamStep == 1 )
	{
		return clamp( ( gpGlobals->realtime - m_fMarineDeathCamRealtime ) / asw_marine_death_cam_time_interp.GetFloat(), 0.001f, 1.0f );
	}
	else if ( m_nMarineDeathCamStep == 2 )
	{
		if ( gpGlobals->realtime > m_fMarineDeathCamRealtime + flHoldTime + asw_marine_death_cam_time_interp_out.GetFloat() )
		{
			m_nMarineDeathCamStep = 3;
		}

		return clamp( 1.0f - ( ( gpGlobals->realtime - ( m_fMarineDeathCamRealtime + flHoldTime ) ) / asw_marine_death_cam_time_interp_out.GetFloat() ), 0.001f, 1.0f );
	}
	else if ( m_nMarineDeathCamStep == 3 )
	{
		return 0.0f;
	}

	return 0.0f;
}

void CAlienSwarm::OnDataChanged( DataUpdateType_t updateType )
{
	if ( m_iPreviousGameState != GetGameState() )
	{
		m_iPreviousGameState = GetGameState();

		IGameEvent * event = gameeventmanager->CreateEvent( "swarm_state_changed" );
		if ( event )
		{
			event->SetInt( "state", m_iPreviousGameState );
			gameeventmanager->FireEventClientSide( event );
		}
	}
}

#else

extern ConVar asw_springcol;
ConVar asw_blip_speech_chance("asw_blip_speech_chance", "0.8", FCVAR_CHEAT, "Chance the tech marines will shout about movement on their scanner after a period of no activity");
ConVar asw_instant_restart("asw_instant_restart", "0", 0, "Whether the game should use the instant restart (if not, it'll do a full reload of the map).");

const char * GenerateNewSaveGameName()
{
	static char szNewSaveName[256];	
	// count up save names until we find one that doesn't exist
	for (int i=1;i<10000;i++)
	{
		Q_snprintf(szNewSaveName, sizeof(szNewSaveName), "save/save%d.campaignsave", i);
		if (!filesystem->FileExists(szNewSaveName))
		{
			Q_snprintf(szNewSaveName, sizeof(szNewSaveName), "save%d.campaignsave", i);
			return szNewSaveName;
		}
	}

	return NULL;
}

CAlienSwarm::CAlienSwarm()
{
	Msg("CAlienSwarm created\n");

	// create the profile list for the server
	//  clients do this is in c_asw_player.cpp
	MarineProfileList();

	ASWEquipmentList();

	m_fObjectiveSlowDownEndTime = 0.0f;
	m_bStartedIntro = 0;
	m_iNumGrubs = 0;	// counts how many grubs have been spawned
	m_fVoteEndTime = 0.0f;
	m_flStimEndTime = 0.0f;
	m_flStimStartTime = 0.0f;
	m_fPreventStimMusicTime = 0.0f;
	m_bForceStylinCam = false;

	m_fMarineDeathTime = 0.0f;
	m_vMarineDeathPos = Vector( 0.0f, 0.0f, 0.0f );
	m_nMarineForDeathCam = -1;

	m_bMarineInvuln = false;

	SetGameState( ASW_GS_NONE );
	m_iSpecialMode = 0;
	m_bMissionSuccess = false;
	m_bMissionFailed = false;
	m_fReserveMarinesEndTime = 0;

	m_nFailAdvice = ASW_FAIL_ADVICE_DEFAULT;

	m_fNextChatterTime = 0.0f;
	m_fNextIncomingChatterTime = 0.0f;
	m_fLastNoAmmoChatterTime = 0;
	m_fLastFireTime = 0.0f;
	m_fNextWWKillConv = random->RandomInt(asw_ww_chatter_interval_min.GetInt(), asw_ww_chatter_interval_max.GetInt());
	m_fNextCompliment = random->RandomInt(asw_compliment_chatter_interval_min.GetInt(), asw_compliment_chatter_interval_max.GetInt());
	m_bSargeAndJaeger = false;
	m_bWolfeAndWildcat = false;
	
	// set which entities should stay around when we restart the mission
	m_MapResetFilter.AddKeepEntity("worldspawn");
	m_MapResetFilter.AddKeepEntity("soundent");
	m_MapResetFilter.AddKeepEntity("asw_gamerules");
	m_MapResetFilter.AddKeepEntity("player");
	m_MapResetFilter.AddKeepEntity("asw_player");
	m_MapResetFilter.AddKeepEntity("player_manager");	
	m_MapResetFilter.AddKeepEntity("predicted_viewmodel");
	m_MapResetFilter.AddKeepEntity("sdk_team_manager");
	m_MapResetFilter.AddKeepEntity("scene_manager");
	m_MapResetFilter.AddKeepEntity("event_queue_saveload_proxy");
	m_MapResetFilter.AddKeepEntity("ai_network");

	m_iMissionRestartCount = 0;
	m_bDoneCrashShieldbugConv = false;
	m_bShouldStartMission = false;

	m_fLastBlipSpeechTime = -200.0f;

	m_iSkillLevel = asw_skill.GetInt();
	OnSkillLevelChanged( m_iSkillLevel );

	//m_pMissionManager = new CASW_Mission_Manager();	
	m_pMissionManager = (CASW_Mission_Manager*) CreateEntityByName( "asw_mission_manager" );
	DispatchSpawn( m_pMissionManager );

	m_fVoteEndTime = 0;
	Q_snprintf(m_szCurrentVoteDescription.GetForModify(), 128, "");
	Q_snprintf(m_szCurrentVoteMapName.GetForModify(), 128, "");
	Q_snprintf(m_szCurrentVoteCampaignName.GetForModify(), 128, "");
	
	m_szCurrentVoteName[0] = '\0';
	m_iCurrentVoteYes = 0;
	m_iCurrentVoteNo = 0;
	m_iCurrentVoteType = (int) ASW_VOTE_NONE;	

	m_hDebriefStats = NULL;

	m_fMissionStartedTime = 0;
	m_fBriefingStartedTime = 0;

	m_iForceReadyType = ASW_FR_NONE;
	m_fForceReadyTime = 0;
	m_iForceReadyCount = 0;
	m_fLaunchOutroMapTime = 0;
	m_bMissionRequiresTech = false;
	m_hEquipReq = NULL;
	m_fRemoveAliensTime = 0;

	m_fNextLaunchingStep = 0;
	m_iMarinesSpawned = 0;
	m_pSpawningSpot = NULL;

	m_bIsIntro = false;
	m_bIsOutro = false;
	m_bIsTutorial = false;

	m_bCheckAllPlayersLeft = false;
	m_fEmptyServerTime = false;

	m_pMapBuilder = NULL;

	m_fLastPowerupDropTime = 0;
	m_flTechFailureRestartTime = 0.0f;
}

CAlienSwarm::~CAlienSwarm()
{
	Msg("CAlienSwarm destroyed\n");
	//if (g_pMarineProfileList)
	//{
		//delete g_pMarineProfileList;
		//g_pMarineProfileList = NULL;
	//}
	//if (g_pASWEquipmentList)
	//{
		//delete g_pASWEquipmentList;
		//g_pASWEquipmentList = NULL;
	//}

	//delete m_pMissionManager;
}

ConVar asw_reserve_marine_time("asw_reserve_marine_time", "30.0f", 0, "Number of seconds marines are reserved for at briefing start");

void CAlienSwarm::Precache( void )
{
	UTIL_PrecacheOther( "asw_marine" );

	PrecacheEffect( "ASWSpeech" );
	PrecacheEffect( "ASWBloodImpact" );
	PrecacheEffect( "DroneGib" );
	PrecacheEffect( "ParasiteGib" );
	PrecacheEffect( "ASWWelderSparks" );
	PrecacheEffect( "DroneBleed" );
	PrecacheEffect( "DroneGib" );
	PrecacheEffect( "HarvesterGib" );
	PrecacheEffect( "GrubGib" );
	PrecacheEffect( "ParasiteGib" );
	PrecacheEffect( "HarvesiteGib" );
	PrecacheEffect( "QueenSpitBurst" );
	PrecacheEffect( "EggGibs" );
	PrecacheEffect( "ElectroStun" );
	PrecacheEffect( "PierceSpark" );
	PrecacheEffect( "ExtinguisherCloud" );
	PrecacheEffect( "ASWTracer" );
	PrecacheEffect( "ASWUTracer" );
	PrecacheEffect( "ASWUTracerRG" );
	PrecacheEffect( "ASWUTraceless" );
	PrecacheEffect( "ASWUTracerUnattached" );
	PrecacheEffect( "aswwatersplash" );
	PrecacheEffect( "aswstunexplo" );
	PrecacheEffect( "ASWExplodeMap" );
	PrecacheEffect( "ASWAcidBurn" );
	PrecacheEffect( "ASWFireBurst" );
	PrecacheEffect( "aswcorpse" );
	PrecacheEffect( "QueenDie" );
	PrecacheEffect( "ASWWheelDust" );
	PrecacheEffect( "RailgunBeam" );
	PrecacheEffect( "ASWUTracerDual" );
	PrecacheEffect( "ASWUTracerDualLeft" );
	PrecacheEffect( "ASWUTracerDualRight" );

	BaseClass::Precache();
}

// spawns the marines needed for the tutorial and starts the mission
void CAlienSwarm::StartTutorial(CASW_Player *pPlayer)
{
	if (!ASWGameResource() || !pPlayer)
		return;

	// select Sarge and assign him to the player
	RosterSelect(pPlayer, 6);
	CASW_Marine_Resource* pMR = ASWGameResource()->GetMarineResource(0);
	if ( pMR )
	{
		pMR->m_iWeaponsInSlots.Set( 0, ASWEquipmentList()->GetIndexForSlot( 0, "asw_weapon_rifle" ) );

		for ( int iWpnSlot = 1; iWpnSlot < ASW_MAX_EQUIP_SLOTS; ++ iWpnSlot )
		{
			pMR->m_iWeaponsInSlots.Set( iWpnSlot, -1 );
		}
	}

	int nTutorialSaveStage = CASW_TutorialStartPoint::GetTutorialSaveStage();

	// select crash as the first marine you meet
	if ( nTutorialSaveStage >= 1 )		// if the player has already discovered Crash on a previous tutorial attempt, give him crash again, since they'll spawn further up the map
	{
		// make sure Sarge has a flamer if he restarts the tutorial past the point where you get a flamer
		pMR->m_iWeaponsInSlots.Set( 1, ASWEquipmentList()->GetIndexForSlot( 1, "asw_weapon_flamer" ) );

		// make sure Sarge has a welder if he restarts the tutorial past the point where you get a welder
		pMR->m_iWeaponsInSlots.Set( 2, ASWEquipmentList()->GetIndexForSlot( 2, "asw_weapon_welder" ) );

		if ( ASWGameResource()->GetObjective(0) )
		{
			ASWGameResource()->GetObjective(0)->SetComplete(true);
		}

		RosterSelect(pPlayer, 0);
	}
	else
	{
		RosterSelect(NULL, 0);
	}

	pMR = ASWGameResource()->GetMarineResource(1);
	if (pMR)
	{
		pMR->m_iWeaponsInSlots.Set( 0, ASWEquipmentList()->GetIndexForSlot( 0, "asw_weapon_vindicator" ) );
		pMR->m_iWeaponsInSlots.Set( 1, ASWEquipmentList()->GetIndexForSlot( 1, "asw_weapon_sentry" ) );

		for ( int iWpnSlot = 2; iWpnSlot < ASW_MAX_EQUIP_SLOTS; ++ iWpnSlot )
		{
			pMR->m_iWeaponsInSlots.Set( iWpnSlot, -1 );
		}
	}

	// select bastille as your third marine
	if ( nTutorialSaveStage >= 2 )		// if the player has already discovered Bastille on a previous tutorial attempt, give him crash again, since they'll spawn further up the map
	{
		RosterSelect(pPlayer, 5);
		if ( ASWGameResource()->GetObjective(1) )
		{
			ASWGameResource()->GetObjective(1)->SetComplete(true);
		}
	}
	else
	{
		RosterSelect(NULL, 5);
	}

	pMR = ASWGameResource()->GetMarineResource(2);
	if (pMR)
	{
		pMR->m_iWeaponsInSlots.Set( 0, ASWEquipmentList()->GetIndexForSlot( 0, "asw_weapon_pdw" ) );
		pMR->m_iWeaponsInSlots.Set( 1, ASWEquipmentList()->GetIndexForSlot( 1, "asw_weapon_heal_grenade" ) );

		for ( int iWpnSlot = 2; iWpnSlot < ASW_MAX_EQUIP_SLOTS; ++ iWpnSlot )
		{
			pMR->m_iWeaponsInSlots.Set( iWpnSlot, -1 );
		}
	}
		
	StartMission();	// spawns marines and causes game state to go into ASW_GS_INGAME	
}

void CAlienSwarm::ReserveMarines()
{
	// flag marines as reserved if a commander was using them last mission
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource || !GetCampaignSave())
		return;

	// don't do reserving in singleplayer
	if ( ASWGameResource() && ASWGameResource()->IsOfflineGame() )
		return;

	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{

		// if no-one was using this marine, skip it
		if ( ( Q_strlen( STRING( GetCampaignSave()->m_LastCommanders[i] ) ) <= 1 )
			|| !GetCampaignSave()->IsMarineAlive(i) )
			continue;
		Msg("reserving marine %d for %s\n", i, STRING(GetCampaignSave()->m_LastCommanders[i]));

		// someone was using it, so flag the marine as reserved
		if ( !pGameResource->IsRosterSelected( i ) )
			pGameResource->SetRosterSelected( i, 2 );
	}

	m_fReserveMarinesEndTime = gpGlobals->curtime + asw_reserve_marine_time.GetFloat();
}

void CAlienSwarm::UnreserveMarines()
{
	// flag marines as reserved if a commander was using them last mission
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{
		// undo reserving of this marine
		if (pGameResource->IsRosterReserved(i))
			pGameResource->SetRosterSelected(i, 0);		
	}

	m_fReserveMarinesEndTime = 0;
}

void CAlienSwarm::AutoselectMarines(CASW_Player *pPlayer)
{
	if (!ASWGameResource() || !GetCampaignSave() || !pPlayer)
		return;

	char buffer[256];
	Q_snprintf(buffer, sizeof(buffer), "%s%s", pPlayer->GetPlayerName(), pPlayer->GetASWNetworkID());
	//Msg("checking autoselect for: %s\n", buffer);
	for ( int m = 0; m < ASWGameResource()->GetMaxMarineResources(); m++ )
	{
		for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
		{
			if (!ASWGameResource()->IsRosterSelected(i))
			{
				if ( GetCampaignSave()->m_LastMarineResourceSlot[ i ] != m )
					continue;

				//Msg("checking %d: %s\n", i,STRING(GetCampaignSave()->m_LastCommanders[i]) );
				if (!Q_strcmp(buffer, STRING(GetCampaignSave()->m_LastCommanders[i])))
				{
					//Msg("this is a match, attempting autoselect\n");
					// check marine isn't wounded first
					bool bWounded = false;
					if (IsCampaignGame() && GetCampaignSave())
					{
						if (GetCampaignSave()->IsMarineWounded(i))
							bWounded = true;
					}
					//if (!bWounded)
					RosterSelect(pPlayer, i);
				}
			}
		}
	}
}

void CAlienSwarm::AllowBriefing()
{
	DevMsg( "Cheat allowing return to briefing\n" );
	SetGameState( ASW_GS_BRIEFING );
	mm_swarm_state.SetValue( "briefing" );
}


void CAlienSwarm::PlayerSpawn( CBasePlayer *pPlayer )
{
	BaseClass::PlayerSpawn(pPlayer);

	CASW_Player *pASWPlayer = ToASW_Player( pPlayer );

	// assign leader if there isn't one already
	if (ASWGameResource() && ASWGameResource()->GetLeader() == NULL)
	{
		if ( pASWPlayer )
		{
			ASWGameResource()->SetLeader( pASWPlayer );
		}
	}

	if ( ShouldQuickStart() )
	{
		//SpawnNextMarine();
		//pASWPlayer->SwitchMarine(0 );
		StartTutorial( pASWPlayer );
	}
	else if (IsTutorialMap() || engine->IsCreatingReslist() || engine->IsCreatingXboxReslist())
	{
		StartTutorial(pASWPlayer);
	}
	else
	{
		AutoselectMarines(ToASW_Player(pPlayer));	
	}

	// ask Steam for our XP amounts
	pASWPlayer->RequestExperience();
}

bool CAlienSwarm::ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
{	
	GetVoiceGameMgr()->ClientConnected( pEntity );

	CASW_Player *pPlayer = dynamic_cast<CASW_Player*>(CBaseEntity::Instance( pEntity ));
	//Msg("ClientConnected, entindex is %d\n", pPlayer ? pPlayer->entindex() : -1);

	if (ASWGameResource())
	{
		int index = ENTINDEX(pEntity) - 1;
		if (index >= 0 && index < 8)
		{
			ASWGameResource()->m_bPlayerReady.Set(index, false);
		}

		// if we have no leader
		if (ASWGameResource()->GetLeader() == NULL)
		{
			if (pPlayer)
				ASWGameResource()->SetLeader(pPlayer);
			//else
				//Msg("Failed to cast connected player\n");
		}
	}	

	return BaseClass::ClientConnected(pEntity, pszName, pszAddress, reject, maxrejectlen);
}

void CAlienSwarm::ClientDisconnected( edict_t *pClient )
{
	//Msg("CAlienSwarm::ClientDisconnected %d\n", pClient);
	if ( pClient )
	{
		CASW_Player *pPlayer = dynamic_cast<CASW_Player*>(CBaseEntity::Instance( pClient ) );
		if ( pPlayer )
		{
			if ( ASWGameResource() )
			{
				for ( int i = 0; i < ASWGameResource()->GetMaxMarineResources(); i++ )
				{
					CASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
					if ( !pMR )
						continue;
					
					if ( pMR->GetCommander() == pPlayer )
					{
						pMR->SetInhabited( false );
					}
				}
			}
			//Msg("  This is an ASW_Player %d\n", pPlayer->entindex());
			RosterDeselectAll(pPlayer);

			// if they're leader, pick another leader
			if ( ASWGameResource() && ASWGameResource()->GetLeader() == pPlayer )
			{
				//Msg("  This is a leader disconnecting\n");
				ASWGameResource()->SetLeader(NULL);
				CASW_Game_Resource::s_bLeaderGivenDifficultySuggestion = false;

				int iPlayerEntIndex = pPlayer->entindex();
				
				CASW_Player *pBestPlayer = NULL;
				for (int i=0;i<ASW_MAX_READY_PLAYERS;i++)
				{
					if (i+1 == iPlayerEntIndex)
						continue; 

					// found a connected player?
					CASW_Player *pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i + 1));
					// if they're not connected, skip them
					if (!pOtherPlayer || !pOtherPlayer->IsConnected())
						continue;
					
					if (!pBestPlayer || pBestPlayer->m_bRequestedSpectator)					
						pBestPlayer = pOtherPlayer;
				}
				if (pBestPlayer)
				{
					ASWGameResource()->SetLeader(pBestPlayer);
				}
			}

			SetMaxMarines(pPlayer);

			// reassign marines owned by this player to someone else
			ReassignMarines(pPlayer);

			SetLeaderVote(pPlayer, -1);
			SetKickVote(pPlayer, -1);
			RemoveVote(pPlayer);	// removes map vote

			// remove next campaign map vote
			if (GetCampaignSave())
				GetCampaignSave()->PlayerDisconnected(pPlayer);

			//UTIL_ClientPrintAll(ASW_HUD_PRINTTALKANDCONSOLE, "#asw_player_disco", pPlayer->GetPlayerName());
		}
	}
	BaseClass::ClientDisconnected(pClient);
	m_bCheckAllPlayersLeft = true;
}

bool CAlienSwarm::ShouldTimeoutClient( int nUserID, float flTimeSinceLastReceived )
{
	CASW_Player *pPlayer = static_cast<CASW_Player*>( UTIL_PlayerByUserId( nUserID ) );
	if ( !pPlayer || !pPlayer->IsConnected() || !pPlayer->HasFullyJoined() )
		return false;

	return ( sv_timeout_when_fully_connected.GetFloat() > 0.0f && flTimeSinceLastReceived > sv_timeout_when_fully_connected.GetFloat() );
}

bool CAlienSwarm::RosterSelect( CASW_Player *pPlayer, int RosterIndex, int nPreferredSlot )
{
	if (!ASWGameResource() || GetGameState()!=ASW_GS_BRIEFING)
		return false;
	if (RosterIndex < 0 || RosterIndex >= ASW_NUM_MARINE_PROFILES)
		return false;

	if (ASWGameResource()->m_iNumMarinesSelected >= ASWGameResource()->m_iMaxMarines)		// too many selected?
	{
		if ( nPreferredSlot == -1 )
			return false;
		
		CASW_Marine_Resource *pExisting = ASWGameResource()->GetMarineResource( nPreferredSlot );		// if we're not swapping out for another then abort
		if ( pExisting && pExisting->GetCommander() != pPlayer )
			return false;
	}

	// one marine each?
	if (ASWGameResource()->m_bOneMarineEach && ASWGameResource()->GetNumMarines(pPlayer) > 0)
	{
		if ( nPreferredSlot == -1 )
			return false;

		CASW_Marine_Resource *pExisting = ASWGameResource()->GetMarineResource( nPreferredSlot );		// if we're not swapping out for another then abort
		if ( pExisting && pExisting->GetCommander() != pPlayer )
			return false;
	}

	// don't select a dead man
	bool bDead = false;	
	if (ASWGameResource()->IsCampaignGame())
	{
		CASW_Campaign_Save *pSave = ASWGameResource()->GetCampaignSave();
		if (pSave)
		{
			if (!pSave->IsMarineAlive(RosterIndex))
			{
				bDead = true;
				return false;
			}
		}
	}

	if (!ASWGameResource()->IsRosterSelected(RosterIndex))		// if not already selected
	{
		bool bCanSelect = true;
		// check we're allowed to take this marine, if he's reserved
		if (ASWGameResource()->IsRosterReserved(RosterIndex) && GetCampaignSave() && RosterIndex>=0 && RosterIndex <ASW_NUM_MARINE_PROFILES)
		{
			if (pPlayer)
			{
				char buffer[256];
				Q_snprintf(buffer, sizeof(buffer), "%s%s", pPlayer->GetPlayerName(), pPlayer->GetASWNetworkID());
				if (Q_strcmp(buffer, STRING(GetCampaignSave()->m_LastCommanders[RosterIndex])))
					bCanSelect = false;
			}
		}
		// check this marine isn't already selected by someone else
		for (int i=0;i<ASWGameResource()->GetMaxMarineResources();i++)
		{
			CASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource(i);
			if (pMR && pMR->GetProfileIndex() == RosterIndex)
			{
				bCanSelect = false;
				break;
			}
		}
		if (bCanSelect)
		{						
			CASW_Marine_Resource* m = (CASW_Marine_Resource*)CreateEntityByName("asw_marine_resource");
			m->SetCommander(pPlayer);
			m->SetProfileIndex(RosterIndex);
			if ( ASWEquipmentList() )
			{
				for ( int iWpnSlot = 0; iWpnSlot < ASW_MAX_EQUIP_SLOTS; ++ iWpnSlot )
				{
					const char *szWeaponClass = m->GetProfile()->m_DefaultWeaponsInSlots[ iWpnSlot ];
					int nWeaponIndex = ASWEquipmentList()->GetIndexForSlot( iWpnSlot, szWeaponClass );
					if ( nWeaponIndex < 0 )		// if there's a bad weapon here, then fall back to one of the starting weapons
					{
						if ( iWpnSlot == 2 )
						{
							nWeaponIndex = ASWEquipmentList()->GetIndexForSlot( iWpnSlot, "asw_weapon_medkit" );
						}
						else
						{
							nWeaponIndex = ASWEquipmentList()->GetIndexForSlot( iWpnSlot, "asw_weapon_rifle" );
						}
					}
					if ( nWeaponIndex >= 0 )
					{
						m->m_iWeaponsInSlots.Set( iWpnSlot, nWeaponIndex );
					}
					else
					{
						Warning( "Bad default weapon for %s in slot %d\n", m->GetProfile()->GetShortName(), iWpnSlot );
					}
				}
			}
			m->Spawn();	// asw needed?
			if ( !ASWGameResource()->AddMarineResource( m, nPreferredSlot ) )
			{
				UTIL_Remove( m );
				return false;
			}

			ASWGameResource()->SetRosterSelected(RosterIndex, 1);			// select the marine

			return true;
		}
	}
	return false;
}

void CAlienSwarm::RosterDeselect( CASW_Player *pPlayer, int RosterIndex)
{
	if (!ASWGameResource())
		return;

	// only allow roster deselection during briefing
	if (GetGameState() != ASW_GS_BRIEFING)
		return;

	if (!ASWGameResource()->IsRosterSelected(RosterIndex))		// if not already selected
		return;

	// check if this marine is selected by this player
	for (int i=0;i<ASWGameResource()->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource* pMR = ASWGameResource()->GetMarineResource(i);
		if (pMR && pMR->GetCommander() == pPlayer && pMR->GetProfileIndex() == RosterIndex)
		{
			ASWGameResource()->SetRosterSelected(RosterIndex, 0);
			ASWGameResource()->DeleteMarineResource(pMR);
			return;
		}
	}
}

void CAlienSwarm::RosterDeselectAll( CASW_Player *pPlayer )
{
	if (!ASWGameResource() || !pPlayer)
		return;

	//Msg("  RosterDeselectAll\n");
	// check if this marine is selected by this player
	int m = ASWGameResource()->GetMaxMarineResources();
	for (int i=m-1;i>=0;i--)
	{
		CASW_Marine_Resource* pMR = ASWGameResource()->GetMarineResource(i);
		if (pMR && pMR->GetCommander() == pPlayer)
		{
			if (GetGameState() == ASW_GS_BRIEFING)
			{
				//Msg("Roster deselecting %d\n", pMR->GetProfileIndex());
				ASWGameResource()->SetRosterSelected(pMR->GetProfileIndex(), 0);
				ASWGameResource()->DeleteMarineResource(pMR);
			}
		}
	}
}

void CAlienSwarm::ReassignMarines(CASW_Player *pPlayer)
{
	if (!ASWGameResource() || !pPlayer)
		return;
	//Msg("  ReassignMarines\n");

	// make sure he's not inhabiting any of them
	pPlayer->LeaveMarines();
	int m = ASWGameResource()->GetMaxMarineResources();
	int iNewCommanderIndex = 1;
	for (int i=m-1;i>=0;i--)
	{
		CASW_Marine_Resource* pMR = ASWGameResource()->GetMarineResource(i);
		if (pMR && pMR->GetCommander() == pPlayer)
		{
			//Msg("marine resourceat slot %d belongs to disconnecting player (entindex %d)\n", i, pPlayer->entindex());
			CASW_Player *pNewCommander = NULL;
			int k = 1;
			while (k <= gpGlobals->maxClients && pNewCommander == NULL)	// loop until we've been round all players, or until we've found a valid one to give this marine to
			{
				//Msg("while loop, k = %d iNewCommanderIndex = %d\n", k, iNewCommanderIndex);
				pNewCommander = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(iNewCommanderIndex));
				//Msg("Newcommander entindex is %d\n", pNewCommander ? pNewCommander->entindex() : -1);
				if (pNewCommander && (!pNewCommander->IsConnected() || pNewCommander == pPlayer))
					pNewCommander = NULL;
				//Msg("after connected/match check: Newcommander entindex is %d\n", pNewCommander ? pNewCommander->entindex() : -1);
				
				k++;
				// loop which commander we're trying to find next (this means the marines get given out in a round robin fashion)
				iNewCommanderIndex++;
				if (iNewCommanderIndex > gpGlobals->maxClients)
				{
					//Msg("iNewCommander looping\n");
					iNewCommanderIndex = 1;
				}
			}
			//Msg("after search loop: Newcommander entindex is %d\n", pNewCommander ? pNewCommander->entindex() : -1);
			
			// sets the marine's commander to the other player we found, or no-one
			pMR->SetCommander( pNewCommander );
			CASW_Marine *pMarine = pMR->GetMarineEntity();
			if ( pMarine && pNewCommander )
			{
				pMarine->SetCommander( pNewCommander );
				pMarine->OrdersFromPlayer( pNewCommander, ASW_ORDER_FOLLOW, pNewCommander->GetMarine(), false );
			}
		}
	}
}

void CAlienSwarm::SetMaxMarines( CASW_Player *pException )
{
	if (GetGameState() != ASW_GS_BRIEFING || !ASWGameResource())
		return;

	CASW_Game_Resource *pGameResource = ASWGameResource();

	// count number of player starts
	int iPlayerStarts = 0;
	if (IsTutorialMap())	// in the tutorial map we just assume max players starts, since briefing isn't used
	{
		iPlayerStarts = 8;
	}
	else
	{
		CBaseEntity *pStartEntity = NULL;
		do
		{
			pStartEntity = gEntList.FindEntityByClassname( pStartEntity, "info_player_start");
			if (pStartEntity && IsValidMarineStart(pStartEntity))
				iPlayerStarts++;
		} while (pStartEntity!=NULL);
	}

	// count number of non spectating players
	int iPlayers = 0;	
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )	
	{
		CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i));

		if ( pOtherPlayer && pOtherPlayer != pException && pOtherPlayer->IsConnected()
				&& (!(pGameResource->GetNumMarines(pOtherPlayer) <= 0 && pGameResource->IsPlayerReady(pOtherPlayer))) )	// if player is ready with no marines selected, he's spectating, so don't count him
		{
			iPlayers++;
		}
	}	

	if (iPlayers < 4)
	{
		pGameResource->SetMaxMarines(4, false);
	}
	else
	{
		if (asw_fair_marine_rules.GetBool())
		{
			pGameResource->SetMaxMarines(iPlayers, true);
			EnforceFairMarineRules();
		}
		else
		{
			pGameResource->SetMaxMarines(iPlayers, false);
		}
	}

	// if we don't have enough player starts, lower the max
	if (iPlayerStarts < pGameResource->m_iMaxMarines)
	{
		pGameResource->SetMaxMarines(iPlayerStarts, pGameResource->m_bOneMarineEach);
	}

	if (asw_override_max_marines.GetInt() > 0)
	{
		pGameResource->SetMaxMarines(asw_override_max_marines.GetInt(), false);
	}

	// if we have too many selected, deselect some
	int k = 0;
	while (pGameResource->m_iNumMarinesSelected > pGameResource->m_iMaxMarines && k < 255)
	{
		pGameResource->RemoveAMarine();
		k++;
	}
}

// 1 marine each 'fair rules' have been turned on, make sure no players have 2 selected
void CAlienSwarm::EnforceFairMarineRules()
{
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )	
	{
		CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i));

		if ( pOtherPlayer && pOtherPlayer->IsConnected() )
		{
			int k = 0;
			while (pGameResource->GetNumMarines(pOtherPlayer) > 1 && k < 255)
			{
				pGameResource->RemoveAMarineFor(pOtherPlayer);
				k++;
			}
		}
	}
}

void CAlienSwarm::ReviveDeadMarines()
{
	if (IsCampaignGame() && GetCampaignSave())
	{
		for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
		{
			GetCampaignSave()->ReviveMarine(i);			
		}
		GetCampaignSave()->SaveGameToFile();
		if (ASWGameResource())
			ASWGameResource()->UpdateMarineSkills(GetCampaignSave());
		UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_marines_revived" );
	}	
}

void CAlienSwarm::LoadoutSelect( CASW_Player *pPlayer, int iRosterIndex, int iInvSlot, int iEquipIndex)
{
	if (!ASWGameResource())
		return;

	if ( iInvSlot < 0 || iInvSlot >= ASW_MAX_EQUIP_SLOTS )
		return;

	// find the appropriate marine resource
	int iMarineIndex=-1;
	for (int i=0;i<ASWGameResource()->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource(i);
		if (!pMR)
			continue;

		if (pMR->GetProfileIndex() == iRosterIndex)
		{
			iMarineIndex = i;
			break;
		}
	}
	if (iMarineIndex == -1)
		return;

	CASW_Marine_Resource* pMarineResource = ASWGameResource()->GetMarineResource(iMarineIndex);
	if (!pMarineResource)
		return;

	// Figure out what item the marine is trying to equip
	CASW_EquipItem *pNewItem = ASWEquipmentList()->GetItemForSlot( iInvSlot, iEquipIndex );
	if ( !pNewItem || !pNewItem->m_bSelectableInBriefing )
		return;
	
	// Figure out if the marine is already carrying an item in the slot
	CASW_EquipItem *pOldItem = ASWEquipmentList()->GetItemForSlot( iInvSlot, pMarineResource->m_iWeaponsInSlots.Get( iInvSlot ) );
	// Can swap the old item for new one?
	if ( !MarineCanPickup( pMarineResource,
		pNewItem ? STRING(pNewItem->m_EquipClass) : NULL,
		pOldItem ? STRING(pOldItem->m_EquipClass) : NULL ) )
		return;

	pMarineResource->m_iWeaponsInSlots.Set( iInvSlot, iEquipIndex );

	if ( ASWHoldoutMode() )
	{
		ASWHoldoutMode()->LoadoutSelect( pMarineResource, iEquipIndex, iInvSlot );
	}
}

// a player wants to start the mission
//  flag it so we can trigger the start in our next think
void CAlienSwarm::RequestStartMission(CASW_Player *pPlayer)
{
	if (!ASWGameResource())
		return;

	// check we actually have some marines selected before starting
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;
	int m = pGameResource->GetMaxMarineResources();	
	bool bCanStart = false;
	bool bTech = false;
	for (int i=0;i<m;i++)
	{
		if (pGameResource->GetMarineResource(i))
		{
			bCanStart = true;

			// check for a tech
			if (pGameResource->GetMarineResource(i)->GetProfile() && pGameResource->GetMarineResource(i)->GetProfile()->CanHack())
				bTech = true;
		}
	}
	if (!bCanStart)
		return;
	if (m_bMissionRequiresTech && !bTech)
		return;
	if (m_hEquipReq.Get() && !m_hEquipReq->AreRequirementsMet())
		return;
	
	if (ASWGameResource()->AreAllOtherPlayersReady(pPlayer->entindex()))
	{
		m_bShouldStartMission = true;
	}
}

void CAlienSwarm::StartMission()
{	
	if (m_iGameState != ASW_GS_BRIEFING)
		return;

	SetForceReady(ASW_FR_NONE);
	
	// check we actually have some marines selected before starting
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if ( !pGameResource )
		return;

	int iMaxMarineResources = pGameResource->GetMaxMarineResources();	
	bool bCanStart = false;
	bool bTech = false;
	bool bMedic = false;
	for ( int i = 0; i < iMaxMarineResources; i++ )
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource( i );
		if ( pMR )
		{
			bCanStart = true;

			// check for a tech
			if ( pMR->GetProfile() && pMR->GetProfile()->CanHack() )
			{
				bTech = true;
			}

			// check for a medic
			if ( pMR->GetProfile() && pMR->GetProfile()->CanUseFirstAid() )
			{
				bMedic = true;
			}

			pMR->m_TimelineFriendlyFire.ClearAndStart();
			pMR->m_TimelineKillsTotal.ClearAndStart();
			pMR->m_TimelineHealth.ClearAndStart();
			pMR->m_TimelineAmmo.ClearAndStart();
			pMR->m_TimelinePosX.ClearAndStart();
			pMR->m_TimelinePosY.ClearAndStart();
		}
	}
	if (!bCanStart)
		return;
	if (m_bMissionRequiresTech && !bTech)	
		return;
	if (m_hEquipReq.Get() && !m_hEquipReq->AreRequirementsMet())
		return;

	// store our current leader (so we can keep the same leader after a map load)
	pGameResource->RememberLeaderID();

	// activate the level's ambient sounds
	StartAllAmbientSounds();

	// carnage mode?
	if (IsCarnageMode())
	{
		ASW_ApplyCarnage_f(2);
	}

	// increase num retries
	if (IsCampaignGame() && GetCampaignSave())
	{
		CASW_Campaign_Save* pSave = GetCampaignSave();
		pSave->IncreaseRetries();
		pSave->UpdateLastCommanders();
		pSave->SaveGameToFile();
	}	

	m_Medals.OnStartMission();	

	if ( ASWDirector() )
	{
		ASWDirector()->OnMissionStarted();
	}

	Msg("==STARTMISSION==\n");

	SetGameState(ASW_GS_LAUNCHING);
	mm_swarm_state.SetValue( "ingame" );
	m_fNextLaunchingStep = gpGlobals->curtime + ASW_LAUNCHING_STEP;

	// reset fail advice
	ASWFailAdvice()->OnMissionStart();

	if ( !bMedic )
	{
		ASWFailAdvice()->OnNoMedicStart();
	}

	if ( ASWHoldoutMode() )
	{
		ASWHoldoutMode()->OnMissionStart();
	}

	// reset various chatter timers
	CASW_Drone_Advanced::s_fNextTooCloseChatterTime = 0;
	CASW_Egg::s_fNextSpottedChatterTime = 0;
	CASW_Marine::s_fNextMadFiringChatter = 0;
	CASW_Marine::s_fNextIdleChatterTime = 0;
	CASW_Parasite::s_fNextSpottedChatterTime = 0;
	CASW_Parasite::s_fLastHarvesiteAttackSound = 0;	
	CASW_Shieldbug::s_fNextSpottedChatterTime = 0;
	CASW_Alien_Goo::s_fNextSpottedChatterTime = 0;
	CASW_Harvester::s_fNextSpawnSoundTime = 0;
	CASW_Harvester::s_fNextPainSoundTime = 0;
// 	CASW_Spawner::s_iFailedUberSpawns = 0;
// 	CASW_Spawner::s_iUberDronesSpawned = 0;
// 	CASW_Spawner::s_iNormalDronesSpawned = 0;
	m_fMissionStartedTime = gpGlobals->curtime;

	// check if certain marines are here for conversation triggering
	bool bSarge, bJaeger, bWildcat, bWolfe;
	bSarge = bJaeger = bWildcat = bWolfe = false;
	for ( int i = 0;i < iMaxMarineResources; i++ )
	{
		if (pGameResource->GetMarineResource(i) && pGameResource->GetMarineResource(i)->GetProfile())
		{
			if (pGameResource->GetMarineResource(i)->GetProfile()->m_VoiceType == ASW_VOICE_SARGE)
				bSarge = true;
			if (pGameResource->GetMarineResource(i)->GetProfile()->m_VoiceType == ASW_VOICE_JAEGER)
				bJaeger = true;
			if (pGameResource->GetMarineResource(i)->GetProfile()->m_VoiceType == ASW_VOICE_WILDCAT)
				bWildcat = true;
			if (pGameResource->GetMarineResource(i)->GetProfile()->m_VoiceType == ASW_VOICE_WOLFE)
				bWolfe = true;
		}
	}

	m_bSargeAndJaeger = bSarge && bJaeger;
	m_bWolfeAndWildcat = bWildcat && bWolfe;

	if (IsCarnageMode())
		asw_last_game_variation.SetValue(1);
	else if (IsUberMode())
		asw_last_game_variation.SetValue(2);
	else if (IsHardcoreMode())
		asw_last_game_variation.SetValue(3);
	else
		asw_last_game_variation.SetValue(0);

	CASW_GameStats.Event_MissionStarted();

	// count eggs in map
	ASWGameResource()->m_iStartingEggsInMap = 0;
	CBaseEntity* pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "asw_egg" )) != NULL)
	{
		ASWGameResource()->m_iStartingEggsInMap++;
	}

	AddBonusChargesToPickups();
}

void CAlienSwarm::UpdateLaunching()
{
	if (!ASWGameResource())
		return;

	if (gpGlobals->curtime < m_fNextLaunchingStep)
		return;

	int iNumMarines = ASWGameResource()->GetNumMarines(NULL);

	if (m_iMarinesSpawned >=iNumMarines || !SpawnNextMarine())
	{
		// we've spawned all we can, finish up and go to ingame state
		
		// any players with no marines should be set to spectating one
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i));
			
			if ( pOtherPlayer && pOtherPlayer->IsConnected() && ASWGameResource())
			{
				if (ASWGameResource()->GetNumMarines(pOtherPlayer) == 0)
					pOtherPlayer->SpectateNextMarine();
			}
		}	

		// notify all our alien spawners that the mission has started
		CBaseEntity* pEntity = NULL;
		while ((pEntity = gEntList.FindEntityByClassname( pEntity, "asw_spawner" )) != NULL)
		{
			CASW_Spawner* spawner = dynamic_cast<CASW_Spawner*>(pEntity);
			spawner->MissionStart();
		}

		SetGameState(ASW_GS_INGAME);
		mm_swarm_state.SetValue( "ingame" );
		DevMsg( "Setting game state to ingame\n" );

		// Alert gamestats of spawning
		CASW_GameStats.Event_MarinesSpawned();

		// tell all players to switch to their first marines
		// loop through all clients, count number of players on each team
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CASW_Player* pPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i));

			if ( pPlayer )
			{
				pPlayer->SwitchMarine(0);
				OrderNearbyMarines( pPlayer, ASW_ORDER_FOLLOW, false );
			}
		}

		// Set up starting stats
		CASW_Game_Resource *pGameResource = ASWGameResource();
		if ( pGameResource )
		{
			int iMaxMarineResources = pGameResource->GetMaxMarineResources();
			for ( int i = 0; i < iMaxMarineResources; i++ )
			{
				CASW_Marine_Resource *pMR = pGameResource->GetMarineResource( i );
				if ( pMR )
				{
					CASW_Marine *pMarine = pMR->GetMarineEntity();
					if ( pMarine )
					{
						pMR->m_TimelineAmmo.RecordValue( pMarine->GetAllAmmoCount() );
						pMR->m_TimelineHealth.RecordValue( pMarine->GetHealth() );
						pMR->m_TimelinePosX.RecordValue( pMarine->GetAbsOrigin().x );
						pMR->m_TimelinePosY.RecordValue( pMarine->GetAbsOrigin().y );
					}
				}
			}
		}

		// Start up any button hints
		for ( int i = 0; i < IASW_Use_Area_List::AutoList().Count(); i++ )
		{
			CASW_Use_Area *pArea = static_cast< CASW_Use_Area* >( IASW_Use_Area_List::AutoList()[ i ] );
			pArea->UpdateWaitingForInput();
		}
	}
	else
	{
		// still have more marines to spawn, set up our next launch stage
		m_fNextLaunchingStep = gpGlobals->curtime + ASW_LAUNCHING_STEP;
	}
}

void CAlienSwarm::ReportMissingEquipment()
{
	if (m_hEquipReq.Get())
		m_hEquipReq->ReportMissingEquipment();
}

void CAlienSwarm::ReportNeedTwoPlayers()
{
#ifdef GAME_DLL		
	UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_need_two_players" );
#endif
}

void CAlienSwarm::RestartMissionCountdown( CASW_Player *pPlayer )
{
	if ( GetGameState() != ASW_GS_INGAME )
	{
		RestartMission( pPlayer );
		return;
	}

	SetForceReady( ASW_FR_INGAME_RESTART );
}

void CAlienSwarm::RestartMission( CASW_Player *pPlayer, bool bForce )
{
	// don't allow restarting if we're on the campaign map, as this does Bad Things (tm)
	if (GetGameState() >= ASW_GS_CAMPAIGNMAP)
		return;

	// if a player is requesting the restart, then check everyone is ready
	if (pPlayer && GetGameState() > ASW_GS_INGAME)	// allow restart without readiness during the game/briefing
	{
		// check other players are ready for the restart
		if ( !bForce && ASWGameResource() && !ASWGameResource()->AreAllOtherPlayersReady( pPlayer->entindex() ) )
		{
			Msg("not all players are ready!\n");
			return;
		}
	}

	if ( GetGameState() == ASW_GS_INGAME && gpGlobals->curtime - ASWGameRules()->m_fMissionStartedTime > 30.0f )
	{
		// They've been playing a bit... go to the mission fail screen instead!
		ASWGameRules()->MissionComplete( false );
		return;
	}

	// if we're ingame, then upload for state (we don't do this once the mission is over, as stats are already sent on MissionComplete)
	// Stats todo:
	//if (GetGameState() == ASW_GS_INGAME && ASWGameStats())
		//ASWGameStats()->AddMapRecord();

	if (IsCampaignGame() && GetCampaignSave())
	{
		CASW_Campaign_Save* pSave = GetCampaignSave();
		//pSave->IncreaseRetries();
		pSave->SaveGameToFile();
	}

	SetForceReady(ASW_FR_NONE);

	if (!asw_instant_restart.GetBool())
	{
		if (ASWGameResource())
			ASWGameResource()->RememberLeaderID();
		//char buffer[64];
		if (IsCampaignGame())
			ChangeLevel_Campaign(STRING(gpGlobals->mapname));
		else
			engine->ChangeLevel(STRING(gpGlobals->mapname), NULL);
		return;
	}

	CBaseEntity *pEnt;
	CBaseEntity *pNextEntity;

	// notify players of our mission restart
	IGameEvent * event = gameeventmanager->CreateEvent( "asw_mission_restart" );
	if ( event )
	{
		m_iMissionRestartCount++;
		event->SetInt( "restartcount", m_iMissionRestartCount );		
		gameeventmanager->FireEvent( event );
	}

	// reset the node count since we'll be loading all these in again
	CNodeEnt::m_nNodeCount = 0;
 
	// find the first entity in the entity list
	pEnt = gEntList.FirstEnt();
	 
	// as long as we've got a valid pointer, keep looping through the list
	while (pEnt != NULL)
	{
		if (m_MapResetFilter.ShouldCreateEntity(pEnt->GetClassname()))		// resetting this entity
		{
			pNextEntity = gEntList.NextEnt(pEnt);
			UTIL_Remove(pEnt);		// mark entity for deletion
			pEnt = pNextEntity;
		}
		else	// keeping this entity, so don't destroy it
		{	
			pEnt = gEntList.NextEnt(pEnt);
		}
	}
		 
	// causes all marked entity to be actually removed
	gEntList.CleanupDeleteList();
		 
	// with any unrequired entities removed, we use MapEntity_ParseAllEntities to reparse the map entities
	// this in effect causes them to spawn back to their normal position.
	MapEntity_ParseAllEntities(engine->GetMapEntitiesString(), &m_MapResetFilter, true);
		
	// let the players know the mission is restarting
	UTIL_ClientPrintAll(HUD_PRINTCENTER, "Restarting Mission");

	m_pMissionManager = new CASW_Mission_Manager();
		
	// respawn players
	for (int i=1;i<=gpGlobals->maxClients;i++)
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex(i);
		
		if (pPlayer)
			pPlayer->Spawn();
	}

	// reset our game state and setup game resource, etc
	LevelInitPostEntity();

	m_fLastBlipSpeechTime = -200.0f;
}

// issues a changelevel command with the campaign argument and save name
void CAlienSwarm::ChangeLevel_Campaign(const char *map)
{
	Assert(ASWGameResource());
	if ( !Q_strnicmp( map, "random", 6 ) && GetCampaignSave() )
	{
		// schedule level generation
		if ( !m_pMapBuilder )
		{
			Msg("Failed to create map builder\n");
			return;
		}
		ASWGameResource()->m_iRandomMapSeed = RandomInt( 1, 65535 );
		Q_snprintf( ASWGameResource()->m_szMapGenerationStatus.GetForModify(), 128, "Generating map..." );
		const char *szNewMapName = UTIL_VarArgs( "campaignrandommap%d.vmf", GetCampaignSave()->m_iNumMissionsComplete.Get() );
		KeyValues *pMissionSettings = new KeyValues( "MissionSettings" );	// TODO: These will need to be filled in with data for random maps in a campaign to work
		KeyValues *pMissionDefinition = new KeyValues( "MissionDefinition" );
		m_pMapBuilder->ScheduleMapGeneration( szNewMapName, Plat_FloatTime() + 1.0f, pMissionSettings, pMissionDefinition );
		return;
	}

	engine->ChangeLevel( UTIL_VarArgs("%s campaign %s", map, ASWGameResource()->GetCampaignSaveName()) , NULL);
}

// called when the marines have finished the mission and want to save their progress and pick the next map to play
//  (Save and proceed)
void CAlienSwarm::CampaignSaveAndShowCampaignMap(CASW_Player* pPlayer, bool bForce)
{
	if (!bForce && pPlayer)
	{
		// abort if other players aren't ready
		if (!ASWGameResource()->AreAllOtherPlayersReady(pPlayer->entindex()))
			return;
	}

	if ( !IsCampaignGame() || !GetCampaignInfo() )
	{
		Msg("Unable to CampaignSaveAndShowCampaignMap as this isn't a campaign game!\n");
		return;
	}

	if (m_iGameState != ASW_GS_DEBRIEF)
	{
		Msg("Unable to CampaignSaveAndShowCampaignMap as game isn't at the debriefing\n");
		return;
	}

	if (!GetMissionSuccess())
	{
		Msg("Unable to CampaignSaveAndShowCampaignMap as mission was failed\n");
		return;
	}

	CASW_Campaign_Save* pSave = GetCampaignSave();
	if (!pSave)
	{
		Msg("Unable to CampaignSaveAndShowCampaignMap as we have no campaign savegame loaded!\n");
		return;
	}

	SetForceReady(ASW_FR_NONE);

	// give each marine some skill points for finishing the mission	
	if (ASWGameResource())
	{
		for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
		{
			CASW_Marine_Profile *pProfile = MarineProfileList()->m_Profiles[i];
			if (pProfile)
			{
				bool bOnMission = false;
				if (!pSave->IsMarineAlive(i))
				{
					Msg("Giving %s no points since s/he's dead.\n", pProfile->m_ShortName);
				}
				else
				{
					for (int k=0;k<ASWGameResource()->GetMaxMarineResources();k++)
					{
						CASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource(k);					
						if (pMR && pMR->GetProfile() == pProfile)
						{
							bOnMission = true;
							break;						
						}
					}
					//if (bOnMission)
					{
						// check the debrief stats to see how many skill points to give
						int iPointsToGive = ASW_SKILL_POINTS_PER_MISSION;
						if (m_hDebriefStats.Get())
						{
							iPointsToGive = m_hDebriefStats->GetSkillPointsAwarded(i);
						}
						//Msg("Giving %s %d points since s/he was on the mission.\n", pProfile->m_ShortName, iPointsToGive );
						for (int sp=0;sp<iPointsToGive;sp++)
						{
							pSave->IncreaseMarineSkill( i, ASW_SKILL_SLOT_SPARE );
						}
 					}
// 					else
// 					{
// 						Msg("Giving %s only 2 points since s/he wasn't on the mission.\n", pProfile->m_ShortName);
// 						pSave->IncreaseMarineSkill( i, ASW_SKILL_SLOT_SPARE );
// 						pSave->IncreaseMarineSkill( i, ASW_SKILL_SLOT_SPARE );
// 					}
				}
			}
		}
		ASWGameResource()->UpdateMarineSkills(pSave);
	}

	Msg("CAlienSwarm::CampaignSaveAndShowCampaignMap saving game and switching to campaign map mode\n");
	pSave->SetMissionComplete(GetCampaignSave()->m_iCurrentPosition);
	// save our awarded medals to file
	m_Medals.AddMedalsToCampaignSave(pSave);

	// if we're starting a new level, update the skill undo state for our marines
	if ( Q_strncmp( STRING(gpGlobals->mapname), "Lobby", 5 ) )		// unless we're on the lobby map, then don't update as it's not really moving to a new level, we're just loading a previous save or starting a new one
		GetCampaignSave()->UpdateSkillUndoState();

	// clear all wounding from the save
	for (int i=0;i<ASW_NUM_MARINE_PROFILES;i++)
	{
		GetCampaignSave()->SetMarineWounded(i, false);
	}

	// set wounding/death of marines
	for (int i=0;i<ASWGameResource()->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource(i);
		if (pMR)
		{
			if (pMR->GetHealthPercent() <= 0 && asw_campaign_death.GetBool() )
			{
				// tell the campaign save that the marine is DEAD!
				Msg("Setting marine %d dead\n", i);
				GetCampaignSave()->SetMarineDead(pMR->GetProfileIndex(), true);
				GetCampaignSave()->SetMarineWounded(pMR->GetProfileIndex(), false);
			}
			else if (pMR->m_bTakenWoundDamage && asw_campaign_wounding.GetBool() )
			{
				// tell the campaign save that the marine is wounded
				GetCampaignSave()->SetMarineWounded(pMR->GetProfileIndex(), true);
				GetCampaignSave()->SetMarineDead(pMR->GetProfileIndex(), false);
			}
			else
			{
				GetCampaignSave()->SetMarineDead(pMR->GetProfileIndex(), false);
				GetCampaignSave()->SetMarineWounded(pMR->GetProfileIndex(), false);
			}
		}
	}

	// increase parasite kill counts
	for (int i=0;i<ASWGameResource()->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource(i);
		if (pMR)
		{
			GetCampaignSave()->AddParasitesKilled(pMR->GetProfileIndex(), pMR->m_iParasitesKilled);			
		}
	}

	pSave->SaveGameToFile();
	
	// if the marines have completed all the missions in the campaign, then launch to the outro instead
	if (CampaignMissionsLeft()<=0)
	{		
		SetGameState(ASW_GS_OUTRO);
		// send user message telling clients to increment their 'campaigns completed' stat and to head to the outro map in x seconds

		// grab some info about the server, so clients know whether to reconnect or not
		int iDedicated = engine->IsDedicatedServer() ? 1 : 0;
		int iHostIndex = -1;
		if (!engine->IsDedicatedServer())
		{
			CBasePlayer *pPlayer = UTIL_GetListenServerHost();
			if (pPlayer)
				iHostIndex = pPlayer->entindex();
		}

		Msg("[S] Sending ASWCampaignCompleted dedicated = %d host = %d\n", iDedicated, iHostIndex);
		CReliableBroadcastRecipientFilter users;
		users.MakeReliable();
		UserMessageBegin( users, "ASWCampaignCompleted" );		
			WRITE_BYTE( iDedicated );
			WRITE_BYTE( iHostIndex );
		MessageEnd();		

		// give one second before changing map, so clients have time to do the above
		Msg("[S] Setting m_fLaunchOutroMapTime\n");
		m_fLaunchOutroMapTime = gpGlobals->curtime + 1.0f;
	}
	else	
	{
		// make sure all players are marked as not ready
		if (ASWGameResource())
		{
			for (int i=0;i<ASW_MAX_READY_PLAYERS;i++)
			{
				ASWGameResource()->m_bPlayerReady.Set(i, false);
			}
		}
		SetGameState(ASW_GS_CAMPAIGNMAP);
		GetCampaignSave()->SelectDefaultNextCampaignMission();
	}
}

// moves the marines from one location to another
bool CAlienSwarm::RequestCampaignMove(int iTargetMission)
{
	// only allow campaign moves if the campaign map is up
	if (m_iGameState != ASW_GS_CAMPAIGNMAP)
		return false;

	if (!GetCampaignSave() || !GetCampaignInfo())
		return false;

	GetCampaignSave()->SetMoveDestination(iTargetMission);

	return true;
}

// moves the marines from one location to another
bool CAlienSwarm::RequestCampaignLaunchMission(int iTargetMission)
{
	// only allow campaign moves if the campaign map is up
	if (m_iGameState != ASW_GS_CAMPAIGNMAP || iTargetMission == 0)	// 0 is the dropzone
		return false;

	if (!GetCampaignSave() || !GetCampaignInfo())
		return false;

	// don't allow the launch if we're not at this location
	if (GetCampaignSave()->m_iCurrentPosition != iTargetMission)
	{
		Msg("RequestCampaignLaunchMission %d failed as current location is %d\n", iTargetMission, GetCampaignSave()->m_iCurrentPosition);
		return false;
	}

	CASW_Campaign_Info::CASW_Campaign_Mission_t* pMission = GetCampaignInfo()->GetMission(iTargetMission);
	if (!pMission)
	{
		Msg("RequestCampaignLaunchMission %d failed as couldn't get this mission from the Campaign Info\n", iTargetMission);
		return false;
	}	

	// save it!
	GetCampaignSave()->SaveGameToFile();

	Msg("CAlienSwarm::RequestCampaignLaunchMission changing mission to %s\n", STRING(pMission->m_MapName));
	if (ASWGameResource())
		ASWGameResource()->RememberLeaderID();
	ChangeLevel_Campaign(STRING(pMission->m_MapName));
	return true;
}

void CAlienSwarm::VerifySpawnLocation( CASW_Marine *pMarine )
{
	// check spawn location is clear
	Vector vecPos = pMarine->GetAbsOrigin();
	trace_t tr;
	UTIL_TraceHull( vecPos,
		vecPos + Vector( 0, 0, 1 ),
		pMarine->CollisionProp()->OBBMins(),
		pMarine->CollisionProp()->OBBMaxs(),
		MASK_PLAYERSOLID,
		pMarine,
		COLLISION_GROUP_NONE,
		&tr );
	if( tr.fraction == 1.0 )		// current location is fine
		return;

	// now find the nearest clear info node
	CAI_Node *pNode = NULL;
	CAI_Node *pNearest = NULL;
	float fNearestDist = -1;

	for (int i=0;i<pMarine->GetNavigator()->GetNetwork()->NumNodes();i++)
	{
		pNode = pMarine->GetNavigator()->GetNetwork()->GetNode(i);
		if (!pNode)
			continue;
		float dist = pMarine->GetAbsOrigin().DistTo(pNode->GetOrigin());
		if (dist < fNearestDist || fNearestDist == -1)
		{
			// check the spot is clear
			vecPos = pNode->GetOrigin();		
			UTIL_TraceHull( vecPos,
				vecPos + Vector( 0, 0, 1 ),
				pMarine->CollisionProp()->OBBMins(),
				pMarine->CollisionProp()->OBBMaxs(),
				MASK_PLAYERSOLID,
				pMarine,
				COLLISION_GROUP_NONE,
				&tr );
			if( tr.fraction == 1.0 )
			{
				fNearestDist = dist;
				pNearest = pNode;
			}
		}
	}
	// found a valid node, teleport there
	if (pNearest)
	{
		Vector vecPos = pNearest->GetOrigin();
		pMarine->Teleport( &vecPos, NULL, NULL );
	}
}

// spawns a marine for each marine in the marine resource list
bool CAlienSwarm::SpawnNextMarine()
{
	if (!ASWGameResource())
		return false;
	
	if (m_iMarinesSpawned == 0)
	{
		if (IsTutorialMap())
			m_pSpawningSpot = CASW_TutorialStartPoint::GetTutorialStartPoint(0);
		else
			m_pSpawningSpot = GetMarineSpawnPoint(NULL);
	}

	if (!m_pSpawningSpot)
	{
		Msg("Failed to spawn a marine! No more spawn points could be found.\n");
		return false;
	}	

	for (int i=m_iMarinesSpawned;i<ASWGameResource()->GetMaxMarineResources() && m_pSpawningSpot;i++)
	{
		CASW_Marine_Resource* pMR = ASWGameResource()->GetMarineResource(i);
		if (!pMR)
			continue;
		
		if ( !SpawnMarineAt( pMR, m_pSpawningSpot->GetAbsOrigin(), m_pSpawningSpot->GetAbsAngles(), false ) )
			return false;

		m_iMarinesSpawned++;

		// grab the next spawn spot
		if (IsTutorialMap())
		{
			m_pSpawningSpot = CASW_TutorialStartPoint::GetTutorialStartPoint(i+1);
		}
		else
		{
			m_pSpawningSpot = GetMarineSpawnPoint(m_pSpawningSpot);
			if (!m_pSpawningSpot)
			{
				Warning("Failed to find a pMarine spawn point.  Map must have 8 info_player_start points!\n");
				return false;
			}
		}
	}

	return true;
}

/**
@param bResurrection  if true, we are resurrecting a marine while the map is in progress; restore initial weapon values rather than save them
*/
bool CAlienSwarm::SpawnMarineAt( CASW_Marine_Resource * RESTRICT pMR, const Vector &vecPos, const QAngle &angFacing, bool bResurrection )
{
	Assert( pMR );
	// create the marine
	CASW_Marine * RESTRICT pMarine = (CASW_Marine*) CreateEntityByName( "asw_marine" );
	if ( !pMarine )
	{
		Msg("ERROR - Failed to spawn a pMarine!");
		return false;
	}

	if (pMR->GetProfile())
	{
		pMarine->SetName(AllocPooledString(pMR->GetProfile()->m_ShortName));
	}
	pMarine->Spawn();

	// position him
	pMarine->SetLocalOrigin( vecPos + Vector(0,0,1) );
	VerifySpawnLocation( pMarine );
	pMarine->m_vecLastSafePosition = vecPos + Vector(0,0,1);
	pMarine->SetAbsVelocity( vec3_origin );
	pMarine->SetAbsAngles( angFacing );
	pMarine->m_fHoldingYaw = angFacing[YAW];
	// set the pMarine's commander, pMarine resource, etc
	pMarine->SetMarineResource(pMR);
	pMarine->SetCommander(pMR->m_Commander);
	pMarine->SetInitialCommander( pMR->m_Commander.Get() );
	int iMarineHealth = MarineSkills()->GetSkillBasedValueByMarineResource(pMR, ASW_MARINE_SKILL_HEALTH);
	int iMarineMaxHealth = iMarineHealth;

	// half the pMarine's health if he's wounded
	if (IsCampaignGame() && GetCampaignSave())
	{
		if (GetCampaignSave()->IsMarineWounded(pMR->GetProfileIndex()))
		{
			iMarineHealth *= 0.5f;
			iMarineMaxHealth = iMarineHealth;
			pMR->m_bHealthHalved = true;
		}
	}
	else if ( ASWHoldoutMode() && bResurrection )
	{
		iMarineHealth *= 0.66;
	}

	pMarine->SetHealth(iMarineHealth);
	pMarine->SetMaxHealth(iMarineMaxHealth);

	pMarine->SetModelFromProfile();
	UTIL_SetSize(pMarine, pMarine->GetHullMins(),pMarine->GetHullMaxs());
	pMR->SetMarineEntity(pMarine);

	if ( ASWHoldoutMode() && bResurrection )
	{
		// give the pMarine the equipment selected on the briefing screen
		for ( int iWpnSlot = 0; iWpnSlot < ASW_MAX_EQUIP_SLOTS; ++ iWpnSlot )
			GiveStartingWeaponToMarine( pMarine, pMR->m_iInitialWeaponsInSlots[ iWpnSlot ], iWpnSlot );
	}
	else
	{
		// give the pMarine the equipment selected on the briefing screen
		for ( int iWpnSlot = 0; iWpnSlot < ASW_MAX_EQUIP_SLOTS; ++ iWpnSlot )
			GiveStartingWeaponToMarine( pMarine, pMR->m_iWeaponsInSlots.Get( iWpnSlot ), iWpnSlot );

		// store off his initial equip selection for stats tracking
		for ( int iWpnSlot = 0; iWpnSlot < ASW_MAX_EQUIP_SLOTS; ++ iWpnSlot )
		{
			pMR->m_iInitialWeaponsInSlots[ iWpnSlot ] = pMR->m_iWeaponsInSlots.Get( iWpnSlot );
		}
	}

	pMarine->GetMarineResource()->UpdateWeaponIndices();

	return true;
}

CBaseEntity* CAlienSwarm::GetMarineSpawnPoint(CBaseEntity *pStartEntity)
{	
	do
	{
		pStartEntity = gEntList.FindEntityByClassname( pStartEntity, "info_player_start");
		if (pStartEntity && IsValidMarineStart(pStartEntity))
			return pStartEntity;
	} while (pStartEntity!=NULL);

	return NULL;
}

// make sure this spot doesn't have a marine on it already
bool CAlienSwarm::IsValidMarineStart(CBaseEntity *pSpot)
{
	//CBaseEntity *ent = NULL;
/*
	for ( CEntitySphereQuery sphere( pSpot->GetAbsOrigin(), 128 ); ent = sphere.GetCurrentEntity(); sphere.NextEntity() )
	{
		CASW_Marine* marine;
		marine = CASW_Marine::AsMarine( ent );
		if (marine!=NULL)
		{
			Msg("rejecting this start spot as a marine is nearby\n");
			return false;
		}
	}*/

	//Msg("this start spot is good\n");
	return true;
}

void CAlienSwarm::StartStim( float duration, CBaseEntity *pSource )
{
	m_flStimEndTime = gpGlobals->curtime + duration;
	m_flStimStartTime = gpGlobals->curtime;
	m_hStartStimPlayer = pSource;
}

void CAlienSwarm::StopStim()
{
	m_flStimEndTime = gpGlobals->curtime;
}

void CAlienSwarm::ThinkUpdateTimescale() RESTRICT 
{
	if ( GetGameState() != ASW_GS_INGAME || m_bMissionFailed )
	{
		// No slowdown when we're not in game
		GameTimescale()->SetDesiredTimescale( 1.0f );
		return;
	}

	if ( asw_marine_death_cam.GetBool() )
	{
		if ( gpGlobals->curtime >= m_fMarineDeathTime && gpGlobals->curtime <= m_fMarineDeathTime + asw_time_scale_delay.GetFloat() + asw_marine_death_cam_time.GetFloat() )
		{
			// Wait for the delay before invuln starts
			if ( gpGlobals->curtime > m_fMarineDeathTime + asw_time_scale_delay.GetFloat() )
			{
				MarineInvuln( true );
			}

			GameTimescale()->SetDesiredTimescaleAtTime( asw_marine_death_cam_time_scale.GetFloat(), asw_marine_death_cam_time_interp.GetFloat(), CGameTimescale::INTERPOLATOR_EASE_IN_OUT, m_fMarineDeathTime + asw_time_scale_delay.GetFloat() );
			return;
		}
		else if ( gpGlobals->curtime > m_fMarineDeathTime + asw_time_scale_delay.GetFloat() + asw_marine_death_cam_time.GetFloat() + 1.5f )
		{
			// Wait for a longer delay before invuln stops
			MarineInvuln( false );

			m_nMarineForDeathCam = -1;
			m_fMarineDeathTime = 0.0f;
		}
	}

	if ( gpGlobals->curtime >= ( m_fObjectiveSlowDownEndTime - asw_objective_slowdown_time.GetFloat() ) && gpGlobals->curtime < m_fObjectiveSlowDownEndTime )
	{
		GameTimescale()->SetDesiredTimescale( asw_objective_update_time_scale.GetFloat(), 1.5f, CGameTimescale::INTERPOLATOR_EASE_IN_OUT, asw_time_scale_delay.GetFloat() );
		return;
	}

	if ( m_flStimEndTime > gpGlobals->curtime )
	{
		GameTimescale()->SetDesiredTimescale( asw_stim_time_scale.GetFloat(), 1.5f, CGameTimescale::INTERPOLATOR_EASE_IN_OUT, asw_time_scale_delay.GetFloat() );
		return;
	}

	GameTimescale()->SetDesiredTimescale( 1.0f, 1.5f, CGameTimescale::INTERPOLATOR_EASE_IN_OUT, asw_time_scale_delay.GetFloat() );
}

void CAlienSwarm::PlayerThink( CBasePlayer *pPlayer )
{
}

// --------------------------------------------------------------------------------------------------- //
// Voice helper
// --------------------------------------------------------------------------------------------------- //

class CVoiceGameMgrHelper : public IVoiceGameMgrHelper
{
public:
	virtual bool		CanPlayerHearPlayer( CBasePlayer *pListener, CBasePlayer *pTalker, bool &bProximity )
	{
		// players can always hear each other in Infested
		return true;
	}
};
CVoiceGameMgrHelper g_VoiceGameMgrHelper;
IVoiceGameMgrHelper *g_pVoiceGameMgrHelper = &g_VoiceGameMgrHelper;

// World.cpp calls this but we don't use it in Infested.
void InitBodyQue()
{
}

void CAlienSwarm::Think()
{
	ThinkUpdateTimescale();
	GetVoiceGameMgr()->Update( gpGlobals->frametime );
	if ( m_iGameState <= ASW_GS_BRIEFING )
	{
		SetSkillLevel( asw_skill.GetInt() );
	}

	switch (m_iGameState)
	{
	case ASW_GS_BRIEFING:	
		{
			// let our mission chooser source think while we're relatively idle in the briefing, in case it needs to be scanning for missions
			if ( missionchooser && missionchooser->LocalMissionSource() && gpGlobals->maxClients > 1 )
			{
				missionchooser->LocalMissionSource()->IdleThink();
			}
			if (m_bShouldStartMission)
				StartMission();

			if (m_fReserveMarinesEndTime != 0 && gpGlobals->curtime > m_fReserveMarinesEndTime)
			{
				UnreserveMarines();
			}
			CheckForceReady();
		}
		break;
	case ASW_GS_LAUNCHING:
		{
			UpdateLaunching();
		}
		break;
	case ASW_GS_DEBRIEF:
		{
			CheckForceReady();
			if (gpGlobals->curtime >= m_fRemoveAliensTime && m_fRemoveAliensTime != 0)
			{
				RemoveAllAliens();
				RemoveNoisyWeapons();
			}
		}
		break;
	case ASW_GS_INGAME:
		{
			if ( m_iForceReadyType == ASW_FR_INGAME_RESTART )
			{
				CheckForceReady();
			}
			CheckTechFailure();
		}
		break;
	case ASW_GS_OUTRO:
		{
			if (gpGlobals->curtime > m_fLaunchOutroMapTime)
			{
#ifdef OUTRO_MAP
				Msg("[S] m_fLaunchOutroMapTime is up, doing changelevel!\n");
				m_fLaunchOutroMapTime = 0;
				if (engine->IsDedicatedServer())
				{
					// change to a single mission
					//engine->ChangeLevel(asw_default_mission.GetString(), NULL);
				}
				else
				{
					// move server to the outro
					CASW_Campaign_Info *pCampaign = GetCampaignInfo();
					const char *pszOutro = "outro_jacob";	 // the default
					if (pCampaign)
					{
						const char *pszCustomOutro = STRING(pCampaign->m_OutroMap);
						if (pszCustomOutro && ( !Q_strnicmp( pszCustomOutro, "outro", 5 ) ))
						{
							pszOutro = pszCustomOutro;
							Msg("[S] Using custom outro\n");
						}
						else
						{
							Msg("[S] No valid custom outro defined in campaign info, using default\n");
						}
					}
					engine->ChangeLevel(pszOutro, NULL);
				}
#endif
			}
		}
		break;
	}

	if (m_iCurrentVoteType != ASW_VOTE_NONE)
		UpdateVote();

	if ( m_pMapBuilder && IsCampaignGame() )
	{
		if ( m_pMapBuilder->IsBuildingMission() )
		{
			m_pMapBuilder->Update( Plat_FloatTime() );
			ASWGameResource()->m_fMapGenerationProgress = m_pMapBuilder->GetProgress();
			if ( Q_strcmp( m_pMapBuilder->GetStatusMessage(), ASWGameResource()->m_szMapGenerationStatus.Get() ) )
			{
				Q_snprintf( ASWGameResource()->m_szMapGenerationStatus.GetForModify(), 128, "%s", m_pMapBuilder->GetStatusMessage() );
			}
		}
		else if ( m_pMapBuilder->GetProgress() == 1.0f )		// finished building
		{
			// check if all the clients have finished generating this map
			bool bAllFinished = true;
			if ( asw_client_build_maps.GetBool() )
			{
				CBasePlayer *pListenServer = engine->IsDedicatedServer() ? NULL : UTIL_GetListenServerHost();
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i));
					if ( pOtherPlayer && pOtherPlayer->IsConnected()
								&& pOtherPlayer != pListenServer					// listen server doesn't generate the map clientside
								&& pOtherPlayer->m_fMapGenerationProgress.Get() < 1.0f )
					{
						bAllFinished = false;
						break;
					}
				}
			}

			if ( bAllFinished )
			{
				//launch the map
				char fixedname[ 512 ];
				Q_strncpy( fixedname, m_pMapBuilder->GetMapName(), sizeof( fixedname ) );
				Q_StripExtension( fixedname, fixedname, sizeof( fixedname ) );
				ChangeLevel_Campaign( fixedname );
			}
		}
	}
}

void CAlienSwarm::OnServerHibernating()
{
	int iPlayers = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>( UTIL_PlayerByIndex( i ) );

		if ( pOtherPlayer && pOtherPlayer->IsConnected() )
		{
			iPlayers++;
		}
	}

	if ( iPlayers <= 0 )
	{
		// when server has no players, switch to the default campaign

		IASW_Mission_Chooser_Source* pSource = missionchooser ? missionchooser->LocalMissionSource() : NULL;
		if ( !pSource )
			return;

		const char *szCampaignName = asw_default_campaign.GetString();
		const char *szMissionName = NULL;
		KeyValues *pCampaignDetails = pSource->GetCampaignDetails( szCampaignName );
		if ( !pCampaignDetails )
		{
			Warning( "Unable to find default campaign %s when server started hibernating.", szCampaignName );
			return;
		}

		bool bSkippedFirst = false;
		for ( KeyValues *pMission = pCampaignDetails->GetFirstSubKey(); pMission; pMission = pMission->GetNextKey() )
		{
			if ( !Q_stricmp( pMission->GetName(), "MISSION" ) )
			{
				if ( !bSkippedFirst )
				{
					bSkippedFirst = true;
				}
				else
				{
					szMissionName = pMission->GetString( "MapName", NULL );
					break;
				}
			}
		}
		if ( !szMissionName )
		{
			Warning( "Unabled to find starting mission for campaign %s when server started hibernating.", szCampaignName );
			return;
		}

		char szSaveFilename[ MAX_PATH ];
		szSaveFilename[ 0 ] = 0;
		if ( !pSource->ASW_Campaign_CreateNewSaveGame( &szSaveFilename[0], sizeof( szSaveFilename ), szCampaignName, ( gpGlobals->maxClients > 1 ), szMissionName ) )
		{
			Warning( "Unable to create new save game when server started hibernating.\n" );
			return;
		}
		engine->ServerCommand( CFmtStr( "%s %s campaign %s\n",
			"changelevel",
			szMissionName,
			szSaveFilename ) );
	}
}

// Respawn a dead marine.
void CAlienSwarm::Resurrect( CASW_Marine_Resource * RESTRICT pMR, CASW_Marine *pRespawnNearMarine )  
{
	//AssertMsg1( !pMR->IsAlive() && 
	//((gpGlobals->curtime - pMR->m_fDeathTime) >= asw_marine_resurrection_interval.GetFloat() ),
	//"Tried to respawn %s before its time!", pMR->GetProfile()->GetShortName() );

	CAI_Network* RESTRICT pNetwork = g_pBigAINet;
	if ( !pNetwork || !pNetwork->NumNodes() )
	{
		Warning("Error: Can't resurrect marines as this map has no node network\n");
		return;
	}

	// walk over the network to find a node close enough to the marines. (For now I'll just choose one at random.)
	//const Vector &vecMarineHullMins = NAI_Hull::Mins( HULL_HUMAN );
	//const Vector &vecMarineHullMaxs = NAI_Hull::Maxs( HULL_HUMAN );
	const int iNumNodes = pNetwork->NumNodes();
	CAI_Node * RESTRICT pSpawnNode = NULL;
	float flNearestDist = FLT_MAX;
	Vector vecSpawnPos;
	Vector vecChosenSpawnPos;
	for ( int i = 0; i < iNumNodes; ++i )
	{
		CAI_Node * RESTRICT const pNode = pNetwork->GetNode( i );
		if ( !pNode || pNode->GetType() != NODE_GROUND )
			continue;

		vecSpawnPos = pNode->GetPosition( HULL_HUMAN );

		// find the nearest marine to this node
		//float flDistance = 0;
		CASW_Marine *pMarine = pRespawnNearMarine; //dynamic_cast<CASW_Marine*>(UTIL_ASW_NearestMarine( vecSpawnPos, flDistance ));
		if ( !pMarine )
			return;

		// TODO: check for exit triggers
#if 0
		// check node isn't in an exit trigger
		bool bInsideEscapeArea = false;
		for ( int d=0; d < pSpawnMan->m_EscapeTriggers.Count(); d++ )
		{
			if (pSpawnMan->m_EscapeTriggers[d]->CollisionProp()->IsPointInBounds( vecPos ) )
			{
				bInsideEscapeArea = true;
				break;
			}
		}
		if ( bInsideEscapeArea )
			continue;
#endif

		float flDist = pMarine->GetAbsOrigin().DistToSqr( vecSpawnPos );
		if ( flDist < flNearestDist )
		{
			// check if there's a route from this node to the marine(s)
			AI_Waypoint_t * RESTRICT const pRoute  = ASWPathUtils()->BuildRoute( vecSpawnPos, pMarine->GetAbsOrigin(), NULL, 100 );
			if ( !pRoute )
			{
				continue;
			}
			else
			{
				ASWPathUtils()->DeleteRoute( pRoute ); // don't leak routes
			}

			// if down here, have a candidate node
			pSpawnNode = pNode;
			flNearestDist = flDist;
			vecChosenSpawnPos = vecSpawnPos;
		}
	}

	if ( !pSpawnNode ) // no acceptable resurrect locations
		return;

	// WISE FWOM YOUW GWAVE!
	if ( !SpawnMarineAt( pMR, vecChosenSpawnPos + Vector(0,0,1), QAngle(0,0,0 ), true ) )
	{
		Msg( "Failed to resurrect marine %s\n", pMR->GetProfile()->GetShortName() );
		return;
	}
	else
	{
		CASW_Marine *pMarine = pMR->GetMarineEntity();
		AssertMsg1( pMarine, "SpawnMarineAt failed to populate marine resource %s with a marine entity!\n", pMR->GetProfile()->GetShortName() );
		// switch commander to the marine if he hasn't already got one selected
		if ( !pMR->GetCommander()->GetMarine() )
			pMR->GetCommander()->SwitchMarine(0 );
		pMarine->PerformResurrectionEffect();
	}
}

void CAlienSwarm::MarineInvuln()
{
	MarineInvuln( !m_bMarineInvuln );
}

void CAlienSwarm::MarineInvuln( bool bInvuln )
{
	m_bMarineInvuln = bInvuln;

	/*char text[256];
	Q_snprintf( text,sizeof(text), m_bMarineInvuln ? "Marines now invulnerable\n" : "Marines can now be hurt\n" );
	UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, text );*/

	for ( int i = 0; i < ASW_MAX_MARINE_RESOURCES; i++ )
	{
		CASW_Marine_Resource* pMR = ASWGameResource()->GetMarineResource(i);
		if (pMR!=NULL && pMR->GetMarineEntity()!=NULL && pMR->GetMarineEntity()->GetHealth() > 0)
		{
			if (m_bMarineInvuln)
				pMR->GetMarineEntity()->m_takedamage = DAMAGE_NO;
			else
				pMR->GetMarineEntity()->m_takedamage = DAMAGE_YES;
		}
	}
}

bool CAlienSwarm::CanHaveAmmo( CBaseCombatCharacter *pPlayer, int iAmmoIndex )
{
	if ( iAmmoIndex > -1 )
	{
		// Get the max carrying capacity for this ammo
		int iMaxCarry = GetAmmoDef()->MaxCarry( iAmmoIndex, pPlayer );

		// asw - allow carrying more ammo if we have duplicate guns
		CASW_Marine* pMarine = CASW_Marine::AsMarine( pPlayer );		
		if (pMarine)
		{
			int iGuns = pMarine->GetNumberOfWeaponsUsingAmmo(iAmmoIndex);
			iMaxCarry *= iGuns;
		}

		// Does the player have room for more of this type of ammo?
		if ( pPlayer->GetAmmoCount( iAmmoIndex ) < iMaxCarry )
			return true;
	}

	return false;
}

void CAlienSwarm::GiveStartingWeaponToMarine(CASW_Marine* pMarine, int iEquipIndex, int iSlot)
{
	if ( !pMarine || iEquipIndex == -1 || iSlot < 0 || iSlot >= ASW_MAX_EQUIP_SLOTS )
		return;

	const char* szWeaponClass = STRING( ASWEquipmentList()->GetItemForSlot( iSlot, iEquipIndex )->m_EquipClass );
		
	CASW_Weapon* pWeapon = dynamic_cast<CASW_Weapon*>(pMarine->Weapon_Create(szWeaponClass));
	if (!pWeapon)
		return;
	// If I have a name, make my weapon match it with "_weapon" appended
	if ( pMarine->GetEntityName() != NULL_STRING )
	{
		const char *pMarineName = STRING(pMarine->GetEntityName());
		const char *pError = UTIL_VarArgs("%s_weapon", pMarineName);
		string_t pooledName = AllocPooledString(pError);
		pWeapon->SetName( pooledName );
	}

	// set the amount of bullets in the gun
	//Msg("Giving starting waepon to marine: %s ",szWeaponClass);
	int iPrimaryAmmo = pWeapon->GetDefaultClip1();	
	int iSecondaryAmmo = IsTutorialMap() ? 0 : pWeapon->GetDefaultClip2();  // no grenades in the tutorial
	// adjust here for medical satchel charges if the marine has the skill for it
	if ( !stricmp(szWeaponClass, "asw_weapon_medical_satchel") || !stricmp(szWeaponClass, "asw_weapon_heal_grenade") )
	{
		if (pMarine->GetMarineProfile() && pMarine->GetMarineProfile()->CanUseFirstAid())
		{
			iPrimaryAmmo = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_HEALING, ASW_MARINE_SUBSKILL_HEALING_CHARGES);
			iSecondaryAmmo = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_HEALING, ASW_MARINE_SUBSKILL_SELF_HEALING_CHARGES);
		}
	}
	if ( !stricmp(szWeaponClass, "asw_weapon_heal_gun") )
	{
		if (pMarine->GetMarineProfile() && pMarine->GetMarineProfile()->CanUseFirstAid())
		{
			iPrimaryAmmo = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_HEALING, ASW_MARINE_SUBSKILL_HEAL_GUN_CHARGES);
		}
	}
	if ( !stricmp(szWeaponClass, "asw_weapon_flares") ||
		 !stricmp(szWeaponClass, "asw_weapon_grenades") || 
		 !stricmp(szWeaponClass, "asw_weapon_mines") ||
		 !stricmp(szWeaponClass, "asw_weapon_electrified_armor") ||
		 !stricmp(szWeaponClass, "asw_weapon_buff_grenade") ||
		 !stricmp(szWeaponClass, "asw_weapon_hornet_barrage") ||
		 !stricmp(szWeaponClass, "asw_weapon_heal_grenade") ||
		 !stricmp(szWeaponClass, "asw_weapon_t75") ||
		 !stricmp(szWeaponClass, "asw_weapon_freeze_grenades") ||
		 !stricmp(szWeaponClass, "asw_weapon_bait") ||
		 !stricmp(szWeaponClass, "asw_weapon_smart_bomb") ||
		 !stricmp(szWeaponClass, "asw_weapon_jump_jet") ||
		 !stricmp(szWeaponClass, "asw_weapon_tesla_trap")
		)
	{
		iPrimaryAmmo += asw_bonus_charges.GetInt();
	}
	pWeapon->SetClip1( iPrimaryAmmo );
	// set secondary bullets in the gun
	//Msg("Setting secondary bullets for %s to %d\n", szWeaponClass, iSecondaryAmmo);
	pWeapon->SetClip2( iSecondaryAmmo );
	
	// equip the weapon
	pMarine->Weapon_Equip_In_Index( pWeapon, iSlot );

	// set the number of clips
	if ( pWeapon->GetPrimaryAmmoType() != -1 )
	{
		int iClips = GetAmmoDef()->MaxCarry( pWeapon->GetPrimaryAmmoType(), pMarine );

		//Msg("Giving %d bullets for primary ammo type %d\n", iClips, pWeapon->GetPrimaryAmmoType());
		pMarine->GiveAmmo( iClips, pWeapon->GetPrimaryAmmoType(), true );
	}
	else
	{
		//Msg("No clips as no primary ammo type\n");
	}
	// if it's primary, switch to it
	if (iSlot == 0)
	{
		// temp comment
		pMarine->Weapon_Switch( pWeapon );
		pWeapon->SetWeaponVisible(true);
	}
	else
	{
		pWeapon->SetWeaponVisible(false);
	}
}

// find all pickups in the level and increment charges
void CAlienSwarm::AddBonusChargesToPickups()
{
	CBaseEntity *ent = NULL;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
	{
		const char *szClass = ent->GetClassname();
		if ( !stricmp(szClass, "asw_pickup_flares") ||
			!stricmp(szClass, "asw_pickup_grenades") || 
			!stricmp(szClass, "asw_pickup_mines")
			)
		{
			CASW_Pickup_Weapon *pPickup = dynamic_cast<CASW_Pickup_Weapon*>(ent);
			if ( pPickup )
			{
				pPickup->m_iBulletsInGun += asw_bonus_charges.GetInt();
			}
		}
	}
}

// AI Class stuff

void CAlienSwarm::InitDefaultAIRelationships()
{
	BaseClass::InitDefaultAIRelationships();

	//  Allocate memory for default relationships
	CBaseCombatCharacter::AllocateDefaultRelationships();

	// set up faction relationships
	CAI_BaseNPC::SetDefaultFactionRelationship(FACTION_MARINES, FACTION_ALIENS, D_HATE, 0 );
	CAI_BaseNPC::SetDefaultFactionRelationship(FACTION_MARINES, FACTION_MARINES, D_LIKE, 0 );

	CAI_BaseNPC::SetDefaultFactionRelationship(FACTION_ALIENS, FACTION_ALIENS, D_LIKE, 0 );
	CAI_BaseNPC::SetDefaultFactionRelationship(FACTION_ALIENS, FACTION_MARINES, D_HATE, 0 );
	CAI_BaseNPC::SetDefaultFactionRelationship(FACTION_ALIENS, FACTION_BAIT, D_HATE, 999 );

	/*
	int iNumClasses = GameRules() ? GameRules()->NumEntityClasses() : LAST_SHARED_ENTITY_CLASS;

	// --------------------------------------------------------------
	// First initialize table so we can report missing relationships
	// --------------------------------------------------------------
	for (int i=0;i<iNumClasses;i++)
	{
		for (int j=0;j<iNumClasses;j++)
		{
			// By default all relationships are neutral of priority zero
			CBaseCombatCharacter::SetDefaultRelationship( (Class_T)i, (Class_T)j, D_NU, 0 );
		}
	}

	// In Alien Swarm:
	//   CLASS_ANTLION = drones
	//   CLASS_COMBINE_HUNTER = shieldbug
	//   CLASS_HEADCRAB = parasites
	//	 CLASS_MANHACK = buzzers
	//	 CLASS_VORTIGAUNT = harvester
	//   CLASS_EARTH_FAUNA = grub
	//   
	//   CLASS_HACKED_ROLLERMINE = computer/button panel
	//   CLASS_MILITARY = door
	//
	//   CLASS_PLAYER_ALLY_VITAL = marines
	//   CLASS_PLAYER_ALLY = colonists

	CBaseCombatCharacter::SetDefaultRelationship(CLASS_ANTLION,				CLASS_PLAYER,				D_HT, 0);	
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_ANTLION,				CLASS_HEADCRAB,				D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_ANTLION,				CLASS_MANHACK,				D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_ANTLION,				CLASS_VORTIGAUNT,			D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_ANTLION,				CLASS_COMBINE_HUNTER,		D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_ANTLION,				CLASS_EARTH_FAUNA,			D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_ANTLION,				CLASS_PLAYER_ALLY_VITAL,	D_HT, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_ANTLION,				CLASS_PLAYER_ALLY,			D_HT, 0);	

	CBaseCombatCharacter::SetDefaultRelationship(CLASS_COMBINE_HUNTER,		CLASS_PLAYER,				D_HT, 0);	
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_COMBINE_HUNTER,		CLASS_HEADCRAB,				D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_COMBINE_HUNTER,		CLASS_MANHACK,				D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_COMBINE_HUNTER,		CLASS_VORTIGAUNT,			D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_COMBINE_HUNTER,		CLASS_ANTLION,				D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_COMBINE_HUNTER,		CLASS_EARTH_FAUNA,			D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_COMBINE_HUNTER,		CLASS_PLAYER_ALLY_VITAL,	D_HT, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_COMBINE_HUNTER,		CLASS_PLAYER_ALLY,			D_HT, 0);
	
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_HEADCRAB,			CLASS_PLAYER,				D_HT, 0);	
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_HEADCRAB,			CLASS_ANTLION,				D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_HEADCRAB,			CLASS_MANHACK,				D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_HEADCRAB,			CLASS_VORTIGAUNT,			D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_HEADCRAB,			CLASS_COMBINE_HUNTER,		D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_HEADCRAB,			CLASS_EARTH_FAUNA,			D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_HEADCRAB,			CLASS_PLAYER_ALLY_VITAL,	D_HT, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_HEADCRAB,			CLASS_PLAYER_ALLY,			D_HT, 0);

	CBaseCombatCharacter::SetDefaultRelationship(CLASS_MANHACK,				CLASS_PLAYER,				D_HT, 0);	
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_MANHACK,				CLASS_ANTLION,				D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_MANHACK,				CLASS_HEADCRAB,				D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_MANHACK,				CLASS_VORTIGAUNT,			D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_MANHACK,				CLASS_COMBINE_HUNTER,		D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_MANHACK,				CLASS_EARTH_FAUNA,			D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_MANHACK,				CLASS_PLAYER_ALLY_VITAL,	D_HT, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_MANHACK,				CLASS_PLAYER_ALLY,			D_HT, 0);

	CBaseCombatCharacter::SetDefaultRelationship(CLASS_VORTIGAUNT,			CLASS_PLAYER,				D_HT, 0);	
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_VORTIGAUNT,			CLASS_ANTLION,				D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_VORTIGAUNT,			CLASS_COMBINE_HUNTER,		D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_VORTIGAUNT,			CLASS_HEADCRAB,				D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_VORTIGAUNT,			CLASS_MANHACK,				D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_VORTIGAUNT,			CLASS_EARTH_FAUNA,			D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_VORTIGAUNT,			CLASS_PLAYER_ALLY_VITAL,	D_HT, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_VORTIGAUNT,			CLASS_PLAYER_ALLY,			D_HT, 0);

	CBaseCombatCharacter::SetDefaultRelationship(CLASS_EARTH_FAUNA,			CLASS_PLAYER,				D_HT, 0);	
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_EARTH_FAUNA,			CLASS_ANTLION,				D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_EARTH_FAUNA,			CLASS_HEADCRAB,				D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_EARTH_FAUNA,			CLASS_MANHACK,				D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_EARTH_FAUNA,			CLASS_VORTIGAUNT,			D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_EARTH_FAUNA,			CLASS_COMBINE_HUNTER,		D_LI, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_EARTH_FAUNA,			CLASS_PLAYER_ALLY_VITAL,	D_HT, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_EARTH_FAUNA,			CLASS_PLAYER_ALLY,			D_HT, 0);
	
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_PLAYER_ALLY_VITAL,	CLASS_PLAYER,				D_LI, 0);	
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_PLAYER_ALLY_VITAL,	CLASS_PLAYER_ALLY,			D_LI, 0);	
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_PLAYER_ALLY_VITAL,	CLASS_ANTLION,				D_HT, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_PLAYER_ALLY_VITAL,	CLASS_HEADCRAB,				D_HT, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_PLAYER_ALLY_VITAL,	CLASS_MANHACK,				D_HT, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_PLAYER_ALLY_VITAL,	CLASS_VORTIGAUNT,			D_HT, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_PLAYER_ALLY_VITAL,	CLASS_COMBINE_HUNTER,		D_HT, 0);
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_PLAYER_ALLY_VITAL,	CLASS_EARTH_FAUNA,			D_NU, 0);
	*/
}

CASW_Mission_Manager* CAlienSwarm::GetMissionManager()
{
	return m_pMissionManager;
}

void CAlienSwarm::CheatCompleteMission()
{
	if (m_iGameState == ASW_GS_INGAME && GetMissionManager())
		GetMissionManager()->CheatCompleteMission();
}

void CAlienSwarm::MissionComplete( bool bSuccess )
{
	if ( m_iGameState >= ASW_GS_DEBRIEF )	// already completed the mission
		return;

	StopStim();

	// setting these variables will make the player's go into their debrief screens
	if ( bSuccess )
	{
		m_bMissionSuccess = true;
		IGameEvent * event = gameeventmanager->CreateEvent( "mission_success" );
		if ( event )
		{
			event->SetString( "strMapName", STRING( gpGlobals->mapname ) );
			gameeventmanager->FireEvent( event );
		}
	}
	else
	{
		m_bMissionFailed = true;
		m_nFailAdvice = ASWFailAdvice()->UseCurrentFailAdvice();
	}
	SetGameState(ASW_GS_DEBRIEF);

	// Clear out any force ready state if we fail or succeed in the middle so that we always give a chance to award XP
	SetForceReady( ASW_FR_NONE );

	bool bSinglePlayer = false;

	CASW_Game_Resource *pGameResource = ASWGameResource();
	if ( pGameResource )
	{
		bSinglePlayer = pGameResource->IsOfflineGame();

		for ( int i = 0; i < ASW_MAX_READY_PLAYERS; i++ )
		{
			// make sure all players are marked as not ready to leave debrief
			pGameResource->m_bPlayerReady.Set(i, false);
		}

		if ( bSuccess )
		{
			// If they got out with half the total squad health, we're calling that well done
			float fTotalHealthPercentage = 0.0f;
			int nNumMarines = 0;

			for ( int i = 0; i < pGameResource->GetMaxMarineResources(); i++ )
			{
				CASW_Marine_Resource *pMR = pGameResource->GetMarineResource( i );
				if ( pMR )
				{
					fTotalHealthPercentage += pMR->GetHealthPercent();
					nNumMarines++;
				}
			}

			pGameResource->OnMissionCompleted( ( nNumMarines <= 0 ) ? ( false ) : ( fTotalHealthPercentage / nNumMarines > 0.5f ) );
		}
		else
		{
			pGameResource->OnMissionFailed();
		}
	}

	DevMsg("Set game state to debrief\n");

	// set all players to FL_FROZEN and calc their XP serverside
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( pPlayer )
		{
			pPlayer->AddFlag( FL_FROZEN );			
		}
	}

	// freeze all the npcs
	CAI_BaseNPC *npc = gEntList.NextEntByClass( (CAI_BaseNPC *)NULL );
	while (npc)
	{
		npc->SetActivity(ACT_IDLE);
		if (!npc->IsCurSchedule(SCHED_NPC_FREEZE))
			npc->ToggleFreeze();			
		npc = gEntList.NextEntByClass(npc);
	}
	// remove all the spawners	
	int iCount = 0;
	CBaseEntity *ent = NULL;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
	{
		if (ent!=NULL && 
				(ent->ClassMatches("asw_spawner")				
					)
			)
		{
			UTIL_Remove( ent );
			iCount++;
		}
	}
	// make marine invulnerable
	for ( int i = 0; i < pGameResource->GetMaxMarineResources(); i++ )
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource( i );
		if ( pMR )
		{
			CASW_Marine *pMarine = pMR->GetMarineEntity();
			if ( pMarine && pMarine->GetHealth() > 0 )
			{
				pMarine->m_takedamage = DAMAGE_NO;
				pMarine->AddFlag( FL_FROZEN );
			}

			pMR->m_TimelineFriendlyFire.RecordFinalValue( 0.0f );
			pMR->m_TimelineKillsTotal.RecordFinalValue( 0.0f );
			pMR->m_TimelineAmmo.RecordFinalValue( pMarine ? pMarine->GetAllAmmoCount() : 0.0f );
			pMR->m_TimelineHealth.RecordFinalValue( pMarine ? pMarine->GetHealth() : 0.0f );
			pMR->m_TimelinePosX.RecordFinalValue( pMarine ? pMarine->GetAbsOrigin().x : pMR->m_TimelinePosX.GetValueAtInterp( 1.0f ) );
			pMR->m_TimelinePosY.RecordFinalValue( pMarine ? pMarine->GetAbsOrigin().y : pMR->m_TimelinePosY.GetValueAtInterp( 1.0f ) );
		}
	}

	// award medals	
#ifndef _DEBUG
	if ( !m_bCheated )
#endif
		m_Medals.AwardMedals();

	// create stats entity to network down all the interesting numbers
	m_hDebriefStats = dynamic_cast<CASW_Debrief_Stats*>(CreateEntityByName("asw_debrief_stats"));
	
	if (m_hDebriefStats.Get() == NULL)
	{
		Msg("ASW: Error! Failed to create Debrief Stats\n");
		return;
	}
	else if ( pGameResource )
	{
		// fill in debrief stats
		//Msg("Created debrief stats, filling in values\n");	
		int iTotalKills = 0;
		float fWorstPenalty = 0;
		for ( int i = 0; i < ASW_MAX_MARINE_RESOURCES; i++ )
		{
			CASW_Marine_Resource *pMR = pGameResource->GetMarineResource( i );
			if ( !pMR )
				continue;
		
			m_hDebriefStats->m_iKills.Set(i, pMR->m_iAliensKilled);
			iTotalKills += pMR->m_iAliensKilled;
			float acc = 0;
			if (pMR->m_iPlayerShotsFired > 0)
			{				
				acc = float(pMR->m_iPlayerShotsFired - pMR->m_iPlayerShotsMissed) / float(pMR->m_iPlayerShotsFired);
				acc *= 100.0f;
			}
			m_hDebriefStats->m_fAccuracy.Set(i,acc);
			m_hDebriefStats->m_iFF.Set(i,pMR->m_fFriendlyFireDamageDealt);
			m_hDebriefStats->m_iDamage.Set(i,pMR->m_fDamageTaken);
			m_hDebriefStats->m_iShotsFired.Set(i,pMR->m_iShotsFired);
			m_hDebriefStats->m_iShotsHit.Set(i, pMR->m_iPlayerShotsFired - pMR->m_iPlayerShotsMissed );
			m_hDebriefStats->m_iWounded.Set(i,pMR->m_bTakenWoundDamage);
			m_hDebriefStats->m_iAliensBurned.Set(i,pMR->m_iMineKills);
			m_hDebriefStats->m_iHealthHealed.Set(i,pMR->m_iMedicHealing);
			m_hDebriefStats->m_iFastHacks.Set(i,pMR->m_iFastDoorHacks + pMR->m_iFastComputerHacks);

			// Set starting equips for the marine
			m_hDebriefStats->m_iStartingEquip0.Set(i, pMR->m_iInitialWeaponsInSlots[0]);
			m_hDebriefStats->m_iStartingEquip1.Set(i, pMR->m_iInitialWeaponsInSlots[1]);
			m_hDebriefStats->m_iStartingEquip2.Set(i, pMR->m_iInitialWeaponsInSlots[2]);
						
			// store the worst penalty for use later when penalizing skill points
			float fPenalty = pMR->m_fFriendlyFireDamageDealt * 2 + pMR->m_fDamageTaken;
			if (fPenalty > fWorstPenalty)
				fWorstPenalty = fPenalty;

			// award an additional skill point if they acheived certain medals in this mission:
			int iMedalPoints = 0;
#ifdef AWARD_SKILL_POINTS_FOR_MEDALS
			iMedalPoints += (m_Medals.HasMedal(MEDAL_PERFECT, pMR, true)) ? 1 : 0;
			iMedalPoints += (m_Medals.HasMedal(MEDAL_IRON_HAMMER, pMR, true)) ? 1 : 0;
			iMedalPoints += (m_Medals.HasMedal(MEDAL_INCENDIARY_DEFENCE, pMR, true)) ? 1 : 0;
			iMedalPoints += (m_Medals.HasMedal(MEDAL_IRON_SWORD, pMR, true)) ? 1 : 0;
			iMedalPoints += (m_Medals.HasMedal(MEDAL_SWARM_SUPPRESSION, pMR, true)) ? 1 : 0;
			iMedalPoints += (m_Medals.HasMedal(MEDAL_SILVER_HALO, pMR, true)) ? 1 : 0;
			iMedalPoints += (m_Medals.HasMedal(MEDAL_GOLDEN_HALO, pMR, true)) ? 1 : 0;
			iMedalPoints += (m_Medals.HasMedal(MEDAL_ELECTRICAL_SYSTEMS_EXPERT, pMR, true)) ? 1 : 0;
			iMedalPoints += (m_Medals.HasMedal(MEDAL_COMPUTER_SYSTEMS_EXPERT, pMR, true)) ? 1 : 0;
#endif

			// give each marine a base of 4 skill points
			m_hDebriefStats->m_iSkillPointsAwarded.Set( i, ASW_SKILL_POINTS_PER_MISSION + iMedalPoints );

			// tell everyone about bouncing shot kills for debugging:
			if (pMR->m_iAliensKilledByBouncingBullets > 0)
			{				
				char buffer[256];
				Q_snprintf(buffer,sizeof(buffer), "%d", pMR->m_iAliensKilledByBouncingBullets);
				UTIL_ClientPrintAll(ASW_HUD_PRINTTALKANDCONSOLE, "#asw_bouncing_kills", pMR->GetProfile()->m_ShortName, buffer);
			}
		}		

		// penalize skill points if each marine's penalty is over threshold of 60 and at least 20% of the worst marine's penalty
#ifdef PENALIZE_SKILL_POINTS
		for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
		{
			CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
			if (!pMR)
				continue;

			float fPenalty = pMR->m_fFriendlyFireDamageDealt * 2 + pMR->m_fDamageTaken;
			if (fPenalty > 60 && fPenalty >= fWorstPenalty * 0.8f)
			{
				int points = m_hDebriefStats->m_iSkillPointsAwarded[i];
				m_hDebriefStats->m_iSkillPointsAwarded.Set(i, points - 1);
				// double penalty if they've done really badly
				if (fPenalty >= 200)
				{
					points = m_hDebriefStats->m_iSkillPointsAwarded[i];
					m_hDebriefStats->m_iSkillPointsAwarded.Set(i, points - 1);
				}
			}

			// a marine is dead, give him nothing
			if (pMR->GetHealthPercent() <= 0)
			{
				if ( asw_campaign_death.GetBool() )
				{
					m_hDebriefStats->m_iSkillPointsAwarded.Set(i, 0);
				}
				else
				{
					// penalize one skill point if they died
					int points = MAX( 0, m_hDebriefStats->m_iSkillPointsAwarded[i] - 1 );
					m_hDebriefStats->m_iSkillPointsAwarded.Set(i, points);
				}
			}
		}
#endif

		// fill in debrief stats team stats/time taken/etc
		m_hDebriefStats->m_iTotalKills = iTotalKills;
		m_hDebriefStats->m_fTimeTaken = gpGlobals->curtime - m_fMissionStartedTime;

		// calc the speedrun time
		int speedrun_time = 180;	// default of 3 mins
		if (GetWorldEntity() && GetSpeedrunTime() > 0)
			speedrun_time = GetSpeedrunTime();

		// put in the previous best times/kills for the debrief stats
		const char *mapName = STRING(gpGlobals->mapname);
		if (MapScores())
		{
			m_hDebriefStats->m_fBestTimeTaken = MapScores()->GetBestTime(mapName, GetSkillLevel());
			m_hDebriefStats->m_iBestKills = MapScores()->GetBestKills(mapName, GetSkillLevel());
			m_hDebriefStats->m_iSpeedrunTime = speedrun_time;
		}

		// check for updating unlocked modes and scores
		if (MapScores() && GetMissionSuccess())	// && !m_bCheated
		{
			bool bJustUnlockedCarnage = false;
			bool bJustUnlockedUber = false;
			bool bJustUnlockedHardcore = false;

			// unlock special modes only in single mission mode
			if (!IsCampaignGame() && GetSkillLevel() >= 2)
			{
				//Msg("Checking for map %s unlocks\n", mapName);
				if (!MapScores()->IsModeUnlocked(mapName, GetSkillLevel(), ASW_SM_CARNAGE))
				{					
					// check for unlocking carnage (if we completed the mission on Insane)
					bJustUnlockedCarnage = (GetSkillLevel() >= 4);
					//Msg("Checked just carnage unlock = %d\n", bJustUnlockedCarnage);
				}
				if (!MapScores()->IsModeUnlocked(mapName, GetSkillLevel(), ASW_SM_UBER))
				{
					// check for unlocking uber
					bJustUnlockedUber = (m_hDebriefStats->m_fTimeTaken.Get() < speedrun_time);
					//Msg("Checked just uber unlock = %d (time take %f, speedryn time = %f)\n", bJustUnlockedUber,
						//m_hDebriefStats->m_fTimeTaken.Get(), speedrun_time);
				}
				if (!MapScores()->IsModeUnlocked(mapName, GetSkillLevel(), ASW_SM_HARDCORE))
				{
					// check for unlocking hardcore (all marines alive on normal or above
					if ( pGameResource )
					{
						bool bAllMarinesAlive = true;
						for ( int i = 0; i < pGameResource->GetMaxMarineResources(); i++ )
						{
							CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
							if (!pMR)
								continue;
							
							if (pMR->GetHealthPercent() <= 0)
							{
								bAllMarinesAlive = false;
								break;
							}
						}
						bJustUnlockedHardcore = bAllMarinesAlive;
						//Msg("Checked just hardcore unlock = %d\n", bJustUnlockedHardcore);
					}
				}
			}			

			int iUnlockModes = (bJustUnlockedCarnage ? ASW_SM_CARNAGE : 0) +
							   (bJustUnlockedUber ? ASW_SM_UBER : 0) +
							   (bJustUnlockedHardcore ? ASW_SM_HARDCORE : 0);

			// put the just unlocked modes into the debrief stats, so players can print a message on their debrief
			m_hDebriefStats->m_bJustUnlockedCarnage = bJustUnlockedCarnage;
			m_hDebriefStats->m_bJustUnlockedUber = bJustUnlockedUber;
			m_hDebriefStats->m_bJustUnlockedHardcore = bJustUnlockedHardcore;

			// notify players if we beat the speedrun time, even if we already have uber unlocked
			m_hDebriefStats->m_bBeatSpeedrunTime = (m_hDebriefStats->m_fTimeTaken.Get() < speedrun_time);

			// notify the mapscores of our data so it can save it
			MapScores()->OnMapCompleted(mapName, GetSkillLevel(), m_hDebriefStats->m_fTimeTaken.Get(), m_hDebriefStats->m_iTotalKills.Get(), iUnlockModes);
		}

		m_hDebriefStats->Spawn();

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )	
		{
			CASW_Player* pOtherPlayer = dynamic_cast< CASW_Player* >( UTIL_PlayerByIndex( i ) );
			if ( pOtherPlayer )
			{
				pOtherPlayer->AwardExperience();
			}
		}
	}

	// reset the progress if we finish the tutorial successfully
	if (IsTutorialMap() && bSuccess)
	{
		asw_tutorial_save_stage.SetValue(0);
	}

	// shut everything up
	StopAllAmbientSounds();


	// store stats for uploading
	CASW_GameStats.Event_MissionComplete( m_bMissionSuccess, m_nFailAdvice, ASWFailAdvice()->GetFailAdviceStatus() );

	// stats todo:
	//if (ASWGameStats())
		//ASWGameStats()->AddMapRecord();

	// print debug messages for uber spawning
	//char buffer[256];
	//Q_snprintf(buffer, sizeof(buffer), "Uber: Fail=%d Spawn=%d Normal:%d\n", 
			//CASW_Spawner::s_iFailedUberSpawns,
			//CASW_Spawner::s_iUberDronesSpawned,
			//CASW_Spawner::s_iNormalDronesSpawned);
	//UTIL_ClientPrintAll(ASW_HUD_PRINTTALKANDCONSOLE, buffer);	

	// set a timer to remove all the aliens once clients have had a chance to fade out
	m_fRemoveAliensTime = gpGlobals->curtime + 2.4f;
}

void CAlienSwarm::RemoveAllAliens()
{
	m_fRemoveAliensTime = 0;
	int iCount = 0;
	CBaseEntity *ent = NULL;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
	{
		for ( int i = 0; i < ASWSpawnManager()->GetNumAlienClasses(); i++ )
		{
			if ( !Q_stricmp( ASWSpawnManager()->GetAlienClass( i )->m_pszAlienClass, ent->GetClassname() ) )
			{
				UTIL_Remove( ent );
				iCount++;
			}
		}
	}
	//Msg("CAlienSwarm::RemoveAllAliens removed %d\n", iCount);
}

void CAlienSwarm::RemoveNoisyWeapons()
{
	int iCount = 0;
	CBaseEntity *ent = NULL;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
	{
		if (  FStrEq("asw_weapon_chainsaw", ent->GetClassname())
			)
		{
			UTIL_Remove( ent );
			iCount++;
		}
	}
	//Msg("CAlienSwarm::RemoveNoisyWeapons removed %d\n", iCount);
}

// send the minimap line draw to everyone
void CAlienSwarm::BroadcastMapLine(CASW_Player *pPlayer, int linetype, int world_x, int world_y)
{
	CRecipientFilter filter;
	filter.AddAllPlayers();
	filter.RemoveRecipient(pPlayer);

	UserMessageBegin( filter, "ASWMapLine" ); // create message 
	WRITE_BYTE( (char) linetype );
	WRITE_BYTE( pPlayer->entindex() );
	WRITE_LONG( world_x );	// send the location of the map line dot
	WRITE_LONG( world_y );
	MessageEnd(); //send message
}

void CAlienSwarm::BlipSpeech(int iMarine)
{
	if (!ASWGameResource())
		return;
	CASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource(iMarine);
	if (!pMR || !pMR->GetProfile() || !pMR->GetProfile()->CanHack())		// check the requested speech is coming from a tech
		return;
	// check no-one else said some blip speech recently
	if (gpGlobals->curtime - m_fLastBlipSpeechTime < ASW_BLIP_SPEECH_INTERVAL * 0.8f)
		return;

	CASW_Marine *pMarine = pMR->GetMarineEntity();
	if (!pMarine)
		return;

	CASW_MarineSpeech *pSpeech = pMarine->GetMarineSpeech();
	if (!pSpeech)
		return;

	IGameEvent * event = gameeventmanager->CreateEvent( "scanner_important" );
	if ( event )
	{
		gameeventmanager->FireEvent( event );
	}

	if ( !m_bPlayedBlipSpeech || random->RandomFloat() < asw_blip_speech_chance.GetFloat() )
	{
		pSpeech->Chatter(CHATTER_SCANNER);
		m_bPlayedBlipSpeech = true;
	}

	m_fLastBlipSpeechTime = gpGlobals->curtime;
}

void CAlienSwarm::MarineKilled( CASW_Marine *pMarine, const CTakeDamageInfo &info )
{
	if ( IsCampaignGame() && GetCampaignSave() )
	{
		GetCampaignSave()->OnMarineKilled();
	}

	for ( int i = 0; i < IASW_Marines_Past_Area_List::AutoList().Count(); i++ )
	{
		CASW_Marines_Past_Area *pArea = static_cast< CASW_Marines_Past_Area* >( IASW_Marines_Past_Area_List::AutoList()[ i ] );
		pArea->OnMarineKilled( pMarine );
	}
}

void CAlienSwarm::AlienKilled(CBaseEntity *pAlien, const CTakeDamageInfo &info)
{
	if (asw_debug_alien_damage.GetBool())
	{
		Msg("Alien %s killed by attacker %s inflictor %s\n", pAlien->GetClassname(), 
			info.GetAttacker() ? info.GetAttacker()->GetClassname() : "unknown",
			info.GetInflictor() ? info.GetInflictor()->GetClassname() : "unknown");
	}
	if (GetMissionManager())
		GetMissionManager()->AlienKilled(pAlien);

	if ( ASWHoldoutMode() )
	{
		ASWHoldoutMode()->OnAlienKilled( pAlien, info );
	}

	CASW_Shieldbug *pSB = dynamic_cast<CASW_Shieldbug*>(pAlien);

	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(info.GetAttacker());
	if ( pMarine )
	{
		CASW_Marine_Resource *pMR = pMarine->GetMarineResource();
		if ( pMR )
		{
			pMR->m_iAliensKilled++;			
			pMR->m_TimelineKillsTotal.RecordValue( 1.0f );

			CASW_Game_Resource *pGameResource = ASWGameResource();
			if ( pGameResource )
			{
				if ( pMR->GetCommander() && pMR->IsInhabited() && pGameResource->GetNumMarines( NULL, true ) > 3 )
				{
					pMR->m_iAliensKilledSinceLastFriendlyFireIncident++;
					if ( pMR->m_iAliensKilledSinceLastFriendlyFireIncident > 25 && !pMR->m_bAwardedFFPartialAchievement )
					{
						pMR->m_bAwardedFFPartialAchievement = true;
						pMR->GetCommander()->AwardAchievement( ACHIEVEMENT_ASW_KILL_WITHOUT_FRIENDLY_FIRE );
					}
				}

				if ( pMarine->GetDamageBuffEndTime() > gpGlobals->curtime && pMarine->m_hLastBuffGrenade.Get() && pMarine->m_hLastBuffGrenade->GetBuffedMarineCount() >= 4 )
				{
					pGameResource->m_iAliensKilledWithDamageAmp++;

					if ( !pGameResource->m_bAwardedDamageAmpAchievement && pMR->GetCommander() && pMR->IsInhabited() )
					{
						static const int nRequiredDamageAmpKills = 15;
						if ( pGameResource->m_iAliensKilledWithDamageAmp >= nRequiredDamageAmpKills )
						{
							pGameResource->m_bAwardedDamageAmpAchievement = true;
							for ( int i = 1; i <= gpGlobals->maxClients; i++ )	
							{
								CASW_Player* pPlayer = dynamic_cast<CASW_Player*>( UTIL_PlayerByIndex( i ) );
								if ( !pPlayer || !pPlayer->IsConnected() || !pPlayer->GetMarine() )
									continue;

								pPlayer->AwardAchievement( ACHIEVEMENT_ASW_GROUP_DAMAGE_AMP );
								if ( pPlayer->GetMarine()->GetMarineResource() )
								{
									pPlayer->GetMarine()->GetMarineResource()->m_bDamageAmpMedal = true;
								}
							}
						}
					}
				}
			}

			//if (pMarine->m_fDieHardTime > 0)
			//{
				//pMR->m_iLastStandKills++;
			//}

			// count rad volume kills
			if (pAlien && pAlien->Classify() != CLASS_EARTH_FAUNA)
			{
				int nOldBarrelKills = pMR->m_iBarrelKills;
				CASW_Radiation_Volume *pRad = dynamic_cast<CASW_Radiation_Volume*>(info.GetInflictor());
				if (pRad)
				{
					pMR->m_iBarrelKills++;
				}

				// count prop kills
				CPhysicsProp *pProp = dynamic_cast<CPhysicsProp*>(info.GetInflictor());
				if (pProp)
				{
					pMR->m_iBarrelKills++;
				}
				if ( pMR->GetCommander() && pMR->IsInhabited() )
				{
					if ( nOldBarrelKills < asw_medal_barrel_kills.GetInt() && pMR->m_iBarrelKills >= asw_medal_barrel_kills.GetInt() )
					{
						pMR->GetCommander()->AwardAchievement( ACHIEVEMENT_ASW_BARREL_KILLS );
					}
				}
			}

			if ( pMR->m_iSavedLife < asw_medal_lifesaver_kills.GetInt() && pAlien->Classify() == CLASS_ASW_DRONE )
			{
				// check if the alien was after another marine and was close to him
				if (pAlien->GetEnemy() != pMarine && pAlien->GetEnemy()
					&& pAlien->GetEnemy()->Classify() == CLASS_ASW_MARINE
					&& pAlien->GetEnemy()->GetHealth() <= 5
					&& pAlien->GetAbsOrigin().DistTo(pAlien->GetEnemy()->GetAbsOrigin()) < asw_medal_lifesaver_dist.GetFloat())
					pMR->m_iSavedLife++;
			}

			if ( pSB )
			{
				pMR->m_iShieldbugsKilled++;
			}

			if ( pAlien->Classify() == CLASS_ASW_PARASITE )
			{
				CASW_Parasite *pPara = dynamic_cast<CASW_Parasite*>(pAlien);
				if (!pPara->m_bDefanged)
				{
					pMR->m_iParasitesKilled++;
				}
			}
		}

		if (!m_bDoneCrashShieldbugConv)
		{			
			if (pSB && random->RandomFloat() < 1.0f)
			{				
				// see if crash was nearby
				CASW_Game_Resource *pGameResource = ASWGameResource();
				if (pGameResource)
				{
					CASW_Marine *pCrash = NULL;
					if (pMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_CRASH && pMarine->GetHealth() > 0)
						pCrash = pMarine;
					if (!pCrash)
					{
						for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
						{
							CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
							CASW_Marine *pOtherMarine = pMR ? pMR->GetMarineEntity() : NULL;
							if (pOtherMarine && (pMarine->GetAbsOrigin().DistTo(pOtherMarine->GetAbsOrigin()) < 600)
										&& pOtherMarine->GetHealth() > 0 && pOtherMarine->GetMarineProfile()
										&& pOtherMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_CRASH)
							{
								pCrash = pOtherMarine;
								break;
							}
						}
					}
					if (pCrash)
					{
						if (CASW_MarineSpeech::StartConversation(CONV_BIG_ALIEN, pMarine))
						{
							m_bDoneCrashShieldbugConv = true;
							return;
						}
					}
				}
			}
		}
			
		// check for doing an conversation from this kill
		if (pMarine->GetMarineProfile())
		{
			if (gpGlobals->curtime > m_fNextWWKillConv)
			{
				if (pMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_WILDCAT)
				{
					m_fNextWWKillConv = gpGlobals->curtime + random->RandomInt(asw_ww_chatter_interval_min.GetInt(), asw_ww_chatter_interval_max.GetInt());
					if (CASW_MarineSpeech::StartConversation(CONV_WILDCAT_KILL, pMarine))
						return;
				}
				else if (pMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_WOLFE)
				{
					m_fNextWWKillConv = gpGlobals->curtime + random->RandomInt(asw_ww_chatter_interval_min.GetInt(), asw_ww_chatter_interval_max.GetInt());
					if (CASW_MarineSpeech::StartConversation(CONV_WOLFE_KILL, pMarine))
						return;
				}
			}

			if (gpGlobals->curtime > m_fNextCompliment)
			{
				if (m_bSargeAndJaeger && pMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_SARGE)
				{
					CASW_Marine *pOtherMarine = ASWGameResource()->FindMarineByVoiceType(ASW_VOICE_JAEGER);
					if (CASW_MarineSpeech::StartConversation(CONV_COMPLIMENT_SARGE, pOtherMarine))
						m_fNextCompliment = gpGlobals->curtime + random->RandomInt(asw_compliment_chatter_interval_min.GetInt(), asw_compliment_chatter_interval_max.GetInt());
					else
						m_fNextCompliment = gpGlobals->curtime + random->RandomInt(asw_compliment_chatter_interval_min.GetInt()*0.2f, asw_compliment_chatter_interval_max.GetInt()-0.2f);
				}
				else if (m_bSargeAndJaeger && pMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_JAEGER)
				{
					CASW_Marine *pOtherMarine = ASWGameResource()->FindMarineByVoiceType(ASW_VOICE_SARGE);
					if (CASW_MarineSpeech::StartConversation(CONV_COMPLIMENT_JAEGER, pOtherMarine))
						m_fNextCompliment = gpGlobals->curtime + random->RandomInt(asw_compliment_chatter_interval_min.GetInt(), asw_compliment_chatter_interval_max.GetInt());
					else
						m_fNextCompliment = gpGlobals->curtime + random->RandomInt(asw_compliment_chatter_interval_min.GetInt()*0.2f, asw_compliment_chatter_interval_max.GetInt()-0.2f);
				}
				else if (m_bWolfeAndWildcat && pMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_WILDCAT)
				{
					CASW_Marine *pOtherMarine = ASWGameResource()->FindMarineByVoiceType(ASW_VOICE_WOLFE);
					if (CASW_MarineSpeech::StartConversation(CONV_COMPLIMENT_WILDCAT,  pOtherMarine))
						m_fNextCompliment = gpGlobals->curtime + random->RandomInt(asw_compliment_chatter_interval_min.GetInt(), asw_compliment_chatter_interval_max.GetInt());
					else
						m_fNextCompliment = gpGlobals->curtime + random->RandomInt(asw_compliment_chatter_interval_min.GetInt()*0.2f, asw_compliment_chatter_interval_max.GetInt()-0.2f);
				}
				else if (m_bWolfeAndWildcat && pMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_WOLFE)
				{
					CASW_Marine *pOtherMarine = ASWGameResource()->FindMarineByVoiceType(ASW_VOICE_WILDCAT);
					if (CASW_MarineSpeech::StartConversation(CONV_COMPLIMENT_WOLFE, pOtherMarine))
						m_fNextCompliment = gpGlobals->curtime + random->RandomInt(asw_compliment_chatter_interval_min.GetInt(), asw_compliment_chatter_interval_max.GetInt());
					else
						m_fNextCompliment = gpGlobals->curtime + random->RandomInt(asw_compliment_chatter_interval_min.GetInt()*0.2f, asw_compliment_chatter_interval_max.GetInt()-0.2f);
				}
			}
		}
	}

	CASW_Sentry_Top *pSentry = NULL;
	if ( !pMarine )
	{
		pSentry = dynamic_cast< CASW_Sentry_Top* >( info.GetAttacker() );
	}
	
	if ( pSentry )
	{
		// count sentry kills
		if ( pSentry->GetSentryBase() && pSentry->GetSentryBase()->m_hDeployer.Get())
		{
			pMarine = pSentry->GetSentryBase()->m_hDeployer.Get();
			if (pMarine && pMarine->GetMarineResource())
			{
				pMarine->GetMarineResource()->m_iSentryKills++;
			}
		}
	}

	CFire *pFire = NULL;
	if ( !pMarine )
	{
		pFire = dynamic_cast< CFire* >( info.GetAttacker() );
	}

	if ( pFire )
	{
		pMarine = dynamic_cast< CASW_Marine* >( pFire->GetOwner() );
	}

	// send a game event for achievements to use
	IGameEvent *pEvent = gameeventmanager->CreateEvent( "alien_died", false );
	if ( !pEvent )
		return;

	CBaseEntity *pWeapon = NULL;

	if ( pSentry )
	{
		pWeapon = pSentry;
	}
	else if ( pFire )
	{
		pWeapon = pFire;
	}
	else
	{
		pWeapon = info.GetWeapon();
	}
	
	pEvent->SetInt( "alien", pAlien ? pAlien->Classify() : 0 );
	pEvent->SetInt( "marine", pMarine ? pMarine->entindex() : 0 );		
	pEvent->SetInt( "weapon", pWeapon ? pWeapon->Classify() : 0 );

	gameeventmanager->FireEvent( pEvent );
}

#endif /* not CLIENT_DLL */

bool CAlienSwarm::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{	
	// HL2 treats movement and tracing against players the same, so just remap here
	if ( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT )
	{
		collisionGroup0 = COLLISION_GROUP_PLAYER;
	}

	if( collisionGroup1 == COLLISION_GROUP_PLAYER_MOVEMENT )
	{
		collisionGroup1 = COLLISION_GROUP_PLAYER;
	}
	
	// aliens shouldn't walk into players (but will this make them get stuck? - might need some extra push out stuff for em)
	if ((collisionGroup0 == ASW_COLLISION_GROUP_ALIEN || collisionGroup0 == ASW_COLLISION_GROUP_BIG_ALIEN)
		&& collisionGroup1 == COLLISION_GROUP_PLAYER)
	{
		return true;
	}

	// grubs don't collide with one another
	if (collisionGroup0 == ASW_COLLISION_GROUP_GRUBS
		&& collisionGroup1 == ASW_COLLISION_GROUP_GRUBS)
	{
		//Msg("Skipped houndeye col\n");
		return false;
	}

	// asw test, let drones pass through one another
#ifndef CLIENT_DLL
	if ((collisionGroup0 == ASW_COLLISION_GROUP_ALIEN || collisionGroup0 == ASW_COLLISION_GROUP_BIG_ALIEN)
			&& collisionGroup1 == ASW_COLLISION_GROUP_ALIEN && asw_springcol.GetBool())
			return false;
#endif

	if ( collisionGroup0 > collisionGroup1 )
	{
		// swap so that lowest is always first
		int tmp = collisionGroup0;
		collisionGroup0 = collisionGroup1;
		collisionGroup1 = tmp;
	}

	// players don't collide with buzzers (since the buzzers use vphysics collision and that makes the player get stuck)
	if (collisionGroup0 == COLLISION_GROUP_PLAYER && collisionGroup1 == ASW_COLLISION_GROUP_BUZZER)
		return false;

	// this collision group only blocks drones
	if (collisionGroup1 == ASW_COLLISION_GROUP_BLOCK_DRONES)
	{
		if (collisionGroup0 == ASW_COLLISION_GROUP_ALIEN || collisionGroup0 == ASW_COLLISION_GROUP_BIG_ALIEN)
			return true;
		else
			return false;
	}

	// marines don't collide with other marines
	if ( !asw_marine_collision.GetBool() )
	{
		if (collisionGroup0 == COLLISION_GROUP_PLAYER && collisionGroup1 == COLLISION_GROUP_PLAYER)
		{
			return false;
		}
	}

	// eggs and parasites don't collide
	if (collisionGroup1 == ASW_COLLISION_GROUP_EGG
		&& collisionGroup0 == ASW_COLLISION_GROUP_PARASITE)
		return false;
	
	// so our parasites don't stop gibs from flying out of people alongside them
	if (collisionGroup0 == COLLISION_GROUP_DEBRIS
		&& collisionGroup1 == ASW_COLLISION_GROUP_PARASITE)
		return false;

	// parasites don't get blocked by big aliens
	if (collisionGroup0 == ASW_COLLISION_GROUP_PARASITE
		&& collisionGroup1 == ASW_COLLISION_GROUP_BIG_ALIEN)
		return false;

	// turn our prediction collision into normal player collision and pass it up
	if (collisionGroup0 == ASW_COLLISION_GROUP_MARINE_POSITION_PREDICTION)
		collisionGroup0 = COLLISION_GROUP_PLAYER;
	if (collisionGroup1 == ASW_COLLISION_GROUP_MARINE_POSITION_PREDICTION)
		collisionGroup1 = COLLISION_GROUP_PLAYER;

	// grubs don't collide with zombies, aliens or marines
	if (collisionGroup1 == ASW_COLLISION_GROUP_GRUBS &&
		(collisionGroup0 == COLLISION_GROUP_PLAYER ||
		 collisionGroup0 == COLLISION_GROUP_NPC))
		
	{
		return false;
	}
	if (collisionGroup0 == ASW_COLLISION_GROUP_GRUBS)
	{
		if (collisionGroup1 == ASW_COLLISION_GROUP_ALIEN ||
			collisionGroup1 == COLLISION_GROUP_PLAYER ||
			collisionGroup1 == ASW_COLLISION_GROUP_BIG_ALIEN )
		return false;
	}

	if (collisionGroup0 == ASW_COLLISION_GROUP_SHOTGUN_PELLET && collisionGroup1 == ASW_COLLISION_GROUP_SHOTGUN_PELLET )
		return false;

	// the pellets that the flamer shoots.  Doesn not collide with small aliens or marines, DOES collide with doors and shieldbugs
	if (collisionGroup1 == ASW_COLLISION_GROUP_FLAMER_PELLETS)
	{
		if (collisionGroup0 == ASW_COLLISION_GROUP_EGG || collisionGroup0 == ASW_COLLISION_GROUP_PARASITE ||
			collisionGroup0 == ASW_COLLISION_GROUP_ALIEN || collisionGroup0 == COLLISION_GROUP_PLAYER ||
			collisionGroup0 == COLLISION_GROUP_NPC || collisionGroup0 == COLLISION_GROUP_DEBRIS ||
			collisionGroup0 == ASW_COLLISION_GROUP_SHOTGUN_PELLET || collisionGroup0 == ASW_COLLISION_GROUP_FLAMER_PELLETS || 
			collisionGroup0 == ASW_COLLISION_GROUP_SENTRY || collisionGroup0 == ASW_COLLISION_GROUP_SENTRY_PROJECTILE 
			|| collisionGroup0 == ASW_COLLISION_GROUP_IGNORE_NPCS || collisionGroup0 == COLLISION_GROUP_WEAPON )
		{
			return false;
		}
		else
			return true;
	}

	// the pellets that the flamer shoots.  Doesn not collide with small aliens or marines, DOES collide with doors and shieldbugs
	if (collisionGroup1 == ASW_COLLISION_GROUP_EXTINGUISHER_PELLETS)
	{
		if (collisionGroup0 == COLLISION_GROUP_DEBRIS ||
			collisionGroup0 == ASW_COLLISION_GROUP_SHOTGUN_PELLET || collisionGroup0 == ASW_COLLISION_GROUP_FLAMER_PELLETS ||
			collisionGroup0 == ASW_COLLISION_GROUP_SENTRY || collisionGroup0 == ASW_COLLISION_GROUP_SENTRY_PROJECTILE
			|| collisionGroup0 == ASW_COLLISION_GROUP_EXTINGUISHER_PELLETS || collisionGroup0 == COLLISION_GROUP_WEAPON )
		{
			return false;
		}
		else
			return true;
	}

	// fire wall pieces don't get blocked by aliens/marines
	if (collisionGroup1 == ASW_COLLISION_GROUP_IGNORE_NPCS)
	{
		if (collisionGroup0 == ASW_COLLISION_GROUP_EGG || collisionGroup0 == ASW_COLLISION_GROUP_PARASITE ||
			collisionGroup0 == ASW_COLLISION_GROUP_ALIEN || collisionGroup0 == COLLISION_GROUP_PLAYER ||
			collisionGroup0 == COLLISION_GROUP_NPC || collisionGroup0 == COLLISION_GROUP_DEBRIS ||
			collisionGroup0 == ASW_COLLISION_GROUP_SHOTGUN_PELLET 
			|| collisionGroup0 == ASW_COLLISION_GROUP_IGNORE_NPCS || collisionGroup0 == ASW_COLLISION_GROUP_BIG_ALIEN)
		{
			return false;
		}
		else
			return true;
	}

	if (collisionGroup1 == ASW_COLLISION_GROUP_PASSABLE)
		return false;

	// Only let projectile blocking debris collide with grenades
	if ( collisionGroup0 == COLLISION_GROUP_DEBRIS_BLOCK_PROJECTILE && ( collisionGroup1 == ASW_COLLISION_GROUP_GRENADES || collisionGroup1 == ASW_COLLISION_GROUP_NPC_GRENADES ) )
		return true;

	// Grenades hit everything but debris, weapons, other projectiles and marines
	if ( collisionGroup1 == ASW_COLLISION_GROUP_GRENADES )
	{
		if ( collisionGroup0 == COLLISION_GROUP_DEBRIS || 
			collisionGroup0 == COLLISION_GROUP_WEAPON ||
			collisionGroup0 == COLLISION_GROUP_PROJECTILE ||
			collisionGroup0 == ASW_COLLISION_GROUP_GRENADES ||
			collisionGroup0 == COLLISION_GROUP_PLAYER )
		{
			return false;
		}
	}

	// sentries dont collide with marines or their own projectiles
	if ( collisionGroup1 == ASW_COLLISION_GROUP_SENTRY )
	{
		if ( collisionGroup0 == ASW_COLLISION_GROUP_SENTRY_PROJECTILE || 
			collisionGroup0 == ASW_COLLISION_GROUP_PLAYER_MISSILE || 
			collisionGroup0 == ASW_COLLISION_GROUP_SHOTGUN_PELLET || 
			collisionGroup0 == COLLISION_GROUP_PLAYER )
		{
			return false;
		}
	}

	// sentry projectiles only collide with doors, walls and shieldbugs
	if ( collisionGroup1 == ASW_COLLISION_GROUP_SENTRY_PROJECTILE )
	{
		if (collisionGroup0 == ASW_COLLISION_GROUP_EGG || collisionGroup0 == ASW_COLLISION_GROUP_PARASITE ||
			collisionGroup0 == ASW_COLLISION_GROUP_ALIEN || collisionGroup0 == COLLISION_GROUP_PLAYER ||
			collisionGroup0 == COLLISION_GROUP_NPC || collisionGroup0 == COLLISION_GROUP_DEBRIS ||
			collisionGroup0 == ASW_COLLISION_GROUP_SHOTGUN_PELLET ||
			collisionGroup0 == ASW_COLLISION_GROUP_IGNORE_NPCS || collisionGroup0 == ASW_COLLISION_GROUP_SENTRY ||
			collisionGroup0 == COLLISION_GROUP_WEAPON)
		{
			return false;
		}
		else
			return true;
	}

	if ( collisionGroup1 == ASW_COLLISION_GROUP_ALIEN_MISSILE )
	{
		if ( collisionGroup0 == COLLISION_GROUP_NPC ||
			collisionGroup0 == ASW_COLLISION_GROUP_SHOTGUN_PELLET || 
			collisionGroup0 == COLLISION_GROUP_WEAPON ||
			collisionGroup0 == COLLISION_GROUP_PROJECTILE ||
			collisionGroup0 == ASW_COLLISION_GROUP_ALIEN_MISSILE ||
			collisionGroup0 == COLLISION_GROUP_PLAYER )	// NOTE: alien projectiles do not collide with marines using their normal touch functions.  Instead we do lag comped traces
		{
			return false;
		}
	}

	if ( collisionGroup1 == ASW_COLLISION_GROUP_PLAYER_MISSILE )
	{
		if ( collisionGroup0 == ASW_COLLISION_GROUP_PLAYER_MISSILE ||
			collisionGroup0 == ASW_COLLISION_GROUP_SHOTGUN_PELLET || 
			collisionGroup0 == COLLISION_GROUP_DEBRIS || 
			collisionGroup0 == COLLISION_GROUP_WEAPON ||
			collisionGroup0 == COLLISION_GROUP_PROJECTILE ||
			collisionGroup0 == COLLISION_GROUP_PLAYER ||
			collisionGroup0 == ASW_COLLISION_GROUP_ALIEN_MISSILE )
		{
			return false;
		}
	}

	// HL2 collision rules
	//If collisionGroup0 is not a player then NPC_ACTOR behaves just like an NPC.
	if ( collisionGroup1 == COLLISION_GROUP_NPC_ACTOR && collisionGroup0 != COLLISION_GROUP_PLAYER )
	{
		collisionGroup1 = COLLISION_GROUP_NPC;
	}

	// grubs don't collide with each other
	if (collisionGroup0 == ASW_COLLISION_GROUP_GRUBS && collisionGroup1 == ASW_COLLISION_GROUP_GRUBS )
		return false;

	// parasites don't collide with each other (fixes double head jumping and crazy leaps when multiple parasites exit a victim)
	if ( ( collisionGroup0 == ASW_COLLISION_GROUP_PARASITE ) && ( collisionGroup1 == ASW_COLLISION_GROUP_PARASITE ) )
		return false;

	// weapons and NPCs don't collide
	if ( collisionGroup0 == COLLISION_GROUP_WEAPON && (collisionGroup1 >= HL2COLLISION_GROUP_FIRST_NPC && collisionGroup1 <= HL2COLLISION_GROUP_LAST_NPC ) )
		return false;

	//players don't collide against NPC Actors.
	//I could've done this up where I check if collisionGroup0 is NOT a player but I decided to just
	//do what the other checks are doing in this function for consistency sake.
	if ( collisionGroup1 == COLLISION_GROUP_NPC_ACTOR && collisionGroup0 == COLLISION_GROUP_PLAYER )
		return false;

	// In cases where NPCs are playing a script which causes them to interpenetrate while riding on another entity,
	// such as a train or elevator, you need to disable collisions between the actors so the mover can move them.
	if ( collisionGroup0 == COLLISION_GROUP_NPC_SCRIPTED && collisionGroup1 == COLLISION_GROUP_NPC_SCRIPTED )
		return false;

	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 ); 
}

bool CAlienSwarm::MarineCanPickup(CASW_Marine_Resource* pMarineResource, const char* szWeaponClass, const char* szSwappingClass)
{
	if (!ASWEquipmentList() || !pMarineResource)
		return false;
	// need to get the weapon data associated with this class
	CASW_WeaponInfo* pWeaponData = ASWEquipmentList()->GetWeaponDataFor(szWeaponClass);
	if (!pWeaponData)
		return false;

	CASW_Marine_Profile* pProfile = pMarineResource->GetProfile();
	if (!pProfile)
		return false;

	// check various class skills
	if (pWeaponData->m_bTech && !pProfile->CanHack())
	{
		Q_snprintf( m_szPickupDenial, sizeof(m_szPickupDenial), "#asw_requires_tech");
		return false;
	}

	if (pWeaponData->m_bFirstAid && !pProfile->CanUseFirstAid())
	{
		Q_snprintf( m_szPickupDenial, sizeof(m_szPickupDenial), "#asw_requires_medic");
		return false;
	}

	if (pWeaponData->m_bSpecialWeapons && pProfile->GetMarineClass() != MARINE_CLASS_SPECIAL_WEAPONS)
	{
		Q_snprintf( m_szPickupDenial, sizeof(m_szPickupDenial), "#asw_requires_sw");
		return false;
	}

	if (pWeaponData->m_bSapper && pProfile->GetMarineClass() != MARINE_CLASS_NCO)
	{
		Q_snprintf( m_szPickupDenial, sizeof(m_szPickupDenial), "#asw_requires_nco");
		return false;
	}

// 	if (pWeaponData->m_bSarge && !pProfile->m_bSarge)
// 	{
// 		Q_snprintf( m_szPickupDenial, sizeof(m_szPickupDenial), "#asw_sarge_only");
// 		return false;
// 	}

	if (pWeaponData->m_bTracker && !pProfile->CanScanner())
	{
		Q_snprintf( m_szPickupDenial, sizeof(m_szPickupDenial), "TRACKING ONLY");
		return false;
	}	

	if (pWeaponData->m_bUnique)
	{
		// if we're swapping a unique item for the same unique item, allow the pickup
		if (szSwappingClass && !Q_strcmp(szWeaponClass, szSwappingClass))
			return true;
		
		// check if we have one of these already
		// todo: shouldn't use these vars when ingame, but should check the marine's inventory?
		for ( int iWpnSlot = 0; iWpnSlot < ASW_MAX_EQUIP_SLOTS; ++ iWpnSlot )
		{
			CASW_EquipItem* pItem = ASWEquipmentList()->GetItemForSlot( iWpnSlot, pMarineResource->m_iWeaponsInSlots[ iWpnSlot ] );
			if ( !pItem )
				continue;

			const char* szItemClass = STRING(pItem->m_EquipClass);
			if ( !Q_strcmp(szItemClass, szWeaponClass) )
			{
				Q_snprintf( m_szPickupDenial, sizeof(m_szPickupDenial), "#asw_cannot_carry_two");
				return false;
			}
		}
	}

	return true;
}

void CAlienSwarm::CreateStandardEntities( void )
{

#ifndef CLIENT_DLL
	// Create the entity that will send our data to the client.

	BaseClass::CreateStandardEntities();

#ifdef _DEBUG
	CBaseEntity *pEnt = 
#endif
	CBaseEntity::Create( "asw_gamerules", vec3_origin, vec3_angle );
	Assert( pEnt );
#endif
}

// return true if our marine is using a weapon that can autoaim at flares
//  and if the target is inside a flare radius
bool CAlienSwarm::CanFlareAutoaimAt(CASW_Marine* pMarine, CBaseEntity *pEntity)
{
	if (!pMarine || !pEntity || !g_pHeadFlare || !pEntity->IsNPC() )
		return false;

	CASW_Weapon* pWeapon = pMarine->GetActiveASWWeapon();
	if (!pWeapon)
		return false;

	if (!pWeapon->ShouldFlareAutoaim())
		return false;

	// go through all our flares and check if this entity is inside any of them
	CASW_Flare_Projectile* pFlare = g_pHeadFlare;
	float dist = 0;
	Vector diff;
	while (pFlare!=NULL)
	{
		diff = pEntity->GetAbsOrigin() - pFlare->GetAbsOrigin();
		dist = diff.Length();
		if (dist <= asw_flare_autoaim_radius.GetFloat())
			return true;

		pFlare = pFlare->m_pNextFlare;
	}

	return false;
}

// returns 0 if it's a single mission, 1 if it's a campaign game
int CAlienSwarm::IsCampaignGame()
{
	CASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
	{
		//Msg("Warning, IsCampaignGame called without asw game resource!\n");
		return 0;
	}

	return pGameResource->IsCampaignGame();
}

CASW_Campaign_Save* CAlienSwarm::GetCampaignSave()
{
	CASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return NULL;
	return pGameResource->GetCampaignSave();
}

CASW_Campaign_Info* CAlienSwarm::GetCampaignInfo()
{
	CASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return NULL;

	if (IsCampaignGame() != 1)
		return NULL;

	// if the campaign info has previously been setup, then just return that
	if (pGameResource->m_pCampaignInfo)
		return pGameResource->m_pCampaignInfo;

	// will only set up the campaign info if the campaign save is here (should've been created in gamerules constructor (and networked down each client))
	CASW_Campaign_Save *pSave = GetCampaignSave();
	if (!pSave)
		return NULL;
	// our savegame is setup, so we can ask it for the name of our campaign and try to load it
	CASW_Campaign_Info *pCampaignInfo = new CASW_Campaign_Info;
	if (pCampaignInfo)
	{
		if (pCampaignInfo->LoadCampaign(pSave->GetCampaignName()))
		{
			// created and loaded okay, notify the asw game resource that some new marine skills are to be networked about
#ifndef CLIENT_DLL
			if (ASWGameResource())
			{
				ASWGameResource()->UpdateMarineSkills(pSave);
			}
			else
			{
				Msg("Warning: Failed to find game resource after loading campaign game.  Marine skills will be incorrect.\n");
			}
#endif			
			pGameResource->m_pCampaignInfo = pCampaignInfo;

			return pCampaignInfo;
		}
		else
		{
			// failed to load the specified campaign			
			delete pCampaignInfo;
#ifdef CLIENT_DLL
			engine->ClientCmd("disconnect\n");
#else
			engine->ServerCommand("disconnect\n");
#endif
		}
	}

	return NULL;
}


extern bool IsExplosionTraceBlocked( trace_t *ptr );

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Custom version of radius damage that doesn't hurt marines so much and has special properties for burn damage
//-----------------------------------------------------------------------------
#define ROBUST_RADIUS_PROBE_DIST 16.0f // If a solid surface blocks the explosion, this is how far to creep along the surface looking for another way to the target
void CAlienSwarm::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore )
{
	const int MASK_RADIUS_DAMAGE = MASK_SHOT&(~CONTENTS_HITBOX);
	CBaseEntity *pEntity = NULL;
	trace_t		tr;
	float		flAdjustedDamage, falloff;
	Vector		vecSpot;

	Vector vecSrc = vecSrcIn;

	if ( asw_debug_alien_damage.GetBool() )
	{
		NDebugOverlay::Circle( vecSrc, QAngle( -90.0f, 0, 0 ), flRadius, 255, 160, 0, 255, true, 4.0f );
	}

	if ( flRadius )
		falloff = info.GetDamage() / flRadius;
	else
		falloff = 1.0;

	float fMarineRadius = flRadius * asw_marine_explosion_protection.GetFloat();
	float fMarineFalloff = falloff / MAX(0.01f, asw_marine_explosion_protection.GetFloat());	 

	if ( info.GetDamageCustom() & DAMAGE_FLAG_NO_FALLOFF )
	{
		falloff = 0.0f;
		fMarineFalloff = 0.0f;
	}
	else if ( info.GetDamageCustom() & DAMAGE_FLAG_HALF_FALLOFF )
	{
		falloff *= 0.5f;
		fMarineFalloff *= 0.5f;
	}

	int bInWater = (UTIL_PointContents ( vecSrc, MASK_WATER ) & MASK_WATER) ? true : false;

	if( bInWater )
	{
		// Only muffle the explosion if deeper than 2 feet in water.
		if( !(UTIL_PointContents(vecSrc + Vector(0, 0, 24),MASK_WATER) & MASK_WATER) )
		{
			bInWater = false;
		}
	}
	
	vecSrc.z += 1;// in case grenade is lying on the ground

	float flHalfRadiusSqr = Square( flRadius / 2.0f );
	//float flMarineHalfRadiusSqr = flHalfRadiusSqr * asw_marine_explosion_protection.GetFloat();

	// iterate on all entities in the vicinity.
	for ( CEntitySphereQuery sphere( vecSrc, flRadius ); (pEntity = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity() )
	{
		// This value is used to scale damage when the explosion is blocked by some other object.
		float flBlockedDamagePercent = 0.0f;

		if ( pEntity == pEntityIgnore )
			continue;

		if ( pEntity->m_takedamage == DAMAGE_NO )
			continue;

		// UNDONE: this should check a damage mask, not an ignore
		if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
		{// houndeyes don't hurt other houndeyes with their attack
			continue;
		}

		// blast's don't tavel into or out of water
		if (bInWater && pEntity->GetWaterLevel() == 0)
			continue;

		if (!bInWater && pEntity->GetWaterLevel() == 3)
			continue;

		// check if this is a marine and if so, he may be outside the explosion radius				
		if (pEntity->Classify() == CLASS_ASW_MARINE)
		{
			if (( vecSrc - pEntity->WorldSpaceCenter() ).Length() > fMarineRadius)
				continue;
		}

		// Check that the explosion can 'see' this entity.
		vecSpot = pEntity->BodyTarget( vecSrc, false );
		UTIL_TraceLine( vecSrc, vecSpot, MASK_RADIUS_DAMAGE, info.GetInflictor(), COLLISION_GROUP_NONE, &tr );

		if( old_radius_damage.GetBool() )
		{
			if ( tr.fraction != 1.0 && tr.m_pEnt != pEntity )
			continue;
		}
		else
		{
			if ( tr.fraction != 1.0 )
			{
				if ( IsExplosionTraceBlocked(&tr) )
				{
					if( ShouldUseRobustRadiusDamage( pEntity ) )
					{
						if( vecSpot.DistToSqr( vecSrc ) > flHalfRadiusSqr )
						{
							// Only use robust model on a target within one-half of the explosion's radius.
							continue;
						}

						Vector vecToTarget = vecSpot - tr.endpos;
						VectorNormalize( vecToTarget );

						// We're going to deflect the blast along the surface that 
						// interrupted a trace from explosion to this target.
						Vector vecUp, vecDeflect;
						CrossProduct( vecToTarget, tr.plane.normal, vecUp );
						CrossProduct( tr.plane.normal, vecUp, vecDeflect );
						VectorNormalize( vecDeflect );

						// Trace along the surface that intercepted the blast...
						UTIL_TraceLine( tr.endpos, tr.endpos + vecDeflect * ROBUST_RADIUS_PROBE_DIST, MASK_RADIUS_DAMAGE, info.GetInflictor(), COLLISION_GROUP_NONE, &tr );
						//NDebugOverlay::Line( tr.startpos, tr.endpos, 255, 255, 0, false, 10 );

						// ...to see if there's a nearby edge that the explosion would 'spill over' if the blast were fully simulated.
						UTIL_TraceLine( tr.endpos, vecSpot, MASK_RADIUS_DAMAGE, info.GetInflictor(), COLLISION_GROUP_NONE, &tr );
						//NDebugOverlay::Line( tr.startpos, tr.endpos, 255, 0, 0, false, 10 );

						if( tr.fraction != 1.0 && tr.DidHitWorld() )
						{
							// Still can't reach the target.
							continue;
						}
						// else fall through
					}
					else
					{
						continue;
					}
				}

				// UNDONE: Probably shouldn't let children block parents either?  Or maybe those guys should set their owner if they want this behavior?
				if( tr.m_pEnt && tr.m_pEnt != pEntity && tr.m_pEnt->GetOwnerEntity() != pEntity )
				{
					// Some entity was hit by the trace, meaning the explosion does not have clear
					// line of sight to the entity that it's trying to hurt. If the world is also
					// blocking, we do no damage.
					CBaseEntity *pBlockingEntity = tr.m_pEnt;
					//Msg( "%s may be blocked by %s...", pEntity->GetClassname(), pBlockingEntity->GetClassname() );

					UTIL_TraceLine( vecSrc, vecSpot, CONTENTS_SOLID, info.GetInflictor(), COLLISION_GROUP_NONE, &tr );

					if( tr.fraction != 1.0 )
					{
						continue;
					}
					
					// asw - don't let npcs reduce the damage from explosions
					if (!pBlockingEntity->IsNPC())
					{	
						// Now, if the interposing object is physics, block some explosion force based on its mass.
						if( pBlockingEntity->VPhysicsGetObject() )
						{
							const float MASS_ABSORB_ALL_DAMAGE = 350.0f;
							float flMass = pBlockingEntity->VPhysicsGetObject()->GetMass();
							float scale = flMass / MASS_ABSORB_ALL_DAMAGE;

							// Absorbed all the damage.
							if( scale >= 1.0f )
							{
								continue;
							}

							ASSERT( scale > 0.0f );
							flBlockedDamagePercent = scale;
							//Msg("Object (%s) weighing %fkg blocked %f percent of explosion damage\n", pBlockingEntity->GetClassname(), flMass, scale * 100.0f);
						}
						else
						{
							// Some object that's not the world and not physics. Generically block 25% damage
							flBlockedDamagePercent = 0.25f;
						}
					}
				}
			}
		}
		// decrease damage for marines
		if (pEntity->Classify() == CLASS_ASW_MARINE)
			flAdjustedDamage = ( vecSrc - tr.endpos ).Length() * fMarineFalloff;
		else
			flAdjustedDamage = ( vecSrc - tr.endpos ).Length() * falloff;
		flAdjustedDamage = info.GetDamage() - flAdjustedDamage;

		if ( flAdjustedDamage <= 0 )
			continue;

		// the explosion can 'see' this entity, so hurt them!
		if (tr.startsolid)
		{
			// if we're stuck inside them, fixup the position and distance
			tr.endpos = vecSrc;
			tr.fraction = 0.0;
		}

		// make explosions hurt asw_doors more
		if (FClassnameIs(pEntity, "asw_door"))
			flAdjustedDamage *= asw_door_explosion_boost.GetFloat();
		
		CTakeDamageInfo adjustedInfo = info;
		//Msg("%s: Blocked damage: %f percent (in:%f  out:%f)\n", pEntity->GetClassname(), flBlockedDamagePercent * 100, flAdjustedDamage, flAdjustedDamage - (flAdjustedDamage * flBlockedDamagePercent) );
		adjustedInfo.SetDamage( flAdjustedDamage - (flAdjustedDamage * flBlockedDamagePercent) );

		// Now make a consideration for skill level!
		if( info.GetAttacker() && info.GetAttacker()->IsPlayer() && pEntity->IsNPC() )
		{
			// An explosion set off by the player is harming an NPC. Adjust damage accordingly.
			adjustedInfo.AdjustPlayerDamageInflictedForSkillLevel();
		}

		// asw - if this is burn damage, don't kill the target, let him burn for a bit
		if ((adjustedInfo.GetDamageType() & DMG_BURN) && adjustedInfo.GetDamage() > 3)
		{
			if (adjustedInfo.GetDamage() > pEntity->GetHealth())
			{
				int newDamage = pEntity->GetHealth() - random->RandomInt(8, 23);
				if (newDamage <= 3)
					newDamage = 3;
				adjustedInfo.SetDamage(newDamage);
			}

			// check if this damage is coming from an incendiary grenade that might need to collect stats
			CASW_Grenade_Vindicator *pGrenade = dynamic_cast<CASW_Grenade_Vindicator*>(adjustedInfo.GetInflictor());
			if (pGrenade)
			{
				pGrenade->BurntAlien(pEntity);
			}
		}

		Vector dir = vecSpot - vecSrc;
		VectorNormalize( dir );

		// If we don't have a damage force, manufacture one
		if ( adjustedInfo.GetDamagePosition() == vec3_origin || adjustedInfo.GetDamageForce() == vec3_origin )
		{
			CalculateExplosiveDamageForce( &adjustedInfo, dir, vecSrc );
		}
		else
		{
			// Assume the force passed in is the maximum force. Decay it based on falloff.
			float flForce = adjustedInfo.GetDamageForce().Length() * falloff;
			adjustedInfo.SetDamageForce( dir * flForce );
			adjustedInfo.SetDamagePosition( vecSrc );
		}

		if ( tr.fraction != 1.0 && pEntity == tr.m_pEnt )
		{
			ClearMultiDamage( );
			pEntity->DispatchTraceAttack( adjustedInfo, dir, &tr );
			ApplyMultiDamage();
		}
		else
		{
			pEntity->TakeDamage( adjustedInfo );
		}

		if ( asw_debug_alien_damage.GetBool() )
		{
			Msg( "Explosion did %f damage to %d:%s\n", adjustedInfo.GetDamage(), pEntity->entindex(), pEntity->GetClassname() );
			NDebugOverlay::Line( vecSrc, pEntity->WorldSpaceCenter(), 255, 255, 0, false, 4 );			
			NDebugOverlay::EntityText( pEntity->entindex(), 0, CFmtStr("%d", (int) adjustedInfo.GetDamage() ), 4.0, 255, 255, 255, 255 );
		}

		// Now hit all triggers along the way that respond to damage... 
		pEntity->TraceAttackToTriggers( adjustedInfo, vecSrc, tr.endpos, dir );
	}
}

ConVar asw_stumble_knockback( "asw_stumble_knockback", "300", FCVAR_CHEAT, "Velocity given to aliens that get knocked back" );
ConVar asw_stumble_lift( "asw_stumble_lift", "300", FCVAR_CHEAT, "Upwards velocity given to aliens that get knocked back" );

void CAlienSwarm::StumbleAliensInRadius( CBaseEntity *pInflictor, const Vector &vecSrcIn, float flRadius )
{
	const int MASK_RADIUS_DAMAGE = MASK_SHOT&(~CONTENTS_HITBOX);
	CBaseEntity *pEntity = NULL;
	trace_t		tr;
	Vector		vecSpot;

	Vector vecSrc = vecSrcIn;

	vecSrc.z += 1;// in case grenade is lying on the ground

	float flHalfRadiusSqr = Square( flRadius / 2.0f );
	//float flMarineHalfRadiusSqr = flHalfRadiusSqr * asw_marine_explosion_protection.GetFloat();

	// iterate on all entities in the vicinity.
	for ( CEntitySphereQuery sphere( vecSrc, flRadius ); (pEntity = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity() )
	{
		if ( pEntity->m_takedamage == DAMAGE_NO )
			continue;

		if ( !pEntity->IsNPC() )
			continue;

		// don't stumble marines
		if (pEntity->Classify() == CLASS_ASW_MARINE )
		{
			continue;
		}

		CASW_Alien *pAlien = dynamic_cast< CASW_Alien* >( pEntity );
		if ( !pAlien )
			continue;

		// Check that the explosion can 'see' this entity.
		vecSpot = pEntity->BodyTarget( vecSrc, false );
		UTIL_TraceLine( vecSrc, vecSpot, MASK_RADIUS_DAMAGE, pInflictor, COLLISION_GROUP_NONE, &tr );


		if ( tr.fraction != 1.0 )
		{
			if ( IsExplosionTraceBlocked(&tr) )
			{
				if( ShouldUseRobustRadiusDamage( pEntity ) )
				{
					if( vecSpot.DistToSqr( vecSrc ) > flHalfRadiusSqr )
					{
						// Only use robust model on a target within one-half of the explosion's radius.
						continue;
					}

					Vector vecToTarget = vecSpot - tr.endpos;
					VectorNormalize( vecToTarget );

					// We're going to deflect the blast along the surface that 
					// interrupted a trace from explosion to this target.
					Vector vecUp, vecDeflect;
					CrossProduct( vecToTarget, tr.plane.normal, vecUp );
					CrossProduct( tr.plane.normal, vecUp, vecDeflect );
					VectorNormalize( vecDeflect );

					// Trace along the surface that intercepted the blast...
					UTIL_TraceLine( tr.endpos, tr.endpos + vecDeflect * ROBUST_RADIUS_PROBE_DIST, MASK_RADIUS_DAMAGE, pInflictor, COLLISION_GROUP_NONE, &tr );
					//NDebugOverlay::Line( tr.startpos, tr.endpos, 255, 255, 0, false, 10 );

					// ...to see if there's a nearby edge that the explosion would 'spill over' if the blast were fully simulated.
					UTIL_TraceLine( tr.endpos, vecSpot, MASK_RADIUS_DAMAGE, pInflictor, COLLISION_GROUP_NONE, &tr );
					//NDebugOverlay::Line( tr.startpos, tr.endpos, 255, 0, 0, false, 10 );

					if( tr.fraction != 1.0 && tr.DidHitWorld() )
					{
						// Still can't reach the target.
						continue;
					}
					// else fall through
				}
				else
				{
					continue;
				}
			}

			// UNDONE: Probably shouldn't let children block parents either?  Or maybe those guys should set their owner if they want this behavior?
			if( tr.m_pEnt && tr.m_pEnt != pEntity && tr.m_pEnt->GetOwnerEntity() != pEntity )
			{
				// Some entity was hit by the trace, meaning the explosion does not have clear
				// line of sight to the entity that it's trying to hurt. If the world is also
				// blocking, we do no damage.
				//CBaseEntity *pBlockingEntity = tr.m_pEnt;
				//Msg( "%s may be blocked by %s...", pEntity->GetClassname(), pBlockingEntity->GetClassname() );

				UTIL_TraceLine( vecSrc, vecSpot, CONTENTS_SOLID, pInflictor, COLLISION_GROUP_NONE, &tr );

				if( tr.fraction != 1.0 )
				{
					continue;
				}
			}
		}

		Vector vecToTarget = pAlien->WorldSpaceCenter() - pInflictor->WorldSpaceCenter();
		vecToTarget.z = 0;
		VectorNormalize( vecToTarget );
		pAlien->Knockback( vecToTarget * asw_stumble_knockback.GetFloat() + Vector( 0, 0, 1 ) * asw_stumble_lift.GetFloat() );
		pAlien->ForceFlinch( vecSrc );
	}
}

void CAlienSwarm::ShockNearbyAliens( CASW_Marine *pMarine, CASW_Weapon *pWeaponSource )
{
	if ( !pMarine )
		return;

	const float flRadius = 160.0f;
	const float flRadiusSqr = flRadius * flRadius;

	// debug stun radius
	//NDebugOverlay::Circle( GetAbsOrigin() + Vector( 0, 0, 1.0f ), QAngle( -90.0f, 0, 0 ), flRadius, 255, 0, 0, 0, true, 5.0f );

	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	int nAIs = g_AI_Manager.NumAIs();

	for ( int i = 0; i < nAIs; i++ )
	{
		CAI_BaseNPC *pNPC = ppAIs[ i ];

		if( !pNPC->IsAlive() )
			continue;

		// ignore hidden objects
		if ( pNPC->IsEffectActive( EF_NODRAW ) )
			continue;

		// Disregard things that want to be disregarded
		if( pNPC->Classify() == CLASS_NONE )
			continue; 

		// Disregard bullseyes
		if( pNPC->Classify() == CLASS_BULLSEYE )
			continue;

		// ignore marines
		if( pNPC->Classify() == CLASS_ASW_MARINE || pNPC->Classify() == CLASS_ASW_COLONIST )
			continue;

		float flDist = (pMarine->GetAbsOrigin() - pNPC->GetAbsOrigin()).LengthSqr();
		if( flDist > flRadiusSqr )
			continue;

		CRecipientFilter filter;
		filter.AddAllPlayers();
		UserMessageBegin( filter, "ASWEnemyZappedByThorns" );
		WRITE_FLOAT( pMarine->entindex() );
		WRITE_SHORT( pNPC->entindex() );
		MessageEnd();

		ClearMultiDamage();	
		CTakeDamageInfo shockDmgInfo( pWeaponSource, pMarine, 5.0f, DMG_SHOCK );					
		Vector vecDir = pNPC->WorldSpaceCenter() - pMarine->WorldSpaceCenter();
		VectorNormalize( vecDir );
		shockDmgInfo.SetDamagePosition( pNPC->WorldSpaceCenter() * vecDir * -20.0f );
		shockDmgInfo.SetDamageForce( vecDir );
		shockDmgInfo.ScaleDamageForce( 1.0 );
		shockDmgInfo.SetWeapon( pWeaponSource );

		trace_t tr;
		UTIL_TraceLine( pMarine->WorldSpaceCenter(), pNPC->WorldSpaceCenter(), MASK_SHOT, pMarine, COLLISION_GROUP_NONE, &tr );
		pNPC->DispatchTraceAttack( shockDmgInfo, vecDir, &tr );
		ApplyMultiDamage();
	}
}

void CAlienSwarm::FreezeAliensInRadius( CBaseEntity *pInflictor, float flFreezeAmount, const Vector &vecSrcIn, float flRadius )
{
	const int MASK_RADIUS_DAMAGE = MASK_SHOT&(~CONTENTS_HITBOX);
	CBaseEntity *pEntity = NULL;
	trace_t		tr;
	Vector		vecSpot;
	int nFrozen = 0;
	Vector vecSrc = vecSrcIn;

	vecSrc.z += 1;// in case grenade is lying on the ground

	float flHalfRadiusSqr = Square( flRadius / 2.0f );
	//float flMarineHalfRadiusSqr = flHalfRadiusSqr * asw_marine_explosion_protection.GetFloat();

	// iterate on all entities in the vicinity.
	for ( CEntitySphereQuery sphere( vecSrc, flRadius ); (pEntity = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity() )
	{
		if ( pEntity->m_takedamage == DAMAGE_NO )
			continue;

		if ( !pEntity->IsNPC() )
			continue;

		// don't stumble marines
		if ( pEntity->Classify() == CLASS_ASW_MARINE )
		{
#ifdef GAME_DLL		
			// but, do extinguish them if they are on fire
			CBaseAnimating *pAnim = assert_cast<CBaseAnimating *>(pEntity);
			if ( pAnim->IsOnFire() )
			{
				CEntityFlame *pFireChild = dynamic_cast<CEntityFlame *>( pAnim->GetEffectEntity() );
				if ( pFireChild )
				{
					pAnim->SetEffectEntity( NULL );
					UTIL_Remove( pFireChild );	
				}			
				pAnim->Extinguish();
			}
#endif
			continue;
		}

		CASW_Alien *pAlien = dynamic_cast< CASW_Alien* >( pEntity );
		if ( !pAlien )
			continue;

		// Check that the explosion can 'see' this entity.
		vecSpot = pEntity->BodyTarget( vecSrc, false );
		UTIL_TraceLine( vecSrc, vecSpot, MASK_RADIUS_DAMAGE, pInflictor, COLLISION_GROUP_NONE, &tr );


		if ( tr.fraction != 1.0 )
		{
			if ( IsExplosionTraceBlocked(&tr) )
			{
				if( ShouldUseRobustRadiusDamage( pEntity ) )
				{
					if( vecSpot.DistToSqr( vecSrc ) > flHalfRadiusSqr )
					{
						// Only use robust model on a target within one-half of the explosion's radius.
						continue;
					}

					Vector vecToTarget = vecSpot - tr.endpos;
					VectorNormalize( vecToTarget );

					// We're going to deflect the blast along the surface that 
					// interrupted a trace from explosion to this target.
					Vector vecUp, vecDeflect;
					CrossProduct( vecToTarget, tr.plane.normal, vecUp );
					CrossProduct( tr.plane.normal, vecUp, vecDeflect );
					VectorNormalize( vecDeflect );

					// Trace along the surface that intercepted the blast...
					UTIL_TraceLine( tr.endpos, tr.endpos + vecDeflect * ROBUST_RADIUS_PROBE_DIST, MASK_RADIUS_DAMAGE, pInflictor, COLLISION_GROUP_NONE, &tr );
					//NDebugOverlay::Line( tr.startpos, tr.endpos, 255, 255, 0, false, 10 );

					// ...to see if there's a nearby edge that the explosion would 'spill over' if the blast were fully simulated.
					UTIL_TraceLine( tr.endpos, vecSpot, MASK_RADIUS_DAMAGE, pInflictor, COLLISION_GROUP_NONE, &tr );
					//NDebugOverlay::Line( tr.startpos, tr.endpos, 255, 0, 0, false, 10 );

					if( tr.fraction != 1.0 && tr.DidHitWorld() )
					{
						// Still can't reach the target.
						continue;
					}
					// else fall through
				}
				else
				{
					continue;
				}
			}

			// UNDONE: Probably shouldn't let children block parents either?  Or maybe those guys should set their owner if they want this behavior?
			if( tr.m_pEnt && tr.m_pEnt != pEntity && tr.m_pEnt->GetOwnerEntity() != pEntity )
			{
				// Some entity was hit by the trace, meaning the explosion does not have clear
				// line of sight to the entity that it's trying to hurt. If the world is also
				// blocking, we do no damage.
				//CBaseEntity *pBlockingEntity = tr.m_pEnt;
				//Msg( "%s may be blocked by %s...", pEntity->GetClassname(), pBlockingEntity->GetClassname() );

				UTIL_TraceLine( vecSrc, vecSpot, CONTENTS_SOLID, pInflictor, COLLISION_GROUP_NONE, &tr );

				if( tr.fraction != 1.0 )
				{
					continue;
				}
			}
		}
#ifdef GAME_DLL
		CBaseAnimating *pAnim = pAlien;
		if ( pAnim->IsOnFire() )
		{
			CEntityFlame *pFireChild = dynamic_cast<CEntityFlame *>( pAnim->GetEffectEntity() );
			if ( pFireChild )
			{
				pAnim->SetEffectEntity( NULL );
				UTIL_Remove( pFireChild );	
			}			
			pAnim->Extinguish();
		}

		pAlien->Freeze( flFreezeAmount, pInflictor, NULL );
		nFrozen++;
#endif
	}
#ifdef GAME_DLL
	if ( nFrozen >= 6 )
	{
		CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>( pInflictor );
		if ( pMarine && pMarine->IsInhabited() && pMarine->GetCommander() )
		{
			pMarine->GetCommander()->AwardAchievement( ACHIEVEMENT_ASW_FREEZE_GRENADE );
			if ( pMarine->GetMarineResource() )
			{
				pMarine->GetMarineResource()->m_bFreezeGrenadeMedal = true;
			}
		}
	}
#endif
}

void CAlienSwarm::ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues )
{
#ifdef GAME_DLL

	CASW_Player *pPlayer = ( CASW_Player * )CBaseEntity::Instance( pEntity );
	if ( !pPlayer )
		return;

	char const *szCommand = pKeyValues->GetName();

	if ( FStrEq( szCommand, "XPUpdate" ) )
	{
		pPlayer->SetNetworkedExperience( pKeyValues->GetInt( "xp" ) );
		pPlayer->SetNetworkedPromotion( pKeyValues->GetInt( "pro" ) );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: called each time a player uses a "cmd" command
// Input  : *pEdict - the player who issued the command
//			Use engine.Cmd_Argv,  engine.Cmd_Argv, and engine.Cmd_Argc to get 
//			pointers the character string command.
//-----------------------------------------------------------------------------
bool CAlienSwarm::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
{
	if( BaseClass::ClientCommand( pEdict, args ) )
		return true;

	CASW_Player *pPlayer = (CASW_Player *) pEdict;

	if ( pPlayer->ClientCommand( args ) )
		return true;

	const char *pcmd = args[0];
	if ( FStrEq( pcmd, "achievement_earned" ) )
	{
		CASW_Player *pPlayer = static_cast<CASW_Player*>( pEdict );
		if ( pPlayer && pPlayer->ShouldAnnounceAchievement() )
		{
			// let's check this came from the client .dll and not the console
			unsigned short mask = UTIL_GetAchievementEventMask();
			int iPlayerID = pPlayer->GetUserID();

			int iAchievement = atoi( args[1] ) ^ mask;
			int code = ( iPlayerID ^ iAchievement ) ^ mask;

			if ( code == atoi( args[2] ) )
			{
				IGameEvent * event = gameeventmanager->CreateEvent( "achievement_earned" );
				if ( event )
				{
					event->SetInt( "player", pEdict->entindex() );
					event->SetInt( "achievement", iAchievement );
					gameeventmanager->FireEvent( event );
				}

				pPlayer->OnAchievementEarned( iAchievement );

				CASW_Marine_Resource *pMR = NULL;
				if ( pPlayer->GetMarine() )
				{
					pMR = pPlayer->GetMarine()->GetMarineResource();
				}
				if ( !pMR )
				{
					pMR = ASWGameResource()->GetFirstMarineResourceForPlayer( pPlayer );
				}
				
				if ( pMR )
				{
					pMR->m_aAchievementsEarned.AddToTail( iAchievement );

					if ( pMR->m_bAwardedMedals )
					{
						// already earned medals, i.e. this achievement was earned during the debrief.
						//  need to update the medal string with the new achievement

						bool bHasMedal = false;
						for ( int i = 0; i < pMR->m_CharMedals.Count(); i++ )
						{
							if ( MedalMatchesAchievement( pMR->m_CharMedals[i], iAchievement ) )
							{
								bHasMedal = true;
								break;
							}
						}
						
						if ( !bHasMedal )
						{
							char achievement_buffer[ 255 ];
							achievement_buffer[0] = 0;

							if ( pMR->m_MedalsAwarded.Get()[0] )
							{
								Q_snprintf( achievement_buffer, sizeof( achievement_buffer ), "%s %d", pMR->m_MedalsAwarded.Get(), -iAchievement );
							}
							else
							{
								Q_snprintf( achievement_buffer, sizeof( achievement_buffer ), "%d", -iAchievement );
							}
							Q_snprintf( pMR->m_MedalsAwarded.GetForModify(), 255, "%s", achievement_buffer );
						}
					}
				}
			}
		}

		return true;
	}

	return false;
}

#endif // #ifndef CLIENT_DLL

bool CAlienSwarm::CanSpendPoint(CASW_Player *pPlayer, int iProfileIndex, int nSkillSlot)
{
	if (!IsCampaignGame() || !ASWGameResource() || !pPlayer)
	{
		//Msg("returning false cos this isn't campaign\n");
		return false;
	}

	CASW_Game_Resource *pGameResource = ASWGameResource();
	// only allow spending if we have the marine selected in multiplayer
	CASW_Marine_Profile *pProfile = NULL;

	if ( pGameResource->IsRosterSelected(iProfileIndex) || IsOfflineGame() )
	{
		bool bSelectedByMe = false;
		for (int i=0; i<pGameResource->GetMaxMarineResources();i++)
		{
			CASW_Marine_Resource *pMarineResource = pGameResource->GetMarineResource(i);
			if (pMarineResource && pMarineResource->GetProfile()->m_ProfileIndex == iProfileIndex)
			{
				pProfile = pMarineResource->GetProfile();
				bSelectedByMe = (pMarineResource->GetCommander() == pPlayer);
				break;
			}
		}
		if (!bSelectedByMe && !IsOfflineGame() )
		{
			//Msg("Returning false because this is multiplayer and he's not selected by me\n");
			return false;
		}
	}
	else
	{
		//Msg("returning false because marine isn't selected and this is multiplayer\n");
		return false;
	}

	if (!pProfile && MarineProfileList())
	{		
		pProfile = MarineProfileList()->GetProfile(iProfileIndex);
	}

	// check the marine isn't dead
	CASW_Campaign_Save *pSave = pGameResource->GetCampaignSave();
	if (!pSave || !pSave->IsMarineAlive(iProfileIndex) || !pProfile)
	{
		//Msg("returning false cos this isn't campaign: save = %d marinealiev=%d pProfile=%d index=%d\n",
			//pSave, pSave ? pSave->IsMarineAlive(iProfileIndex) : 0, pProfile, iProfileIndex );
		return false;
	}
	
	int iCurrentSkillValue = ASWGameResource()->GetMarineSkill( iProfileIndex, nSkillSlot );
	int iMaxSkillValue = MarineSkills()->GetMaxSkillPoints( pProfile->GetSkillMapping( nSkillSlot ) );
	int iSparePoints = ASWGameResource()->GetMarineSkill( iProfileIndex, ASW_SKILL_SLOT_SPARE );
	
	//Msg("returning comparison\n");
	return ((iCurrentSkillValue < iMaxSkillValue) && (iSparePoints > 0));
}

#ifndef CLIENT_DLL
bool CAlienSwarm::SpendSkill(int iProfileIndex, int nSkillSlot)
{
	if (iProfileIndex < 0 || iProfileIndex >= ASW_NUM_MARINE_PROFILES )
		return false;

	if (nSkillSlot < 0 || nSkillSlot >= ASW_SKILL_SLOT_SPARE)		// -1 since the last skill is 'spare' points
		return false;

	if (!ASWGameResource() || !IsCampaignGame())
		return false;

	CASW_Marine_Profile *pProfile = MarineProfileList()->m_Profiles[iProfileIndex];
	if (!pProfile)
		return false;

	int iCurrentSkillValue = ASWGameResource()->GetMarineSkill(iProfileIndex, nSkillSlot);
	int iMaxSkillValue = MarineSkills()->GetMaxSkillPoints( pProfile->GetSkillMapping( nSkillSlot ) );
	int iSparePoints = ASWGameResource()->GetMarineSkill(iProfileIndex, ASW_SKILL_SLOT_SPARE);

	// skill is maxed out
	if (iCurrentSkillValue >= iMaxSkillValue)
		return false;

	// check we have some spare points
	if (iSparePoints <= 0)
		return false;

	// grab our current campaign save game
	CASW_Campaign_Save *pSave = ASWGameResource()->GetCampaignSave();
	if (!pSave)
		return false;

	// don't spend points on dead marines
	if (!pSave->IsMarineAlive(iProfileIndex))
		return false;

	// spend the point
	pSave->IncreaseMarineSkill(iProfileIndex, nSkillSlot);
	pSave->ReduceMarineSkill(iProfileIndex, ASW_SKILL_SLOT_SPARE);
	ASWGameResource()->UpdateMarineSkills(pSave);
	pSave->SaveGameToFile();	// save with the new stats

	// trigger an animation for anyone to see	
	CReliableBroadcastRecipientFilter users;
	users.MakeReliable();
	UserMessageBegin( users, "ASWSkillSpent" );
		WRITE_BYTE( iProfileIndex );
		WRITE_BYTE( nSkillSlot );
	MessageEnd();

	return true;
}

bool CAlienSwarm::SkillsUndo(CASW_Player *pPlayer, int iProfileIndex)
{
	if (!pPlayer)
		return false;

	if (!ASWGameResource() || !IsCampaignGame())
		return false;

	if (iProfileIndex < 0 || iProfileIndex >= ASW_NUM_MARINE_PROFILES )
		return false;

	if (GetGameState() != ASW_GS_BRIEFING)
		return false;

	// grab our current campaign save game
	CASW_Campaign_Save *pSave = ASWGameResource()->GetCampaignSave();
	if (!pSave)
		return false;

	// don't undo if marine is dead
	if (!ASWGameResource()->GetCampaignSave()->IsMarineAlive(iProfileIndex))
		return false;

	bool bSinglePlayer = ASWGameResource()->IsOfflineGame();

	// check we have this marine selected
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (pGameResource->IsRosterSelected(iProfileIndex))
	{		
		bool bSelectedByMe = false;
		for (int i=0; i<pGameResource->GetMaxMarineResources();i++)
		{
			CASW_Marine_Resource *pMarineResource = pGameResource->GetMarineResource(i);
			if (pMarineResource && pMarineResource->GetProfile()->m_ProfileIndex == iProfileIndex)
			{
				bSelectedByMe = (pMarineResource->GetCommander() == pPlayer);
				break;                                     
			}
		}
		if (!bSelectedByMe)
			return false;
	}
	else if (!bSinglePlayer)	// has to be selected in multiplayer to manipulate skills
		return false;


	// revert skills
	pSave->RevertSkillsToUndoState(iProfileIndex);
	ASWGameResource()->UpdateMarineSkills(pSave);
	pSave->SaveGameToFile();	// save with the new stats
	return true;
}

void CAlienSwarm::OnSkillLevelChanged( int iNewLevel )
{
	Msg("Skill level changed\n");
	if ( !( GetGameState() == ASW_GS_BRIEFING || GetGameState() == ASW_GS_DEBRIEF ) )
	{
		m_bCheated = true;
	}

	const char *szDifficulty = "normal";
	if (iNewLevel == 1)	//  easy
	{
		m_iMissionDifficulty = 3;
		szDifficulty = "easy";
	}
	else if (iNewLevel == 3) // hard
	{
		m_iMissionDifficulty = 7;
		szDifficulty = "hard";
	}
	else if (iNewLevel == 4) // insane
	{
		m_iMissionDifficulty = 10;
		szDifficulty = "insane";
	}
	else if (iNewLevel == 5) // imba
	{
		m_iMissionDifficulty = 13;
		szDifficulty = "imba";
	}
	else  // normal
	{
		m_iMissionDifficulty = 5;
	}

	// modify mission difficulty by campaign modifier
	if ( IsCampaignGame() )
	{				
		if ( GetCampaignInfo() && GetCampaignSave() && !GetCampaignSave()->UsingFixedSkillPoints() )
		{
			int iCurrentLoc = GetCampaignSave()->m_iCurrentPosition;
			CASW_Campaign_Info::CASW_Campaign_Mission_t* mission = GetCampaignInfo()->GetMission(iCurrentLoc);
			if (mission)
			{
				m_iMissionDifficulty += mission->m_iDifficultyMod;
			}
		}
	}

	// reduce difficulty by 1 for each missing marine
	if ( ASWGameResource() && asw_adjust_difficulty_by_number_of_marines.GetBool() )
	{
		int nMarines = ASWGameResource()->GetNumMarines( NULL, false );
		if ( nMarines == 3 )
		{
			m_iMissionDifficulty--;
		}
		else if ( nMarines <= 2 )
		{
			m_iMissionDifficulty -= 2;
		}
	}
	// make sure difficulty doesn't go too low
	m_iMissionDifficulty = MAX( m_iMissionDifficulty, 2 );

	// modify health of all live aliens
	if ( ASWSpawnManager() )
	{
		for ( int i = 0; i < ASWSpawnManager()->GetNumAlienClasses(); i++ )
		{
			FindAndModifyAlienHealth( ASWSpawnManager()->GetAlienClass( i )->m_pszAlienClass );
		}
	}

	if ( gameeventmanager )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "difficulty_changed" );
		if ( event )
		{
			event->SetInt( "newDifficulty", iNewLevel );
			event->SetInt( "oldDifficulty", m_iSkillLevel );
			event->SetString( "strDifficulty", szDifficulty );
			gameeventmanager->FireEvent( event );
		}
	}

	UpdateMatchmakingTagsCallback( NULL, "0", 0.0f );
		
	m_iSkillLevel = iNewLevel;
}

void CAlienSwarm::FindAndModifyAlienHealth(const char *szClass)
{
	if ( !szClass || szClass[0] == 0 )
		return;

	CBaseEntity* pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, szClass )) != NULL)
	{
		IASW_Spawnable_NPC* pNPC = dynamic_cast<IASW_Spawnable_NPC*>(pEntity);			
		if (pNPC)
		{
			pNPC->SetHealthByDifficultyLevel();
		}
	}
}

void CAlienSwarm::RequestSkill( CASW_Player *pPlayer, int nSkill )
{
	if ( !( m_iGameState == ASW_GS_BRIEFING || m_iGameState == ASW_GS_DEBRIEF ) )	// don't allow skill change outside of briefing
		return;

	if ( nSkill >= 1 && nSkill <= 5 && ASWGameResource() && ASWGameResource()->GetLeader() == pPlayer )
	{
		ConVar *var = (ConVar *)cvar->FindVar( "asw_skill" );
		if (var)
		{
			int iOldSkill = var->GetInt();

			var->SetValue( nSkill );

			if ( iOldSkill != var->GetInt() )
			{
				CReliableBroadcastRecipientFilter filter;
				filter.RemoveRecipient( pPlayer );		// notify everyone except the player changing the difficulty level
				switch(var->GetInt())
				{
				case 1: UTIL_ClientPrintFilter( filter, ASW_HUD_PRINTTALKANDCONSOLE, "#asw_set_difficulty_easy", pPlayer->GetPlayerName() ); break;
				case 2: UTIL_ClientPrintFilter( filter, ASW_HUD_PRINTTALKANDCONSOLE, "#asw_set_difficulty_normal", pPlayer->GetPlayerName() ); break;
				case 3: UTIL_ClientPrintFilter( filter, ASW_HUD_PRINTTALKANDCONSOLE, "#asw_set_difficulty_hard", pPlayer->GetPlayerName() ); break;
				case 4: UTIL_ClientPrintFilter( filter, ASW_HUD_PRINTTALKANDCONSOLE, "#asw_set_difficulty_insane", pPlayer->GetPlayerName() ); break;
				case 5: UTIL_ClientPrintFilter( filter, ASW_HUD_PRINTTALKANDCONSOLE, "#asw_set_difficulty_imba", pPlayer->GetPlayerName() ); break;
				}
			}
		}
	}
}

void CAlienSwarm::RequestSkillUp(CASW_Player *pPlayer)
{
	if (m_iGameState != ASW_GS_BRIEFING)	// don't allow skill change outside of briefing
		return;

	if (m_iSkillLevel < 5 && ASWGameResource() && ASWGameResource()->GetLeader() == pPlayer)
	{
		ConVar *var = (ConVar *)cvar->FindVar( "asw_skill" );
		if (var)
		{
			var->SetValue(m_iSkillLevel + 1);
		}
	}
		//SetSkillLevel(m_iSkillLevel + 1);
}

void CAlienSwarm::RequestSkillDown(CASW_Player *pPlayer)
{
	if (m_iGameState != ASW_GS_BRIEFING)	// don't allow skill change outside of briefing
		return;

	if (m_iSkillLevel > 1 && ASWGameResource() && ASWGameResource()->GetLeader() == pPlayer)
	{
		ConVar *var = (ConVar *)cvar->FindVar( "asw_skill" );
		if (var)
		{
			var->SetValue(m_iSkillLevel - 1);
		}
	}

		//SetSkillLevel(m_iSkillLevel - 1);
}

// alters alien health by 20% per notch away from 8
float CAlienSwarm::ModifyAlienHealthBySkillLevel(float health)
{
	float fDiff = GetMissionDifficulty() - 5;
	float f = 1.0 + fDiff * asw_difficulty_alien_health_step.GetFloat();

	return f * health;
}

// alters damage by 20% per notch away from 8
float CAlienSwarm::ModifyAlienDamageBySkillLevel( float flDamage )
{
	if (asw_debug_alien_damage.GetBool())
		Msg("Modifying alien damage by difficulty level. Base damage is %f.  Modified is ", flDamage);
	float fDiff = GetMissionDifficulty() - 5;
	float f = 1.0 + fDiff * asw_difficulty_alien_damage_step.GetFloat();
	if (asw_debug_alien_damage.GetBool())
		Msg("%f\n", (f* flDamage));
	return MAX(1.0f, f * flDamage);
}

void CAlienSwarm::ClientSettingsChanged( CBasePlayer *pPlayer )
{
	const char *pszNewName = engine->GetClientConVarValue( pPlayer->entindex(), "name" );

	const char *pszOldName = pPlayer->GetPlayerName();

	CASW_Player *pASWPlayer = (CASW_Player*)pPlayer;

	// first check if new name is really different
	if (  pszOldName[0] != 0 && 
		  Q_strncmp( pszOldName, pszNewName, ASW_MAX_PLAYER_NAME_LENGTH-1 ) )
	{
		// ok, player send different name

		// check if player is allowed to change it's name
		if ( pASWPlayer->CanChangeName() )
		{
			// change name instantly
			pASWPlayer->ChangeName( pszNewName );
		}
		else
		{
			// no change allowed, force engine to use old name again
			engine->ClientCommand( pPlayer->edict(), "name \"%s\"", pszOldName );
		}
	}

	const char *pszFov = engine->GetClientConVarValue( pPlayer->entindex(), "fov_desired" );
	if ( pszFov )
	{
		int iFov = atoi(pszFov);
		if ( m_bIsIntro )
			iFov = clamp( iFov, 20, 90 );
		else
			iFov = clamp( iFov, 20, 90 );
		pPlayer->SetDefaultFOV( iFov );
	}
}

// something big in the level has exploded and failed the mission for us
void CAlienSwarm::ExplodedLevel()
{
	if (GetGameState() == ASW_GS_INGAME)
	{
		MissionComplete(false);
		// kill all marines
		CASW_Game_Resource *pGameResource = ASWGameResource();
		if (pGameResource)
		{
			for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
			{
				CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
				if (!pMR)
					continue;
				
				CASW_Marine *pMarine = pMR->GetMarineEntity();
				if (!pMarine || pMarine->GetHealth() <= 0)
					continue;

				//CTakeDamageInfo info(NULL, NULL, Vector(0,0,0), pMarine->GetAbsOrigin(), 2000,
						//DMG_NEVERGIB);
				//pMarine->TakeDamage(info);

				//if (pMarine->m_iHealth > 0)
				//{
					pMarine->m_iHealth = 0;
					pMarine->Event_Killed( CTakeDamageInfo( pMarine, pMarine, 0, DMG_NEVERGIB ) );
					pMarine->Event_Dying();
				//}
			}
		}
	}
}

edict_t *CAlienSwarm::DoFindClientInPVS( edict_t *pEdict, unsigned char *pvs, unsigned pvssize )
{
	CBaseEntity *pe = GetContainingEntity( pEdict );
	if ( !pe )
		return NULL;

	Vector view = pe->EyePosition();
	bool bCorpseCanSee;
	CASW_Marine *pEnt = UTIL_ASW_AnyMarineCanSee(view, 384.0f, bCorpseCanSee);
	if (pEnt)
		return pEnt->edict();

	return NULL;		// returns a marine who can see us	
}

void CAlienSwarm::SetInfoHeal( CASW_Info_Heal *pInfoHeal )
{
	if ( m_hInfoHeal.Get() )
	{
		Msg("Warning: Only 1 asw_info_heal allowed per map!\n" );
	}
	m_hInfoHeal = pInfoHeal;
}

CASW_Info_Heal *CAlienSwarm::GetInfoHeal()
{
	return m_hInfoHeal.Get();
}

// returns the lowest skill level a mission was completed on, in a campaign game
int CAlienSwarm::GetLowestSkillLevelPlayed()
{
	if (!IsCampaignGame() || !GetCampaignSave() || GetCampaignSave()->m_iLowestSkillLevelPlayed == 0)
	{
		return GetSkillLevel();
	}

	return GetCampaignSave()->m_iLowestSkillLevelPlayed;
}

void CAlienSwarm::ClearLeaderKickVotes(CASW_Player *pPlayer, bool bClearLeader, bool bClearKick)
{
	if (!pPlayer || !ASWGameResource())
		return;

	int iSlotIndex = pPlayer->entindex();		// keep index 1 based for comparing the player set indices
	if (iSlotIndex < 0 || iSlotIndex >= ASW_MAX_READY_PLAYERS)
		return;

	// unflag any players voting for him
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )	
	{
		CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i));

		if ( pOtherPlayer && pOtherPlayer->IsConnected())
		{
			if ( bClearLeader && pOtherPlayer->m_iLeaderVoteIndex == iSlotIndex )
				pOtherPlayer->m_iLeaderVoteIndex = -1;
			if ( bClearKick && pOtherPlayer->m_iKickVoteIndex == iSlotIndex )
				pOtherPlayer->m_iKickVoteIndex = -1;
		}
	}

	iSlotIndex -= 1;	// make index zero based for our total arrays

	// reset his totals
	if (bClearLeader)
		ASWGameResource()->m_iLeaderVotes.Set(iSlotIndex, 0);
	if (bClearKick)
		ASWGameResource()->m_iKickVotes.Set(iSlotIndex, 0);
}

void CAlienSwarm::SetLeaderVote(CASW_Player *pPlayer, int iPlayerIndex)
{
	// if we're leader, then allow us to give leadership over to someone immediately
	if (ASWGameResource() && pPlayer == ASWGameResource()->GetLeader())
	{
		CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(iPlayerIndex));
		if (pOtherPlayer && pOtherPlayer != pPlayer)
		{
			ASWGameResource()->SetLeader(pOtherPlayer);
			CASW_Game_Resource::s_bLeaderGivenDifficultySuggestion = false;
			UTIL_ClientPrintAll(ASW_HUD_PRINTTALKANDCONSOLE, "#asw_player_made_leader", pOtherPlayer->GetPlayerName());
			ClearLeaderKickVotes(pOtherPlayer, true, false);
			return;
		}
	}

	int iOldPlayer = pPlayer->m_iLeaderVoteIndex;
	pPlayer->m_iLeaderVoteIndex = iPlayerIndex;

	// if we were previously voting for someone, update their vote count	
	if (iOldPlayer != -1)
	{
		// this loop goes through every player, counting how many players have voted for the old guy
		int iOldPlayerVotes = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )	
		{
			CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i));

			if ( pOtherPlayer && pOtherPlayer->IsConnected())
			{
				if ( pOtherPlayer->m_iLeaderVoteIndex == iOldPlayer )
					iOldPlayerVotes++;
			}
		}

		// updates the target player's game resource entry with the number of leader votes against him
		if (iOldPlayer >= 0 && iOldPlayer < ASW_MAX_READY_PLAYERS && ASWGameResource())
		{
			ASWGameResource()->m_iLeaderVotes.Set(iOldPlayer-1, iOldPlayerVotes);
		}
	}
		
	if (iPlayerIndex == -1)
		return;

	// check if this player has enough votes now
	int iVotes = 0;
	int iPlayers = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i));
		
		if ( pOtherPlayer && pOtherPlayer->IsConnected())
		{
			if ( pOtherPlayer->m_iLeaderVoteIndex == iPlayerIndex )
				iVotes++;

			iPlayers++;
		}
	}

	if (iPlayerIndex > 0 && iPlayerIndex <= ASW_MAX_READY_PLAYERS && ASWGameResource())
	{
		ASWGameResource()->m_iLeaderVotes.Set(iPlayerIndex-1, iVotes);
	}	

	int iVotesNeeded = asw_vote_leader_fraction.GetFloat() * iPlayers;	
	// make sure we're not rounding down the number of needed players
	if ((float(iPlayers) * asw_vote_leader_fraction.GetFloat()) > iVotesNeeded)
		iVotesNeeded++;
	if (iVotesNeeded <= 2)
		iVotesNeeded = 2;
	if (iPlayerIndex > 0 && iVotes >= iVotesNeeded)
	{
		Msg("leader voting %d in (votes %d/%d\n", iPlayerIndex, iVotes, iVotesNeeded);
		// make this player leader!
		CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(iPlayerIndex));
		if (pOtherPlayer)
		{
			ASWGameResource()->SetLeader(pOtherPlayer);
			CASW_Game_Resource::s_bLeaderGivenDifficultySuggestion = false;
			UTIL_ClientPrintAll(ASW_HUD_PRINTTALKANDCONSOLE, "#asw_player_made_leader", pOtherPlayer->GetPlayerName());
			ClearLeaderKickVotes(pOtherPlayer, true, false);
		}
	}
	else if (iVotes == 1 && iPlayerIndex != -1)	// if this is the first vote of this kind, check about announcing it
	{
		pPlayer->m_iKLVotesStarted++;
		if (pPlayer->m_iKLVotesStarted < 3 || (gpGlobals->curtime - pPlayer->m_fLastKLVoteTime) > 10.0f)
		{
			CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(iPlayerIndex));
			if (pOtherPlayer)
			{
				UTIL_ClientPrintAll(ASW_HUD_PRINTTALKANDCONSOLE, "#asw_started_leader_vote", pPlayer->GetPlayerName(), pOtherPlayer->GetPlayerName());
			}
		}
		pPlayer->m_fLastKLVoteTime = gpGlobals->curtime;
	}
}

void CAlienSwarm::SetKickVote(CASW_Player *pPlayer, int iPlayerIndex)
{
	if (!pPlayer)
		return;

	int iOldPlayer = pPlayer->m_iKickVoteIndex;
	pPlayer->m_iKickVoteIndex = iPlayerIndex;

	// if we were previously voting for someone, update their vote count	
	if (iOldPlayer != -1)
	{
		// this loop goes through every player, counting how many players have voted for the old guy
		int iOldPlayerVotes = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )	
		{
			CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i));

			if ( pOtherPlayer && pOtherPlayer->IsConnected())
			{
				if ( pOtherPlayer->m_iKickVoteIndex == iOldPlayer )
					iOldPlayerVotes++;
			}
		}

		// updates the target player's game resource entry with the number of kick votes against him
		if (iOldPlayer > 0 && iOldPlayer <= ASW_MAX_READY_PLAYERS && ASWGameResource())
		{
			ASWGameResource()->m_iKickVotes.Set(iOldPlayer-1, iOldPlayerVotes);
		}
	}
		
	if (iPlayerIndex == -1)
		return;


	// check if this player has enough votes now to be kicked
	int iVotes = 0;
	int iPlayers = 0;
	// this loop goes through every player, counting how many players there are and how many there are that
	//  have voted for the same player to be kicked as the one that started this function
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )	
	{
		CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i));

		if ( pOtherPlayer && pOtherPlayer->IsConnected())
		{
			if ( pOtherPlayer->m_iKickVoteIndex == iPlayerIndex )
				iVotes++;

			iPlayers++;
		}
	}

	// updates the target player's game resource entry with the number of kick votes against him
	if (iPlayerIndex >= 0 && iPlayerIndex < ASW_MAX_READY_PLAYERS && ASWGameResource())
	{
		ASWGameResource()->m_iKickVotes.Set(iPlayerIndex-1, iVotes);
	}	

	int iVotesNeeded = asw_vote_kick_fraction.GetFloat() * iPlayers;	
	// make sure we're not rounding down the number of needed players
	if ((float(iPlayers) * asw_vote_kick_fraction.GetFloat()) > iVotesNeeded)
	{		
		iVotesNeeded++;
		Msg("Rounding needed votes up to %d\n", iVotesNeeded);
	}
	if (iVotesNeeded < 2)
	{
		Msg("Increasing needed votes to 2 because that's the minimum for a kick vote\n");
		iVotesNeeded = 2;
	}
	Msg("Players %d, Votes %d, Votes needed %d\n", iPlayers, iVotes, iVotesNeeded);
	if (iPlayerIndex > 0 && iVotes >= iVotesNeeded)
	{
		// kick this player
		CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(iPlayerIndex));
		if (pOtherPlayer)
		{
			ClearLeaderKickVotes(pOtherPlayer, false, true);

			Msg("kick voting %d out (votes %d/%d)\n", iPlayerIndex, iVotes, iVotesNeeded);
			ClientPrint( pOtherPlayer, HUD_PRINTCONSOLE, "#asw_kicked_by_vote" );
			UTIL_ClientPrintAll(ASW_HUD_PRINTTALKANDCONSOLE, "#asw_player_kicked", pOtherPlayer->GetPlayerName());

			bool bPlayerCrashed = false;
			INetChannelInfo *pNetChanInfo = engine->GetPlayerNetInfo( pOtherPlayer->entindex() );
			if ( !pNetChanInfo || pNetChanInfo->IsTimingOut() )
			{
				// don't ban the player
				DevMsg( "Will not ban kicked player: net channel was idle for %.2f sec.\n", pNetChanInfo ? pNetChanInfo->GetTimeSinceLastReceived() : 0.0f );
				bPlayerCrashed = true;
			}
			if ( ( sv_vote_kick_ban_duration.GetInt() > 0 ) && !bPlayerCrashed )
			{
				// don't roll the kick command into this, it will fail on a lan, where kickid will go through
				engine->ServerCommand( CFmtStr( "banid %d %d;", sv_vote_kick_ban_duration.GetInt(), pOtherPlayer->GetUserID() ) );
			}

			char buffer[256];
			Q_snprintf(buffer, sizeof(buffer), "kickid %d\n", pOtherPlayer->GetUserID());
			Msg("sending command: %s\n", buffer);
			engine->ServerCommand(buffer);			

			//if (iPlayerIndex >= 0 && iPlayerIndex < ASW_MAX_READY_PLAYERS && ASWGameResource())
			//{
				//ASWGameResource()->m_iKickVotes.Set(iPlayerIndex-1, 0);
			//}	
		}
	}
	else if (iVotes == 1 && iPlayerIndex != -1)	// if this is the first vote of this kind, check about announcing it
	{
		pPlayer->m_iKLVotesStarted++;
		if (pPlayer->m_iKLVotesStarted < 3 || (gpGlobals->curtime - pPlayer->m_fLastKLVoteTime) > 10.0f)
		{
			CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(iPlayerIndex));
			if (pOtherPlayer)
			{
				UTIL_ClientPrintAll(ASW_HUD_PRINTTALKANDCONSOLE, "#asw_started_kick_vote", pPlayer->GetPlayerName(), pOtherPlayer->GetPlayerName());
			}
		}
		pPlayer->m_fLastKLVoteTime = gpGlobals->curtime;
	}
}

void CAlienSwarm::StartVote(CASW_Player *pPlayer, int iVoteType, const char *szVoteName, int nCampaignIndex)
{
	if (!pPlayer)
		return;

	if (GetCurrentVoteType() != ASW_VOTE_NONE)
	{
		ClientPrint(pPlayer, ASW_HUD_PRINTTALKANDCONSOLE, "#asw_vote_already_in_progress");
		return;
	}

	if (iVoteType < ASW_VOTE_SAVED_CAMPAIGN || iVoteType > ASW_VOTE_CHANGE_MISSION)
		return;

	// Check this is a valid vote, i.e.:
	//   if it's a map change, check that map exists
	//   if it's a campaign change, check that campaign exists
	//   if it's a saved campaign game, check that save exists
	// This can all be done by querying the local mission source...
	if (!missionchooser || !missionchooser->LocalMissionSource())
		return;

	IASW_Mission_Chooser_Source* pMissionSource = missionchooser->LocalMissionSource();

	if (iVoteType == ASW_VOTE_CHANGE_MISSION && !pMissionSource->MissionExists(szVoteName, true))
	{
		ClientPrint(pPlayer, ASW_HUD_PRINTTALKANDCONSOLE, "#asw_mission_doesnt_exist");
		return;
	}
	if (iVoteType == ASW_VOTE_SAVED_CAMPAIGN && !pMissionSource->SavedCampaignExists(szVoteName))
	{
		ClientPrint(pPlayer, ASW_HUD_PRINTTALKANDCONSOLE, "#asw_save_doesnt_exist");
		return;
	}

	ASW_Mission_Chooser_Mission *pContainingCampaign = NULL;
	if ( iVoteType == ASW_VOTE_CHANGE_MISSION && nCampaignIndex != -1 )
	{
		pContainingCampaign = pMissionSource->GetCampaign( nCampaignIndex );
		if ( !pContainingCampaign )
		{
			ClientPrint(pPlayer, ASW_HUD_PRINTTALKANDCONSOLE, "#asw_campaign_doesnt_exist");
			return;
		}
	}

	// start the new vote!
	m_iCurrentVoteType = iVoteType;
	Q_strncpy( m_szCurrentVoteName, szVoteName, 128 );
	// store a pretty description if we can
	if (iVoteType == ASW_VOTE_CHANGE_MISSION)
	{		
		Q_strncpy( m_szCurrentVoteDescription.GetForModify(), pMissionSource->GetPrettyMissionName(szVoteName), 128 );
		Q_strncpy( m_szCurrentVoteMapName.GetForModify(), szVoteName, 128 );
		if ( !pContainingCampaign )
		{
			Q_strncpy( m_szCurrentVoteCampaignName.GetForModify(), "", 128 );
		}
		else
		{
			Q_strncpy( m_szCurrentVoteCampaignName.GetForModify(), pContainingCampaign->m_szMissionName, 128 );
		}
	}
	else if (iVoteType == ASW_VOTE_SAVED_CAMPAIGN)
	{		
		Q_strncpy( m_szCurrentVoteDescription.GetForModify(), pMissionSource->GetPrettySavedCampaignName(szVoteName), 128 );
	}
	m_iCurrentVoteYes = 0;
	m_iCurrentVoteNo = 0;
	m_fVoteEndTime = gpGlobals->curtime + asw_vote_duration.GetFloat();
	m_PlayersVoted.Purge();

	// clear out current votes for all players
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )	
	{
		CASW_Player* pOtherPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex(i));
		if (pOtherPlayer)
			pOtherPlayer->m_iMapVoted = 0;
	}
	
	// print a message telling players about the new vote that has started and how to bring up their voting options
	const char *desc = m_szCurrentVoteDescription;
	if (iVoteType == ASW_VOTE_CHANGE_MISSION)
	{
		UTIL_ClientPrintAll(ASW_HUD_PRINTTALKANDCONSOLE, "#asw_vote_mission_start", pPlayer->GetPlayerName(), desc);		
	}
	else if (iVoteType == ASW_VOTE_SAVED_CAMPAIGN)
	{
		UTIL_ClientPrintAll(ASW_HUD_PRINTTALKANDCONSOLE, "#asw_vote_saved_start", pPlayer->GetPlayerName(), desc);		
	}
// 	else if (iVoteType == ASW_VOTE_CAMPAIGN)
// 	{
// 		UTIL_ClientPrintAll(ASW_HUD_PRINTTALKANDCONSOLE, "#asw_vote_campaign_start", pPlayer->GetPlayerName(), desc);		
// 	}

	//UTIL_ClientPrintAll(ASW_HUD_PRINTTALKANDCONSOLE, "#asw_press_vote_key", "^%playerlist%" );
	CastVote(pPlayer, true);
}

void CAlienSwarm::CastVote(CASW_Player *pPlayer, bool bVoteYes)
{
	if (!pPlayer)
		return;

	if (m_iCurrentVoteType == ASW_VOTE_NONE)
		return;

	// get an ID for this player
	const char *pszNetworkID = pPlayer->GetASWNetworkID();
	
	// check this player hasn't voted already
	for (int i=0;i<m_PlayersVoted.Count();i++)
	{
		const char *p = STRING(m_PlayersVoted[i]);
		if (!Q_strcmp(p, pszNetworkID))
		{
			ClientPrint(pPlayer, ASW_HUD_PRINTTALKANDCONSOLE, "#asw_already_voted");
			return;
		}
	}

	pPlayer->m_iMapVoted = bVoteYes ? 2 : 1;

	// add this player to the list of those who have voted
	string_t stringID = AllocPooledString(pszNetworkID);
	m_PlayersVoted.AddToTail(stringID);

	// count his vote
	if (bVoteYes)
		m_iCurrentVoteYes++;
	else
		m_iCurrentVoteNo++;

	UpdateVote();
}

// removes a player's vote from the current vote (used when they disconnect)
void CAlienSwarm::RemoveVote(CASW_Player *pPlayer)
{
	if (!pPlayer || pPlayer->m_iMapVoted.Get() == 0)
		return;

	if (m_iCurrentVoteType == ASW_VOTE_NONE)
		return;

	// count his vote
	if (pPlayer->m_iMapVoted.Get() == 2)
		m_iCurrentVoteYes--;
	else
		m_iCurrentVoteNo--;

	UpdateVote();
}

void CAlienSwarm::UpdateVote()
{
	if (m_iCurrentVoteType == ASW_VOTE_NONE)
		return;

	// check if a yes/no total has reached the amount needed to make a decision
	int iNumPlayers = UTIL_ASW_GetNumPlayers();
	int iNeededVotes = asw_vote_map_fraction.GetFloat() * iNumPlayers;
	if (iNeededVotes < 1)
		iNeededVotes = 1;
	// make sure we're not rounding down the number of needed players
	if ((float(iNumPlayers) * asw_vote_map_fraction.GetFloat()) > iNeededVotes)
		iNeededVotes++;

	bool bSinglePlayer = ASWGameResource() && ASWGameResource()->IsOfflineGame();

	if (m_iCurrentVoteYes >= iNeededVotes)
	{
		if (ASWGameResource())
			ASWGameResource()->RememberLeaderID();

		// make it so
		UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_vote_passed" );
		if (m_iCurrentVoteType == ASW_VOTE_CHANGE_MISSION)
		{
			// if we're ingame, then upload for state (we don't do this once the mission is over, as stats are already sent on MissionComplete)
			// stats todo:
			//if (GetGameState() == ASW_GS_INGAME && ASWGameStats())
				//ASWGameStats()->AddMapRecord();

			const char *szCampaignName = GetCurrentVoteCampaignName();
			if ( !szCampaignName || !szCampaignName[0] )
			{
				// changes level into single mission mode
				engine->ChangeLevel(GetCurrentVoteName(), NULL);
			}
			else
			{
				// start a new campaign on the specified mission
				IASW_Mission_Chooser_Source* pSource = missionchooser ? missionchooser->LocalMissionSource() : NULL;
				if ( !pSource )
					return;

				char szSaveFilename[ MAX_PATH ];
				szSaveFilename[ 0 ] = 0;
				const char *szStartingMission = GetCurrentVoteName();
				if ( !pSource->ASW_Campaign_CreateNewSaveGame( &szSaveFilename[0], sizeof( szSaveFilename ), szCampaignName, ( gpGlobals->maxClients > 1 ), szStartingMission ) )
				{
					Msg( "Unable to create new save game.\n" );
					return;
				}
				engine->ServerCommand( CFmtStr( "%s %s campaign %s\n",
					"changelevel",
					szStartingMission ? szStartingMission : "lobby",
					szSaveFilename ) );
			}
		}
		else if (m_iCurrentVoteType == ASW_VOTE_SAVED_CAMPAIGN)
		{
			// if we're ingame, then upload for state (we don't do this once the mission is over, as stats are already sent on MissionComplete)
			// stats todo:
			//if (GetGameState() == ASW_GS_INGAME && ASWGameStats())
				//ASWGameStats()->AddMapRecord();

			// check the save file still exists (in very rare cases it could have been deleted automatically and the player clicked really fast!)
			if ( !missionchooser || !missionchooser->LocalMissionSource() || missionchooser->LocalMissionSource()->SavedCampaignExists(GetCurrentVoteName()) )
			{
				// load this saved campaign game
				char szMapCommand[MAX_PATH];
				Q_snprintf(szMapCommand, sizeof( szMapCommand ), "asw_server_loadcampaign %s %s change\n",
					GetCurrentVoteName(),
					bSinglePlayer ? "SP" : "MP"
				);
				engine->ServerCommand(szMapCommand);
			}
		}

		m_iCurrentVoteType = (int) ASW_VOTE_NONE;		
		m_iCurrentVoteYes = 0;
		m_iCurrentVoteNo = 0;
		m_fVoteEndTime = 0;
		m_PlayersVoted.Purge();
	}
	else if ( asw_vote_map_fraction.GetFloat() <= 1.0f && (m_iCurrentVoteNo >= iNeededVotes || gpGlobals->curtime >= m_fVoteEndTime		// check if vote has timed out also
		|| (m_iCurrentVoteNo + m_iCurrentVoteYes) >= iNumPlayers) )			// or if everyone's voted and it didn't trigger a yes
	{
		// the people decided against this vote, clear it all
		UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_vote_failed" );
		m_iCurrentVoteType = (int) ASW_VOTE_NONE;		
		m_iCurrentVoteYes = 0;
		m_iCurrentVoteNo = 0;
		m_fVoteEndTime = 0;
		m_PlayersVoted.Purge();
	}
}

void CAlienSwarm::SetInitialGameMode()
{
	m_iSpecialMode = 0;
	if (asw_last_game_variation.GetInt() == 1)
		SetCarnageMode(true);
	else if (asw_last_game_variation.GetInt() == 2)
		SetUberMode(true);
	else if (asw_last_game_variation.GetInt() == 3)
		SetHardcoreMode(true);
	Msg("SetInitialGameMode to %d (last variation = %d)\n", m_iSpecialMode.Get(), asw_last_game_variation.GetInt());
}

void CAlienSwarm::SetCarnageMode(bool bCarnageMode)
{
	if (m_iGameState != ASW_GS_BRIEFING)
		return;

	if (bCarnageMode)
	{
		if (MapScores() && MapScores()->IsModeUnlocked(STRING(gpGlobals->mapname), GetSkillLevel(), ASW_SM_CARNAGE))
			m_iSpecialMode |= (int) ASW_SM_CARNAGE;
	}
	else
		m_iSpecialMode &= ~ASW_SM_CARNAGE;

	Msg("Changed carnage mode to %d\n", bCarnageMode);
}

void CAlienSwarm::SetUberMode(bool bUberMode)
{
	if (m_iGameState != ASW_GS_BRIEFING)
		return;

	if (bUberMode)
	{
		if (MapScores() && MapScores()->IsModeUnlocked(STRING(gpGlobals->mapname), GetSkillLevel(), ASW_SM_UBER))
			m_iSpecialMode |= (int) ASW_SM_UBER;
	}
	else
		m_iSpecialMode &= ~ASW_SM_UBER;

	Msg("Changed uber mode to %d\n", bUberMode);
}

void CAlienSwarm::SetHardcoreMode(bool bHardcoreMode)
{
	if (m_iGameState != ASW_GS_BRIEFING)
		return;

	if (bHardcoreMode)
	{
		if (MapScores() && MapScores()->IsModeUnlocked(STRING(gpGlobals->mapname), GetSkillLevel(), ASW_SM_HARDCORE))
			m_iSpecialMode |= (int) ASW_SM_HARDCORE;
	}
	else
		m_iSpecialMode &= ~ASW_SM_HARDCORE;

	Msg("Changed hardcore mode to %d\n", bHardcoreMode);
}

void CAlienSwarm::StopAllAmbientSounds()
{
	CBaseEntity *pAmbient = gEntList.FindEntityByClassname( NULL, "ambient_generic" );
	while ( pAmbient != NULL )
	{
		//Msg("sending ambient generic fade out to entity %d %s\n", pAmbient->entindex(), STRING(pAmbient->GetEntityName()));
		pAmbient->KeyValue("fadeout", "1");
		pAmbient->SetNextThink( gpGlobals->curtime + 0.1f );
		pAmbient = gEntList.FindEntityByClassname( pAmbient, "ambient_generic" );
	}

	CBaseEntity *pFire = gEntList.FindEntityByClassname( NULL, "env_fire" );
	while ( pFire != NULL )
	{
		FireSystem_ExtinguishFire(pFire);
		pFire = gEntList.FindEntityByClassname( pFire, "env_fire" );
	}
}

void CAlienSwarm::StartAllAmbientSounds()
{
	CBaseEntity *pAmbient = gEntList.FindEntityByClassname( NULL, "ambient_generic" );
	while ( pAmbient != NULL )
	{		
		pAmbient->KeyValue("aswactivate", "1");		
		pAmbient = gEntList.FindEntityByClassname( pAmbient, "ambient_generic" );
	}
}

bool CAlienSwarm::AllowSoundscapes()
{
	if (UTIL_ASW_MissionHasBriefing(STRING(gpGlobals->mapname)) && GetGameState() != ASW_GS_INGAME)
		return false;

	return true;
}

void CAlienSwarm::SetForceReady(int iForceReadyType)
{
	if (iForceReadyType < ASW_FR_NONE || iForceReadyType > ASW_FR_INGAME_RESTART)
		return;
	// don't allow restarting if we're on the campaign map, as this does Bad Things (tm)  NOTE: Campaign stuff has it's own force launch code
	if (GetGameState() == ASW_GS_CAMPAIGNMAP)
		return;
	// make sure force ready types are being requested at the right stage of the game
	if (iForceReadyType == ASW_FR_BRIEFING && GetGameState() != ASW_GS_BRIEFING)
		return;
	if (iForceReadyType >= ASW_FR_CONTINUE && iForceReadyType <= ASW_FR_CAMPAIGN_MAP && GetGameState() != ASW_GS_DEBRIEF)
		return;
	// don't allow force ready to be changed if there's already a force ready in progress (unless we're clearing it)
	if (m_iForceReadyType != ASW_FR_NONE && iForceReadyType != ASW_FR_NONE)
		return;
	if ( iForceReadyType == ASW_FR_INGAME_RESTART && GetGameState() != ASW_GS_INGAME )
		return;

	m_iForceReadyType = iForceReadyType;

	if (iForceReadyType == ASW_FR_NONE)
	{
		m_fForceReadyTime = 0;
		m_iForceReadyCount = 0;
		return;
	}

	m_fForceReadyTime = gpGlobals->curtime + 5.9f;
	m_iForceReadyCount = 5;

	// check to see if it should end immediately
	CheckForceReady();

	// if it didn't, print a message saying we're starting a countdown
	if (m_iForceReadyType == ASW_FR_INGAME_RESTART)
	{
		//UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_ingame_restart_5" );
		m_flRestartingMissionTime = m_fForceReadyTime;
	}
	else if (m_iForceReadyType > ASW_FR_NONE)
	{
		UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_force_ready_5" );
	}
}

void CAlienSwarm::CheckForceReady()
{
	if (m_iForceReadyType <= ASW_FR_NONE || !ASWGameResource())
		return;

	// abort briefing force ready if no marines are selected
	if (m_iForceReadyType == ASW_FR_BRIEFING && !ASWGameResource()->AtLeastOneMarine())
	{
		SetForceReady(ASW_FR_NONE);
		return;
	}

	if ( ASWGameResource()->GetLeader() && GetGameState()!=ASW_GS_INGAME )
	{
		// check if everyone made themselves ready anyway
		if ( ASWGameResource()->AreAllOtherPlayersReady(ASWGameResource()->GetLeader()->entindex()) )
		{
			FinishForceReady();
			return;
		}
	}

	int iSecondsLeft = m_fForceReadyTime - gpGlobals->curtime;
	if (iSecondsLeft < m_iForceReadyCount)
	{
		m_iForceReadyCount = iSecondsLeft;
		if ( m_iForceReadyType == ASW_FR_INGAME_RESTART )
		{
// 			switch (iSecondsLeft)
// 			{
// 				case 4:  UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_ingame_restart_4" ); break;
// 				case 3:  UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_ingame_restart_3" ); break;
// 				case 2:  UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_ingame_restart_2" ); break;
// 				case 1:  UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_ingame_restart_1" ); break;
// 				default: break;
// 			}
		}
		else
		{
			switch (iSecondsLeft)
			{
				case 4:  UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_force_ready_4" ); break;
				case 3:  UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_force_ready_3" ); break;
				case 2:  UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_force_ready_2" ); break;
				case 1:  UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_force_ready_1" ); break;
				default: break;
			}
		}
		if (iSecondsLeft <= 0)
		{
			FinishForceReady();
			return;									
		}
	}		
}

void CAlienSwarm::FinishForceReady()
{
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	switch (m_iForceReadyType)
	{
		case ASW_FR_BRIEFING:		// forcing a start of the mission
			{
				SetForceReady(ASW_FR_NONE);	

				// check we actually have some marines selected before starting			
				int m = pGameResource->GetMaxMarineResources();	
				bool bCanStart = false;
				for (int i=0;i<m;i++)
				{
					if (pGameResource->GetMarineResource(i))
					{
						bCanStart = true;
					}
				}
				if (!bCanStart)
					return;

				SetMaxMarines();
				
				m_bShouldStartMission = true;			
			}
			break;
		case ASW_FR_CONTINUE:	// force save and continue
			{
				SetForceReady(ASW_FR_NONE);	
				CampaignSaveAndShowCampaignMap(NULL, true);
			}
			break;
		case ASW_FR_RESTART:	// force a mission restart
		case ASW_FR_INGAME_RESTART:
			{
				SetForceReady(ASW_FR_NONE);

				if ( gpGlobals->curtime - m_fMissionStartedTime > 30.0f && GetGameState() == ASW_GS_INGAME )
				{
					MissionComplete( false );		
				}
				else
				{
					RestartMission( NULL, true );
				}
			}
			break;
		case ASW_FR_CAMPAIGN_MAP:
			{
				SetForceReady(ASW_FR_NONE);	
				// launch campaign map without saving
				// make sure all players are marked as not ready
				for (int i=0;i<ASW_MAX_READY_PLAYERS;i++)
				{
					pGameResource->m_bPlayerReady.Set(i, false);
				}				
				SetGameState(ASW_GS_CAMPAIGNMAP);
				GetCampaignSave()->SelectDefaultNextCampaignMission();
			}
			break;

		default: break;
	};	
}

void CAlienSwarm::OnSVCheatsChanged()
{
	ConVarRef sv_cheats( "sv_cheats" );
	if ( sv_cheats.GetBool() )
	{
		m_bCheated = true;
	}
}
//ConCommand asw_notify_ch( "asw_notify_ch", StartedCheating_f, "Internal use", 0 );

int CAlienSwarm::GetSpeedrunTime( void )
{
	CAlienSwarmProxy* pSwarmProxy = dynamic_cast<CAlienSwarmProxy*>(gEntList.FindEntityByClassname( NULL, "asw_gamerules" ));
	Assert(pSwarmProxy);

	return pSwarmProxy->m_iSpeedrunTime;
}

void CAlienSwarm::BroadcastSound( const char *sound )
{
	CBroadcastRecipientFilter filter;
	filter.MakeReliable();

	UserMessageBegin ( filter, "BroadcastAudio" );
		WRITE_STRING( sound );
	MessageEnd();
}

void CAlienSwarm::OnPlayerFullyJoined( CASW_Player *pPlayer )
{
	// Set briefing start time
	m_fBriefingStartedTime = gpGlobals->curtime;
}

void CAlienSwarm::DropPowerup( CBaseEntity *pSource, const CTakeDamageInfo &info, const char *pszSourceClass )
{
	if ( !asw_drop_powerups.GetBool() || !pSource )
		return;

	if ( !ASWHoldoutMode() )
		return;

	float flChance = 0.0f;
	if ( m_fLastPowerupDropTime == 0 )
	{
		flChance = 0.05f;
		m_fLastPowerupDropTime = gpGlobals->curtime;
	}

	if ( (m_fLastPowerupDropTime + 4) < gpGlobals->curtime )
	{
		flChance = MIN( (gpGlobals->curtime - m_fLastPowerupDropTime)/100.0f, 0.5f );
	}

	//Msg( "Powerup chance = %f\n", flChance );

	if ( RandomFloat( 0.0f, 1.0f ) > flChance )
		return;

	m_fLastPowerupDropTime = gpGlobals->curtime;

	int nPowerupType = RandomInt( 0, NUM_POWERUP_TYPES-1 );

	const char *m_szPowerupClassname;
	switch ( nPowerupType )
	{
	case POWERUP_TYPE_FREEZE_BULLETS:
		m_szPowerupClassname = "asw_powerup_freeze_bullets"; break;
	case POWERUP_TYPE_FIRE_BULLETS:
		m_szPowerupClassname = "asw_powerup_fire_bullets"; break;
	case POWERUP_TYPE_ELECTRIC_BULLETS:
		m_szPowerupClassname = "asw_powerup_electric_bullets"; break;
	//case POWERUP_TYPE_CHEMICAL_BULLETS:
	//	m_szPowerupClassname = "asw_powerup_chemical_bullets"; break;
	//case POWERUP_TYPE_EXPLOSIVE_BULLETS:
	//	m_szPowerupClassname = "asw_powerup_explosive_bullets"; break;
	case POWERUP_TYPE_INCREASED_SPEED:
		m_szPowerupClassname = "asw_powerup_increased_speed"; break;
	default:
		return; break;
	}

	CASW_Powerup *pPowerup = (CASW_Powerup *)CreateEntityByName( m_szPowerupClassname );		
	UTIL_SetOrigin( pPowerup, pSource->WorldSpaceCenter() );
	pPowerup->Spawn();

	Vector vel = -info.GetDamageForce();
	vel.NormalizeInPlace();
	vel *= RandomFloat( 10, 20 );
	vel += RandomVector( -10, 10 );
	vel.z = RandomFloat( 40, 60 );
	pPowerup->SetAbsVelocity( vel );
}

void CAlienSwarm::CheckTechFailure()
{	
	if ( m_flTechFailureRestartTime > 0 && gpGlobals->curtime >= m_flTechFailureRestartTime )
	{
		CASW_Game_Resource *pGameResource = ASWGameResource();
		if ( !pGameResource )
			return;

		// count number of live techs
		bool bTech = false;
		for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
		{
			CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
			if (pMR && pMR->GetHealthPercent() > 0 && pMR->GetProfile() && pMR->GetProfile()->CanHack())
			{
				bTech = true;
				break;
			}
		}
		if ( !bTech && pGameResource->CountAllAliveMarines() > 0 )
		{
			// tell all clients that it's time to restart
			CReliableBroadcastRecipientFilter users;
			users.MakeReliable();
			UserMessageBegin( users, "ASWTechFailure" );
			MessageEnd();

			SetForceReady( ASW_FR_INGAME_RESTART );
		}
	}
}
#endif  // !CLIENT_DLL

void CAlienSwarm::RefreshSkillData ( bool forceUpdate )
{
#ifndef CLIENT_DLL
	if ( !forceUpdate )
	{
		if ( GlobalEntity_IsInTable( "skill.cfg" ) )
			return;
	}
	GlobalEntity_Add( "skill.cfg", STRING(gpGlobals->mapname), GLOBAL_ON );
	char	szExec[256];

	ConVar const *skill = cvar->FindVar( "asw_skill" );

	SetSkillLevel( skill ? skill->GetInt() : 1 );

	// HL2 current only uses one skill config file that represents MEDIUM skill level and
	// synthesizes EASY and HARD. (sjb)
	Q_snprintf( szExec,sizeof(szExec), "exec skill_manifest.cfg\n" );

	engine->ServerCommand( szExec );
	engine->ServerExecute();
#endif // not CLIENT_DLL
}

bool CAlienSwarm::IsMultiplayer()
{
	// always true - controls behavior of achievement manager, etc.
	return true;
}

bool CAlienSwarm::IsOfflineGame()
{
	return ( ASWGameResource() && ASWGameResource()->IsOfflineGame() );
}

int CAlienSwarm::CampaignMissionsLeft()
{
	if (!IsCampaignGame())
		return 2;

	CASW_Campaign_Info *pCampaign = GetCampaignInfo();
	if (!pCampaign)
		return 2;

	CASW_Campaign_Save *pSave = GetCampaignSave();
	if (!pSave)
		return 2;

	int iNumMissions = pCampaign->GetNumMissions() - 1;	// minus one, for the atmospheric entry which isn't a completable mission
	
	return (iNumMissions - pSave->m_iNumMissionsComplete);
}

bool CAlienSwarm::MarineCanPickupAmmo(CASW_Marine *pMarine, CASW_Ammo *pAmmo)
{
	if (!pMarine || !pAmmo)
		return false;

	int iGuns = pMarine->GetNumberOfWeaponsUsingAmmo(pAmmo->m_iAmmoIndex);
	if (iGuns <= 0)
	{
		// just show the name of the ammo, without the 'take'
#ifdef CLIENT_DLL
		Q_snprintf(m_szPickupDenial, sizeof(m_szPickupDenial), "%s", pAmmo->m_szNoGunText);
#endif
		return false;
	}

	if ( pAmmo->m_iAmmoIndex < 0 || pAmmo->m_iAmmoIndex >= MAX_AMMO_SLOTS )
		return false;

	int iMax = GetAmmoDef()->MaxCarry(pAmmo->m_iAmmoIndex, pMarine) * iGuns;
	int iAdd = MIN( 1, iMax - pMarine->GetAmmoCount(pAmmo->m_iAmmoIndex) );
	if ( iAdd < 1 )
	{
#ifdef CLIENT_DLL
		Q_snprintf(m_szPickupDenial, sizeof(m_szPickupDenial), "%s", pAmmo->m_szAmmoFullText);
#endif
		return false;
	}

	return true;
}

bool CAlienSwarm::MarineCanPickupPowerup(CASW_Marine *pMarine, CASW_Powerup *pPowerup)
{
	if ( !pMarine )
		return false;

	CASW_Weapon* pWeapon = pMarine->GetActiveASWWeapon();
	if ( !pWeapon || !IsBulletBasedWeaponClass( pWeapon->Classify() ) )
	{
#ifdef CLIENT_DLL
		Q_snprintf(m_szPickupDenial, sizeof(m_szPickupDenial), "%s", "#asw_powerup_requires_bulletammo");
#endif
		return false;
	}
	
	return true;
}

bool CAlienSwarm::MarineHasRoomInAmmoBag(CASW_Marine *pMarine, int iAmmoIndex)
{
	if (!pMarine)
		return false;

	if ( iAmmoIndex < 0 || iAmmoIndex >= MAX_AMMO_SLOTS )
		return false;

	CASW_Weapon_Ammo_Bag *pBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pMarine->GetWeapon(0));
	if (pBag)
	{
		if (pBag->HasRoomForAmmo(iAmmoIndex))
			return true;
	}

	pBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pMarine->GetWeapon(1));
	if (pBag)
	{
		if (pBag->HasRoomForAmmo(iAmmoIndex))
			return true;
	}

	return false;
}

#ifdef GAME_DLL
void CheatsChangeCallback( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	if ( ASWGameRules() )
	{
		ASWGameRules()->OnSVCheatsChanged();
	}
}
#endif

void CAlienSwarm::LevelInitPostEntity()
{
	
	// check if we're the intro/outro map
	char mapName[255];
#ifdef CLIENT_DLL
	Q_FileBase( engine->GetLevelName(), mapName, sizeof(mapName) );	
#else
	Q_strcpy( mapName, STRING(gpGlobals->mapname) );
#endif

	m_bIsIntro = ( !Q_strnicmp( mapName, "intro_", 6 ) );
	m_bIsOutro = ( !Q_strnicmp( mapName, "outro_", 6 ) );
	m_bIsTutorial = ( !Q_strnicmp( mapName, "tutorial", 8 ) );
	m_bIsLobby = ( !Q_strnicmp( mapName, "Lobby", 5 ) );

	if ( ASWHoldoutMode() )
	{
		ASWHoldoutMode()->LevelInitPostEntity();
	}

	if ( missionchooser && missionchooser->RandomMissions() )
	{
		missionchooser->RandomMissions()->LevelInitPostEntity( mapName );
	}

#ifndef CLIENT_DLL
	engine->ServerCommand("exec newmapsettings\n");

	m_bPlayedBlipSpeech = false;
	m_bQuickStart = false;

	KeyValues *pLaunchOptions = engine->GetLaunchOptions();
	if ( pLaunchOptions )
	{
		for ( KeyValues *pKey = pLaunchOptions->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
		{
			if ( !Q_stricmp( pKey->GetString(), "quickstart" ) )
			{
				m_bQuickStart = true;
			}
		}
	}

	// create the game resource
	if (ASWGameResource()!=NULL)
	{
		Msg("ASW: ASWGameResource already exists, removing it\n");
		UTIL_Remove( ASWGameResource() );
	}
	
	CreateEntityByName( "asw_game_resource" );
	
	if (ASWGameResource() == NULL)
	{
		Msg("ASW: Error! Failed to create ASWGameResource\n");
		return;
	}

	// find the objective entities
	ASWGameResource()->FindObjectives();

	// setup the savegame entity, if we're in a campaign game
	if (IsCampaignGame())
	{
		if (!ASWGameResource()->CreateCampaignSave())
		{
			Msg("ERROR: Failed to create campaign save object\n");
		}
		else
		{
			if (!ASWGameResource()->GetCampaignSave()->LoadGameFromFile(ASWGameResource()->GetCampaignSaveName()))
			{
				Msg("ERROR: Failed to load campaign save game: %s\n", ASWGameResource()->GetCampaignSaveName());
			}
			else
			{
				// with the save game loaded, now load the campaign info
				if (!GetCampaignInfo())
				{
					Msg("ERROR: Failed to load in the campaign associated with the current save game: %s\n", ASWGameResource()->GetCampaignSaveName());
				}
				else
				{
					// update our difficulty level with the relevant campaign modifier
					OnSkillLevelChanged( m_iSkillLevel );
					ReserveMarines();
				}
			}
		}
	}
	// todo: if we fail to load the campaign save file above, then gracefully fall into single mission mode?

	// make sure we're on easy mode for the tutorial
	if ( IsTutorialMap() )
	{
		asw_skill.SetValue( 0 );
		m_iSkillLevel = asw_skill.GetInt();
		OnSkillLevelChanged( m_iSkillLevel );
	}

	// set available game modes
	if (MapScores())
	{
		m_iUnlockedModes = (MapScores()->IsModeUnlocked(STRING(gpGlobals->mapname), GetSkillLevel(), ASW_SM_CARNAGE) ? ASW_SM_CARNAGE : 0) +
							(MapScores()->IsModeUnlocked(STRING(gpGlobals->mapname), GetSkillLevel(), ASW_SM_UBER) ? ASW_SM_UBER : 0) +
							(MapScores()->IsModeUnlocked(STRING(gpGlobals->mapname), GetSkillLevel(), ASW_SM_HARDCORE) ? ASW_SM_HARDCORE : 0);
	}
	else
	{
		m_iUnlockedModes = 0;
	}
	
	SetGameState(ASW_GS_BRIEFING);
	mm_swarm_state.SetValue( "briefing" );
	SetMaxMarines();
	SetInitialGameMode();

	// create the burning system
	CASW_Burning *pFire = dynamic_cast<CASW_Burning*>( CreateEntityByName( "asw_burning" ) );	
	pFire->Spawn();

	ConVar *var = (ConVar *)cvar->FindVar( "sv_cheats" );
	if ( var )
	{
		m_bCheated = var->GetBool();
		static bool s_bInstalledCheatsChangeCallback = false;
		if ( !s_bInstalledCheatsChangeCallback )
		{
			var->InstallChangeCallback( CheatsChangeCallback );
			s_bInstalledCheatsChangeCallback = true;
		}
	}

	if ( IsLobbyMap() )
	{
		OnServerHibernating();
	}
#endif  // !CLIENT_DLL
}

int CAlienSwarm::TotalInfestDamage()
{
	if (IsHardcoreMode())
	{
		return 500;
	}

	switch ( m_iSkillLevel )
	{
	case 1:
		return 175;
	case 2:
		return 225;
	case 3:
		return 270;
	case 4:
		return 280;	// BARELY survivable with Bastille and heal beacon
	case 5:
		return 280;
	}

	// ASv1 Total infest damage = 90 + difficulty * 20;
	// Infest damage rate = Total infest damage / 20;
	//
	// i.e.:
	// 120 at difficulty 1	 -   6.0 damage/sec  - 2 full skill satchel heals.  ASv1: 1 medpack/satchel
	// 160 at difficulty 3   -   8.0 damage/sec   - 2 full skill satchel heals
	// 200 at difficulty 5	 -  10.0 damage/sec   - 3 full skill satchel heals  ASv1: 2 satchel heals
	// 240 at difficulty 7   -  12.0 damage/sec  - 3+ full skill satchel heals  ASv1: 3 satchel heals, if you get there instantly
	// 260 at difficulty 8   -  13.0 damage/sec  - 3+ full skill satchel heals  Can't outheal
	// 300 at difficulty 10  -  15.0 damage/sec  - 4- full skill satchel heals  Can't outheal  Can survive if healed by a level 3 xenowound marine if they're quick.
	//                                             (double number of heals needed for a 0 skill medic)
	//
	// Med satchel heals 4 health every 0.33 seconds, which is 12 hps/sec (same as ASv1)	
	// Med satchel heals 25 hps at skill 0, 65 hps at skill 5.
	// Med satchel has 4 to 9 charges.
	//  Giving a total hps healed for 1 satchel = 100 to 675
	//  Medkit heals 50 hps for normal marines and 50 to 80 for medics)
	// In ASv1 satchel healed 25 to 85.   Charges were: 6, 7 (at 3 star) or 8 (at 4 star+)
	// Giving a total hps healed for 1 satchel = 150 to 680
	return 225;
}

bool CAlienSwarm::ShouldPlayStimMusic()
{
	return (gpGlobals->curtime > m_fPreventStimMusicTime.Get());
}

const char* CAlienSwarm::GetFailAdviceText( void )
{
	switch ( ASWGameRules()->m_nFailAdvice )
	{
	case ASW_FAIL_ADVICE_LOW_AMMO:				return "#asw_fail_advice_low_ammo";
	case ASW_FAIL_ADVICE_INFESTED_LOTS:			return "#asw_fail_advice_infested_lots";
	case ASW_FAIL_ADVICE_INFESTED:				return "#asw_fail_advice_infested";
	case ASW_FAIL_ADVICE_SWARMED:				return "#asw_fail_advice_swarmed";
	case ASW_FAIL_ADVICE_FRIENDLY_FIRE:			return "#asw_fail_advice_friendly_fire";
	case ASW_FAIL_ADVICE_HACKER_DAMAGED:		return "#asw_fail_advice_hacker_damaged";
	case ASW_FAIL_ADVICE_WASTED_HEALS:			return "#asw_fail_advice_wasted_heals";
	case ASW_FAIL_ADVICE_IGNORED_HEALING:		return "#asw_fail_advice_ignored_healing";
	case ASW_FAIL_ADVICE_IGNORED_ADRENALINE:	return "#asw_fail_advice_ignored_adrenaline";
	case ASW_FAIL_ADVICE_IGNORED_SECONDARY:		return "#asw_fail_advice_ignored_secondary";
	case ASW_FAIL_ADVICE_IGNORED_WELDER:		return "#asw_fail_advice_ignored_welder";
	case ASW_FAIL_ADVICE_IGNORED_LIGHTING:		return "#asw_fail_advice_ignored_lighting";	// TODO
	case ASW_FAIL_ADVICE_SLOW_PROGRESSION:		return "#asw_fail_advice_slow_progression";
	case ASW_FAIL_ADVICE_SHIELD_BUG:			return "#asw_fail_advice_shield_bug";
	case ASW_FAIL_ADVICE_DIED_ALONE:			return "#asw_fail_advice_died_alone";
	case ASW_FAIL_ADVICE_NO_MEDICS:				return "#asw_fail_advice_no_medics";

	case ASW_FAIL_ADVICE_DEFAULT:
	default:
		switch ( RandomInt( 0, 2 ) )
		{
		case 0: return "#asw_fail_advice_teamwork";
		case 1: return "#asw_fail_advice_loadout";
		case 2: return "#asw_fail_advice_routes";
		}
	}

	return "#asw_fail_advice_teamwork";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAlienSwarm::Damage_IsTimeBased( int iDmgType )
{
	// Damage types that are time-based.
	return ( ( iDmgType & ( DMG_PARALYZE | DMG_NERVEGAS | DMG_RADIATION | DMG_DROWNRECOVER | DMG_ACID | DMG_SLOWBURN ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAlienSwarm::Damage_ShouldGibCorpse( int iDmgType )
{
	// Damage types that gib the corpse.
	return ( ( iDmgType & ( DMG_CRUSH | DMG_FALL | DMG_BLAST | DMG_SONIC | DMG_CLUB | DMG_INFEST ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAlienSwarm::Damage_NoPhysicsForce( int iDmgType )
{
	// Damage types that don't have to supply a physics force & position.
	int iTimeBasedDamage = Damage_GetTimeBased();
	return ( ( iDmgType & ( DMG_FALL | DMG_BURN | DMG_PLASMA | DMG_DROWN | iTimeBasedDamage | DMG_CRUSH | DMG_PHYSGUN | DMG_PREVENT_PHYSICS_FORCE | DMG_INFEST ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAlienSwarm::Damage_ShouldNotBleed( int iDmgType )
{
	// Damage types that don't make the player bleed.
	return ( ( iDmgType & ( DMG_ACID ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAlienSwarm::Damage_GetTimeBased( void )
{
	int iDamage = ( DMG_PARALYZE | DMG_NERVEGAS | DMG_RADIATION | DMG_DROWNRECOVER | DMG_ACID | DMG_SLOWBURN );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CAlienSwarm::Damage_GetShouldGibCorpse( void )
{
	int iDamage = ( DMG_CRUSH | DMG_FALL | DMG_BLAST | DMG_SONIC | DMG_CLUB | DMG_INFEST  );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CAlienSwarm::Damage_GetNoPhysicsForce( void )
{
	int iTimeBasedDamage = Damage_GetTimeBased();
	int iDamage = ( DMG_FALL | DMG_BURN | DMG_PLASMA | DMG_DROWN | iTimeBasedDamage | DMG_CRUSH | DMG_PHYSGUN | DMG_PREVENT_PHYSICS_FORCE | DMG_INFEST );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CAlienSwarm::Damage_GetShouldNotBleed( void )
{
	int iDamage = ( DMG_ACID );
	return iDamage;
}

// movement uses this axis to decide where the marine should go from his forward/sidemove
const QAngle& CAlienSwarm::GetTopDownMovementAxis()
{
	static QAngle axis = ASW_MOVEMENT_AXIS;
	return axis;
}

bool CAlienSwarm::IsHardcoreFF()
{
	return ( asw_marine_ff_absorption.GetInt() != 1 || asw_sentry_friendly_fire_scale.GetFloat() != 0.0f );
}

bool CAlienSwarm::IsOnslaught()
{
	return ( asw_horde_override.GetBool() || asw_wanderer_override.GetBool() );
}
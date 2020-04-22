//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "fmtstr.h"
#ifdef GAME_DLL
#include "gamestats.h"
#endif
#include "cs_gamestats_shared.h"

ConVar sv_noroundstats( "sv_noroundstats", "1", FCVAR_REPLICATED, "A temporary variable that can be used for disabling upload of round stats." );

const MapName_MapStatId MapName_StatId_Table[] =
{
	{"cs_assault",  CSSTAT_MAP_WINS_CS_ASSAULT,		CSSTAT_MAP_ROUNDS_CS_ASSAULT },
	{"cs_compound", CSSTAT_MAP_WINS_CS_COMPOUND,	CSSTAT_MAP_ROUNDS_CS_COMPOUND },
	{"cs_havana",   CSSTAT_MAP_WINS_CS_HAVANA,		CSSTAT_MAP_ROUNDS_CS_HAVANA },
	{"cs_italy",    CSSTAT_MAP_WINS_CS_ITALY,		CSSTAT_MAP_ROUNDS_CS_ITALY },
	{"cs_militia",  CSSTAT_MAP_WINS_CS_MILITIA,		CSSTAT_MAP_ROUNDS_CS_MILITIA },
	{"cs_office",   CSSTAT_MAP_WINS_CS_OFFICE,		CSSTAT_MAP_ROUNDS_CS_OFFICE },
	{"de_aztec",    CSSTAT_MAP_WINS_DE_AZTEC,		CSSTAT_MAP_ROUNDS_DE_AZTEC },
	{"de_cbble",    CSSTAT_MAP_WINS_DE_CBBLE,		CSSTAT_MAP_ROUNDS_DE_CBBLE },
	{"de_chateau",  CSSTAT_MAP_WINS_DE_CHATEAU,		CSSTAT_MAP_ROUNDS_DE_CHATEAU },
	{"de_dust2",    CSSTAT_MAP_WINS_DE_DUST2,		CSSTAT_MAP_ROUNDS_DE_DUST2 },
	{"de_dust",     CSSTAT_MAP_WINS_DE_DUST,		CSSTAT_MAP_ROUNDS_DE_DUST },
	{"de_inferno",  CSSTAT_MAP_WINS_DE_INFERNO,		CSSTAT_MAP_ROUNDS_DE_INFERNO },
	{"de_nuke",     CSSTAT_MAP_WINS_DE_NUKE,		CSSTAT_MAP_ROUNDS_DE_NUKE },
	{"de_piranesi", CSSTAT_MAP_WINS_DE_PIRANESI,	CSSTAT_MAP_ROUNDS_DE_PIRANESI },
	{"de_port",     CSSTAT_MAP_WINS_DE_PORT,		CSSTAT_MAP_ROUNDS_DE_PORT },
	{"de_prodigy",  CSSTAT_MAP_WINS_DE_PRODIGY,		CSSTAT_MAP_ROUNDS_DE_PRODIGY },
	{"de_tides",    CSSTAT_MAP_WINS_DE_TIDES,		CSSTAT_MAP_ROUNDS_DE_TIDES },
	{"de_train",    CSSTAT_MAP_WINS_DE_TRAIN,		CSSTAT_MAP_ROUNDS_DE_TRAIN },
	{"",			CSSTAT_UNDEFINED,				CSSTAT_UNDEFINED },
};

const WeaponName_StatId WeaponName_StatId_Table[] =
{
	{ WEAPON_DEAGLE,		CSSTAT_KILLS_DEAGLE,	CSSTAT_SHOTS_DEAGLE,	CSSTAT_HITS_DEAGLE, 	CSSTAT_DAMAGE_DEAGLE 	},
	{ WEAPON_USP,			CSSTAT_KILLS_USP,		CSSTAT_SHOTS_USP,		CSSTAT_HITS_USP,		CSSTAT_DAMAGE_USP		},
	{ WEAPON_GLOCK,			CSSTAT_KILLS_GLOCK,		CSSTAT_SHOTS_GLOCK,		CSSTAT_HITS_GLOCK,		CSSTAT_DAMAGE_GLOCK		},
	{ WEAPON_P228,			CSSTAT_KILLS_P228,		CSSTAT_SHOTS_P228,		CSSTAT_HITS_P228,		CSSTAT_DAMAGE_P228		},
	{ WEAPON_ELITE,			CSSTAT_KILLS_ELITE,		CSSTAT_SHOTS_ELITE,		CSSTAT_HITS_ELITE,		CSSTAT_DAMAGE_ELITE		},
	{ WEAPON_FIVESEVEN,		CSSTAT_KILLS_FIVESEVEN, CSSTAT_SHOTS_FIVESEVEN, CSSTAT_HITS_FIVESEVEN,	CSSTAT_DAMAGE_FIVESEVEN	},
	{ WEAPON_AWP,			CSSTAT_KILLS_AWP,		CSSTAT_SHOTS_AWP,		CSSTAT_HITS_AWP,		CSSTAT_DAMAGE_AWP		},
	{ WEAPON_AK47,			CSSTAT_KILLS_AK47,		CSSTAT_SHOTS_AK47,		CSSTAT_HITS_AK47,		CSSTAT_DAMAGE_AK47		},
	{ WEAPON_M4A1,			CSSTAT_KILLS_M4A1,		CSSTAT_SHOTS_M4A1,		CSSTAT_HITS_M4A1,		CSSTAT_DAMAGE_M4A1		},
	{ WEAPON_AUG,			CSSTAT_KILLS_AUG,		CSSTAT_SHOTS_AUG,		CSSTAT_HITS_AUG,		CSSTAT_DAMAGE_AUG		},
	{ WEAPON_SG552,			CSSTAT_KILLS_SG552,		CSSTAT_SHOTS_SG552,		CSSTAT_HITS_SG552, 		CSSTAT_DAMAGE_SG552 	},
	{ WEAPON_SG550,			CSSTAT_KILLS_SG550,		CSSTAT_SHOTS_SG550,		CSSTAT_HITS_SG550, 		CSSTAT_DAMAGE_SG550 	},
	{ WEAPON_GALIL,			CSSTAT_KILLS_GALIL,		CSSTAT_SHOTS_GALIL,		CSSTAT_HITS_GALIL, 		CSSTAT_DAMAGE_GALIL 	},
	{ WEAPON_FAMAS,			CSSTAT_KILLS_FAMAS,		CSSTAT_SHOTS_FAMAS,		CSSTAT_HITS_FAMAS, 		CSSTAT_DAMAGE_FAMAS 	},
	{ WEAPON_SCOUT,			CSSTAT_KILLS_SCOUT,		CSSTAT_SHOTS_SCOUT,		CSSTAT_HITS_SCOUT, 		CSSTAT_DAMAGE_SCOUT 	},
	{ WEAPON_G3SG1,			CSSTAT_KILLS_G3SG1,		CSSTAT_SHOTS_G3SG1,		CSSTAT_HITS_G3SG1, 		CSSTAT_DAMAGE_G3SG1 	},
	{ WEAPON_P90,			CSSTAT_KILLS_P90,		CSSTAT_SHOTS_P90,		CSSTAT_HITS_P90,		CSSTAT_DAMAGE_P90		},
	{ WEAPON_MP5NAVY,		CSSTAT_KILLS_MP5NAVY,	CSSTAT_SHOTS_MP5NAVY,	CSSTAT_HITS_MP5NAVY,	CSSTAT_DAMAGE_MP5NAVY	},
	{ WEAPON_TMP,			CSSTAT_KILLS_TMP,		CSSTAT_SHOTS_TMP,		CSSTAT_HITS_TMP,		CSSTAT_DAMAGE_TMP		},
	{ WEAPON_MAC10,			CSSTAT_KILLS_MAC10,		CSSTAT_SHOTS_MAC10,		CSSTAT_HITS_MAC10,		CSSTAT_DAMAGE_MAC10		},
	{ WEAPON_UMP45,			CSSTAT_KILLS_UMP45,		CSSTAT_SHOTS_UMP45,		CSSTAT_HITS_UMP45,		CSSTAT_DAMAGE_UMP45		},
	{ WEAPON_M3,			CSSTAT_KILLS_M3,		CSSTAT_SHOTS_M3,		CSSTAT_HITS_M3,			CSSTAT_DAMAGE_M3		},
	{ WEAPON_XM1014,		CSSTAT_KILLS_XM1014,	CSSTAT_SHOTS_XM1014,	CSSTAT_HITS_XM1014,		CSSTAT_DAMAGE_XM1014	},
	{ WEAPON_M249,			CSSTAT_KILLS_M249,		CSSTAT_SHOTS_M249,		CSSTAT_HITS_M249,		CSSTAT_DAMAGE_M249		},
	{ WEAPON_KNIFE,			CSSTAT_KILLS_KNIFE,		CSSTAT_SHOTS_KNIFE,		CSSTAT_HITS_KNIFE,		CSSTAT_DAMAGE_KNIFE 	},
	{ WEAPON_HEGRENADE,	CSSTAT_KILLS_HEGRENADE, CSSTAT_SHOTS_HEGRENADE,	CSSTAT_HITS_HEGRENADE,	CSSTAT_DAMAGE_HEGRENADE 	},

	{ WEAPON_NONE,						CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED },	// This is a sentinel value so we can loop through all the stats
};

CSStatProperty CSStatProperty_Table[] =
{
//		StatId									Steam Name							Localization Token						Client Update Priority
	{	CSSTAT_SHOTS_HIT,						"i_NumShotsHit",					"#GAMEUI_Stat_NumHits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_FIRED,						"i_NumShotsFired",					"#GAMEUI_Stat_NumShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_KILLS,							"i_Number_Of_Kills",				"#GAMEUI_Stat_NumKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_DEATHS,							"i_Number_Of_Deaths",				"#GAMEUI_Stat_NumDeaths",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_DAMAGE,							"i_Damage_Done",					"#GAMEUI_Stat_DamageDone",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_NUM_BOMBS_PLANTED,				"i_Number_Of_PlantedBombs",			"#GAMEUI_Stat_NumPlantedBombs",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_NUM_BOMBS_DEFUSED,				"i_Number_Of_DefusedBombs",			"#GAMEUI_Stat_NumDefusedBombs",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_PLAYTIME,						"i_Time_Played",					"#GAMEUI_Stat_TimePlayed",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_ROUNDS_WON,						"total_wins",						"#GAMEUI_Stat_TotalWins",				CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_T_ROUNDS_WON,					NULL,								NULL,									CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_CT_ROUNDS_WON,					NULL,								NULL,									CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_ROUNDS_PLAYED,					"i_Total_Rounds",					"#GAMEUI_Stat_TotalRounds",				CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_PISTOLROUNDS_WON,				"total_wins_pistolround",			"#GAMEUI_Stat_PistolRoundWins",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MONEY_EARNED,					"i_Money_Earned",					"#GAMEUI_Stat_MoneyEarned",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_OBJECTIVES_COMPLETED,			NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_BOMBS_DEFUSED_WITHKIT,			NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_KILLS_DEAGLE,					"total_kills_deagle",				"#GAMEUI_Stat_DeagleKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_USP,						"total_kills_usp",					"#GAMEUI_Stat_USPKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_GLOCK,						"total_kills_glock",				"#GAMEUI_Stat_GlockKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_P228,						"total_kills_p228",					"#GAMEUI_Stat_P228Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_ELITE,						"total_kills_elite",				"#GAMEUI_Stat_EliteKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_FIVESEVEN,					"total_kills_fiveseven",			"#GAMEUI_Stat_FiveSevenKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_AWP,						"total_kills_awp",					"#GAMEUI_Stat_AWPKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_AK47,						"total_kills_ak47",					"#GAMEUI_Stat_AK47Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_M4A1,						"total_kills_m4a1",					"#GAMEUI_Stat_M4A1Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_AUG,						"total_kills_aug",					"#GAMEUI_Stat_AUGKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SG552,						"total_kills_sg552",				"#GAMEUI_Stat_SG552Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SG550,						"total_kills_sg550",				"#GAMEUI_Stat_SG550Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_GALIL,						"total_kills_galil",				"#GAMEUI_Stat_GALILKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_FAMAS,						"total_kills_famas",				"#GAMEUI_Stat_FAMASKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SCOUT,						"total_kills_scout",				"#GAMEUI_Stat_ScoutKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_G3SG1,						"total_kills_g3sg1",				"#GAMEUI_Stat_G3SG1Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_P90,						"total_kills_p90",					"#GAMEUI_Stat_P90Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_MP5NAVY,					"total_kills_mp5navy",				"#GAMEUI_Stat_MP5NavyKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_TMP,						"total_kills_tmp",					"#GAMEUI_Stat_TMPKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_MAC10,						"total_kills_mac10",				"#GAMEUI_Stat_MAC10Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_UMP45,						"total_kills_ump45",				"#GAMEUI_Stat_UMP45Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_M3,						"total_kills_m3",					"#GAMEUI_Stat_M3Kills",					CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_XM1014,					"total_kills_xm1014",				"#GAMEUI_Stat_XM1014Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_M249,						"total_kills_m249",					"#GAMEUI_Stat_M249Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_KNIFE,						"total_kills_knife",				"#GAMEUI_Stat_KnifeKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_HEGRENADE,					"total_kills_hegrenade",			"#GAMEUI_Stat_HEGrenadeKills",			CSSTAT_PRIORITY_HIGH,			},

	{	CSSTAT_SHOTS_DEAGLE,					"total_shots_deagle",				"#GAMEUI_Stat_DeagleShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_USP,						"total_shots_usp",					"#GAMEUI_Stat_USPShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_GLOCK,						"total_shots_glock",				"#GAMEUI_Stat_GlockShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_P228,						"total_shots_p228",					"#GAMEUI_Stat_P228Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_ELITE,						"total_shots_elite",				"#GAMEUI_Stat_EliteShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_FIVESEVEN,					"total_shots_fiveseven",			"#GAMEUI_Stat_FiveSevenShots",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_AWP,						"total_shots_awp",					"#GAMEUI_Stat_AWPShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_AK47,						"total_shots_ak47",					"#GAMEUI_Stat_AK47Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_M4A1,						"total_shots_m4a1",					"#GAMEUI_Stat_M4A1Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_AUG,						"total_shots_aug",					"#GAMEUI_Stat_AUGShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SG552,						"total_shots_sg552",				"#GAMEUI_Stat_SG552Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SG550,						"total_shots_sg550",				"#GAMEUI_Stat_SG550Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_GALIL,						"total_shots_galil",				"#GAMEUI_Stat_GALILShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_FAMAS,						"total_shots_famas",				"#GAMEUI_Stat_FAMASShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SCOUT,						"total_shots_scout",				"#GAMEUI_Stat_ScoutShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_G3SG1,						"total_shots_g3sg1",				"#GAMEUI_Stat_G3SG1Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_P90,						"total_shots_p90",					"#GAMEUI_Stat_P90Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_MP5NAVY,					"total_shots_mp5navy",				"#GAMEUI_Stat_MP5NavyShots",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_TMP,						"total_shots_tmp",					"#GAMEUI_Stat_TMPShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_MAC10,						"total_shots_mac10",				"#GAMEUI_Stat_MAC10Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_UMP45,						"total_shots_ump45",				"#GAMEUI_Stat_UMP45Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_M3,						"total_shots_m3",					"#GAMEUI_Stat_M3Shots",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_XM1014,					"total_shots_xm1014",				"#GAMEUI_Stat_XM1014Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_M249,						"total_shots_m249",					"#GAMEUI_Stat_M249Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_KNIFE,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_SHOTS_HEGRENADE,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},	

	{	CSSTAT_HITS_DEAGLE,						"total_hits_deagle",				"#GAMEUI_Stat_DeagleHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_USP,						"total_hits_usp",					"#GAMEUI_Stat_USPHits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_GLOCK,						"total_hits_glock",					"#GAMEUI_Stat_GlockHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_P228,						"total_hits_p228",					"#GAMEUI_Stat_P228Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_ELITE,						"total_hits_elite",					"#GAMEUI_Stat_EliteHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_FIVESEVEN,					"total_hits_fiveseven",				"#GAMEUI_Stat_FiveSevenHits",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_AWP,						"total_hits_awp",					"#GAMEUI_Stat_AWPHits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_AK47,						"total_hits_ak47",					"#GAMEUI_Stat_AK47Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_M4A1,						"total_hits_m4a1",					"#GAMEUI_Stat_M4A1Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_AUG,						"total_hits_aug",					"#GAMEUI_Stat_AUGHits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SG552,						"total_hits_sg552",					"#GAMEUI_Stat_SG552Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SG550,						"total_hits_sg550",					"#GAMEUI_Stat_SG550Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_GALIL,						"total_hits_galil",					"#GAMEUI_Stat_GALILHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_FAMAS,						"total_hits_famas",					"#GAMEUI_Stat_FAMASHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SCOUT,						"total_hits_scout",					"#GAMEUI_Stat_ScoutHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_G3SG1,						"total_hits_g3sg1",					"#GAMEUI_Stat_G3SG1Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_P90,						"total_hits_p90",					"#GAMEUI_Stat_P90Hits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_MP5NAVY,					"total_hits_mp5navy",				"#GAMEUI_Stat_MP5NavyHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_TMP,						"total_hits_tmp",					"#GAMEUI_Stat_TMPHits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_MAC10,						"total_hits_mac10",					"#GAMEUI_Stat_MAC10Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_UMP45,						"total_hits_ump45",					"#GAMEUI_Stat_UMP45Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_M3,							"total_hits_m3",					"#GAMEUI_Stat_M3Hits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_XM1014,						"total_hits_xm1014",				"#GAMEUI_Stat_XM1014Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_M249,						"total_hits_m249",					"#GAMEUI_Stat_M249Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_KNIFE,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_HITS_HEGRENADE,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},	

	{	CSSTAT_DAMAGE_DEAGLE,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_USP,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_GLOCK,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_P228,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_ELITE,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_FIVESEVEN,				NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_AWP,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_AK47,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_M4A1,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_AUG,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SG552,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SG550,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_GALIL,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_FAMAS,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SCOUT,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_G3SG1,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_P90,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_MP5NAVY,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_TMP,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_MAC10,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_UMP45,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_M3,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_XM1014,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_M249,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_KNIFE,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_HEGRENADE,				NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},	

	{	CSSTAT_KILLS_HEADSHOT,					"total_kills_headshot",				"#GAMEUI_Stat_HeadshotKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_ENEMY_BLINDED,				"total_kills_enemy_blinded",		"#GAMEUI_Stat_BlindedEnemyKills",		CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_WHILE_BLINDED,				NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_KILLS_WITH_LAST_ROUND,			NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_KILLS_ENEMY_WEAPON,				"total_kills_enemy_weapon",			"#GAMEUI_Stat_EnemyWeaponKills",		CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_KNIFE_FIGHT,				"total_kills_knife_fight",			"#GAMEUI_Stat_KnifeFightKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_WHILE_DEFENDING_BOMB,		NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_DECAL_SPRAYS,					"total_decal_sprays",				"#GAMEUI_Stat_DecalSprays",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_TOTAL_JUMPS,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_NIGHTVISION_DAMAGE,				"total_nightvision_damage",			"#GAMEUI_Stat_NightvisionDamage",		CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_KILLS_WHILE_LAST_PLAYER_ALIVE,	NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_KILLS_ENEMY_WOUNDED,				NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_FALL_DAMAGE,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_NUM_HOSTAGES_RESCUED,			"i_Number_Of_RescuedHostages",		"#GAMEUI_Stat_NumRescuedHostages",		CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_NUM_BROKEN_WINDOWS,				"i_Number_Of_BrokenWindows",		"#GAMEUI_Stat_NumBrokenWindows",		CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_PROPSBROKEN_ALL,					NULL,								NULL,									CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_PROPSBROKEN_MELON,				NULL,								NULL,									CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_PROPSBROKEN_OFFICEELECTRONICS,	NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_PROPSBROKEN_OFFICERADIO,			NULL,								NULL,									CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_PROPSBROKEN_OFFICEJUNK,			NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_PROPSBROKEN_ITALY_MELON,			NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER,		"total_kills_against_zoomed_sniper","#GAMEUI_Stat_ZoomedSniperKills",		CSSTAT_PRIORITY_HIGH,			},

	{	CSSTAT_WEAPONS_DONATED,					"total_weapons_donated",			"#GAMEUI_Stat_WeaponsDonated",			CSSTAT_PRIORITY_HIGH,			},

	{	CSSTAT_ITEMS_PURCHASED,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_MONEY_SPENT,						NULL,								NULL,									CSSTAT_PRIORITY_LOW,			},

	{	CSSTAT_DOMINATIONS,						"total_dominations",				"#GAMEUI_Stat_Dominations",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_DOMINATION_OVERKILLS,			"total_domination_overkills",		"#GAMEUI_Stat_DominationOverkills",		CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_REVENGES,						"total_revenges",					"#GAMEUI_Stat_Revenges",				CSSTAT_PRIORITY_HIGH,			},

	{	CSSTAT_MVPS,							"total_mvps",						"#GAMEUI_Stat_MVPs",					CSSTAT_PRIORITY_HIGH,			},

	{	CSSTAT_GRENADE_DAMAGE,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_GRENADE_POSTHUMOUSKILLS,			NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_GRENADES_THROWN,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
    {	CSTAT_ITEMS_DROPPED_VALUE,              NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_MAP_WINS_CS_ASSAULT,				"total_wins_map_cs_assault",		"#GAMEUI_Stat_WinsMapCSAssault",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_CS_COMPOUND,			"total_wins_map_cs_compound",		"#GAMEUI_Stat_WinsMapCSCompound",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_CS_HAVANA,				"total_wins_map_cs_havana",			"#GAMEUI_Stat_WinsMapCSHavana",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_CS_ITALY,				"total_wins_map_cs_italy",			"#GAMEUI_Stat_WinsMapCSItaly",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_CS_MILITIA,				"total_wins_map_cs_militia",		"#GAMEUI_Stat_WinsMapCSMilitia",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_CS_OFFICE,				"total_wins_map_cs_office",			"#GAMEUI_Stat_WinsMapCSOffice",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_AZTEC,				"total_wins_map_de_aztec",			"#GAMEUI_Stat_WinsMapDEAztec",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_CBBLE,				"total_wins_map_de_cbble",			"#GAMEUI_Stat_WinsMapDECbble",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_CHATEAU,				"total_wins_map_de_chateau",		"#GAMEUI_Stat_WinsMapDEChateau",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_DUST2,				"total_wins_map_de_dust2",			"#GAMEUI_Stat_WinsMapDEDust2",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_DUST,				"total_wins_map_de_dust",			"#GAMEUI_Stat_WinsMapDEDust",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_INFERNO,				"total_wins_map_de_inferno",		"#GAMEUI_Stat_WinsMapDEInferno",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_NUKE,				"total_wins_map_de_nuke",			"#GAMEUI_Stat_WinsMapDENuke",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_PIRANESI,			"total_wins_map_de_piranesi",		"#GAMEUI_Stat_WinsMapDEPiranesi",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_PORT,				"total_wins_map_de_port",			"#GAMEUI_Stat_WinsMapDEPort",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_PRODIGY,				"total_wins_map_de_prodigy",		"#GAMEUI_Stat_WinsMapDEProdigy",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_TIDES,				"total_wins_map_de_tides",			"#GAMEUI_Stat_WinsMapDETides",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_TRAIN,				"total_wins_map_de_train",			"#GAMEUI_Stat_WinsMapDETrain",			CSSTAT_PRIORITY_ENDROUND,		},

	{	CSSTAT_MAP_ROUNDS_CS_ASSAULT,			"total_rounds_map_cs_assault",		"#GAMEUI_Stat_RoundsMapCSAssault",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_CS_COMPOUND,			"total_rounds_map_cs_compound",		"#GAMEUI_Stat_RoundsMapCSCompound",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_CS_HAVANA,			"total_rounds_map_cs_havana",		"#GAMEUI_Stat_RoundsMapCSHavana",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_CS_ITALY,				"total_rounds_map_cs_italy",		"#GAMEUI_Stat_RoundsMapCSItaly",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_CS_MILITIA,			"total_rounds_map_cs_militia",		"#GAMEUI_Stat_RoundsMapCSMilitia",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_CS_OFFICE,			"total_rounds_map_cs_office",		"#GAMEUI_Stat_RoundsMapCSOffice",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_AZTEC,				"total_rounds_map_de_aztec",		"#GAMEUI_Stat_RoundsMapDEAztec",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_CBBLE,				"total_rounds_map_de_cbble",		"#GAMEUI_Stat_RoundsMapDECbble",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_CHATEAU,			"total_rounds_map_de_chateau",		"#GAMEUI_Stat_RoundsMapDEChateau",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_DUST2,				"total_rounds_map_de_dust2",		"#GAMEUI_Stat_RoundsMapDEDust2",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_DUST,				"total_rounds_map_de_dust",			"#GAMEUI_Stat_RoundsMapDEDust",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_INFERNO,			"total_rounds_map_de_inferno",		"#GAMEUI_Stat_RoundsMapDEInferno",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_NUKE,				"total_rounds_map_de_nuke",			"#GAMEUI_Stat_RoundsMapDENuke",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_PIRANESI,			"total_rounds_map_de_piranesi",		"#GAMEUI_Stat_RoundsMapDEPiranesi",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_PORT,				"total_rounds_map_de_port",			"#GAMEUI_Stat_RoundsMapDEPort",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_PRODIGY,			"total_rounds_map_de_prodigy",		"#GAMEUI_Stat_RoundsMapDEProdigy",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_TIDES,				"total_rounds_map_de_tides",		"#GAMEUI_Stat_RoundsMapDETides",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_TRAIN,				"total_rounds_map_de_train",		"#GAMEUI_Stat_RoundsMapDETrain",		CSSTAT_PRIORITY_ENDROUND,		},

	// only client tracks these
	{	CSSTAT_LASTMATCH_T_ROUNDS_WON,			"last_match_t_wins",				"#GameUI_Stat_LastMatch_TWins",			CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_CT_ROUNDS_WON,			"last_match_ct_wins",				"#GameUI_Stat_LastMatch_CTWins",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_ROUNDS_WON,			"last_match_wins",					"#GameUI_Stat_LastMatch_RoundsWon",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_KILLS,					"last_match_kills",					"#GameUI_Stat_LastMatch_Kills",			CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_DEATHS,				"last_match_deaths",				"#GameUI_Stat_LastMatch_Deaths",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_MVPS,					"last_match_mvps",					"#GameUI_Stat_LastMatch_MVPS",			CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_DAMAGE,				"last_match_damage",				"#GameUI_Stat_LastMatch_Damage",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_MONEYSPENT,			"last_match_money_spent",			"#GameUI_Stat_LastMatch_MoneySpent",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_DOMINATIONS,			"last_match_dominations",			"#GameUI_Stat_LastMatch_Dominations",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_REVENGES,				"last_match_revenges",				"#GameUI_Stat_LastMatch_Revenges",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_MAX_PLAYERS,			"last_match_max_players",			"#GameUI_Stat_LastMatch_MaxPlayers",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_FAVWEAPON_ID,			"last_match_favweapon_id",			"#GameUI_Stat_LastMatch_FavWeapon",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_FAVWEAPON_SHOTS,		"last_match_favweapon_shots",		"#GameUI_Stat_LastMatch_FavWeaponShots",CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_FAVWEAPON_HITS,		"last_match_favweapon_hits",		"#GameUI_Stat_LastMatch_FavWeaponHits",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_FAVWEAPON_KILLS,		"last_match_favweapon_kills",		"#GameUI_Stat_LastMatch_FavWeaponKills",CSSTAT_PRIORITY_NEVER },

	{	CSSTAT_UNDEFINED	},	// sentinel
};

const WeaponName_StatId& GetWeaponTableEntryFromWeaponId( CSWeaponID id )
{
	int i;

	//yes this for loop has no statement block. All we are doing is incrementing i to the appropriate point.
	for (i = 0 ; WeaponName_StatId_Table[i].weaponId != WEAPON_NONE ; ++i)
	{
		if (WeaponName_StatId_Table[i].weaponId == id )
		{
			break;
		}
	}
	return WeaponName_StatId_Table[i];
}

void StatsCollection_t::Aggregate( const StatsCollection_t& other )
{
	for ( int i = 0; i < CSSTAT_MAX; ++i )
	{
		m_iValue[i] += other[i];
	}
}

//=============================================================================
//
// Helper functions for creating key values
//
void AddDataToKV( KeyValues* pKV, const char* name, int data )
{
	pKV->SetInt( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, uint64 data )
{
	pKV->SetUint64( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, float data )
{
	pKV->SetFloat( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, bool data )
{
	pKV->SetInt( name, data ? true : false );
}
void AddDataToKV( KeyValues* pKV, const char* name, const char* data )
{
	pKV->SetString( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, const Color& data )
{
	pKV->SetColor( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, short data )
{
	pKV->SetInt( name, data );
}
void AddDataToKV( KeyValues* pKV, const char* name, unsigned data )
{
	pKV->SetInt( name, data );
}
void AddPositionDataToKV( KeyValues* pKV, const char* name, const Vector &data )
{
	// Append the data name to the member
	pKV->SetFloat( CFmtStr("%s%s", name, "_X"), data.x );
	pKV->SetFloat( CFmtStr("%s%s", name, "_Y"), data.y );
	pKV->SetFloat( CFmtStr("%s%s", name, "_Z"), data.z );
}

//=============================================================================//

//=============================================================================
//
// Helper functions for creating key values from arrays
//
void AddArrayDataToKV( KeyValues* pKV, const char* name, const short *data, unsigned size )
{
	for( unsigned i=0; i<size; ++i )
		pKV->SetInt( CFmtStr("%s_%d", name, i) , data[i] );
}
void AddArrayDataToKV( KeyValues* pKV, const char* name, const byte *data, unsigned size )
{
	for( unsigned i=0; i<size; ++i )
		pKV->SetInt( CFmtStr("%s_%d", name, i), data[i] );
}
void AddArrayDataToKV( KeyValues* pKV, const char* name, const unsigned *data, unsigned size )
{
	for( unsigned i=0; i<size; ++i )
		pKV->SetInt( CFmtStr("%s_%d", name, i), data[i] );
}
void AddStringDataToKV( KeyValues* pKV, const char* name, const char*data )
{
	if( name == NULL )
		return;

	pKV->SetString( name, data );
}
//=============================================================================//


void IGameStatTracker::PrintGamestatMemoryUsage( void )
{
	StatContainerList_t* pStatList = GetStatContainerList();
	if( !pStatList )
		return;

	int iListSize = pStatList->Count();

	// For every stat list being tracked, print out its memory usage
	for( int i=0; i < iListSize; ++i )
	{
		pStatList->operator []( i )->PrintMemoryUsage();
	}
}

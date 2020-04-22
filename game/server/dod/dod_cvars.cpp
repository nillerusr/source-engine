//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "dod_cvars.h"


/*
// Gameplay
cvar_t	cvar_forcechasecam			= {"mp_forcechasecam",	"1",	FCVAR_SERVER };
cvar_t	cvar_forcecamera			= {"mp_forcecamera",	"1",	FCVAR_SERVER };


// Clan cvars
cvar_t  cvar_waveTime				= {"mp_clan_respawntime",			"0",		FCVAR_SERVER };
cvar_t  cvar_clanRestartRound		= {"mp_clan_restartround",			"0",		FCVAR_SERVER };
cvar_t  cvar_clanReadyRestart		= {"mp_clan_readyrestart",			"0",		FCVAR_SERVER };
cvar_t  cvar_clanMatchWarmup		= {"mp_clan_match_warmup",			"0",		FCVAR_SERVER };
cvar_t  cvar_clanMatch				= {"mp_clan_match",					"0",		FCVAR_SERVER };
cvar_t  cvar_clanTimer				= {"mp_clan_timer",					"20",		FCVAR_SERVER };
cvar_t  cvar_clanScoring			= {"mp_clan_scoring",				"0",		FCVAR_SERVER };
cvar_t	cvar_clanScoringValuesAllies	= {"mp_clan_scoring_values_allies", "111111", FCVAR_SERVER };
cvar_t	cvar_clanScoringValuesAxis	= {"mp_clan_scoring_values_axis",	"111111", FCVAR_SERVER };
cvar_t  cvar_clanScoringDelay		= {"mp_clan_scoring_delay",			"60",		FCVAR_SERVER };
cvar_t  cvar_clanReadyString		= {"mp_clan_ready_signal",			"ready",	FCVAR_SERVER };
cvar_t  cvar_clanScoringBonusAllies	= {"mp_clan_scoring_bonus_allies",	"-1",		FCVAR_SERVER };
cvar_t  cvar_clanScoringBonusAxis	= {"mp_clan_scoring_bonus_axis",	"-1",		FCVAR_SERVER };


// Other Server Cvars ( Not FCVAR_SERVER! )
//===================
cvar_t  cvar_nummapmarkers			= {"mp_nummapmarkers",		"1" };
cvar_t  cvar_markerstaytime			= {"mp_markerstaytime",		"30"};

cvar_t  cvar_teamSpectators			= {"mp_allowspectators",	"1" };
cvar_t	cvar_logScores				= {"mp_log_scores",			"0" };
cvar_t  cvar_logScoresDelay			= {"mp_log_scores_delay",	"60"};
cvar_t  cvar_tkPenalty				= {"mp_tkpenalty",			"6" };
* now mp_limitteams * cvar_t  cvar_teamOver				= {"mp_teamlimit",			"2" };
cvar_t  cvar_deathMsg				= {"mp_deathmsg",			"1" };
cvar_t  cvar_chatMsg				= {"mp_chatmsg",			"1" };
cvar_t  cvar_alliesclasses			= {"mp_alliesclasses",		"-1"};
cvar_t  cvar_axisclasses			= {"mp_axisclasses",		"-1"};
*/

	ConVar mp_winlimit( "mp_winlimit", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Max score one team can reach before server changes maps", true, 0, false, 0 );
	
	ConVar mp_clan_restartround( "mp_clan_restartround", "0", FCVAR_GAMEDLL, "If non-zero, the round will restart in the specified number of seconds" );	

	ConVar mp_limitteams( 
		"mp_limitteams", 
		"2", 
		FCVAR_REPLICATED,
		"Max # of players 1 team can have over another",
		true, 1,	// min value
		true, 20	// max value
		);

	ConVar mp_autokick(
		"mp_autokick",
		"0",
		FCVAR_REPLICATED,
		"Kick idle/team-killing players" );

	ConVar mp_combinemglimits(
		"mp_combinemglimits",
		"0",
		FCVAR_REPLICATED,
		"Set to 1 to combine the class limit cvars for mg34 and mg42. New limit is sum of two" );

	// Class limit cvars
	// welcome to ugly-town

	ConVar	mp_limitAlliesRifleman(		"mp_limit_allies_rifleman", "-1", FCVAR_REPLICATED, "Class limit for team: Allies class: Rifleman" );
	ConVar	mp_limitAlliesAssault(		"mp_limit_allies_assault", "-1", FCVAR_REPLICATED, "Class limit for team: Allies class: Assault" );
	ConVar	mp_limitAlliesSupport(		"mp_limit_allies_support", "-1", FCVAR_REPLICATED, "Class limit for team: Allies class: Support" );
	ConVar	mp_limitAlliesSniper(		"mp_limit_allies_sniper", "-1", FCVAR_REPLICATED, "Class limit for team: Allies class: Sniper" );
	ConVar	mp_limitAlliesMachinegun(	"mp_limit_allies_mg", "-1", FCVAR_REPLICATED, "Class limit for team: Allies class: Machinegunner" );
	ConVar	mp_limitAlliesRocket(		"mp_limit_allies_rocket", "-1", FCVAR_REPLICATED, "Class limit for team: Allies class: Rocket" );

	ConVar	mp_limitAxisRifleman(		"mp_limit_axis_rifleman", "-1", FCVAR_REPLICATED, "Class limit for team: Axis class: Rifleman" );
	ConVar	mp_limitAxisAssault(		"mp_limit_axis_assault", "-1", FCVAR_REPLICATED, "Class limit for team: Axis class: Assault" );
	ConVar	mp_limitAxisSupport(		"mp_limit_axis_support", "-1", FCVAR_REPLICATED, "Class limit for team: Axis class: Support" );
	ConVar	mp_limitAxisSniper(			"mp_limit_axis_sniper", "-1", FCVAR_REPLICATED, "Class limit for team: Axis class: Sniper" );
	ConVar	mp_limitAxisMachinegun(		"mp_limit_axis_mg", "-1", FCVAR_REPLICATED, "Class limit for team: Axis class: Machinegunner" );
	ConVar	mp_limitAxisRocket(			"mp_limit_axis_rocket", "-1", FCVAR_REPLICATED, "Class limit for team: Axis class: Rocket" );

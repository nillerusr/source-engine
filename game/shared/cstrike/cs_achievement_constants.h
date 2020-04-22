//========= Copyright Valve Corporation, All rights reserved. ============//
//-------------------------------------------------------------
// File:		cs_achievement_constants.h
// Desc: 		Declare contants used by achievements (mostly) in one location for simpler tweaking
// Author: 		Peter Freese <peter@hiddenpath.com>
// Date: 		2009/03/11
// Copyright:	© 2009 Hidden Path Entertainment
//-------------------------------------------------------------

#ifndef CS_ACHIEVEMENT_CONSTANTS_H
#define CS_ACHIEVEMENT_CONSTANTS_H
#ifdef _WIN32
#pragma once
#endif

namespace AchievementConsts
{
	const int	DefaultMinOpponentsForAchievement			= 5;
	const int	KillingSpree_Kills							= 5;
	const float KillingSpree_WindowTime						= 15.0f;
	const float KillingSpreeEnder_TimeWindow				= 5.0f;
	const int	KillEnemyTeam_MinKills						= 5;
	const int	LastPlayerAlive_MinPlayersOnTeam			= 5;
	const int	KillsWithMultipleGuns_MinWeapons			= 5;
	const float	BombDefuseCloseCall_MaxTimeRemaining		= 1.0f;
	const int	KillLowDamage_MaxHealthLeft					= 5;
    const int   DamageNoKill_MaxHealthLeftOnKill            = 5;
	const float	BombDefuseNeededKit_MaxTime					= 5.0f;
	const float	FastBombPlant_Time							= 25.0f;
	const int	KillEnemiesWhileBlind_Kills					= 1;
    const int   KillEnemiesWhileBlindHard_Kills				= 2;
	const int	SurviveGrenade_MinDamage                    = 80;
	const int	KillWhenAtLowHealth_MaxHealth               = 1;
	const int	GrenadeMultiKill_MinKills					= 3;
	const int	BombMultiKill_MinKills						= 5;
	const float	FastRoundWin_Time							= 30.0f;	
	const int	UnstoppableForce_Kills						= 10;
	const int	BreakPropsInRound_Props						= 15;
	const int	HeadshotsInRound_Kills						= 5;	
	const int	BreakWindowsInOfficeRound_Windows			= 14;
	const float	FastHostageRescue_Time						= 90.0f;
	const int	SurviveManyAttacks_NumberDamagingPlayers	= 5;
    const float KillInAir_MinimumHeight                     = 100.0f; //100-120 is probably best. Also used for killing while in the air
    const float KillBombPickup_MaxTime                      = 3.0f;
    const int   WinRoundsWithoutBuying_Rounds               = 10;
    const int   ConcurrentDominations_MinDominations        = 3;
    const int   ExtendedDomination_AdditionalKills			= 4;
    const int   SameUniform_MinPlayers                      = 5;
    const int   FriendsSameUniform_MinPlayers               = 4;
	const float KillEnemyNearBomb_MaxDistance				= 480.0f;
	const int   GrenadeDamage_MinDamage						= 200;
}

#endif // CS_ACHIEVEMENT_CONSTANTS_H

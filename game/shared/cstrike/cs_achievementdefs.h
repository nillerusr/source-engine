//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared CS definitions.
//
//=============================================================================//

#ifndef CS_ACHIEVEMENTDEFS_H
#define CS_ACHIEVEMENTDEFS_H
#ifdef _WIN32
#pragma once
#endif



//=============================================================================
// Achievement ID Definitions
//=============================================================================


typedef enum
{
    CSInvalidAchievement = -1,

    // Bomb-related Achievements
    CSBombAchievementsStart = 1000,        // First bomb-related achievement

	CSWinBombPlant,
	CSWinBombDefuse,
	CSDefuseAndNeededKit,
    CSBombDefuseCloseCall,
    CSKilledDefuser,
    CSPlantBombWithin25Seconds,
	CSKillBombPickup,
    CSBombMultikill,
    CSGooseChase,
    CSWinBombPlantAfterRecovery,
    CSDefuseDefense,
	CSPlantBombsLow,
	CSDefuseBombsLow,

    CSBombAchievementsEnd,                  // Must be after last bomb-related achievement


    // Hostage-related Achievements
    CSHostageAchievementsStart = 2000,      // First hostage-related achievement

    CSRescueAllHostagesInARound,
	CSKilledRescuer,
	CSFastHostageRescue,
    CSRescueHostagesLow,
    CSRescueHostagesMid,

    CSHostageAchievmentEnd,                 // Must be after last hostage-related achievement

    // General Kill Achievements
    CSKillAchievementsStart = 3000,         // First kill-related achievement

    CSEnemyKillsLow,
    CSEnemyKillsMed,
    CSEnemyKillsHigh,
	CSSurvivedHeadshotDueToHelmet,
    CSKillEnemyReloading,
    CSKillingSpree,
    CSKillsWithMultipleGuns,
    CSHeadshots,
	CSAvengeFriend,
	CSSurviveGrenade,
	CSDominationsLow,
	CSDominationsHigh,
	CSRevengesLow,
	CSRevengesHigh,
	CSDominationOverkillsLow,
	CSDominationOverkillsHigh,
	CSDominationOverkillsMatch,
	CSExtendedDomination,
	CSConcurrentDominations,
    CSKillEnemyBlinded,
    CSKillEnemiesWhileBlind,
    CSKillEnemiesWhileBlindHard,
    CSKillsEnemyWeapon,
    CSKillWithEveryWeapon,
    CSWinKnifeFightsLow,
    CSWinKnifeFightsHigh,
    CSKilledDefuserWithGrenade,
    CSKillSniperWithSniper,
    CSKillSniperWithKnife,
    CSHipShot,
    CSKillSnipers,
    CSKillWhenAtLowHealth,
	CSPistolRoundKnifeKill,
	CSWinDualDuel,
    CSGrenadeMultikill,
    CSKillWhileInAir,
    CSKillEnemyInAir,
    CSKillerAndEnemyInAir,
	CSKillEnemyWithFormerGun,
	CSKillTwoWithOneShot,
	CSSnipeTwoFromSameSpot,

    CSKillAchievementEnd,                   // Must be after last kill-related achievement

    // Weapon-related Achievements
    CSWeaponAchievementsStart = 4000,       // First weapon-related achievement

    CSEnemyKillsDeagle,
    CSEnemyKillsUSP,
    CSEnemyKillsGlock,
    CSEnemyKillsP228,
    CSEnemyKillsElite,
    CSEnemyKillsFiveSeven,
    CSEnemyKillsAWP,
    CSEnemyKillsAK47,
    CSEnemyKillsM4A1,
    CSEnemyKillsAUG,
    CSEnemyKillsSG552,
    CSEnemyKillsSG550,
    CSEnemyKillsGALIL,
    CSEnemyKillsFAMAS,
    CSEnemyKillsScout,
    CSEnemyKillsG3SG1,
    CSEnemyKillsP90,
    CSEnemyKillsMP5NAVY,
    CSEnemyKillsTMP,
    CSEnemyKillsMAC10,
    CSEnemyKillsUMP45,
    CSEnemyKillsM3,
    CSEnemyKillsXM1014,
    CSEnemyKillsM249,
    CSEnemyKillsKnife,
    CSEnemyKillsHEGrenade,
    CSMetaPistol,
    CSMetaRifle,
    CSMetaSMG,
    CSMetaShotgun,
    CSMetaWeaponMaster,

    CSWeaponAchievementsEnd,                // Must be after last weapon-related achievement

    // General Achievements
    CSGeneralAchievementsStart = 5000,      // First general achievement

    CSWinRoundsLow,
    CSWinRoundsMed,
    CSWinRoundsHigh,
	CSMoneyEarnedLow,
	CSMoneyEarnedMed,
	CSMoneyEarnedHigh,
    CSGiveDamageLow,
    CSGiveDamageMed,
    CSGiveDamageHigh,
    CSPosthumousGrenadeKill,
    CSKillEnemyTeam,
    CSLastPlayerAlive,
    CSKillEnemyLastBullet,
    CSKillingSpreeEnder,
    CSDamageNoKill,
    CSKillLowDamage,
    CSSurviveManyAttacks,
    CSLosslessExtermination,
    CSFlawlessVictory,
    CSDecalSprays,
    CSBreakWindows,
    CSBreakProps,
    CSUnstoppableForce,
    CSImmovableObject,
    CSHeadshotsInRound,
    CSWinPistolRoundsLow,
    CSWinPistolRoundsMed,
    CSWinPistolRoundsHigh,
    CSFastRoundWin,
    CSNightvisionDamage,
    CSSilentWin,
    CSBloodlessVictory,
    CSDonateWeapons,
    CSWinRoundsWithoutBuying,
    CSSameUniform,
    CSFriendsSameUniform,
    CSCauseFriendlyFireWithFlashbang,
	CSWinClanMatch,
	CSCollectHolidayGifts,

    CSGeneralAchievementsEnd,               // Must be after last general achievement

    CSWinMapAchievementsStart = 6000,

    CSWinMapCS_ASSAULT,
    CSWinMapCS_COMPOUND,
    CSWinMapCS_HAVANA,  
    CSWinMapCS_ITALY,
    CSWinMapCS_MILITIA,
    CSWinMapCS_OFFICE,
    CSWinMapDE_AZTEC,
    CSWinMapDE_CBBLE,
    CSWinMapDE_CHATEAU,
    CSWinMapDE_DUST,
    CSWinMapDE_DUST2,
    CSWinMapDE_INFERNO,
    CSWinMapDE_NUKE,
    CSWinMapDE_PIRANESI,
    CSWinMapDE_PORT,
    CSWinMapDE_PRODIGY,
    CSWinMapDE_TIDES,
    CSWinMapDE_TRAIN,

    CSWinMapAchievementsEnd                 //Must be after last map-based achievement

} eCSAchievementType;


#endif // CS_ACHIEVEMENTDEFS_H

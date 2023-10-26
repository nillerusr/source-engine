//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ASW_PLAYER_SHARED_H
#define ASW_PLAYER_SHARED_H

#ifdef _WIN32
#pragma once
#endif

// Shared header file for players
#if defined( CLIENT_DLL )
	#define CASW_Player C_ASW_Player
#endif

#define ASW_USE_KEY_HOLD_SENTRY_TIME 2.0

enum
{
	ASW_USE_RELEASE_QUICK = 0,
	ASW_USE_HOLD_START,
	ASW_USE_HOLD_RELEASE_FULL,
};

#define ASW_PROMOTION_CAP 6
#define ASW_NUM_EXPERIENCE_LEVELS 26

extern int g_iLevelExperience[ ASW_NUM_EXPERIENCE_LEVELS ];
extern float g_flPromotionXPScale[ ASW_PROMOTION_CAP + 1 ];

int LevelFromXP( int iExperience, int iPromotion );

enum CASW_Earned_XP_t
{
	ASW_XP_MISSION,
	ASW_XP_KILLS,
	ASW_XP_TIME,
	ASW_XP_FRIENDLY_FIRE,
	ASW_XP_DAMAGE_TAKEN,
	ASW_XP_HEALING,
	ASW_XP_HACKING,
	ASW_XP_MEDALS,		// medals/achievements

	ASW_XP_TOTAL,		// must be after individual types

	ASW_NUM_XP_TYPES
};

#endif // ASW_PLAYER_SHARED_H

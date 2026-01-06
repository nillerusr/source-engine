//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Achievements for Entropy : Zero 2.
// The implementation is heavily based on the achievements from Entropy : Zero
//
//=============================================================================

#include "cbase.h"

#ifdef GAME_DLL

#include "achievementmgr.h"
#include "baseachievement.h"
#include "npc_citizen17.h"
#include "point_bonusmaps_accessor.h"
#include "hl2_shareddefs.h"

#define KILL_ALIENSWXBOW_COUNT 25
#define KILL_REBELSW357_COUNT 18
#define XENGRENADE_WEIGHT_COUNT 10000
#define KICK_DOORS_COUNT 60
#define SQUAD_COUNT_CHAPTER1 4
#define SQUAD_COUNT_CHAPTER2 5

////////////////////////////////////////////////////////////////////////////////////////////////////
// Chapter Completion Achievements
////////////////////////////////////////////////////////////////////////////////////////////////////

class CAchievementEZ2CompleteChapter0 : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C0"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C0" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter0, ACHIEVEMENT_EZ2_CH0, "ACH_EZ2_CH0", 5 );

class CAchievementEZ2CompleteChapter1 : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C1"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C1" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter1, ACHIEVEMENT_EZ2_CH1, "ACH_EZ2_CH1", 5 );

class CAchievementEZ2CompleteChapter2 : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C2"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C2" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter2, ACHIEVEMENT_EZ2_CH2, "ACH_EZ2_CH2", 5 );

class CAchievementEZ2CompleteChapter3 : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C3"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C3" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter3, ACHIEVEMENT_EZ2_CH3, "ACH_EZ2_CH3", 5 );

class CAchievementEZ2CompleteChapter4 : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C4"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C4" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter4, ACHIEVEMENT_EZ2_CH4, "ACH_EZ2_CH4", 5 );

class CAchievementEZ2CompleteChapter4a : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C4a"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C4a" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter4a, ACHIEVEMENT_EZ2_CH4a, "ACH_EZ2_CH4a", 5 );

class CAchievementEZ2CompleteChapter5 : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C5"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C5" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter5, ACHIEVEMENT_EZ2_CH5, "ACH_EZ2_CH5", 5 );

class CAchievementEZ2CompleteChapter6 : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C6"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C6" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter6, ACHIEVEMENT_EZ2_CH6, "ACH_EZ2_CH6", 5 );

////////////////////////////////////////////////////////////////////////////////////////////////////
// Narrative Achievements
////////////////////////////////////////////////////////////////////////////////////////////////////

class CAchievementEZ2StillAlive : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_STILL_ALIVE"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_STILL_ALIVE" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2StillAlive, ACHIEVEMENT_EZ2_STILL_ALIVE, "ACH_EZ2_STILL_ALIVE", 5 );

class CAchievementEZ2SpareCC : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_SPARE_CC"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_SPARE_CC" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2SpareCC, ACHIEVEMENT_EZ2_SPARE_CC, "ACH_EZ2_SPARE_CC", 5 );

class CAchievementEZ2MeetWilson : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_MEET_WILSON"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_MEET_WILSON" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2MeetWilson, ACHIEVEMENT_EZ2_MEET_WILSON, "ACH_EZ2_MEET_WILSON", 5 );

class CAchievementEZ2DeliverWilson : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_DELIVER_WILSON"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_DELIVER_WILSON" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2DeliverWilson, ACHIEVEMENT_EZ2_DELIVER_WILSON, "ACH_EZ2_DELIVER_WILSON", 5 );

class CAchievementEZ2MindWipe : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_MIND_WIPE"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_MIND_WIPE" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2MindWipe, ACHIEVEMENT_EZ2_MIND_WIPE, "ACH_EZ2_MIND_WIPE", 5 );

class CAchievementEZ2Superfuture : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_SUPERFUTURE"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_SUPERFUTURE" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2Superfuture, ACHIEVEMENT_EZ2_SUPERFUTURE, "ACH_EZ2_SUPERFUTURE", 5 );

class CAchievementEZ2XenGrenadeHeli : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_XENGRENADE_HELICOPTER"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_XENGRENADE_HELICOPTER" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2XenGrenadeHeli, ACHIEVEMENT_EZ2_XENGRENADE_HELICOPTER, "ACH_EZ2_XENGRENADE_HELICOPTER", 5 );

class CAchievementEZ2DeliverLoneWolf : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_DELIVER_LONEWOLF"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_DELIVER_LONEWOLF" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2DeliverLoneWolf, ACHIEVEMENT_EZ2_DELIVER_LONEWOLF, "ACH_EZ2_DELIVER_LONEWOLF", 5 );

class CAchievementEZ2SeeBorealis : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_SEE_BOREALIS"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_SEE_BOREALIS" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2SeeBorealis, ACHIEVEMENT_EZ2_SEE_BOREALIS, "ACH_EZ2_SEE_BOREALIS", 5 );

class CAchievementEZ2MeetCC : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_MEET_CC"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_MEET_CC" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2MeetCC, ACHIEVEMENT_EZ2_MEET_CC, "ACH_EZ2_MEET_CC", 5 );

////////////////////////////////////////////////////////////////////////////////////////////////////
// Collectible Achievements
////////////////////////////////////////////////////////////////////////////////////////////////////

// Find all of the Arbeit 1 recording boxes
class CAchievementEZ2FindAllRecordingBoxes : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_RECORDING_RG01", "EZ2_RECORDING_RG02", "EZ2_RECORDING_RG03", "EZ2_RECORDING_RG04", "EZ2_RECORDING_RG05", "EZ2_RECORDING_CC01", "EZ2_RECORDING_CC02", "EZ2_RECORDING_CC03", "EZ2_RECORDING_CC04", "EZ2_RECORDING_CC05", "EZ2_RECORDING_CC06", "EZ2_RECORDING_CC07", "EZ2_RECORDING_CC08", "EZ2_RECORDING_CC09", "EZ2_RECORDING_CC10", "EZ2_RECORDING_CC11", "EZ2_RECORDING_CC12", "EZ2_RECORDING_CC13", "EZ2_RECORDING_RG06"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ2_RECORDING");
		SetGameDirFilter("EntropyZero2");
		SetGoal(m_iNumComponents);
	}

	// Show progress for this achievement
	virtual bool ShouldShowProgressNotification() { return true; }
};
DECLARE_ACHIEVEMENT(CAchievementEZ2FindAllRecordingBoxes, ACHIEVEMENT_EZ2_RECORDING, "ACH_EZ2_RECORDING", 5);

class CAchievementEZ2FindAllWilsonClosets : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_WILSONCLOSET_01", "EZ2_WILSONCLOSET_02", "EZ2_WILSONCLOSET_03", "EZ2_WILSONCLOSET_04", "EZ2_WILSONCLOSET_05", "EZ2_WILSONCLOSET_06", "EZ2_WILSONCLOSET_07", "EZ2_WILSONCLOSET_08", "EZ2_WILSONCLOSET_09", "EZ2_WILSONCLOSET_10", "EZ2_WILSONCLOSET_11",
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_WILSONCLOSET" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( m_iNumComponents );
	}

	// Show progress for this achievement
	virtual bool ShouldShowProgressNotification() { return true; }
};
DECLARE_ACHIEVEMENT( CAchievementEZ2FindAllWilsonClosets, ACHIEVEMENT_EZ2_WILSON_CLOSETS, "ACH_EZ2_WILSON_CLOSETS", 5 );

////////////////////////////////////////////////////////////////////////////////////////////////////
// Kill Achievements
////////////////////////////////////////////////////////////////////////////////////////////////////

class CAchievementEZ2KillChapter3Gonome : public CFailableAchievement
{
protected:

	virtual void Init()
	{
		SetVictimFilter( "npc_zassassin" );
		// Use flag ACH_LISTEN_KILL_EVENTS instead of ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS in case
		//   the way the gonome dies doesn't treat the player as the attacker
		SetFlags( ACH_LISTEN_MAP_EVENTS | ACH_LISTEN_KILL_EVENTS | ACH_SAVE_WITH_GAME );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}

	// If we somehow manage to reach Chapter 3 without killing the gonome, fail this achievement
	virtual void OnEvaluationEvent()
	{
		if (IsAchieved())
			return;

		SetFailed();
	}

	// map event where achievement is activated
	virtual const char *GetActivationEventName() { return "EZ2_START_KILL_BEAST"; }
	// map event where achievement is evaluated for success
	virtual const char *GetEvaluationEventName() { return "EZ2_COMPLETE_C3"; }
};
DECLARE_ACHIEVEMENT( CAchievementEZ2KillChapter3Gonome, ACHIEVEMENT_EZ2_KILL_BEAST, "ACH_EZ2_KILL_BEAST", 5 );

class CAchievementEZ2KillTwoGonomes : public CFailableAchievement
{
protected:

	virtual void Init()
	{
		SetVictimFilter( "npc_zassassin" );
		SetFlags( ACH_LISTEN_MAP_EVENTS | ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_WITH_GAME );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 2 );
	}

	// If we complete Chapter 4a without killing two gonomes, fail this achievement.
	// 
	virtual void OnEvaluationEvent()
	{
		if (IsAchieved())
			return;

		SetFailed();
	}

	// map event where achievement is activated
	virtual const char *GetActivationEventName() { return "EZ2_START_XENT"; }
	// map event where achievement is evaluated for success
	virtual const char *GetEvaluationEventName() { return "EZ2_COMPLETE_C4a"; }
};
DECLARE_ACHIEVEMENT( CAchievementEZ2KillTwoGonomes, ACHIEVEMENT_EZ2_KILL_TWOGONOMES, "ACH_EZ2_KILL_TWOGONOMES", 5 );

class CAchievementEZ2AdvisorDead : public CFailableAchievement
{
protected:

	virtual void Init()
	{
		SetVictimFilter( "npc_advisor" );
		SetFlags( ACH_LISTEN_MAP_EVENTS | ACH_LISTEN_KILL_EVENTS | ACH_SAVE_WITH_GAME );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}

	// map event where achievement is activated\
	// In this case we don't track the achievement until the conclusion of Chapter 5 to exclude any custom maps
	virtual const char *GetActivationEventName() { return "EZ2_START_KILL_ADVISOR"; }
	// map event where achievement is evaluated for success
	// This event has nothing to do with this achievement, but we need an evaluation event that will fire after this one is concluded
	virtual const char *GetEvaluationEventName() { return "EZ2_STILL_ALIVE"; }

	// Show progress for this achievement
	virtual bool ShouldShowProgressNotification() { return true; }
};
DECLARE_ACHIEVEMENT( CAchievementEZ2AdvisorDead, ACHIEVEMENT_EZ2_ADVISOR_DEAD, "ACH_EZ2_ADVISOR_DEAD", 5 );

// Kill a certain number of aliens with the crossbow
class CAchievementEZ2KillAliensWithCrossbow : public CBaseAchievement
{
protected:

	void Init()
	{
		SetAttackerFilter( "player" );
		SetInflictorFilter( "crossbow_bolt" );
		SetFlags( ACH_LISTEN_KILL_EVENTS | ACH_SAVE_GLOBAL );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( KILL_ALIENSWXBOW_COUNT );
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event )
	{
		if (!pAttacker)
			return;

		if (!pVictim)
			return;

		if (!pAttacker->IsPlayer())
			return;

		// Check if the victim is an alien
		int lVictimClassification = pVictim->Classify();
		switch (lVictimClassification)
		{
		case CLASS_ALIEN_FAUNA:
		case CLASS_ALIEN_PREDATOR:
		case CLASS_ANTLION:
		case CLASS_BARNACLE:
		case CLASS_HEADCRAB:
		case CLASS_BULLSQUID:
		case CLASS_HOUNDEYE:
		case CLASS_RACE_X:
		case CLASS_VORTIGAUNT:
		case CLASS_ZOMBIE:
			IncrementCount();
		default:
			return;
		}
	}

	// Show progress for this achievement
	virtual bool ShouldShowProgressNotification() { return true; }
};
DECLARE_ACHIEVEMENT( CAchievementEZ2KillAliensWithCrossbow, ACHIEVEMENT_EZ2_KILL_ALIENSWXBOW, "ACH_EZ2_KILL_ALIENSWXBOW", 5 );

// HACKHACK: Checking our active weapon isn't enough because the player could be killing NPCs using the APC, a mounted gun, grenades, etc.
// As a result, we hook directly into the 357 class to turn this on when firing a bullet and turn it off directly afterwards. This ensures
// that the achievement only counts during the relevant window.
bool g_bEZ2357AchievementHack = false;

class CAchievementEZ2KillRebelsWith357 : public CBaseAchievement
{
protected:

	void Init()
	{
		SetAttackerFilter( "player" );
		SetVictimFilter( "npc_citizen" );
		SetFlags( ACH_LISTEN_KILL_EVENTS | ACH_SAVE_GLOBAL );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( KILL_REBELSW357_COUNT );
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event )
	{
		if (!pAttacker)
			return;

		if (!pVictim)
			return;

		if (!pAttacker->IsPlayer())
			return;

		CBaseCombatWeapon * pWeapon = pAttacker->MyCombatCharacterPointer()->GetActiveWeapon();
		if (pWeapon && pWeapon->ClassMatches( "weapon_357" ))
		{
			if (g_bEZ2357AchievementHack)
			{
				IncrementCount();
			}
		}
	}

	// Show progress for this achievement
	virtual bool ShouldShowProgressNotification() { return true; }
};
DECLARE_ACHIEVEMENT( CAchievementEZ2KillRebelsWith357, ACHIEVEMENT_EZ2_KILL_REBELSW357, "ACH_EZ2_KILL_REBELSW357", 5 );

class CAchievementEZ2KillJumpRebelsMidair : public CBaseAchievement
{
protected:

	void Init()
	{
		SetAttackerFilter( "player" );
		SetVictimFilter( "npc_citizen" );
		SetFlags( ACH_LISTEN_KILL_EVENTS | ACH_SAVE_GLOBAL );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event )
	{
		if (!pAttacker)
			return;

		if (!pVictim)
			return;

		if (!pAttacker->IsPlayer())
			return;

		CNPC_Citizen *pCitizen = static_cast<CNPC_Citizen *>(pVictim);

		// Only count jump rebels
		if (pCitizen->GetCitiznType() != CT_LONGFALL)
			return;

		// If this citizen is mid-jump, increment the count
		if ( pCitizen->GetActivity() == ACT_GLIDE )
			IncrementCount();
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2KillJumpRebelsMidair, ACHIEVEMENT_EZ2_KILL_JUMPREBELMIDAIR, "ACH_EZ2_KILL_JUMPREBELMIDAIR", 5 );

// This achievement is a "collectible kill achievement"
// It is both a kill achievement and a component achievement.
// Each kill will process a fake component event based on the map name that the kill took place in
class CAchievementEZ2KillTemporalCrabs : public CBaseAchievement
{
protected:

	void Init()
	{
		SetVictimFilter( "npc_headcrab_black" );
		static const char *szComponents[] =
		{
			"EZ2_KILL_TEMPORALCRAB_ez2_c4_3", "EZ2_KILL_TEMPORALCRAB_ez2_c5_1", "EZ2_KILL_TEMPORALCRAB_ez2_c5_2a", "EZ2_KILL_TEMPORALCRAB_ez2_c5_3"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_KILL_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_KILL_TEMPORALCRAB" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( m_iNumComponents );
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event )
	{
		if (!pVictim)
			return;

		CAI_BaseNPC * pNPC = pVictim->MyNPCPointer();
		if (!pNPC)
			return;

		// Victim must be temporal
		if (pNPC->m_tEzVariant != EZ_VARIANT_TEMPORAL)
			return;

		const char * pEventComponent = UTIL_VarArgs( "EZ2_KILL_TEMPORALCRAB_%s", m_pAchievementMgr->GetMapName() );

		if (cc_achievement_debug.GetInt() > 0)
		{
			Msg( "Temporal crab %s was killed and triggered an achievement event: %s\n", pNPC->GetDebugName(), pEventComponent );
		}

		// Process a component event based on the current map name (This way each map's temporal crab is unique)
		this->OnComponentEvent( pEventComponent );
	}

	// Show progress for this achievement
	virtual bool ShouldShowProgressNotification() { return true; }
};
DECLARE_ACHIEVEMENT( CAchievementEZ2KillTemporalCrabs, ACHIEVEMENT_EZ2_KILL_TEMPORALCRAB, "ACH_EZ2_KILL_TEMPORALCRABS", 5 );

////////////////////////////////////////////////////////////////////////////////////////////////////
// Xen Grenade Achievements
////////////////////////////////////////////////////////////////////////////////////////////////////

class CAchievementEZ2XenGrenadeWeight : public CBaseAchievement
{
protected:
	void Init()
	{
		SetAttackerFilter( "player" ); // TODO - Should this achievement filter on the player being the thrower?
		SetFlags( ACH_LISTEN_XENGRENADE_EVENTS | ACH_FILTER_ATTACKER_IS_PLAYER | ACH_SAVE_GLOBAL );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( XENGRENADE_WEIGHT_COUNT );
	}

	// Divide this achievement into increments of 10
	void CalcProgressMsgIncrement()
	{
		m_iProgressMsgIncrement = XENGRENADE_WEIGHT_COUNT / 10;
	}

	// Show progress for this achievement
	virtual bool ShouldShowProgressNotification() { return true; }
};
DECLARE_ACHIEVEMENT( CAchievementEZ2XenGrenadeWeight, ACHIEVEMENT_EZ2_XENGRENADE_WEIGHT, "ACH_EZ2_XENGRENADE_WEIGHT", 5 );

////////////////////////////////////////////////////////////////////////////////////////////////////
// Kick Achievements
////////////////////////////////////////////////////////////////////////////////////////////////////

class CAchievementEZ2KickDoors : public CBaseAchievement
{
protected:

	void Init()
	{
		SetAttackerFilter( "player" );
		SetVictimFilter( "prop_door_rotating" );
		SetFlags( ACH_LISTEN_KICK_EVENTS | ACH_SAVE_GLOBAL );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( KICK_DOORS_COUNT );
	}

	virtual void Event_EntityKicked( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event )
	{
		// We only want one kick per entity
		int iIndex = pVictim->FindContextByName( "kicked" );
		if (iIndex != -1 && atoi( pVictim->GetContextValue( iIndex ) ) > 0)
			return;

		IncrementCount();
	}

	// Show progress for this achievement
	virtual bool ShouldShowProgressNotification() { return true; }
};
DECLARE_ACHIEVEMENT( CAchievementEZ2KickDoors, ACHIEVEMENT_EZ2_KICK_DOORS, "ACH_EZ2_KICK_DOORS", 5 );

class CAchievementEZ2KickAdvisor : public CBaseAchievement
{
protected:

	void Init()
	{
		SetAttackerFilter( "player" );
		SetVictimFilter( "npc_advisor" );
		SetFlags( ACH_LISTEN_KICK_EVENTS | ACH_SAVE_GLOBAL );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2KickAdvisor, ACHIEVEMENT_EZ2_KICK_ADVISOR, "ACH_EZ2_KICK_ADVISOR", 5 );

////////////////////////////////////////////////////////////////////////////////////////////////////
// Squad Achievements
////////////////////////////////////////////////////////////////////////////////////////////////////

class CAchievementEZ2SquadChapter1 : public CSquadAchievement
{
protected:
	void Init()
	{
		SetSquadFilter( "npc_combine_s" );
		SetFlags( ACH_LISTEN_MAP_EVENTS | ACH_SAVE_GLOBAL );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( SQUAD_COUNT_CHAPTER1 );
	}

	// Evaluate the squad when we receive this event
	virtual const char *GetEvaluationEventName() { return "EZ2_SQUAD_CH1"; }

};
DECLARE_ACHIEVEMENT( CAchievementEZ2SquadChapter1, ACHIEVEMENT_EZ2_SQUAD_CH1, "ACH_EZ2_SQUAD_CH1", 5 );

class CAchievementEZ2SquadChapter2 : public CSquadAchievement
{
protected:
	void Init()
	{
		SetSquadFilter( "npc_combine_s" );
		SetFlags( ACH_LISTEN_MAP_EVENTS | ACH_SAVE_GLOBAL );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( SQUAD_COUNT_CHAPTER2 );
	}

	// Evaluate the squad when we receive this event
	virtual const char *GetEvaluationEventName() { return "EZ2_SQUAD_CH2"; }

};
DECLARE_ACHIEVEMENT( CAchievementEZ2SquadChapter2, ACHIEVEMENT_EZ2_SQUAD_CH2, "ACH_EZ2_SQUAD_CH2", 5 );

////////////////////////////////////////////////////////////////////////////////////////////////////
// Misc. Event Achievements
////////////////////////////////////////////////////////////////////////////////////////////////////

class CAchievementEZ2FlipAPC : public CBaseAchievement
{
protected:

	void Init() 
	{
		SetFlags( ACH_LISTEN_MAP_EVENTS | ACH_SAVE_WITH_GAME );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "vehicle_overturned" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( 0 == Q_strcmp( event->GetName(), "vehicle_overturned" ) )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2FlipAPC, ACHIEVEMENT_EZ2_FLIP_APC, "ACH_EZ2_FLIP_APC", 5 );

class CAchievementEZ2HealedBySurrenderedMedic : public CBaseAchievement
{
protected:

	void Init() 
	{
		SetFlags( ACH_LISTEN_MAP_EVENTS | ACH_SAVE_WITH_GAME );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "medic_heal_player" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( 0 == Q_strcmp( event->GetName(), "medic_heal_player" ) )
		{
			CBaseEntity *pEnt = UTIL_EntityByIndex( event->GetInt( "medic", 0 ) );
			if (!pEnt || !pEnt->ClassMatches("npc_citizen"))
				return;

			CNPC_Citizen *pCitizen = static_cast<CNPC_Citizen*>(pEnt);
			if (!pCitizen->IsSurrendered())
				return;

			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2HealedBySurrenderedMedic, ACHIEVEMENT_EZ2_SURRENDERED_HEAL, "ACH_EZ2_SURRENDERED_HEAL", 5 );

////////////////////////////////////////////////////////////////////////////////////////////////////

#define DECLARE_EZ2_MAP_EVENT_ACHIEVEMENT( achievementID, achievementName, iPointValue )					\
	DECLARE_MAP_EVENT_ACHIEVEMENT_( achievementID, achievementName, "EntropyZero2", iPointValue, false )

#define DECLARE_EZ2_MAP_EVENT_ACHIEVEMENT_HIDDEN( achievementID, achievementName, iPointValue )					\
	DECLARE_MAP_EVENT_ACHIEVEMENT_( achievementID, achievementName, "EntropyZero2", iPointValue, true )

////////////////////////////////////////////////////////////////////////////////////////////////////
// Complete the mod on Normal difficulty
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAchievementEZ2FinishNormal : public CFailableAchievement
{
protected:

	void Init()
	{
		SetFlags(ACH_LISTEN_SKILL_EVENTS | ACH_LISTEN_MAP_EVENTS | ACH_SAVE_WITH_GAME);
		SetGameDirFilter("EntropyZero2");
		SetGoal(1);
	}

	virtual void Event_SkillChanged(int iSkillLevel, IGameEvent * event)
	{
		if (g_iSkillLevel < SKILL_MEDIUM)
		{
			SetAchieved(false);
			SetFailed();
		}
	}

	// Upon activating this achievement, test the current skill level
	virtual void OnActivationEvent() {
		Activate();
		Event_SkillChanged(g_iSkillLevel, NULL);
	}

	// map event where achievement is activated
	virtual const char *GetActivationEventName() { return "EZ2_START_GAME"; }
	// map event where achievement is evaluated for success
	virtual const char *GetEvaluationEventName() { return "EZ2_BEAT_GAME"; }
};
DECLARE_ACHIEVEMENT(CAchievementEZ2FinishNormal, ACHIEVEMENT_EZ2_NMODE, "ACH_EZ2_NMODE", 15);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Complete the mod on Hard difficulty
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAchievementEZ2FinishHard : public CFailableAchievement
{
protected:

	void Init()
	{
		SetFlags(ACH_LISTEN_SKILL_EVENTS | ACH_LISTEN_MAP_EVENTS | ACH_SAVE_WITH_GAME);
		SetGameDirFilter("EntropyZero2");
		SetGoal(1);
	}

	virtual void Event_SkillChanged(int iSkillLevel, IGameEvent * event)
	{
		if (g_iSkillLevel < SKILL_HARD)
		{
			SetAchieved(false);
			SetFailed();
		}
	}

	// Upon activating this achievement, test the current skill level
	virtual void OnActivationEvent() {
		Activate();
		Event_SkillChanged(g_iSkillLevel, NULL);
	}

	// map event where achievement is activated
	virtual const char *GetActivationEventName() { return "EZ2_START_GAME"; }
	// map event where achievement is evaluated for success
	virtual const char *GetEvaluationEventName() { return "EZ2_BEAT_GAME"; }
};
DECLARE_ACHIEVEMENT(CAchievementEZ2FinishHard, ACHIEVEMENT_EZ2_HMODE, "ACH_EZ2_HMODE", 15);

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

#define NUM_CHALLENGE_MAPS 8
#define NUM_CHALLENGES (NUM_CHALLENGE_MAPS * 5) // 40

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBaseAchievementEZ2ChallengeAllMedal : public CBaseAchievement
{
protected:

	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( NUM_CHALLENGES );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "challenge_map_complete" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( 0 == Q_strcmp( event->GetName(), "challenge_map_complete" ) )
		{
			int numMedals = event->GetInt( Medal(), 0 );
			SetCount( numMedals );

			// if we've hit goal, award the achievement
			if (m_iGoal > 0)
			{
				if (m_iCount >= m_iGoal)
				{
					AwardAchievement();
				}
				else
				{
					HandleProgressUpdate();
				}
			}
		}
	}

	// Show progress for this achievement
	virtual bool ShouldShowProgressNotification() { return true; }

	// Show progress every time we complete a challenge
	void CalcProgressMsgIncrement()
	{
		m_iProgressMsgIncrement = 1;
	}

	virtual const char *Medal() { return "numgold"; }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Get bronze medals on all challenges
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAchievementEZ2ChallengesAllBronze : public CBaseAchievementEZ2ChallengeAllMedal
{
protected:
	virtual const char *Medal() { return "numbronze"; }
};
DECLARE_ACHIEVEMENT( CAchievementEZ2ChallengesAllBronze, ACHIEVEMENT_EZ2_CHALLENGES_ALL_BRONZE, "ACH_EZ2_CHALLENGES_ALL_BRONZE", 5 );

////////////////////////////////////////////////////////////////////////////////////////////////////
// Get silver medals on all challenges
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAchievementEZ2ChallengesAllSilver : public CBaseAchievementEZ2ChallengeAllMedal
{
protected:
	virtual const char *Medal() { return "numsilver"; }
};
DECLARE_ACHIEVEMENT( CAchievementEZ2ChallengesAllSilver, ACHIEVEMENT_EZ2_CHALLENGES_ALL_SILVER, "ACH_EZ2_CHALLENGES_ALL_SILVER", 5 );

////////////////////////////////////////////////////////////////////////////////////////////////////
// Get gold medals on all challenges
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAchievementEZ2ChallengesAllGold : public CBaseAchievementEZ2ChallengeAllMedal
{
protected:
	virtual const char *Medal() { return "numgold"; }
};
DECLARE_ACHIEVEMENT( CAchievementEZ2ChallengesAllGold, ACHIEVEMENT_EZ2_CHALLENGES_ALL_GOLD, "ACH_EZ2_CHALLENGES_ALL_GOLD", 5 );
#endif // GAME_DLL
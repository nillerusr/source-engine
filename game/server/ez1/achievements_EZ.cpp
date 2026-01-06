//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

// Breadman. ALL Breadman!

#include "cbase.h"

#ifdef GAME_DLL

#include "achievementmgr.h"
#include "baseachievement.h"

// Achievements for EZero.
// OMG IT WORKS I CANNE BELIEVE IT CAPTAIN I DEED IT!

// Declare these fuckers in hl2orage.spa.h

////////////////////////////////////////////////////////////////////////////////////////////////////
// Find and wobble all of the hula girls.
class CAchievementEZFindAllHulaGirls : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_HULA_01", "EZ_HULA_02", "EZ_HULA_03", "EZ_HULA_04", "EZ_HULA_05", "EZ_HULA_06", "EZ_HULA_07"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_HULA");
		SetGameDirFilter("EntropyZero");
		SetGoal(m_iNumComponents);
	}

	// Show progress for this achievement
	virtual bool ShouldShowProgressNotification() { return true; }
};
DECLARE_ACHIEVEMENT( CAchievementEZFindAllHulaGirls, ACHIEVEMENT_EZ_HULA, "ACH_EZ_HULA", 5 );
////////////////////////////////////////////////////////////////////////////////////////////////////
// Find all of the poem pieces
class CAchievementEZFindAllPoemPieces : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_POEM_01", "EZ_POEM_02", "EZ_POEM_03", "EZ_POEM_04", "EZ_POEM_05", "EZ_POEM_06", "EZ_POEM_07", "EZ_POEM_08", "EZ_POEM_09", "EZ_POEM_10"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_POEM");
		SetGameDirFilter("EntropyZero");
		SetGoal(m_iNumComponents);
	}

	// Show progress for this achievement
	virtual bool ShouldShowProgressNotification() { return true; }
};
DECLARE_ACHIEVEMENT(CAchievementEZFindAllPoemPieces, ACHIEVEMENT_EZ_POEM, "ACH_EZ_POEM", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Find all of the Lambda Generation Tags
class CAchievementEZNextGeneration : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_NXTGEN_01", "EZ_NXTGEN_02", "EZ_NXTGEN_03", "EZ_NXTGEN_04", "EZ_NXTGEN_05", "EZ_NXTGEN_06", "EZ_NXTGEN_07"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_NXTGEN");
		SetGameDirFilter("EntropyZero");
		SetGoal(m_iNumComponents);
	}

	// Show progress for this achievement
	virtual bool ShouldShowProgressNotification() { return true; }
};
DECLARE_ACHIEVEMENT(CAchievementEZNextGeneration, ACHIEVEMENT_EZ_NXTGEN, "ACH_EZ_NXTGEN", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
// A Real Class Act! (Get five kills with manhacks)
class CAchievementEZClassy : public CBaseAchievement
{
protected:

	virtual void Init()
	{
		SetInflictorFilter("npc_manhack");
		SetFlags(ACH_LISTEN_KILL_EVENTS | ACH_SAVE_WITH_GAME);
		SetGameDirFilter("EntropyZero");
		SetGoal(5);
	}

	// This is working just fine but it isn't telling us we're making progress
	virtual void Event_EntityKilled(CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event)
	{
			IncrementCount();
	}

	virtual bool ShouldShowProgressNotification() { return true; }
};
DECLARE_ACHIEVEMENT(CAchievementEZClassy, ACHIEVEMENT_EZ_CLASSY, "ACH_EZ_CLASSY", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Ten sterilize Credits
class CAchievementEZTenSC : public CBaseAchievement
{
protected:

	virtual void Init()
	{
		SetVictimFilter("npc_citizen");
		SetFlags(ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_WITH_GAME);
		SetGameDirFilter("EntropyZero");
		SetGoal(10);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZTenSC, ACHIEVEMENT_EZ_TENSC, "ACH_EZ_TENSC", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Ten sterilize Credits
class CAchievementEZTwentySC : public CBaseAchievement
{
protected:

	virtual void Init()
	{
		SetVictimFilter("npc_citizen");
		SetFlags(ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_WITH_GAME);
		SetGameDirFilter("EntropyZero");
		SetGoal(20);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZTwentySC, ACHIEVEMENT_EZ_TWENTYSC, "ACH_EZ_TWENTYSC", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Thirty sterilize Credits
class CAchievementEZThirtySC : public CBaseAchievement
{
protected:

	virtual void Init()
	{
		SetVictimFilter("npc_citizen");
		SetFlags(ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_WITH_GAME);
		SetGameDirFilter("EntropyZero");
		SetGoal(30);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZThirtySC, ACHIEVEMENT_EZ_THIRTYSC, "ACH_EZ_THIRTYSC", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Forty sterilize Credits
class CAchievementEZFortySC : public CBaseAchievement
{
protected:

	virtual void Init()
	{
		SetVictimFilter("npc_citizen");
		SetFlags(ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_WITH_GAME);
		SetGameDirFilter("EntropyZero");
		SetGoal(40);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZFortySC, ACHIEVEMENT_EZ_FORTYSC, "ACH_EZ_FORTYSC", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Fifty sterilize Credits
class CAchievementEZFiftySC : public CBaseAchievement
{
protected:

	virtual void Init()
	{
		SetVictimFilter("npc_citizen");
		SetFlags(ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_WITH_GAME);
		SetGameDirFilter("EntropyZero");
		SetGoal(50);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZFiftySC, ACHIEVEMENT_EZ_FIFTYSC, "ACH_EZ_FIFTYSC", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////

// Blargh lets hope this works.
#define DECLARE_EZ_MAP_EVENT_ACHIEVEMENT( achievementID, achievementName, iPointValue )					\
	DECLARE_MAP_EVENT_ACHIEVEMENT_( achievementID, achievementName, "EntropyZero", iPointValue, false )

#define DECLARE_EZ_MAP_EVENT_ACHIEVEMENT_HIDDEN( achievementID, achievementName, iPointValue )					\
	DECLARE_MAP_EVENT_ACHIEVEMENT_( achievementID, achievementName, "EntropyZero", iPointValue, true )

/// These are one-offs.
// Pick up that can
class CAchievementEZPcan : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_PCAN"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_PCAN");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZPcan, ACHIEVEMENT_EZ_PCAN, "ACH_EZ_PCAN", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Now put it in the trashcan
class CAchievementEZTCAN : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_TCAN"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_TCAN");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZTCAN, ACHIEVEMENT_EZ_TCAN, "ACH_EZ_TCAN", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// I know the pieces fit
class CAchievementEZSchism : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_SCHISM"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_SCHISM");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZSchism, ACHIEVEMENT_EZ_SCHISM, "ACH_EZ_SCHISM", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Kindergarten Cop
class CAchievementEZKCop : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_KCOP"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_KCOP");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZKCop, ACHIEVEMENT_EZ_KCOP, "ACH_EZ_KCOP", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Plosive Expletive
class CAchievementEZGrenade : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_PE"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_PE");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZGrenade, ACHIEVEMENT_EZ_PE, "ACH_EZ_PE", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Sucker's Luck
class CAchievementEZMapSuckersLuck : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_SLUCK"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_SLUCK");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZMapSuckersLuck, ACHIEVEMENT_EZ_SLUCK, "ACH_EZ_SLUCK", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Sucker's Luck
class CAchievementEZSneakEater : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_SEATER"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_SEATER");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZSneakEater, ACHIEVEMENT_EZ_SEATER, "ACH_EZ_SEATER", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Deadly Exploit
class CAchievementEZExploit : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_DEXPLOIT"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_DEXPLOIT");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZExploit, ACHIEVEMENT_EZ_DEXPLOIT, "ACH_EZ_DEXPLOIT", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Very Dead Tenant
class CAchievementEZVDT : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_VDT"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_VDT");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZVDT, ACHIEVEMENT_EZ_VDT, "ACH_EZ_VDT", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Still Alive
class CAchievementEZSALIVE : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_SALIVE"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_SALIVE");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZSALIVE, ACHIEVEMENT_EZ_SALIVE, "ACH_EZ_SALIVE", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// More Gun
class CAchievementEZMGUN : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_MGUN"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_MGUN");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZMGUN, ACHIEVEMENT_EZ_MGUN, "ACH_EZ_MGUN", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Baby sittin' job
class CAchievementEZBSJOB : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_BSJOB"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_BSJOB");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZBSJOB, ACHIEVEMENT_EZ_BSJOB, "ACH_EZ_BSJOB", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
//	Solo
class CAchievementEZSolo: public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_SOLO"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_SOLO");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZSolo, ACHIEVEMENT_EZ_SOLO, "ACH_EZ_SOLO", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// OUTBREAK OUTBREAK
class CAchievementEZOutbreak : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_OUTBREAK"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_OUTBREAK");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZOutbreak, ACHIEVEMENT_EZ_OUTBREAK, "ACH_EZ_OUTBREAK", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Vort Soul / Now known as Hunter Killer
class CAchievementEZVSoul : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_VSOUL"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_VSOUL");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZVSoul, ACHIEVEMENT_EZ_VSOUL, "ACH_EZ_VSOUL", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// On Ice
class CAchievementEZOICE : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_OICE"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_OICE");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZOICE, ACHIEVEMENT_EZ_OICE, "ACH_EZ_OICE", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Vague Voices
class CAchievementEZVVOICE : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ_VVOICE"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ_VVOICE");
		SetGameDirFilter("EntropyZero");
		SetGoal(1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZVVOICE, ACHIEVEMENT_EZ_VVOICE, "ACH_EZ_VVOICE", 5);
///////////////////////////////////////////////////////////////////////////////////////
/// Cremator
class CAchievementEZCremator : public CBaseAchievement
{
protected:
	void Init()
	{
		SetFlags(ACH_SAVE_WITH_GAME);
		SetGoal(20);

		if (IsPC())
		{
			// only in Ep1 for PC. (Shared across EPX for X360.)
			SetGameDirFilter("EntropyZero");
		}
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent("flare_ignite_npc");
	}

	void FireGameEvent_Internal(IGameEvent *event)
	{
		if (0 == Q_strcmp(event->GetName(), "flare_ignite_npc"))
		{
			CBaseEntity *pEntityIgnited = UTIL_EntityByIndex(event->GetInt("entindex"));
			// was it a zombie that got set on fire?
			if (pEntityIgnited &&
				((pEntityIgnited->ClassMatches("npc_zombie")) || (pEntityIgnited->ClassMatches("npc_zombine")) ||
				(pEntityIgnited->ClassMatches("npc_fastzombie")) || (pEntityIgnited->ClassMatches("npc_poisonzombie"))))
			{
				IncrementCount();
			}
		}
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZCremator, ACHIEVEMENT_EZ_CREMATOR, "ACH_EZ_CREMATOR", 5);
///////////////////////////////////////////////////////////////////////////////////////////////
/// Eviction Notice
class CAchievementEZEvictionNotice : public CBaseAchievement
{
protected:

	virtual void Init()
	{
		SetVictimFilter("npc_vortigaunt");
		SetFlags(ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_WITH_GAME);
		SetGameDirFilter("EntropyZero");
		SetGoal(20);
	}
};
DECLARE_ACHIEVEMENT(CAchievementEZEvictionNotice, ACHIEVEMENT_EZ_ENOTICE, "ACH_EZ_ENOTICE", 5);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Complete the mod on Normal difficulty
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAchievementEZFinishNormal : public CFailableAchievement
{
protected:

	void Init()
	{
		SetFlags(ACH_LISTEN_SKILL_EVENTS | ACH_LISTEN_MAP_EVENTS | ACH_SAVE_WITH_GAME);
		SetGameDirFilter("EntropyZero");
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
	virtual const char *GetActivationEventName() { return "EZ_START_GAME"; }
	// map event where achievement is evaluated for success
	virtual const char *GetEvaluationEventName() { return "EZ_BEAT_GAME"; }
};
DECLARE_ACHIEVEMENT(CAchievementEZFinishNormal, ACHIEVEMENT_EZ_NMODE, "ACH_EZ_NMODE", 15);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Complete the mod on Hard difficulty
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAchievementEZFinishHard : public CFailableAchievement
{
protected:

	void Init()
	{
		SetFlags(ACH_LISTEN_SKILL_EVENTS | ACH_LISTEN_MAP_EVENTS | ACH_SAVE_WITH_GAME);
		SetGameDirFilter("EntropyZero");
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
	virtual const char *GetActivationEventName() { return "EZ_START_GAME"; }
	// map event where achievement is evaluated for success
	virtual const char *GetEvaluationEventName() { return "EZ_BEAT_GAME"; }
};
DECLARE_ACHIEVEMENT(CAchievementEZFinishHard, ACHIEVEMENT_EZ_HMODE, "ACH_EZ_HMODE", 15);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Die 100 times in Entropy : Zero
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAchievementEZBulletHangover : public CBaseAchievement
{
protected:

	virtual void Init()
	{
		SetVictimFilter("player");
		SetFlags(ACH_LISTEN_KILL_EVENTS | ACH_SAVE_GLOBAL);
		SetGameDirFilter("EntropyZero");
		SetGoal(100);
	}

	// Don't show progress for this achievement
	virtual bool ShouldShowProgressNotification() { return false; }

	//// map event where achievement is activated
	virtual const char *GetActivationEventName() { return "EZ_START_GAME"; }
};
DECLARE_ACHIEVEMENT(CAchievementEZBulletHangover, ACHIEVEMENT_EZ_BHANGOVER, "ACH_EZ_BHANGOVER", 5);
#endif // GAME_DLL
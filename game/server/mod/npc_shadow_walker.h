//=//=============================================================================//
//
// Purpose: A malevolent being from a parallel universe which at one point
//		may have been human.		
//
//		npc_shadow_walker is designed to be reusable as a generic horror 
//		game style npc. Its model and sound files may be configured through
//		the hammer editor using keyfields.
//
//	Author: 1upD
//
//=============================================================================//
#include "cbase.h"
#include "ai_default.h"
#include "npc_base_custom.h"


//=========================================================
//=========================================================
class CNPC_ShadowWalker : public CAI_BlendingHost<CNPC_BaseCustomNPC>
{
	DECLARE_DATADESC();
	DECLARE_CLASS(CNPC_ShadowWalker, CAI_BlendingHost<CNPC_BaseCustomNPC>);

public:
	void				Precache(void);
	void				Spawn(void);
	Class_T				Classify(void);
	virtual int			SelectFailSchedule(int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode);
	virtual int			SelectIdleSchedule();
	virtual int			SelectAlertSchedule();
	virtual int			SelectCombatSchedule();
	virtual Activity	NPC_TranslateActivity(Activity eNewActivity);
	virtual int			TranslateSchedule(int scheduleType);

	void				Activate();
	void				FixupWeapon();

	DEFINE_CUSTOM_AI;
};

LINK_ENTITY_TO_CLASS(npc_shadow_walker, CNPC_ShadowWalker);

//=//=============================================================================//
//
// Purpose: A base class from which to extend new custom NPCs.
//	This class may seem redundant with CAI_BaseNPC and a lot of Valve's NPC classes.
//	However, the redundancy is necessary for compatibility with a variety of mods;
//	I want all new NPC content to be isolated from existing classes.
//
//	Author: 1upD
//
//=============================================================================//
#include "cbase.h"
#include "ai_default.h"
#include "ai_task.h"
#include "ai_schedule.h"
#include "entitylist.h"
#include "activitylist.h"
#include "ai_basenpc.h"
#include "ai_blended_movement.h"
#include "ai_behavior_actbusy.h"

//=========================================================
// schedules
//=========================================================
enum
{
	SCHED_MELEE_ATTACK_NOINTERRUPT = LAST_SHARED_SCHEDULE,
	SCHED_HIDE,

	LAST_BASE_CUSTOM_SCHED
};

//=========================================================
// states
//=========================================================
enum
{
	NPC_STATE_FIRST = NPC_STATE_DEAD,
	NPC_STATE_AMBUSH,
	NPC_STATE_SURRENDER,
	NPC_STATE_LAST_CUSTOM
};

// -----------------------------------------------
//	Squad slots
// -----------------------------------------------
enum
{
	LAST_SQUADSLOT = 100,	// Custom NPCs might share a squad with any NPC, so let's just be safe and skip to a high number
	SQUAD_SLOT_CHASE_1,
	SQUAD_SLOT_CHASE_2,
	LAST_CUSTOM_SQUADSLOT
};

//=========================================================
//=========================================================
typedef CAI_BlendingHost< CAI_BehaviorHost<CAI_BaseNPC> > CAI_CustomNPCBase;

class CNPC_BaseCustomNPC : public CAI_CustomNPCBase
{
	DECLARE_CLASS(CNPC_BaseCustomNPC, CAI_CustomNPCBase);

public:
	void	Precache(void);
	void	Spawn(void);
	Class_T Classify(void);
	virtual int				SelectFailSchedule(int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode);
	virtual int 			SelectScheduleRetrieveItem();
	virtual int 			SelectScheduleWander();
	virtual int 			SelectSchedule();
	virtual int				SelectIdleSchedule();
	virtual int				SelectAlertSchedule();
	virtual int				SelectCombatSchedule();
	virtual int				SelectAmbushSchedule();
	virtual int				SelectSurrenderSchedule();
	virtual float			GetSequenceGroundSpeed(CStudioHdr *pStudioHdr, int iSequence);
	virtual Activity		NPC_TranslateActivity(Activity eNewActivity);
	virtual int				TranslateSchedule(int scheduleType);

	// Custom states
	virtual NPC_STATE	SelectIdealState(void);
	NPC_STATE			SelectAmbushIdealState();
	NPC_STATE			SelectSurrenderIdealState();

	// Sounds
	virtual void		PlaySound(string_t soundname, bool optional);
	virtual void		DeathSound(const CTakeDamageInfo &info) { PlaySound(m_iszDeathSound, true); }
	virtual void		AlertSound(void) { PlaySound(m_iszAlertSound, false); };
	virtual void		IdleSound(void) { PlaySound(m_iszIdleSound, false); };
	virtual void		PainSound(const CTakeDamageInfo &info) { PlaySound(m_iszPainSound, true); };
	virtual void		FearSound(void) { PlaySound(m_iszFearSound, false); };
	virtual void		LostEnemySound(void) { PlaySound(m_iszLostEnemySound, false); };
	virtual void		FoundEnemySound(void);

	void				Activate();
	virtual void		FixupWeapon();

	// Inputs
	virtual void InputSetSpeedModifier(inputdata_t &inputdata);
	virtual void InputEnableOpenDoors(inputdata_t &inputdata);
	virtual void InputDisableOpenDoors(inputdata_t &inputdata);
	virtual void InputEnablePickupWeapons(inputdata_t &inputdata);
	virtual void InputDisablePickupWeapons(inputdata_t &inputdata);

	DECLARE_DATADESC();

	string_t m_iszWeaponModelName;			// Path/filename of model to override weapon model.

	string_t m_iszFearSound;			// Path/filename of WAV file to play.
	string_t m_iszDeathSound;			// Path/filename of WAV file to play.
	string_t m_iszIdleSound;			// Path/filename of WAV file to play.
	string_t m_iszPainSound;			// Path/filename of WAV file to play.
	string_t m_iszAlertSound;			// Path/filename of WAV file to play.
	string_t m_iszLostEnemySound;		// Path/filename of WAV file to play.
	string_t m_iszFoundEnemySound;		// Path/filename of WAV file to play.

	DEFINE_CUSTOM_AI;

protected:
	bool		HasRangedWeapon();
	void		PrecacheNPCSoundScript(string_t* SoundName, string_t defaultSoundName);

	int			m_iNumSquadmates;
	bool		m_bUseBothSquadSlots;	// If true use two squad slots, if false use one squad slot
	bool		m_bCannotOpenDoors;		// If true, this NPC cannot open doors. The condition is reversed because originally it could.
	bool		m_bCanPickupWeapons;			// If true, this NPC is able to pick up weapons off of the ground just like npc_citizen.
	bool		m_bWanderToggle;		// Boolean to toggle wandering / standing every think cycle
	float		m_flNextSoundTime;		// Next time at which this NPC is allowed to play an NPC sound
	float		m_flNextFoundEnemySoundTime;	// Next time at which this NPC is allowed to play an NPC sound
	float		m_flSpeedModifier;		// Modifier to apply to move distance
};


LINK_ENTITY_TO_CLASS(npc_base_custom, CNPC_BaseCustomNPC);

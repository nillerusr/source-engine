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
#include "npc_base_custom.h"
#include "ai_hull.h"
#include "soundent.h"
#include "game.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "basehlcombatweapon_shared.h"
#include "ai_squadslot.h"
#include "ai_squad.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//---------------------------------------------------------
// Constants
//---------------------------------------------------------
// TODO: Replace these with fields so that other NPCs can override them
const float MIN_TIME_NEXT_SOUND = 0.5f;
const float MAX_TIME_NEXT_SOUND = 1.0f;
const float MIN_TIME_NEXT_FOUNDENEMY_SOUND = 2.0f;
const float MAX_TIME_NEXT_FOUNDENEMY_SOUND = 5.0f;

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC(CNPC_BaseCustomNPC)
	DEFINE_KEYFIELD(m_iszWeaponModelName, FIELD_STRING, "WeaponModel"),
	DEFINE_KEYFIELD(m_iHealth, FIELD_INTEGER, "Health"),
	DEFINE_KEYFIELD(m_iszFearSound, FIELD_SOUNDNAME, "FearSound"),
	DEFINE_KEYFIELD(m_iszDeathSound, FIELD_SOUNDNAME, "DeathSound"),
	DEFINE_KEYFIELD(m_iszIdleSound, FIELD_SOUNDNAME, "IdleSound"),
	DEFINE_KEYFIELD(m_iszPainSound, FIELD_SOUNDNAME, "PainSound"),
	DEFINE_KEYFIELD(m_iszAlertSound, FIELD_SOUNDNAME, "AlertSound"),
	DEFINE_KEYFIELD(m_iszLostEnemySound, FIELD_SOUNDNAME, "LostEnemySound"),
	DEFINE_KEYFIELD(m_iszFoundEnemySound, FIELD_SOUNDNAME, "FoundEnemySound"),
	DEFINE_KEYFIELD(m_bUseBothSquadSlots, FIELD_BOOLEAN, "UseBothSquadSlots"),
	DEFINE_KEYFIELD(m_bCannotOpenDoors, FIELD_BOOLEAN, "CannotOpenDoors"),
	DEFINE_KEYFIELD(m_bCanPickupWeapons, FIELD_BOOLEAN, "CanPickupWeapons"),

	DEFINE_FIELD(m_iNumSquadmates, FIELD_INTEGER),
	DEFINE_FIELD(m_bWanderToggle, FIELD_BOOLEAN),
	DEFINE_FIELD(m_flNextSoundTime, FIELD_TIME),
	DEFINE_FIELD(m_flNextFoundEnemySoundTime, FIELD_TIME),
	DEFINE_FIELD(m_flSpeedModifier, FIELD_TIME),

	DEFINE_INPUTFUNC(FIELD_FLOAT, "SetSpeedModifier", InputSetSpeedModifier),
	DEFINE_INPUTFUNC(FIELD_VOID, "EnableOpenDoors", InputEnableOpenDoors),
	DEFINE_INPUTFUNC(FIELD_VOID, "DisableOpenDoors", InputDisableOpenDoors),
	DEFINE_INPUTFUNC(FIELD_VOID, "EnablePickupWeapons", InputEnablePickupWeapons),
	DEFINE_INPUTFUNC(FIELD_VOID, "DisablePickupWeapons", InputDisablePickupWeapons)
END_DATADESC()

AI_BEGIN_CUSTOM_NPC(npc_base_custom, CNPC_BaseCustomNPC)
//=========================================================
// > Melee_Attack_NoInterrupt
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_MELEE_ATTACK_NOINTERRUPT,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
	"		TASK_MELEE_ATTACK1		0"
	""
	"	Interrupts"
	"		COND_ENEMY_DEAD"
	"		COND_ENEMY_OCCLUDED"
);

//=========================================================
// 	SCHED_HIDE
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HIDE,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_COMBAT_FACE"
	"		TASK_STOP_MOVING			0"
	"		TASK_FIND_COVER_FROM_ENEMY	0"
	"		TASK_RUN_PATH				0"
	"		TASK_WAIT_FOR_MOVEMENT		0"
	"		TASK_REMEMBER				MEMORY:INCOVER"
	"		TASK_FACE_ENEMY				0"
	""
	"	Interrupts"
	"		COND_HEAR_DANGER"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
);
AI_END_CUSTOM_NPC()


//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_BaseCustomNPC::Precache( void )
{
	// If no model name is supplied, use the default citizen model
	if (!GetModelName())
	{
		SetModelName(MAKE_STRING("models/monster/subject.mdl")); // TODO replace this with citizen
	}

	if (&m_iszWeaponModelName && m_iszWeaponModelName != MAKE_STRING("")) {
		PrecacheModel(STRING(m_iszWeaponModelName));
	}
	else {
		PrecacheModel("models/props_canal/mattpipe.mdl"); // Default weapon model
	}

	PrecacheModel(STRING(GetModelName()));
	PrecacheNPCSoundScript(&m_iszFearSound, MAKE_STRING("NPC_BaseCustomr.Fear"));
	PrecacheNPCSoundScript(&m_iszIdleSound, MAKE_STRING("NPC_BaseCustom.Idle"));
	PrecacheNPCSoundScript(&m_iszAlertSound, MAKE_STRING("NPC_BaseCustom.Alert"));
	PrecacheNPCSoundScript(&m_iszPainSound, MAKE_STRING("NPC_BaseCustom.Pain"));
	PrecacheNPCSoundScript(&m_iszLostEnemySound, MAKE_STRING("NPC_BaseCustom.LostEnemy"));
	PrecacheNPCSoundScript(&m_iszFoundEnemySound, MAKE_STRING("NPC_BaseCustom.FoundEnemy"));
	PrecacheNPCSoundScript(&m_iszDeathSound, MAKE_STRING("NPC_BaseCustom.Death"));

	m_bWanderToggle = false;

	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_BaseCustomNPC::Spawn( void )
{
	Precache();

	SetModel(STRING(GetModelName()));
	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	SetBloodColor( BLOOD_COLOR_RED );
	
	// If the health has not been set through Hammer, use a default health value of 75
	if (m_iHealth < 1) 
	{
		m_iHealth = 75;
	}

	m_flFieldOfView		= 0.5;
	m_flNextSoundTime = gpGlobals->curtime;
	m_flNextFoundEnemySoundTime = gpGlobals->curtime;
	m_NPCState			= NPC_STATE_NONE;
	m_flSpeedModifier = 1.0f;

	CapabilitiesClear();

	if (!HasSpawnFlags(SF_NPC_START_EFFICIENT))
	{
		CapabilitiesAdd(bits_CAP_ANIMATEDFACE | bits_CAP_TURN_HEAD); // The default model has no face animations, but a custom model might
		CapabilitiesAdd(bits_CAP_SQUAD);
		CapabilitiesAdd(bits_CAP_USE_WEAPONS | bits_CAP_AIM_GUN | bits_CAP_MOVE_SHOOT);
		CapabilitiesAdd(bits_CAP_WEAPON_MELEE_ATTACK1 || bits_CAP_WEAPON_MELEE_ATTACK2);
		CapabilitiesAdd(bits_CAP_DUCK);
		CapabilitiesAdd(bits_CAP_USE_SHOT_REGULATOR);

		if (!m_bCannotOpenDoors) {
			CapabilitiesAdd(bits_CAP_DOORS_GROUP);
		}
	}

	CapabilitiesAdd(bits_CAP_MOVE_GROUND);
	SetMoveType(MOVETYPE_STEP);

	NPCInit();
}

void CNPC_BaseCustomNPC::Activate()
{
	BaseClass::Activate();
	FixupWeapon();
}

//-----------------------------------------------------------------------------
// Purpose: If this NPC has some kind of custom weapon behavior,
//	set up the weapon after spawn.
//-----------------------------------------------------------------------------
void CNPC_BaseCustomNPC::FixupWeapon()
{
	// Do nothing
}


//-----------------------------------------------------------------------------
// Purpose: Choose a schedule after schedule failed
//-----------------------------------------------------------------------------
int CNPC_BaseCustomNPC::SelectFailSchedule(int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode)
{
	switch (failedSchedule)
	{
	case SCHED_NEW_WEAPON:
		// If failed trying to pick up a weapon, try again in one second. This is because other AI code
		// has put this off for 10 seconds under the assumption that the citizen would be able to 
		// pick up the weapon that they found. 
		m_flNextWeaponSearchTime = gpGlobals->curtime + 1.0f;
		break;
	}

	return BaseClass::SelectFailSchedule(failedSchedule, failedTask, taskFailCode);
}

//-----------------------------------------------------------------------------
// Purpose: Select a schedule to retrieve better weapons if they are available.
//-----------------------------------------------------------------------------
int CNPC_BaseCustomNPC::SelectScheduleRetrieveItem()
{
	if (m_bCanPickupWeapons && HasCondition(COND_BETTER_WEAPON_AVAILABLE))
	{
		CBaseHLCombatWeapon *pWeapon = dynamic_cast<CBaseHLCombatWeapon *>(Weapon_FindUsable(WEAPON_SEARCH_DELTA));
		if (pWeapon)
		{
			m_flNextWeaponSearchTime = gpGlobals->curtime + 10.0;
			// Now lock the weapon for several seconds while we go to pick it up.
			pWeapon->Lock(10.0, this);
			SetTarget(pWeapon);
			return SCHED_NEW_WEAPON;
		}
	}

	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: Select ideal state.
//		Conditions for custom states are defined here.
//-----------------------------------------------------------------------------
NPC_STATE CNPC_BaseCustomNPC::SelectIdealState(void)
{
	switch ((int)this->m_NPCState) {
		case NPC_STATE_AMBUSH:
			return SelectAmbushIdealState();
		case NPC_STATE_SURRENDER:
			return SelectSurrenderIdealState();
		default:
			return BaseClass::SelectIdealState();
	}

}

NPC_STATE CNPC_BaseCustomNPC::SelectAmbushIdealState()
{
	// AMBUSH goes to ALERT upon death of enemy
	if (GetEnemy() == NULL)
	{
		return NPC_STATE_ALERT;
	}

	// If I am not in a squad, there is no reason to ambush
	if (!m_pSquad) {
		return NPC_STATE_COMBAT;
	}

	// If I am the last in a squad, attack!
	if (m_pSquad->NumMembers() == 1) {
		return NPC_STATE_COMBAT;
	}

	if (OccupyStrategySlotRange(SQUAD_SLOT_CHASE_1, SQUAD_SLOT_CHASE_2)) {
		return NPC_STATE_COMBAT;
	}

	// The best ideal state is the current ideal state.
	return (NPC_STATE)NPC_STATE_AMBUSH;
}

NPC_STATE CNPC_BaseCustomNPC::SelectSurrenderIdealState()
{
	return NPC_STATE_ALERT;
}

//-----------------------------------------------------------------------------
// Purpose: Select a schedule to retrieve better weapons if they are available.
//-----------------------------------------------------------------------------
int CNPC_BaseCustomNPC::SelectScheduleWander()
{
	m_bWanderToggle = !m_bWanderToggle;
	if (m_bWanderToggle) {
		return SCHED_IDLE_WANDER;
	}
	else {
		return SCHED_NONE;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Select a schedule to execute based on conditions. 
// This is the most critical AI method.
//-----------------------------------------------------------------------------
int CNPC_BaseCustomNPC::SelectSchedule()
{
	switch ((int)m_NPCState)
	{
	case NPC_STATE_IDLE:
		AssertMsgOnce(GetEnemy() == NULL, "NPC has enemy but is not in combat state?");
		return SelectIdleSchedule();

	case NPC_STATE_ALERT:
		AssertMsgOnce(GetEnemy() == NULL, "NPC has enemy but is not in combat state?");
		return SelectAlertSchedule();

	case NPC_STATE_COMBAT:
		return SelectCombatSchedule();
	case NPC_STATE_AMBUSH:
		return SelectAmbushSchedule();
	case NPC_STATE_SURRENDER:
		return SelectSurrenderSchedule();
	default:
		return BaseClass::SelectSchedule();
	}
}

//-----------------------------------------------------------------------------
// Idle schedule selection
//-----------------------------------------------------------------------------
int CNPC_BaseCustomNPC::SelectIdleSchedule()
{
	int nSched = SelectFlinchSchedule();
	if (nSched != SCHED_NONE)
		return nSched;

	if (HasCondition(COND_HEAR_DANGER) ||
		HasCondition(COND_HEAR_COMBAT) ||
		HasCondition(COND_HEAR_WORLD) ||
		HasCondition(COND_HEAR_BULLET_IMPACT) ||
		HasCondition(COND_HEAR_PLAYER))
	{
		// Investigate sound source
		return SCHED_ALERT_FACE_BESTSOUND;
	}

	nSched = SelectScheduleRetrieveItem();
	if (nSched != SCHED_NONE)
		return nSched;

	// no valid route! Wander instead
	if (GetNavigator()->GetGoalType() == GOALTYPE_NONE) {
		return SCHED_IDLE_STAND;
	}

	// valid route. Get moving
	return SCHED_IDLE_WALK;
}

//-----------------------------------------------------------------------------
// Alert schedule selection
//-----------------------------------------------------------------------------
int CNPC_BaseCustomNPC::SelectAlertSchedule()
{
	// Per default base NPC, check flinch schedule first
	int nSched = SelectFlinchSchedule();
	if (nSched != SCHED_NONE)
		return nSched;

	// Scan around for new enemies
	if (HasCondition(COND_ENEMY_DEAD) && SelectWeightedSequence(ACT_VICTORY_DANCE) != ACTIVITY_NOT_AVAILABLE)
		return SCHED_ALERT_SCAN;

	if (HasCondition(COND_HEAR_DANGER) ||
		HasCondition(COND_HEAR_PLAYER) ||
		HasCondition(COND_HEAR_WORLD) ||
		HasCondition(COND_HEAR_BULLET_IMPACT) ||
		HasCondition(COND_HEAR_COMBAT))
	{
		// Investigate sound source
		AlertSound();
		return SCHED_ALERT_FACE_BESTSOUND;
	}

	nSched = SelectScheduleRetrieveItem();
	if (nSched != SCHED_NONE)
		return nSched;

	// no valid route! Wander instead
	if (GetNavigator()->GetGoalType() == GOALTYPE_NONE) {
		return SCHED_ALERT_STAND;
	}

	// valid route. Get moving
	return SCHED_ALERT_WALK;
}

//-----------------------------------------------------------------------------
// Combat schedule selection
//-----------------------------------------------------------------------------
int CNPC_BaseCustomNPC::SelectCombatSchedule()
{
	return BaseClass::SelectSchedule(); // Let Base NPC handle it
}

//-----------------------------------------------------------------------------
// Combat schedule selection
//-----------------------------------------------------------------------------
int CNPC_BaseCustomNPC::SelectAmbushSchedule()
{
	// Check enemy death
	if (HasCondition(COND_ENEMY_DEAD))
	{
		// clear the current (dead) enemy and try to find another.
		SetEnemy(NULL);

		if (ChooseEnemy())
		{
			SetState(NPC_STATE_COMBAT);
			FoundEnemySound();
			ClearCondition(COND_ENEMY_DEAD);
			return SelectSchedule();
		}

		SetState(NPC_STATE_ALERT);
		return SelectSchedule();
	}

	
	CBaseEntity* pEnemy = GetEnemy();
	if (pEnemy && EnemyDistance(pEnemy) < 128)
	{
		SetState(NPC_STATE_COMBAT);
		return SelectSchedule();
	}
	
	if (pEnemy == NULL || HasCondition(COND_LOST_ENEMY)) {
		SetState(NPC_STATE_ALERT);
		return SelectSchedule();
	}

	// If I am the last in a squad, attack!
	if (m_iNumSquadmates > m_pSquad->NumMembers())
		SetState(SelectAmbushIdealState());

	if (HasCondition(COND_LIGHT_DAMAGE)) {
		SetState(NPC_STATE_COMBAT);
	}

	if (HasCondition(COND_SEE_ENEMY) && HasCondition(COND_ENEMY_FACING_ME) && HasCondition(COND_HAVE_ENEMY_LOS)) {
		if(GetState() != NPC_STATE_COMBAT)
			SetState(SelectAmbushIdealState());
		return SCHED_HIDE;
	}

	m_iNumSquadmates = m_pSquad->NumMembers();

	return SCHED_COMBAT_FACE;
}

int CNPC_BaseCustomNPC::SelectSurrenderSchedule()
{
	return BaseClass::SelectSchedule();
}

bool CNPC_BaseCustomNPC::HasRangedWeapon()
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();

	if (pWeapon)
		return !(FClassnameIs(pWeapon, "weapon_crowbar") || FClassnameIs(pWeapon, "weapon_stunstick") || FClassnameIs(pWeapon, "weapon_custommelee"));

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Override base class activiites
//-----------------------------------------------------------------------------
Activity CNPC_BaseCustomNPC::NPC_TranslateActivity(Activity activity)
{
	switch (activity) {
	case ACT_RUN_AIM_SHOTGUN:
		return ACT_RUN_AIM_RIFLE;
	case ACT_WALK_AIM_SHOTGUN:
		return ACT_WALK_AIM_RIFLE;
	case ACT_IDLE_ANGRY_SHOTGUN:
		return ACT_IDLE_ANGRY_SMG1;
	case ACT_RANGE_ATTACK_SHOTGUN_LOW:
		return ACT_RANGE_ATTACK_SMG1_LOW;
	case ACT_IDLE_MELEE:
	case ACT_IDLE_ANGRY_MELEE:  // If the NPC has a melee weapon but is in an idle state, don't raise the weapon
		if (m_NPCState == NPC_STATE_IDLE)
			return ACT_IDLE_SUITCASE;
	default:
		return BaseClass::NPC_TranslateActivity(activity);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override base class schedules
//-----------------------------------------------------------------------------
int CNPC_BaseCustomNPC::TranslateSchedule(int scheduleType)
{
	return BaseClass::TranslateSchedule(scheduleType);
}

//-----------------------------------------------------------------------------
// Purpose: Play sound when an enemy is spotted. This sound has a separate
//	timer from other sounds to prevent looping if the NPC gets caught
//	in a 'found enemy' condition.
//-----------------------------------------------------------------------------
void CNPC_BaseCustomNPC::FoundEnemySound(void)
{
	if (gpGlobals->curtime > m_flNextFoundEnemySoundTime)
	{
		m_flNextFoundEnemySoundTime = gpGlobals->curtime + random->RandomFloat(MIN_TIME_NEXT_FOUNDENEMY_SOUND, MAX_TIME_NEXT_FOUNDENEMY_SOUND);
		PlaySound(m_iszFoundEnemySound, true);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Play NPC soundscript
//-----------------------------------------------------------------------------
void CNPC_BaseCustomNPC::PlaySound(string_t soundname, bool required /*= false */)
{
	// TODO: Check if silent
	if (required || gpGlobals->curtime > m_flNextSoundTime)
	{
		m_flNextSoundTime = gpGlobals->curtime + random->RandomFloat(MIN_TIME_NEXT_SOUND, MAX_TIME_NEXT_SOUND);
		//CPASAttenuationFilter filter2(this, STRING(soundname));
		EmitSound(STRING(soundname));
	}
}

//-----------------------------------------------------------------------------
// Purpose: Assign a default soundscript if none is provided, then precache
//-----------------------------------------------------------------------------
void CNPC_BaseCustomNPC::PrecacheNPCSoundScript(string_t * SoundName, string_t defaultSoundName) 
{
	if (!SoundName) {
		*SoundName = defaultSoundName;
	}
	PrecacheScriptSound(STRING(*SoundName));
}

//-----------------------------------------------------------------------------
// Purpose: Get movement speed, multipled by modifier
//-----------------------------------------------------------------------------
float CNPC_BaseCustomNPC::GetSequenceGroundSpeed(CStudioHdr *pStudioHdr, int iSequence)
{
	float t = SequenceDuration(pStudioHdr, iSequence);

	if (t > 0)
	{
		return (GetSequenceMoveDist(pStudioHdr, iSequence) * m_flSpeedModifier / t);
	}
	else
	{
		return 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hammer input to change the speed of the NPC
//-----------------------------------------------------------------------------
void CNPC_BaseCustomNPC::InputSetSpeedModifier(inputdata_t &inputdata)
{
	this->m_flSpeedModifier = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: Hammer input to enable opening doors
//-----------------------------------------------------------------------------
void CNPC_BaseCustomNPC::InputEnableOpenDoors(inputdata_t &inputdata)
{
	m_bCannotOpenDoors = false;
	if (!HasSpawnFlags(SF_NPC_START_EFFICIENT))
	{
		CapabilitiesAdd(bits_CAP_DOORS_GROUP);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hammer input to enable opening doors
//-----------------------------------------------------------------------------
void CNPC_BaseCustomNPC::InputDisableOpenDoors(inputdata_t &inputdata)
{
	m_bCannotOpenDoors = true;
	if (!HasSpawnFlags(SF_NPC_START_EFFICIENT))
	{
		CapabilitiesRemove(bits_CAP_DOORS_GROUP);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hammer input to enable weapon pickup behavior
//-----------------------------------------------------------------------------
void CNPC_BaseCustomNPC::InputEnablePickupWeapons(inputdata_t &inputdata)
{
	m_bCanPickupWeapons = true;
}

//-----------------------------------------------------------------------------
// Purpose: Hammer input to enable weapon pickup behavior
//-----------------------------------------------------------------------------
void CNPC_BaseCustomNPC::InputDisablePickupWeapons(inputdata_t &inputdata)
{
	m_bCanPickupWeapons = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
// Output : 
//-----------------------------------------------------------------------------
Class_T	CNPC_BaseCustomNPC::Classify( void )
{
	return	CLASS_NONE;
}

//=============================================================================//
//
// Purpose: CS:S weapons recreated from scratch in Source SDK 2013 for usage in a Half-Life 2 setting.
//
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "weapon_css_base.h"

// Mapbase v7.0 has a framework in place to expand the number of activities available to NPCs.
#if MAPBASE_VER_INT >= 7000
#define USE_EXTERN_ACTTABLES 1
#else
#define USE_EXTERN_ACTTABLES 0
#endif

#if USE_EXTERN_ACTTABLES == 1
extern acttable_t *GetPistolActtable();
extern int GetPistolActtableCount();

extern acttable_t *Get357Acttable();
extern int Get357ActtableCount();

extern acttable_t *GetSMG1Acttable();
extern int GetSMG1ActtableCount();

extern acttable_t *GetAR2Acttable();
extern int GetAR2ActtableCount();

extern acttable_t *GetShotgunActtable();
extern int GetShotgunActtableCount();
#endif

//-----------------------------------------------------------------------------
// CSSHL2_WEAPON_PISTOL / CSSHL2_WEAPON_PISTOL_POWERFUL
//-----------------------------------------------------------------------------

#if USE_EXTERN_ACTTABLES == 1
acttable_t *GetCSSActTable_Pistol()
{
	return GetPistolActtable();
}

int GetCSSActTableCount_Pistol() { return GetPistolActtableCount(); }

acttable_t *GetCSSActTable_Revolver()
{
	return Get357Acttable();
}

int GetCSSActTableCount_Revolver() { return Get357ActtableCount(); }
#else
static acttable_t	g_DefaultCSSPistolActtable[] =
{
	{ ACT_IDLE,						ACT_IDLE_PISTOL,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_PISTOL,			true },
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_PISTOL,		true },
	{ ACT_RELOAD,					ACT_RELOAD_PISTOL,				true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_PISTOL,			true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_PISTOL,				true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_PISTOL,true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_PISTOL_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_PISTOL_LOW,	false },
	{ ACT_COVER_LOW,				ACT_COVER_PISTOL_LOW,			false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_PISTOL_LOW,		false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_WALK,						ACT_WALK_PISTOL,				false },
	{ ACT_RUN,						ACT_RUN_PISTOL,					false },

	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_PISTOL,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_PISTOL,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_PISTOL,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_PISTOL,					false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_PISTOL,				false },
};

acttable_t *GetCSSActTable_Pistol()
{
	return g_DefaultCSSPistolActtable;
}

int GetCSSActTableCount_Pistol() { return ARRAYSIZE( g_DefaultCSSPistolActtable ); }

acttable_t *GetCSSActTable_Revolver()
{
	// No difference without Mapbase
	return g_DefaultCSSPistolActtable;
}

int GetCSSActTableCount_Revolver() { return ARRAYSIZE( g_DefaultCSSPistolActtable ); }
#endif

//-----------------------------------------------------------------------------
// CSSHL2_WEAPON_SMG1 / CSSHL2_WEAPON_SMG2
//-----------------------------------------------------------------------------

#if USE_EXTERN_ACTTABLES == 1
acttable_t *GetCSSActTable_SMG1()
{
	return GetSMG1Acttable();
}

int GetCSSActTableCount_SMG1() { return GetSMG1ActtableCount(); }

static acttable_t	g_DefaultCSSSMG2Acttable[] =
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_SMG2, true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SMG2,	true },

#if EXPANDED_HL2_UNUSED_WEAPON_ACTIVITIES
	// Optional new NPC activities
	// (these should fall back to SMG animations when they don't exist on an NPC)
	{ ACT_RELOAD,					ACT_RELOAD_SMG2,			true },
	{ ACT_IDLE,						ACT_IDLE_SMG2,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SMG2,		true },

// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SMG2_RELAXED,			false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SMG2_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_SMG2,			false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_SMG2_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_SMG2_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_SMG2,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_SMG2_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_SMG2_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_SMG2,				false },//always aims

// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG2_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_SMG2_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG2,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_SMG2_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_SMG2_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_SMG2,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_SMG2_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_SMG2_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_SMG2,				false },//always aims
//End readiness activities

	{ ACT_WALK,						ACT_WALK_SMG2,					true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_SMG2,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,					true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,				true },
	{ ACT_RUN,						ACT_RUN_SMG2,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_SMG2,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,					true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,				true },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SMG2_LOW,		true },
	{ ACT_COVER_LOW,				ACT_COVER_SMG2_LOW,				false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_SMG2_LOW,			false },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SMG2_LOW,			false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SMG2,		true },

	{ ACT_ARM,						ACT_ARM_RIFLE,					false },
	{ ACT_DISARM,					ACT_DISARM_RIFLE,				false },

#if EXPANDED_HL2_COVER_ACTIVITIES
	{ ACT_RANGE_AIM_MED,			ACT_RANGE_AIM_SMG2_MED,			false },
	{ ACT_RANGE_ATTACK1_MED,		ACT_RANGE_ATTACK_SMG2_MED,		false },
#endif

#if EXPANDED_HL2DM_ACTIVITIES
	// HL2:DM activities (for third-person animations in SP)
	{ ACT_HL2MP_IDLE,                    ACT_HL2MP_IDLE_SMG2,                    false },
	{ ACT_HL2MP_RUN,                    ACT_HL2MP_RUN_SMG2,                    false },
	{ ACT_HL2MP_IDLE_CROUCH,            ACT_HL2MP_IDLE_CROUCH_SMG2,            false },
	{ ACT_HL2MP_WALK_CROUCH,            ACT_HL2MP_WALK_CROUCH_SMG2,            false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,    ACT_HL2MP_GESTURE_RANGE_ATTACK_SMG2,    false },
	{ ACT_HL2MP_GESTURE_RELOAD,            ACT_HL2MP_GESTURE_RELOAD_SMG2,        false },
	{ ACT_HL2MP_JUMP,                    ACT_HL2MP_JUMP_SMG2,                    false },
	{ ACT_HL2MP_WALK,					ACT_HL2MP_WALK_SMG2,					false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK2,	ACT_HL2MP_GESTURE_RANGE_ATTACK2_SMG2,    false },
#endif
#endif
};

acttable_t *GetCSSActTable_SMG2()
{
	return g_DefaultCSSSMG2Acttable;
}

int GetCSSActTableCount_SMG2() { return ARRAYSIZE( g_DefaultCSSSMG2Acttable ); }
#else
static acttable_t	g_DefaultCSSSMG1Acttable[] =
{
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_SMG1,			true },
	{ ACT_RELOAD,					ACT_RELOAD_SMG1,				true },
	{ ACT_IDLE,						ACT_IDLE_SMG1,					true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SMG1,			true },

	{ ACT_WALK,						ACT_WALK_RIFLE,					true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true  },
	
// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SMG1_RELAXED,			false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SMG1_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
//End readiness activities

	{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,			true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,		true },
	{ ACT_RUN,						ACT_RUN_RIFLE,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_RIFLE,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,			true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,		true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SMG1,	true },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SMG1_LOW,		true },
	{ ACT_COVER_LOW,				ACT_COVER_SMG1_LOW,				false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_SMG1_LOW,			false },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SMG1_LOW,			false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SMG1,		true },

	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_SMG1,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_SMG1,						false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_SMG1,				false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_SMG1,				false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_SMG1,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_SMG1,			false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_SMG1,					false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_SMG1,					false },
};

acttable_t *GetCSSActTable_SMG1()
{
	return g_DefaultCSSSMG1Acttable;
}

int GetCSSActTableCount_SMG1() { return ARRAYSIZE( g_DefaultCSSSMG1Acttable ); }

acttable_t *GetCSSActTable_SMG2()
{
	// Use default SMG1 for now
	return g_DefaultCSSSMG1Acttable;
}

int GetCSSActTableCount_SMG2() { return ARRAYSIZE( g_DefaultCSSSMG1Acttable ); }
#endif

//-----------------------------------------------------------------------------
// CSSHL2_WEAPON_AR1 / CSSHL2_WEAPON_AR2
//-----------------------------------------------------------------------------

#if USE_EXTERN_ACTTABLES == 1
static acttable_t	g_DefaultCSSAR1Acttable[] =
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_AR1, true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_AR1,	true },
	
#if EXPANDED_HL2_UNUSED_WEAPON_ACTIVITIES
	// Optional new NPC activities
	// (these should fall back to AR2 animations when they don't exist on an NPC)
	{ ACT_RELOAD,					ACT_RELOAD_AR1,			true },
	{ ACT_IDLE,						ACT_IDLE_AR1,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_AR1,		true },

// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_AR1_RELAXED,			false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_AR1_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_AR1,			false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_AR1_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_AR1_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_AR1,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_AR1_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_AR1_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_AR1,				false },//always aims

// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_AR1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_AR1_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_AR1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_AR1_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_AR1_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_AR1,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_AR1_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_AR1_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_AR1,				false },//always aims
//End readiness activities

	{ ACT_WALK,						ACT_WALK_AR1,					true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_AR1,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,					true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,				true },
	{ ACT_RUN,						ACT_RUN_AR1,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_AR1,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,					true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,				true },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_AR1_LOW,		true },
	{ ACT_COVER_LOW,				ACT_COVER_AR1_LOW,				false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_AR1_LOW,			false },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_AR1_LOW,			false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_AR1,		true },

	{ ACT_ARM,						ACT_ARM_RIFLE,					false },
	{ ACT_DISARM,					ACT_DISARM_RIFLE,				false },

#if EXPANDED_HL2_COVER_ACTIVITIES
	{ ACT_RANGE_AIM_MED,			ACT_RANGE_AIM_AR1_MED,			false },
	{ ACT_RANGE_ATTACK1_MED,		ACT_RANGE_ATTACK_AR1_MED,		false },
#endif

#if EXPANDED_HL2DM_ACTIVITIES
	// HL2:DM activities (for third-person animations in SP)
	{ ACT_HL2MP_IDLE,                    ACT_HL2MP_IDLE_AR1,                    false },
	{ ACT_HL2MP_RUN,                    ACT_HL2MP_RUN_AR1,                    false },
	{ ACT_HL2MP_IDLE_CROUCH,            ACT_HL2MP_IDLE_CROUCH_AR1,            false },
	{ ACT_HL2MP_WALK_CROUCH,            ACT_HL2MP_WALK_CROUCH_AR1,            false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,    ACT_HL2MP_GESTURE_RANGE_ATTACK_AR1,    false },
	{ ACT_HL2MP_GESTURE_RELOAD,            ACT_HL2MP_GESTURE_RELOAD_AR1,        false },
	{ ACT_HL2MP_JUMP,                    ACT_HL2MP_JUMP_AR1,                    false },
	{ ACT_HL2MP_WALK,					ACT_HL2MP_WALK_AR1,					false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK2,	ACT_HL2MP_GESTURE_RANGE_ATTACK2_AR1,    false },
#endif
#endif
};

acttable_t *GetCSSActTable_AR1()
{
	return g_DefaultCSSAR1Acttable;
}

int GetCSSActTableCount_AR1() { return ARRAYSIZE( g_DefaultCSSAR1Acttable ); }

acttable_t *GetCSSActTable_AR2()
{
	return GetAR2Acttable();
}

int GetCSSActTableCount_AR2() { return GetAR2ActtableCount(); }
#else
static acttable_t	g_DefaultCSSAR2Acttable[] =
{
#if AR2_ACTIVITY_FIX == 1
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_AR2,			true },
	{ ACT_RELOAD,					ACT_RELOAD_AR2,				true },
	{ ACT_IDLE,						ACT_IDLE_AR2,					true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_AR2,			false },

	{ ACT_WALK,						ACT_WALK_AR2,					true },

// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_AR2_RELAXED,			false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_AR2_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_AR2,			false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_AR2_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_AR2_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_AR2,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_AR2_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_AR2_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_AR2_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_AR2_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_AR2,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_AR2_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_AR2_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_AR2,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_AR2_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_AR2_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
//End readiness activities

	{ ACT_WALK_AIM,					ACT_WALK_AIM_AR2,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,			true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,		true },
	{ ACT_RUN,						ACT_RUN_AR2,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_RIFLE,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,			true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,		true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_AR2,	false },
	{ ACT_COVER_LOW,				ACT_COVER_SMG1_LOW,				false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_AR2_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_AR2_LOW,		false },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SMG1_LOW,			false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_AR2,		true },
//	{ ACT_RANGE_ATTACK2, ACT_RANGE_ATTACK_AR2_GRENADE, true },
#else
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_AR2,			true },
	{ ACT_RELOAD,					ACT_RELOAD_SMG1,				true },		// FIXME: hook to AR2 unique
	{ ACT_IDLE,						ACT_IDLE_SMG1,					true },		// FIXME: hook to AR2 unique
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SMG1,			true },		// FIXME: hook to AR2 unique

	{ ACT_WALK,						ACT_WALK_RIFLE,					true },

// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SMG1_RELAXED,			false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SMG1_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
//End readiness activities

	{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,			true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,		true },
	{ ACT_RUN,						ACT_RUN_RIFLE,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_RIFLE,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,			true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,		true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_AR2,	false },
	{ ACT_COVER_LOW,				ACT_COVER_SMG1_LOW,				false },		// FIXME: hook to AR2 unique
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_AR2_LOW,			false },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SMG1_LOW,		true },		// FIXME: hook to AR2 unique
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SMG1_LOW,			false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SMG1,		true },
//	{ ACT_RANGE_ATTACK2, ACT_RANGE_ATTACK_AR2_GRENADE, true },
#endif

	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_AR2,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_AR2,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_AR2,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_AR2,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_AR2,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_AR2,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_AR2,					false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_AR2,				false },
};

acttable_t *GetCSSActTable_AR1()
{
	// Use default AR2 for now
	return g_DefaultCSSAR2Acttable;
}

int GetCSSActTableCount_AR1() { return ARRAYSIZE( g_DefaultCSSAR2Acttable ); }

acttable_t *GetCSSActTable_AR2()
{
	return g_DefaultCSSAR2Acttable;
}

int GetCSSActTableCount_AR2() { return ARRAYSIZE( g_DefaultCSSAR2Acttable ); }
#endif

//-----------------------------------------------------------------------------
// CSSHL2_WEAPON_SHOTGUN
//-----------------------------------------------------------------------------

#if USE_EXTERN_ACTTABLES == 1
acttable_t *GetCSSActTable_Shotgun()
{
	return GetShotgunActtable();
}

int GetCSSActTableCount_Shotgun() { return GetShotgunActtableCount(); }
#else
static acttable_t	g_DefaultCSSShotgunActtable[] =
{
	{ ACT_IDLE,						ACT_IDLE_SMG1,					true },	// FIXME: hook to shotgun unique

	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_SHOTGUN,			true },
	{ ACT_RELOAD,					ACT_RELOAD_SHOTGUN,					false },
	{ ACT_WALK,						ACT_WALK_RIFLE,						true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SHOTGUN,				true },

// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SHOTGUN_RELAXED,		false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SHOTGUN_STIMULATED,	false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_SHOTGUN_AGITATED,		false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_RIFLE,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
//End readiness activities

	{ ACT_WALK_AIM,					ACT_WALK_AIM_SHOTGUN,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,				true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,			true },
	{ ACT_RUN,						ACT_RUN_RIFLE,						true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_SHOTGUN,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,				true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,			true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SHOTGUN,	true },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SHOTGUN_LOW,		true },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SHOTGUN_LOW,				false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SHOTGUN,			false },

	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_SHOTGUN,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_SHOTGUN,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_SHOTGUN,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_SHOTGUN,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_SHOTGUN,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_SHOTGUN,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_SHOTGUN,					false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_SHOTGUN,				false },
};

acttable_t *GetCSSActTable_Shotgun()
{
	return g_DefaultCSSShotgunActtable;
}

int GetCSSActTableCount_Shotgun() { return ARRAYSIZE( g_DefaultCSSShotgunActtable ); }
#endif

//-----------------------------------------------------------------------------
// CSSHL2_WEAPON_SNIPER_RIFLE
//-----------------------------------------------------------------------------

#if USE_EXTERN_ACTTABLES == 1
static acttable_t	g_DefaultCSSSniperRifleActtable[] =
{
	{	ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_SNIPER_RIFLE, true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_SNIPER_RIFLE,	true },

#if EXPANDED_HL2_UNUSED_WEAPON_ACTIVITIES
	// Optional new NPC activities
	// (these should fall back to AR2 animations when they don't exist on an NPC)
	{ ACT_RELOAD,					ACT_RELOAD_SNIPER_RIFLE,			true },
	{ ACT_IDLE,						ACT_IDLE_SNIPER_RIFLE,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SNIPER_RIFLE,		true },

// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_SNIPER_RIFLE_RELAXED,			false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_SNIPER_RIFLE_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_SNIPER_RIFLE,			false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_SNIPER_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_SNIPER_RIFLE_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_SNIPER_RIFLE,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_SNIPER_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_SNIPER_RIFLE_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_SNIPER_RIFLE,				false },//always aims

// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SNIPER_RIFLE_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_SNIPER_RIFLE_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SNIPER_RIFLE,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_SNIPER_RIFLE_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_SNIPER_RIFLE_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_SNIPER_RIFLE,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_SNIPER_RIFLE_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_SNIPER_RIFLE_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_SNIPER_RIFLE,				false },//always aims
//End readiness activities

	{ ACT_WALK,						ACT_WALK_SNIPER_RIFLE,					true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_SNIPER_RIFLE,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,					true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,				true },
	{ ACT_RUN,						ACT_RUN_SNIPER_RIFLE,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_SNIPER_RIFLE,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,					true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,				true },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SNIPER_RIFLE_LOW,		true },
	{ ACT_COVER_LOW,				ACT_COVER_SNIPER_RIFLE_LOW,				false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_SNIPER_RIFLE_LOW,			false },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_SNIPER_RIFLE_LOW,			false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SNIPER_RIFLE,		true },

	{ ACT_ARM,						ACT_ARM_RIFLE,					false },
	{ ACT_DISARM,					ACT_DISARM_RIFLE,				false },

#if EXPANDED_HL2_COVER_ACTIVITIES
	{ ACT_RANGE_AIM_MED,			ACT_RANGE_AIM_SNIPER_RIFLE_MED,			false },
	{ ACT_RANGE_ATTACK1_MED,		ACT_RANGE_ATTACK_SNIPER_RIFLE_MED,		false },
#endif

#if EXPANDED_HL2DM_ACTIVITIES
	// HL2:DM activities (for third-person animations in SP)
	{ ACT_HL2MP_IDLE,                    ACT_HL2MP_IDLE_SNIPER_RIFLE,                    false },
	{ ACT_HL2MP_RUN,                    ACT_HL2MP_RUN_SNIPER_RIFLE,                    false },
	{ ACT_HL2MP_IDLE_CROUCH,            ACT_HL2MP_IDLE_CROUCH_SNIPER_RIFLE,            false },
	{ ACT_HL2MP_WALK_CROUCH,            ACT_HL2MP_WALK_CROUCH_SNIPER_RIFLE,            false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,    ACT_HL2MP_GESTURE_RANGE_ATTACK_SNIPER_RIFLE,    false },
	{ ACT_HL2MP_GESTURE_RELOAD,            ACT_HL2MP_GESTURE_RELOAD_SNIPER_RIFLE,        false },
	{ ACT_HL2MP_JUMP,                    ACT_HL2MP_JUMP_SNIPER_RIFLE,                    false },
	{ ACT_HL2MP_WALK,					ACT_HL2MP_WALK_SNIPER_RIFLE,					false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK2,	ACT_HL2MP_GESTURE_RANGE_ATTACK2_SNIPER_RIFLE,    false },
#endif
#endif
};

acttable_t *GetCSSActTable_SniperRifle()
{
	return g_DefaultCSSSniperRifleActtable;
}

int GetCSSActTableCount_SniperRifle() { return ARRAYSIZE( g_DefaultCSSSniperRifleActtable ); }
#else
acttable_t *GetCSSActTable_SniperRifle()
{
	// Use default AR2 for now
	return g_DefaultCSSAR2Acttable;
}

int GetCSSActTableCount_SniperRifle() { return ARRAYSIZE( g_DefaultCSSAR2Acttable ); }
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#ifndef TF_PLAYERANIMSTATE_H
#define TF_PLAYERANIMSTATE_H
#ifdef _WIN32
#pragma once
#endif


#include "convar.h"
#include "iplayeranimstate.h"
#include "asw_shareddefs.h"
#include "base_playeranimstate.h"


#ifdef CLIENT_DLL
	class C_BaseAnimatingOverlay;
	class C_WeaponCSBase;
	#define CBaseAnimatingOverlay C_BaseAnimatingOverlay
	#define CWeaponCSBase C_WeaponCSBase
	#define CASW_Player C_ASW_Player
#else
	class CBaseAnimatingOverlay;
	class CWeaponCSBase; 
	class CASW_Player;
#endif

enum PlayerAnimEvent_t
{
	PLAYERANIMEVENT_FIRE_GUN_PRIMARY=0,
	PLAYERANIMEVENT_FIRE_GUN_SECONDARY,
	PLAYERANIMEVENT_THROW_GRENADE,
	PLAYERANIMEVENT_JUMP,
	PLAYERANIMEVENT_RELOAD,
	PLAYERANIMEVENT_WEAPON_SWITCH,
	PLAYERANIMEVENT_FLINCH,
	PLAYERANIMEVENT_GO,
	PLAYERANIMEVENT_HALT,
	PLAYERANIMEVENT_PICKUP,
	PLAYERANIMEVENT_HEAL,
	PLAYERANIMEVENT_KICK,
	PLAYERANIMEVENT_PUNCH,
	PLAYERANIMEVENT_GETUP,
	PLAYERANIMEVENT_BAYONET,
	PLAYERANIMEVENT_MELEE,
	PLAYERANIMEVENT_MELEE_LAST = PLAYERANIMEVENT_MELEE + ASW_MAX_MELEE_ATTACKS,
	
	PLAYERANIMEVENT_COUNT
};


class IASWPlayerAnimState : virtual public IPlayerAnimState
{
public:
	// This is called by both the client and the server in the same way to trigger events for
	// players firing, jumping, throwing grenades, etc.
	virtual void DoAnimationEvent( PlayerAnimEvent_t event ) = 0;
	virtual bool IsAnimatingReload() = 0;	
	virtual bool IsDoingEmoteGesture() = 0;

	virtual bool IsAnimatingJump() = 0;
	virtual int GetMiscSequence() = 0;
	virtual float GetMiscCycle() = 0;
	virtual void SetMiscPlaybackRate( float flRate ) = 0;
	
	// for events that happened in the past, re-pose the model appropriately
	virtual void MiscCycleRewind( float flCycle ) = 0;
	virtual void MiscCycleUnrewind() = 0;
};


// This abstracts the differences between CS players and hostages.
class IASWPlayerAnimStateHelpers
{
public:
	virtual CBaseCombatWeapon* ASWAnim_GetActiveWeapon() = 0;
	virtual bool ASWAnim_CanMove() = 0;
};


IASWPlayerAnimState* CreatePlayerAnimState( CBaseAnimatingOverlay *pEntity, IASWPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences );
IASWPlayerAnimState* CreateHostageAnimState( CBaseAnimatingOverlay *pEntity, IASWPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences );

// If this is set, then the game code needs to make sure to send player animation events
// to the local player if he's the one being watched.
extern ConVar cl_showanimstate;


#endif // TF_PLAYERANIMSTATE_H

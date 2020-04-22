//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_PLAYERANIMSTATE_H
#define DOD_PLAYERANIMSTATE_H
#ifdef _WIN32
#pragma once
#endif


#include "convar.h"
#include "iplayeranimstate.h"


#if defined( CLIENT_DLL )
	class C_DODPlayer;
	#define CDODPlayer C_DODPlayer
#else
	class CDODPlayer;
#endif

enum PlayerAnimEvent_t
{
	PLAYERANIMEVENT_FIRE_GUN=0,
	PLAYERANIMEVENT_THROW_GRENADE,
	PLAYERANIMEVENT_ROLL_GRENADE,
	PLAYERANIMEVENT_JUMP,
	PLAYERANIMEVENT_RELOAD,
	PLAYERANIMEVENT_SECONDARY_ATTACK,
	PLAYERANIMEVENT_HANDSIGNAL,
	PLAYERANIMEVENT_PLANT_TNT,
	PLAYERANIMEVENT_DEFUSE_TNT,

	PLAYERANIMEVENT_HS_NONE,
	PLAYERANIMEVENT_CANCEL_GESTURES,	// cancel current gesture

	PLAYERANIMEVENT_COUNT
};

class IDODPlayerAnimState : virtual public IPlayerAnimState
{
public:
	// This is called by both the client and the server in the same way to trigger events for
	// players firing, jumping, throwing grenades, etc.
	virtual void DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 ) = 0;

	virtual void ShowDebugInfo( void ) = 0;
};


IDODPlayerAnimState* CreatePlayerAnimState( CDODPlayer *pPlayer );


// If this is set, then the game code needs to make sure to send player animation events
// to the local player if he's the one being watched.
extern ConVar cl_showanimstate;


#endif // DOD_PLAYERANIMSTATE_H

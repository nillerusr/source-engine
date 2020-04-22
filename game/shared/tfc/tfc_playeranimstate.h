//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TFC_PLAYERANIMSTATE_H
#define TFC_PLAYERANIMSTATE_H
#ifdef _WIN32
#pragma once
#endif


#include "convar.h"
#include "iplayeranimstate.h"


#if defined( CLIENT_DLL )
	class C_TFCPlayer;
	#define CTFCPlayer C_TFCPlayer
#else
	class CTFCPlayer;
#endif


enum PlayerAnimEvent_t
{
	PLAYERANIMEVENT_FIRE_GUN=0,
	PLAYERANIMEVENT_THROW_GRENADE,
	PLAYERANIMEVENT_JUMP,
	PLAYERANIMEVENT_RELOAD,
	PLAYERANIMEVENT_DIE,
	
	PLAYERANIMEVENT_COUNT
};


class ITFCPlayerAnimState : virtual public IPlayerAnimState
{
public:
	// This is called by both the client and the server in the same way to trigger events for
	// players firing, jumping, throwing grenades, etc.
	virtual void DoAnimationEvent( PlayerAnimEvent_t event, int nData ) = 0;
};


ITFCPlayerAnimState* CreatePlayerAnimState( CTFCPlayer *pPlayer );


// If this is set, then the game code needs to make sure to send player animation events
// to the local player if he's the one being watched.
extern ConVar cl_showanimstate;


#endif // TFC_PLAYERANIMSTATE_H

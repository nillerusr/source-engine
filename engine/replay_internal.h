//========= Copyright Valve Corporation, All rights reserved. ============//
//
//----------------------------------------------------------------------------------------

#ifndef REPLAY_INTERNAL_H
#define REPLAY_INTERNAL_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/ireplaysystem.h"
#include "replay/iserverreplaycontext.h"

#if !defined( DEDICATED )
#include "replay/iclientreplaycontext.h"
#include "replay/ireplayperformancemanager.h"
#include "replay/ireplayperformancecontroller.h"
#include "replay/ireplaymoviemanager.h"
#include "replay/ireplaymovierenderer.h"
#endif

//----------------------------------------------------------------------------------------

extern IReplaySystem				*g_pReplay;
extern IServerReplayContext			*g_pServerReplayContext;
extern CreateInterfaceFn			g_fnReplayFactory;

#if !defined( DEDICATED )
extern IClientReplayContext			*g_pClientReplayContext;
extern IReplayManager				*g_pReplayManager;
extern IReplayMovieManager			*g_pReplayMovieManager;
extern IReplayPerformanceManager	*g_pReplayPerformanceManager;
extern IReplayPerformanceController	*g_pReplayPerformanceController;
extern IReplayMovieRenderer			*g_pReplayMovieRenderer;
#endif
 
//----------------------------------------------------------------------------------------

#define	SETUP_CVAR_REF( _cvar )	ConVarRef _cvar( #_cvar );	Assert( _cvar.IsValid() );
#define GET_REPLAY_DBG_REF()	SETUP_CVAR_REF( replay_debug )

//----------------------------------------------------------------------------------------
// Purpose: Init/shutdown the replay system
//----------------------------------------------------------------------------------------
void ReplaySystem_Init( bool bDedicated );
void ReplaySystem_Shutdown();

//----------------------------------------------------------------------------------------
// Purpose: Check to see if replay is supported on the running game/platform
//----------------------------------------------------------------------------------------
bool Replay_IsSupportedModAndPlatform();

//----------------------------------------------------------------------------------------

#endif // REPLAY_INTERNAL_H

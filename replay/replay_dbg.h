//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef REPLAY_DBG_H
#define REPLAY_DBG_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/ienginereplay.h"
#include "convar.h"

//----------------------------------------------------------------------------------------

extern ConVar			replay_debug;
extern IEngineReplay	*g_pEngine;

//----------------------------------------------------------------------------------------

#define IF_REPLAY_DBG( x_ )			if ( replay_debug.GetBool() ) { x_; }
#define IF_REPLAY_DBGN( x_, n_ )	if ( replay_debug.GetInt() >= n_ ) { x_; }
#define IF_REPLAY_DBG2( x_ )		IF_REPLAY_DBGN( x_, 2 )
#define IF_REPLAY_DBG3( x_ )		IF_REPLAY_DBGN( x_, 3 )

#ifndef DEDICATED
#define DBG( x_ )		IF_REPLAY_DBG( Msg( "%f: " x_, g_pEngine->GetHostTime() ) )
#define DBGN( x_, n_ )	IF_REPLAY_DBGN( Msg( "%f: " x_, g_pEngine->GetHostTime() ), n_ )
#define DBG2( x_ )		DBGN( x_, 2 )
#define DBG3( x_ )		DBGN( x_, 3 )
#else
#define DBG( x_ )	
#define DBG2( x_ )	
#endif

//----------------------------------------------------------------------------------------

#endif // REPLAY_DBG_H

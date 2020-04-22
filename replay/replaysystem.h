//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef REPLAYDLL_H
#define REPLAYDLL_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/ireplaysystem.h"
#include "replay/ienginereplay.h"
#include "replay/iclientreplay.h"
#include "replay/iserverreplay.h"
#include "replay/ireplaydemoplayer.h"
#include "replay/ireplayserver.h"
#include "igameevents.h"
#include "engine/IEngineTrace.h"
#include "engine/idownloadsystem.h"
#include "icliententitylist.h"
#if !defined( DEDICATED )
#include "cl_replaycontext.h"
#include "engine/ivdebugoverlay.h"
#endif
#include "vgui/ILocalize.h"
#include "sv_replaycontext.h"
#include "convar.h"

//----------------------------------------------------------------------------------------

extern IReplaySystem				*g_pReplay;
extern IClientReplay				*g_pClient;
extern IServerReplay				*g_pServer;
extern IGameEventManager2			*g_pGameEventManager;
extern IEngineTrace					*g_pEngineTraceClient;
extern IReplayDemoPlayer			*g_pReplayDemoPlayer;
extern IEngineReplay				*g_pEngine;
extern vgui::ILocalize				*g_pVGuiLocalize;

#if !defined( DEDICATED )
extern IEngineClientReplay			*g_pEngineClient;
extern IVDebugOverlay				*g_pDebugOverlay;
extern IDownloadSystem				*g_pDownloadSystem;
#endif

//----------------------------------------------------------------------------------------

inline IReplayServer *ReplayServer()
{
	return g_pEngine->GetReplayServer();
}

inline IServer *ReplayServerAsIServer()
{
	return g_pEngine->GetReplayServerAsIServer();
}

//----------------------------------------------------------------------------------------

void		Replay_MsgBox( const char *pText );	// Display a message box
void		Replay_MsgBox( const wchar_t *pText );
const char	*Replay_GetBaseDir();	// Returns the replays base dir - eg, "/home/user/<...>/replays/"
const char *Replay_GetDownloadURLPath();
const char *Replay_GetDownloadURL();
void		Replay_CrackURL( const char *pURL, char *pBaseURLOut, char *pURLPathOut );
#ifndef DEDICATED
void		Replay_HudMsg( const char *pText, const char *pSound = NULL, bool bUrgent = false );
#endif

//----------------------------------------------------------------------------------------

#endif // REPLAYDLL_H

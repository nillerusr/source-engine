//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "replay/replaylib.h"
#include "replay/replayutils.h"
#include "replay/iclientreplaycontext.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

IClientReplayContext *g_pClientReplayContext = NULL;

//----------------------------------------------------------------------------------------

bool ReplayLib_Init( const char *pGameDir, IClientReplayContext *pClientReplayContext )
{
	Replay_SetGameDir( pGameDir );

	g_pClientReplayContext = pClientReplayContext;

	return true;
}

//----------------------------------------------------------------------------------------

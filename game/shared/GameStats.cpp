//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "GameStats.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static CBaseGameStats s_GameStats_Singleton;
CBaseGameStats *gamestats = &s_GameStats_Singleton; //start out pointing at the basic version which does nothing by default

void UpdatePerfStats( void ) { }
void SetGameStatsHandler( CGameStats *pGameStats ) { }
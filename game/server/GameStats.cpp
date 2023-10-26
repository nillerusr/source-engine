//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "GameStats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IUploadGameStats *gamestatsuploader;
extern ConVar skill;

static CBaseGameStats s_GameStats_Singleton;
CBaseGameStats *gamestats = &s_GameStats_Singleton; //start out pointing at the basic version which does nothing by default

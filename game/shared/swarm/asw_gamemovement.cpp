//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Special handling for hl2 usable ladders
//
//=============================================================================//
#include "cbase.h"
#include "coordsize.h"
#include "movevars_shared.h"
#include "in_buttons.h"
#include "utlrbtree.h"
#include "gamemovement.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Expose standard movement interface.
static CGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameMovement, IGameMovement,INTERFACENAME_GAMEMOVEMENT, g_GameMovement );
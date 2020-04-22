//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// nav_edit.cpp
// Implementation of Navigation Mesh edit mode
// Author: Michael Booth, 2003-2004

#include "cbase.h"
#include "nav_mesh.h"
#include "cs_nav_pathfind.h"
#include "cs_nav_node.h"
#include "nav_colors.h"
#include "Color.h"
#include "tier0/vprof.h"
#include "collisionutils.h"

ConVar nav_show_area_info( "nav_show_area_info", "0.5", FCVAR_GAMEDLL, "Duration in seconds to show nav area ID and attributes while editing" );
ConVar nav_snap_to_grid( "nav_snap_to_grid", "0", FCVAR_GAMEDLL, "Snap to the nav generation grid when creating new nav areas" );
ConVar nav_create_place_on_ground( "nav_create_place_on_ground", "0", FCVAR_GAMEDLL, "If true, nav areas will be placed flush with the ground when created by hand." );

#if DEBUG_NAV_NODES
extern ConVar nav_show_nodes;
#endif // DEBUG_NAV_NODES


//--------------------------------------------------------------------------------------------------------------
void EditNav_Precache(void *pUser)
{
	CBaseEntity::PrecacheScriptSound( "Bot.EditSwitchOn" );
	CBaseEntity::PrecacheScriptSound( "EDIT_TOGGLE_PLACE_MODE" );
	CBaseEntity::PrecacheScriptSound( "Bot.EditSwitchOff" );
	CBaseEntity::PrecacheScriptSound( "EDIT_PLACE_PICK" );
	CBaseEntity::PrecacheScriptSound( "EDIT_DELETE" );
	CBaseEntity::PrecacheScriptSound( "EDIT.ToggleAttribute" );
	CBaseEntity::PrecacheScriptSound( "EDIT_SPLIT.MarkedArea" );
	CBaseEntity::PrecacheScriptSound( "EDIT_SPLIT.NoMarkedArea" );
	CBaseEntity::PrecacheScriptSound( "EDIT_MERGE.Enable" );
	CBaseEntity::PrecacheScriptSound( "EDIT_MERGE.Disable" );
	CBaseEntity::PrecacheScriptSound( "EDIT_MARK.Enable" );
	CBaseEntity::PrecacheScriptSound( "EDIT_MARK.Disable" );
	CBaseEntity::PrecacheScriptSound( "EDIT_MARK_UNNAMED.Enable" );
	CBaseEntity::PrecacheScriptSound( "EDIT_MARK_UNNAMED.NoMarkedArea" );
	CBaseEntity::PrecacheScriptSound( "EDIT_MARK_UNNAMED.MarkedArea" );
	CBaseEntity::PrecacheScriptSound( "EDIT_CONNECT.AllDirections" );
	CBaseEntity::PrecacheScriptSound( "EDIT_CONNECT.Added" );
	CBaseEntity::PrecacheScriptSound( "EDIT_DISCONNECT.MarkedArea" );
	CBaseEntity::PrecacheScriptSound( "EDIT_DISCONNECT.NoMarkedArea" );
	CBaseEntity::PrecacheScriptSound( "EDIT_SPLICE.MarkedArea" );
	CBaseEntity::PrecacheScriptSound( "EDIT_SPLICE.NoMarkedArea" );
	CBaseEntity::PrecacheScriptSound( "EDIT_SELECT_CORNER.MarkedArea" );
	CBaseEntity::PrecacheScriptSound( "EDIT_SELECT_CORNER.NoMarkedArea" );
	CBaseEntity::PrecacheScriptSound( "EDIT_MOVE_CORNER.MarkedArea" );
	CBaseEntity::PrecacheScriptSound( "EDIT_MOVE_CORNER.NoMarkedArea" );
	CBaseEntity::PrecacheScriptSound( "EDIT_BEGIN_AREA.Creating" );
	CBaseEntity::PrecacheScriptSound( "EDIT_BEGIN_AREA.NotCreating" );
	CBaseEntity::PrecacheScriptSound( "EDIT_END_AREA.Creating" );
	CBaseEntity::PrecacheScriptSound( "EDIT_END_AREA.NotCreating" );
	CBaseEntity::PrecacheScriptSound( "EDIT_WARP_TO_MARK" );
}

#ifdef CSTRIKE_DLL
PRECACHE_REGISTER_FN( EditNav_Precache );
#endif



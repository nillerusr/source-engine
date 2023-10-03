//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: System to generate events as specified entities become visible to players.
//
// $NoKeywords: $
//=====================================================================================//

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// A callback which will automatically be used instead of generating the generic
// 'on_entity_visible' game event. Often used so that the entity can generate
// its own, more unique game event. 
// Return TRUE if you want the generic event generated after the callback
// Return FALSE if you do not want the generic event generated
typedef bool (*VisibilityMonitorCallback)( CBaseEntity *pVisibleEntity, CBasePlayer *pViewingPlayer );

// This callback tells us which object is being seen, and by which client. The callback returns 'true'
// to allow visibility monitor to acknowledge the event, or false to suppress the event.
typedef bool (*VisibilityMonitorEvaluator)( CBaseEntity *pVisibleEntity, CBasePlayer *pViewingPlayer );

extern void VisibilityMonitor_AddEntity( CBaseEntity *pEntity, float flMinDist, VisibilityMonitorCallback pfnCallback, VisibilityMonitorEvaluator pfnEvaluator );
extern void VisibilityMonitor_RemoveEntity( CBaseEntity *pEntity );

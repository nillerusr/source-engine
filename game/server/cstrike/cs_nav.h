//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// nav.h
// Data structures and constants for the Navigation Mesh system
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

#ifndef _CS_NAV_H_
#define _CS_NAV_H_

#include "nav.h"

/**
 * Below are several constants used by the navigation system.
 * @todo Move these into TheNavMesh singleton.
 */
const float BotRadius = 10.0f;				///< circular extent that contains bot

class CNavArea;
class CSNavNode;

#if 0
//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if given entity can be ignored when moving
 */
#define WALK_THRU_DOORS				0x01
#define WALK_THRU_BREAKABLES		0x02
#define WALK_THRU_TOGGLE_BRUSHES	0x04
#define WALK_THRU_EVERYTHING		(WALK_THRU_DOORS | WALK_THRU_BREAKABLES | WALK_THRU_TOGGLE_BRUSHES)
inline bool IsEntityWalkable( CBaseEntity *entity, unsigned int flags )
{
	if (FClassnameIs( entity, "worldspawn" ))
		return false;

	if (FClassnameIs( entity, "player" ))
		return false;

	// if we hit a door, assume its walkable because it will open when we touch it
	if (FClassnameIs( entity, "prop_door*" ) || FClassnameIs( entity, "func_door*" ))
		return (flags & WALK_THRU_DOORS) ? true : false;

	// if we hit a clip brush, ignore it if it is not BRUSHSOLID_ALWAYS
	if (FClassnameIs( entity, "func_brush" ))
	{
		CFuncBrush *brush = (CFuncBrush *)entity;
		switch ( brush->m_iSolidity )
		{
		case CFuncBrush::BRUSHSOLID_ALWAYS:
			return false;
		case CFuncBrush::BRUSHSOLID_NEVER:
			return true;
		case CFuncBrush::BRUSHSOLID_TOGGLE:
			return (flags & WALK_THRU_TOGGLE_BRUSHES) ? true : false;
		}
	}

	// if we hit a breakable object, assume its walkable because we will shoot it when we touch it
	if (FClassnameIs( entity, "func_breakable" ) && entity->GetHealth() && entity->m_takedamage == DAMAGE_YES)
		return (flags & WALK_THRU_BREAKABLES) ? true : false;

	if (FClassnameIs( entity, "func_breakable_surf" ) && entity->m_takedamage == DAMAGE_YES)
		return (flags & WALK_THRU_BREAKABLES) ? true : false;

	return false;
}
#endif

#endif // _CS_NAV_H_

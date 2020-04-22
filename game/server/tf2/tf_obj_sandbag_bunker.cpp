//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A stationary sandbag bunker that players can take cover in.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_obj.h"
#include "tf_team.h"
#include "engine/IEngineSound.h"
#include "tf_obj_sandbag_bunker.h"

#define SANDBAGBUNKER_MINS			Vector(-60, -60, 0)
#define SANDBAGBUNKER_MAXS			Vector( 60,  60, 50)
#define SANDBAGBUNKER_MODEL			"models/objects/obj_sandbag_bunker.mdl"


IMPLEMENT_SERVERCLASS_ST(CObjectSandbagBunker, DT_ObjectSandbagBunker)
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS(obj_sandbag_bunker, CObjectSandbagBunker);
PRECACHE_REGISTER(obj_sandbag_bunker);

// CVars
ConVar	obj_sandbag_bunker_health( "obj_sandbag_bunker_health","100", FCVAR_NONE, "Sandbag bunker health" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectSandbagBunker::CObjectSandbagBunker( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSandbagBunker::Spawn( void )
{
	Precache();
	SetModel( SANDBAGBUNKER_MODEL );
	SetSolid( SOLID_BBOX );

	UTIL_SetSize(this, SANDBAGBUNKER_MINS, SANDBAGBUNKER_MAXS);
	m_takedamage = DAMAGE_YES;
	m_iHealth = obj_sandbag_bunker_health.GetInt();

	m_fObjectFlags |= OF_DOESNT_NEED_POWER;
	SetType( OBJ_SANDBAG_BUNKER );

	SetSolid( SOLID_VPHYSICS );
	VPhysicsInitStatic();

	BaseClass::Spawn();

	SetCollisionGroup( COLLISION_GROUP_VEHICLE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSandbagBunker::Precache( void )
{
	PrecacheModel( SANDBAGBUNKER_MODEL );
}

//-----------------------------------------------------------------------------
// Purpose: Gets info about the control panels
//-----------------------------------------------------------------------------
void CObjectSandbagBunker::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "screen_basic_with_disable";
}
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Vehicle blockers
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_team.h"
#include "tf_obj.h"
#include "tf_obj_dragonsteeth.h"

// ------------------------------------------------------------------------ //
// Dragon's teeth defines
#define DRAGONSTEETH_MINS				Vector(-20, -20, 0)
#define DRAGONSTEETH_MAXS				Vector( 20,  20, 35)
#define DRAGONSTEETH_MODEL				"models/objects/human_obj_dragonsteeth.mdl"
#define DRAGONSTEETH_ASSEMBLING_MODEL	"models/objects/human_obj_dragonsteeth_build.mdl"

IMPLEMENT_SERVERCLASS_ST( CObjectDragonsTeeth, DT_ObjectDragonsTeeth )
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS(obj_dragonsteeth, CObjectDragonsTeeth);
PRECACHE_REGISTER(obj_dragonsteeth);

ConVar	obj_dragonsteeth_health( "obj_dragonsteeth_health","200", FCVAR_NONE, "Dragon's Teeth health" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectDragonsTeeth::CObjectDragonsTeeth()
{
	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectDragonsTeeth::Spawn( void )
{
	SetModel( DRAGONSTEETH_MODEL );
	SetSolid( SOLID_BBOX );
	UTIL_SetSize(this, DRAGONSTEETH_MINS, DRAGONSTEETH_MAXS);

	m_iHealth = obj_dragonsteeth_health.GetInt();
	m_fObjectFlags |= OF_DOESNT_NEED_POWER | OF_SUPPRESS_APPEAR_ON_MINIMAP | OF_ALLOW_REPEAT_PLACEMENT | OF_ALIGN_TO_GROUND;
	SetType( OBJ_DRAGONSTEETH );

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectDragonsTeeth::Precache()
{
	BaseClass::Precache();
	PrecacheModel( DRAGONSTEETH_MODEL );
	PrecacheModel( DRAGONSTEETH_ASSEMBLING_MODEL );
}

//-----------------------------------------------------------------------------
// Purpose: Start building the object
//-----------------------------------------------------------------------------
bool CObjectDragonsTeeth::StartBuilding( CBaseEntity *pBuilder )
{
	if ( BaseClass::StartBuilding( pBuilder ) )
	{
		// Dragonsteeth randomise their Y before building
		QAngle vecAngles = GetAbsAngles();
		vecAngles[YAW] = RandomFloat(0,360);
		SetAbsAngles( vecAngles );

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : act - 
//-----------------------------------------------------------------------------
void CObjectDragonsTeeth::OnActivityChanged( Activity act )
{
	BaseClass::OnActivityChanged( act );

	switch ( act )
	{
	case ACT_OBJ_ASSEMBLING:
		SetModel( DRAGONSTEETH_ASSEMBLING_MODEL );
		break;
	default:
		SetModel( DRAGONSTEETH_MODEL );
		break;
	}
}

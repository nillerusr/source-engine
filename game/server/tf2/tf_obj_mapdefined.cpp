//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_team.h"
#include "tf_gamerules.h"
#include "tf_obj.h"
#include "tf_obj_mapdefined.h"
#include "ndebugoverlay.h"

extern ConVar obj_damage_factor;

// Map defined object spawnflags
#define SF_MAPDEFOBJ_SUPPRESS_MINIMAP		0x0001
#define SF_MAPDEFOBJ_SUPPRESS_ATTACKNOTIFY	0x0002
#define SF_MAPDEFOBJ_DOESNT_NEED_POWER		0x0004

BEGIN_DATADESC( CObjectMapDefined )
	// keys 
	DEFINE_KEYFIELD_NOT_SAVED( m_iszCustomName , FIELD_STRING, "CustomName" ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CObjectMapDefined, DT_ObjectMapDefined)
	SendPropString( SENDINFO( m_szCustomName ) ),
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS(obj_mapdefined, CObjectMapDefined);
LINK_ENTITY_TO_CLASS(func_obj_mapdefined, CObjectMapDefined);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectMapDefined::CObjectMapDefined()
{
	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectMapDefined::Spawn()
{
	// Get the model from the map
	char *szModel = (char *)STRING( GetModelName() );
	if (!szModel || !*szModel)
	{
		Warning( "obj_mapdefined at %.0f %.0f %0.f missing modelname\n", GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
		UTIL_Remove( this );
		return;
	}

	memset( m_szCustomName.GetForModify(), 0, sizeof(m_szCustomName) );
	if ( m_iszCustomName != NULL_STRING )
	{
		Q_strncpy( m_szCustomName.GetForModify(), STRING(m_iszCustomName), sizeof(m_szCustomName) );
	}

	Precache();

	// Get the bounding box from the model
	if ( szModel[0] != '*' )
	{
		SetModel( szModel );
		Vector vecMins, vecMaxs;
		const model_t *pModel = GetModel();
		modelinfo->GetModelBounds( pModel, vecMins, vecMaxs );
		UTIL_SetSize(this, vecMins, vecMaxs );
	}
	else
	{
		// Don't call our internal setmodel to avoid the error
		UTIL_SetModel( this, szModel );

		// No control panels / power on map geometry objects
		m_fObjectFlags |= OF_DOESNT_HAVE_A_MODEL;
		m_fObjectFlags |= OF_DOESNT_NEED_POWER;
	}

	SetSolid( SOLID_VPHYSICS );
	SetType( OBJ_MAPDEFINED );

	// Setup object flags
	if ( HasSpawnFlags( SF_MAPDEFOBJ_SUPPRESS_MINIMAP ) )
	{
		m_fObjectFlags |= OF_SUPPRESS_APPEAR_ON_MINIMAP;
	}
	if ( HasSpawnFlags( SF_MAPDEFOBJ_SUPPRESS_ATTACKNOTIFY ) )
	{
		m_fObjectFlags |= OF_SUPPRESS_NOTIFY_UNDER_ATTACK;
	}
	if ( HasSpawnFlags( SF_MAPDEFOBJ_DOESNT_NEED_POWER ) )
	{
		m_fObjectFlags |= OF_DOESNT_NEED_POWER;
	}

	// If I don't have health, make me invulnerable
	if ( !m_iHealth )
	{
		m_bInvulnerable = true;
	}

	BaseClass::Spawn();

	// Override base object settings
	SetCollisionGroup( COLLISION_GROUP_NONE );

	FinishedBuilding();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectMapDefined::Precache()
{
	PrecacheModel( STRING( GetModelName() ) );
	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CObjectMapDefined::OnTakeDamage( const CTakeDamageInfo &info )
{
	CTakeDamageInfo childInfo = info;

	// Hack around the damage factor applied to objects
	if ( obj_damage_factor.GetFloat() )
	{
		float flDamage = info.GetDamage() * (1 / obj_damage_factor.GetFloat());
		childInfo.SetDamage( flDamage );
	}

	return BaseClass::OnTakeDamage( childInfo );
}
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
#include "tf_obj_empgenerator.h"
#include "ndebugoverlay.h"

BEGIN_DATADESC( CObjectEMPGenerator )

	DEFINE_THINKFUNC( EMPThink ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CObjectEMPGenerator, DT_ObjectEMPGenerator)
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS(obj_empgenerator, CObjectEMPGenerator);
PRECACHE_REGISTER(obj_empgenerator);

ConVar	obj_empgenerator_health( "obj_empgenerator_health","100", FCVAR_NONE, "EMP Generator health" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectEMPGenerator::CObjectEMPGenerator()
{
	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectEMPGenerator::Spawn()
{
	BaseClass::Spawn();

	Precache();
	SetModel( EMPGENERATOR_MODEL );
	SetSolid( SOLID_BBOX );
	SetCollisionGroup( TFCOLLISION_GROUP_COMBATOBJECT );

	UTIL_SetSize(this, EMPGENERATOR_MINS, EMPGENERATOR_MAXS);
	m_takedamage = DAMAGE_YES;
	m_iHealth = obj_empgenerator_health.GetInt();

	SetThink( EMPThink );
	SetNextThink( gpGlobals->curtime + EMPGENERATOR_RATE );
	m_flExpiresAt = gpGlobals->curtime + EMPGENERATOR_LIFETIME;

	SetType( OBJ_EMPGENERATOR );
	m_fObjectFlags |= OF_SUPPRESS_NOTIFY_UNDER_ATTACK | OF_SUPPRESS_TECH_ANALYZER | 
		OF_DONT_AUTO_REPAIR | OF_DONT_PREVENT_BUILD_NEAR_OBJ | OF_DOESNT_NEED_POWER;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectEMPGenerator::Precache()
{
	PrecacheModel( EMPGENERATOR_MODEL );
}

//-----------------------------------------------------------------------------
// Purpose: Look for enemies to EMP
//-----------------------------------------------------------------------------
void CObjectEMPGenerator::EMPThink( void )
{
	if ( !GetTeam() )
		return;

	// Time to die?
	if ( gpGlobals->curtime > m_flExpiresAt )
	{
		UTIL_Remove( this );
		return;
	}

	// Look for nearby enemies to EMP
	CBaseEntity *pEntity = NULL;
	for ( CEntitySphereQuery sphere( GetAbsOrigin(), EMPGENERATOR_RADIUS ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		if ( InSameTeam( pEntity ) )
			continue;
		if ( !pEntity->CanBePoweredUp() )
			continue;

		pEntity->AttemptToPowerup( POWERUP_EMP, EMPGENERATOR_EMP_TIME );
	}
}

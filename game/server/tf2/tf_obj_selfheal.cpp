//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Upgrade that heals the object over time
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_team.h"
#include "tf_gamerules.h"
#include "tf_obj.h"
#include "tf_obj_selfheal.h"
#include "ndebugoverlay.h"

// ------------------------------------------------------------------------ //
// Self Heal defines
#define SELFHEAL_MINS				Vector(-10, -10, 0)
#define SELFHEAL_MAXS				Vector( 10,  10, 10)
#define SELFHEAL_MODEL				"models/objects/obj_selfheal.mdl"

BEGIN_DATADESC( CObjectSelfHeal )

	DEFINE_THINKFUNC( SelfHealThink ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CObjectSelfHeal, DT_ObjectSelfHeal)
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS(obj_selfheal, CObjectSelfHeal);
PRECACHE_REGISTER(obj_selfheal);

ConVar	obj_selfheal_health( "obj_selfheal_health","100", FCVAR_NONE, "Self-Heal health" );
ConVar	obj_selfheal_rate( "obj_selfheal_rate","1.0", FCVAR_NONE, "Rate at which the Self-Heal object repairs it's parent" );
ConVar	obj_selfheal_amount( "obj_selfheal_amount","3", FCVAR_NONE, "Amount of health healed by a Self-Heal object per tick" );

#define SELFHEAL_THINK_CONTEXT		"SelfHealThink"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectSelfHeal::CObjectSelfHeal()
{
	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSelfHeal::Spawn()
{
	Precache();
	SetModel( SELFHEAL_MODEL );
	SetCollisionGroup( TFCOLLISION_GROUP_COMBATOBJECT );

	UTIL_SetSize(this, SELFHEAL_MINS, SELFHEAL_MAXS);
	m_takedamage = DAMAGE_YES;
	m_iHealth = obj_selfheal_health.GetInt();

	SetType( OBJ_SELFHEAL );
	m_fObjectFlags |= OF_SUPPRESS_NOTIFY_UNDER_ATTACK | OF_SUPPRESS_TECH_ANALYZER | 
		OF_DONT_AUTO_REPAIR | OF_MUST_BE_BUILT_ON_ATTACHMENT;

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSelfHeal::Precache()
{
	PrecacheModel( SELFHEAL_MODEL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSelfHeal::FinishedBuilding( void )
{
	BaseClass::FinishedBuilding();

	SetContextThink( SelfHealThink, gpGlobals->curtime + obj_selfheal_rate.GetFloat(), SELFHEAL_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose: Heal the object I'm attached to
//-----------------------------------------------------------------------------
void CObjectSelfHeal::SelfHealThink( void )
{
	if ( !GetTeam() )
		return;

	CBaseObject *pObject = GetParentObject();
	if ( !pObject )
	{
		Killed();
		return;
	}

	SetNextThink( gpGlobals->curtime + obj_selfheal_rate.GetFloat(), SELFHEAL_THINK_CONTEXT );

	// Don't heal if we've been EMPed
	if ( HasPowerup( POWERUP_EMP ) )
		return;

	// Don't bring objects back from the dead
	if ( !pObject->IsAlive() || pObject->IsDying() )
		return;

	// Repair our parent if it's hurt
	pObject->Repair( obj_selfheal_amount.GetFloat() );
}

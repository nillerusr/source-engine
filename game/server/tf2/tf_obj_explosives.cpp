//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Upgrade that explodes when it's object dies, injuring nearby enemies
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_team.h"
#include "tf_gamerules.h"
#include "tf_obj.h"
#include "ndebugoverlay.h"
#include "tf_obj_baseupgrade_shared.h"
#include "hierarchy.h"

// ------------------------------------------------------------------------ //
// Explosives defines
#define EXPLOSIVE_MINS				Vector(-10, -10, 0)
#define EXPLOSIVE_MAXS				Vector( 10,  10, 10)
#define EXPLOSIVE_MODEL				"models/objects/obj_explosives.mdl"

// ------------------------------------------------------------------------ //
// Explosives upgrade
// ------------------------------------------------------------------------ //
class CObjectExplosives : public CBaseObjectUpgrade
{
	DECLARE_CLASS( CObjectExplosives, CBaseObjectUpgrade );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CObjectExplosives();

	virtual void	Spawn();
	virtual void	Precache();
	virtual void	Killed( void );

	// Explosivo
	void			ExplodeThink( void );
};

BEGIN_DATADESC( CObjectExplosives )

	DEFINE_THINKFUNC( ExplodeThink ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CObjectExplosives, DT_ObjectExplosives)
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS(obj_explosives, CObjectExplosives);
PRECACHE_REGISTER(obj_explosives);

ConVar	obj_explosives_health( "obj_explosives_health","1", FCVAR_NONE, "Explosives health" );
ConVar	obj_explosives_damage( "obj_explosives_damage","100", FCVAR_NONE, "Explosives damage" );
ConVar	obj_explosives_radius( "obj_explosives_radius","256", FCVAR_NONE, "Explosives damage" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectExplosives::CObjectExplosives()
{
	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectExplosives::Spawn()
{
	Precache();
	SetModel( EXPLOSIVE_MODEL );
	SetCollisionGroup( TFCOLLISION_GROUP_COMBATOBJECT );

	UTIL_SetSize(this, EXPLOSIVE_MINS, EXPLOSIVE_MAXS);
	m_takedamage = DAMAGE_YES;
	m_iHealth = obj_explosives_health.GetInt();

	SetType( OBJ_EXPLOSIVES );
	m_fObjectFlags |= OF_SUPPRESS_NOTIFY_UNDER_ATTACK | OF_SUPPRESS_TECH_ANALYZER | OF_DONT_AUTO_REPAIR | OF_MUST_BE_BUILT_ON_ATTACHMENT;

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectExplosives::Precache()
{
	PrecacheModel( EXPLOSIVE_MODEL );
}

//-----------------------------------------------------------------------------
// Purpose: Explosivo!
//-----------------------------------------------------------------------------
void CObjectExplosives::Killed( void )
{
	// Tell 'em I'm dying now
	m_bDying = true;

	// Remove myself from the entity I was built on so it can die
	DetachObjectFromObject();

	// Delay the explosion & death so that it's not blocked by the entity we were built on
	SetThink( ExplodeThink );
	SetNextThink( gpGlobals->curtime + 0.3 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectExplosives::ExplodeThink( void )
{
	// Do radius damage
	RadiusDamage( CTakeDamageInfo( this, GetBuilder(), obj_explosives_damage.GetFloat(), DMG_BLAST ), GetAbsOrigin(), obj_explosives_radius.GetFloat(), CLASS_NONE, NULL );

	// Kill myself
	BaseClass::Killed();
}
#include "cbase.h"
#include "asw_weapon_fist.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "asw_marine_resource.h"
#endif
#include "asw_gamerules.h"
#include "asw_marine_skills.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Fist, DT_ASW_Weapon_Fist )

BEGIN_NETWORK_TABLE( CASW_Weapon_Fist, DT_ASW_Weapon_Fist )
#ifdef CLIENT_DLL
#else
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Fist )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_fist, CASW_Weapon_Fist );
PRECACHE_WEAPON_REGISTER( asw_weapon_fist );

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore (really?)
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Fist )
	
END_DATADESC()

ConVar asw_fist_ragdoll_chance( "asw_fist_ragdoll_chance", "0.4f" );
#endif /* not client */

ConVar asw_fist_passive_damage_scale( "asw_fist_passive_damage_scale", "2.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "Damage scale applied from charged fist passive item" );


CASW_Weapon_Fist::CASW_Weapon_Fist()
{

}


CASW_Weapon_Fist::~CASW_Weapon_Fist()
{

}

void CASW_Weapon_Fist::PrimaryAttack( void )
{
	// passive, do nothing
}
#include "cbase.h"
#include "asw_weapon_normal_armor.h"
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

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Normal_Armor, DT_ASW_Weapon_Normal_Armor )

BEGIN_NETWORK_TABLE( CASW_Weapon_Normal_Armor, DT_ASW_Weapon_Normal_Armor )
#ifdef CLIENT_DLL
#else
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Normal_Armor )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_normal_armor, CASW_Weapon_Normal_Armor );
PRECACHE_WEAPON_REGISTER( asw_weapon_normal_armor );

#ifndef CLIENT_DLL

ConVar asw_marine_passive_armor_scale( "asw_marine_passive_armor_scale", "0.8", FCVAR_CHEAT, "'normal' armor will scale damage taken by this much" );

//---------------------------------------------------------
// Save/Restore (really?)
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Normal_Armor )
	
END_DATADESC()

#endif /* not client */

CASW_Weapon_Normal_Armor::CASW_Weapon_Normal_Armor()
{

}


CASW_Weapon_Normal_Armor::~CASW_Weapon_Normal_Armor()
{

}

void CASW_Weapon_Normal_Armor::PrimaryAttack( void )
{
	// passive, do nothing
}

/*
void CASW_Weapon_Normal_Armor::Precache()
{
	BaseClass::Precache();
}
*/

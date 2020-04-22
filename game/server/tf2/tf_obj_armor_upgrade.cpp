//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "tf_obj_armor_upgrade.h"


#define AERIAL_SENTRY_STATION_MODEL			"models/objects/obj_aerial_sentry_station.mdl"


LINK_ENTITY_TO_CLASS( obj_armor_upgrade, CArmorUpgrade );


IMPLEMENT_SERVERCLASS_ST( CArmorUpgrade, DT_ArmorUpgrade )
END_SEND_TABLE()


CArmorUpgrade::CArmorUpgrade()
{
	UseClientSideAnimation();
}


void CArmorUpgrade::Spawn()
{
	SetModel( AERIAL_SENTRY_STATION_MODEL );
	SetType( OBJ_ARMOR_UPGRADE );
}

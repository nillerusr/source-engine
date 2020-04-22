//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//


#include "cbase.h"
#include "player.h"
#include "items.h"
#include "gamerules.h"
#include "hl1_items.h"


void CHL1Item::Spawn( void )
{
	SetMoveType( MOVETYPE_FLYGRAVITY );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE | FSOLID_TRIGGER );
	CollisionProp()->UseTriggerBounds( true, 24.0f );
	
	SetCollisionGroup( COLLISION_GROUP_DEBRIS );

	SetTouch( &CItem::ItemTouch );

#ifdef HL1_DLL
    if ( g_pGameRules->IsMultiplayer() )
        AddEffects( EF_NOSHADOW );
#endif


}


void CHL1Item::Activate( void )
{
	BaseClass::Activate();

	if ( UTIL_DropToFloor( this, MASK_SOLID ) == 0 )
	{
		Warning( "Item %s fell out of level at %f,%f,%f\n", GetClassname(), GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z);
		UTIL_Remove( this );
		return;
	}
}

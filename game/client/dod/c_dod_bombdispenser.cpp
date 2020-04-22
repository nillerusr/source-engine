//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "c_dod_player.h"
#include "dodoverview.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class C_DODBombDispenserMapIcon : public C_BaseEntity
{
	DECLARE_CLASS( C_DODBombDispenserMapIcon, C_BaseEntity );

	DECLARE_NETWORKCLASS();

public:
	virtual ~C_DODBombDispenserMapIcon();
	virtual void OnDataChanged( DataUpdateType_t type );
};

IMPLEMENT_NETWORKCLASS_ALIASED( DODBombDispenserMapIcon, DT_DODBombDispenserMapIcon )

BEGIN_NETWORK_TABLE(C_DODBombDispenserMapIcon, DT_DODBombDispenserMapIcon )
END_NETWORK_TABLE()

C_DODBombDispenserMapIcon::~C_DODBombDispenserMapIcon( void )
{
	GetDODOverview()->RemoveObjectByIndex( entindex() );
}

void C_DODBombDispenserMapIcon::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		GetDODOverview()->AddObject( "sprites/obj_icons/icon_bomb_dispenser", entindex(), -1 );
	}
}
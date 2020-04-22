//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "triggers.h"

class CHostageRescueZone : public CBaseTrigger
{
public:
	DECLARE_CLASS( CHostageRescueZone, CBaseTrigger );
	DECLARE_DATADESC();

	void CHostageRescue();
	void Spawn();

	void HostageRescueTouch( CBaseEntity* pOther );
};


LINK_ENTITY_TO_CLASS( func_hostage_rescue, CHostageRescueZone );


BEGIN_DATADESC( CHostageRescueZone )

	//Functions
	DEFINE_FUNCTION( HostageRescueTouch ),

END_DATADESC()


void CHostageRescueZone::Spawn()
{
	InitTrigger();
	SetTouch( &CHostageRescueZone::HostageRescueTouch );
}

void CHostageRescueZone::HostageRescueTouch( CBaseEntity *pOther )
{
	variant_t emptyVariant;
	pOther->AcceptInput( "OnRescueZoneTouch", NULL, NULL, emptyVariant, 0 );
}


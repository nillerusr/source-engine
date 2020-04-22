//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "dod_location.h"

#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( dod_location, CDODLocation );

void CDODLocation::Spawn( void )
{
	BaseClass::Spawn();
}

bool CDODLocation::KeyValue( const char *szKeyName, const char *szValue )
{	
	if (FStrEq(szKeyName, "location_name"))	//name of this location
	{
		Q_strncpy( m_szLocationName, szValue, sizeof(m_szLocationName) );
	}
	else
		return CBaseEntity::KeyValue( szKeyName, szValue );

	return true;
}
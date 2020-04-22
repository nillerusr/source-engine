//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include <KeyValues.h>
#include "tfc_weapon_parse.h"


FileWeaponInfo_t* CreateWeaponInfo()
{
	return new CTFCWeaponInfo;
}


CTFCWeaponInfo::CTFCWeaponInfo()
{
}


void CTFCWeaponInfo::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	BaseClass::Parse( pKeyValuesData, szWeaponName );
}



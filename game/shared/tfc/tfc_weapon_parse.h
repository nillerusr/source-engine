//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TFC_WEAPON_PARSE_H
#define TFC_WEAPON_PARSE_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_parse.h"
#include "networkvar.h"


//--------------------------------------------------------------------------------------------------------
class CTFCWeaponInfo : public FileWeaponInfo_t
{
public:
	DECLARE_CLASS_GAMEROOT( CTFCWeaponInfo, FileWeaponInfo_t );
	
	CTFCWeaponInfo();
	
	virtual void Parse( ::KeyValues *pKeyValuesData, const char *szWeaponName );
};


#endif // TFC_WEAPON_PARSE_H

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_basecombatweapon.h"
#include "tf_obj.h"
#include "tf_obj_empgenerator.h"
#include "weapon_basecombatobject.h"

//-----------------------------------------------------------------------------
// Purpose: Combat object weapon for the EMP Generator
//-----------------------------------------------------------------------------
class CWeaponObjEMPGenerator : public CWeaponBaseCombatObject
{
	DECLARE_CLASS( CWeaponObjEMPGenerator, CWeaponBaseCombatObject );
public:
	CWeaponObjEMPGenerator( void );

	DECLARE_SERVERCLASS();
};

IMPLEMENT_SERVERCLASS_ST( CWeaponObjEMPGenerator, DT_WeaponObjEMPGenerator )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_obj_empgenerator, CWeaponObjEMPGenerator );
PRECACHE_WEAPON_REGISTER(weapon_obj_empgenerator);

CWeaponObjEMPGenerator::CWeaponObjEMPGenerator( void )
{
	m_szObjectName = "obj_empgenerator";
	m_vecBuildMins = EMPGENERATOR_MINS - Vector( 4,4,4 );
	m_vecBuildMaxs = EMPGENERATOR_MAXS + Vector( 4,4,4 );
}
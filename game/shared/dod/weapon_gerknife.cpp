//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbasemelee.h"

#if defined( CLIENT_DLL )

	#define CWeaponGerKnife C_WeaponGerKnife

#endif


class CWeaponGerKnife : public CWeaponDODBaseMelee
{
public:
	DECLARE_CLASS( CWeaponGerKnife, CWeaponDODBaseMelee );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();
	
	CWeaponGerKnife()  {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_GERKNIFE; }

private:
	CWeaponGerKnife( const CWeaponGerKnife & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponGerKnife, DT_WeaponGerKnife )

BEGIN_NETWORK_TABLE( CWeaponGerKnife, DT_WeaponGerKnife )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponGerKnife )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_gerknife, CWeaponGerKnife );
PRECACHE_WEAPON_REGISTER( weapon_gerknife );

acttable_t CWeaponGerKnife::m_acttable[] = 
{
	{ ACT_IDLE,								ACT_DOD_STAND_AIM_KNIFE,				false },
	{ ACT_CROUCHIDLE,						ACT_DOD_CROUCH_AIM_KNIFE,				false },
	{ ACT_RUN_CROUCH,						ACT_DOD_CROUCHWALK_AIM_KNIFE,			false },
	{ ACT_WALK,								ACT_DOD_WALK_AIM_KNIFE,					false },
	{ ACT_RUN,								ACT_DOD_RUN_AIM_KNIFE,					false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_KNIFE,				false },
	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_KNIFE,			false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_KNIFE,		false },
};

IMPLEMENT_ACTTABLE( CWeaponGerKnife );
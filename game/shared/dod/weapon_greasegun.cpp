//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodfullauto.h"


#if defined( CLIENT_DLL )

	#define CWeaponGreaseGun C_WeaponGreaseGun

#endif


class CWeaponGreaseGun : public CDODFullAutoWeapon
{
public:
	DECLARE_CLASS( CWeaponGreaseGun, CDODFullAutoWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();
	
	CWeaponGreaseGun()  {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_GREASEGUN; }

private:
	CWeaponGreaseGun( const CWeaponGreaseGun & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponGreaseGun, DT_WeaponGreaseGun )

BEGIN_NETWORK_TABLE( CWeaponGreaseGun, DT_WeaponGreaseGun )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponGreaseGun )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_greasegun, CWeaponGreaseGun );
PRECACHE_WEAPON_REGISTER( weapon_greasegun );

acttable_t CWeaponGreaseGun::m_acttable[] = 
{
	// Aim
	{ ACT_IDLE,								ACT_DOD_STAND_AIM_GREASE,				false },
	{ ACT_CROUCHIDLE,						ACT_DOD_CROUCH_AIM_GREASE,				false },
	{ ACT_RUN_CROUCH,						ACT_DOD_CROUCHWALK_AIM_GREASE,			false },
	{ ACT_WALK,								ACT_DOD_WALK_AIM_GREASE,				false },
	{ ACT_RUN,								ACT_DOD_RUN_AIM_GREASE,					false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_GREASE,				false },

	// Attack ( prone? )
	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_GREASE,			false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_GREASE,		false },

	// Reload ( prone? )
	{ ACT_RELOAD,							ACT_DOD_RELOAD_GREASEGUN,				false },
	{ ACT_DOD_RELOAD_PRONE,					ACT_DOD_RELOAD_PRONE_GREASEGUN,			false },
};

IMPLEMENT_ACTTABLE( CWeaponGreaseGun );
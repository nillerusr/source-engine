//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodsemiauto.h"

#if defined( CLIENT_DLL )

	#define CWeaponM1Carbine C_WeaponM1Carbine

#endif


class CWeaponM1Carbine : public CDODSemiAutoWeapon
{
public:
	DECLARE_CLASS( CWeaponM1Carbine, CDODSemiAutoWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();
	
	CWeaponM1Carbine()  {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_M1CARBINE; }

	virtual float GetRecoil( void ) { return 1.4f; }

private:
	CWeaponM1Carbine( const CWeaponM1Carbine & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponM1Carbine, DT_WeaponM1Carbine )

BEGIN_NETWORK_TABLE( CWeaponM1Carbine, DT_WeaponM1Carbine )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponM1Carbine )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_m1carbine, CWeaponM1Carbine );
PRECACHE_WEAPON_REGISTER( weapon_m1carbine );

acttable_t CWeaponM1Carbine::m_acttable[] = 
{
	{ ACT_DOD_STAND_AIM,					ACT_DOD_STAND_AIM_RIFLE,				false },
	{ ACT_DOD_CROUCH_AIM,					ACT_DOD_CROUCH_AIM_RIFLE,				false },
	{ ACT_DOD_CROUCHWALK_AIM,				ACT_DOD_CROUCHWALK_AIM_RIFLE,			false },
	{ ACT_DOD_WALK_AIM,						ACT_DOD_WALK_AIM_RIFLE,					false },
	{ ACT_DOD_RUN_AIM,						ACT_DOD_RUN_AIM_RIFLE,					false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_RIFLE,				false },
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_IDLE_RIFLE,			false },
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_IDLE_RIFLE,				false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_IDLE_RIFLE,				false },
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_IDLE_RIFLE,			false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_IDLE_RIFLE,				false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_IDLE_RIFLE,					false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_IDLE_RIFLE,				false },

	// Attack ( prone? )
	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_RIFLE,			false },
	{ ACT_DOD_PRIMARYATTACK_CROUCH,			ACT_DOD_PRIMARYATTACK_RIFLE,			false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_RIFLE,		false },

	{ ACT_RELOAD,							ACT_DOD_RELOAD_M1CARBINE,				false },
	{ ACT_DOD_RELOAD_CROUCH,				ACT_DOD_RELOAD_CROUCH_M1CARBINE,		false },
	{ ACT_DOD_RELOAD_PRONE,					ACT_DOD_RELOAD_PRONE_M1CARBINE,			false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_K98,					false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_K98,					false },
};

IMPLEMENT_ACTTABLE( CWeaponM1Carbine );
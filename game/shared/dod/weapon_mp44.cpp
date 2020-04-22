//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodfireselect.h"


#if defined( CLIENT_DLL )

	#define CWeaponMP44 C_WeaponMP44

#endif


class CWeaponMP44 : public CDODFireSelectWeapon
{
public:
	DECLARE_CLASS( CWeaponMP44, CDODFireSelectWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();
	
	CWeaponMP44()  {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_MP44; }

	// weapon id for stats purposes
	virtual DODWeaponID GetStatsWeaponID( void )
	{
		if ( IsSemiAuto() )
			return WEAPON_MP44_SEMIAUTO;
		else 
			return WEAPON_MP44;
	}

	virtual float GetRecoil( void ) { return 5.0f; }

private:
	CWeaponMP44( const CWeaponMP44 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponMP44, DT_WeaponMP44 )

BEGIN_NETWORK_TABLE( CWeaponMP44, DT_WeaponMP44 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponMP44 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_mp44, CWeaponMP44 );
PRECACHE_WEAPON_REGISTER( weapon_mp44 );

acttable_t CWeaponMP44::m_acttable[] = 
{
	{ ACT_DOD_STAND_AIM,					ACT_DOD_STAND_AIM_MP44,					false },
	{ ACT_DOD_CROUCH_AIM,					ACT_DOD_CROUCH_AIM_MP44,				false },
	{ ACT_DOD_CROUCHWALK_AIM,				ACT_DOD_CROUCHWALK_AIM_MP44,			false },
	{ ACT_DOD_WALK_AIM,						ACT_DOD_WALK_AIM_MP44,					false },
	{ ACT_DOD_RUN_AIM,						ACT_DOD_RUN_AIM_MP44,					false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_MP44,					false },
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_IDLE_MP44,			false },
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_IDLE_MP44,				false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_IDLE_MP44,				false },
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_IDLE_MP44,			false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_IDLE_MP44,					false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_IDLE_MP44,					false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_IDLE_MP44,				false },

	// Attack ( prone? )
	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_MP44,			false },
	{ ACT_DOD_PRIMARYATTACK_CROUCH,			ACT_DOD_PRIMARYATTACK_MP44,			false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_MP44,	false },

	// Reload ( prone? )
	{ ACT_RELOAD,							ACT_DOD_RELOAD_MP44,				false },
	{ ACT_DOD_RELOAD_CROUCH,				ACT_DOD_RELOAD_CROUCH_MP44,			false },
	{ ACT_DOD_RELOAD_PRONE,					ACT_DOD_RELOAD_PRONE_MP44,			false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_MP44,				false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_MP44,				false },
};

IMPLEMENT_ACTTABLE( CWeaponMP44 );
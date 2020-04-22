//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodfireselect.h"

#if defined( CLIENT_DLL )

	#define CWeaponBAR C_WeaponBAR

#endif


class CWeaponBAR : public CDODFireSelectWeapon
{
public:
	DECLARE_CLASS( CWeaponBAR, CDODFireSelectWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();
	
	CWeaponBAR()  {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_BAR; }

	// weapon id for stats purposes
	virtual DODWeaponID GetStatsWeaponID( void )
	{
		if ( IsSemiAuto() )
			return WEAPON_BAR_SEMIAUTO;
		else 
			return WEAPON_BAR;
	}

	virtual float GetRecoil( void ) { return 5.0f; }

private:
	CWeaponBAR( const CWeaponBAR & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponBAR, DT_WeaponBAR )

BEGIN_NETWORK_TABLE( CWeaponBAR, DT_WeaponBAR )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponBAR )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_bar, CWeaponBAR );
PRECACHE_WEAPON_REGISTER( weapon_bar );

acttable_t CWeaponBAR::m_acttable[] = 
{
	{ ACT_DOD_STAND_AIM,					ACT_DOD_STAND_AIM_BAR,					false },
	{ ACT_DOD_CROUCH_AIM,					ACT_DOD_CROUCH_AIM_BAR,					false },
	{ ACT_DOD_CROUCHWALK_AIM,				ACT_DOD_CROUCHWALK_AIM_BAR,				false },
	{ ACT_DOD_WALK_AIM,						ACT_DOD_WALK_AIM_BAR,					false },
	{ ACT_DOD_RUN_AIM,						ACT_DOD_RUN_AIM_BAR,					false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_BAR,					false },
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_IDLE_BAR,				false },
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_IDLE_BAR,					false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_IDLE_BAR,				false },	
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_IDLE_BAR,			false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_IDLE_BAR,					false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_IDLE_BAR,					false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_IDLE_BAR,				false },

	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_BAR,				false },
	{ ACT_DOD_PRIMARYATTACK_CROUCH,			ACT_DOD_PRIMARYATTACK_BAR,				false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_BAR,		false },

	{ ACT_RELOAD,							ACT_DOD_RELOAD_BAR,						false },
	{ ACT_DOD_RELOAD_CROUCH,				ACT_DOD_RELOAD_CROUCH_BAR,				false },
	{ ACT_DOD_RELOAD_PRONE,					ACT_DOD_RELOAD_PRONE_BAR,				false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_K98,					false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_K98,					false },
};

IMPLEMENT_ACTTABLE( CWeaponBAR );
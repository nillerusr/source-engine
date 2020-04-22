//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbasemelee.h"

#if defined( CLIENT_DLL )

	#define CWeaponSpade C_WeaponSpade

#endif


class CWeaponSpade : public CWeaponDODBaseMelee
{
public:
	DECLARE_CLASS( CWeaponSpade, CWeaponDODBaseMelee );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();
	
	CWeaponSpade()  {}

	virtual Activity GetMeleeActivity( void ) { return ACT_VM_PRIMARYATTACK; }

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_SPADE; }

private:
	CWeaponSpade( const CWeaponSpade & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSpade, DT_WeaponSpade )

BEGIN_NETWORK_TABLE( CWeaponSpade, DT_WeaponSpade )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponSpade )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_spade, CWeaponSpade );
PRECACHE_WEAPON_REGISTER( weapon_spade );

acttable_t CWeaponSpade::m_acttable[] = 
{
	{ ACT_DOD_STAND_AIM,					ACT_DOD_STAND_AIM_SPADE,				false },
	{ ACT_DOD_CROUCH_AIM,					ACT_DOD_CROUCH_AIM_SPADE,				false },
	{ ACT_DOD_CROUCHWALK_AIM,				ACT_DOD_CROUCHWALK_AIM_SPADE,			false },
	{ ACT_DOD_WALK_AIM,						ACT_DOD_WALK_AIM_SPADE,					false },
	{ ACT_DOD_RUN_AIM,						ACT_DOD_RUN_AIM_SPADE,					false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_SPADE,				false },
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_AIM_SPADE,			false },
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_AIM_SPADE,				false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_AIM_SPADE,				false },
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_AIM_SPADE,			false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_AIM_SPADE,					false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_AIM_SPADE,					false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_AIM_SPADE,				false },

	{ ACT_RANGE_ATTACK2,					ACT_DOD_PRIMARYATTACK_SPADE,			false },
	{ ACT_DOD_SECONDARYATTACK_CROUCH,		ACT_DOD_PRIMARYATTACK_CROUCH_SPADE,		false },
	{ ACT_DOD_SECONDARYATTACK_PRONE,		ACT_DOD_PRIMARYATTACK_PRONE_SPADE,		false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_STICKGRENADE,			false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_STICKGRENADE,			false },
};

IMPLEMENT_ACTTABLE( CWeaponSpade );
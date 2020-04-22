//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbasemelee.h"
#include "dod_shareddefs.h"

#if defined( CLIENT_DLL )

	#define CWeaponAmerKnife C_WeaponAmerKnife

#endif


class CWeaponAmerKnife : public CWeaponDODBaseMelee
{
public:
	DECLARE_CLASS( CWeaponAmerKnife, CWeaponDODBaseMelee );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();
	
	CWeaponAmerKnife()  {}

	virtual Activity GetMeleeActivity( void ) { return ACT_VM_PRIMARYATTACK; }

	virtual DODWeaponID GetWeaponID( void ) const { return WEAPON_AMERKNIFE; }

	virtual void PrimaryAttack()
	{
		MeleeAttack( 60, MELEE_DMG_FIST, 0.2f, 0.4f );
	}

private:
	CWeaponAmerKnife( const CWeaponAmerKnife & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponAmerKnife, DT_WeaponAmerKnife )

BEGIN_NETWORK_TABLE( CWeaponAmerKnife, DT_WeaponAmerKnife )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponAmerKnife )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_amerknife, CWeaponAmerKnife );
PRECACHE_WEAPON_REGISTER( weapon_amerknife );

acttable_t CWeaponAmerKnife::m_acttable[] = 
{
	{ ACT_DOD_STAND_AIM,					ACT_DOD_STAND_AIM_KNIFE,				false },
	{ ACT_DOD_CROUCH_AIM,					ACT_DOD_CROUCH_AIM_KNIFE,				false },
	{ ACT_DOD_CROUCHWALK_AIM,				ACT_DOD_CROUCHWALK_AIM_KNIFE,			false },
	{ ACT_DOD_WALK_AIM,						ACT_DOD_WALK_AIM_KNIFE,					false },
	{ ACT_DOD_RUN_AIM,						ACT_DOD_RUN_AIM_KNIFE,					false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_KNIFE,				false },
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_AIM_KNIFE,			false },
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_AIM_KNIFE,				false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_AIM_KNIFE,				false },
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_AIM_KNIFE,			false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_AIM_KNIFE,					false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_AIM_KNIFE,					false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_AIM_KNIFE,				false },

	{ ACT_RANGE_ATTACK2,					ACT_DOD_PRIMARYATTACK_KNIFE,			false },
	{ ACT_DOD_SECONDARYATTACK_CROUCH,		ACT_DOD_PRIMARYATTACK_CROUCH_KNIFE,		false },
	{ ACT_DOD_SECONDARYATTACK_PRONE,		ACT_DOD_PRIMARYATTACK_PRONE_KNIFE,		false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_KNIFE,					false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_KNIFE,				false },
};

IMPLEMENT_ACTTABLE( CWeaponAmerKnife );

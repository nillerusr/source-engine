//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodsemiauto.h"

#if defined( CLIENT_DLL )

#define CWeaponP38 C_WeaponP38

#endif


class CWeaponP38 : public CDODSemiAutoWeapon
{
public:
	DECLARE_CLASS( CWeaponP38, CDODSemiAutoWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();

	CWeaponP38()  {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_P38; }

	virtual Activity GetIdleActivity( void );
	virtual Activity GetPrimaryAttackActivity( void );
	virtual Activity GetDrawActivity( void );

	virtual float GetRecoil( void ) { return 1.4f; }

private:
	CWeaponP38( const CWeaponP38 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponP38, DT_WeaponP38 )

BEGIN_NETWORK_TABLE( CWeaponP38, DT_WeaponP38 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponP38 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_p38, CWeaponP38 );
PRECACHE_WEAPON_REGISTER( weapon_p38 );

acttable_t CWeaponP38::m_acttable[] = 
{
	{ ACT_DOD_STAND_AIM,					ACT_DOD_STAND_AIM_PISTOL,				false },
	{ ACT_DOD_CROUCH_AIM,					ACT_DOD_CROUCH_AIM_PISTOL,				false },
	{ ACT_DOD_CROUCHWALK_AIM,				ACT_DOD_CROUCHWALK_AIM_PISTOL,			false },
	{ ACT_DOD_WALK_AIM,						ACT_DOD_WALK_AIM_PISTOL,				false },
	{ ACT_DOD_RUN_AIM,						ACT_DOD_RUN_AIM_PISTOL,					false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_PISTOL,				false },
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_IDLE_PISTOL,			false },
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_IDLE_PISTOL,				false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_IDLE_PISTOL,				false },
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_IDLE_PISTOL,			false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_IDLE_PISTOL,				false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_IDLE_PISTOL,				false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_IDLE_PISTOL,				false },

	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_PISTOL,			false },
	{ ACT_DOD_PRIMARYATTACK_CROUCH,			ACT_DOD_PRIMARYATTACK_PISTOL,			false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_PISTOL,		false },

	{ ACT_RELOAD,							ACT_DOD_RELOAD_PISTOL,					false },
	{ ACT_DOD_RELOAD_CROUCH,				ACT_DOD_RELOAD_CROUCH_PISTOL,			false },
	{ ACT_DOD_RELOAD_PRONE,					ACT_DOD_RELOAD_PRONE_PISTOL,			false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_PISTOL,					false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_PISTOL,				false },
};

IMPLEMENT_ACTTABLE( CWeaponP38 );

Activity CWeaponP38::GetIdleActivity( void )
{
	Activity actIdle;

	if( m_iClip1 <= 0 )
		actIdle = ACT_VM_IDLE_EMPTY;	
	else
		actIdle = ACT_VM_IDLE;

	return actIdle;
}

Activity CWeaponP38::GetPrimaryAttackActivity( void )
{
	Activity actPrim;

	if( m_iClip1 <= 0 )
		actPrim = ACT_VM_PRIMARYATTACK_EMPTY;	
	else
		actPrim = ACT_VM_PRIMARYATTACK;

	return actPrim;
}

Activity CWeaponP38::GetDrawActivity( void )
{
	Activity actDraw;

	if( m_iClip1 <= 0 )
		actDraw = ACT_VM_DRAW_EMPTY;	
	else
		actDraw = ACT_VM_DRAW;

	return actDraw;
}
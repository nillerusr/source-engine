//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodfullauto.h"


#if defined( CLIENT_DLL )

#define CWeaponC96 C_WeaponC96

#endif


class CWeaponC96 : public CDODFullAutoWeapon
{
public:
	DECLARE_CLASS( CWeaponC96, CDODFullAutoWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();

	CWeaponC96()  {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_C96; }

	virtual Activity GetIdleActivity( void );
	virtual Activity GetPrimaryAttackActivity( void );
	virtual Activity GetDrawActivity( void );
	virtual Activity GetReloadActivity( void );

	virtual float GetRecoil( void ) { return 3.0f; }

private:
	CWeaponC96( const CWeaponC96 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponC96, DT_WeaponC96 )

BEGIN_NETWORK_TABLE( CWeaponC96, DT_WeaponC96 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponC96 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_c96, CWeaponC96 );
PRECACHE_WEAPON_REGISTER( weapon_c96 );

acttable_t CWeaponC96::m_acttable[] = 
{
	{ ACT_DOD_STAND_AIM,					ACT_DOD_STAND_AIM_C96,			false },
	{ ACT_DOD_CROUCH_AIM,					ACT_DOD_CROUCH_AIM_C96,			false },
	{ ACT_DOD_CROUCHWALK_AIM,				ACT_DOD_CROUCHWALK_AIM_C96,		false },
	{ ACT_DOD_WALK_AIM,						ACT_DOD_WALK_AIM_C96,			false },
	{ ACT_DOD_RUN_AIM,						ACT_DOD_RUN_AIM_C96,			false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_C96,			false },
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_IDLE_C96,		false },
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_IDLE_C96,			false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_IDLE_C96,		false },
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_IDLE_C96,	false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_IDLE_C96,			false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_IDLE_C96,			false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_IDLE_C96,		false },

	// Attack ( prone? )
	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_C96,		false },
	{ ACT_DOD_PRIMARYATTACK_CROUCH,			ACT_DOD_PRIMARYATTACK_C96,		false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_C96,false },

	// Reload ( prone? )
	{ ACT_RELOAD,							ACT_DOD_RELOAD_C96,				false },
	{ ACT_DOD_RELOAD_CROUCH,				ACT_DOD_RELOAD_CROUCH_C96,		false },
	{ ACT_DOD_RELOAD_PRONE,					ACT_DOD_RELOAD_PRONE_C96,		false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_MP44,			false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_MP44,			false },
};

IMPLEMENT_ACTTABLE( CWeaponC96 );

Activity CWeaponC96::GetIdleActivity( void )
{
	Activity actIdle;

	if( m_iClip1 <= 0 )
		actIdle = ACT_VM_IDLE_EMPTY;	
	else
		actIdle = ACT_VM_IDLE;

	return actIdle;
}

Activity CWeaponC96::GetPrimaryAttackActivity( void )
{
	Activity actPrim;

	if( m_iClip1 <= 0 )
		actPrim = ACT_VM_PRIMARYATTACK_EMPTY;	
	else
		actPrim = ACT_VM_PRIMARYATTACK;

	return actPrim;
}

Activity CWeaponC96::GetDrawActivity( void )
{
	Activity actDraw;

	if( m_iClip1 <= 0 )
		actDraw = ACT_VM_DRAW_EMPTY;	
	else
		actDraw = ACT_VM_DRAW;

	return actDraw;
}

Activity CWeaponC96::GetReloadActivity( void )
{
	Activity actReload;

	if( m_iClip1 <= 0 )
		actReload = ACT_VM_RELOAD_EMPTY;	
	else
		actReload = ACT_VM_RELOAD;

	return actReload;
}
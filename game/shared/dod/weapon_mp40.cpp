//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodfullauto_punch.h"


#if defined( CLIENT_DLL )

	#define CWeaponMP40 C_WeaponMP40

#endif


class CWeaponMP40 : public CDODFullAutoPunchWeapon
{
public:
	DECLARE_CLASS( CWeaponMP40, CDODFullAutoPunchWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();
	
	CWeaponMP40()  {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_MP40; }
	virtual DODWeaponID GetAltWeaponID( void ) const	{ return WEAPON_MP40_PUNCH; }

	virtual Activity GetIdleActivity( void );

	virtual float GetRecoil( void ) { return 2.2f; }

private:
	CWeaponMP40( const CWeaponMP40 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponMP40, DT_WeaponMP40 )

BEGIN_NETWORK_TABLE( CWeaponMP40, DT_WeaponMP40 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponMP40 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_mp40, CWeaponMP40 );
PRECACHE_WEAPON_REGISTER( weapon_mp40 );

acttable_t CWeaponMP40::m_acttable[] = 
{
	{ ACT_DOD_STAND_AIM,					ACT_DOD_STAND_AIM_MP40,					false },
	{ ACT_DOD_CROUCH_AIM,					ACT_DOD_CROUCH_AIM_MP40,				false },
	{ ACT_DOD_CROUCHWALK_AIM,				ACT_DOD_CROUCHWALK_AIM_MP40,			false },
	{ ACT_DOD_WALK_AIM,						ACT_DOD_WALK_AIM_MP40,					false },
	{ ACT_DOD_RUN_AIM,						ACT_DOD_RUN_AIM_MP40,					false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_MP40,					false },
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_IDLE_MP40,			false },
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_IDLE_MP40,				false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_IDLE_MP40,				false },
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_IDLE_MP40,			false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_IDLE_MP40,					false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_IDLE_MP40,					false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_IDLE_MP40,				false },

	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_MP40,			false },
	{ ACT_DOD_PRIMARYATTACK_CROUCH,			ACT_DOD_PRIMARYATTACK_MP40,			false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_MP40,	false },
	{ ACT_RANGE_ATTACK2,					ACT_DOD_SECONDARYATTACK_MP40,		false },
	{ ACT_DOD_SECONDARYATTACK_CROUCH,		ACT_DOD_SECONDARYATTACK_CROUCH_MP40,false },
	{ ACT_DOD_SECONDARYATTACK_PRONE,		ACT_DOD_SECONDARYATTACK_PRONE_MP40,	false },

	{ ACT_RELOAD,							ACT_DOD_RELOAD_MP40,				false },
	{ ACT_DOD_RELOAD_CROUCH,				ACT_DOD_RELOAD_CROUCH_MP40,			false },
	{ ACT_DOD_RELOAD_PRONE,					ACT_DOD_RELOAD_PRONE_MP40,			false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_MP44,				false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_MP44,				false },
};

IMPLEMENT_ACTTABLE( CWeaponMP40 );

Activity CWeaponMP40::GetIdleActivity( void )
{
	Activity actIdle;

	if( m_iClip1 < GetMaxClip1() )
		actIdle = ACT_VM_IDLE_EMPTY;	
	else
		actIdle = ACT_VM_IDLE;

	return actIdle;
}

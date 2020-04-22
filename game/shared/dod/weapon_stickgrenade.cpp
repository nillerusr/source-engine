//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbasegrenade.h"

#ifdef CLIENT_DLL
	#define CWeaponStickGrenade C_WeaponStickGrenade
#else
	#include "dod_stickgrenade.h"
#endif

class CWeaponStickGrenade : public CWeaponDODBaseGrenade
{
public:
	DECLARE_CLASS( CWeaponStickGrenade, CWeaponDODBaseGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();

	CWeaponStickGrenade() {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_FRAG_GER; }

#ifndef CLIENT_DLL

	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, float flLifeTime = GRENADE_FUSE_LENGTH )
	{
		// align the stickgrenade vertically and spin end over end
		QAngle vecNewAngles = QAngle(45,pPlayer->EyeAngles().y,0);
		AngularImpulse angNewImpulse = AngularImpulse( 0, 600, 0 );

		CDODStickGrenade::Create( vecSrc, vecNewAngles, vecVel, angNewImpulse, pPlayer, flLifeTime, GetWeaponID() );
	}

#endif

	CWeaponStickGrenade( const CWeaponStickGrenade & ) {}
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponStickGrenade, DT_WeaponStickGrenade )

BEGIN_NETWORK_TABLE(CWeaponStickGrenade, DT_WeaponStickGrenade)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponStickGrenade )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_frag_ger, CWeaponStickGrenade );
PRECACHE_WEAPON_REGISTER( weapon_frag_ger );

acttable_t CWeaponStickGrenade::m_acttable[] = 
{
	// Move this out to the specific grenades???
	{ ACT_DOD_STAND_AIM,					ACT_DOD_STAND_AIM_GREN_STICK,				false },
	{ ACT_DOD_CROUCH_AIM,					ACT_DOD_CROUCH_AIM_GREN_STICK,				false },
	{ ACT_DOD_CROUCHWALK_AIM,				ACT_DOD_CROUCHWALK_AIM_GREN_STICK,			false },
	{ ACT_DOD_WALK_AIM,						ACT_DOD_WALK_AIM_GREN_STICK,				false },
	{ ACT_DOD_RUN_AIM,						ACT_DOD_RUN_AIM_GREN_STICK,					false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_GREN_STICK,				false },
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_AIM_GREN_STICK,			false },
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_AIM_GREN_STICK,				false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_AIM_GREN_STICK,				false },
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_AIM_GREN_STICK,			false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_AIM_GREN_STICK,				false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_AIM_GREN_STICK,					false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_AIM_GREN_STICK,				false },

	{ ACT_RANGE_ATTACK1,				ACT_DOD_PRIMARYATTACK_GREN_STICK,		false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,		ACT_DOD_PRIMARYATTACK_PRONE_GREN_STICK,	false },
	{ ACT_DOD_PRIMARYATTACK_CROUCH,		ACT_DOD_PRIMARYATTACK_CROUCH_GREN_STICK, false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_STICKGRENADE,			false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_STICKGRENADE,			false },
};

IMPLEMENT_ACTTABLE( CWeaponStickGrenade );


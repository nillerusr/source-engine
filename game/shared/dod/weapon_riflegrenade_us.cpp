//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_riflegrenade.h"

#if defined( CLIENT_DLL )

	#include "c_dod_player.h"
	#define CWeaponRifleGrenadeUS C_WeaponRifleGrenadeUS

#else

	#include "dod_riflegrenade_us.h"
	#include "dod_player.h"

#endif

class CWeaponRifleGrenadeUS : public CWeaponBaseRifleGrenade
{
public:
	DECLARE_CLASS( CWeaponRifleGrenadeUS, CWeaponBaseRifleGrenade );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();

	CWeaponRifleGrenadeUS()  {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_RIFLEGREN_US; }

#ifndef CLIENT_DLL
	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, float flLifeTime = GRENADE_FUSE_LENGTH )
	{
		CDODRifleGrenadeUS::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, flLifeTime, GetWeaponID() );
	}
#endif

	virtual const char *GetCompanionWeaponName( void )
	{
		return "weapon_garand";
	}

private:
	CWeaponRifleGrenadeUS( const CWeaponRifleGrenadeUS & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponRifleGrenadeUS, DT_WeaponRifleGrenadeUS )

BEGIN_NETWORK_TABLE( CWeaponRifleGrenadeUS, DT_WeaponRifleGrenadeUS )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponRifleGrenadeUS )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_riflegren_us, CWeaponRifleGrenadeUS );
PRECACHE_WEAPON_REGISTER( weapon_riflegren_us );

acttable_t CWeaponRifleGrenadeUS::m_acttable[] = 
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

	// Attack ( prone? deployed? )
	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_RIFLE,			false },
	{ ACT_DOD_PRIMARYATTACK_CROUCH,			ACT_DOD_PRIMARYATTACK_RIFLE,			false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_RIFLE,		false },

	{ ACT_RELOAD,							ACT_DOD_RELOAD_RIFLEGRENADE,			false },
	{ ACT_DOD_RELOAD_CROUCH,				ACT_DOD_RELOAD_CROUCH_RIFLEGRENADE,		false },
	{ ACT_DOD_RELOAD_PRONE,					ACT_DOD_RELOAD_PRONE_RIFLEGRENADE,		false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_K98,					false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_K98,					false },
};

IMPLEMENT_ACTTABLE( CWeaponRifleGrenadeUS );

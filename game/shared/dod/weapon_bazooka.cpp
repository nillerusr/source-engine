//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbaserpg.h"

#if defined( CLIENT_DLL )

	#include "c_dod_player.h"
	#define CWeaponBazooka C_WeaponBazooka

#else

	#include "rocket_bazooka.h"
	#include "dod_player.h"
	
#endif


class CWeaponBazooka : public CDODBaseRocketWeapon
{
public:
	DECLARE_CLASS( CWeaponBazooka, CDODBaseRocketWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();
	
	CWeaponBazooka()  {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_BAZOOKA; }

	virtual void FireRocket( void );

private:
	CWeaponBazooka( const CWeaponBazooka & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponBazooka, DT_WeaponBazooka )

BEGIN_NETWORK_TABLE( CWeaponBazooka, DT_WeaponBazooka )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponBazooka )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_bazooka, CWeaponBazooka );
PRECACHE_WEAPON_REGISTER( weapon_bazooka );

acttable_t CWeaponBazooka::m_acttable[] = 
{
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_BAZOOKA,				false },
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_IDLE_BAZOOKA,			false },
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_IDLE_BAZOOKA,				false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_IDLE_BAZOOKA,			false },
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_IDLE_BAZOOKA,		false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_IDLE_BAZOOKA,				false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_IDLE_BAZOOKA,				false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_IDLE_BAZOOKA,			false },

	// Zoomed Aim
	{ ACT_DOD_IDLE_ZOOMED,					ACT_DOD_STAND_ZOOM_BAZOOKA,				false },
	{ ACT_DOD_CROUCH_ZOOMED,				ACT_DOD_CROUCH_ZOOM_BAZOOKA,			false },
	{ ACT_DOD_CROUCHWALK_ZOOMED,			ACT_DOD_CROUCHWALK_ZOOM_BAZOOKA,		false },
	{ ACT_DOD_WALK_ZOOMED,					ACT_DOD_WALK_ZOOM_BAZOOKA,				false },
	{ ACT_DOD_PRONE_ZOOMED,					ACT_DOD_PRONE_ZOOM_BAZOOKA,				false },
	{ ACT_DOD_PRONE_FORWARD_ZOOMED,			ACT_DOD_PRONE_ZOOM_FORWARD_BAZOOKA,		false },

	// Attack ( must be zoomed )
	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_BAZOOKA,			false },
	{ ACT_DOD_PRIMARYATTACK_CROUCH,			ACT_DOD_PRIMARYATTACK_BAZOOKA,			false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_BAZOOKA,	false },

	// Reload ( zoomed or not, prone or not )
	{ ACT_RELOAD,							ACT_DOD_RELOAD_BAZOOKA,					false },
	{ ACT_DOD_RELOAD_CROUCH,				ACT_DOD_RELOAD_CROUCH_BAZOOKA,			false },
	{ ACT_DOD_RELOAD_PRONE,					ACT_DOD_RELOAD_PRONE_BAZOOKA,			false },	
	{ ACT_DOD_RELOAD_DEPLOYED,				ACT_DOD_ZOOMLOAD_BAZOOKA,				false },
	{ ACT_DOD_RELOAD_PRONE_DEPLOYED,		ACT_DOD_ZOOMLOAD_PRONE_BAZOOKA,			false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_BAZOOKA,				false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_BAZOOKA,				false },
};

IMPLEMENT_ACTTABLE( CWeaponBazooka );

void CWeaponBazooka::FireRocket( void )
{
#ifndef CLIENT_DLL

	CBasePlayer *pPlayer = GetPlayerOwner();

#ifdef DBGFLAG_ASSERT
	CBazookaRocket *pRocket = 
#endif //DEBUG		
		CBazookaRocket::Create( pPlayer->Weapon_ShootPosition(), pPlayer->EyeAngles(), pPlayer );

	Assert( pRocket );

#endif
}

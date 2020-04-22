//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbaserpg.h"

#ifndef CLIENT_DLL
	#include "rocket_pschreck.h"
#endif

#if defined( CLIENT_DLL )

	#define CWeaponPschreck C_WeaponPschreck

#endif


class CWeaponPschreck : public CDODBaseRocketWeapon
{
public:
	DECLARE_CLASS( CWeaponPschreck, CDODBaseRocketWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();
	
	CWeaponPschreck()  {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_PSCHRECK; }

	virtual void FireRocket( void );

private:
	CWeaponPschreck( const CWeaponPschreck & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponPschreck, DT_WeaponPschreck )

BEGIN_NETWORK_TABLE( CWeaponPschreck, DT_WeaponPschreck )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponPschreck )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_pschreck, CWeaponPschreck );
PRECACHE_WEAPON_REGISTER( weapon_pschreck );

acttable_t CWeaponPschreck::m_acttable[] = 
{
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_PSCHRECK,				false },
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_IDLE_PSCHRECK,		false },
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_IDLE_PSCHRECK,			false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_IDLE_PSCHRECK,			false },
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_IDLE_PSCHRECK,		false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_IDLE_PSCHRECK,				false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_IDLE_PSCHRECK,				false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_IDLE_PSCHRECK,			false },

	// Zoomed Aim
	{ ACT_DOD_IDLE_ZOOMED,					ACT_DOD_STAND_ZOOM_PSCHRECK,			false },
	{ ACT_DOD_CROUCH_ZOOMED,				ACT_DOD_CROUCH_ZOOM_PSCHRECK,			false },
	{ ACT_DOD_CROUCHWALK_ZOOMED,			ACT_DOD_CROUCHWALK_ZOOM_PSCHRECK,		false },
	{ ACT_DOD_WALK_ZOOMED,					ACT_DOD_WALK_ZOOM_PSCHRECK,				false },
	{ ACT_DOD_PRONE_ZOOMED,					ACT_DOD_PRONE_ZOOM_PSCHRECK,			false },
	{ ACT_DOD_PRONE_FORWARD_ZOOMED,			ACT_DOD_PRONE_ZOOM_FORWARD_PSCHRECK,	false },

	// Attack ( must be zoomed )
	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_PSCHRECK,			false },
	{ ACT_DOD_PRIMARYATTACK_CROUCH,			ACT_DOD_PRIMARYATTACK_PSCHRECK,			false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_PSCHRECK,	false },

	// Reload ( zoomed or not, prone or not )
	{ ACT_RELOAD,							ACT_DOD_RELOAD_PSCHRECK,				false },
	{ ACT_DOD_RELOAD_CROUCH,				ACT_DOD_RELOAD_CROUCH_PSCHRECK,			false },
	{ ACT_DOD_RELOAD_PRONE,					ACT_DOD_RELOAD_PRONE_PSCHRECK,			false },	
	{ ACT_DOD_RELOAD_DEPLOYED,				ACT_DOD_ZOOMLOAD_PSCHRECK,				false },
	{ ACT_DOD_RELOAD_PRONE_DEPLOYED,		ACT_DOD_ZOOMLOAD_PRONE_PSCHRECK,		false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_PSCHRECK,				false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_PSCHRECK,				false },
};

IMPLEMENT_ACTTABLE( CWeaponPschreck );

void CWeaponPschreck::FireRocket( void )
{
#ifndef CLIENT_DLL

	CBasePlayer *pPlayer = GetPlayerOwner();

#ifdef DBGFLAG_ASSERT
	CPschreckRocket *pRocket = 
#endif //DEBUG		
		CPschreckRocket::Create( pPlayer->Weapon_ShootPosition(), pPlayer->GetAbsAngles(), pPlayer );

	Assert( pRocket );

#endif
}

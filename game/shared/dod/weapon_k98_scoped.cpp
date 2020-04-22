//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodsniper.h"

#if defined( CLIENT_DLL )

	#define CWeaponK98Scoped C_WeaponK98Scoped

#endif


class CWeaponK98Scoped : public CDODSniperWeapon
{
public:
	DECLARE_CLASS( CWeaponK98Scoped, CDODSniperWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();
	
	CWeaponK98Scoped()  {}

	virtual void Spawn( void );

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_K98_SCOPED; }

	// weapon id for stats purposes
	virtual DODWeaponID GetStatsWeaponID( void )
	{
		if ( IsFullyZoomed() )
			return WEAPON_K98_SCOPED_ZOOMED;
		else 
			return WEAPON_K98_SCOPED;
	}

	virtual bool ShouldAutoEjectBrass( void ) { return false; }
	virtual bool ShouldDrawCrosshair( void ) { return false; }

	virtual bool ShouldDrawViewModel( void )
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

		Assert( pPlayer );

		if ( !pPlayer )
			return false;

		if ( pPlayer->GetFOV() < 90 )
			return false;

		// handle case when we are spectating someone and don't know their fov
		if ( IsFullyZoomed() )
			return false;

		return BaseClass::ShouldDrawViewModel();
	}

	virtual bool ShouldDrawMuzzleFlash( void )
	{
		return ShouldDrawViewModel();
	}

	virtual Activity GetPrimaryAttackActivity( void	);

	virtual float GetFireDelay( void );

	virtual float GetRecoil( void ) { return 6.0f; }

private:
	CWeaponK98Scoped( const CWeaponK98Scoped & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponK98Scoped, DT_WeaponK98Scoped )

BEGIN_NETWORK_TABLE( CWeaponK98Scoped, DT_WeaponK98Scoped )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponK98Scoped )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_k98_scoped, CWeaponK98Scoped );
PRECACHE_WEAPON_REGISTER( weapon_k98_scoped );

void CWeaponK98Scoped::Spawn( void )
{
	m_iAltFireHint = HINT_USE_ZOOM;

	BaseClass::Spawn();
}

acttable_t CWeaponK98Scoped::m_acttable[] = 
{
	{ ACT_DOD_STAND_AIM,					ACT_DOD_STAND_AIM_BOLT,					false },
	{ ACT_DOD_CROUCH_AIM,					ACT_DOD_CROUCH_AIM_BOLT,				false },
	{ ACT_DOD_CROUCHWALK_AIM,				ACT_DOD_CROUCHWALK_AIM_BOLT,			false },
	{ ACT_DOD_WALK_AIM,						ACT_DOD_WALK_AIM_BOLT,					false },
	{ ACT_DOD_RUN_AIM,						ACT_DOD_RUN_AIM_BOLT,					false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_BOLT,					false },
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_IDLE_BOLT,			false },
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_IDLE_BOLT,				false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_IDLE_BOLT,				false },
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_IDLE_BOLT,			false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_IDLE_BOLT,					false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_IDLE_BOLT,					false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_IDLE_BOLT,				false },

	// Zoomed Aim
	{ ACT_DOD_IDLE_ZOOMED,					ACT_DOD_STAND_ZOOM_BOLT,				false },
	{ ACT_DOD_CROUCH_ZOOMED,				ACT_DOD_CROUCH_ZOOM_BOLT,				false },
	{ ACT_DOD_CROUCHWALK_ZOOMED,			ACT_DOD_CROUCHWALK_ZOOM_BOLT,			false },
	{ ACT_DOD_WALK_ZOOMED,					ACT_DOD_WALK_ZOOM_BOLT,					false },
	{ ACT_DOD_PRONE_ZOOMED,					ACT_DOD_PRONE_ZOOM_BOLT,				false },
	{ ACT_DOD_PRONE_FORWARD_ZOOMED,			ACT_DOD_PRONE_ZOOM_FORWARD_BOLT,		false },

	// Attack ( prone? )
	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_BOLT,				false },
	{ ACT_DOD_PRIMARYATTACK_CROUCH,			ACT_DOD_PRIMARYATTACK_BOLT,				false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_BOLT,		false },

	// Reload ( prone ? )
	{ ACT_RELOAD,							ACT_DOD_RELOAD_BOLT,					false },
	{ ACT_DOD_RELOAD_CROUCH,				ACT_DOD_RELOAD_CROUCH_BOLT,				false },
	{ ACT_DOD_RELOAD_PRONE,					ACT_DOD_RELOAD_PRONE_BOLT,				false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_K98,					false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_K98,					false },
};

IMPLEMENT_ACTTABLE( CWeaponK98Scoped );

Activity CWeaponK98Scoped::GetPrimaryAttackActivity( void )
{
	Activity actPrim;

	if( m_iClip1 <= 0 )
		actPrim = ACT_VM_PRIMARYATTACK_EMPTY;	
	else
		actPrim = ACT_VM_PRIMARYATTACK;

	return actPrim;
}

float CWeaponK98Scoped::GetFireDelay( void )
{
	if ( m_iClip1 <= 0 )
	{
		return SequenceDuration();
	}

	return BaseClass::GetFireDelay();
}
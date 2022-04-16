//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodsniper.h"
#include "dod_shareddefs.h"

#ifndef CLIENT_DLL
	#include "te_effect_dispatch.h"
	#include "effect_dispatch_data.h"
#endif

#if defined( CLIENT_DLL )
	#define CWeaponGarand C_WeaponGarand

	#include "c_dod_player.h"
#endif


class CWeaponGarand : public CDODSniperWeapon
{
public:
	DECLARE_CLASS( CWeaponGarand, CDODSniperWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();
	
	CWeaponGarand()  {}

	virtual void	Spawn( void );

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_GARAND; }

	// weapon id for stats purposes
	virtual DODWeaponID GetStatsWeaponID( void )
	{
		if ( IsFullyZoomed() )
            return WEAPON_GARAND_ZOOMED;
		else 
			return WEAPON_GARAND;
	}

	virtual Activity GetIdleActivity( void );
	virtual Activity GetPrimaryAttackActivity( void );
	virtual Activity GetDrawActivity( void );

	virtual void PrimaryAttack( void );
	virtual bool Reload( void );

	virtual float GetZoomedFOV( void ) { return 55; }

	virtual bool HideViewModelWhenZoomed( void ) { return false; }

	virtual bool ShouldZoomOutBetweenShots( void ) { return false; }
	virtual bool ShouldRezoomAfterReload( void ) { return true; }

	virtual float GetRecoil( void ) { return 4.0f; }

private:
	CWeaponGarand( const CWeaponGarand & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponGarand, DT_WeaponGarand )

BEGIN_NETWORK_TABLE( CWeaponGarand, DT_WeaponGarand )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponGarand )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_garand, CWeaponGarand );
PRECACHE_WEAPON_REGISTER( weapon_garand );

void CWeaponGarand::Spawn( void )
{
	m_iAltFireHint = HINT_USE_IRON_SIGHTS;

	BaseClass::Spawn();
}

acttable_t CWeaponGarand::m_acttable[] = 
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

	// Zoomed Aim
	{ ACT_DOD_IDLE_ZOOMED,					ACT_DOD_STAND_ZOOM_RIFLE,				false },
	{ ACT_DOD_CROUCH_ZOOMED,				ACT_DOD_CROUCH_ZOOM_RIFLE,				false },
	{ ACT_DOD_CROUCHWALK_ZOOMED,			ACT_DOD_CROUCHWALK_ZOOM_RIFLE,			false },
	{ ACT_DOD_WALK_ZOOMED,					ACT_DOD_WALK_ZOOM_RIFLE,				false },
	{ ACT_DOD_PRONE_ZOOMED,					ACT_DOD_PRONE_ZOOM_RIFLE,				false },
	{ ACT_DOD_PRONE_FORWARD_ZOOMED,			ACT_DOD_PRONE_ZOOM_FORWARD_RIFLE,		false },

	// Attack ( prone? deployed? )
	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_RIFLE,			false },
	{ ACT_DOD_PRIMARYATTACK_CROUCH,			ACT_DOD_PRIMARYATTACK_RIFLE,			false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_RIFLE,		false },

	{ ACT_RELOAD,							ACT_DOD_RELOAD_RIFLE,					false },
	{ ACT_DOD_RELOAD_CROUCH,				ACT_DOD_RELOAD_CROUCH_RIFLE,			false },
	{ ACT_DOD_RELOAD_PRONE,					ACT_DOD_RELOAD_PRONE_RIFLE,				false },

	// Hand Signals
	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_K98,					false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_K98,					false },
};

IMPLEMENT_ACTTABLE( CWeaponGarand );

Activity CWeaponGarand::GetIdleActivity( void )
{
	Activity actIdle;

	if( m_iClip1 <= 0 )
		actIdle = ACT_VM_IDLE_EMPTY;	
	else
		actIdle = ACT_VM_IDLE;

	return actIdle;
}

Activity CWeaponGarand::GetPrimaryAttackActivity( void )
{
	Activity actPrim;

	if ( IsFullyZoomed() )
		actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED;
	else if( m_iClip1 <= 0 )
		actPrim = ACT_VM_PRIMARYATTACK_EMPTY;	
	else
		actPrim = ACT_VM_PRIMARYATTACK;

	return actPrim;
}

Activity CWeaponGarand::GetDrawActivity( void )
{
	Activity actDraw;

	if( m_iClip1 <= 0 )
		actDraw = ACT_VM_DRAW_EMPTY;	
	else
		actDraw = ACT_VM_DRAW;

	return actDraw;
}

void CWeaponGarand::PrimaryAttack( void )
{
	int clip = m_iClip1;

	BaseClass::PrimaryAttack();

	// If we just fired our last bullet
	if( clip != m_iClip1 && m_iClip1 == 0 )
	{
		// clip "DING!"
		WeaponSound( SPECIAL1 );

#ifndef CLIENT_DLL
		CEffectData data;
		data.m_nHitBox = EJECTBRASS_GARANDCLIP;
		GetPlayerOwner()->GetAttachment( 2, data.m_vOrigin, data.m_vAngles  );
	
		// shoot it up in the air
		data.m_vAngles.x = -90;
		data.m_vAngles.y = 0;
		data.m_vAngles.z = 0;

		DispatchEffect( "DOD_EjectBrass", data );
#endif
	}
}

bool CWeaponGarand::Reload( void )
{
	if( m_iClip1 > 0 )
	{
#ifdef CLIENT_DLL
		CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
		Assert( pPlayer );
		pPlayer->HintMessage( HINT_GARAND_RELOAD, true );
#endif
		return false;
	}

	return BaseClass::Reload();
}


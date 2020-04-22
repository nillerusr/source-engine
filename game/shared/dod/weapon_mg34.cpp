//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbipodgun.h"

#if defined( CLIENT_DLL )

	#define CWeaponMG34 C_WeaponMG34
	#include "c_dod_player.h"

#else

	#include "dod_player.h"
#endif


class CWeaponMG34 : public CDODBipodWeapon
{
public:
	DECLARE_CLASS( CWeaponMG34, CDODBipodWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();
	
	CWeaponMG34()  {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_MG34; }

	virtual void PrimaryAttack( void );
	virtual bool Reload( void );

	Activity GetReloadActivity( void );
	Activity GetDrawActivity( void );
	Activity GetDeployActivity( void );
	Activity GetUndeployActivity( void );
	Activity GetIdleActivity( void );
	Activity GetPrimaryAttackActivity( void );

	virtual bool ShouldDrawCrosshair( void ) { return IsDeployed(); }

private:
	CWeaponMG34( const CWeaponMG34 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponMG34, DT_WeaponMG34 )

BEGIN_NETWORK_TABLE( CWeaponMG34, DT_WeaponMG34 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponMG34 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_mg34, CWeaponMG34 );
PRECACHE_WEAPON_REGISTER( weapon_mg34 );

acttable_t CWeaponMG34::m_acttable[] = 
{
	// Aim
	{ ACT_IDLE,								ACT_DOD_STAND_AIM_MG,					false },
	{ ACT_CROUCHIDLE,						ACT_DOD_CROUCH_AIM_MG,					false },
	{ ACT_RUN_CROUCH,						ACT_DOD_CROUCHWALK_AIM_MG,				false },
	{ ACT_WALK,								ACT_DOD_WALK_AIM_MG,					false },
	{ ACT_RUN,								ACT_DOD_RUN_AIM_MG,						false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_MG,					false },

	// Deployed Aim
	{ ACT_DOD_DEPLOYED,						ACT_DOD_DEPLOY_MG,						false },
	{ ACT_DOD_PRONE_DEPLOYED,				ACT_DOD_PRONE_DEPLOY_MG,				false },

	// Attack ( prone? deployed? )
	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_MG,				false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_MG,			false },
	{ ACT_DOD_PRIMARYATTACK_DEPLOYED,		ACT_DOD_PRIMARYATTACK_DEPLOYED_MG,		false },
	{ ACT_DOD_PRIMARYATTACK_PRONE_DEPLOYED,	ACT_DOD_PRIMARYATTACK_PRONE_DEPLOYED_MG,false },

	// Reload ( prone? deployed? )	
	{ ACT_DOD_RELOAD_DEPLOYED,				ACT_DOD_RELOAD_DEPLOYED_MG34,			false },
	{ ACT_DOD_RELOAD_PRONE_DEPLOYED,		ACT_DOD_RELOAD_PRONE_DEPLOYED_MG34,		false },
};

IMPLEMENT_ACTTABLE( CWeaponMG34 );

void CWeaponMG34::PrimaryAttack( void )
{
#ifdef CLIENT_DLL
	C_DODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
#else
	CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );
#endif

	Assert( pPlayer );

	if( !IsDeployed() )
	{
#ifdef CLIENT_DLL 
		pPlayer->HintMessage( HINT_MG_FIRE_UNDEPLOYED );
#endif
		pPlayer->m_Shared.SetSlowedTime( 0.2 );

		float flStamina = pPlayer->m_Shared.GetStamina();

		pPlayer->m_Shared.SetStamina( flStamina - 15 );
	}

	BaseClass::PrimaryAttack();
}

bool CWeaponMG34::Reload( void )
{
	if( !IsDeployed() )
	{
		ClientPrint( GetPlayerOwner(), HUD_PRINTCENTER, "#Dod_mg_reload" );
		return false;
	}

	return BaseClass::Reload();
}

Activity CWeaponMG34::GetReloadActivity( void )
{
	return ACT_VM_RELOAD;
}

Activity CWeaponMG34::GetDrawActivity( void )
{
	Activity actDraw;

	if( m_iClip1 <= 0 )
		actDraw = ACT_VM_DRAW_EMPTY;	
	else
		actDraw = ACT_VM_DRAW;

	return actDraw;
}

Activity CWeaponMG34::GetDeployActivity( void )
{
	Activity actDeploy;

	if( m_iClip1 <= 0 )
		actDeploy = ACT_VM_DEPLOY_EMPTY;	
	else
		actDeploy = ACT_VM_DEPLOY;

	return actDeploy;
}

Activity CWeaponMG34::GetUndeployActivity( void )
{
	Activity actUndeploy;

	if( m_iClip1 <= 0 )
		actUndeploy = ACT_VM_UNDEPLOY_EMPTY;	
	else
		actUndeploy = ACT_VM_UNDEPLOY;

	return actUndeploy;
}

Activity CWeaponMG34::GetIdleActivity( void )
{
	Activity actIdle;

	if( IsDeployed() )
	{
		if( m_iClip1 <= 0 )
			actIdle = ACT_VM_IDLE_DEPLOYED_EMPTY;	
		else
			actIdle = ACT_VM_IDLE_DEPLOYED;
	}
	else
	{
		if( m_iClip1 <= 0 )
			actIdle = ACT_VM_IDLE_EMPTY;	
		else
			actIdle = ACT_VM_IDLE;
	}

	return actIdle;
}

Activity CWeaponMG34::GetPrimaryAttackActivity( void )
{
	Activity actPrim;

	if( IsDeployed() )
	{
		if( m_iClip1 <= 0 )
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_EMPTY;	
		else
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED;
	}
	else
	{
		if( m_iClip1 <= 0 )
			actPrim = ACT_VM_PRIMARYATTACK_EMPTY;	
		else
			actPrim = ACT_VM_PRIMARYATTACK;
	}

	return actPrim;
}


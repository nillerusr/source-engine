//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_dodbipodgun.h"

#if defined( CLIENT_DLL )

	#include "c_dod_player.h"

	#define CWeapon30cal C_Weapon30cal

#else

	#include "dod_player.h"

#endif


class CWeapon30cal : public CDODBipodWeapon
{
public:
	DECLARE_CLASS( CWeapon30cal, CDODBipodWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();
	
	CWeapon30cal()  {}

	virtual DODWeaponID GetWeaponID( void ) const		{ return WEAPON_30CAL; }

	// weapon id for stats purposes
	virtual DODWeaponID GetStatsWeaponID( void )
	{
		if ( !IsDeployed() )
			return WEAPON_30CAL_UNDEPLOYED;
		else 
			return WEAPON_30CAL;
	}

	virtual void PrimaryAttack( void );

	virtual bool ShouldDrawCrosshair( void ) { return IsDeployed(); }

	virtual bool Reload( void );

	virtual Activity GetDrawActivity( void );
	virtual Activity GetDeployActivity( void );
	virtual Activity GetUndeployActivity( void );
	virtual Activity GetIdleActivity( void );
	virtual Activity GetPrimaryAttackActivity( void );

	virtual float GetRecoil( void );

private:
	CWeapon30cal( const CWeapon30cal & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( Weapon30cal, DT_Weapon30cal )

BEGIN_NETWORK_TABLE( CWeapon30cal, DT_Weapon30cal )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeapon30cal )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_30cal, CWeapon30cal );
PRECACHE_WEAPON_REGISTER( weapon_30cal );

acttable_t CWeapon30cal::m_acttable[] = 
{
	{ ACT_DOD_STAND_AIM,					ACT_DOD_STAND_AIM_30CAL,				false },
	{ ACT_DOD_CROUCH_AIM,					ACT_DOD_CROUCH_AIM_30CAL,				false },
	{ ACT_DOD_CROUCHWALK_AIM,				ACT_DOD_CROUCHWALK_AIM_30CAL,			false },
	{ ACT_DOD_WALK_AIM,						ACT_DOD_WALK_AIM_30CAL,					false },
	{ ACT_DOD_RUN_AIM,						ACT_DOD_RUN_AIM_30CAL,					false },
	{ ACT_PRONE_IDLE,						ACT_DOD_PRONE_AIM_30CAL,				false },
	{ ACT_PRONE_FORWARD,					ACT_DOD_PRONEWALK_IDLE_30CAL,			false },
	{ ACT_DOD_STAND_IDLE,					ACT_DOD_STAND_IDLE_30CAL,				false },
	{ ACT_DOD_CROUCH_IDLE,					ACT_DOD_CROUCH_IDLE_30CAL,				false },
	{ ACT_DOD_CROUCHWALK_IDLE,				ACT_DOD_CROUCHWALK_IDLE_30CAL,			false },
	{ ACT_DOD_WALK_IDLE,					ACT_DOD_WALK_IDLE_30CAL,				false },
	{ ACT_DOD_RUN_IDLE,						ACT_DOD_RUN_IDLE_30CAL,					false },
	{ ACT_SPRINT,							ACT_DOD_SPRINT_IDLE_30CAL,				false },

	{ ACT_RANGE_ATTACK1,					ACT_DOD_PRIMARYATTACK_30CAL,			false },
	{ ACT_DOD_PRIMARYATTACK_CROUCH,			ACT_DOD_PRIMARYATTACK_30CAL,			false },
	{ ACT_DOD_PRIMARYATTACK_PRONE,			ACT_DOD_PRIMARYATTACK_PRONE_30CAL,		false },
	{ ACT_DOD_DEPLOYED,						ACT_DOD_DEPLOY_30CAL,					false },
	{ ACT_DOD_PRONE_DEPLOYED,				ACT_DOD_PRONE_DEPLOY_30CAL,				false },
	{ ACT_DOD_PRIMARYATTACK_DEPLOYED,		ACT_DOD_PRIMARYATTACK_DEPLOYED_30CAL,	false },
	{ ACT_DOD_PRIMARYATTACK_PRONE_DEPLOYED,	ACT_DOD_PRIMARYATTACK_PRONE_DEPLOYED_30CAL,false },
	{ ACT_DOD_RELOAD_DEPLOYED,				ACT_DOD_RELOAD_DEPLOYED_30CAL,			false },
	{ ACT_DOD_RELOAD_PRONE_DEPLOYED,		ACT_DOD_RELOAD_PRONE_DEPLOYED_30CAL,	false },

	{ ACT_DOD_HS_IDLE,						ACT_DOD_HS_IDLE_30CAL,					false },
	{ ACT_DOD_HS_CROUCH,					ACT_DOD_HS_CROUCH_30CAL,				false },
};

IMPLEMENT_ACTTABLE( CWeapon30cal );

bool CWeapon30cal::Reload( void )
{
	if( !IsDeployed() )
	{
#ifdef CLIENT_DLL
		CDODPlayer *pPlayer = ToDODPlayer( GetPlayerOwner() );

		if ( pPlayer )
			pPlayer->HintMessage( HINT_MG_DEPLOY_TO_RELOAD );
#endif
		return false;
	}

	return BaseClass::Reload();
}

void CWeapon30cal::PrimaryAttack( void )
{
	if ( m_iClip1 <= 0 )
	{
		if (m_bFireOnEmpty)
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
		}

		return;
	}

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

Activity CWeapon30cal::GetDrawActivity( void )
{
	Activity actDraw;

	if( m_iClip1 <= 0 )
		actDraw = ACT_VM_DRAW_EMPTY;	
	else
		actDraw = ACT_VM_DRAW;

	return actDraw;
}

Activity CWeapon30cal::GetDeployActivity( void )
{
	Activity actDeploy;

	switch ( m_iClip1 )
	{
	case 8:
		actDeploy = ACT_VM_DEPLOY_8;
		break;
	case 7:
		actDeploy = ACT_VM_DEPLOY_7;
		break;
	case 6:
		actDeploy = ACT_VM_DEPLOY_6;
		break;
	case 5:
		actDeploy = ACT_VM_DEPLOY_5;
		break;
	case 4:
		actDeploy = ACT_VM_DEPLOY_4;
		break;
	case 3:
		actDeploy = ACT_VM_DEPLOY_3;
		break;
	case 2:
		actDeploy = ACT_VM_DEPLOY_2;
		break;
	case 1:
		actDeploy = ACT_VM_DEPLOY_1;
		break;
	case 0:
		actDeploy = ACT_VM_DEPLOY_EMPTY;
		break;
	default:
		actDeploy = ACT_VM_DEPLOY;
		break;
	}

	return actDeploy;
}

Activity CWeapon30cal::GetUndeployActivity( void )
{
	Activity actUndeploy;

	switch ( m_iClip1 )
	{
	case 8:
		actUndeploy = ACT_VM_UNDEPLOY_8;
		break;
	case 7:
		actUndeploy = ACT_VM_UNDEPLOY_7;
		break;
	case 6:
		actUndeploy = ACT_VM_UNDEPLOY_6;
		break;
	case 5:
		actUndeploy = ACT_VM_UNDEPLOY_5;
		break;
	case 4:
		actUndeploy = ACT_VM_UNDEPLOY_4;
		break;
	case 3:
		actUndeploy = ACT_VM_UNDEPLOY_3;
		break;
	case 2:
		actUndeploy = ACT_VM_UNDEPLOY_2;
		break;
	case 1:
		actUndeploy = ACT_VM_UNDEPLOY_1;
		break;
	case 0:
		actUndeploy = ACT_VM_UNDEPLOY_EMPTY;
		break;
	default:
		actUndeploy = ACT_VM_UNDEPLOY;
		break;
	}

	return actUndeploy;
}

Activity CWeapon30cal::GetIdleActivity( void )
{
	Activity actIdle;

	if( IsDeployed() )
	{
		switch ( m_iClip1 )
		{
		case 8:
			actIdle = ACT_VM_IDLE_DEPLOYED_8;
			break;
		case 7:
			actIdle = ACT_VM_IDLE_DEPLOYED_7;
			break;
		case 6:
			actIdle = ACT_VM_IDLE_DEPLOYED_6;
			break;
		case 5:
			actIdle = ACT_VM_IDLE_DEPLOYED_5;
			break;
		case 4:
			actIdle = ACT_VM_IDLE_DEPLOYED_4;
			break;
		case 3:
			actIdle = ACT_VM_IDLE_DEPLOYED_3;
			break;
		case 2:
			actIdle = ACT_VM_IDLE_DEPLOYED_2;
			break;
		case 1:
			actIdle = ACT_VM_IDLE_DEPLOYED_1;
			break;
		case 0:
			actIdle = ACT_VM_IDLE_DEPLOYED_EMPTY;
			break;
		default:
			actIdle = ACT_VM_IDLE_DEPLOYED;
			break;
		}
	}
	else
	{
		switch ( m_iClip1 )
		{
		case 8:
			actIdle = ACT_VM_IDLE_8;
			break;
		case 7:
			actIdle = ACT_VM_IDLE_7;
			break;
		case 6:
			actIdle = ACT_VM_IDLE_6;
			break;
		case 5:
			actIdle = ACT_VM_IDLE_5;
			break;
		case 4:
			actIdle = ACT_VM_IDLE_4;
			break;
		case 3:
			actIdle = ACT_VM_IDLE_3;
			break;
		case 2:
			actIdle = ACT_VM_IDLE_2;
			break;
		case 1:
			actIdle = ACT_VM_IDLE_1;
			break;
		case 0:
			actIdle = ACT_VM_IDLE_EMPTY;
			break;
		default:
			actIdle = ACT_VM_IDLE;
			break;
		}
	}

	return actIdle;
}

Activity CWeapon30cal::GetPrimaryAttackActivity( void )
{
	Activity actPrim;

	int maxhax = m_iClip1 + 2;

	if( IsDeployed() )
	{
		switch ( maxhax )
		{
		case 8:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_8;
			break;
		case 7:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_7;
			break;
		case 6:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_6;
			break;
		case 5:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_5;
			break;
		case 4:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_4;
			break;
		case 3:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_3;
			break;
		case 2:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_2;
			break;
		case 1:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_1;
			break;
		case 0:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED_EMPTY;
			break;
		default:
			actPrim = ACT_VM_PRIMARYATTACK_DEPLOYED;
			break;
		}
	}
	else
	{
		switch ( maxhax )
		{
		case 8:
			actPrim = ACT_VM_PRIMARYATTACK_8;
			break;
		case 7:
			actPrim = ACT_VM_PRIMARYATTACK_7;
			break;
		case 6:
			actPrim = ACT_VM_PRIMARYATTACK_6;
			break;
		case 5:
			actPrim = ACT_VM_PRIMARYATTACK_5;
			break;
		case 4:
			actPrim = ACT_VM_PRIMARYATTACK_4;
			break;
		case 3:
			actPrim = ACT_VM_PRIMARYATTACK_3;
			break;
		case 2:
			actPrim = ACT_VM_PRIMARYATTACK_2;
			break;
		case 1:
			actPrim = ACT_VM_PRIMARYATTACK_1;
			break;
		case 0:
			actPrim = ACT_VM_PRIMARYATTACK_EMPTY;
			break;
		default:
			actPrim = ACT_VM_PRIMARYATTACK;
			break;
		}
	}

	return actPrim;
}

float CWeapon30cal::GetRecoil( void )
{
	CDODPlayer *p = ToDODPlayer( GetPlayerOwner() );

	if( p && p->m_Shared.IsInMGDeploy() )
	{
		return 0.0f;
	}

	return 20;
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "order_helpers.h"
#include "order_killmortarguy.h"
#include "tf_team.h"


#define KILLMORTARGUY_DIST		3500


IMPLEMENT_SERVERCLASS_ST( COrderKillMortarGuy, DT_OrderKillMortarGuy )
END_SEND_TABLE()


static bool IsValidFn_DeployedBrianJacobsons( void *pUserData, int iClient )
{
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)UTIL_PlayerByIndex( iClient+1 );
	if ( !pPlayer || pPlayer->IsClass( TFCLASS_UNDECIDED ) || !pPlayer->GetTeam() )
		return false;

	// Is this person on an enemy team?
	CSortBase *pSortBase = (CSortBase*)pUserData;
	CBaseTFPlayer *pMyPlayer = pSortBase->m_pPlayer;

	if( pPlayer->GetTeam()->GetTeamNumber() == pMyPlayer->GetTeam()->GetTeamNumber() )
		return false;

	// Is he alive?
	if( !pPlayer->IsAlive() )
		return false;

	// ROBIN: Removed mortar object. This needs to handle mortar vehicles instead.
	// Is he looking surly?
	//if( pPlayer->GetNumObjects( OBJ_MORTAR ) == 0 )
		//return false;

	// Is he close enough?
	if( pMyPlayer->GetAbsOrigin().DistTo( pPlayer->GetAbsOrigin() ) > KILLMORTARGUY_DIST )
		return false;

	// Is he visible to the tactical?
//	if( !pMyPlayer->GetTFTeam()->IsEntityVisibleToTactical( pPlayer ) )
//		return false;

	// KILL HIM!!!
	return true;
}


static int SortFn_PlayerEntsByDistance( void *pUserData, int a, int b )
{
	CBaseEntity *pEdictA = CBaseEntity::Instance( engine->PEntityOfEntIndex( a+1 ) );
	CBaseEntity *pEdictB = CBaseEntity::Instance( engine->PEntityOfEntIndex( b+1 ) );

	if ( !pEdictA || !pEdictB )
		return 1;

	CSortBase *pSortBase = (CSortBase*)pUserData;
	const Vector &v = pSortBase->m_pPlayer->GetAbsOrigin();

	return v.DistTo( pEdictA->GetAbsOrigin() ) < v.DistTo( pEdictB->GetAbsOrigin() );
}


bool COrderKillMortarGuy::CreateOrder( CPlayerClass *pClass )
{
	CSortBase info;
	info.m_pPlayer = pClass->GetPlayer();
	
	// Look for an enemy sniper visible to the 
	int supports[MAX_PLAYERS];
	int nSupports = BuildSortedActiveList( 
		supports,							// the sorted list
		MAX_PLAYERS,					
		SortFn_PlayerEntsByDistance,		// sort on distance
		IsValidFn_DeployedBrianJacobsons,	// only get deployed support guys
		&info,								// pUserData
		gpGlobals->maxClients				// how many players to look through
		);

	// Kill the closest punk.
	if( nSupports )
	{
		CBaseTFPlayer *pBrian = (CBaseTFPlayer*)UTIL_PlayerByIndex( supports[0]+1 );
		Assert( pBrian );

		COrderKillMortarGuy *pOrder = new COrderKillMortarGuy;
		pClass->GetTeam()->AddOrder( 
			ORDER_KILL,
			pBrian,
			pClass->GetPlayer(),
			1e24,
			60,
			pOrder
			);

		return true;
	}
	else
	{
		return false;
	}
}


bool COrderKillMortarGuy::UpdateOnEvent( COrderEvent_Base *pEvent )
{
	if ( pEvent->GetType() == ORDER_EVENT_PLAYER_KILLED )
	{
		COrderEvent_PlayerKilled *pKilled = (COrderEvent_PlayerKilled*)pEvent;
		if ( pKilled->m_pPlayer == GetTargetEntity() )
			return true;
	}

	return BaseClass::UpdateOnEvent( pEvent );
}




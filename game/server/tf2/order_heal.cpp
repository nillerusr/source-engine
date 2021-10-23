//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "order_heal.h"
#include "tf_team.h"
#include "tf_playerclass.h"
#include "order_helpers.h"


#define MAX_HEAL_DIST 1500

IMPLEMENT_SERVERCLASS_ST( COrderHeal, DT_OrderHeal )
END_SEND_TABLE()


bool IsValidFn_Heal( void *pUserData, int a )
{
	// Can't heal dead players.
	CSortBase *p = (CSortBase*)pUserData;
	CBasePlayer *pPlayer = p->m_pPlayer->GetTeam()->GetPlayer( a );

	// Can't heal yourself...
	if (p->m_pPlayer == pPlayer)
		return false;

	// Don't heal players that are too far away...
	const Vector &vPlayer = p->m_pPlayer->GetAbsOrigin();
	if (vPlayer.DistToSqr(pPlayer->GetAbsOrigin()) > MAX_HEAL_DIST * MAX_HEAL_DIST )
		return false;

	return pPlayer->IsAlive() && pPlayer->m_iHealth < pPlayer->m_iMaxHealth;
}


int SortFn_Heal( void *pUserData, int a, int b )
{
	CSortBase *p = (CSortBase*)pUserData;
	
	const Vector &vPlayer = p->m_pPlayer->GetAbsOrigin();
	const Vector &va = p->m_pPlayer->GetTeam()->GetPlayer( a )->GetAbsOrigin();
	const Vector &vb = p->m_pPlayer->GetTeam()->GetPlayer( b )->GetAbsOrigin();

	return vPlayer.DistToSqr( va ) < vPlayer.DistToSqr( vb );
}


bool COrderHeal::CreateOrder( CPlayerClass *pClass )
{
	CTFTeam *pTeam = pClass->GetTeam();

	CSortBase info;
	info.m_pPlayer = pClass->GetPlayer();

	int sorted[MAX_PLAYERS];
	int nSorted = BuildSortedActiveList( 
		sorted,
		MAX_PLAYERS,
		SortFn_Heal,
		IsValidFn_Heal,
		&info,
		pTeam->GetNumPlayers()
		);

	if ( nSorted )
	{
		COrderHeal *pOrder = new COrderHeal;
		
		pClass->GetTeam()->AddOrder( 
			ORDER_HEAL, 
			pTeam->GetPlayer( sorted[0] ), 
			pClass->GetPlayer(), 
			1e24,
			60,
			pOrder );

		return true;
	}
	else
	{
		return false;
	}
}


bool COrderHeal::Update()
{
	CBaseEntity *pTarget = GetTargetEntity();
	if ( !pTarget || pTarget->m_iHealth >= pTarget->m_iMaxHealth )
		return true;

	return false;	
}


bool COrderHeal::UpdateOnEvent( COrderEvent_Base *pEvent )
{
	if ( pEvent->GetType() == ORDER_EVENT_PLAYER_KILLED )
	{
		COrderEvent_PlayerKilled *pKilled = (COrderEvent_PlayerKilled*)pEvent;
		if ( pKilled->m_pPlayer == GetTargetEntity() )
			return true;
	}

	return BaseClass::UpdateOnEvent( pEvent );
}


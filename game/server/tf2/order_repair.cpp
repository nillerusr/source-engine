//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "order_repair.h"
#include "tf_team.h"
#include "tf_class_defender.h"
#include "order_helpers.h"
#include "tf_obj.h"


IMPLEMENT_SERVERCLASS_ST( COrderRepair, DT_OrderRepair )
END_SEND_TABLE()


static int SortFn_Defender( void *pUserData, int a, int b )
{
	CSortBase *p = (CSortBase*)pUserData;
	
	const Vector &vOrigin1 = p->m_pPlayer->GetTFTeam()->GetObject( a )->GetAbsOrigin();
	const Vector &vOrigin2 = p->m_pPlayer->GetTFTeam()->GetObject( b )->GetAbsOrigin();

	return p->m_pPlayer->GetAbsOrigin().DistTo( vOrigin1 ) < p->m_pPlayer->GetAbsOrigin().DistTo( vOrigin2 );
}


static bool IsValidFn_RepairFriendlyObjects( void *pUserData, int a )
{
	// Only pick objects that are damaged.
	CSortBase *p = (CSortBase*)pUserData;
	CBaseObject *pObj = p->m_pPlayer->GetTFTeam()->GetObject( a );

	// Skip objects under construction
	if ( pObj->IsBuilding() )
		return false;

	return ( pObj->m_iHealth < pObj->m_iMaxHealth );
}


static bool IsValidFn_RepairOwnObjects( void *pUserData, int a )
{
	// Only pick objects that are damaged.
	CSortBase *pSortBase = (CSortBase*)pUserData;
	CBaseObject *pObj = pSortBase->m_pPlayer->GetObject(a);

	// Skip objects under construction
	if ( !pObj || pObj->IsBuilding() )
		return false;

	return pObj->m_iHealth < pObj->m_iMaxHealth;
}


bool COrderRepair::CreateOrder_RepairFriendlyObjects( CPlayerClassDefender *pClass )
{
	if( !pClass->CanBuildSentryGun() )
		return false;

	CBaseTFPlayer *pPlayer = pClass->GetPlayer();
	CTFTeam *pTeam = pClass->GetTeam();

	// Sort the list and filter out fully healed objects..
	CSortBase info;
	info.m_pPlayer = pPlayer;

	int sorted[MAX_TEAM_OBJECTS];
	int nSorted = BuildSortedActiveList(
		sorted,
		MAX_TEAM_OBJECTS,
		SortFn_Defender,
		IsValidFn_RepairFriendlyObjects,
		&info,
		pTeam->GetNumObjects() );

	// If the player is close enough to the closest damaged object, issue an order.	
	if( nSorted )
	{
		CBaseObject *pObjToHeal = pTeam->GetObject( sorted[0] );

		static float flClosestDist = 1024;
		if( pPlayer->GetAbsOrigin().DistTo( pObjToHeal->GetAbsOrigin() ) < flClosestDist )
		{
			COrder *pOrder = new COrderRepair;

			pTeam->AddOrder( 
				ORDER_REPAIR,
				pObjToHeal,
				pPlayer,
				1e24,
				60,
				pOrder
				);
			
			return true;
		}
	}

	return false;
}


bool COrderRepair::CreateOrder_RepairOwnObjects( CPlayerClass *pClass )
{
	CSortBase info;
	info.m_pPlayer = pClass->GetPlayer();

	int sorted[16];
	int nSorted = BuildSortedActiveList(
		sorted,
		sizeof( sorted ) / sizeof( sorted[0] ),
		SortFn_PlayerObjectsByDistance,
		IsValidFn_RepairOwnObjects,
		&info,
		info.m_pPlayer->GetObjectCount() );

	if( nSorted )
	{
		// Make an order to repair the closest damaged object.
		CBaseObject *pObj = info.m_pPlayer->GetObject( sorted[0] );
		if (!pObj)
			return false;

		COrderRepair *pOrder = new COrderRepair;
		info.m_pPlayer->GetTFTeam()->AddOrder( 
			ORDER_REPAIR,
			pObj,
			info.m_pPlayer,
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


bool COrderRepair::Update()
{
	CBaseEntity *pEnt = GetTargetEntity();
	if( !pEnt )
		return true;

	// Kill the order when the object is repaired.
	return pEnt->m_iHealth >= pEnt->m_iMaxHealth;
}



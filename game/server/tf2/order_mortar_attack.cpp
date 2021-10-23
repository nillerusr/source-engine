//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "order_mortar_attack.h"
#include "tf_team.h"
#include "order_helpers.h"
#include "tf_obj.h"


// How far away the escort guy will get orders to shell enemy objects.
#define ENEMYOBJ_MORTAR_DIST		3000


IMPLEMENT_SERVERCLASS_ST( COrderMortarAttack, DT_OrderMortarAttack )
END_SEND_TABLE()


static bool IsValidFn_WithinMortarRange( void *pUserData, int a )
{
	CSortBase *p = (CSortBase*)pUserData;
	CBaseObject *pObj = p->GetTeam()->GetObject( a );
	return pObj->GetAbsOrigin().DistTo( p->m_pPlayer->GetAbsOrigin() ) < ENEMYOBJ_MORTAR_DIST;
}


bool COrderMortarAttack::CreateOrder( CPlayerClass *pClass )
{
	// Look for some nearby enemy objects that would be fun to destroy.
	CTFTeam *pEnemyTeam;
	if ( !pClass->GetTeam() || (pEnemyTeam = pClass->GetTeam()->GetEnemyTeam()) == NULL )
		return false;

	CBaseTFPlayer *pPlayer = pClass->GetPlayer();
	
	CSortBase info;
	info.m_pPlayer = pPlayer;
	info.m_pTeam = pEnemyTeam;

	int sorted[MAX_TEAM_OBJECTS];
	int nSorted = BuildSortedActiveList(
		sorted,									// the sorted list of objects
		MAX_TEAM_OBJECTS,
		SortFn_DistanceAndConcentration,		// sort on distance and entity concentration
		IsValidFn_WithinMortarRange,			// filter function
		&info,									// user data
		pEnemyTeam->GetNumObjects()				// number of objects to check
		);

	if( nSorted > 0 )
	{
		CBaseEntity *pEnt = pEnemyTeam->GetObject( sorted[0] );

		COrderMortarAttack *pOrder = new COrderMortarAttack;

		pClass->GetTeam()->AddOrder( 
			ORDER_MORTAR_ATTACK,
			pEnt,
			pPlayer,
			1e24,
			40,
			pOrder
			);

		return true;
	}
	else
	{
		return false;
	}
}



//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "order_resupply.h"
#include "tf_team.h"
#include "tf_playerclass.h"
#include "order_helpers.h"


// Orders to build resupplies near objects come in within this range.
#define RESUPPLY_ORDER_MAXDIST		2000


IMPLEMENT_SERVERCLASS_ST( COrderResupply, DT_OrderResupply )
END_SEND_TABLE()


bool COrderResupply::CreateOrder( CPlayerClass *pClass )
{
	COrderResupply *pOrder = new COrderResupply;
	if ( OrderCreator_GenericObject( pClass, OBJ_RESUPPLY, RESUPPLY_ORDER_MAXDIST, pOrder ) )
	{
		return true;
	}
	else
	{
		UTIL_RemoveImmediate( pOrder );
		return false;
	}
}


bool COrderResupply::Update()
{
	CBaseEntity *pEnt = GetTargetEntity();
	if( !pEnt )
		return true;

	if ( !m_hOwningPlayer.Get() || m_hOwningPlayer->CanBuild( OBJ_RESUPPLY ) != CB_CAN_BUILD )
		return true;

	CTFTeam *pTeam = m_hOwningPlayer->GetTFTeam();
	if ( pTeam->GetNumResuppliesCoveringPosition( pEnt->GetAbsOrigin() ) )
		return true;

	return BaseClass::Update();	
}



//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "order_buildshieldwall.h"
#include "tf_team.h"
#include "order_helpers.h"


// The defender will get orders to cover objects with sentry guns this far away.
#define SANDBAG_ORDER_DIST		3500


IMPLEMENT_SERVERCLASS_ST( COrderBuildShieldWall, DT_OrderBuildShieldWall )
END_SEND_TABLE()


bool COrderBuildShieldWall::CreateOrder( CPlayerClass *pClass )
{
	COrderBuildShieldWall *pOrder = new COrderBuildShieldWall;
	if ( OrderCreator_GenericObject( pClass, OBJ_SHIELDWALL, 2000, pOrder ) )
	{
		return true;
	}
	else
	{
		UTIL_RemoveImmediate( pOrder );
		return false;
	}
}


bool COrderBuildShieldWall::Update()
{
	CBaseEntity *pEnt = GetTargetEntity();
	if( !pEnt )
		return true;

	if ( !m_hOwningPlayer.Get() || m_hOwningPlayer->CanBuild( OBJ_SHIELDWALL ) != CB_CAN_BUILD )
		return true;

	CTFTeam *pTeam = m_hOwningPlayer->GetTFTeam();
	if ( pTeam->GetNumShieldWallsCoveringPosition( pEnt->GetAbsOrigin() ) )
		return true;

	return false;
}

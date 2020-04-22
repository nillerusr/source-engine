//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "order_buildsentrygun.h"
#include "tf_class_defender.h"
#include "tf_team.h"
#include "order_helpers.h"


// The defender will get orders to cover objects with sentry guns this far away.
#define SENTRYGUN_ORDER_DIST		3500


IMPLEMENT_SERVERCLASS_ST( COrderBuildSentryGun, DT_OrderBuildSentryGun )
END_SEND_TABLE()


bool COrderBuildSentryGun::CreateOrder( CPlayerClassDefender *pClass )
{
	if ( !pClass->CanBuildSentryGun() )
		return false;

	COrderBuildSentryGun *pOrder = new COrderBuildSentryGun;
	if ( OrderCreator_GenericObject( pClass, OBJ_SENTRYGUN_PLASMA, SENTRYGUN_ORDER_DIST, pOrder ) )
	{
		return true;
	}
	else
	{
		UTIL_RemoveImmediate( pOrder );
		return false;
	}
}


bool COrderBuildSentryGun::Update()
{
	// If the entity we were supposed to cover with the sentry gun is covered now,
	// then the order is done.
	CBaseEntity *pEnt = GetTargetEntity();
	if( !pEnt || !m_hOwningPlayer.Get() )
		return true;
	
	CTFTeam *pTeam = m_hOwningPlayer->GetTFTeam();
	if( pTeam->IsCoveredBySentryGun( pEnt->GetAbsOrigin() ) )
		return true;

	return false;
}


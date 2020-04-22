//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "order_resourcepump.h"
#include "tf_team.h"
#include "tf_obj_resourcepump.h"
#include "tf_func_resource.h"
#include "order_helpers.h"


IMPLEMENT_SERVERCLASS_ST( COrderResourcePump, DT_OrderResourcePump )
END_SEND_TABLE()


bool COrderResourcePump::CreateOrder( CPlayerClass *pClass )
{
	COrderResourcePump *pOrder = new COrderResourcePump;

	if ( OrderCreator_ResourceZoneObject( pClass->GetPlayer(), OBJ_RESOURCEPUMP, pOrder ) )
	{
		return true;
	}
	else
	{
		UTIL_RemoveImmediate( pOrder );
		return false;
	}
}


bool COrderResourcePump::Update()
{
	// Can they still build resource pumps?
	if ( !m_hOwningPlayer.Get() || m_hOwningPlayer->CanBuild( OBJ_RESOURCEPUMP ) != CB_CAN_BUILD )
		return true;

	// Lost our resource zone?
	if ( !GetTargetEntity() )
		return true;
	// Is our target zone now empty?
	if ( ((CResourceZone*)GetTargetEntity())->IsEmpty() )
		return true;

	// Have they built a pump on this zone?
	for( int i=0; i < m_hOwningPlayer->GetObjectCount(); i++ )
	{
		CBaseObject *pObj = m_hOwningPlayer->GetObject(i);

		if( pObj && pObj->GetType() == OBJ_RESOURCEPUMP )
		{
			CObjectResourcePump *pPump = (CObjectResourcePump*)pObj;
			CResourceZone *pZone = pPump->GetResourceZone();
			if( pZone && pZone->entindex() == m_iTargetEntIndex && !pZone->IsEmpty() )
				return true;
		}
	}

	return BaseClass::Update();
}



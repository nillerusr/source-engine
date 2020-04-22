//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "order_player.h"


IMPLEMENT_SERVERCLASS_ST( COrderPlayer, DT_OrderPlayer )
END_SEND_TABLE()



bool COrderPlayer::UpdateOnEvent( COrderEvent_Base *pEvent )
{
	// All player orders give up if the player disconnects
	if ( pEvent->GetType() == ORDER_EVENT_PLAYER_DISCONNECTED )
		return true;

	return BaseClass::UpdateOnEvent( pEvent );
}


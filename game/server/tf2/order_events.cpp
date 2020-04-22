//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//


#include "cbase.h"

#include "order_events.h"
#include "tf_team.h"

//-----------------------------------------------------------------------------
// Purpose: Fire an event for all teams telling them to update their orders
//-----------------------------------------------------------------------------
void GlobalOrderEvent( COrderEvent_Base *pOrder )
{
	// Loop through the teams
	for ( int i = 0; i < GetNumberOfTeams(); i++ )
	{
		CTFTeam *pTeam = GetGlobalTFTeam( i );
		pTeam->UpdateOrdersOnEvent( pOrder );
	}
}

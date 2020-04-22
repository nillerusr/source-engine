//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: DOD's custom CPlayerResource
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "dod_player.h"
#include "player_resource.h"
#include "dod_player_resource.h"
#include <coordsize.h>

// Datatable
IMPLEMENT_SERVERCLASS_ST(CDODPlayerResource, DT_DODPlayerResource)
	SendPropArray3( SENDINFO_ARRAY3(m_iObjScore), SendPropInt( SENDINFO_ARRAY(m_iObjScore), 12 ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_iPlayerClass), SendPropInt( SENDINFO_ARRAY(m_iPlayerClass), 4 ) ),
END_SEND_TABLE()

BEGIN_DATADESC( CDODPlayerResource )
	// DEFINE_ARRAY( m_iObjScore, FIELD_INTEGER, MAX_PLAYERS+1 ),
	// DEFINE_ARRAY( m_iPlayerClass, FIELD_INTEGER, MAX_PLAYERS+1 ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( dod_player_manager, CDODPlayerResource );

CDODPlayerResource::CDODPlayerResource( void )
{
	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDODPlayerResource::UpdatePlayerData( void )
{
	int i;

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CDODPlayer *pPlayer = (CDODPlayer*)UTIL_PlayerByIndex( i );
		
		if ( pPlayer && pPlayer->IsConnected() )
		{
			m_iObjScore.Set( i, pPlayer->GetScore() );
			m_iPlayerClass.Set( i, pPlayer->m_Shared.PlayerClass() );
		}
	}

	BaseClass::UpdatePlayerData();
}

void CDODPlayerResource::Spawn( void )
{
	int i;

	for ( i=0; i < MAX_PLAYERS+1; i++ )
	{
		m_iObjScore.Set( i, 0 );
		m_iPlayerClass.Set( i, PLAYERCLASS_UNDEFINED );
	}

	BaseClass::Spawn();
}

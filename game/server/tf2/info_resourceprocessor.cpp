//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A team's resource processor back at their base
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_team.h"
#include "info_resourceprocessor.h"
#include "tf_player.h"
#include "npc_minicarrier.h"
#include "tf_gamerules.h"

BEGIN_DATADESC( CResourceProcessor )

	// functions
	DEFINE_FUNCTION( ProcessorTouch ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CResourceProcessor, DT_ResourceProcessor)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( info_resourceprocessor, CResourceProcessor);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceProcessor::Spawn( void )
{
	Precache();

	SetSolid( SOLID_SLIDEBOX );
	SetMoveType( MOVETYPE_NONE );
	AddFlag( FL_NOTARGET );

	UTIL_SetSize( pev, Vector(-32,-32,0), Vector(32,32, 128) );
	SetModel( "models/objects/obj_resourceprocessor.mdl" );
	SetTouch( ProcessorTouch );

	m_flHackSpawnHeight = 256;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceProcessor::Precache( void )
{
	PrecacheModel( "models/objects/obj_resourceprocessor.mdl" );

	UTIL_PrecacheOther( "npc_minicarrier" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceProcessor::Activate( void )
{
	BaseClass::Activate();
	if ( GetTeamNumber() < 0 || GetTeamNumber() >= GetNumberOfTeams() )
	{
		Warning( "Warning, info_resourceprocessor with invalid Team Number set.\n" );
		UTIL_Remove( this );
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CResourceProcessor::ProcessorTouch( CBaseEntity *pOther )
{
	// Players 
	if ( pOther->IsPlayer() )
	{
		// Ignore touches from enemy players
		if ( !InSameTeam( pOther ) )
			return;

		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)pOther;

		// Remove all the player's resources and add them to the team's stash
		for ( int i = 0; i < MAX_RESOURCE_TYPES; i++ )
		{
			int iCount = pPlayer->GetResourceChunkCount(i, false);
			if ( iCount )
			{
				pPlayer->RemoveResourceChunks( i, iCount, false );
				AddResources( i, iCount * CHUNK_RESOURCE_VALUE );
			}

			// Now remove processed versions too
			iCount = pPlayer->GetResourceChunkCount(i, true);
			if ( iCount )
			{
				pPlayer->RemoveResourceChunks( i, iCount, true );
				AddResources( i, iCount * PROCESSED_CHUNK_RESOURCE_VALUE );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add resources to the processor, and hence the team's bank
//-----------------------------------------------------------------------------
void CResourceProcessor::AddResources( int iResourceType, float flResources )
{
	if ( !GetTeam() )
		return;

	((CTFTeam *)GetTeam())->AddResources( iResourceType, flResources );
	((CTFTeam *)GetTeam())->ResourceLoadDeposited();
}


//-----------------------------------------------------------------------------
// Purpose: Spawn a minicarrier
//-----------------------------------------------------------------------------
void CResourceProcessor::SpawnMiniCarrier( void )
{
	CNPC_MiniCarrier *pMiniCarrier = (CNPC_MiniCarrier*)CreateEntityByName( "npc_minicarrier" );
	pMiniCarrier->Spawn();
	pMiniCarrier->ChangeTeam( m_iTeamNumber );

	// Find a clear spot near me & spawn in it
	if ( !EntityPlacementTest( pMiniCarrier, GetAbsOrigin() + Vector(0,0,m_flHackSpawnHeight), 
			pMiniCarrier->GetAbsOrigin(), false ) )
	{
		Warning( "Failed to find empty space to spawn a minicarrier.\n" );
		( ( CTFTeam * )GetTeam() )->RemoveRobot( pMiniCarrier );
		UTIL_Remove( pMiniCarrier );
		return;
	}

	m_flHackSpawnHeight += 64;

	engine->SetOrigin( pMiniCarrier->pev, pMiniCarrier->GetOrigin() );
	pMiniCarrier->SetHomeProcessor( this );
}

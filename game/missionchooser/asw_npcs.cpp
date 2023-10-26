//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// 
//
//===============================================================================

#include "asw_npcs.h"
#include "asw_mission_chooser.h"
#include "TileSource/MapLayout.h"
#include "TileSource/Room.h"
#include "TileSource/RoomTemplate.h"
#include "KeyValues.h"
#include "asw_spawn_selection.h"
#include "layout_system/tilegen_layout_system.h"

ConVar asw_encounters_distance_min( "asw_encounters_distance_min", "900", FCVAR_CHEAT, "Min distance between edges of encounter circles" );
ConVar asw_encounter_radius_min( "asw_encounter_radius_min", "384.0f", FCVAR_CHEAT );
ConVar asw_encounter_radius_max( "asw_encounter_radius_max", "512.0f", FCVAR_CHEAT );

// TODO: Parameters exposed to level designer?

void CASWMissionChooserNPCs::InitFixedSpawns( CLayoutSystem *pLayoutSystem, CMapLayout *pLayout )
{
	// init the spawn set for this mission
	KeyValues *pGenerationOptions = pLayout->GetGenerationOptions();
	if ( !pGenerationOptions )
	{
		Warning( "Error placed fixed alien spawns, no generation options in this layout." );
		return;
	}

	bool bChosenSpawnSet = false;
	const char *szNamedSpawnSet = pGenerationOptions->GetString( "AlienSpawnSet" );
	if ( szNamedSpawnSet && szNamedSpawnSet[0] )
	{
		bChosenSpawnSet = SpawnSelection()->SetCurrentSpawnSet( szNamedSpawnSet );
	}

	if ( !bChosenSpawnSet )
	{
		SpawnSelection()->SetCurrentSpawnSet( pGenerationOptions->GetInt( "Difficulty", 5 ) );
	}

	// if we have any rooms with the alien encounter tag, then just use those for fixed spawn locations
	bool bAlienEncounterTag = false;
	int iRooms = pLayout->m_PlacedRooms.Count();
	for ( int i = 0; i < iRooms; i++ )
	{
		CRoom *pRoom = pLayout->m_PlacedRooms[i];
		if ( pRoom && pRoom->m_pRoomTemplate && pRoom->m_pRoomTemplate->HasTag( "AlienEncounter" ) )
		{
			bAlienEncounterTag = true;

			CASW_Encounter *pEncounter = new CASW_Encounter();

			// pick a random spot in this room
			Vector vecWorldMins, vecWorldMaxs;
			pRoom->GetWorldBounds( &vecWorldMins, &vecWorldMaxs );
			Vector vecPos = vecWorldMins + pLayoutSystem->GetRandomFloat( 0, 1 ) * ( vecWorldMaxs - vecWorldMins );
			vecPos.z = 0;
			pEncounter->SetEncounterPosition( vecPos );
			pEncounter->SetEncounterRadius( pLayoutSystem->GetRandomFloat( asw_encounter_radius_min.GetFloat(), asw_encounter_radius_max.GetFloat() ) );

			// add spawn defs
			// TODO: more spawns in bigger rooms? or rooms with higher weights?
			int iSpawnsPerEncounter = pLayoutSystem->GetRandomInt( CurrentSpawnSet()->GetMinSpawnsPerEncounter(), CurrentSpawnSet()->GetMaxSpawnsPerEncounter() );
			for ( int i = 0; i < iSpawnsPerEncounter; i++ )
			{
				CASW_Spawn_Definition* pSpawnDef = CurrentSpawnSet()->GetSpawnDef( ASW_NPC_SPAWN_TYPE_FIXED );
				if ( !pSpawnDef )
					continue;
				pEncounter->AddSpawnDef( pSpawnDef );
			}

			pLayout->m_Encounters.AddToTail( pEncounter );
		}
	}

	if ( bAlienEncounterTag )
	{
		pLayout->MarkEncounterRooms();
		return;
	}

	// find area of the mission
	int iTotalArea = 0;
	for ( int i = 0; i < iRooms; i++ )
	{
		CRoom *pRoom = pLayout->m_PlacedRooms[i];
		iTotalArea += ( pRoom->m_pRoomTemplate->GetTilesX() * ASW_TILE_SIZE ) * ( pRoom->m_pRoomTemplate->GetTilesY() * ASW_TILE_SIZE );
	}

	// decide how many encounters we want
	int iEncounters = pLayoutSystem->GetRandomInt( CurrentSpawnSet()->GetMinEncounters(), CurrentSpawnSet()->GetMaxEncounters() );

	// distance between encounters
	//float flMinDistance = asw_encounters_distance_min.GetFloat();

	// randomly pick rooms for the encounters to be in, using the room weights
	CUtlVector<CRoom*> candidates;
	float flTotalWeight = 0;
	for ( int i = 0; i < iRooms; i++ )
	{
		CRoom *pRoom = pLayout->m_PlacedRooms[i];
		if ( pRoom->GetSpawnWeight() > 0
				&& !pRoom->m_pRoomTemplate->IsEscapeRoom() 
				&& !pRoom->m_pRoomTemplate->IsStartRoom() 
				&& !pRoom->m_pRoomTemplate->IsBorderRoom() )
		{
			flTotalWeight += pRoom->GetSpawnWeight();
			candidates.AddToTail( pRoom );
		}
	}

	for ( int e = 0; e < iEncounters; e++ )
	{
		float flChosen = pLayoutSystem->GetRandomFloat( 0, flTotalWeight );
		CRoom *pChosenRoom = NULL;
		for ( int i = 0; i < candidates.Count(); i++ )
		{
			CRoom *pRoom = candidates[i];
			flChosen -= pRoom->GetSpawnWeight();
			if ( flChosen <= 0 )
			{
				pChosenRoom = pRoom;
				break;
			}
		}

		if ( !pChosenRoom )
			continue;

		CASW_Encounter *pEncounter = new CASW_Encounter();

		// pick a random spot in this room
		Vector vecWorldMins, vecWorldMaxs;
		pChosenRoom->GetWorldBounds( &vecWorldMins, &vecWorldMaxs );
		//Vector vecPos = vecWorldMins + pLayoutSystem->GetRandomFloat( 0, 1 ) * ( vecWorldMaxs - vecWorldMins );
		Vector vecPos = ( vecWorldMins + vecWorldMaxs ) * 0.5f; // center of the room
		vecPos.z = 0;
		pEncounter->SetEncounterPosition( vecPos );
		pEncounter->SetEncounterRadius( pLayoutSystem->GetRandomFloat( asw_encounter_radius_min.GetFloat(), asw_encounter_radius_max.GetFloat() ) );

		// add spawn defs
		// TODO: more spawns in bigger rooms? or rooms with higher weights?
		int iSpawnsPerEncounter = pLayoutSystem->GetRandomInt( CurrentSpawnSet()->GetMinSpawnsPerEncounter(), CurrentSpawnSet()->GetMaxSpawnsPerEncounter() );
		for ( int i = 0; i < iSpawnsPerEncounter; i++ )
		{
			CASW_Spawn_Definition* pSpawnDef = CurrentSpawnSet()->GetSpawnDef( ASW_NPC_SPAWN_TYPE_FIXED );
			if ( !pSpawnDef )
				continue;
			pEncounter->AddSpawnDef( pSpawnDef );
		}

		pLayout->m_Encounters.AddToTail( pEncounter );
	}

	PushEncountersApart( pLayout );
	pLayout->MarkEncounterRooms();
}

void CASWMissionChooserNPCs::PushEncountersApart( CMapLayout *pLayout )
{
	// get a list of valid rooms
	int iRooms = pLayout->m_PlacedRooms.Count();
	CUtlVector<CRoom*> candidates;
	float flTotalWeight = 0;
	for ( int i = 0; i < iRooms; i++ )
	{
		CRoom *pRoom = pLayout->m_PlacedRooms[i];
		if ( pRoom->GetSpawnWeight() > 0
			&& !pRoom->m_pRoomTemplate->IsEscapeRoom() 
			&& !pRoom->m_pRoomTemplate->IsStartRoom() 
			&& !pRoom->m_pRoomTemplate->IsBorderRoom() )
		{
			// skip 1x1 rooms
			if ( pRoom->m_pRoomTemplate->GetTilesX() * pRoom->m_pRoomTemplate->GetTilesY() <= 1 )
				continue;

			flTotalWeight += pRoom->GetSpawnWeight();
			candidates.AddToTail( pRoom );
		}
	}

	// push the encounters apart 
	int iSteps = 6;
	for ( int i = 0; i < iSteps; i++ )
	{
		int iEncounters = pLayout->m_Encounters.Count();
		for ( int e = 0; e < iEncounters; e++ )
		{
			Vector vecPush = vec3_origin;
			CASW_Encounter *pEncounter = pLayout->m_Encounters[ e ];
			// accumulate a force from all other nearby encounters
			for ( int k = 0; k < iEncounters; k++ )
			{
				if ( e == k )
					continue;
				CASW_Encounter *pOther = pLayout->m_Encounters[ k ];
				Vector dir = pEncounter->GetEncounterPosition() - pOther->GetEncounterPosition();	// direction from other to us
				float dist = dir.NormalizeInPlace();
				float flMinDistance = asw_encounters_distance_min.GetFloat() + pEncounter->GetEncounterRadius() + pOther->GetEncounterRadius();
				if ( dist < flMinDistance )
				{
					float flPush = flMinDistance - dist;		// push us exactly out of the min
					vecPush += dir * flPush;
				}
			}
			pEncounter->SetEncounterPosition( pEncounter->GetEncounterPosition() + vecPush );

			// clamp encounters to room bounds
			CRoom *pClosestRoom = NULL;
			float flClosestDist = 65535.0f;
			bool bWithinARoom = false;
			Vector pos = pEncounter->GetEncounterPosition();
			for ( int k = 0; k < candidates.Count(); k++ )
			{
				CRoom *pRoom = candidates[k];
				Vector vecMins, vecMaxs;
				pRoom->GetWorldBounds( &vecMins, &vecMaxs );
				if ( pos.x >= vecMins.x && pos.x <= vecMaxs.x
								&& pos.y >= vecMins.y && pos.y <= vecMaxs.y )
				{
					bWithinARoom = true;
					break;
				}
				Vector vecMiddle = ( vecMins + vecMaxs ) * 0.5f;
				float flDist = vecMiddle.DistTo( pos );
				if ( flDist < flClosestDist )
				{
					flClosestDist = flDist;
					pClosestRoom = pRoom;
				}
			}
			// if encounter wasn't in any room, then clamp to the closest
			if ( !bWithinARoom && pClosestRoom )
			{
				Vector vecMins, vecMaxs;
				pClosestRoom->GetWorldBounds( &vecMins, &vecMaxs );
				pos.x = clamp( pos.x, vecMins.x, vecMaxs.x );
				pos.y = clamp( pos.y, vecMins.y, vecMaxs.y );
				pEncounter->SetEncounterPosition( pos );
			}

			/*
			// put encounter in the center of its room
			for ( int k = 0; k < candidates.Count(); k++ )
			{
				CRoom *pRoom = candidates[k];
				Vector vecMins, vecMaxs;
				pRoom->GetWorldBounds( &vecMins, &vecMaxs );
				if ( pos.x >= vecMins.x && pos.x <= vecMaxs.x
					&& pos.y >= vecMins.y && pos.y <= vecMaxs.y )
				{
					Vector mid = ( vecMins + vecMaxs ) * 0.5f;
					mid.z = 0;
					pEncounter->SetEncounterPosition ( mid );
					break;
				}
			}
			*/
		}
	}
}
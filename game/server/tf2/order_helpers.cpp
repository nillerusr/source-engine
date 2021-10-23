//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "order_helpers.h"
#include "tf_team.h"
#include "tf_func_resource.h"
#include "tf_obj.h"


// ------------------------------------------------------------------------ //
// CSortBase implementation.
// ------------------------------------------------------------------------ //

CSortBase::CSortBase()
{
	m_pPlayer = 0;
	m_pTeam = 0;
}


CTFTeam* CSortBase::GetTeam()
{
	if ( m_pTeam )
		return m_pTeam;
	else
		return m_pPlayer->GetTFTeam();
}



// ------------------------------------------------------------------------ //
// Global functions.
// ------------------------------------------------------------------------ //

int SortFn_TeamPlayersByDistance( void *pUserData, int a, int b )
{
	CSortBase *p = (CSortBase*)pUserData;
	
	const Vector &vPlayer = p->m_pPlayer->GetAbsOrigin();
	const Vector &va = p->m_pPlayer->GetTeam()->GetPlayer( a )->GetAbsOrigin();
	const Vector &vb = p->m_pPlayer->GetTeam()->GetPlayer( b )->GetAbsOrigin();

	return vPlayer.DistTo( va ) < vPlayer.DistTo( vb );
}


// This is a generic function that takes a number of items and builds a sorted
// list of the valid items.
int BuildSortedActiveList( 
	int *pList,		// This is the list where the final data is placed.
	int nMaxItems, 
	sortFn pSortFn,			// Callbacks.
	isValidFn pIsValidFn,	// This can be null, in which case all items are valid.
	void *pUserData,		// Passed into the function pointers.
	int nItems				// Number of items in the list to sort.
	)
{
	// First build the list of active items.
	if( nItems > nMaxItems )
		nItems = nMaxItems;

	int nActive = 0;
	for( int i=0; i < nItems; i++ )
	{
		if( pIsValidFn )
		{
			if( !pIsValidFn( pUserData, i ) )
				continue;
		}

		int j;
		for( j=0; j < nActive; j++ )
		{
			Assert( pList[j] < nItems );
			if( pSortFn( pUserData, i, pList[j] ) > 0 )
			{
				break;
			}
		}

		// Slide everything up.
		if( nActive )
		{
			Q_memmove( &pList[j+1], &pList[j], (nActive-j) * sizeof(int) );
		}

		// Add the new item to the list.
		pList[j] = i;
		++nActive;

		for (int l = 0; l < nActive ; ++l )
		{
			Assert( pList[l] < nItems );
		}

	}

	return nActive;
}

	
// Finds the closest resource zone without the specified object on it and
// gives an order to the player to build the object.
bool OrderCreator_ResourceZoneObject( 
	CBaseTFPlayer *pPlayer, 
	int objType,
	COrder *pOrder
	)
{
	// Can we even build a resource box?
	if ( pPlayer->CanBuild( objType ) != CB_CAN_BUILD )
		return false;

	CTFTeam *pTeam = pPlayer->GetTFTeam();
	if( !pTeam )
		return false;

	// Let's have one near each resource zone that we own.
	CResourceZone *pClosest = 0;
	float flClosestDist = 100000000;

	CBaseEntity *pEntity = NULL;
	while( (pEntity = gEntList.FindEntityByClassname( pEntity, "trigger_resourcezone" )) != NULL )
	{
		CResourceZone *pZone = (CResourceZone*)pEntity;
		
		// Ignore empty zones and zones not captured by this team.
		if ( pZone->IsEmpty() || !pZone->GetActive() )
			continue;
		
		Vector vZoneCenter = pZone->WorldSpaceCenter();

		// Look for a resource pump on this zone.
		bool bPump = objType == OBJ_RESOURCEPUMP && pPlayer->NumPumpsOnResourceZone( pZone ) == 0;
		if ( bPump )
		{
			// Make sure it's their preferred tech.
			float flTestDist = pPlayer->GetAbsOrigin().DistTo( vZoneCenter );
			if ( flTestDist < flClosestDist )
			{
				pClosest = pZone;
				flClosestDist = flTestDist;
			}
		}
	}

	if ( pClosest )
	{
		// No pump here. Build one!
		pPlayer->GetTFTeam()->AddOrder( 
			ORDER_BUILD,
			pClosest,
			pPlayer,
			1e24,
			60,
			pOrder
			);

		return true;
	}
	else
	{
		return false;
	}
}


int SortFn_PlayerObjectsByDistance( void *pUserData, int a, int b )
{
	CSortBase *pSortBase = (CSortBase*)pUserData;
	
	CBaseObject* pObjA = pSortBase->m_pPlayer->GetObject(a);
	CBaseObject* pObjB = pSortBase->m_pPlayer->GetObject(b);
	if (!pObjA)
		return false;
	if (!pObjB)
		return true;

	const Vector &v = pSortBase->m_pPlayer->GetAbsOrigin();

	return v.DistTo( pObjA->GetAbsOrigin() ) < v.DistTo( pObjB->GetAbsOrigin() );
}


int SortFn_TeamObjectsByDistance( void *pUserData, int a, int b )
{
	CSortBase *pSortBase = (CSortBase*)pUserData;
	
	CBaseObject *pObj1 = pSortBase->m_pPlayer->GetTFTeam()->GetObject( a );
	CBaseObject *pObj2 = pSortBase->m_pPlayer->GetTFTeam()->GetObject( b );
	const Vector &v = pSortBase->m_pPlayer->GetAbsOrigin();

	return v.DistTo( pObj1->GetAbsOrigin() ) < v.DistTo( pObj2->GetAbsOrigin() );
}


int SortFn_PlayerEntitiesByDistance( void *pUserData, int a, int b )
{
	CSortBase *pSortBase = (CSortBase*)pUserData;
	
	CBaseEntity *pObj1 = CBaseEntity::Instance( engine->PEntityOfEntIndex( a+1 ) );
	CBaseEntity *pObj2 = CBaseEntity::Instance( engine->PEntityOfEntIndex( b+1 ) );
	const Vector &v = pSortBase->m_pPlayer->GetAbsOrigin();

	return v.DistTo( pObj1->GetAbsOrigin() ) < v.DistTo( pObj2->GetAbsOrigin() );
}


int SortFn_DistanceAndConcentration( void *pUserData, int a, int b )
{
	CSortBase *p = (CSortBase*)pUserData;
	
	// Compare distances. Each rope attachment to another ent 
	// subtracts 200 inches, so the order is biased towards covering
	// groups of objects together.
	CBaseObject *pObjectA = p->GetTeam()->GetObject( a );
	CBaseObject *pObjectB = p->GetTeam()->GetObject( b );

	const Vector &vOrigin1 = pObjectA->GetAbsOrigin();
	const Vector &vOrigin2 = p->GetTeam()->GetObject( b )->GetAbsOrigin();

	float flScore1 = -p->m_pPlayer->GetAbsOrigin().DistTo( vOrigin1 );
	float flScore2 = -p->m_pPlayer->GetAbsOrigin().DistTo( vOrigin2 );

	flScore1 += pObjectA->RopeCount() * 200;
	flScore2 += pObjectB->RopeCount() * 200;

	return flScore1 > flScore2;
}


bool IsValidFn_NearAndNotCovered( void *pUserData, int a )
{
	CSortBase *p = (CSortBase*)pUserData;
	CBaseObject *pObj = p->m_pPlayer->GetTFTeam()->GetObject( a );

	// Is the object too far away to be covered?
	if ( p->m_pPlayer->GetAbsOrigin().DistTo( pObj->GetAbsOrigin() ) > p->m_flMaxDist )
		return false;

	// Don't cover certain entities (like sentry guns, sand bags, etc).
	switch( p->m_ObjectType )
	{
		case OBJ_SENTRYGUN_PLASMA:
		{
			if ( !pObj->WantsCoverFromSentryGun() )
				return false;

			if ( p->m_pPlayer->GetTFTeam()->IsCoveredBySentryGun( pObj->GetAbsOrigin() ) )
				return false;
		}
		break;
		
		case OBJ_SHIELDWALL:
		{
			if ( !pObj->WantsCover() )
				return false;

			if ( p->m_pPlayer->GetTFTeam()->GetNumShieldWallsCoveringPosition( pObj->GetAbsOrigin() ) )
				return false;
		}
		break;

		case OBJ_RESUPPLY:
		{
			if ( p->m_pPlayer->GetTFTeam()->GetNumResuppliesCoveringPosition( pObj->GetAbsOrigin() ) )
				return false;
		}
		break;

		case OBJ_RESPAWN_STATION:
		{
			if ( p->m_pPlayer->GetTFTeam()->GetNumRespawnStationsCoveringPosition( pObj->GetAbsOrigin() ) )
				return false;
		}
		break;

		default:
		{
			Assert( !"Unsupported object type" );
		}
		break;
	}

	return true;
}


bool OrderCreator_GenericObject( 
	CPlayerClass *pClass, 
	int objectType, 
	float flMaxDist,
	COrder *pOrder
	)
{
	// Can we build one?
	if ( pClass->CanBuild( objectType ) != CB_CAN_BUILD )
		return false;

	CBaseTFPlayer *pPlayer = pClass->GetPlayer();
	CTFTeam *pTeam = pClass->GetTeam();

	// Sort nearby objects.
	CSortBase info;
	info.m_pPlayer = pPlayer;
	info.m_flMaxDist = flMaxDist;
	info.m_ObjectType = objectType;

	int sorted[MAX_TEAM_OBJECTS];
	int nSorted = BuildSortedActiveList(
		sorted,									// the sorted list of objects
		MAX_TEAM_OBJECTS,
		SortFn_DistanceAndConcentration,		// sort on distance and entity concentration
		IsValidFn_NearAndNotCovered,			// filter function
		&info,									// user data
		pTeam->GetNumObjects()					// number of objects to check
		);

	if( nSorted )
	{
		// Ok, make an order to cover the closest object with a sentry gun.
		CBaseEntity *pEnt = pTeam->GetObject( sorted[0] );

		pTeam->AddOrder( 
			ORDER_BUILD,
			pEnt,
			pPlayer,
			flMaxDist,
			60,
			pOrder
			);

		return true;
	}
	else
	{
		return false;
	}
}

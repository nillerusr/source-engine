//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ORDER_HELPERS_H
#define ORDER_HELPERS_H
#ifdef _WIN32
#pragma once
#endif


#include "orders.h"


class CTFTeam;


class CSortBase
{
public:

					CSortBase();

	// Returns m_pTeam, and if that doesn't exist, returns m_pPlayer->GetTFTeam().
	CTFTeam*		GetTeam();


public:
	
	CBaseTFPlayer	*m_pPlayer;

	// If this is left at null, then GetTeam() returns m_pPlayer->GetTFTeam().
	CTFTeam			*m_pTeam;
	
	// One of the OBJ_ defines telling what type of object it's thinking of building.
	int				m_ObjectType;
	
	// If the object is further from m_vPlayerOrigin than this, then an order
	// won't be generated to cover it.
	float			m_flMaxDist;
};


// Return positive if iItem1 > iItem2.
// Return negative if iItem1 < iItem2.
// Return zero if they're equal.
typedef int (*sortFn)( void *pUserData, int iItem1, int iItem2 );
typedef bool (*isValidFn)( void *pUserData, int iItem );



// Index engine->PEntityOfIndex(a+1) and b+1.
int SortFn_PlayerEntitiesByDistance( void *pUserData, int a, int b );

// Helper sort function. Sorts CSortBase::m_pPlayer's objects by distance. 
// pUserData must be a CSortBase.
int SortFn_PlayerObjectsByDistance( void *pUserData, int a, int b );

// Helper sort function. Sorts CSortBase::m_pPlayer->GetTeam()'s objects by distance. 
// pUserData must be a CSortBase.
int SortFn_TeamObjectsByDistance( void *pUserData, int a, int b );

// Sort by distance and concentation. pUserData must point at something
// derived from CSortBase.
int SortFn_DistanceAndConcentration( void *pUserData, int a, int b );

// pUserData is a CSortBase
// a and b index CSortBase::m_pPlayer->GetTeam()->GetPlayer()
// Sort players on distance.
int SortFn_TeamPlayersByDistance( void *pUserData, int a, int b );


// pUserdata is a CSortBase.
//
// Rejects the object if:
// - it's already covered by CSortBase::m_ObjectType
// - it's being ferried
// - it's further from the player than CSortBase::m_flMaxDist;
//
// This function currently supports:
//		- OBJ_SENTRYGUN_PLASMA
//		- OBJ_SANDBAG
//		- OBJ_AUTOREPAIR
//		- OBJ_SHIELDWALL
//		- OBJ_RESUPPLY
bool IsValidFn_NearAndNotCovered( void *pUserData, int a );



// This is a generic function that takes a number of items and builds a sorted
// list of the valid items.
int BuildSortedActiveList( 
	int *pList,		// This is the list where the final data is placed.
	int nMaxItems, 
	sortFn pSortFn,			// Callbacks.
	isValidFn pIsValidFn,	// This can be null, in which case all items are valid.
	void *pUserData,		// Passed into the function pointers.
	int nItems				// Number of items in the list to sort.
	);

// Finds the closest resource zone without the specified object on it and
// gives an order to the player to build the object.
// This function supports OBJ_RESOURCEBOX, OBJ_RESOURCEPUMP, and OBJ_ZONE_INCREASER.
bool OrderCreator_ResourceZoneObject( 
	CBaseTFPlayer *pPlayer, 
	int objType,
	COrder *pOrder
	);

// This function is shared by lots of the order creation functions.
// It makes an order to create a specific type of object by looking for nearby 
// concentrations of team objects that aren't "covered" by objectType.
// 
// It uses IsValidFn_NearAndNotCovered, so any object type you specify in here
// must be supported in IsValidFn_NearAndNotCovered.
bool OrderCreator_GenericObject( 
	CPlayerClass *pClass, 
	int objectType, 
	float flMaxDist,
	COrder *pOrder
	);



#endif // ORDER_HELPERS_H

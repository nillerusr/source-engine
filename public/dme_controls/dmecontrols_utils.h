//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef DMECONTROLS_UTILS_H
#define DMECONTROLS_UTILS_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/KeyValues.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"
#include "movieobjects/timeutils.h"

//-----------------------------------------------------------------------------
// Helper method to insert + extract DmElement handles into keyvalues
//-----------------------------------------------------------------------------
inline void SetElementKeyValue( KeyValues *pKeyValues, const char *pName, CDmElement *pElement )
{
	pKeyValues->SetInt( pName, pElement ? pElement->GetHandle() : DMELEMENT_HANDLE_INVALID );
}

template< class T >
T* GetElementKeyValue( KeyValues *pKeyValues, const char *pName )
{
	DmElementHandle_t h = (DmElementHandle_t)pKeyValues->GetInt( pName, DMELEMENT_HANDLE_INVALID );
	return GetElement<T>( h );
}

inline KeyValues *CreateElementKeyValues( const char *pName, const char *pKey, CDmElement *pElement )
{
	return new KeyValues( pName, pKey, pElement ? ( int )pElement->GetHandle() : DMELEMENT_HANDLE_INVALID );
}

inline void AddStandardElementKeys( KeyValues *pKeyValues, CDmElement *pElement )
{
	SetElementKeyValue( pKeyValues, "dmeelement", pElement );

	if ( pElement )
	{
		char buf[ 256 ];
		UniqueIdToString( pElement->GetId(), buf, sizeof( buf ) );
		pKeyValues->SetString( "text", buf );
		pKeyValues->SetString( "type", pElement->GetTypeString() );
	}
}


//-----------------------------------------------------------------------------
// Helper method to insert + extract DmeTime_t into keyvalues
//-----------------------------------------------------------------------------
inline void SetDmeTimeKeyValue( KeyValues *pKeyValues, const char *pName, DmeTime_t t )
{
	pKeyValues->SetInt( pName, t.GetTenthsOfMS() );
}

inline DmeTime_t GetDmeTimeKeyValue( KeyValues *pKeyValues, const char *pName, DmeTime_t defaultTime = DMETIME_ZERO )
{
	return DmeTime_t( pKeyValues->GetInt( pName, defaultTime.GetTenthsOfMS() ) );
}


inline bool ElementTree_IsArrayItem( KeyValues *itemData )
{
	return !itemData->IsEmpty( "arrayIndex" );
}

inline CDmAttribute *ElementTree_GetAttribute( KeyValues *itemData )
{
	CDmElement *pOwner = GetElementKeyValue< CDmElement >( itemData, "ownerelement" );
	if ( !pOwner )
		return NULL;

	const char *pAttributeName = itemData->GetString( "attributeName", "" );
	return pOwner->GetAttribute( pAttributeName );
}

inline DmAttributeType_t ElementTree_GetAttributeType( KeyValues *itemData )
{
	CDmElement *pOwner = GetElementKeyValue< CDmElement >( itemData, "ownerelement" );
	if ( !pOwner )
		return AT_UNKNOWN;

	const char *pAttributeName = itemData->GetString( "attributeName", "" );
	CDmAttribute *pAttribute = pOwner->GetAttribute( pAttributeName );
	if ( !pAttribute )
		return AT_UNKNOWN;

	return pAttribute->GetType();
}



inline bool ElementTree_GetDroppableItems( CUtlVector< KeyValues * >& msglist, const char *elementType, CUtlVector< CDmElement * >& list )
{
	int c = msglist.Count();
	for ( int i = 0; i < c; ++i )
	{	
		KeyValues *data = msglist[ i ];

		CDmElement *e = GetElementKeyValue<CDmElement>( data, "dmeelement" );
		if ( !e )
		{
			continue;
		}

		//if ( !e->IsA( elementType ) )
		//{
		//	continue;
		//}

		list.AddToTail( e );
	}

	return list.Count() != 0;
}

inline void ElementTree_RemoveListFromArray( CDmAttribute *pArrayAttribute, CUtlVector< CDmElement * >& list )
{
	CDmrElementArray<> array( pArrayAttribute );
	int c = array.Count();
	for ( int i = c - 1 ; i >= 0 ; --i )
	{
		CDmElement *element = array[ i ];
		if ( list.Find( element ) != list.InvalidIndex() )
		{
			array.Remove( i );
		}
	}
}

#endif // DMECONTROLS_UTILS_H

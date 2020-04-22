//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: EconItemFactory: Manages rolling for items requested by the game server
//
//=============================================================================

#ifndef ECONITEMFACTORY_H
#define ECONITEMFACTORY_H
#ifdef _WIN32
#pragma once
#endif

class CEconItem;
class CEconGameAccount;

namespace GCSDK
{
	class CGCSharedObjectCache;
}

#include "game_item_schema.h"

//-----------------------------------------------------------------------------
// CEconItemFactory
// Factory responsible for rolling random items
//-----------------------------------------------------------------------------
class CEconItemFactory
{
public:
	CEconItemFactory( );
	
	// Gets a pointer to the underlying item schema the factory is using
	GameItemSchema_t &GetSchema() { return m_schema; }

	// Create a random item based on the incoming item selection criteria
	CEconItem *CreateRandomItem( const CEconGameAccount *pGameAccount, const CItemSelectionCriteria &criteria );

	// Create an item from a specific definition index
	CEconItem *CreateSpecificItem( const CEconGameAccount *pGameAccount, item_definition_index_t unDefinitionIndex );

	CEconItem *CreateSpecificItem( GCSDK::CGCSharedObjectCache *pUserSOCache, item_definition_index_t unDefinitionIndex )
	{
		return CreateSpecificItem( pUserSOCache->GetSingleton<CEconGameAccount>(), unDefinitionIndex );
	}

	uint64 GetNextID() { Assert( m_bIsInitialized ); return m_ulNextObjID++; }

	bool BYieldingInit();
	bool BIsInitialized() { return m_bIsInitialized; }

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_schema );
	}
#endif // DBGFLAG_VALIDATE

	void							 ApplyStaticAttributeToItem( CEconItem *pItem, const static_attrib_t& staticAttrib, const CEconGameAccount *pGameAccount ) const;
	const CEconItemDefinition		*RollItemDefinition( const CItemSelectionCriteria &criteria ) const;

private:
	bool							 BAddGCGeneratedAttributesToItem( const CEconGameAccount *pGameAccount, CEconItem *pItem ) const;

private:

	// The schema this factory uses to create items
	GameItemSchema_t m_schema;

	// the next item ID to give out
	itemid_t m_ulNextObjID;
	bool m_bIsInitialized;
};

#endif //ECONITEMFACTORY_H

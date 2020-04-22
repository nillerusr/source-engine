//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: EconItemFactory: Manages rolling for items requested by the game server
//
//=============================================================================

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#include "econ/econ_assetapi_context.h"


using namespace GCSDK;

//-----------------------------------------------------------------------------
// Purpose:	Constructor
//-----------------------------------------------------------------------------
CEconItemFactory::CEconItemFactory(  )
	: m_ulNextObjID( 0 )
	, m_bIsInitialized( false )
{
}


//-----------------------------------------------------------------------------
// Purpose:	Initializes the item factory and schema. Return false if init failed
//-----------------------------------------------------------------------------
bool CEconItemFactory::BYieldingInit()
{
	CUtlVector< CUtlString > vecErrors;
	bool bRet = m_schema.BInit( "scripts/items/unencrypted/items_master.txt", "GAME", &vecErrors );

	FOR_EACH_VEC( vecErrors, nError )
	{
		EmitError( SPEW_GC, "%s\n", vecErrors[nError].Get() );
	}

	static const char *pchMaxIDQuery = "SELECT MAX( ID ) FROM "
		"( select max(ID) AS ID FROM Item UNION SELECT MAX(ID) AS ID FROM ForeignItem ) as tbl";

	CSQLAccess sqlAccess;
	if( !sqlAccess.BYieldingExecuteSingleResult<uint64, uint64>( NULL, pchMaxIDQuery, k_EGCSQLType_int64, &m_ulNextObjID, NULL ) )
	{
		EmitError( SPEW_GC, "Failed to read max item ID" );
		return false;
	}
	m_ulNextObjID++; // our next ID is one past the current max ID

	m_bIsInitialized = bRet;
	return bRet;
}

static const CEconItemQualityDefinition *GetQualityDefinitionForItemCreation( const CItemSelectionCriteria *pOptionalCriteria, const CEconItemDefinition *pItemDef )
{
	Assert( pItemDef );

	// Do we have a quality specified? If so, is it a valid quality? If not, we fall back to the
	// quality specified by the item definition, the schema, etc.
	uint8 unQuality = k_unItemQuality_Any;

	// Quality specified in generation request via criteria?
	if ( pOptionalCriteria && pOptionalCriteria->BQualitySet() )
	{
		unQuality = pOptionalCriteria->GetQuality();
	}

	// If not: quality specified in item definition?
	if ( unQuality == k_unItemQuality_Any )
	{
		unQuality = pItemDef->GetQuality();
	}

	// Final fallback: default quality in schema.
	if ( unQuality == k_unItemQuality_Any )
	{
		unQuality = GetItemSchema()->GetDefaultQuality();
	}

	AssertMsg( unQuality != k_unItemQuality_Any, "Unable to locate valid quality!" );

	return GetItemSchema()->GetQualityDefinition( unQuality );
}

//-----------------------------------------------------------------------------
// Purpose:	Creates an item matching the incoming item selection criteria
// Input:	pItem - Pointer to the item to fill in
//			criteria - The criteria that the generated item must match
// Output:	True if a matching item could be generated, false otherwise
//-----------------------------------------------------------------------------
CEconItem *CEconItemFactory::CreateRandomItem( const CEconGameAccount *pGameAccount, const CItemSelectionCriteria &criteria )
{
	// Find a matching item definition.
	const CEconItemDefinition *pItemDef = RollItemDefinition( criteria );
	if ( NULL == pItemDef )
	{
		EmitWarning( SPEW_GC, 2, "CEconItemFactory::CreateRandomItem(): Item creation request with no matching definition\n" );
		return NULL;
	}

	const CEconItemQualityDefinition *pQualityDef = GetQualityDefinitionForItemCreation( &criteria, pItemDef );
	if ( NULL == pQualityDef )
	{
		EmitWarning( SPEW_GC, 2, "CEconItemFactory::CreateRandomItem(): Item creation request with unknown quality\n" );
		return NULL;
	}

	// At this point we have everything that can fail will already have failed, so we can safely
	// create an item and just move properties over to it.
	CEconItem *pItem = new CEconItem();
	pItem->SetItemID( GetNextID() );
	pItem->SetDefinitionIndex( pItemDef->GetDefinitionIndex() );
	pItem->SetItemLevel( criteria.BItemLevelSet() ? criteria.GetItemLevel() : pItemDef->RollItemLevel() );
	pItem->SetQuality( pQualityDef->GetDBValue() );
	pItem->SetInventoryToken( criteria.GetInitialInventory() );
	pItem->SetQuantity( criteria.BInitialQuantitySet() ? criteria.GetInitialQuantity() : pItemDef->GetDefaultDropQuantity() );
	// don't set account ID
	
	// Add any custom attributes we need
	if( !BAddGCGeneratedAttributesToItem( pGameAccount, pItem ) )
	{
		delete pItem;
		EmitWarning( SPEW_GC, 2, "CEconItemFactory::CreateSpecificItem(): Failed to generate attributes\n" );
		return NULL;
	}

	return pItem;
}

//-----------------------------------------------------------------------------
// Purpose:	Creates an item based on a specific item definition index
// Input:	pItem - Pointer to the item to fill in
//			unDefinitionIndex - The definition index of the item to create
// Output:	True if a matching item could be generated, false otherwise
//-----------------------------------------------------------------------------
CEconItem *CEconItemFactory::CreateSpecificItem( const CEconGameAccount *pGameAccount, item_definition_index_t unDefinitionIndex )
{
	// Find the matching index
	const CEconItemDefinition *pItemDef = m_schema.GetItemDefinition( unDefinitionIndex );
	if ( NULL == pItemDef )
	{
		EmitWarning( SPEW_GC, 2, "CEconItemFactory::CreateSpecificItem(): Item creation request with no matching definition (def index %u)\n", unDefinitionIndex );
		return NULL;
	}

	const CEconItemQualityDefinition *pQualityDef = GetQualityDefinitionForItemCreation( NULL, pItemDef );
	if ( NULL == pQualityDef )
	{
		EmitWarning( SPEW_GC, 2, "CEconItemFactory::CreateSpecificItem(): Item creation request with unknown quality\n" );
		return NULL;
	}

	CEconItem *pItem = new CEconItem();
	if ( pGameAccount != NULL )
		pItem->SetItemID( GetNextID() );
	pItem->SetDefinitionIndex( unDefinitionIndex );
	pItem->SetItemLevel( pItemDef->RollItemLevel() );
	pItem->SetQuality( pQualityDef->GetDBValue() );
	// don't set inventory token
	pItem->SetQuantity( MAX( 1, pItemDef->GetDefaultDropQuantity() ) );

	// Startup test code calls this with a null pGameAccount. 
	if ( pGameAccount != NULL )
	{
		pItem->SetAccountID( pGameAccount->Obj().m_unAccountID );

		// Add any custom attributes we need
		if( !BAddGCGeneratedAttributesToItem( pGameAccount, pItem ) )
		{
			delete pItem;
			EmitWarning( SPEW_GC, 2, "CEconItemFactory::CreateSpecificItem(): Failed to generate attributes\n" );
			return NULL;
		}
	}

	return pItem;
}


//-----------------------------------------------------------------------------
// Purpose:	Randomly chooses an item definition that matches the criteria
// Input:	sCriteria - The criteria that the generated item must match
// Output:	The chosen item definition, or NULL if no item could be selected
//-----------------------------------------------------------------------------
const CEconItemDefinition *CEconItemFactory::RollItemDefinition( const CItemSelectionCriteria &criteria ) const
{
	// Determine which item templates match the criteria
	CUtlVector<item_definition_index_t> vecMatches;
	const CEconItemSchema::ItemDefinitionMap_t &mapDefs = m_schema.GetItemDefinitionMap();

	FOR_EACH_MAP_FAST( mapDefs, i )
	{
		if ( criteria.BEvaluate( mapDefs[i] ) )
		{
			vecMatches.AddToTail( mapDefs.Key( i ) );
		}
	}

	if ( 0 == vecMatches.Count() )
		return NULL;

	// Choose a random match
	int iIndex = RandomInt( 0, vecMatches.Count() - 1 );
	return m_schema.GetItemDefinition( vecMatches[iIndex] );
}

//-----------------------------------------------------------------------------
// Purpose:	Generates attributes that the item definition insists it always has, but must be generated by the GC
// Input:	
//-----------------------------------------------------------------------------
bool CEconItemFactory::BAddGCGeneratedAttributesToItem( const CEconGameAccount *pGameAccount, CEconItem *pItem ) const
{
	const CEconItemDefinition *pDef = m_schema.GetItemDefinition( pItem->GetDefinitionIndex() );
	if ( !pDef )
		return false;

	const CUtlVector<static_attrib_t> &vecStaticAttribs = pDef->GetStaticAttributes();

	// Only generate attributes that force the GC to generate them (so they vary per item created)
	FOR_EACH_VEC( vecStaticAttribs, i )
	{
		if ( vecStaticAttribs[i].bForceGCToGenerate )
		{
			ApplyStaticAttributeToItem( pItem, vecStaticAttribs[i], pGameAccount );
		}
	}

	const IEconTool* pTool = pDef->GetEconTool();
	if( pTool )
	{
		if( !pTool->BGenerateDynamicAttributes( pItem, pGameAccount ) )
			return false;
	}

	if ( !pDef->BApplyPropertyGenerators( pItem ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEconItemFactory::ApplyStaticAttributeToItem( CEconItem *pItem, const static_attrib_t& staticAttrib, const CEconGameAccount *pGameAccount ) const
{
	static CSchemaAttributeDefHandle pAttr_ElevateQuality( "elevate quality" );
	static CSchemaAttributeDefHandle pAttr_ElevateToUnusual( "elevate to unusual if applicable" );
	
	static CSchemaAttributeDefHandle pAttr_Particle( "attach particle effect" );
	static CSchemaAttributeDefHandle pAttr_HatUnusual( "hat only unusual effect" );

	static CSchemaAttributeDefHandle pAttrDef_TauntUnusual( "taunt only unusual effect" );
	static CSchemaAttributeDefHandle pAttrDef_TauntUnusualAttr( "on taunt attach particle index" );

	const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( staticAttrib.iDefIndex );
	Assert( pAttrDef );

	// Special-case the elevate-quality attribute.
	if ( pAttrDef == pAttr_ElevateQuality )
	{
		//AssertMsg( CEconItem::GetTypedAttributeType<CSchemaAttributeType_Default>( pAttrDef ), "Elevate quality attribute doesn't have the right type!" );
		int iQuality = (int)staticAttrib.m_value.asFloat;

		// Do not change the quality of an item to Strange if it is not basic
		if ( iQuality == AE_STRANGE )
		{
			if ( pItem->GetQuality() == AE_UNIQUE || pItem->GetQuality() == AE_PAINTKITWEAPON || pItem->GetQuality() == AE_NORMAL )
			{
				pItem->SetQuality( iQuality );
			}
			// If the quality is strange, strangify this item
			StrangifyItemInPlace( pItem );
		}
		else
		{
			pItem->SetQuality( iQuality );
		}

		return;
	}
	// Special-case to elevate-quality only if item has particles.  This 'attr' needs to be added LAST in a lootlist
	// Or rather after particles may have been granted
	else if ( pAttrDef == pAttr_ElevateToUnusual )
	{
		// Scan all attributes.
		if ( pItem->FindAttribute( pAttr_Particle ) || pItem->FindAttribute( pAttrDef_TauntUnusualAttr ) )
		{
			pItem->SetQuality( AE_UNUSUAL );
		}
		return;
	}
	else if ( pAttrDef == pAttr_HatUnusual )
	{
		// Ensure the target item is a hat, if it is not bail, if it is setup a particle effect attr (Whole head items are considered 'hats' for purposes of unusuals )

		if ( !(pItem->GetItemDefinition()->GetEquipRegionMask() & GetItemSchema()->GetEquipRegionBitMaskByName( "hat" ) ) 
		  && !(pItem->GetItemDefinition()->GetEquipRegionMask() & GetItemSchema()->GetEquipRegionBitMaskByName( "whole_head" ) ) 
		) {
			// does not match, bail
			return;
		}
			
		// create a new static attrib
		static_attrib_t unusualAttr( staticAttrib );

		// load the normal attach effect instead
		pAttr_Particle->GetAttributeType()->LoadOrGenerateEconAttributeValue( pItem, pAttr_Particle, unusualAttr, pGameAccount );
		return;
	}
	else if ( pAttrDef == pAttrDef_TauntUnusual )
	{
		// Ensure the target item is a taunt, if it is not bail
		if ( pItem->GetItemDefinition()->GetLoadoutSlot( 0 ) != LOADOUT_POSITION_TAUNT )
		{
			// does not match, bail
			CFmtStr fmtStr( "Attempted to put an unusual taunt effect onto item %s, but it's not a taunt!  Check which lootlists it appears in and remove it from any that are trying to unusualize it!", pItem->GetItemDefinition()->GetItemDefinitionName() );
			EmitError( SPEW_GC, "%s\n", fmtStr.Get() );
			return;
		}

		// create a new static attrib
		static_attrib_t unusualAttr( staticAttrib );

		// load the normal attach effect instead
		pAttrDef_TauntUnusualAttr->GetAttributeType()->LoadOrGenerateEconAttributeValue( pItem, pAttrDef_TauntUnusualAttr, unusualAttr, pGameAccount );
		return;
	}

	// Custom attribute initialization code?
	pAttrDef->GetAttributeType()->LoadOrGenerateEconAttributeValue( pItem, pAttrDef, staticAttrib, pGameAccount );
}

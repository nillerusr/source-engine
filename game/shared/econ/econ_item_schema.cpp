//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: EconItemSchema: Defines a schema for econ items
//
//=============================================================================

#include "cbase.h"
#include "econ_item_schema.h"
#include "tier1/fmtstr.h"
#include "tier1/UtlSortVector.h"
#include "tier2/tier2.h"
#include "filesystem.h"
#include "schemainitutils.h"
#include "gcsdk/gcsdk_auto.h"
#include "rtime.h"
#include "item_selection_criteria.h"
#include "crypto.h"
#include "checksum_sha1.h"

#include <google/protobuf/text_format.h>
#include <string.h>

#include "materialsystem/imaterialsystem.h"
#include "materialsystem/itexture.h"
#include "materialsystem/itexturecompositor.h"

#if ( defined( _MSC_VER ) && _MSC_VER >= 1900 )
#define timezone _timezone
#define daylight _daylight
#endif

// For holiday-limited loot lists.
#include "econ_holidays.h"

// Only used for startup testing.
#include "econ_item_tools.h"

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	#include "econ_item_system.h"
	#include "econ_item.h"
	#include "activitylist.h"

	#if defined(TF_CLIENT_DLL) || defined(TF_DLL)
		#include "tf_gcmessages.h"
	#endif
#endif

#ifdef GC_DLL
#include "gcgamebase.h"
#include <memory>					// unique_ptr
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace GCSDK;


CEconItemSchema & GEconItemSchema()
{
#if defined( EXTERNALTESTS_DLL )
	static CEconItemSchema g_econItemSchema;
	return g_econItemSchema;
#elif defined( GC_DLL )
	return *GEconManager()->GetItemSchema();
#else
	return *ItemSystem()->GetItemSchema();
#endif
}

const char *g_szDropTypeStrings[] =
{
	"",		 // Blank and none mean the same thing: stay attached to the body.
	"none",
	"drop",	 // The item drops off the body.
	"break", // Not implemented, but an example of a type that could be added.
};

const char *g_TeamVisualSections[TEAM_VISUAL_SECTIONS] = 
{
	"visuals",		// TF_TEAM_UNASSIGNED. Visual changes applied to both teams.
	NULL,			// TF_TEAM_SPECTATOR. Unused.
	"visuals_red",	// TF_TEAM_RED
	"visuals_blu",	// TF_TEAM_BLUE
	"visuals_mvm_boss",	// Hack to override things in MvM at a general level
};

int GetTeamVisualsFromString( const char *pszString )
{
	for ( int i = 0; i < TEAM_VISUAL_SECTIONS; i++ )
	{
		// There's a NULL hidden in g_TeamVisualSections
		if ( g_TeamVisualSections[i] && !Q_stricmp( pszString, g_TeamVisualSections[i] ) )
			return i;
	}
	return -1;
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
// Used to convert strings to ints for wearable animation types
const char *g_WearableAnimTypeStrings[ NUM_WAP_TYPES ] =
{
	"on_spawn",			// WAP_ON_SPAWN,
	"start_building",	// WAP_START_BUILDING,
	"stop_building",	// WAP_STOP_BUILDING,
	"start_taunting",		// WAP_START_TAUNTING,
	"stop_taunting",	// WAP_STOP_TAUNTING,
};
#endif

const char *g_AttributeDescriptionFormats[] =
{
	"value_is_percentage",				// ATTDESCFORM_VALUE_IS_PERCENTAGE,
	"value_is_inverted_percentage",		// ATTDESCFORM_VALUE_IS_INVERTED_PERCENTAGE
	"value_is_additive",				// ATTDESCFORM_VALUE_IS_ADDITIVE
	"value_is_additive_percentage",		// ATTDESCFORM_VALUE_IS_ADDITIVE_PERCENTAGE
	"value_is_or",						// ATTDESCFORM_VALUE_IS_OR
	"value_is_date",					// ATTDESCFORM_VALUE_IS_DATE
	"value_is_account_id",				// ATTDESCFORM_VALUE_IS_ACCOUNT_ID
	"value_is_particle_index",			// ATTDESCFORM_VALUE_IS_PARTICLE_INDEX -> Could change to "string index"
	"value_is_killstreakeffect_index",	// ATTDESCFORM_VALUE_IS_KILLSTREAKEFFECT_INDEX -> Could change to "string index"
	"value_is_killstreak_idleeffect_index",  // ATTDESCFORM_VALUE_IS_KILLSTREAK_IDLEEFFECT_INDEX
	"value_is_item_def",				// ATTDESCFORM_VALUE_IS_ITEM_DEF
	"value_is_from_lookup_table",		// ATTDESCFORM_VALUE_IS_FROM_LOOKUP_TABLE
};

const char *g_EffectTypes[NUM_EFFECT_TYPES] =
{
	"unusual",		// ATTRIB_EFFECT_UNUSUAL,
	"strange",		// ATTRIB_EFFECT_STRANGE,
	"neutral",		// ATTRIB_EFFECT_NEUTRAL = 0,
	"positive",		// ATTRIB_EFFECT_POSITIVE,
	"negative",		// ATTRIB_EFFECT_NEGATIVE,
};

//-----------------------------------------------------------------------------
// Purpose: Set the capabilities bitfield based on whether the entry is true/false.
//-----------------------------------------------------------------------------
const char *g_Capabilities[] =
{
	"paintable",				// ITEM_CAP_PAINTABLE
	"nameable",					// ITEM_CAP_NAMEABLE
	"decodable",				// ITEM_CAP_DECODABLE
	"can_craft_if_purchased",	// ITEM_CAP_CAN_BE_CRAFTED_IF_PURCHASED
	"can_customize_texture",	// ITEM_CAP_CAN_CUSTOMIZE_TEXTURE
	"usable",					// ITEM_CAP_USABLE
	"usable_gc",				// ITEM_CAP_USABLE_GC
	"can_gift_wrap",			// ITEM_CAP_CAN_GIFT_WRAP
	"usable_out_of_game",		// ITEM_CAP_USABLE_OUT_OF_GAME
	"can_collect",				// ITEM_CAP_CAN_COLLECT
	"can_craft_count",			// ITEM_CAP_CAN_CRAFT_COUNT
	"can_craft_mark",			// ITEM_CAP_CAN_CRAFT_MARK
	"paintable_team_colors",	// ITEM_CAP_PAINTABLE_TEAM_COLORS
	"can_be_restored",			// ITEM_CAP_CAN_BE_RESTORED
	"strange_parts",			// ITEM_CAP_CAN_USE_STRANGE_PARTS
	"can_card_upgrade",			// ITEM_CAP_CAN_CARD_UPGRADE
	"can_strangify",			// ITEM_CAP_CAN_STRANGIFY
	"can_killstreakify",		// ITEM_CAP_CAN_KILLSTREAKIFY
	"can_consume",				// ITEM_CAP_CAN_CONSUME_ITEMS
	"can_spell_page",			// ITEM_CAP_CAN_SPELLBOOK_PAGE
	"has_slots",				// ITEM_CAP_HAS_SLOTS
	"duck_upgradable",			// ITEM_CAP_DUCK_UPGRADABLE
	"can_unusualify",			// ITEM_CAP_CAN_UNUSUALIFY
};
COMPILE_TIME_ASSERT( ARRAYSIZE(g_Capabilities) == NUM_ITEM_CAPS );

#define RETURN_ATTRIBUTE_STRING( attrib_name, default_string ) \
	static CSchemaAttributeDefHandle pAttribString( attrib_name ); \
	const char *pchResultAttribString = default_string; \
	FindAttribute_UnsafeBitwiseCast< CAttribute_String >( this, pAttribString, &pchResultAttribString ); \
	return pchResultAttribString;

#define RETURN_ATTRIBUTE_STRING_F( func_name, attrib_name, default_string ) \
	const char *func_name( void ) const { RETURN_ATTRIBUTE_STRING( attrib_name, default_string ) }

static void ParseCapability( item_capabilities_t &capsBitfield, KeyValues* pEntry )
{
	int idx = StringFieldToInt(  pEntry->GetName(), g_Capabilities, ARRAYSIZE(g_Capabilities) );
	if ( idx < 0 )
	{
		return;
	}
	int bit = 1 << idx;
	if ( pEntry->GetBool() )
	{
		(int&)capsBitfield |= bit;
	}
	else
	{
		(int&)capsBitfield &= ~bit;
	}
}

#ifdef GC_DLL
static bool BGetPaymentRule( KeyValues *pKVRule, EPaymentRuleType *out_pePaymentRuleType, double *out_pRevenueShare )
{
	Assert( out_pePaymentRuleType );
	Assert( out_pRevenueShare );

	struct payment_rule_lookup_t
	{
		const char *m_pszStr;
		EPaymentRuleType m_eRuleType;
	};

	static payment_rule_lookup_t s_Lookup[] =
	{
		{ "workshop_revenue_share",		kPaymentRule_SteamWorkshopFileID },
		{ "partner_revenue_share",		kPaymentRule_PartnerSteamID },
		{ "bundle_revenue_share",		kPaymentRule_Bundle },
	};

	for ( int i = 0; i < ARRAYSIZE( s_Lookup ); i++ )
	{
		KeyValues *pKVKey = pKVRule->FindKey( s_Lookup[i].m_pszStr );
		if ( !pKVKey )
			continue;

		*out_pePaymentRuleType = s_Lookup[i].m_eRuleType;
		*out_pRevenueShare = atof( pKVKey->GetString() ) * 100.0;				// KeyValues doesn't support parsing a string as a double-precision value, so we do it by hand
		return true;
	}

	return false;
}
#endif // GC_DLL

//-----------------------------------------------------------------------------
// Purpose: CEconItemSeriesDefinition
//-----------------------------------------------------------------------------
CEconItemSeriesDefinition::CEconItemSeriesDefinition( void )
	: m_nValue( INT_MAX )
{
}


//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CEconItemSeriesDefinition::CEconItemSeriesDefinition( const CEconItemSeriesDefinition &that )
{
	( *this ) = that;
}


//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CEconItemSeriesDefinition &CEconItemSeriesDefinition::operator=( const CEconItemSeriesDefinition &rhs )
{
	m_nValue = rhs.m_nValue;
	m_strName = rhs.m_strName;

	return *this;
}


//-----------------------------------------------------------------------------
// Purpose:	Initialize the quality definition
// Input:	pKVQuality - The KeyValues representation of the quality
//			schema - The overall item schema for this attribute
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSeriesDefinition::BInitFromKV( KeyValues *pKVSeries, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{

	m_nValue = pKVSeries->GetInt( "value", -1 );
	m_strName = pKVSeries->GetName();
	
	m_strLockKey = pKVSeries->GetString( "loc_key" );
	m_strUiFile = pKVSeries->GetString( "ui" );

	// Check for required fields
	SCHEMA_INIT_CHECK(
		NULL != pKVSeries->FindKey( "value" ),
		"Quality definition %s: Missing required field \"value\"", pKVSeries->GetName() );

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEconItemQualityDefinition::CEconItemQualityDefinition( void )
:	m_nValue( INT_MAX )
,	m_bCanSupportSet( false )
{
}


//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CEconItemQualityDefinition::CEconItemQualityDefinition( const CEconItemQualityDefinition &that )
{
	(*this) = that;
}


//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CEconItemQualityDefinition &CEconItemQualityDefinition::operator=( const CEconItemQualityDefinition &rhs )
{
	m_nValue = rhs.m_nValue; 
	m_strName =	rhs.m_strName; 
	m_bCanSupportSet = rhs.m_bCanSupportSet;

	return *this;
}


//-----------------------------------------------------------------------------
// Purpose:	Initialize the quality definition
// Input:	pKVQuality - The KeyValues representation of the quality
//			schema - The overall item schema for this attribute
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemQualityDefinition::BInitFromKV( KeyValues *pKVQuality, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{

	m_nValue = pKVQuality->GetInt( "value", -1 );
	m_strName = pKVQuality->GetName();	
	m_bCanSupportSet = pKVQuality->GetBool( "canSupportSet" );
#ifdef GC_DLL
	m_strHexColor = pKVQuality->GetString( "hexColor" );
#endif // GC_DLL

	// Check for required fields
	SCHEMA_INIT_CHECK( 
		NULL != pKVQuality->FindKey( "value" ), 
		"Quality definition %s: Missing required field \"value\"", pKVQuality->GetName() );

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	return SCHEMA_INIT_SUCCESS();
#endif // GC_DLL

	// Check for data consistency
	SCHEMA_INIT_CHECK( 
		0 != Q_stricmp( GetName(), "any" ), 
		"Quality definition any: The quality name \"any\" is a reserved keyword and cannot be used." );

	SCHEMA_INIT_CHECK( 
		m_nValue != k_unItemQuality_Any, 
		"Quality definition %s: Invalid value (%d). It is reserved for Any", GetName(), k_unItemQuality_Any );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// CEconItemRarityDefinition
//-----------------------------------------------------------------------------
CEconItemRarityDefinition::CEconItemRarityDefinition( void )
	: m_nValue( INT_MAX )
	, m_nLootlistWeight( 0 )
{
}

//-----------------------------------------------------------------------------
// Purpose:	Initialize the rarity definition
//-----------------------------------------------------------------------------
bool CEconItemRarityDefinition::BInitFromKV( KeyValues *pKVRarity, KeyValues *pKVRarityWeights, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	m_nValue = pKVRarity->GetInt( "value", -1 );
	m_strName = pKVRarity->GetName();
	m_strLocKey = pKVRarity->GetString( "loc_key" );
	m_strWepLocKey = pKVRarity->GetString( "loc_key_weapon" );

	m_iAttribColor = GetAttribColorIndexForName( pKVRarity->GetString( "color" ) );
	m_strDropSound = pKVRarity->GetString( "drop_sound" );
	m_strNextRarity = pKVRarity->GetString( "next_rarity" ); // Not required.

#ifdef GC_DLL
	if ( pKVRarityWeights )
	{
		m_nLootlistWeight = pKVRarityWeights->GetInt( m_strName, 0 );
	}
#endif
	//

	// Check for required fields
	SCHEMA_INIT_CHECK(
		NULL != pKVRarity->FindKey( "value" ),
		"Rarity definition %s: Missing required field \"value\"", pKVRarity->GetName() );

	SCHEMA_INIT_CHECK(
		NULL != pKVRarity->FindKey( "loc_key" ),
		"Rarity definition %s: Missing required field \"loc_key\"", pKVRarity->GetName() );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconColorDefinition::BInitFromKV( KeyValues *pKVColor, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	m_strName		= pKVColor->GetName();
	m_strColorName	= pKVColor->GetString( "color_name" );
#ifdef GC_DLL
	m_strHexColor	= pKVColor->GetString( "hex_color" );
#endif // GC_DLL

	SCHEMA_INIT_CHECK(
		!m_strColorName.IsEmpty(),
		"Quality definition %s: missing \"color_name\"", GetName() );

#ifdef GC_DLL
	SCHEMA_INIT_CHECK(
		!m_strHexColor.IsEmpty(),
		"Quality definition %s: missing \"hex_color\"", GetName() );
#endif // GC_DLL

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CEconItemSetDefinition::CEconItemSetDefinition( void )
	: m_pszName( NULL )
	, m_pszLocalizedName( NULL )
	, m_iBundleItemDef( INVALID_ITEM_DEF_INDEX )
	, m_bIsHiddenSet( false )
{
}

//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CEconItemSetDefinition::CEconItemSetDefinition( const CEconItemSetDefinition &that )
{
	(*this) = that;
}

//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CEconItemSetDefinition &CEconItemSetDefinition::operator=( const CEconItemSetDefinition &other )
{
	m_pszName = other.m_pszName;
	m_pszLocalizedName = other.m_pszLocalizedName;
	m_iItemDefs = other.m_iItemDefs;
	m_iAttributes = other.m_iAttributes;
	m_iBundleItemDef = other.m_iBundleItemDef;
	m_bIsHiddenSet = other.m_bIsHiddenSet;

	return *this;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CEconItemSetDefinition::BInitFromKV( KeyValues *pKVItemSet, CUtlVector<CUtlString> *pVecErrors )
{
	m_pszName = pKVItemSet->GetName();

	m_iBundleItemDef = INVALID_ITEM_DEF_INDEX;
	const char *pszBundleName = pKVItemSet->GetString( "store_bundle" );
	if ( pszBundleName && pszBundleName[0] )
	{
		CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszBundleName );
		if ( pDef )
		{
			m_iBundleItemDef = pDef->GetDefinitionIndex();
		}

		SCHEMA_INIT_CHECK( 
			pDef != NULL,
			"Item set %s: Bundle definition \"%s\" was not found", m_pszName, pszBundleName );
	}

	m_pszLocalizedName = pKVItemSet->GetString( "name", NULL );
	m_bIsHiddenSet = pKVItemSet->GetBool( "is_hidden_set", false );

	KeyValues *pKVItems = pKVItemSet->FindKey( "items" );
	if ( pKVItems )
	{
		FOR_EACH_SUBKEY( pKVItems, pKVItem )
		{
			const char *pszName = pKVItem->GetName();

			CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszName );

			SCHEMA_INIT_CHECK( 
				pDef != NULL,
				"Item set %s: Item definition \"%s\" was not found", m_pszName, pszName );

			const item_definition_index_t unDefIndex = pDef->GetDefinitionIndex();

			SCHEMA_INIT_CHECK(
				!m_iItemDefs.IsValidIndex( m_iItemDefs.Find( unDefIndex ) ),
				"Item set %s: item definition \"%s\" appears multiple times", m_pszName, pszName );
			SCHEMA_INIT_CHECK(
				!pDef->GetItemSetDefinition(),
				"Item set %s: item definition \"%s\" specified in multiple item sets", m_pszName, pszName );

			m_iItemDefs.AddToTail( unDefIndex );
			pDef->SetItemSetDefinition( this );

			// FIXME: hack to work around crafting item criteria
			pDef->GetRawDefinition()->SetString( "item_set", m_pszName );
		}
	}

	KeyValues *pKVAttributes = pKVItemSet->FindKey( "attributes" );
	if ( pKVAttributes )
	{
		FOR_EACH_SUBKEY( pKVAttributes, pKVAttribute )
		{
			const char *pszName = pKVAttribute->GetName();

			const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinitionByName( pszName );
			SCHEMA_INIT_CHECK( 
				pAttrDef != NULL,
				"Item set %s: Attribute definition \"%s\" was not found", m_pszName, pszName );
			SCHEMA_INIT_CHECK(
				pAttrDef->BIsSetBonusAttribute(),
				"Item set %s: Attribute definition \"%s\" is not a set bonus attribute", m_pszName, pszName );

			int iIndex = m_iAttributes.AddToTail();
			m_iAttributes[iIndex].m_iAttribDefIndex = pAttrDef->GetDefinitionIndex();
			m_iAttributes[iIndex].m_flValue = pKVAttribute->GetFloat( "value" );
		}
	}

	// Sanity check.
	SCHEMA_INIT_CHECK( m_pszLocalizedName != NULL,
		"Item set %s: Set contains no localized name", m_pszName );
	SCHEMA_INIT_CHECK( m_iItemDefs.Count() > 0,
		"Item set %s: Set contains no items", m_pszName );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CEconItemSetDefinition::IterateAttributes( class IEconItemAttributeIterator *pIterator ) const
{
	FOR_EACH_VEC( m_iAttributes, i )
	{
		const itemset_attrib_t& itemsetAttrib = m_iAttributes[i];

		const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( itemsetAttrib.m_iAttribDefIndex );
		if ( !pAttrDef )
			continue;

		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );

		// We know (and assert) that we only need 32 bits of data to store this attribute
		// data. We don't know anything about the type but we'll let the type handle it
		// below.
		attribute_data_union_t value;
		value.asFloat = itemsetAttrib.m_flValue;

		if ( !pAttrType->OnIterateAttributeValue( pIterator, pAttrDef, value ) )
			return;
	}
}

//-----------------------------------------------------------------------------
CEconItemCollectionDefinition::CEconItemCollectionDefinition( void )
	: m_pszName( NULL )
	, m_pszLocalizedName( NULL )
	, m_pszLocalizedDesc( NULL )
	, m_iRarityMin( k_unItemRarity_Any )
	, m_iRarityMax( k_unItemRarity_Any )
{
}

//-----------------------------------------------------------------------------
// 

//-----------------------------------------------------------------------------
static int SortCollectionByRarity( item_definition_index_t const *a, item_definition_index_t const *b )
{
	Assert( a );
	Assert( *a );
	Assert( b );
	Assert( *b );

	CEconItemDefinition *pItemA = GetItemSchema()->GetItemDefinition( *a );
	CEconItemDefinition *pItemB = GetItemSchema()->GetItemDefinition( *b );

	if ( !pItemA || !pItemB )
	{
		AssertMsg( 0, "ItemDef Doesn't exist for sorting" );
		return 1;
	}
	
	// If same Rarity, leave in current position?
	if ( pItemA->GetRarity() == pItemB->GetRarity() && pItemA->GetCustomPainkKitDefinition() && pItemB->GetCustomPainkKitDefinition() )
	{
#ifdef CLIENT_DLL
		// Sort by localized name
		// paintkits sort by paintkit name
		auto paintkitA = pItemA->GetCustomPainkKitDefinition();
		auto paintkitB = pItemB->GetCustomPainkKitDefinition();
		auto paintkitALocName = paintkitA->GetLocalizeName();
		auto paintkitBLocName = paintkitB->GetLocalizeName();
		auto pkALocalized = g_pVGuiLocalize->Find( paintkitALocName );
		auto pkBLocalized = g_pVGuiLocalize->Find( paintkitBLocName );
		if ( pkALocalized )
		{
			if ( pkBLocalized )
			{
				return V_wcscmp( pkALocalized, pkBLocalized );
			}
			else
			{
				return -1;
			}
		}
		else
		{
			return pkBLocalized ? 1 : -1;
		}
#else
		return 0;
#endif
	}

	return ( pItemA->GetRarity() > pItemB->GetRarity() ) ? -1 : 1;
}


//-----------------------------------------------------------------------------
bool CEconItemCollectionDefinition::BInitFromKV( KeyValues *pKVPItemCollection, CUtlVector<CUtlString> *pVecErrors )
{
	m_pszName = pKVPItemCollection->GetName();

	m_pszLocalizedName = pKVPItemCollection->GetString( "name", NULL );
	m_pszLocalizedDesc = pKVPItemCollection->GetString( "description", NULL );

	m_bIsReferenceCollection = pKVPItemCollection->GetBool( "is_reference_collection", false );

	KeyValues *pKVItems = pKVPItemCollection->FindKey( "items" );

	// Create a 'lootlist' from this collection
	KeyValues *pCollectionLootList = NULL;
	bool bIsLootList = false;
	if ( !m_bIsReferenceCollection )
	{
		pCollectionLootList = new KeyValues( m_pszName );
	}

	if ( pKVItems )
	{
		// Traverse rarity items and set rarity
		// Create a lootlist if applicable
		FOR_EACH_TRUE_SUBKEY( pKVItems, pKVRarity )
		{
			bIsLootList = true;
			// Get the Rarity Value
			const CEconItemRarityDefinition *pRarity = GetItemSchema()->GetRarityDefinitionByName( pKVRarity->GetName() );
			SCHEMA_INIT_CHECK( pRarity != NULL, "Item collection %s: Rarity type \"%s\" was not found", m_pszName, pKVRarity->GetName() );
			
			// Create a lootlist
			if ( !m_bIsReferenceCollection )
			{
				CFmtStr lootlistname( "%s_%s", m_pszName, pRarity->GetName() );
				const char *pszName = V_strdup( lootlistname.Get() );
				pKVRarity->SetInt( "rarity", pRarity->GetDBValue() );
				SCHEMA_INIT_CHECK( GetItemSchema()->BInsertLootlist( pszName, pKVRarity, pVecErrors ), "Invalid collection lootlist %s", pszName );
				KeyValues *pTempRarityKey = pKVRarity->FindKey( "rarity" );
				Assert( pTempRarityKey );
				pKVRarity->RemoveSubKey( pTempRarityKey );
				pTempRarityKey->deleteThis();
				pCollectionLootList->SetInt( pszName, pRarity->GetLootlistWeight() );
			}

			// Items in the Rarity
			FOR_EACH_VALUE( pKVRarity, pKVItem )
			{
				const char *pszName = pKVItem->GetName();

				CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszName );

				SCHEMA_INIT_CHECK(
					pDef != NULL,
					"Item set %s: Item definition \"%s\" was not found", m_pszName, pszName );

				const item_definition_index_t unDefIndex = pDef->GetDefinitionIndex();

				SCHEMA_INIT_CHECK(
					!m_iItemDefs.IsValidIndex( m_iItemDefs.Find( unDefIndex ) ),
					"Item Collection %s: item definition \"%s\" appears multiple times", m_pszName, pszName );

				m_iItemDefs.AddToTail( unDefIndex );

				// Collection Reference
				if ( !m_bIsReferenceCollection )
				{
					SCHEMA_INIT_CHECK(
						!pDef->GetItemCollectionDefinition(),
						"Item Collection %s: item definition \"%s\" specified in multiple item sets", m_pszName, pszName );
					pDef->SetItemCollectionDefinition( this );
				}

				// Item Rarity
				pDef->SetRarity( pRarity->GetDBValue() );
			}
		}

		// Loose Items
		FOR_EACH_VALUE( pKVItems, pKVItem )
		{
			const char *pszName = pKVItem->GetName();

			CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszName );

			SCHEMA_INIT_CHECK(
				pDef != NULL,
				"Item set %s: Item definition \"%s\" was not found", m_pszName, pszName );

			const item_definition_index_t unDefIndex = pDef->GetDefinitionIndex();

			SCHEMA_INIT_CHECK(
				!m_iItemDefs.IsValidIndex( m_iItemDefs.Find( unDefIndex ) ),
				"Item Collection %s: item definition \"%s\" appears multiple times", m_pszName, pszName );
			
			m_iItemDefs.AddToTail( unDefIndex );

			if ( !m_bIsReferenceCollection )
			{
				SCHEMA_INIT_CHECK(
					!pDef->GetItemCollectionDefinition(),
					"Item Collection %s: item definition \"%s\" specified in multiple item sets", m_pszName, pszName );
				pDef->SetItemCollectionDefinition( this );
			}
		}

		// Sort by Rarity
		m_iItemDefs.Sort( &SortCollectionByRarity );
	}

	if ( !m_bIsReferenceCollection && bIsLootList )
	{
		// Insert collection lootlist
		GetItemSchema()->BInsertLootlist( m_pszName, pCollectionLootList, pVecErrors );
	}

	if ( pCollectionLootList )
	{
		pCollectionLootList->deleteThis();
	}

	// Sorted high to low
	m_iRarityMax = GetItemSchema()->GetItemDefinition( m_iItemDefs[ 0 ] )->GetRarity();
	m_iRarityMin = GetItemSchema()->GetItemDefinition( m_iItemDefs[ m_iItemDefs.Count() - 1] )->GetRarity();
	// Verify that there is no gaps in the Rarity (would cause crafting problems and makes no sense)

	if ( !m_bIsReferenceCollection )
	{
		int iRarityVerify = m_iRarityMax;
		FOR_EACH_VEC( m_iItemDefs, i )
		{
			int iNextRarity = GetItemSchema()->GetItemDefinition( m_iItemDefs[i] )->GetRarity();
			SCHEMA_INIT_CHECK( iRarityVerify - iNextRarity <= 1, "Items in Collection %s: Have a gap in rarity tiers", m_pszName );
			iRarityVerify = iNextRarity;
		}
	}

	// Sanity check.
	SCHEMA_INIT_CHECK( m_pszLocalizedName != NULL,
		"Item Collection %s: Collection contains no localized name", m_pszName );
	SCHEMA_INIT_CHECK( m_pszLocalizedDesc != NULL,
		"Item Collection %s: Collection contains no localized description", m_pszName );
	SCHEMA_INIT_CHECK( m_iItemDefs.Count() > 0,
		"Item Collection %s: Collection contains no items", m_pszName );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// CEconItemPaintKitDefinition
//-----------------------------------------------------------------------------
CEconItemPaintKitDefinition::CEconItemPaintKitDefinition( void )
	: m_pszName( NULL )
{
}

//-----------------------------------------------------------------------------
CEconItemPaintKitDefinition::~CEconItemPaintKitDefinition( void )
{
	FOR_EACH_VEC( m_vecPaintKitWearKVP, i )
	{
		if ( m_vecPaintKitWearKVP[i] )
		{
			m_vecPaintKitWearKVP[i]->deleteThis();
		}
	}

	m_vecPaintKitWearKVP.Purge();
}

bool VerifyPaintKitComposite( KeyValues *pKVWearInputItems, const char* pName, int iWearLevel, CUtlVector<CUtlString> *pVecErrors )
{
	SCHEMA_INIT_CHECK( pKVWearInputItems != NULL, "Paint Kit %s: Does not contain Wear Level %d", pName, iWearLevel );
#ifdef CLIENT_DLL
	int w = 1;
	int h = 1;
	int seed = 0;
	ITextureCompositor* pWeaponSkinBaseCompositor = NULL;

	SafeAssign( &pWeaponSkinBaseCompositor, materials->NewTextureCompositor( w, h, pName, TF_TEAM_RED, seed, pKVWearInputItems, TEX_COMPOSITE_CREATE_FLAGS_VERIFY_SCHEMA_ONLY ) );
	SCHEMA_INIT_CHECK( pWeaponSkinBaseCompositor != NULL, "Could Not Create Weapon Skin Compositor for [%s][Wear %d][Team Red]", pName, iWearLevel);
	SafeRelease( &pWeaponSkinBaseCompositor );

	SafeAssign( &pWeaponSkinBaseCompositor, materials->NewTextureCompositor( w, h, pName, TF_TEAM_BLUE, seed, pKVWearInputItems, TEX_COMPOSITE_CREATE_FLAGS_VERIFY_SCHEMA_ONLY ) );
	SCHEMA_INIT_CHECK( pWeaponSkinBaseCompositor != NULL, "Could Not Create Weapon Skin Compositor for [%s][Wear %d][Team BLUE]", pName, iWearLevel );
	SafeRelease( &pWeaponSkinBaseCompositor );
#endif // CLIENT_DLL
	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
bool CEconItemPaintKitDefinition::BInitFromKV( KeyValues *pKVPItemPaintKit, CUtlVector<CUtlString> *pVecErrors )
{
	m_pszName = pKVPItemPaintKit->GetName();
	m_pszLocalizedName = m_pszName;		// localization key is same as paintkit name for ease of generation

	SCHEMA_INIT_CHECK( m_pszLocalizedName != NULL, "Paint Kit %s: PaintKit contains no localized name", m_pszName );

	KeyValues *pKVWearInputItems = NULL;

	pKVWearInputItems = pKVPItemPaintKit->FindKey( "wear_level_1", false );
	SCHEMA_INIT_CHECK( VerifyPaintKitComposite( pKVWearInputItems, m_pszName, 1, pVecErrors ), "Could Not Create Weapon Skin Compositor for [%s][Wear %d]", m_pszName, 1 );
	m_vecPaintKitWearKVP.AddToTail( pKVWearInputItems->MakeCopy() );
	
	pKVWearInputItems = pKVPItemPaintKit->FindKey( "wear_level_2", false );
	SCHEMA_INIT_CHECK( VerifyPaintKitComposite( pKVWearInputItems, m_pszName, 2, pVecErrors ), "Could Not Create Weapon Skin Compositor for [%s][Wear %d]", m_pszName, 2 );
	m_vecPaintKitWearKVP.AddToTail( pKVWearInputItems->MakeCopy() );

	pKVWearInputItems = pKVPItemPaintKit->FindKey( "wear_level_3", false );
	SCHEMA_INIT_CHECK( VerifyPaintKitComposite( pKVWearInputItems, m_pszName, 3, pVecErrors ), "Could Not Create Weapon Skin Compositor for [%s][Wear %d]", m_pszName, 3 );
	m_vecPaintKitWearKVP.AddToTail( pKVWearInputItems->MakeCopy() );

	pKVWearInputItems = pKVPItemPaintKit->FindKey( "wear_level_4", false );
	SCHEMA_INIT_CHECK( VerifyPaintKitComposite( pKVWearInputItems, m_pszName, 4, pVecErrors ), "Could Not Create Weapon Skin Compositor for [%s][Wear %d]", m_pszName, 4 );
	m_vecPaintKitWearKVP.AddToTail( pKVWearInputItems->MakeCopy() );

	pKVWearInputItems = pKVPItemPaintKit->FindKey( "wear_level_5", false );
	SCHEMA_INIT_CHECK( VerifyPaintKitComposite( pKVWearInputItems, m_pszName, 5, pVecErrors ), "Could Not Create Weapon Skin Compositor for [%s][Wear %d]", m_pszName, 5 );
	m_vecPaintKitWearKVP.AddToTail( pKVWearInputItems->MakeCopy() );

	return SCHEMA_INIT_SUCCESS();
}

KeyValues *CEconItemPaintKitDefinition::GetPaintKitWearKV( int nWear )
{
	// Wear is 1-5 but this vec is 0-4
	int iIndex = nWear - 1;

	if ( !m_vecPaintKitWearKVP.IsValidIndex( iIndex ) )
	{
		iIndex = 0;
		Assert( m_vecPaintKitWearKVP.IsValidIndex( iIndex ) );
		DevMsg( "Invalid Paint Kit or Paint Kit Wear Entry (%s) Wear (%d).", m_pszName, nWear );
		return NULL;
	}

	return m_vecPaintKitWearKVP[iIndex];
}

//-----------------------------------------------------------------------------
// CEconOperationDefinition
//-----------------------------------------------------------------------------
CEconOperationDefinition::CEconOperationDefinition( void )
	: m_pszName( NULL )
	, m_unRequiredItemDefIndex( INVALID_ITEM_DEF_INDEX )
	, m_unGatewayItemDefIndex( INVALID_ITEM_DEF_INDEX )
{
}

//-----------------------------------------------------------------------------
CEconOperationDefinition::~CEconOperationDefinition( void )
{
	if ( m_pKVItem )
		m_pKVItem->deleteThis();
	m_pKVItem = NULL;
}

//-----------------------------------------------------------------------------
bool CEconOperationDefinition::BInitFromKV( KeyValues *pKVPOperation, CUtlVector<CUtlString> *pVecErrors )
{
	m_unOperationID = V_atoi( pKVPOperation->GetName() );

	m_pszName = pKVPOperation->GetString( "name", NULL );
	SCHEMA_INIT_CHECK( m_pszName != NULL, "OperationDefinition %s does not have 'name'", m_pszName );

	// initialize required item def index if we specified one
	const char *pszRequiredName = pKVPOperation->GetString( "required_item_name", NULL );
	if ( pszRequiredName )
	{
		CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszRequiredName );
		SCHEMA_INIT_CHECK( pDef != NULL, "OperationDefinition couldn't find item def from required name '%s'", pszRequiredName );

		m_unRequiredItemDefIndex = pDef->GetDefinitionIndex();
	}

	const char *pszGatewayItemName = pKVPOperation->GetString( "gateway_item_name", NULL );
	if ( pszGatewayItemName )
	{
		CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszGatewayItemName );
		SCHEMA_INIT_CHECK( pDef != NULL, "OperationDefinition couldn't find item def from gateway name '%s'", pszGatewayItemName );

		m_unGatewayItemDefIndex = pDef->GetDefinitionIndex();
	}

	if ( m_unGatewayItemDefIndex != INVALID_ITEM_DEF_INDEX )
	{
		SCHEMA_INIT_CHECK( m_unRequiredItemDefIndex != INVALID_ITEM_DEF_INDEX, "If a gateway item is set, a required item must be set!  Mismatch in %d", m_unOperationID );
	}

	const char *pszOperationStartDate = pKVPOperation->GetString( "operation_start_date", NULL );
	SCHEMA_INIT_CHECK( pszOperationStartDate != NULL, "OperationDefinition %s does not have 'operation_start_date'", m_pszName );
	m_OperationStartDate = ( pszOperationStartDate && pszOperationStartDate[0] )
							? CRTime::RTime32FromFmtString( "YYYY-MM-DD hh:mm:ss" , pszOperationStartDate )
							: RTime32(0);

	const char *pszDropEndDate = pKVPOperation->GetString( "stop_giving_to_player_date", NULL );
	SCHEMA_INIT_CHECK( pszDropEndDate != NULL, "OperationDefinition %s does not have 'stop_giving_to_player_date'", m_pszName );
	m_StopGivingToPlayerDate = ( pszDropEndDate && pszDropEndDate[0] )
							? CRTime::RTime32FromFmtString( "YYYY-MM-DD hh:mm:ss" , pszDropEndDate )
							: RTime32(0);

	const char *pszOperationEndDate = pKVPOperation->GetString( "stop_adding_to_queue_date", NULL );
	SCHEMA_INIT_CHECK( pszOperationEndDate != NULL, "OperationDefinition %s does not have 'stop_adding_to_queue_date'", m_pszName );
	m_StopAddingToQueueDate = ( pszOperationEndDate && pszOperationEndDate[0] )
							? CRTime::RTime32FromFmtString( "YYYY-MM-DD hh:mm:ss" , pszOperationEndDate )
							: RTime32(0);

	m_pszQuestLogResFile	= pKVPOperation->GetString( "quest_log_res_file", NULL );
	m_pszQuestListResFile	= pKVPOperation->GetString( "quest_list_res_file", NULL );

	m_pszOperationLootList	= pKVPOperation->GetString( "operation_lootlist" );
	SCHEMA_INIT_CHECK( m_pszOperationLootList != NULL, "OperationDefinition %s does not have 'operation_lootlist'", m_pszName );

	m_bIsCampaign			= pKVPOperation->GetBool( "is_campaign" );
	m_unMaxDropCount		= pKVPOperation->GetInt( "max_drop_count" );

#ifdef GC_DLL

	m_rtQueueFreqMin	= pKVPOperation->GetFloat( "queue_freq_min" ) * k_nSecondsPerHour; // converts specified hours to seconds
	SCHEMA_INIT_CHECK( m_rtQueueFreqMin != 0.f, "OperationDefinition %s does not have 'queue_freq_min'", m_pszName );

	m_rtQueueFreqMax	= pKVPOperation->GetFloat( "queue_freq_max" ) * k_nSecondsPerHour; // converts specified hours to seconds
	SCHEMA_INIT_CHECK( m_rtQueueFreqMax != 0.f, "OperationDefinition %s does not have 'queue_freq_max'", m_pszName );

	SCHEMA_INIT_CHECK( m_rtQueueFreqMin <= m_rtQueueFreqMax, "OperationDefinition %s 'queue_freq_min' must be less than 'queue_freq_max'", m_pszName );

	m_rtDropFreqMin		= pKVPOperation->GetFloat( "drop_freq_min" ) * k_nSecondsPerHour; // converts specified hours to seconds
	SCHEMA_INIT_CHECK( m_rtDropFreqMin != 0.f, "OperationDefinition %s does not have 'drop_freq_min'", m_pszName );

	m_rtDropFreqMax		= pKVPOperation->GetFloat( "drop_freq_max" ) * k_nSecondsPerHour; // converts specified hours to seconds
	SCHEMA_INIT_CHECK( m_rtDropFreqMax != 0.f, "OperationDefinition %s does not have 'drop_freq_max'", m_pszName );

	SCHEMA_INIT_CHECK( m_rtDropFreqMin <= m_rtDropFreqMax, "OperationDefinition %s 'drop_freq_min' must be less than 'drop_freq_max'", m_pszName );
	
	m_unSeed			= pKVPOperation->GetInt( "seed_drops" );
	m_unMaxHeldDrops	= pKVPOperation->GetInt( "max_held_drops" );
	m_nMaxQueueCount	= pKVPOperation->GetInt( "max_queue_count" );
	m_unMaxDropPerThink	= pKVPOperation->GetInt( "max_drop_per_think", 1 );

	m_pszContractRewardLootlist[ REWARD_CASE ]		= pKVPOperation->GetString( "contract_reward_case_lootlist" );
	m_pszContractRewardLootlist[ REWARD_WEAPON ]	= pKVPOperation->GetString( "contract_reward_weapon_lootlist" );

#endif // GC_DLL

	m_pKVItem = pKVPOperation->MakeCopy();

	return SCHEMA_INIT_SUCCESS();
}

#ifdef GC_DLL
#ifdef STAGING_ONLY
	GCConVar gc_quick_operation_drop_name( "gc_quick_operation_drop_name", "" );
	GCConVar gc_quick_operation_drop_rate( "gc_quick_operation_drop_rate", "10" );
#endif // STAGING_ONLY

RTime32	CEconOperationDefinition::GetMinQueueFreq() const
{ 
#ifdef STAGING_ONLY
	if ( Q_stricmp( gc_quick_operation_drop_name.GetString(), m_pszName ) == 0 )
	{
		return gc_quick_operation_drop_rate.GetInt();
	}
#endif

	return m_rtQueueFreqMin;
}

RTime32	CEconOperationDefinition::GetMaxQueueFreq() const
{ 
#ifdef STAGING_ONLY
	if ( Q_stricmp( gc_quick_operation_drop_name.GetString(), m_pszName ) == 0 )
	{
		return gc_quick_operation_drop_rate.GetInt() + 2;
	}
#endif

	return m_rtQueueFreqMax;
}

RTime32	CEconOperationDefinition::GetMinDropFreq() const 
{ 
#ifdef STAGING_ONLY
	if ( Q_stricmp( gc_quick_operation_drop_name.GetString(), m_pszName ) == 0 )
	{
		return gc_quick_operation_drop_rate.GetInt();
	}
#endif

	return m_rtDropFreqMin;
}

RTime32	CEconOperationDefinition::GetMaxDropFreq() const 
{ 
#ifdef STAGING_ONLY
	if ( Q_stricmp( gc_quick_operation_drop_name.GetString(), m_pszName ) == 0 )
	{
		return gc_quick_operation_drop_rate.GetInt() + 2;
	}
#endif

	return m_rtDropFreqMax;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool BCommonInitPropertyGeneratorsFromKV( const char *pszContext, CUtlVector<const IEconItemPropertyGenerator *> *out_pvecGenerators, KeyValues *pKV, CUtlVector<CUtlString> *pVecErrors )
{
	// Forward declaration so factory functions can be wherever.
	IEconItemPropertyGenerator *CreateChangeQualityGenerator( KeyValues *, CUtlVector<CUtlString> * );
	IEconItemPropertyGenerator *CreateRandomEvenChanceAttrGenerator( KeyValues *, CUtlVector<CUtlString> * );
	IEconItemPropertyGenerator *CreateUniformLineItemLootListGenerator( KeyValues *, CUtlVector<CUtlString> * );
	IEconItemPropertyGenerator *CreateDynamicAttrsGenerator( KeyValues *, CUtlVector<CUtlString> * );
	
	// "Factory".
	struct econ_item_property_generator_factory_entry_t
	{
		const char *m_pszGeneratorName;
		IEconItemPropertyGenerator * (* m_funcCreateGeneratorInstance)( KeyValues *, CUtlVector<CUtlString> * );
	};

	static econ_item_property_generator_factory_entry_t s_Generators[] =
	{
		{ "change_quality",					&CreateChangeQualityGenerator },
		{ "random_even_chance_attr",		&CreateRandomEvenChanceAttrGenerator },
		{ "uniform_line_item_loot_list",	&CreateUniformLineItemLootListGenerator },
		{ "dynamic_attrs",					&CreateDynamicAttrsGenerator },
	};

	Assert( out_pvecGenerators );
	Assert( pVecErrors );

	// No input data means "we succeeded here".
	if ( !pKV )
		return true;

	// Handle each generator one at a time. We'll try to initialize the whole set to get as many
	// errors as possible and then return accumulated errors.
	FOR_EACH_SUBKEY( pKV, pKVGenerator )
	{
		const char *pszGeneratorName = pKVGenerator->GetName();

		IEconItemPropertyGenerator *pGenerator = NULL;
		for ( const auto& gen : s_Generators )
		{
			if ( Q_stricmp( gen.m_pszGeneratorName, pszGeneratorName ) != 0 )
				continue;

			pGenerator = (*gen.m_funcCreateGeneratorInstance)( pKVGenerator, pVecErrors );
			SCHEMA_INIT_CHECK( pGenerator != nullptr, "%s: property generator \"%s\" failed to initialize.\n", pszContext, pszGeneratorName );
		
			out_pvecGenerators->AddToTail( pGenerator );
			break;
		}

		// Make sure we found a way to create this instance.
		SCHEMA_INIT_CHECK( pGenerator != nullptr, "%s: unknown type for property generator \"%s\".\n", pszContext, pszGeneratorName );
	}

	return SCHEMA_INIT_SUCCESS();
}
#endif // GC_DLL

//-----------------------------------------------------------------------------
// Dtor
//-----------------------------------------------------------------------------
CEconLootListDefinition::~CEconLootListDefinition( void )
{
#ifdef GC_DLL
	m_RandomAttribs.PurgeAndDeleteElements();
	m_PropertyGenerators.PurgeAndDeleteElements();
#endif
}

#ifdef GC_DLL
bool CEconLootListDefinition::AddRandomAtrributes( KeyValues *pRandomAttributesKV, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /*= NULL*/ )
{
	const char *pszAttrName = pRandomAttributesKV->GetName();

	// We've found the random attribute block. Parse it.
	random_attrib_t *pRandomAttr = pschema.CreateRandomAttribute( m_pszName, pRandomAttributesKV, pVecErrors );
	
	SCHEMA_INIT_CHECK(
		NULL != pRandomAttr,
		CFmtStr( "Loot List %s: Failed to create random_attrib_t '%s'", m_pszName, pszAttrName ) );

	m_RandomAttribs.AddToTail( pRandomAttr );

	return true;
}

bool CEconLootListDefinition::AddRandomAttributesFromTemplates( KeyValues *pRandomAttributesKV, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /*= NULL*/ )
{
	const char *pszAttrName = pRandomAttributesKV->GetName();

	// try to find attr by template name
	random_attrib_t *pRandomAttrTemplate = pschema.GetRandomAttributeTemplateByName( pszAttrName );
	
	SCHEMA_INIT_CHECK(
		NULL != pRandomAttrTemplate,
		CFmtStr( "Loot List %s: Couldn't find random_attrib_t '%s' from attribute_templates", m_pszName, pszAttrName ) );

	// craete a copy of the template and add to the list
	random_attrib_t *pRandomAttr = new random_attrib_t;
	*pRandomAttr = *pRandomAttrTemplate;
	m_RandomAttribs.AddToTail( pRandomAttr );

	return true;
}
#endif // GC_DLL
//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
static const char *g_pszDefaultRevolvingLootListHeader = "#Econ_Revolving_Loot_List";

bool CEconLootListDefinition::BInitFromKV( KeyValues *pKVLootList, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors )
{
	m_pszName = pKVLootList->GetName();
	m_pszLootListHeader = g_pszDefaultRevolvingLootListHeader;
	m_pszLootListFooter = NULL;
	m_pszCollectionReference = NULL;
	m_bPublicListContents = true;
#ifdef GC_DLL
	m_iNoDupesIterations = -1;				// disable no-dupes functionality by default
	m_unRarity = pKVLootList->GetInt( "rarity", k_unItemRarity_Any );
#endif // GC_DLL
	bool bCollectionLootList = false;

	FOR_EACH_SUBKEY( pKVLootList, pKVListItem )
	{
		const char *pszName = pKVListItem->GetName();
		
		if ( !Q_strcmp( pszName, "loot_list_header_desc" ) )
		{
			// Make sure we didn't specify multiple entries.
			SCHEMA_INIT_CHECK(
				g_pszDefaultRevolvingLootListHeader == m_pszLootListHeader,
				"Loot list %s: Multiple header descriptions specified", m_pszName );

			m_pszLootListHeader = pKVListItem->GetString();

			SCHEMA_INIT_CHECK(
				NULL != m_pszLootListHeader,
				"Loot list %s: Invalid header description specified", m_pszName );

			continue;
		}
		else if ( !Q_strcmp( pszName, "loot_list_footer_desc" ) )
		{
			// Make sure we didn't specify multiple entries.
			SCHEMA_INIT_CHECK(
				NULL == m_pszLootListFooter,
				"Loot list %s: Multiple footer descriptions specified", m_pszName );

			m_pszLootListFooter = pKVListItem->GetString();

			SCHEMA_INIT_CHECK(
				NULL != m_pszLootListFooter,
				"Loot list %s: Invalid header description specified", m_pszName );

			continue;
		}
		else if ( !Q_strcmp( pszName, "loot_list_collection" ) )
		{
			// Set name as the collection lootlist name
			pszName = pKVListItem->GetString();
			m_pszCollectionReference = pszName;
			bCollectionLootList = true;
		}
		else if ( !Q_strcmp( pszName, "hide_lootlist" ) )
		{
			m_bPublicListContents = !pKVListItem->GetBool( nullptr, true );
			continue;
		}
		else if ( !Q_strcmp( pszName, "rarity" ) )
		{
			// already parsed up top
			continue;
		}
#ifdef GC_DLL
		else if ( !Q_strcmp( pszName, "random_attributes" ) )
		{
			AddRandomAtrributes( pKVListItem, pschema, pVecErrors );
			continue;
		}
		else if ( !Q_strcmp( pszName, "attribute_templates" ) )
		{
			FOR_EACH_SUBKEY( pKVListItem, pKVAttributeTemplate )
			{
				if ( pKVAttributeTemplate->GetInt() == 0 )
					continue;

				bool bAdded = AddRandomAttributesFromTemplates( pKVAttributeTemplate, pschema, pVecErrors );
				SCHEMA_INIT_CHECK( bAdded, "Loot list %s: Failed to attribute_templates '%s'", m_pszName, pKVAttributeTemplate->GetName() );
			}

			continue;
		}
		else if ( !Q_strcmp( pszName, "public_list_contents" ) )
		{
			m_bPublicListContents = pKVListItem->GetBool( nullptr, true );
			continue;
		}
		else if ( !Q_stricmp( pszName, "__no_dupes_iter_count" ) )
		{
			m_iNoDupesIterations = pKVListItem->GetInt( nullptr, -1 );
			continue;
		}
		else if ( !Q_strcmp( pszName, "additional_drop" ) )
		{
			float		fChance			   = pKVListItem->GetFloat( "chance", 0.0f );
			bool		bPremiumOnly	   = pKVListItem->GetBool( "premium_only", false );
			const char *pszLootList		   = pKVListItem->GetString( "loot_list", "" );
			const char *pszRequiredHoliday = pKVListItem->GetString( "required_holiday", NULL );
			const char *pszDropPerdiodStartDate = pKVListItem->GetString( "start_date", NULL );
			const char *pszDropPerdiodEndDate	= pKVListItem->GetString( "end_date", NULL );

			int iRequiredHolidayIndex = pszRequiredHoliday
									  ? EconHolidays_GetHolidayForString( pszRequiredHoliday )
									  : kHoliday_None;

			RTime32 dropStartDate = ( pszDropPerdiodStartDate && pszDropPerdiodStartDate[0] )
							? CRTime::RTime32FromFmtString( "YYYY-MM-DD hh:mm:ss" , pszDropPerdiodStartDate )
							: RTime32(0);	// Default to the start of time

			// Check that if we convert back to a string, we get the same value
			char rtimeBuf[k_RTimeRenderBufferSize];
			SCHEMA_INIT_CHECK(
				pszDropPerdiodStartDate == NULL || Q_strcmp( CRTime::RTime32ToString( dropStartDate, rtimeBuf ), pszDropPerdiodStartDate ) == 0,
				"Malformed start drop date \"%s\" for additional_drop in lootlist %s.  Must be of the form \"YYYY-MM-DD hh:mm:ss\"", pszDropPerdiodStartDate, m_pszName );


			RTime32 dropEndDate = ( pszDropPerdiodEndDate && pszDropPerdiodEndDate[0] )
						  ? CRTime::RTime32FromFmtString( "YYYY-MM-DD hh:mm:ss" , pszDropPerdiodEndDate )
						  : ~RTime32(0);	// Default to the end of time

			// Check that if we convert back to a string, we get the same value
			SCHEMA_INIT_CHECK(
				pszDropPerdiodEndDate == NULL || Q_strcmp( CRTime::RTime32ToString( dropEndDate, rtimeBuf ), pszDropPerdiodEndDate ) == 0,
				"Malformed end drop date \"%s\" for additional_drop in lootlist %s.  Must be of the form \"YYYY-MM-DD hh:mm:ss\"", pszDropPerdiodEndDate, m_pszName );

			SCHEMA_INIT_CHECK(
				fChance > 0.0f && fChance <= 1.0f,
				"Loot list %s: Invalid \"additional_drop\" chance %.2f", m_pszName, fChance );

			SCHEMA_INIT_CHECK(
				pszLootList && pszLootList[0],
				"Loot list %s: Missing \"additional_drop\" loot list name", m_pszName );

			SCHEMA_INIT_CHECK(
				(pszRequiredHoliday == NULL) == (iRequiredHolidayIndex == kHoliday_None),
				"Loot list %s: Unknown or missing holiday \"%s\"", m_pszName, pszRequiredHoliday ? pszRequiredHoliday : "(null)" );

			if ( pszLootList )
			{
				const CEconLootListDefinition *pLootListDef = GetItemSchema()->GetLootListByName( pszLootList );
				SCHEMA_INIT_CHECK(
					pLootListDef,
					"Loot list %s: Invalid \"additional_drop\" loot list \"%s\"", m_pszName, pszLootList );

				if ( pLootListDef )
				{
					drop_period_t dropPeriod = { dropStartDate, dropEndDate };
					loot_list_additional_drop_t additionalDrop = { fChance, bPremiumOnly, pszLootList, iRequiredHolidayIndex, dropPeriod };
					m_AdditionalDrops.AddToTail( additionalDrop );
				}
			}
			continue;
		}
		else if ( !Q_strcmp( pszName, "property_generators" ) )
		{
			SCHEMA_INIT_SUBSTEP( BCommonInitPropertyGeneratorsFromKV( m_pszName, &m_PropertyGenerators, pKVListItem, pVecErrors ) );
			continue;
		}
#endif // GC_DLL

		int iDef = 0;
		// First, see if we've got a loot list name, for embedded loot lists
		int iIdx = 0;
		if ( GetItemSchema()->GetLootListByName( pszName, &iIdx ) )
		{
			// HACKY: Store loot list indices as negatives, starting from -1, because 0 is a valid item index
			iDef = 0 - 1 - iIdx;
		}
		else
		{
			// Not a loot list. See if it's an item index. Check the first character.
			if ( pszName[0] >= '0' && pszName[0] <= '9' )
			{
				iDef = atoi( pszName );
			}
			else
			{
				// Not a number. See if we can find an item def with a matching name.
				const CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszName );
				if ( pDef )
				{
					iDef = pDef->GetDefinitionIndex();
				}

				SCHEMA_INIT_CHECK( 
					pDef != NULL,
					"Loot list %s: Item definition \"%s\" was not found", m_pszName, pszName );
			}
		}

		// Default to the start dropping at the start of time and end dropping at the end of time
		drop_period_t dropPeriod = { RTime32(0), ~RTime32(0) };

		// Make sure we never put non-enabled items into loot lists
		if ( iDef > 0 )
		{
			const CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinition( iDef );
			SCHEMA_INIT_CHECK( 
				pItemDef != NULL,
				"Loot list %s: Item definition index \"%s\" (%d) was not found", m_pszName, pszName, iDef );

			static CSchemaAttributeDefHandle pAttribDef_StartDropDate( "start drop date" );
			static CSchemaAttributeDefHandle pAttribDef_EndDropDate( "end drop date" );

			CAttribute_String value;
			// Check for start drop date attribute on this item
			if ( FindAttribute( pItemDef, pAttribDef_StartDropDate, &value ) )
			{
				const char* pszStartDate = value.value().c_str();
				dropPeriod.m_DropStartDate = CRTime::RTime32FromFmtString( "YYYY-MM-DD hh:mm:ss" , pszStartDate );

				// Check that if we convert back to a string, we get the same value
				char rtimeBuf[k_RTimeRenderBufferSize];
				SCHEMA_INIT_CHECK(
					Q_strcmp( CRTime::RTime32ToString( dropPeriod.m_DropStartDate, rtimeBuf ), pszStartDate ) == 0,
					"Malformed start drop date \"%s\" for item %s.  Must be of the form \"YYYY-MM-DD hh:mm:ss\"",
					pszStartDate, pItemDef->GetDefinitionName() );
			}

			// Check for end drop date attribute on this item
			if ( FindAttribute( pItemDef, pAttribDef_EndDropDate, &value ) )
			{
				const char* pszEndDate = value.value().c_str();
				dropPeriod.m_DropEndDate = CRTime::RTime32FromFmtString( "YYYY-MM-DD hh:mm:ss" , pszEndDate );

				// Check that if we convert back to a string, we get the same value
				char rtimeBuf[k_RTimeRenderBufferSize];
				SCHEMA_INIT_CHECK(
					Q_strcmp( CRTime::RTime32ToString( dropPeriod.m_DropEndDate, rtimeBuf ), pszEndDate ) == 0,
					"Malformed end drop date \"%s\" for item %s.  Must be of the form \"YYYY-MM-DD hh:mm:ss\"", pszEndDate, pItemDef->GetDefinitionName() );
			}

#ifdef GC_DLL
			if ( pItemDef )
			{
				SCHEMA_INIT_CHECK( 
					true == pItemDef->BEnabled(), 
					"Loot list %s: Item definition \"%s\" (%d) isn't enabled, not allowed in loot lists", m_pszName, pItemDef->GetDefinitionName(), iDef );
			}
#endif // GC_DLL
		}

		float fItemWeight = 0.f;
#ifdef GC_DLL
		fItemWeight = bCollectionLootList ? 1.0f : pKVListItem->GetFloat();
		SCHEMA_INIT_CHECK( 
			fItemWeight > 0.0f,
			"Loot list %s: Item definition index \"%s\" (%d) has invalid weight %.2f", m_pszName, pszName, iDef, fItemWeight );
#endif // GC_DLL

		// Add this item
		drop_item_t dropItem = { iDef, fItemWeight, dropPeriod };
		m_DropList.AddToTail( dropItem );
	}

#ifdef GC_DLL

	int nNumTimeLimitedItems = 0;
	FOR_EACH_VEC( m_DropList, i )
	{
		// If either the start date or the end date is set, tally it up as a time limited item
		if( m_DropList[i].m_dropPeriod.m_DropStartDate != RTime32(0) || m_DropList[i].m_dropPeriod.m_DropEndDate != ~RTime32(0) )
		{
			++nNumTimeLimitedItems;
		}
	}

	// Verify that at least one item in a lootlist does not have a drop period that limits when it can drop.
	// This guarantees that we will always drop *something*
	SCHEMA_INIT_CHECK( m_DropList.Count() > nNumTimeLimitedItems, "Lootlist \"%s\" is made up entirely of limited-time items!  At least one must not be time-limited.", m_pszName );
#endif

	return SCHEMA_INIT_SUCCESS();
}

#ifdef GC_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static bool BContainsDuplicateItemDefs( const CUtlVector<CEconLootListDefinition::rolled_item_defs_t>& vecItemDefsA, const CUtlVector<CEconLootListDefinition::rolled_item_defs_t>& vecItemDefsB )
{
	CUtlHashtable<const CEconItemDefinition *> hashItemDefs;

	auto BPopulateAndLookForDupes = [&] ( const CUtlVector<CEconLootListDefinition::rolled_item_defs_t>& vecItemDefs )
	{
		for ( const auto& rolledItemDef : vecItemDefs )
		{
			if ( hashItemDefs.HasElement( rolledItemDef.m_pItemDef ) )
				return true;

			hashItemDefs.Insert( rolledItemDef.m_pItemDef );
		}

		return false;
	};

	return BPopulateAndLookForDupes( vecItemDefsA )
		|| BPopulateAndLookForDupes( vecItemDefsB );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconLootListDefinition::BGenerateSingleRollRandomItems( const CEconGameAccount *pGameAccount, bool bFreeAccount, CUtlVector<CEconItem *> *out_pvecItems, const CUtlVector< item_definition_index_t > *pVecAvoidItemDefs /*= NULL*/ ) const
{
	Assert( out_pvecItems );

	// Where is our source of random numbers coming from? If we're a no-dupe list,
	// we want to have reproducible state so we use our account ID as a unique-ish
	// seed.
	//
	// Wrap the whole thing in a smart pointer so we clean up whenever/however we
	// leave.
	std::unique_ptr<IUniformRandomStream> pRandomStream( [=]() -> IUniformRandomStream *
	{
		if ( !BIsInternalNoDupesLootList() || !pGameAccount )
			return new CDefaultUniformRandomStream;
	
	
		CUniformRandomStream *pAccountUniformRandomStream = new CUniformRandomStream;
		pAccountUniformRandomStream->SetSeed( pGameAccount->Obj().m_unAccountID );

		return pAccountUniformRandomStream;
	}() );

	// Make however many passes through our loot list code until we've generated the
	// right number of passing sets. (Most loot lists let anything pass. Some specify
	// no duplicate definitions allowed.)
	CUtlVector<rolled_item_defs_t> vecCumulativeItemDefs;			// total list of everything we've generated so far in any number of result sets
	CUtlVector<rolled_item_defs_t> vecItemDefs;						// current list under evaluation
	int iNoDupesIterations = 0;

	// This actually isn't guaranteed to converge, and is guaranteed not to converge if
	// we set up a broken lootlist. We set a really high bar here to catch broken/pathologically
	// bad cases here without grinding the whole GC to a halt.
	enum { kUpperBoundIterationSanityCheck = 2000 };
	int iTotalIterations = 0;
	
	while ( true )
	{
		// Don't runaway.
		iTotalIterations++;
		if ( iTotalIterations >= kUpperBoundIterationSanityCheck )
			return false;

		// Generate all of our item defs and their lootlists
		vecItemDefs.Purge();
		if ( !RollRandomItemsAndAdditionalItems( pRandomStream.get(), bFreeAccount, &vecItemDefs, pVecAvoidItemDefs ) )
			return false;

		// If we don't care about dupes and we got any results at all we're done.
		if ( !BIsInternalNoDupesLootList() )
			break;

		// If we do care about dupes and we have some, ignore this set of items.
		if ( BContainsDuplicateItemDefs( vecItemDefs, vecCumulativeItemDefs ) )
			continue;

		// Did we get to the right result set?
		iNoDupesIterations++;
		if ( iNoDupesIterations > m_iNoDupesIterations )
			break;
		
		// Store off the list of definition indices we've already used them so they don't get reused
		// in a later set.
		vecCumulativeItemDefs.AddVectorToTail( vecItemDefs );
	}
	
	// If we get down to here, we expect that we've rolled at least one item def
	Assert( vecItemDefs.Count() > 0 );
	FOR_EACH_VEC( vecItemDefs, i )
	{
		const rolled_item_defs_t& rolledDef = vecItemDefs[i];
		Assert( rolledDef.m_pItemDef );
		Assert( rolledDef.m_vecAffectingLootLists.Count() > 0 );

		// Create the items
		CEconItem *pItem = GEconManager()->GetItemFactory().CreateSpecificItem( pGameAccount, rolledDef.m_pItemDef->GetDefinitionIndex() );
		out_pvecItems->AddToTail( pItem );
		
		// Go through and let all the affecting lootlists attach their attributes to the item
		FOR_EACH_VEC( rolledDef.m_vecAffectingLootLists, j )
		{
			const CEconLootListDefinition *pLootList = rolledDef.m_vecAffectingLootLists[j];
			Assert( pLootList );
			if ( !pLootList->BAttachLootListAttributes( pGameAccount, pItem ) )
				return false;
		}
	}

	return ( out_pvecItems->Count() > 0 );
}

class CRollSimulator
{
public:

	CRollSimulator()
		: m_mapRarityCounts( StringLessThan )
		, m_mapCounts( DefLessFunc( item_definition_index_t ) )
		, m_mapUnusualHatEffectsCount( DefLessFunc( uint32 ) ) 
		, m_mapUnusualTauntEffectsCount( DefLessFunc( uint32 ) )
		, m_nNumIters( 0 )
	{}

	void RollLootlist( const char* pszLootListName, int nRolls, bool bWipePreviousResults = false )
	{
		const CEconLootListDefinition* pLootlist = GetItemSchema()->GetLootListByName( pszLootListName );

		if ( !pLootlist )
		{
			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Invalid lootlist \"%s\".\n", pszLootListName );
			return;
		}

		// Clear out results first?
		if ( bWipePreviousResults )
		{
			m_mapCounts.Purge();
			m_mapRarityCounts.Purge();
			m_DropsMaxes.Clear();
			m_DropsTotals.Clear();
		}

		while( nRolls-- )
		{
			AutoYield();
			CUtlVector<CEconItem *> vecItems;
			pLootlist->BGenerateSingleRollRandomItems( NULL, false, &vecItems );

			// Tally up what we got.
			for( auto pItem : vecItems )
			{
				AutoYield();
				// Insert item def if we need
				item_definition_index_t nDefIndex = pItem->GetDefinitionIndex();
				auto idx = m_mapCounts.Find( nDefIndex );
				if ( m_mapCounts.InvalidIndex() == idx )
				{
					idx = m_mapCounts.Insert( nDefIndex );
				}

				// Insert rarity if needed
				const CEconItemRarityDefinition* pItemRarity = GetItemSchema()->GetRarityDefinition( pItem->GetItemDefinition()->GetRarity() );
				if ( pItemRarity )
				{
					const char* pszRarity = GGCGameBase()->LocalizeToken( pItemRarity->GetLocKey() , k_Lang_English );
					auto rarityIdx = m_mapRarityCounts.Find( pszRarity );
					if ( m_mapRarityCounts.InvalidIndex() == rarityIdx )
					{
						rarityIdx = m_mapRarityCounts.Insert( pszRarity, 0 );
					}

					m_mapRarityCounts[ rarityIdx ] += 1;
				}

				// Count
				DropResult_t& result = m_mapCounts[ idx ];
				++result.m_nRollCount;
				++m_DropsTotals.m_nRollCount;
				m_DropsMaxes.m_nRollCount = Max( m_DropsMaxes.m_nRollCount, result.m_nRollCount );

				// Strange count
				if ( BIsItemStrange( pItem ) )
				{
					++result.m_nStrangeCount;
					++m_DropsTotals.m_nStrangeCount;
					m_DropsMaxes.m_nStrangeCount = Max( m_DropsMaxes.m_nStrangeCount, result.m_nStrangeCount );
				}

				// Unusual count
				static CSchemaAttributeDefHandle pAttrDef_ParticleEffect( "attach particle effect" );
				static CSchemaAttributeDefHandle pAttrDef_TauntUnusualAttr( "on taunt attach particle index" );
				if ( pAttrDef_ParticleEffect && pAttrDef_TauntUnusualAttr )
				{
					uint32 nUnusualHatValue = 0;
					uint32 nUnusualTauntValue = 0;
					pItem->FindAttribute( pAttrDef_ParticleEffect, &nUnusualHatValue );
					pItem->FindAttribute( pAttrDef_TauntUnusualAttr, &nUnusualTauntValue );
					// Cant use quality cause of old legacy items.  Quality is just a quick test
					if ( nUnusualHatValue != 0 || nUnusualTauntValue != 0 )
					{
						++result.m_nUnusualCount;
						++m_DropsTotals.m_nUnusualCount;
						m_DropsMaxes.m_nUnusualCount = Max( m_DropsMaxes.m_nUnusualCount, result.m_nUnusualCount );
					}

					if ( nUnusualHatValue )
					{
						nUnusualHatValue = (uint32)((float&)nUnusualHatValue);
						auto idx = m_mapUnusualHatEffectsCount.Find( nUnusualHatValue );
						if ( idx == m_mapUnusualHatEffectsCount.InvalidIndex() )
						{
							idx = m_mapUnusualHatEffectsCount.Insert( nUnusualHatValue, 0 );
						}

						++m_mapUnusualHatEffectsCount[ idx ];
					}

					if ( nUnusualTauntValue )
					{
						nUnusualTauntValue = (uint32)((float&)nUnusualTauntValue);
						auto idx = m_mapUnusualTauntEffectsCount.Find( nUnusualTauntValue );
						if ( idx == m_mapUnusualTauntEffectsCount.InvalidIndex() )
						{
							idx = m_mapUnusualTauntEffectsCount.Insert( nUnusualTauntValue, 0 );
						}

						++m_mapUnusualTauntEffectsCount[ idx ];
					}
				}
			}

			// Delete what we got
			vecItems.PurgeAndDeleteElements();
		}
	}

	void PrintRarityBreakdwn()
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "-- Rarity breakdown:\n" );
		int nMaxDigits = 0;
		while( m_mapRarityCounts.Count() )
		{
			// Go through and print the most rolled to least rolled rarities
			auto maxIdx = m_mapRarityCounts.InvalidIndex();

			// Find the most hit
			FOR_EACH_MAP_FAST( m_mapRarityCounts, i )
			{
				AutoYield();
				if ( maxIdx == m_mapRarityCounts.InvalidIndex() ||  m_mapRarityCounts[ i ] > m_mapRarityCounts[ maxIdx ] )
				{
					maxIdx = i;
				}

				nMaxDigits = Max( nMaxDigits, NumDigits( m_mapRarityCounts[ maxIdx ] ) );
			}

			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%9s %*d %.3f%%\n",
					  m_mapRarityCounts.Key( maxIdx ), nMaxDigits, m_mapRarityCounts[ maxIdx ],
					  100.f * (float)m_mapRarityCounts[ maxIdx ] / m_DropsTotals.m_nRollCount );
			m_mapRarityCounts.RemoveAt( maxIdx );
		}

		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%-*s  %*s  %*s\n", 9 + 1 + NumDigits( m_DropsMaxes.m_nRollCount ) + 6,
				  "-- Item breakdown", NumDigits( m_DropsMaxes.m_nStrangeCount ) + 1, "S", NumDigits( m_DropsMaxes.m_nUnusualCount ), "U"  );
		
	}

	void PrintUnusualCounts()
	{
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "-- Unusual Hats breakdown:\n" );
		int nTotal = PrintUnusualsForType( m_mapUnusualHatEffectsCount );
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "-- %d total unusual hats\n", nTotal );
		
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "-- Unusual Taunts breakdown:\n" );
		nTotal = PrintUnusualsForType( m_mapUnusualTauntEffectsCount );
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "-- %d total unusual taunts\n", nTotal );
	}

	void PrintTotals()
	{
		while( m_mapCounts.Count() )
		{
			// Go through and print the most rolled to least rolled
			auto maxIdx = m_mapCounts.InvalidIndex();

			// Find the most hit
			FOR_EACH_MAP_FAST( m_mapCounts, i )
			{
				AutoYield();
				if ( maxIdx == m_mapCounts.InvalidIndex() ||  m_mapCounts[ i ].m_nRollCount > m_mapCounts[ maxIdx ].m_nRollCount )
				{
					maxIdx = i;
				}
			}

			DropResult_t& result = m_mapCounts[ maxIdx ];
			CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinition( m_mapCounts.Key( maxIdx ) );

			const CEconItemRarityDefinition* pItemRarity = GetItemSchema()->GetRarityDefinition( pItemDef->GetRarity() );
			const char* pszRarity = pItemRarity ? GGCGameBase()->LocalizeToken( pItemRarity->GetLocKey() , k_Lang_English ) : "";
			CFmtStr rollString( "%*d %2.3f%%", NumDigits( m_DropsMaxes.m_nRollCount ), result.m_nRollCount,
								100.f * (float)result.m_nRollCount / m_DropsTotals.m_nRollCount );
			CFmtStr strangeString( "%*d", NumDigits( m_DropsMaxes.m_nStrangeCount ), result.m_nStrangeCount );
			CFmtStr unusualString( "%*d", NumDigits( m_DropsMaxes.m_nUnusualCount ), result.m_nUnusualCount );
			const char* pszItemname = GGCGameBase()->LocalizeToken( pItemDef->GetCustomPainkKitDefinition() ? pItemDef->GetCustomPainkKitDefinition()->GetName() : pItemDef->GetItemBaseName(), k_Lang_English );

			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%9s %s  %s  %s  %s\n", pszRarity, rollString.Get(), strangeString.Get(), unusualString.Get(), pszItemname );

			m_mapCounts.RemoveAt( maxIdx );
		}

		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "-- Total Items: %d Strange: %d (%.3f%%) Unusual: %d (%.3f%%)\n"
			, m_DropsTotals.m_nRollCount
			, m_DropsTotals.m_nStrangeCount
			, ( 100.f * (float)m_DropsTotals.m_nStrangeCount / m_DropsTotals.m_nRollCount )
			, m_DropsTotals.m_nUnusualCount
			, ( 100.f * (float)m_DropsTotals.m_nUnusualCount / m_DropsTotals.m_nRollCount ) );
	}

	struct DropResult_t
	{
		DropResult_t() { Clear(); }

		void Clear()
		{
			m_nStrangeCount = 0;
			m_nUnusualCount = 0;
			m_nRollCount = 0;
		}

		int m_nStrangeCount;
		int m_nUnusualCount;
		int m_nRollCount;
	};

	const DropResult_t& GetTotalDrops() const { return m_DropsTotals; }
	const DropResult_t& GetMaxesDrops() const { return m_DropsMaxes; }

private:

	int PrintUnusualsForType( CUtlMap< uint32, int >& mapUnusuals )
	{
		int nMaxDigits = 0;
		int nTotal = 0;
		FOR_EACH_MAP_FAST( mapUnusuals, i )
		{
			nTotal += mapUnusuals[ i ];
		}

		while( mapUnusuals.Count() )
		{
			// Go through and print the most rolled to least rolled unusual effects
			auto maxIdx = mapUnusuals.InvalidIndex();

			// Find the most hit
			FOR_EACH_MAP_FAST( mapUnusuals, i )
			{
				AutoYield();
				if ( maxIdx == mapUnusuals.InvalidIndex() ||  mapUnusuals[ i ] > mapUnusuals[ maxIdx ] )
				{
					maxIdx = i;
				}

				nMaxDigits = Max( nMaxDigits, NumDigits( mapUnusuals[ maxIdx ] ) );
			}

			char particleNameEntry[128];
			Q_snprintf( particleNameEntry, ARRAYSIZE( particleNameEntry ), "#Attrib_Particle%d", mapUnusuals.Key( maxIdx ) );
			const char* pszParticleName = GGCGameBase()->LocalizeToken( particleNameEntry, k_Lang_English );

			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%*d %.3f%% %s (%d)\n",
				nMaxDigits,	  
				mapUnusuals[ maxIdx ],
				100.f * (float)mapUnusuals[ maxIdx ] / nTotal,
				pszParticleName,
				mapUnusuals.Key( maxIdx ) );

			mapUnusuals.RemoveAt( maxIdx );
		}

		return nTotal;
	}

	void AutoYield()
	{
		if ( ++m_nNumIters % 100 == 0 )
		{
			if ( GJobCur().BYieldIfNeeded() )
			{
				// If we re-entered logon surge we should go away for a while
				while ( GGCGameBase()->BIsInLogonSurge() )
				{
					GJobCur().BYieldingWaitOneFrame();
				}
			}
		}
	};

	int NumDigits( int nNumber )
	{
		int digits = 0;

		if ( nNumber <= 0) 
		{
			digits = 1; 
		}

		while ( nNumber ) 
		{
			nNumber /= 10;
			++digits;
		}

		return digits;
	}

	

	CUtlMap< item_definition_index_t, DropResult_t > m_mapCounts;
	CUtlMap< uint32, int > m_mapUnusualHatEffectsCount;
	CUtlMap< uint32, int > m_mapUnusualTauntEffectsCount;
	// Rarity name is the key
	CUtlMap< const char*, int > m_mapRarityCounts;
	DropResult_t m_DropsTotals;
	DropResult_t m_DropsMaxes;
	size_t m_nNumIters;
};

GC_CON_COMMAND( simulate_lootlist_contents, "<lootlist> <iterations> Check item distribution from a given lootlist n times" )
{
	if ( !BCheckArgs( 2, args, simulate_lootlist_contents_command ) )
		return;

	const char* pszLootListName = args[1];
	int nRolls = args.ArgC() == 3 ? atoi( args[2] ) : 1000;

	CRollSimulator simulator;
	simulator.RollLootlist( pszLootListName, nRolls );
	simulator.PrintRarityBreakdwn();
	simulator.PrintUnusualCounts();
	simulator.PrintTotals();
}


#ifdef GC_DLL
class CItemSourceFinder
{
public:
	CItemSourceFinder( const char* pszItemName )
		: m_pItemDef( GetItemSchema()->GetItemDefinitionByName( pszItemName ) )
	{
		Assert( m_pItemDef );
		if ( !m_pItemDef )
		{
			EG_ERROR( SPEW_CONSOLE, "%s is not a valid item", pszItemName );
			return;
		}

		// Find out what series this crate belongs to.
		static CSchemaAttributeDefHandle pAttr_CrateSeries( "set supply crate series" );
		if ( !pAttr_CrateSeries )
			return;

		auto& mapItemDefs = GetItemSchema()->GetItemDefinitionMap();
		auto& mapRevolvingLootlists = GetItemSchema()->GetRevolvingLootLists();

		CUtlDict< int > dictSeenLootlists;

		// Look through all the item defs and see if any of them statically specify a lootlist that they want to open
		FOR_EACH_MAP_FAST( mapItemDefs, i )
		{
			const CEconItemDefinition* pSourceItemDef = mapItemDefs[ i ];
			const CEconLootListDefinition* pLootlist = NULL;

			const CEconTool_Gift* pGift = pSourceItemDef->GetTypedEconTool< CEconTool_Gift >();
			// Self-opening crate?
			if ( pGift )
			{
				pLootlist = GetItemSchema()->GetLootListByName(	pGift->GetLootListName() );
			}
			else // Crate with an item series?
			{
				int iCrateSeries;
				{
					float fCrateSeries;		// crate series ID is stored as a float internally because we hate ourselves
					if ( !FindAttribute_UnsafeBitwiseCast<attrib_value_t>( pSourceItemDef, pAttr_CrateSeries, &fCrateSeries ) || fCrateSeries == 0.0f )
						continue;

					iCrateSeries = fCrateSeries;
				}

				auto idx = mapRevolvingLootlists.Find( iCrateSeries );
				if ( idx == mapRevolvingLootlists.InvalidIndex() )
					continue;

				pLootlist = GetItemSchema()->GetLootListByName( mapRevolvingLootlists[ idx ] );
			}

			if ( !pLootlist )
				continue;

			// Mark that we've seen this lootlist already
			dictSeenLootlists.Insert( pLootlist->GetName() );

			DropSource_t& source = m_vecSources[ m_vecSources.AddToTail() ];
			source.m_pDroppingItem = pSourceItemDef;
			ChanceForItemFromLootlist( m_pItemDef, pLootlist, source.lootlistSource );
		}

		CEconItemDefinition* pCrateItemDef = GetItemSchema()->GetItemDefinitionByName( "Supply Crate" );
		Assert( pCrateItemDef );

		// Go through all the revolving lootlists and see if they have the item.  Assume that
		// they're from a "Supply Crate".
		FOR_EACH_MAP_FAST( mapRevolvingLootlists, i )
		{
			if ( !pCrateItemDef )
				continue;

			if ( mapRevolvingLootlists.Key( i ) <= 0 )
				continue;

			auto pLootlist = GetItemSchema()->GetLootListByName( mapRevolvingLootlists[ i ] );
			if ( !pLootlist )
				continue;

			// This lootlist was on a different crate already
			if ( dictSeenLootlists.Find( pLootlist->GetName() ) != dictSeenLootlists.InvalidIndex() )
				continue;

			DropSource_t& source = m_vecSources[ m_vecSources.AddToTail() ];
			source.m_pDroppingItem = pCrateItemDef;
			ChanceForItemFromLootlist( m_pItemDef, pLootlist, source.lootlistSource );
		}

		// Not available at all!
		if ( !m_vecSources.Count() )
		{
			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Not available from any sources!\n" );
			return;
		}

		// Sort greatest % chance
		auto lambdaSort = [] ( DropSource_t const *pLHS, DropSource_t const *pRHS ) -> int
		{
			return pLHS->lootlistSource.m_flChance < pRHS->lootlistSource.m_flChance;
		};
		m_vecSources.Sort( lambdaSort );
	}

	void PrintSources()
	{
		FOR_EACH_VEC( m_vecSources, i )
		{
			m_vecSources[ i ].PrintSources();
		}
	}

	bool BDropsFromLootlist( const CEconLootListDefinition* pLootlist )
	{
		FOR_EACH_VEC( m_vecSources, i )
		{
			if ( m_vecSources[ i ].lootlistSource.BDropsFromLootlist( pLootlist ) )
				return true;
		}

		return false;
	}

private:

	struct DropSource_t
	{
		void PrintSources()
		{
			if ( lootlistSource.m_flChance == 0.f )
				return;

			bool bSelfOpening = m_pDroppingItem->GetTypedEconTool< CEconTool_Gift >() != NULL;

			// Print the name of the item, and whether it's a self-opening item, or a crate
			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%3.5f%% %s (%d - %s)\n", 
				lootlistSource.m_flChance * 100.f,
				m_pDroppingItem->GetDefinitionName(),
				m_pDroppingItem->GetDefinitionIndex(),
				bSelfOpening ? "Self-Opening" : "Crate/Case" );

			// Print all the sources
			lootlistSource.PrintSources( 1 );

			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\n" );
		}

		struct LootListSource_t
		{
			LootListSource_t()
				: m_flChance( 0.f )
			{}

			bool BDropsFromLootlist( const CEconLootListDefinition* pLootlist )
			{
				// Skip no-chance entries
				if ( m_flChance == 0.f )
					return false;


				if ( m_pDroppingLootlist == pLootlist )
					return true;

				FOR_EACH_VEC( m_vecLootlistSources, i )
				{
					if ( m_vecLootlistSources[ i ].BDropsFromLootlist( pLootlist ) )
						return true;
				}

				return false;
			}

			void PrintSources( int nInset )
			{
				// Skip no-chance entries
				if ( m_flChance == 0.f )
					return;

				auto& mapRevolvingLootlists = GetItemSchema()->GetRevolvingLootLists();
				int nRevolvingIdx = mapRevolvingLootlists.InvalidIndex();

				// Check if our lootlist is one of the revolving lootlists.  If so, we want
				// to print out its index within revolving_lootlists, so we can map that to
				// the attribute value of supply_crate_series (187)
				FOR_EACH_MAP_FAST( mapRevolvingLootlists, i )
				{
					if ( V_stricmp( mapRevolvingLootlists[ i ], m_pDroppingLootlist->GetName() ) == 0 )
					{
						nRevolvingIdx = mapRevolvingLootlists.Key( i );
						break;
					}
				}
			
				if ( nRevolvingIdx != mapRevolvingLootlists.InvalidIndex() && nRevolvingIdx > 0 )
				{
					// It's in revolving_lootlists.  Print its index in there.
					EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%*.5f%% (%d) %s\n",
							4 * nInset + 5,
							m_flChance * 100.f,
							nRevolvingIdx,
							m_pDroppingLootlist->GetName() );
				}
				else 
				{
					// Not in the revolving lootlist
					EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%*.5f%% %s\n",
							4 * nInset + 5,
							m_flChance * 100.f,
							m_pDroppingLootlist->GetName() );
				}

				// Sort greatest % chance
				auto lambdaSort = [] ( LootListSource_t const *pLHS, LootListSource_t const *pRHS ) -> int
				{
					return pLHS->m_flChance < pRHS->m_flChance;
				};

				m_vecLootlistSources.Sort( lambdaSort );

				// Print out children indented a lil bit
				FOR_EACH_VEC( m_vecLootlistSources, i )
				{
					m_vecLootlistSources[i].PrintSources( nInset + 1 );
				}
			}

			float m_flChance;
			const CEconLootListDefinition* m_pDroppingLootlist;

			CCopyableUtlVector< LootListSource_t > m_vecLootlistSources;
		};

		const CEconItemDefinition* m_pDroppingItem;
		LootListSource_t lootlistSource;
	};

	float ChanceForItemFromLootlist( const CEconItemDefinition* pItemDef, const CEconLootListDefinition* pLootlist, DropSource_t::LootListSource_t& lootlistSource )
	{
		auto& vecContents = pLootlist->GetLootListContents();
		// Accumulate our chance of dropping the specified item
		lootlistSource.m_flChance = 0.f;
		lootlistSource.m_pDroppingLootlist = pLootlist;

		// Gather the items in this lootlist that we're able to roll for at this time
		float flTotalWeight = 0.f;
		CUtlVector<CEconLootListDefinition::drop_item_t> vecValidContents;
		FOR_EACH_VEC( vecContents, i )
		{
			if( !vecContents[ i ].m_dropPeriod.IsValidForTime( CRTime::RTime32TimeCur() ) )
				continue;

			flTotalWeight += vecContents[ i ].m_flWeight;
			vecValidContents.AddToTail( vecContents[ i ] );
		}

		// Go through valid contents, and see if the specified item is in there
		FOR_EACH_VEC( vecValidContents, i )
		{
			const int iItemDef = vecValidContents[ i ].m_iItemOrLootlistDef;
			const float flChance = vecValidContents[ i ].m_flWeight / flTotalWeight;

			if ( iItemDef < 0 ) // Lootlist
			{
				int iLLIndex = (iItemDef * -1) - 1;
				auto pSubLootlist = GetItemSchema()->GetLootListByIndex( iLLIndex );
				
				// One of our sub-lootlists might drop it.  Add in it's chance within
				// the sub-lootlist scaled by the chance to roll that sub-lootlist.
				auto& subSource = lootlistSource.m_vecLootlistSources[ lootlistSource.m_vecLootlistSources.AddToTail() ];
				lootlistSource.m_flChance += ChanceForItemFromLootlist( pItemDef, pSubLootlist, subSource ) * flChance;
			}
			else if ( pItemDef->GetDefinitionIndex() == iItemDef )
			{
				// We drop it!  Add the chance
				lootlistSource.m_flChance += flChance;
			}
		}

		// Treat additional drops just the same as nested lootlists.
		auto& vecAdditionalDrops = pLootlist->GetAdditionalDrops();
		FOR_EACH_VEC( vecAdditionalDrops, i )
		{
			auto& additionalDrop = vecAdditionalDrops[ i ];
			if ( !additionalDrop.m_dropPeriod.IsValidForTime( CRTime::RTime32TimeCur() ) )
				continue;
		
			auto pSubLootlist = GetItemSchema()->GetLootListByName( additionalDrop.m_pszLootListDefName );
			auto& subSource = lootlistSource.m_vecLootlistSources[ lootlistSource.m_vecLootlistSources.AddToTail() ];
			lootlistSource.m_flChance += ChanceForItemFromLootlist( pItemDef, pSubLootlist, subSource ) * additionalDrop.m_fChance;
		}

		// Return the total chance
		return lootlistSource.m_flChance;
	}

	CUtlVector< DropSource_t > m_vecSources;
	const CEconItemDefinition* m_pItemDef;
};

GC_CON_COMMAND( item_sources, "Lists the sources for obtaining a list of specific items" )
{
	if ( !BCheckArgs( 1, args, item_sources_command ) )
		return;

	for ( int i=1; i < args.ArgC(); ++i )
	{
		const char* pszItemName = args[i];
		const CEconItemDefinition* pItemDef = GetItemSchema()->GetItemDefinitionByName( pszItemName );
		if ( pItemDef )
		{
			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\nChecking items for lootlists containing (%d) %s...\n", pItemDef->GetDefinitionIndex(), pItemDef->GetDefinitionName() );
			CItemSourceFinder source( pszItemName );
			source.PrintSources();
		}
		else
		{
			EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\n\"%s\" is not a valid item name\n", pszItemName );
		}
	}
}

GC_CON_COMMAND( list_keys, "Lists all the keys in the schema" )
{
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Spewing keys...\n" );

	auto& mapItemDefs = GetItemSchema()->GetItemDefinitionMap();
	FOR_EACH_MAP_FAST( mapItemDefs, i )
	{
		const CEconItemDefinition* pItemDef = mapItemDefs[ i ];
		if ( !pItemDef || !pItemDef->GetEconTool() || ( Q_strcmp( pItemDef->GetEconTool()->GetTypeName(), "decoder_ring" ) != 0 ) )
		{
			continue;
		}

		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%llu - %s\n", pItemDef->GetDefinitionIndex(), pItemDef->GetItemDefinitionName() );
	}
}

ConVar case_behavior_rolls_per_lootlist( "case_behavior_rolls_per_lootlist", "1000000", FCVAR_REPLICATED, "How many times to roll a lootlist" );
class CJobCaseBehaviorCheck : public CGCGameBaseJob
{
public:
	CJobCaseBehaviorCheck()
		: CGCGameBaseJob( GGCGameBase() )
	{}

	virtual bool BYieldingRunGCJob()
	{
		// Wait until logon surge ends to get going
		while ( GGCGameBase()->BIsInLogonSurge() )
		{
			GJobCur().BYieldingWaitOneFrame();
		}

		CRollSimulator simulator;

		// Convert the hash to something readable
		char pchSHAHex[41];
		memset( pchSHAHex, 0, sizeof( pchSHAHex ) );
		V_binarytohex( GetItemSchema()->GetSchemaSHA().m_shaDigest, 20, pchSHAHex, 41 );

		RTime32 now = CRTime::RTime32TimeCur();
		int nNewRecords = 0;

		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Beginning CJobCaseBehaviorCheck\n" );

		auto& lootlists = GetItemSchema()->GetRevolvingLootLists();

		// Go through all of the lootlists
		FOR_EACH_MAP( lootlists, i )
		{
			const CEconLootListDefinition* pEconLootlist = GetItemSchema()->GetLootListByName( lootlists[ i ] );
			if ( pEconLootlist )
			{
				// Check if we have data for this lootlist on this hash already
				{
					CSQLAccess sqlReadAccess;
					CUtlVector< CSchCaseBehavior > vecExistingResult;
					sqlReadAccess.AddBindParam( pchSHAHex );
					sqlReadAccess.AddBindParam( i );
					if ( sqlReadAccess.BYieldingReadRecordsWithWhereClause( &vecExistingResult, "SchemaSHA = ? and Series = ?", CSET_FULL( CSchCaseBehavior ) ) )
					{
						// Already got it?  Skip the work
						if ( vecExistingResult.Count() )
						{
							continue;
						}
					}
				}

				// Roll 'em up
				simulator.RollLootlist( pEconLootlist->GetName(), case_behavior_rolls_per_lootlist.GetInt(), true );

				// Insert record
				CSchCaseBehavior behavior;
				behavior.SetVarCharField( behavior.m_VarCharSchemaSHA, pchSHAHex, true, CSchCaseBehavior::k_iField_VarCharSchemaSHA );
				behavior.m_RTime32Date = now;
				behavior.m_unSeries = i;
				behavior.SetVarCharField( behavior.m_VarCharLootListName, pEconLootlist->GetName(), true, CSchCaseBehavior::k_iField_VarCharLootListName );
				behavior.m_fStrangeChance = 100.f * (float)simulator.GetTotalDrops().m_nStrangeCount / simulator.GetTotalDrops().m_nRollCount;
				behavior.m_fUnusualChance = 100.f * (float)simulator.GetTotalDrops().m_nUnusualCount / simulator.GetTotalDrops().m_nRollCount;

				CSQLAccess sqlAccess;
				sqlAccess.BBeginTransaction( "CJobCaseBehaviorCheck" );
				sqlAccess.BYieldingInsertOrUpdateOnPK( &behavior );
				sqlAccess.BCommitTransaction( true );
				++nNewRecords;
			}

			BYieldIfNeeded();
		}

		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Completed CJobUnusualChecker.  %d updated records.\n", nNewRecords );

		return true;
	}
};

void CEconItemSchema::PerformCaseBehaviorCheck()
{
	CJob* pJob = new CJobCaseBehaviorCheck();
	pJob->StartJobDelayed( NULL );
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconLootListDefinition::RollRandomItemsAndAdditionalItems( IUniformRandomStream *pRandomStream, bool bFreeAccount, CUtlVector<rolled_item_defs_t> *out_pVecRolledItems, const CUtlVector< item_definition_index_t > *pVecAvoidItemDefs ) const
{
	Assert( out_pVecRolledItems );

	// Roll to see what items we get from this loot list.
	bool bCreatedItems = RollRandomItemDef( pRandomStream, bFreeAccount, out_pVecRolledItems, pVecAvoidItemDefs );
	
	// Do we have additional drops?	
	FOR_EACH_VEC( m_AdditionalDrops, i )
	{
		if ( !bCreatedItems )
			break;

		// Is this within the period this is allowed to drop?
		if ( !m_AdditionalDrops[i].m_dropPeriod.IsValidForTime( CRTime::RTime32TimeCur() ) )
			continue;

		// Does this only apply to premium accounts?
		if ( m_AdditionalDrops[i].m_bPremiumOnly && bFreeAccount )
			continue;

		// Does this only apply on certain holidays?
		if ( m_AdditionalDrops[i].m_iRequiredHolidayIndex != kHoliday_None && !EconHolidays_IsHolidayActive( m_AdditionalDrops[i].m_iRequiredHolidayIndex, CRTime::RTime32TimeCur() ) )
			continue;

		// Random chance is in the range 0-1 so generate a value in that range to "roll".
		if ( pRandomStream->RandomFloat( 0.0f, 1.0f ) > m_AdditionalDrops[i].m_fChance )
			continue;

		// Roll!
		const char *pszAdditionalDropLootList = m_AdditionalDrops[i].m_pszLootListDefName;
		const CEconLootListDefinition *pLootListDef = GetItemSchema()->GetLootListByName( pszAdditionalDropLootList );
		if ( pLootListDef == NULL )
		{
			AssertMsg2( false, "Loot list '%s' specifies unknown additional drop '%s'", GetName(), pszAdditionalDropLootList );
			return false;
		}

		bCreatedItems &= pLootListDef->RollRandomItemsAndAdditionalItems( pRandomStream, bFreeAccount, out_pVecRolledItems );
	}

	// If we failed to create some items, we might still have chosen some item defs before those failures. In that case, clear
	// out any choices we've made so far
	if ( !bCreatedItems )
	{
		out_pVecRolledItems->Purge();
	}

	Assert( bCreatedItems == (out_pVecRolledItems->Count() > 0) );

	return bCreatedItems;
}


bool CEconLootListDefinition::RollRandomItemDef( IUniformRandomStream *pRandomStream, bool bFreeAccount, CUtlVector<rolled_item_defs_t> *out_pVecRolledItems, const CUtlVector< item_definition_index_t > *pVecAvoidItemDefs ) const
{
	Assert( out_pVecRolledItems );

	CUtlVector< rolled_item_defs_t > vecScratchDefs;
	CUtlVector<const drop_item_t*> vecValidDrops;

	// Gather the items in this lootlist that we're able to roll for at this time
	float flTotalWeight = 0.f;
	FOR_EACH_VEC( m_DropList, i )
	{
		if( !m_DropList[i].m_dropPeriod.IsValidForTime( CRTime::RTime32TimeCur() ) )
			continue;

		// Skip any item defs that are in our avoid list (if we have one)
		if ( pVecAvoidItemDefs )
		{
			item_definition_index_t defIndex = m_DropList[i].m_iItemOrLootlistDef;
			if ( pVecAvoidItemDefs->Find( defIndex ) != pVecAvoidItemDefs->InvalidIndex() )
				continue;
		}

		// If this is valid, add it to the list and add its weight to the total
		vecValidDrops.AddToTail( &m_DropList[i] );
		flTotalWeight += m_DropList[i].m_flWeight;
	}

	// Roll to see what item drops.
	float flRand = pRandomStream->RandomFloat(0.0f, 1.0f) * flTotalWeight;

	float flAccum = 0.0f;
	FOR_EACH_VEC( vecValidDrops, i )
	{
		flAccum += vecValidDrops[i]->m_flWeight;
		if ( flRand <= flAccum )
		{
			const int iItemDef = vecValidDrops[i]->m_iItemOrLootlistDef;	// not item_definition_index because it might also be a negative value to indicate a sub lootlist

			if ( iItemDef >= 0 )
			{
				const CEconItemDefinition* pItemDef = GetItemSchema()->GetItemDefinition( iItemDef );
				if( !pItemDef )
					return false;

				// Add the item def and the lootlist
				rolled_item_defs_t& rolledDef = vecScratchDefs[ vecScratchDefs.AddToTail() ];
				rolledDef.m_pItemDef = pItemDef;
			}
			else
			{
				// In the case where iItemDef is negative, it's a nested loot list. Ask that list to choose an item.
				// HACKY: Store loot list indices as negatives, starting from -1, because 0 is a valid item index
				int iLLIndex = (iItemDef * -1) - 1;
				const CEconLootListDefinition *pNestedLootList = GetItemSchema()->GetLootListByIndex( iLLIndex );
				if ( !pNestedLootList )
					return false;

				if( !pNestedLootList->RollRandomItemsAndAdditionalItems( pRandomStream, bFreeAccount, &vecScratchDefs, pVecAvoidItemDefs ) )
					return false;
			}

			// Add ourselves to the list of affecting lootlists, so that we and all nested loot lists will affect this item
			// We intentionally don't do this in our calling function because we don't want to include additional drops.
			FOR_EACH_VEC( vecScratchDefs, j )
			{
				vecScratchDefs[j].m_vecAffectingLootLists.AddToTail( this );
			}

			// We want to exit the loop here regardless of whether items were successfully so that we only perform a single
			// item-generating roll on this list.
			break;
		}
	}

	// Feed item defs, if they exist, back to our caller
	out_pVecRolledItems->AddVectorToTail( vecScratchDefs ); 
	
	// Did we successfully create any items?
	return ( vecScratchDefs.Count() > 0 );
}

//-----------------------------------------------------------------------------
// Purpose: find a list of lootlists with rarity from this lootlist
//-----------------------------------------------------------------------------
void CEconLootListDefinition::GetRarityLootLists( CUtlVector< const CEconLootListDefinition* > *out_pVecRarityLootList ) const
{
	Assert( out_pVecRarityLootList );
	if ( m_unRarity != k_unItemRarity_Any )
	{
		out_pVecRarityLootList->AddToTail( this );
	}

	FOR_EACH_VEC( m_DropList, i )
	{
		const int iItemDef = m_DropList[i].m_iItemOrLootlistDef;	// not item_definition_index because it might also be a negative value to indicate a sub lootlist
		if ( iItemDef < 0 )
		{
			// In the case where iItemDef is negative, it's a nested loot list. Ask that list to choose an item.
			// HACKY: Store loot list indices as negatives, starting from -1, because 0 is a valid item index
			int iLLIndex = (iItemDef * -1) - 1;
			const CEconLootListDefinition *pNestedLootList = GetItemSchema()->GetLootListByIndex( iLLIndex );
			if ( !pNestedLootList )
				return;

			pNestedLootList->GetRarityLootLists( out_pVecRarityLootList );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: get all item defs from this lootlist ( not lootlist item def )
//-----------------------------------------------------------------------------
void CEconLootListDefinition::GetItemDefs( CUtlVector< item_definition_index_t > *out_pVecItemDefs ) const
{
	Assert( out_pVecItemDefs );

	FOR_EACH_VEC( m_DropList, i )
	{
		const int iItemDef = m_DropList[i].m_iItemOrLootlistDef;	// not item_definition_index because it might also be a negative value to indicate a sub lootlist
		if ( iItemDef >= 0 )
		{
			out_pVecItemDefs->AddToTail( (item_definition_index_t)iItemDef );
		}
		else
		{
			// In the case where iItemDef is negative, it's a nested loot list. Ask that list to choose an item.
			// HACKY: Store loot list indices as negatives, starting from -1, because 0 is a valid item index
			int iLLIndex = (iItemDef * -1) - 1;
			const CEconLootListDefinition *pNestedLootList = GetItemSchema()->GetLootListByIndex( iLLIndex );
			if ( !pNestedLootList )
				return;

			pNestedLootList->GetItemDefs( out_pVecItemDefs );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Given a vector of possible attributes, roll to see which ones are
//			chosen.  We allocate memory for these new attributes, so it's the
//			responsibility of the caller to free these attributes when they're
//			done with them!
//-----------------------------------------------------------------------------
void CEconLootListDefinition::RollRandomAttributes( CUtlVector< static_attrib_t >& vecAttributes, const CEconGameAccount *pGameAccount ) const
{
	for ( int i=0; i<m_RandomAttribs.Count(); ++i )
	{
		const random_attrib_t* rattr = m_RandomAttribs[i];
		rattr->RollRandomAttributes( vecAttributes, pGameAccount );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconLootListDefinition::drop_period_t::IsValidForTime( const RTime32& time ) const
{
	if( time >= m_DropStartDate && time < m_DropEndDate )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconLootListDefinition::BAttachLootListAttributes( const CEconGameAccount *pGameAccount, CEconItem *pItem ) const
{
	//static CSchemaAttributeDefHandle pAttr_ElevateQuality( "elevate quality" );

	// Gather and apply old-style random attributes.
	CUtlVector< static_attrib_t > vecAttributes;
	RollRandomAttributes( vecAttributes, pGameAccount );

	FOR_EACH_VEC( vecAttributes, i )
	{
		GEconManager()->GetItemFactory().ApplyStaticAttributeToItem( pItem, vecAttributes[i], pGameAccount );
		vecAttributes[i].GetAttributeDefinition()->GetAttributeType()->UnloadEconAttributeValue( &vecAttributes[i].m_value );
	}

	// Apply all relevant property generators.
	for ( auto pGenerator : m_PropertyGenerators )
	{
		if ( !pGenerator->BGenerateProperties( pItem ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool lootlist_attrib_t::BInitFromKV( const char *pszContext, KeyValues *pKVKey, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors )
{
	SCHEMA_INIT_SUBSTEP( m_staticAttrib.BInitFromKV_MultiLine( pszContext, pKVKey, pVecErrors ) );

	SCHEMA_INIT_CHECK(
		pKVKey->FindKey( "weight" ),
		"Context '%s': Attribute \"%s\" missing required 'weight' field", pszContext, pKVKey->GetName() );

	m_flWeight = pKVKey->GetFloat( "weight" );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we should stop rolling from this random_attrib_t
//-----------------------------------------------------------------------------
bool random_attrib_t::RollRandomAttributes( CUtlVector< static_attrib_t >& vecAttributes, const CEconGameAccount *pGameAccount ) const
{
	if ( m_flChanceOfRandomAttribute && RandomFloat() <= m_flChanceOfRandomAttribute )
	{
		// We're attaching a random attribute. Determine which attribute.
		float flRand = 0.0f;
		if ( !m_bPickAllAttributes )
		{
			// Pick one attribute to add
			// Otherwise we'll pick them all
			flRand = RandomFloat( 0.f, 1.f ) * m_flTotalAttributeWeight;
		}
			
		float flAccum = 0.f;
		for ( int iAttrib = 0; iAttrib < m_RandomAttributes.Count(); ++iAttrib )
		{
			const lootlist_attrib_t& randomAttrib = m_RandomAttributes[iAttrib];

			flAccum += randomAttrib.m_flWeight;
			if ( flRand <= flAccum )
			{
				// Add the attribute
				static_attrib_t &staticAttrib = vecAttributes[ vecAttributes.AddToTail( randomAttrib.m_staticAttrib ) ];
				const CEconItemAttributeDefinition *pAttrDef = staticAttrib.GetAttributeDefinition();
				const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
				// Generate a special value?
				pAttrType->InitializeNewEconAttributeValue( &staticAttrib.m_value );
				pAttrDef->GetAttributeType()->GenerateEconAttributeValue( pAttrDef, staticAttrib, pGameAccount, &staticAttrib.m_value );
					
				if ( !m_bPickAllAttributes )
				{
					// We're only picking one attribute from the list
					return true;
				}
			}
		}
	}

	return false;
}

#endif // GC_DLL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEconLootListDefinition::EnumerateUserFacingPotentialDrops( IEconLootListIterator *pIt ) const
{
	Assert( pIt );

	// Loot lists have the option of specifying that their contents should not be publicly
	// listed. This is used on the GC for things like the "rare item drop list" to prevent
	// every single potentially-unusual hat from showing up.
	if ( !BPublicListContents() )
		return;

	FOR_EACH_VEC( GetLootListContents(), i )
	{
		const int iID = GetLootListContents()[i].m_iItemOrLootlistDef;

		// Nested loot lists are stored as negative indices.
		if ( iID < 0 )
		{
			const CEconLootListDefinition *pLootListDef = GetItemSchema()->GetLootListByIndex( (-iID) - 1 );
			if ( !pLootListDef )
				continue;

			pLootListDef->EnumerateUserFacingPotentialDrops( pIt );
		}
		else
		{
			pIt->OnIterate( iID );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
/*static*/ CSchemaAttributeDefHandle CAttributeLineItemLootList::s_pAttrDef_RandomDropLineItems[] =
{
	CSchemaAttributeDefHandle( "random drop line item 0" ),
	CSchemaAttributeDefHandle( "random drop line item 1" ),
	CSchemaAttributeDefHandle( "random drop line item 2" ),
	CSchemaAttributeDefHandle( "random drop line item 3" ),
};

#ifdef GC_DLL
/*static*/ CSchemaAttributeDefHandle CAttributeLineItemLootList::s_pAttrDef_RandomDropLineItemUnusualChance( "random drop line item unusual chance" );		// "one out of this many"
/*static*/ CSchemaAttributeDefHandle CAttributeLineItemLootList::s_pAttrDef_RandomDropLineItemUnusualList( "random drop line item unusual list" );	
#endif // GC_DLL
CSchemaAttributeDefHandle CAttributeLineItemLootList::s_pAttrDef_RandomDropLineItemFooterDesc( "random drop line item footer desc" );	

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAttributeLineItemLootList::EnumerateUserFacingPotentialDrops( IEconLootListIterator *pIt ) const
{
	Assert( pIt );
	
	for ( int i = 0; i < ARRAYSIZE( s_pAttrDef_RandomDropLineItems ); i++ )
	{
		uint32 unItemDef;
		COMPILE_TIME_ASSERT( sizeof( unItemDef ) >= sizeof( item_definition_index_t ) );

		// If we run out of attributes we have set we're done.
		if ( !m_pEconItem->FindAttribute( s_pAttrDef_RandomDropLineItems[i], &unItemDef ) )
			break;

		pIt->OnIterate( unItemDef );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CAttributeLineItemLootList::GetLootListHeaderLocalizationKey() const
{
	return g_pszDefaultRevolvingLootListHeader;
}

//-----------------------------------------------------------------------------
const char *CAttributeLineItemLootList::GetLootListFooterLocalizationKey() const
{
	CAttribute_String sFooter;
	const char* pszFooter = NULL;
	if ( FindAttribute_UnsafeBitwiseCast<CAttribute_String>( m_pEconItem, s_pAttrDef_RandomDropLineItemFooterDesc, &pszFooter ) )
	{
		return pszFooter;
	}
	return NULL;
}
//-----------------------------------------------------------------------------
const char *CAttributeLineItemLootList::GetLootListCollectionReference() const
{
	// TODO : Implement me!
	return NULL;
}
//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
const CEconLootListDefinition* CEconItemSchema::GetLootListByName( const char* pListName, int *out_piIndex ) const
{
	auto idx = m_mapLootLists.Find( pListName );
	if ( !m_mapLootLists.IsValidIndex( idx ) )
		return NULL;

	if ( out_piIndex )
	{
		*out_piIndex = idx;
	}

	return m_mapLootLists[idx];
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
const CQuestObjectiveDefinition* CEconItemSchema::GetQuestObjectiveByDefIndex( int iIdx ) const
{
	auto nMapIndex = m_mapQuestObjectives.Find( iIdx );
	if ( nMapIndex != m_mapQuestObjectives.InvalidIndex() )
	{
		return m_mapQuestObjectives[ nMapIndex ];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEconCraftingRecipeDefinition::CEconCraftingRecipeDefinition( void )
	: m_nDefIndex( 0 )
#ifdef GC_DLL
	, m_bIsCraftableByUnverifiedClient( false )
#endif // GC_DLL
{
}

//-----------------------------------------------------------------------------
// Purpose:	Initialize the attribute definition
// Input:	pKVAttribute - The KeyValues representation of the attribute
//			schema - The overall item schema for this attribute
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconCraftingRecipeDefinition::BInitFromKV( KeyValues *pKVRecipe, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	m_nDefIndex = Q_atoi( pKVRecipe->GetName() );
	
	// Check for required fields
	SCHEMA_INIT_CHECK( 
		NULL != pKVRecipe->FindKey( "input_items" ), 
		"Recipe definition %d: Missing required field \"input_items\"", m_nDefIndex );

	SCHEMA_INIT_CHECK( 
		NULL != pKVRecipe->FindKey( "output_items" ), 
		"Recipe definition %d: Missing required field \"output_items\"", m_nDefIndex );

	m_bDisabled = pKVRecipe->GetBool( "disabled" );
	m_strName = pKVRecipe->GetString( "name" );	
	m_strN_A = pKVRecipe->GetString( "n_A" );	 
	m_strDescInputs = pKVRecipe->GetString( "desc_inputs" );	
	m_strDescOutputs = pKVRecipe->GetString( "desc_outputs" );	 
	m_strDI_A = pKVRecipe->GetString( "di_A" );	 
	m_strDI_B = pKVRecipe->GetString( "di_B" );	 
	m_strDI_C = pKVRecipe->GetString( "di_C" );	 
	m_strDO_A = pKVRecipe->GetString( "do_A" );	 
	m_strDO_B = pKVRecipe->GetString( "do_B" );	 
	m_strDO_C = pKVRecipe->GetString( "do_C" );	 

#ifdef GC_DLL
	m_bIsCraftableByUnverifiedClient = pKVRecipe->GetBool( "is_craftable_by_unverified_clients", false );
#endif // GC_DLL
	m_bRequiresAllSameClass = pKVRecipe->GetBool( "all_same_class" );
	m_bRequiresAllSameSlot = pKVRecipe->GetBool( "all_same_slot" );
	m_iCacheClassUsageForOutputFromItem = pKVRecipe->GetInt( "add_class_usage_to_output", -1 );
	m_iCacheSlotUsageForOutputFromItem = pKVRecipe->GetInt( "add_slot_usage_to_output", -1 );
	m_iCacheSetForOutputFromItem = pKVRecipe->GetInt( "add_set_to_output", -1 );
	m_bPremiumAccountOnly = pKVRecipe->GetBool( "premium_only", false );
	m_iCategory = (recipecategories_t)StringFieldToInt( pKVRecipe->GetString("category"), g_szRecipeCategoryStrings, ARRAYSIZE(g_szRecipeCategoryStrings) );

	// Read in all the input items
	KeyValues *pKVInputItems = pKVRecipe->FindKey( "input_items" );
	if ( NULL != pKVInputItems )
	{
		FOR_EACH_TRUE_SUBKEY( pKVInputItems, pKVInputItem )
		{
			int index = m_InputItemsCriteria.AddToTail();
			SCHEMA_INIT_SUBSTEP( m_InputItemsCriteria[index].BInitFromKV( pKVInputItem ) );

			// Recipes ignore the enabled flag when generating items
			m_InputItemsCriteria[index].SetIgnoreEnabledFlag( true );

			index = m_InputItemDupeCounts.AddToTail();
			m_InputItemDupeCounts[index] = atoi( pKVInputItem->GetName() );
		}
	}

	// Read in all the output items
	KeyValues *pKVOutputItems = pKVRecipe->FindKey( "output_items" );
	if ( NULL != pKVOutputItems )
	{
		FOR_EACH_TRUE_SUBKEY( pKVOutputItems, pKVOutputItem )
		{
			int index = m_OutputItemsCriteria.AddToTail();
			SCHEMA_INIT_SUBSTEP( m_OutputItemsCriteria[index].BInitFromKV( pKVOutputItem ) );

			// Recipes ignore the enabled flag when generating items
			m_OutputItemsCriteria[index].SetIgnoreEnabledFlag( true );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Purpose: Serializes the criteria to and from messages
//-----------------------------------------------------------------------------
bool CEconCraftingRecipeDefinition::BSerializeToMsg( CSOItemRecipe & msg ) const
{
	msg.set_def_index( m_nDefIndex );
	msg.set_name( m_strName );
	msg.set_n_a( m_strN_A );
	msg.set_desc_inputs( m_strDescInputs );
	msg.set_desc_outputs( m_strDescOutputs );
	msg.set_di_a( m_strDI_A );
	msg.set_di_b( m_strDI_B );
	msg.set_di_c( m_strDI_C );
	msg.set_do_a( m_strDO_A );
	msg.set_do_b( m_strDO_B );
	msg.set_do_c( m_strDO_C );
	msg.set_requires_all_same_class( m_bRequiresAllSameClass );
	msg.set_requires_all_same_slot( m_bRequiresAllSameSlot );
	msg.set_class_usage_for_output( m_iCacheClassUsageForOutputFromItem );
	msg.set_slot_usage_for_output( m_iCacheSlotUsageForOutputFromItem );
	msg.set_set_for_output( m_iCacheSetForOutputFromItem );

	FOR_EACH_VEC( m_InputItemsCriteria, i )
	{
		CSOItemCriteria *pCrit = msg.add_input_items_criteria();
		if ( !m_InputItemsCriteria[i].BSerializeToMsg( *pCrit ) )
			return false;
	}

	FOR_EACH_VEC( m_InputItemDupeCounts, i )
	{
		msg.add_input_item_dupe_counts( m_InputItemDupeCounts[i] );
	}

	FOR_EACH_VEC( m_OutputItemsCriteria, i )
	{
		CSOItemCriteria *pCrit = msg.add_output_items_criteria();
		if ( !m_OutputItemsCriteria[i].BSerializeToMsg( *pCrit ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Serializes the criteria to and from messages
//-----------------------------------------------------------------------------
bool CEconCraftingRecipeDefinition::BDeserializeFromMsg( const CSOItemRecipe & msg )
{
	m_nDefIndex = msg.def_index();
	m_strName = msg.name().c_str();
	m_strN_A = msg.n_a().c_str();
	m_strDescInputs = msg.desc_inputs().c_str();
	m_strDescOutputs = msg.desc_outputs().c_str();
	m_strDI_A = msg.di_a().c_str();
	m_strDI_B = msg.di_b().c_str();
	m_strDI_C = msg.di_c().c_str();
	m_strDO_A = msg.do_a().c_str();
	m_strDO_B = msg.do_b().c_str();
	m_strDO_C = msg.do_c().c_str();

	m_bRequiresAllSameClass = msg.requires_all_same_class();
	m_bRequiresAllSameSlot = msg.requires_all_same_slot();
	m_iCacheClassUsageForOutputFromItem = msg.class_usage_for_output();
	m_iCacheSlotUsageForOutputFromItem = msg.slot_usage_for_output();
	m_iCacheSetForOutputFromItem = msg.set_for_output();

	// Read how many input items there are
	uint32 unCount = msg.input_items_criteria_size();
	m_InputItemsCriteria.SetSize( unCount );
	for ( uint32 i = 0; i < unCount; i++ )
	{
		if ( !m_InputItemsCriteria[i].BDeserializeFromMsg( msg.input_items_criteria( i ) ) )
			return false;
	}

	// Read how many input item dupe counts there are
	unCount = msg.input_item_dupe_counts_size();
	m_InputItemDupeCounts.SetSize( unCount );
	for ( uint32 i = 0; i < unCount; i++ )
	{
		m_InputItemDupeCounts[i] = msg.input_item_dupe_counts( i );
	}

	// Read how many output items there are
	unCount = msg.output_items_criteria_size();
	m_OutputItemsCriteria.SetSize( unCount );
	for ( uint32 i = 0; i < unCount; i++ )
	{
		if ( !m_OutputItemsCriteria[i].BDeserializeFromMsg( msg.output_items_criteria( i ) ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the vector contains a set of items that matches the inputs for this recipe
//			Note it will fail if the vector contains extra items that aren't needed.
//
//-----------------------------------------------------------------------------
bool CEconCraftingRecipeDefinition::ItemListMatchesInputs( CUtlVector<CEconItem*> *vecCraftingItems, KeyValues *out_pkvCraftParams, bool bIgnoreSlop, CUtlVector<uint64> *vecChosenItems ) const
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
int CEconCraftingRecipeDefinition::GetTotalInputItemsRequired( void ) const
{
	int iCount = 0;
	FOR_EACH_VEC( m_InputItemsCriteria, i )
	{
		if ( m_InputItemDupeCounts[i] )
		{
			iCount += m_InputItemDupeCounts[i];
		}
		else
		{
			iCount++;
		}
	}
	return iCount;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#ifdef GC_DLL
	#define GC_SCH_REFERENCE( TAttribSchType ) \
		TAttribSchType, 
#else
	#define GC_SCH_REFERENCE( TAttribSchType )
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
unsigned int Internal_GetAttributeTypeUniqueIdentifierNextValue()
{
	static unsigned int s_unUniqueCounter = 0;

	unsigned int unCounter = s_unUniqueCounter;
	s_unUniqueCounter++;
	return unCounter;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#ifdef GC_DLL
template < typename TAttribSchType, typename TRecordBaseType >
static TAttribSchType *GetTypedSch( TRecordBaseType *pRecordBase )
{
	Assert( pRecordBase->GetITable() == TAttribSchType::k_iTable );

#if ENABLE_TYPED_ATTRIBUTE_PARANOIA
	TAttribSchType *pTypedSch = dynamic_cast<TAttribSchType *>( pRecordBase );
	Assert( pTypedSch );
	return pTypedSch;
#else
	return static_cast<TAttribSchType *>( pRecordBase );
#endif
}
#endif // GC_DLL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template < GC_SCH_REFERENCE( typename TAttribSchType ) typename TAttribInMemoryType >
class CSchemaAttributeTypeBase : public ISchemaAttributeTypeBase<TAttribInMemoryType>
{
public:
#ifdef GC_DLL
	virtual CColumnSet& GetFullColumnSet() const OVERRIDE
	{
		static CColumnSet sFullColumnSet( CColumnSet::Full<TAttribSchType>() );

		return sFullColumnSet;
	}

	virtual CRecordBase *CreateTypedSchRecord() const OVERRIDE
	{
		return new TAttribSchType;
	}
#endif // GC_DLL
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template < GC_SCH_REFERENCE( typename TAttribSchType ) typename TProtobufValueType >
class CSchemaAttributeTypeProtobufBase : public CSchemaAttributeTypeBase< GC_SCH_REFERENCE( TAttribSchType ) TProtobufValueType >
{
public:
	virtual void ConvertTypedValueToByteStream( const TProtobufValueType& typedValue, ::std::string *out_psBytes ) const OVERRIDE
	{
		DbgVerify( typedValue.SerializeToString( out_psBytes ) );
	}

	virtual void ConvertByteStreamToTypedValue( const ::std::string& sBytes, TProtobufValueType *out_pTypedValue ) const OVERRIDE
	{
		DbgVerify( out_pTypedValue->ParseFromString( sBytes ) );
	}

	virtual bool BConvertStringToEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const char *pszValue, union attribute_data_union_t *out_pValue, bool bEnableTerribleBackwardsCompatibilitySchemaParsingCode ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_pValue );
		
		std::string sValue( pszValue );
		TProtobufValueType typedValue;
		if ( !google::protobuf::TextFormat::ParseFromString( sValue, &typedValue ) )
			return false;

		this->ConvertTypedValueToEconAttributeValue( typedValue, out_pValue );
		return true;
	}

	virtual void ConvertEconAttributeValueToString( const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value, std::string *out_ps ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_ps );

		google::protobuf::TextFormat::PrintToString( this->GetTypedValueContentsFromEconAttributeValue( value ), out_ps );
	}
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSchemaAttributeType_String : public CSchemaAttributeTypeProtobufBase< GC_SCH_REFERENCE( CSchItemAttributeString ) CAttribute_String >
{
public:
#ifdef GC_DLL
	virtual void ConvertEconAttributeValueToSch( itemid_t unItemId, const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value, GCSDK::CRecordBase *out_pSchRecord ) const OVERRIDE
	{
		Assert( out_pSchRecord );
		Assert( pAttrDef );
		Assert( pAttrDef->GetAttributeType() == this );

		CSchItemAttributeString *out_psch = GetTypedSch<CSchItemAttributeString>( out_pSchRecord );

		CAttribute_String typedValue;
		this->ConvertEconAttributeValueToTypedValue( value, &typedValue );

		// const CAttribute_String& typedValue = GetTypedValueContentsFromEconAttributeValue( value );

		out_psch->m_ulItemID			= unItemId;
		out_psch->m_unAttrDefIndex		= pAttrDef->GetDefinitionIndex();
		WRITE_VAR_CHAR_FIELD( (*out_psch), VarCharAttrStrValue, typedValue.value().c_str() );
	}

	virtual void LoadSchToEconAttributeValue( CEconItem *pTargetItem, const CEconItemAttributeDefinition *pAttrDef, const GCSDK::CRecordBase *pSchRecord ) const OVERRIDE
	{
		Assert( pTargetItem );
		Assert( pAttrDef );
		Assert( pSchRecord );
		Assert( pAttrDef->GetAttributeType() == this );

		const CSchItemAttributeString *psch = GetTypedSch<const CSchItemAttributeString>( pSchRecord );

		CAttribute_String typedValue;
		typedValue.set_value( READ_VAR_CHAR_FIELD( (*psch), m_VarCharAttrStrValue ) );

		pTargetItem->SetDynamicAttributeValue( pAttrDef, typedValue );
	}
#endif // GC_DLL

	// We intentionally override the convert-to-/convert-from-string functions for strings so that string literals can be
	// specified in the schema, etc. without worrying about the protobuf text format.
	virtual bool BConvertStringToEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const char *pszValue, union attribute_data_union_t *out_pValue, bool bEnableTerribleBackwardsCompatibilitySchemaParsingCode ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_pValue );

		CAttribute_String typedValue;
		typedValue.set_value( pszValue );

		this->ConvertTypedValueToEconAttributeValue( typedValue, out_pValue );

		return true;
	}

	virtual void ConvertEconAttributeValueToString( const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value, std::string *out_ps ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_ps );

		*out_ps = this->GetTypedValueContentsFromEconAttributeValue( value ).value().c_str();
	}
};

void CopyStringAttributeValueToCharPointerOutput( const CAttribute_String *pValue, const char **out_pValue )
{
	Assert( pValue );
	Assert( out_pValue );

	*out_pValue = pValue->value().c_str();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSchemaAttributeType_DynamicRecipeComponentDefinedItem : public CSchemaAttributeTypeProtobufBase< GC_SCH_REFERENCE( CSchItemAttributeDynamicRecipeComponentDefinedItem ) CAttribute_DynamicRecipeComponent >
{
public:
#ifdef GC_DLL
	virtual void ConvertEconAttributeValueToSch( itemid_t unItemId, const CEconItemAttributeDefinition *pAttrDef, const union attribute_data_union_t& value, GCSDK::CRecordBase *out_pSchRecord ) const OVERRIDE
	{
		Assert( out_pSchRecord );
		Assert( pAttrDef );
		Assert( pAttrDef->GetAttributeType() == this );

		CSchItemAttributeDynamicRecipeComponentDefinedItem *out_psch = GetTypedSch<CSchItemAttributeDynamicRecipeComponentDefinedItem>( out_pSchRecord );

		CAttribute_DynamicRecipeComponent typedValue;
		ConvertEconAttributeValueToTypedValue( value, &typedValue );

		out_psch->m_ulItemID			= unItemId;
		out_psch->m_unAttrDefIndex		= pAttrDef->GetDefinitionIndex();
		out_psch->m_unItemDef			= typedValue.def_index();
		out_psch->m_unItemQuality		= typedValue.item_quality();
		out_psch->m_unFlags				= typedValue.component_flags();
		out_psch->m_unItemCount			= typedValue.num_required();
		out_psch->m_unItemsFulfilled	= typedValue.num_fulfilled();
		WRITE_VAR_CHAR_FIELD( (*out_psch), VarCharAttrStr, typedValue.attributes_string().c_str() );
	}

	virtual void LoadSchToEconAttributeValue( CEconItem *pTargetItem, const CEconItemAttributeDefinition *pAttrDef, const GCSDK::CRecordBase *pSchRecord ) const OVERRIDE
	{
		Assert( pTargetItem );
		Assert( pAttrDef );
		Assert( pSchRecord );
		Assert( pAttrDef->GetAttributeType() == this );

		const CSchItemAttributeDynamicRecipeComponentDefinedItem *psch = GetTypedSch<const CSchItemAttributeDynamicRecipeComponentDefinedItem>( pSchRecord );

		CAttribute_DynamicRecipeComponent typedValue;
		typedValue.set_def_index( psch->m_unItemDef );
		typedValue.set_item_quality( psch->m_unItemQuality );
		typedValue.set_component_flags( psch->m_unFlags );
		typedValue.set_attributes_string( READ_VAR_CHAR_FIELD( (*psch), m_VarCharAttrStr ) );
		typedValue.set_num_required( psch->m_unItemCount );
		typedValue.set_num_fulfilled( psch->m_unItemsFulfilled );

		pTargetItem->SetDynamicAttributeValue( pAttrDef, typedValue );
	}

	virtual bool BConvertStringToEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const char *pszValue, union attribute_data_union_t *out_pValue, bool bEnableTerribleBackwardsCompatibilitySchemaParsingCode ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_pValue );
		
		std::string sValue( pszValue );
		// What's happened here is we've renamed some fields within CAttribute_DynamicRecipeComponent,
		// but steam contains the strings of the old format serialized, and keeps sending them to us.
		// Rather than updating steam, we're going to made a protobuff class that can accept the new
		// and old formats, and put the corret values into the correct members of the new format.

		
		CAttribute_DynamicRecipeComponent_COMPAT_NEVER_SERIALIZE_THIS_OUT  typedCompatValue;
		CAttribute_DynamicRecipeComponent typedActualValue;

#ifdef STAGING_ONLY
		auto *pActualFields = typedActualValue.descriptor();
		auto *pCompatFields = typedCompatValue.descriptor();
		for ( int i=0; i < pActualFields->field_count(); ++i )
		{
			const bool bFoundField = pCompatFields->FindFieldByName( pActualFields->field( i )->name() ) != NULL;
			Assert( bFoundField );
			if ( !bFoundField )
			{
				EmitError( SPEW_GC, "Missing field '%s' in CAttribute_DynamicRecipeComponent_COMPAT_NEVER_SERIALIZE_THIS_OUT\n", pActualFields->field( i )->name() );
				return false;
			}
		}
#endif // STAGING_ONLY

		if ( !google::protobuf::TextFormat::ParseFromString( sValue, &typedCompatValue ) )
		{
			EmitError( SPEW_GC, "Failed to parse recipe component into compatible protobuf\n" );
			return false;
		}

		if ( typedCompatValue.has_component_flags() )
			typedActualValue.set_component_flags( typedCompatValue.component_flags() );
		else if ( typedCompatValue.has_item_flags() )
			typedActualValue.set_component_flags( typedCompatValue.item_flags() );
		else 
		{
			EmitError( SPEW_GC, "Failed to parse component_flags.  component_flags: %d, item_flags: %d\n", typedCompatValue.component_flags(), typedCompatValue.item_flags() );
			return false;
		}

		if ( typedCompatValue.has_def_index() )
			typedActualValue.set_def_index( typedCompatValue.def_index() );
		else if ( typedCompatValue.has_item_def() )
			typedActualValue.set_def_index( typedCompatValue.item_def() );
		else if ( typedActualValue.component_flags() & DYNAMIC_RECIPE_FLAG_PARAM_ITEM_DEF_SET )
		{
			EmitError( SPEW_GC, "Failed to parse item_def.  def_index: %d, item_def: %d\n", typedCompatValue.def_index(), typedCompatValue.item_def() );
			return false;
		}

		typedActualValue.set_item_quality( typedCompatValue.item_quality() );

		
		
		typedActualValue.set_attributes_string( typedCompatValue.attributes_string() );

		if( typedCompatValue.has_num_required() )
			typedActualValue.set_num_required( typedCompatValue.num_required() );
		else if ( typedCompatValue.has_item_count() )
			typedActualValue.set_num_required( typedCompatValue.item_count() );
		else
		{
			EmitError( SPEW_GC, "Failed to parse component_flags.  num_required: %d, item_count: %d\n", typedCompatValue.num_required(), typedCompatValue.item_count() );
			return false;
		}

		if ( typedCompatValue.has_items_fulfilled() )
			typedActualValue.set_num_fulfilled( typedCompatValue.items_fulfilled() );
		else if ( typedCompatValue.has_num_fulfilled() )
			typedActualValue.set_num_fulfilled( typedCompatValue.num_fulfilled() );
		else
		{
			EmitError( SPEW_GC, "Failed to parse num_fulfilled.  items_fulfilled: %d, num_fulfilled: %d\n", typedCompatValue.items_fulfilled(), typedCompatValue.num_fulfilled() );
			return false;
		}

		this->ConvertTypedValueToEconAttributeValue( typedActualValue, out_pValue );
		return true;
	}

#endif // GC_DLL
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSchemaAttributeType_ItemSlotCriteria : public CSchemaAttributeTypeProtobufBase< GC_SCH_REFERENCE( CSchItemAttributeItemSlotCriteria ) CAttribute_ItemSlotCriteria >
{
public:
#ifdef GC_DLL
	virtual void ConvertEconAttributeValueToSch( itemid_t unItemId, const CEconItemAttributeDefinition *pAttrDef, const union attribute_data_union_t& value, GCSDK::CRecordBase *out_pSchRecord ) const OVERRIDE
	{
		Assert( out_pSchRecord );
		Assert( pAttrDef );
		Assert( pAttrDef->GetAttributeType() == this );

		AssertMsg( 0, "Implement this when we want this attribute to be dynamic" );
	}

	virtual void LoadSchToEconAttributeValue( CEconItem *pTargetItem, const CEconItemAttributeDefinition *pAttrDef, const GCSDK::CRecordBase *pSchRecord ) const OVERRIDE
	{
		Assert( pTargetItem );
		Assert( pAttrDef );
		Assert( pSchRecord );
		Assert( pAttrDef->GetAttributeType() == this );
		
		AssertMsg( 0, "Implement this when we want this attribute to be dynamic" );
	}
#endif // GC_DLL

	virtual bool BConvertStringToEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const char *pszValue, union attribute_data_union_t *out_pValue, bool bEnableTerribleBackwardsCompatibilitySchemaParsingCode ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_pValue );

		std::string sValue( pszValue );
		CAttribute_ItemSlotCriteria typedValue;
		if ( !google::protobuf::TextFormat::ParseFromString( sValue, &typedValue ) )
			return false;

		this->ConvertTypedValueToEconAttributeValue( typedValue, out_pValue );

		return true;
	}

	virtual void ConvertEconAttributeValueToString( const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value, std::string *out_ps ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_ps );

		this->ConvertEconAttributeValueToString( pAttrDef, value, out_ps );
	}
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSchemaAttributeType_WorldItemPlacement : public CSchemaAttributeTypeProtobufBase < GC_SCH_REFERENCE( CSchItemAttributeWorldItemPlacement ) CAttribute_WorldItemPlacement >
{
public:
#ifdef GC_DLL
	virtual void ConvertEconAttributeValueToSch( itemid_t unItemId, const CEconItemAttributeDefinition *pAttrDef, const union attribute_data_union_t& value, GCSDK::CRecordBase *out_pSchRecord ) const OVERRIDE
	{
		Assert( out_pSchRecord );
		Assert( pAttrDef );
		Assert( pAttrDef->GetAttributeType() == this );

		CSchItemAttributeWorldItemPlacement *out_psch = GetTypedSch< CSchItemAttributeWorldItemPlacement >( out_pSchRecord );

		CAttribute_WorldItemPlacement typedValue;
		ConvertEconAttributeValueToTypedValue( value, &typedValue );

		out_psch->m_ulItemID = unItemId;
		out_psch->m_unAttrDefIndex = pAttrDef->GetDefinitionIndex();
		out_psch->m_ulOriginalItemID = typedValue.original_item_id();
		out_psch->m_fPosX = typedValue.pos_x();
		out_psch->m_fPosY = typedValue.pos_y();
		out_psch->m_fPosZ = typedValue.pos_z();
		out_psch->m_fAngX = typedValue.ang_x();
		out_psch->m_fAngY = typedValue.ang_y();
		out_psch->m_fAngZ = typedValue.ang_z();
	}

	virtual void LoadSchToEconAttributeValue( CEconItem *pTargetItem, const CEconItemAttributeDefinition *pAttrDef, const GCSDK::CRecordBase *pSchRecord ) const OVERRIDE
	{
		Assert( pTargetItem );
		Assert( pAttrDef );
		Assert( pSchRecord );
		Assert( pAttrDef->GetAttributeType() == this );

		const CSchItemAttributeWorldItemPlacement *psch = GetTypedSch< const CSchItemAttributeWorldItemPlacement >( pSchRecord );

		CAttribute_WorldItemPlacement typedValue;
		typedValue.set_original_item_id( psch->m_ulOriginalItemID );
		typedValue.set_pos_x( psch->m_fPosX );
		typedValue.set_pos_y( psch->m_fPosY );
		typedValue.set_pos_x( psch->m_fPosZ );
		typedValue.set_ang_x( psch->m_fAngX );
		typedValue.set_ang_y( psch->m_fAngY );
		typedValue.set_ang_z( psch->m_fAngZ );

		pTargetItem->SetDynamicAttributeValue( pAttrDef, typedValue );
	}
#endif // GC_DLL

	virtual bool BConvertStringToEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const char *pszValue, union attribute_data_union_t *out_pValue, bool bEnableTerribleBackwardsCompatibilitySchemaParsingCode ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_pValue );

		CAttribute_WorldItemPlacement typedValue;

		uint32 unValue = ( pszValue ) ? atoi( pszValue ) : 0;
		
		// Item forcing us to create the attribute (via force_gc_to_generate)
		if ( unValue == 0 )
		{
			typedValue.set_original_item_id( INVALID_ITEM_ID );
			typedValue.set_pos_x( 0.f );
			typedValue.set_pos_y( 0.f );
			typedValue.set_pos_z( 0.f );
			typedValue.set_ang_x( 0.f );
			typedValue.set_ang_y( 0.f );
			typedValue.set_ang_z( 0.f );
		}
		else
		{
			std::string sValue( pszValue );
			if ( !google::protobuf::TextFormat::ParseFromString( sValue, &typedValue ) )
				return false;
		}
		
		this->ConvertTypedValueToEconAttributeValue( typedValue, out_pValue );
		return true;
	}

	virtual void ConvertEconAttributeValueToString( const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value, std::string *out_ps ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_ps );

		this->ConvertEconAttributeValueToString( pAttrDef, value, out_ps );
	}
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSchemaAttributeType_Float : public CSchemaAttributeTypeBase< GC_SCH_REFERENCE( CSchItemAttributeFloat ) float >
{
public:
#ifdef GC_DLL
	virtual void ConvertEconAttributeValueToSch( itemid_t unItemId, const CEconItemAttributeDefinition *pAttrDef, const union attribute_data_union_t& value, GCSDK::CRecordBase *out_pSchRecord ) const OVERRIDE
	{
		Assert( out_pSchRecord );
		Assert( pAttrDef );
		Assert( pAttrDef->GetAttributeType() == this );

		CSchItemAttributeFloat *out_pschItemAttribute = GetTypedSch<CSchItemAttributeFloat>( out_pSchRecord );

		// @note Tom Bui: we store the value as an unsigned integer in the DB, so just treat the field as a bunch of bits
		out_pschItemAttribute->m_ulItemID		= unItemId;
		out_pschItemAttribute->m_unAttrDefIndex = pAttrDef->GetDefinitionIndex();
		out_pschItemAttribute->m_fValue			= value.asFloat;
	}

	virtual void LoadSchToEconAttributeValue( CEconItem *pTargetItem, const CEconItemAttributeDefinition *pAttrDef, const GCSDK::CRecordBase *pSchRecord ) const OVERRIDE
	{
		Assert( pTargetItem );
		Assert( pAttrDef );
		Assert( pSchRecord );
		Assert( pAttrDef->GetAttributeType() == this );

		const CSchItemAttributeFloat *pschItemAttribute = GetTypedSch<const CSchItemAttributeFloat>( pSchRecord );

		pTargetItem->SetDynamicAttributeValue( pAttrDef, pschItemAttribute->m_fValue );
	}
#endif // GC_DLL

	virtual bool BConvertStringToEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const char *pszValue, union attribute_data_union_t *out_pValue, bool bEnableTerribleBackwardsCompatibilitySchemaParsingCode ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_pValue );

		out_pValue->asFloat = Q_atof( pszValue );
		return true;
	}

	virtual void ConvertEconAttributeValueToString( const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value, std::string *out_ps ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_ps );

		*out_ps = CFmtStr( "%f", value.asFloat ).Get();
	}
	
	virtual void ConvertTypedValueToByteStream( const float& typedValue, ::std::string *out_psBytes ) const OVERRIDE
	{
		Assert( out_psBytes );
		Assert( out_psBytes->size() == 0 );

		out_psBytes->resize( sizeof( float ) );
		*reinterpret_cast<float *>( &((*out_psBytes)[0]) ) = typedValue;		// overwrite string contents (sizeof( float ) bytes)
	}

	virtual void ConvertByteStreamToTypedValue( const ::std::string& sBytes, float *out_pTypedValue ) const OVERRIDE
	{
		Assert( out_pTypedValue );
		Assert( sBytes.size() == sizeof( float ) );

		*out_pTypedValue = *reinterpret_cast<const float *>( &sBytes[0] );
	}

	virtual bool BSupportsGameplayModificationAndNetworking() const OVERRIDE
	{
		return true;
	}
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSchemaAttributeType_UInt64 : public CSchemaAttributeTypeBase< GC_SCH_REFERENCE( CSchItemAttributeUInt64 ) uint64 >
{
public:
#ifdef GC_DLL
	virtual void ConvertEconAttributeValueToSch( itemid_t unItemId, const CEconItemAttributeDefinition *pAttrDef, const union attribute_data_union_t& value, GCSDK::CRecordBase *out_pSchRecord ) const OVERRIDE
	{
		Assert( out_pSchRecord );
		Assert( pAttrDef );
		Assert( pAttrDef->GetAttributeType() == this );

		uint64 ulValue;
		ConvertEconAttributeValueToTypedValue( value, &ulValue );

		CSchItemAttributeUInt64 *out_pschItemAttribute = GetTypedSch<CSchItemAttributeUInt64>( out_pSchRecord );

		// @note Tom Bui: we store the value as an unsigned integer in the DB, so just treat the field as a bunch of bits
		out_pschItemAttribute->m_ulItemID		= unItemId;
		out_pschItemAttribute->m_unAttrDefIndex = pAttrDef->GetDefinitionIndex();
		out_pschItemAttribute->m_ulValue		= ulValue;
	}

	virtual void LoadSchToEconAttributeValue( CEconItem *pTargetItem, const CEconItemAttributeDefinition *pAttrDef, const GCSDK::CRecordBase *pSchRecord ) const OVERRIDE
	{
		Assert( pTargetItem );
		Assert( pAttrDef );
		Assert( pSchRecord );
		Assert( pAttrDef->GetAttributeType() == this );

		const CSchItemAttributeUInt64 *pschItemAttribute = GetTypedSch<const CSchItemAttributeUInt64>( pSchRecord );

		pTargetItem->SetDynamicAttributeValue( pAttrDef, pschItemAttribute->m_ulValue );
	}
#endif // GC_DLL

	virtual bool BConvertStringToEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const char *pszValue, union attribute_data_union_t *out_pValue, bool bEnableTerribleBackwardsCompatibilitySchemaParsingCode ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_pValue );

		out_pValue->asUint32 = V_atoui64( pszValue );
		return true;
	}

	virtual void ConvertEconAttributeValueToString( const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value, std::string *out_ps ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_ps );

		uint64 ulValue;
		ConvertEconAttributeValueToTypedValue( value, &ulValue );

		*out_ps = CFmtStr( "%llu", ulValue ).Get();
	}
	
	virtual void ConvertTypedValueToByteStream( const uint64& typedValue, ::std::string *out_psBytes ) const OVERRIDE
	{
		Assert( out_psBytes );
		Assert( out_psBytes->size() == 0 );

		out_psBytes->resize( sizeof( uint64 ) );
		*reinterpret_cast<uint64 *>( &((*out_psBytes)[0]) ) = typedValue;		// overwrite string contents (sizeof( uint64 ) bytes)
	}

	virtual void ConvertByteStreamToTypedValue( const ::std::string& sBytes, uint64 *out_pTypedValue ) const OVERRIDE
	{
		Assert( out_pTypedValue );
		Assert( sBytes.size() == sizeof( uint64 ) );

		*out_pTypedValue = *reinterpret_cast<const uint64 *>( &sBytes[0] );
	}
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSchemaAttributeType_Default : public CSchemaAttributeTypeBase< GC_SCH_REFERENCE( CSchItemAttribute ) attrib_value_t >
{
public:
#ifdef GC_DLL
	virtual bool BAssetClassExportedAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value ) const OVERRIDE
	{
		Assert( pAttrDef );

		static CSchemaAttributeDefHandle pAttrib_TradableAfter( "tradable after date" );

		// Don't include "tradable after date" if it's in the past
		// See IEconItemInterface::IsTradable for the specific logic on how this affects tradability
		if ( pAttrDef == pAttrib_TradableAfter && CRTime::RTime32TimeCur() > value.asUint32 )
			return false;

		return CSchemaAttributeTypeBase< GC_SCH_REFERENCE( CSchItemAttribute ) attrib_value_t >::BAssetClassExportedAttributeValue( pAttrDef, value );
	}

	virtual void ConvertEconAttributeValueToSch( itemid_t unItemId, const CEconItemAttributeDefinition *pAttrDef, const union attribute_data_union_t& value, GCSDK::CRecordBase *out_pSchRecord ) const OVERRIDE
	{
		Assert( out_pSchRecord );
		Assert( pAttrDef );
		Assert( pAttrDef->GetAttributeType() == this );

		CSchItemAttribute *out_pschItemAttribute = GetTypedSch<CSchItemAttribute>( out_pSchRecord );

		// @note Tom Bui: we store the value as an unsigned integer in the DB, so just treat the field as a bunch of bits
		out_pschItemAttribute->m_ulItemID		= unItemId;
		out_pschItemAttribute->m_unAttrDefIndex = pAttrDef->GetDefinitionIndex();
		out_pschItemAttribute->m_unValue		= value.asUint32;
	}

	virtual void LoadSchToEconAttributeValue( CEconItem *pTargetItem, const CEconItemAttributeDefinition *pAttrDef, const GCSDK::CRecordBase *pSchRecord ) const OVERRIDE
	{
		Assert( pTargetItem );
		Assert( pAttrDef );
		Assert( pSchRecord );
		Assert( pAttrDef->GetAttributeType() == this );

		const CSchItemAttribute *pschItemAttribute = GetTypedSch<const CSchItemAttribute>( pSchRecord );

		pTargetItem->SetDynamicAttributeValue( pAttrDef, pschItemAttribute->m_unValue );
	}

	virtual void LoadOrGenerateEconAttributeValue( CEconItem *pTargetItem, const CEconItemAttributeDefinition *pAttrDef, const static_attrib_t& staticAttrib, const CEconGameAccount *pGameAccount ) const OVERRIDE
	{
		Assert( pTargetItem );
		Assert( pTargetItem->GetItemDefinition() );
		Assert( pAttrDef );

		// Wear is reassigned by attributes but has a default value from itemdef prefab
		static CSchemaAttributeDefHandle pAttrDef_PaintkitWear( "set_item_texture_wear" );

		// do not apply an attribute if it already exists.  If the new and the old attribute value is different then we assert (and use the latest value)
		attrib_value_t unValue = 0;
		if ( pTargetItem->FindAttribute( pAttrDef, &unValue ) && pAttrDef != pAttrDef_PaintkitWear )
		{
			AssertMsg4( unValue == staticAttrib.m_value.asUint32,
				"Item id %llu (%s) attempting to generate dynamic attribute value for '%s' (%d) when attribute already exists with a different Value! This probably indicates some sort of code flow error calling LoadOrGenerateEconAttributeValue() late.",
				pTargetItem->GetItemID(), pTargetItem->GetItemDefinition()->GetDefinitionName(), pAttrDef->GetDefinitionName(), pAttrDef->GetDefinitionIndex() );

			if ( unValue == staticAttrib.m_value.asUint32 )
				return;
		}
		
		// Could be raw integer bits or raw floating-point bits depending on where in the union we stored the value. We copy the
		// bit pattern indiscriminately.
		attribute_data_union_t ResultValue;
		ResultValue = staticAttrib.m_value;
		GenerateEconAttributeValue( pAttrDef, staticAttrib, pGameAccount, &ResultValue );
		LoadEconAttributeValue( pTargetItem, pAttrDef, ResultValue );
	}

	virtual void GenerateEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const static_attrib_t& staticAttrib, const CEconGameAccount *pGameAccount, attribute_data_union_t* out_pValue ) const
	{
		Assert( pAttrDef );
		AssertMsg( pGameAccount || !staticAttrib.m_pKVCustomData, "Cannot run custom logic with no game account object! Passing in NULL for pGameAccount is only supported when we know we won't be running custom value generation code!" );
		Assert( out_pValue );

		if( staticAttrib.m_pKVCustomData )
		{
			Internal_RunCustomAttributeValueLogic( staticAttrib, pGameAccount, out_pValue );
		}
	}
#endif // GC_DLL

	virtual bool BConvertStringToEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const char *pszValue, union attribute_data_union_t *out_pValue, bool bEnableTerribleBackwardsCompatibilitySchemaParsingCode ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_pValue );

		if ( bEnableTerribleBackwardsCompatibilitySchemaParsingCode )
		{
			// Not having any value specified is valid -- we interpret this as "default", or 0 as both an in int and a float.
			out_pValue->asFloat = pszValue
								? atof( pszValue )
								: 0.0f;
		}
		// This is terrible backwards-compatibility code to support the pulling of values from econ asset classes.
		else
		{
			if ( pAttrDef->IsStoredAsInteger() )
			{
				out_pValue->asUint32 = (uint32)Q_atoui64( pszValue );
			}
			else if ( pAttrDef->IsStoredAsFloat() )
			{
				out_pValue->asFloat = Q_atof( pszValue );
			}
			else
			{
				Assert( !"Unknown storage type for CSchemaAttributeType_Default::BConvertStringToEconAttributeValue()!" );
				return false;
			}
		}

		return true;
	}

	virtual void ConvertEconAttributeValueToString( const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value, std::string *out_ps ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_ps );

		if( pAttrDef->IsStoredAsFloat() )
		{
			*out_ps = CFmtStr( "%f", value.asFloat ).Get();
		}
		else if( pAttrDef->IsStoredAsInteger() )
		{
			*out_ps = CFmtStr( "%u", value.asUint32 ).Get();
		}
		else
		{
			Assert( !"Unknown storage type for CSchemaAttributeType_Default::ConvertEconAttributeValueToString()!" );
		}
	}
	
	virtual void ConvertTypedValueToByteStream( const attrib_value_t& typedValue, ::std::string *out_psBytes ) const OVERRIDE
	{
		Assert( out_psBytes );
		Assert( out_psBytes->size() == 0 );

		out_psBytes->resize( sizeof( attrib_value_t ) );
		*reinterpret_cast<attrib_value_t *>( &((*out_psBytes)[0]) ) = typedValue;		// overwrite string contents (sizeof( attrib_value_t ) bytes)
	}

	virtual void ConvertByteStreamToTypedValue( const ::std::string& sBytes, attrib_value_t *out_pTypedValue ) const OVERRIDE
	{
		Assert( out_pTypedValue );
#ifdef GC_DLL
		// The GC is expected to always have internally-consistent information.
		Assert( sBytes.size() == sizeof( attrib_value_t ) );
#else
		// Game clients and servers may have partially out-of-date information, or may have downloaded a new schema
		// but not know how to parse an attribute of a certain type, etc. In these cases, because we know we
		// aren't on the GC, temporarily failing to load these values until the client shuts down and updates
		// is about the best we can hope for.
		if ( sBytes.size() < sizeof( attrib_value_t ) )
		{
			*out_pTypedValue = attrib_value_t();
			return;
		}
#endif

		*out_pTypedValue = *reinterpret_cast<const attrib_value_t *>( &sBytes[0] );
	}

	virtual bool BSupportsGameplayModificationAndNetworking() const OVERRIDE
	{
		return true;
	}
	
private:
#ifdef GC_DLL
	void Internal_RunCustomAttributeValueLogic( const static_attrib_t& staticAttrib, const CEconGameAccount *pGameAccount, attribute_data_union_t* out_pValue ) const
	{
		AssertMsg( pGameAccount, "No game account when running custom attribute value logic!" );

		float flValue = 0;

		const char *pszMethod = staticAttrib.m_pKVCustomData->GetString( "method", NULL );

		if ( Q_stricmp( pszMethod, "employee_number" ) == 0 )
		{
			flValue = pGameAccount->Obj().m_rtime32FirstPlayed;
		}
		else if ( Q_stricmp( pszMethod, "date" ) == 0 ) // Not used?
		{
			flValue = CRTime::RTime32TimeCur();
		}
		else if ( Q_stricmp( pszMethod, "year" ) == 0 )
		{
			flValue = CRTime( CRTime::RTime32TimeCur() ).GetYear();
		}
		else if ( Q_stricmp( pszMethod, "gifts_given_out" ) == 0 )
		{
			flValue = pGameAccount->Obj().m_unNumGiftsGiven;
		}
		else if ( Q_stricmp( pszMethod, "expiration_period_hours_from_now" ) == 0 )
		{
			flValue = CRTime::RTime32DateAdd( CRTime::RTime32TimeCur(), staticAttrib.m_value.asFloat, k_ETimeUnitHour );
		}
		else if ( Q_stricmp( pszMethod, "def index from lootlist" ) == 0 )
		{
			const char* pszLootlistName = staticAttrib.m_pKVCustomData->GetString( "lootlist" );
			// Custom data stores the lootlist
			const CEconLootListDefinition* pLootList = GEconItemSchema().GetLootListByName( pszLootlistName );

			if( !pLootList )
			{
				AssertMsg1( 0, "Lootlist '%s' not found when performing custom attribute logic", pszLootlistName );
				return;
			}

			CUtlVector<CEconLootListDefinition::rolled_item_defs_t> vecRolledItems;
			// Roll our item def
			CDefaultUniformRandomStream RandomStream;
			if( !pLootList->RollRandomItemsAndAdditionalItems( &RandomStream, false, &vecRolledItems ) )
			{
				AssertMsg1( 0, "Error generating item defs from lootlist '%s'", pszLootlistName );
				return;
			}

			// Just take the first one's def index
			flValue = vecRolledItems.Head().m_pItemDef->GetDefinitionIndex();
		}
		else
		{
			AssertMsg1( false, "Unknown value for 'method': '%s'", pszMethod );
		}

		// Put the value into the right part of the union
		if ( staticAttrib.GetAttributeDefinition()->IsStoredAsFloat() )
		{
			(*out_pValue).asFloat = flValue;
		}
		else if ( staticAttrib.GetAttributeDefinition()->IsStoredAsInteger() )
		{
			(*out_pValue).asUint32 = (uint32)flValue;
		}
		else
		{
			AssertMsg1( 0, "Unknown storage type for CSchemaAttributeType_Default::Internal_RunCustomAttributeValueLogic() for attribute %s", staticAttrib.GetAttributeDefinition()->GetDefinitionName() );
		}
	}
#endif
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEconItemAttributeDefinition::CEconItemAttributeDefinition( void )
:	m_pKVAttribute( NULL ),
	m_pAttrType( NULL ),
	m_bHidden( false ),
	m_bWebSchemaOutputForced( false ),
	m_bStoredAsInteger( false ),
	m_bInstanceData( false ),
	m_bIsSetBonus( false ),
	m_iUserGenerationType( 0 ),
	m_iEffectType( ATTRIB_EFFECT_NEUTRAL ),
	m_iDescriptionFormat( 0 ),
	m_pszDescriptionString( NULL ),
	m_pszArmoryDesc( NULL ),
	m_pszDefinitionName( NULL ),
	m_pszAttributeClass( NULL ),
	m_ItemDefinitionTag( INVALID_ECON_TAG_HANDLE ),
	m_bCanAffectMarketName( false ),
	m_bCanAffectRecipeComponentName( false )
#ifndef GC_DLL
  , m_iszAttributeClass( NULL_STRING )
#endif
{
}


//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CEconItemAttributeDefinition::CEconItemAttributeDefinition( const CEconItemAttributeDefinition &that )
{
	(*this) = that;
}


//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CEconItemAttributeDefinition &CEconItemAttributeDefinition::operator=( const CEconItemAttributeDefinition &rhs )
{
	m_nDefIndex = rhs.m_nDefIndex;
	m_pAttrType = rhs.m_pAttrType;
	m_bHidden = rhs.m_bHidden;
	m_bWebSchemaOutputForced = rhs.m_bWebSchemaOutputForced;
	m_bStoredAsInteger = rhs.m_bStoredAsInteger;
	m_iUserGenerationType = rhs.m_iUserGenerationType;
	m_bInstanceData = rhs.m_bInstanceData;
	m_bIsSetBonus = rhs.m_bIsSetBonus;
	m_iEffectType = rhs.m_iEffectType;
	m_iDescriptionFormat = rhs.m_iDescriptionFormat;
	m_pszDescriptionString = rhs.m_pszDescriptionString;
	m_pszArmoryDesc = rhs.m_pszArmoryDesc;
	m_pszDefinitionName = rhs.m_pszDefinitionName;
	m_pszAttributeClass = rhs.m_pszAttributeClass;
	m_ItemDefinitionTag = rhs.m_ItemDefinitionTag;
	m_bCanAffectMarketName = rhs.m_bCanAffectMarketName;
	m_bCanAffectRecipeComponentName = rhs.m_bCanAffectRecipeComponentName;
#ifndef GC_DLL
	m_iszAttributeClass = rhs.m_iszAttributeClass;
#endif

	m_pKVAttribute = NULL;
	if ( NULL != rhs.m_pKVAttribute )
	{
		m_pKVAttribute = rhs.m_pKVAttribute->MakeCopy();

		// Re-assign string pointers
		m_pszDefinitionName = m_pKVAttribute->GetString("name");
		m_pszDescriptionString = m_pKVAttribute->GetString( "description_string", NULL );
		m_pszArmoryDesc = m_pKVAttribute->GetString( "armory_desc", NULL );
		m_pszAttributeClass = m_pKVAttribute->GetString( "attribute_class", NULL );

		Assert( V_strcmp( m_pszDefinitionName, rhs.m_pszDefinitionName ) == 0 );
		Assert( V_strcmp( m_pszDescriptionString, rhs.m_pszDescriptionString ) == 0 );
		Assert( V_strcmp( m_pszArmoryDesc, rhs.m_pszArmoryDesc ) == 0 );
		Assert( V_strcmp( m_pszAttributeClass, rhs.m_pszAttributeClass ) == 0 );
	}
	else
	{
		Assert( m_pszDefinitionName == NULL );
		Assert( m_pszDescriptionString == NULL );
		Assert( m_pszArmoryDesc == NULL );
		Assert( m_pszAttributeClass == NULL );
	}
	return *this;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CEconItemAttributeDefinition::~CEconItemAttributeDefinition( void )
{
	if ( m_pKVAttribute )
		m_pKVAttribute->deleteThis();
	m_pKVAttribute = NULL;
}


//-----------------------------------------------------------------------------
// Purpose:	Initialize the attribute definition
// Input:	pKVAttribute - The KeyValues representation of the attribute
//			schema - The overall item schema for this attribute
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemAttributeDefinition::BInitFromKV( KeyValues *pKVAttribute, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	m_pKVAttribute = pKVAttribute->MakeCopy();
	m_nDefIndex = Q_atoi( m_pKVAttribute->GetName() );
	
	m_pszDefinitionName = m_pKVAttribute->GetString("name", "(unnamed)");
	m_bHidden = m_pKVAttribute->GetInt( "hidden", 0 ) != 0;
	m_bWebSchemaOutputForced = m_pKVAttribute->GetInt( "force_output_description", 0 ) != 0;
	m_bStoredAsInteger = m_pKVAttribute->GetInt( "stored_as_integer", 0 ) != 0;
	m_bIsSetBonus = m_pKVAttribute->GetBool( "is_set_bonus", false );
	m_bCanAffectMarketName = m_pKVAttribute->GetBool( "can_affect_market_name", false );
	m_bCanAffectRecipeComponentName = m_pKVAttribute->GetBool( "can_affect_recipe_component_name", false );
	m_iUserGenerationType = m_pKVAttribute->GetInt( "is_user_generated", 0 );
	m_iEffectType = (attrib_effect_types_t)StringFieldToInt( m_pKVAttribute->GetString("effect_type"), g_EffectTypes, ARRAYSIZE(g_EffectTypes) );
	m_iDescriptionFormat = StringFieldToInt( m_pKVAttribute->GetString("description_format"), g_AttributeDescriptionFormats, ARRAYSIZE(g_AttributeDescriptionFormats) );
	m_pszDescriptionString = m_pKVAttribute->GetString( "description_string", NULL );
	m_pszArmoryDesc = m_pKVAttribute->GetString( "armory_desc", NULL );
	m_pszAttributeClass = m_pKVAttribute->GetString( "attribute_class", NULL );
	m_bInstanceData = pKVAttribute->GetBool( "instance_data", false );

	const char *pszTag = m_pKVAttribute->GetString( "apply_tag_to_item_definition", NULL );
	m_ItemDefinitionTag = pszTag ? GetItemSchema()->GetHandleForTag( pszTag ) : INVALID_ECON_TAG_HANDLE;

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	m_iszAttributeClass = NULL_STRING;
#endif
	const char *pszAttrType = m_pKVAttribute->GetString( "attribute_type", NULL );		// NULL implies "default type" for backwards compatibility
	m_pAttrType = GetItemSchema()->GetAttributeType( pszAttrType );

	SCHEMA_INIT_CHECK( 
		NULL != m_pKVAttribute->FindKey( "name" ), 
		"Attribute definition %s: Missing required field \"name\"", m_pKVAttribute->GetName() );

	SCHEMA_INIT_CHECK(
		NULL != m_pAttrType,
		"Attribute definition %s: Unable to find attribute data type '%s'", m_pszDefinitionName, pszAttrType ? pszAttrType : "(default)" );

	if ( m_bIsSetBonus )
	{
		SCHEMA_INIT_CHECK(
			m_pAttrType->BSupportsGameplayModificationAndNetworking(),
			"Attribute definition %s: set as set bonus attribute but does not support gameplay modification/networking!", m_pszDefinitionName );
	}

	m_unAssetClassBucket = pKVAttribute->GetInt( "asset_class_bucket", 0 );
	m_eAssetClassAttrExportRule = k_EAssetClassAttrExportRule_Default;
	if ( char const *szRule = pKVAttribute->GetString( "asset_class_export", NULL ) )
	{
		if ( !V_stricmp( szRule, "skip" ) )
		{
			m_eAssetClassAttrExportRule = k_EAssetClassAttrExportRule_Skip;
		}
		else if ( !V_stricmp( szRule, "gconly" ) )
		{
			m_eAssetClassAttrExportRule = EAssetClassAttrExportRule_t( k_EAssetClassAttrExportRule_GCOnly | k_EAssetClassAttrExportRule_Skip );
		}
		else if ( !V_stricmp( szRule, "bucketed" ) )
		{
			SCHEMA_INIT_CHECK( m_unAssetClassBucket, "Attribute definition %s: Asset class export rule '%s' is incompatible", m_pszDefinitionName, szRule );
			m_eAssetClassAttrExportRule = k_EAssetClassAttrExportRule_Bucketed;
		}
		else if ( !V_stricmp( szRule, "default" ) )
		{
			m_eAssetClassAttrExportRule = k_EAssetClassAttrExportRule_Default;
		}
		else
		{
			SCHEMA_INIT_CHECK( false, "Attribute definition %s: Invalid asset class export rule '%s'", m_pszDefinitionName, szRule );
		}
	}

	// Check for misuse of asset class bucket
	SCHEMA_INIT_CHECK( ( !m_unAssetClassBucket || m_bInstanceData ), "Attribute definition %s: Cannot use \"asset_class_bucket\" on class-level attributes", m_pKVAttribute->GetName() );


	return SCHEMA_INIT_SUCCESS();
}

CQuestObjectiveDefinition::CQuestObjectiveDefinition( void )
	: m_pszDescriptionToken( NULL )
	, m_nDefIndex( 0 )
	, m_nPoints( 0 )
{}

CQuestObjectiveDefinition::~CQuestObjectiveDefinition()
{}

bool CQuestObjectiveDefinition::BInitFromKV( KeyValues *pKVItem, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	m_nDefIndex = pKVItem->GetInt( "defindex", -1 );
	m_pszDescriptionToken = pKVItem->GetString( "description_string" );
	m_bOptional = pKVItem->GetBool( "optional", false );
	m_bAdvanced = pKVItem->GetBool( "advanced", false );
	m_nPoints = pKVItem->GetInt( "points", 0 );

	SCHEMA_INIT_CHECK( m_nDefIndex != -1, "Quest objective missing def index" );
	SCHEMA_INIT_CHECK( m_pszDescriptionToken != NULL, "Quest objective is missing a description" );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Constructor
//-----------------------------------------------------------------------------
CEconItemDefinition::CEconItemDefinition( void )
:	m_pKVItem( NULL ),
m_bEnabled( false ),
m_unMinItemLevel( 1 ),
m_unMaxItemLevel( 1 ),
m_iArmoryRemap( 0 ),
m_iStoreRemap( 0 ),
m_nItemQuality( k_unItemQuality_Any ),
m_nForcedItemQuality( k_unItemQuality_Any ),
m_nDefaultDropQuantity( 1 ),
m_bLoadOnDemand( false ),
m_pTool( NULL ),
m_rtExpiration( 0 ),
m_BundleInfo( NULL ),
#ifdef TF_CLIENT_DLL
m_unNumConcreteItems( 0 ),
#endif // TF_CLIENT_DLL
m_nPopularitySeed( 0 ),
m_pszDefinitionName( NULL ),
m_pszItemClassname( NULL ),
m_pszClassToken( NULL ),
m_pszSlotToken( NULL ),
m_pszItemBaseName( NULL ),
m_pszItemTypeName( NULL ),
m_pszItemDesc( NULL ),
m_pszArmoryDesc( NULL ),
m_pszInventoryModel( NULL ),
m_pszInventoryImage( NULL ),
m_pszHolidayRestriction( NULL ),
m_iSubType( 0 ),
m_pszBaseDisplayModel( NULL ),
m_iDefaultSkin( -1 ),
m_pszWorldDisplayModel( NULL ),
m_pszWorldExtraWearableModel( NULL ),
m_pszWorldExtraWearableViewModel( NULL ),
m_pszVisionFilteredDisplayModel( NULL ),
m_pszBrassModelOverride( NULL ),
m_bHideBodyGroupsDeployedOnly( false ),
m_bAttachToHands( false ),
m_bAttachToHandsVMOnly( false ),
m_bProperName( false ),
m_bFlipViewModel( false ),
m_bActAsWearable( false ),
m_bActAsWeapon( false ),
m_iDropType( 1 ),
m_bHidden( false ),
m_bShouldShowInArmory( false ),
m_bIsPackBundle( false ),
m_pOwningPackBundle( NULL ),
m_bIsPackItem( false ),
m_bBaseItem( false ),
m_pszItemLogClassname( NULL ),
m_pszItemIconClassname( NULL ),
m_pszDatabaseAuditTable( NULL ),
m_bImported( false ),
m_pItemSetDef( NULL ),
m_pItemCollectionDef( NULL ),
m_pItemPaintKitDef( NULL ),
m_pszArmoryRemap( NULL ),
m_pszStoreRemap( NULL ),
m_unSetItemRemapDefIndex( INVALID_ITEM_DEF_INDEX ),
m_pszXifierRemapClass( NULL ),
m_pszBaseFunctionalItemName( NULL ),
m_pszParticleSuffix( NULL ),
m_pszCollectionReference( NULL ),
m_nItemRarity( k_unItemRarity_Any ),
m_unItemSeries( 0 ),
m_bValidForShuffle( false ),
m_bValidForSelfMade( true )
{
	for ( int team = 0; team < TEAM_VISUAL_SECTIONS; team++ )
	{
		m_PerTeamVisuals[team] = NULL;
	}

	m_pDictIcons = new CUtlDict< CUtlString >;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CEconItemDefinition::~CEconItemDefinition( void )
{
	for ( int i = 0; i < ARRAYSIZE( m_PerTeamVisuals ); i++ )
		delete m_PerTeamVisuals[i];

#ifdef GC_DLL
	m_vecPropertyGenerators.PurgeAndDeleteElements();
#endif // GC_DLL

	if ( m_pKVItem )
		m_pKVItem->deleteThis();
	m_pKVItem = NULL;
	delete m_pTool;
	delete m_BundleInfo;
	delete m_pDictIcons;
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose: Stomp our base data with extra testing data specified by the player
//-----------------------------------------------------------------------------
bool CEconItemDefinition::BInitFromTestItemKVs( int iNewDefIndex, KeyValues *pKVItem, CUtlVector<CUtlString>* pVecErrors )
{
	// The KeyValues are stored in the player entity, so we can cache our name there

	m_nDefIndex = iNewDefIndex;
	m_unSetItemRemapDefIndex = m_nDefIndex;

	bool bTestingExistingItem = pKVItem->GetBool( "test_existing_item", false );
	if ( !bTestingExistingItem )
	{
		m_pszDefinitionName = pKVItem->GetString( "name", NULL );
		m_pszItemBaseName = pKVItem->GetString( "name", NULL );

#ifdef CLIENT_DLL
		pKVItem->SetString( "name", VarArgs("Test Item %d", iNewDefIndex) );
#else
		pKVItem->SetString( "name", UTIL_VarArgs("Test Item %d", iNewDefIndex) );
#endif

		m_pszBaseDisplayModel = pKVItem->GetString( "model_player", NULL );
		m_pszVisionFilteredDisplayModel = pKVItem->GetString( "model_vision_filtered", NULL );
		m_bAttachToHands = pKVItem->GetInt( "attach_to_hands", 0 ) != 0;

		BInitVisualBlockFromKV( pKVItem );
	}

	// Handle attributes
	m_vecStaticAttributes.Purge();
	int iPaintCanIndex = pKVItem->GetInt("paintcan_index", 0);
	if ( iPaintCanIndex )
	{
		static CSchemaAttributeDefHandle pAttrDef_PaintRGB( "set item tint RGB" );

		const CEconItemDefinition *pCanDef = GetItemSchema()->GetItemDefinition(iPaintCanIndex);
		
		float flRGBVal;
		if ( pCanDef && pAttrDef_PaintRGB && FindAttribute_UnsafeBitwiseCast<attrib_value_t>( pCanDef, pAttrDef_PaintRGB, &flRGBVal ) )
		{
			static_attrib_t& StaticAttrib = m_vecStaticAttributes[ m_vecStaticAttributes.AddToTail() ];

			StaticAttrib.iDefIndex = pAttrDef_PaintRGB->GetDefinitionIndex();
			StaticAttrib.m_value.asFloat = flRGBVal;							// this is bad! but we're in crazy hack code for UI customization of item definitions that don't exist so
		}
	}

	int iUnusualEffectIndex = pKVItem->GetInt( "unusual_index", 0 );
	if ( iUnusualEffectIndex )
	{
		static CSchemaAttributeDefHandle pAttrDef_AttachParticleStatic( "attach particle effect static" );

		const attachedparticlesystem_t *pSystem = GetItemSchema()->GetAttributeControlledParticleSystem( iUnusualEffectIndex );

		if ( pAttrDef_AttachParticleStatic && pSystem )
		{
			static_attrib_t& StaticAttrib = m_vecStaticAttributes[ m_vecStaticAttributes.AddToTail() ];

			StaticAttrib.iDefIndex = pAttrDef_AttachParticleStatic->GetDefinitionIndex();
			StaticAttrib.m_value.asFloat = iUnusualEffectIndex;					// this is bad! but we're in crazy hack code for UI customization of item definitions that don't exist so
		}
	}

	return true;
}

animation_on_wearable_t *GetOrCreateAnimationActivity( perteamvisuals_t *pVisData, const char *pszActivityName )
{
	FOR_EACH_VEC( pVisData->m_Animations, i )
	{
		if ( Q_stricmp(pVisData->m_Animations[i].pszActivity, pszActivityName) == 0 )
			return &pVisData->m_Animations[i];
	}

	animation_on_wearable_t *pEntry = &pVisData->m_Animations[pVisData->m_Animations.AddToTail()];

	pEntry->iActivity = kActivityLookup_Unknown;	// We can't look it up yet, the activity list hasn't been populated.
	pEntry->pszActivity = pszActivityName;
	pEntry->iReplacement = kActivityLookup_Unknown;
	pEntry->pszReplacement = NULL;
	pEntry->pszSequence = NULL;
	pEntry->pszScene = NULL;
	pEntry->pszRequiredItem = NULL;

	return pEntry;
}

activity_on_wearable_t *GetOrCreatePlaybackActivity( perteamvisuals_t *pVisData, wearableanimplayback_t iPlayback )
{
	FOR_EACH_VEC( pVisData->m_Animations, i )
	{
		if ( pVisData->m_Activities[i].iPlayback == iPlayback )
			return &pVisData->m_Activities[i];
	}

	activity_on_wearable_t *pEntry = &pVisData->m_Activities[pVisData->m_Activities.AddToTail()];

	pEntry->iPlayback = iPlayback;
	pEntry->iActivity = kActivityLookup_Unknown;	// We can't look it up yet, the activity list hasn't been populated.
	pEntry->pszActivity = NULL;

	return pEntry;
}

#endif // defined(CLIENT_DLL) || defined(GAME_DLL)

//-----------------------------------------------------------------------------
// Purpose: Handle parsing the per-team visual block from the keyvalues
//-----------------------------------------------------------------------------
void CEconItemDefinition::BInitVisualBlockFromKV( KeyValues *pKVItem, CUtlVector<CUtlString> *pVecErrors )
{
	// Visuals
	for ( int team = 0; team < TEAM_VISUAL_SECTIONS; team++ )
	{
		m_PerTeamVisuals[team] = NULL;

		if ( !g_TeamVisualSections[team] )
			continue;

		KeyValues *pVisualsKV = pKVItem->FindKey( g_TeamVisualSections[team] );
		if ( pVisualsKV )
		{
			perteamvisuals_t *pVisData = new perteamvisuals_t();
#if defined(CLIENT_DLL) || defined(GAME_DLL)
			KeyValues *pKVEntry = pVisualsKV->GetFirstSubKey();
			while ( pKVEntry )
			{
				const char *pszEntry = pKVEntry->GetName();

				if ( !Q_stricmp( pszEntry, "use_visualsblock_as_base" ) )
				{
					// Start with a copy of an existing PerTeamVisuals
					const char *pszString = pKVEntry->GetString();
					int nOverrideTeam = GetTeamVisualsFromString( pszString );
					if ( nOverrideTeam != -1 )
					{
						*pVisData = *m_PerTeamVisuals[nOverrideTeam];
					}
					else
					{
						pVecErrors->AddToTail( CFmtStr( "Unknown visuals block: %s", pszString ).Access() );
					}
				}
				else if ( !Q_stricmp( pszEntry, "attached_models" ) )
				{
					FOR_EACH_SUBKEY( pKVEntry, pKVAttachedModelData )
					{
						int iAtt = pVisData->m_AttachedModels.AddToTail();
						pVisData->m_AttachedModels[iAtt].m_iModelDisplayFlags = pKVAttachedModelData->GetInt( "model_display_flags", kAttachedModelDisplayFlag_MaskAll );
						pVisData->m_AttachedModels[iAtt].m_pszModelName = pKVAttachedModelData->GetString( "model", NULL );
					}
				}
				else if ( !Q_stricmp( pszEntry, "attached_models_festive" ) )
				{
					FOR_EACH_SUBKEY( pKVEntry, pKVAttachedModelData )
					{
						int iAtt = pVisData->m_AttachedModelsFestive.AddToTail();
						pVisData->m_AttachedModelsFestive[iAtt].m_iModelDisplayFlags = pKVAttachedModelData->GetInt( "model_display_flags", kAttachedModelDisplayFlag_MaskAll );
						pVisData->m_AttachedModelsFestive[iAtt].m_pszModelName = pKVAttachedModelData->GetString( "model", NULL );
					}
				}
				else if ( !Q_stricmp( pszEntry, "attached_particlesystems" ) )
				{
					FOR_EACH_SUBKEY( pKVEntry, pKVAttachedParticleSystemData )
					{
						int iAtt = pVisData->m_AttachedParticles.AddToTail();
						pVisData->m_AttachedParticles[iAtt].pszSystemName = pKVAttachedParticleSystemData->GetString( "system", NULL );
						pVisData->m_AttachedParticles[iAtt].pszControlPoints[0] = pKVAttachedParticleSystemData->GetString( "attachment", NULL );
						pVisData->m_AttachedParticles[iAtt].bFollowRootBone = pKVAttachedParticleSystemData->GetBool( "attach_to_rootbone" );
						pVisData->m_AttachedParticles[iAtt].iCustomType = 0;
					}
				}
				else if ( !Q_stricmp( pszEntry, "custom_particlesystem2" ) )
				{
					int iAtt = pVisData->m_AttachedParticles.AddToTail();
					pVisData->m_AttachedParticles[iAtt].pszSystemName = pKVEntry->GetString( "system", NULL );
					pVisData->m_AttachedParticles[iAtt].iCustomType = 2;
				}
				else if ( !Q_stricmp( pszEntry, "custom_particlesystem" ) )
				{
					int iAtt = pVisData->m_AttachedParticles.AddToTail();
					pVisData->m_AttachedParticles[iAtt].pszSystemName = pKVEntry->GetString( "system", NULL );
					pVisData->m_AttachedParticles[iAtt].iCustomType = 1;
				}
				else if ( !Q_stricmp( pszEntry, "playback_activity" ) )
				{
					FOR_EACH_SUBKEY( pKVEntry, pKVSubKey )
					{
						int iPlaybackInt = StringFieldToInt( pKVSubKey->GetName(), g_WearableAnimTypeStrings, ARRAYSIZE(g_WearableAnimTypeStrings) );
						if ( iPlaybackInt >= 0 )
						{
							activity_on_wearable_t *pEntry = GetOrCreatePlaybackActivity( pVisData, (wearableanimplayback_t)iPlaybackInt );
							pEntry->pszActivity = pKVSubKey->GetString();
						}
					}
				}
				else if ( !Q_stricmp( pszEntry, "animation_replacement" ) )
				{
					FOR_EACH_SUBKEY( pKVEntry, pKVSubKey )
					{
						animation_on_wearable_t *pEntry = GetOrCreateAnimationActivity( pVisData, pKVSubKey->GetName() );
						pEntry->pszReplacement = pKVSubKey->GetString();
					}
				}
				else if ( !Q_stricmp( pszEntry, "animation_sequence" ) )
				{
					FOR_EACH_SUBKEY( pKVEntry, pKVSubKey )
					{
						animation_on_wearable_t *pEntry = GetOrCreateAnimationActivity( pVisData, pKVSubKey->GetName() );
						pEntry->pszSequence = pKVSubKey->GetString();
					}
				}
				else if ( !Q_stricmp( pszEntry, "animation_scene" ) )
				{
					FOR_EACH_SUBKEY( pKVEntry, pKVSubKey )
					{
						animation_on_wearable_t *pEntry = GetOrCreateAnimationActivity( pVisData, pKVSubKey->GetName() );
						pEntry->pszScene = pKVSubKey->GetString();
					}
				}
				else if ( !Q_stricmp( pszEntry, "animation_required_item" ) )
				{
					FOR_EACH_SUBKEY( pKVEntry, pKVSubKey )
					{
						animation_on_wearable_t *pEntry = GetOrCreateAnimationActivity( pVisData, pKVSubKey->GetName() );
						pEntry->pszRequiredItem = pKVSubKey->GetString();
					}
				}
				else if ( !Q_stricmp( pszEntry, "player_bodygroups" ) )
				{
					FOR_EACH_SUBKEY( pKVEntry, pKVBodygroupKey )
					{
						const char *pszBodygroupName = pKVBodygroupKey->GetName();
						int iValue = pKVBodygroupKey->GetInt();

						// Track bodygroup information for this item in particular.
						pVisData->m_Maps.m_ModifiedBodyGroupNames.Insert( pszBodygroupName, iValue );

						// Track global schema state.
						GetItemSchema()->AssignDefaultBodygroupState( pszBodygroupName, iValue );
					}
				}
				else if ( !Q_stricmp( pszEntry, "skin" ) )
				{
					pVisData->iSkin = pKVEntry->GetInt();
				}
				else if ( !Q_stricmp( pszEntry, "use_per_class_bodygroups" ) )
				{
					pVisData->bUsePerClassBodygroups = pKVEntry->GetBool();
				}
				else if ( !Q_stricmp( pszEntry, "muzzle_flash" ) )
				{
					pVisData->pszMuzzleFlash = pKVEntry->GetString();
				}
				else if ( !Q_stricmp( pszEntry, "tracer_effect" ) )
				{
					pVisData->pszTracerEffect = pKVEntry->GetString();
				}
				else if ( !Q_stricmp( pszEntry, "particle_effect" ) )
				{
					pVisData->pszParticleEffect = pKVEntry->GetString();
				}
				else if ( !Q_strnicmp( pszEntry, "custom_sound", 12 ) )			// intentionally comparing prefixes
				{
					int iIndex = 0;
					if ( pszEntry[12] )
					{
						iIndex = clamp( atoi( &pszEntry[12] ), 0, MAX_VISUALS_CUSTOM_SOUNDS-1 );
					}
					pVisData->pszCustomSounds[iIndex] = pKVEntry->GetString();
				}
				else if ( !Q_stricmp( pszEntry, "material_override" ) )
				{
					pVisData->pszMaterialOverride = pKVEntry->GetString();
				}
				else if ( !Q_strnicmp( pszEntry, "sound_", 6 ) )				// intentionally comparing prefixes
				{
					int iIndex = GetWeaponSoundFromString( &pszEntry[6] );
					if ( iIndex != -1 )
					{
						pVisData->pszWeaponSoundReplacements[iIndex] = pKVEntry->GetString();
					}
				}
				else if ( !Q_stricmp( pszEntry, "code_controlled_bodygroup" ) )
				{
					const char *pBodyGroupName = pKVEntry->GetString( "bodygroup", NULL );
					const char *pFuncName = pKVEntry->GetString( "function", NULL );
					if ( pBodyGroupName && pFuncName )
					{
						codecontrolledbodygroupdata_t ccbgd = { pFuncName, NULL };
						pVisData->m_Maps.m_CodeControlledBodyGroupNames.Insert( pBodyGroupName, ccbgd );
					}
				}
				else if ( !Q_stricmp( pszEntry, "vm_bodygroup_override" ) )
				{
					pVisData->m_iViewModelBodyGroupOverride = pKVEntry->GetInt();
				}
				else if ( !Q_stricmp( pszEntry, "vm_bodygroup_state_override" ) )
				{
					pVisData->m_iViewModelBodyGroupStateOverride = pKVEntry->GetInt();
				}
				else if ( !Q_stricmp( pszEntry, "wm_bodygroup_override" ) )
				{
					pVisData->m_iWorldModelBodyGroupOverride = pKVEntry->GetInt();
				}
				else if ( !Q_stricmp( pszEntry, "wm_bodygroup_state_override" ) )
				{
					pVisData->m_iWorldModelBodyGroupStateOverride = pKVEntry->GetInt();
				}

				pKVEntry = pKVEntry->GetNextKey();
			}
#endif // defined(CLIENT_DLL) || defined(GAME_DLL)
			KeyValues *pStylesDataKV = pVisualsKV->FindKey( "styles" );
			if ( pStylesDataKV )
			{
				// Styles are only valid in the base "visuals" section.
				if ( team == 0 )
				{
					BInitStylesBlockFromKV( pStylesDataKV, pVisData, pVecErrors );
				}
				// ...but they used to be valid everywhere, so spit out a warning if people are trying to use
				// the old style of per-team styles.
				else
				{
					pVecErrors->AddToTail( "Per-team styles blocks are no longer valid. Use \"skin_red\" and \"skin_blu\" in a style entry instead." );
				}
			}

			m_PerTeamVisuals[team] = pVisData;
		}
	}
}

#ifdef GC_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template < typename T >
static void NthPermutation ( T *pData, unsigned int unDataCount, unsigned int unIdx )
{
	for ( unsigned int i = 1; i < unDataCount; i++ )
	{
		std::swap( pData[ unIdx % (i + 1) ], pData[ i ] );
		unIdx = unIdx / (i + 1);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconItemDefinition::BApplyPropertyGenerators( CEconItem *pItem ) const
{
	Assert( pItem );

	for ( const IEconItemPropertyGenerator *pPropertyGenerator : m_vecPropertyGenerators )
	{
		if ( !pPropertyGenerator->BGenerateProperties( pItem ) )
			return false;
	}

	return true;
}

#endif // GC_DLL

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEconItemDefinition::GeneratePrecacheModelStrings( bool bDynamicLoad, CUtlVector<const char *> *out_pVecModelStrings ) const
{
	Assert( out_pVecModelStrings );

	// Add base model.
	out_pVecModelStrings->AddToTail( GetBasePlayerDisplayModel() );

	// Add styles.
	if ( GetNumStyles() )
	{
		for ( style_index_t i=0; i<GetNumStyles(); ++i )
		{
			const CEconStyleInfo *pStyle = GetStyleInfo( i );
			Assert( pStyle );

			pStyle->GeneratePrecacheModelStringsForStyle( out_pVecModelStrings );
		}
	}

	// Precache all the attached models
	for ( int team = 0; team < TEAM_VISUAL_SECTIONS; team++ )
	{
		perteamvisuals_t *pPerTeamVisuals = GetPerTeamVisual( team );
		if ( !pPerTeamVisuals )
			continue;

		for ( int model = 0; model < pPerTeamVisuals->m_AttachedModels.Count(); model++ )
		{
			out_pVecModelStrings->AddToTail( pPerTeamVisuals->m_AttachedModels[model].m_pszModelName );
		}

		// Festive
		for ( int model = 0; model < pPerTeamVisuals->m_AttachedModelsFestive.Count(); model++ )
		{
			out_pVecModelStrings->AddToTail( pPerTeamVisuals->m_AttachedModelsFestive[model].m_pszModelName );
		}
	}

	if ( GetExtraWearableModel() )
	{
		out_pVecModelStrings->AddToTail( GetExtraWearableModel() );
	}

	if ( GetExtraWearableViewModel() )
	{
		out_pVecModelStrings->AddToTail( GetExtraWearableViewModel() );
	}

	if ( GetVisionFilteredDisplayModel() )
	{
		out_pVecModelStrings->AddToTail( GetVisionFilteredDisplayModel() );
	}

	// We don't need to cache the inventory model, because it's never loaded by the game
}

void CEconItemDefinition::GeneratePrecacheSoundStrings( bool bDynamicLoad, CUtlVector<const char *> *out_pVecSoundStrings ) const
{
	Assert( out_pVecSoundStrings );

	for ( int iTeam = 0; iTeam < TEAM_VISUAL_SECTIONS; ++iTeam )
	{
		for ( int iSound = 0; iSound < MAX_VISUALS_CUSTOM_SOUNDS; ++iSound )
		{
			const char *pSoundName = GetCustomSound( iTeam, iSound );
			if ( pSoundName && pSoundName[ 0 ] != '\0' )
			{
				out_pVecSoundStrings->AddToTail( pSoundName );
			}
		}
	}
}
#endif // #if defined(CLIENT_DLL) || defined(GAME_DLL)

//-----------------------------------------------------------------------------
const char	*CEconItemDefinition::GetDefinitionString( const char *pszKeyName, const char *pszDefaultValue ) const
{
	// !FIXME! Here we could do a dynamic lookup to apply the prefab overlay logic.
	// This could save a lot of duplicated data
	if ( m_pKVItem )
		return m_pKVItem->GetString( pszKeyName, pszDefaultValue );
	return pszDefaultValue;
}

//-----------------------------------------------------------------------------
KeyValues	*CEconItemDefinition::GetDefinitionKey( const char *pszKeyName ) const
{
	// !FIXME! Here we could do a dynamic lookup to apply the prefab overlay logic.
	// This could save a lot of duplicated data
	if ( m_pKVItem )
		return m_pKVItem->FindKey( pszKeyName );
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Parse the styles sub-section of the visuals block.
//-----------------------------------------------------------------------------
void CEconItemDefinition::BInitStylesBlockFromKV( KeyValues *pKVStyles, perteamvisuals_t *pVisData, CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_SUBKEY( pKVStyles, pKVStyle )
	{
		CEconStyleInfo *pStyleInfo = GetItemSchema()->CreateEconStyleInfo();
		Assert( pStyleInfo );

		pStyleInfo->BInitFromKV( pKVStyle, pVecErrors );

		pVisData->m_Styles.AddToTail( pStyleInfo );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Parse one style from the styles block.
//-----------------------------------------------------------------------------
void CEconStyleInfo::BInitFromKV( KeyValues *pKVStyle, CUtlVector<CUtlString> *pVecErrors )
{
	enum { kInvalidSkinKey = -1, };

	Assert( pKVStyle );

	// A "skin" entry means "use this index for all of our teams, no matter how many we have".
	int iCommonSkin = pKVStyle->GetInt( "skin", kInvalidSkinKey );
	if ( iCommonSkin != kInvalidSkinKey )
	{
		for ( int i = 0; i < TEAM_VISUAL_SECTIONS; i++ )
		{
			m_iSkins[i] = iCommonSkin;
		}
	}

	int iCommonViewmodelSkin = pKVStyle->GetInt( "v_skin", kInvalidSkinKey );
	if ( iCommonViewmodelSkin != kInvalidSkinKey )
	{
		for ( int i=0; i<TEAM_VISUAL_SECTIONS; i++ )
		{
			m_iViewmodelSkins[i] = iCommonViewmodelSkin;
		}
	}

	// If we don't have a base entry, we look for a unique entry for each team. This will be
	// handled in a subclass if necessary.

	// Are we hiding additional bodygroups when this style is active?
	KeyValues *pKVHideBodygroups = pKVStyle->FindKey( "additional_hidden_bodygroups" );
	if ( pKVHideBodygroups )
	{
		FOR_EACH_SUBKEY( pKVHideBodygroups, pKVBodygroup )
		{
			m_vecAdditionalHideBodygroups.AddToTail( pKVBodygroup->GetName() );
		}
	}

	// Remaining common properties.
	m_pszName = pKVStyle->GetString( "name", "#TF_UnknownStyle" );
	m_pszBasePlayerModel = pKVStyle->GetString( "model_player", NULL );
	m_bIsSelectable = pKVStyle->GetBool( "selectable", true );
	m_pszInventoryImage = pKVStyle->GetString( "image_inventory", NULL );

	KeyValues *pKVBodygroup = pKVStyle->FindKey( "bodygroup" );
	if ( pKVBodygroup )
	{
		m_pszBodygroupName = pKVBodygroup->GetString( "name", NULL );
		Assert( m_pszBodygroupName );
		m_iBodygroupSubmodelIndex = pKVBodygroup->GetInt( "submodel_index", -1 );
		Assert( m_iBodygroupSubmodelIndex != -1 );
	}
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEconStyleInfo::GeneratePrecacheModelStringsForStyle( CUtlVector<const char *> *out_pVecModelStrings ) const
{
	Assert( out_pVecModelStrings );

	if ( GetBasePlayerDisplayModel() != NULL )
	{
		out_pVecModelStrings->AddToTail( GetBasePlayerDisplayModel() );
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose:	Item definition initialization helpers.
//-----------------------------------------------------------------------------
static void RecursiveInheritKeyValues( KeyValues *out_pValues, KeyValues *pInstance )
{
	KeyValues *pPrevSubKey = NULL;
	for ( KeyValues * pSubKey = pInstance->GetFirstSubKey(); pSubKey != NULL; pPrevSubKey = pSubKey, pSubKey = pSubKey->GetNextKey() )
	{
		// If this assert triggers, you have an item that uses a prefab but has multiple keys with the same name
		AssertMsg2 ( !pPrevSubKey || pPrevSubKey->GetNameSymbol() != pSubKey->GetNameSymbol(),
			"Item definition \"%s\" has multiple attributes of the same name (%s) can't use prefabs", pInstance->GetName(), pSubKey->GetName() );

		KeyValues::types_t eType = pSubKey->GetDataType();
		switch ( eType )
		{
		case KeyValues::TYPE_STRING:		out_pValues->SetString( pSubKey->GetName(), pSubKey->GetString() );			break;
		case KeyValues::TYPE_INT:			out_pValues->SetInt( pSubKey->GetName(), pSubKey->GetInt() );				break;
		case KeyValues::TYPE_FLOAT:			out_pValues->SetFloat( pSubKey->GetName(), pSubKey->GetFloat() );			break;
		case KeyValues::TYPE_WSTRING:		out_pValues->SetWString( pSubKey->GetName(), pSubKey->GetWString() );		break;
		case KeyValues::TYPE_COLOR:			out_pValues->SetColor( pSubKey->GetName(), pSubKey->GetColor() ) ;			break;
		case KeyValues::TYPE_UINT64:		out_pValues->SetUint64( pSubKey->GetName(), pSubKey->GetUint64() ) ;		break;

		// "NONE" means "KeyValues"
		case KeyValues::TYPE_NONE:
		{
			// We may already have this part of the tree to stuff data into/overwrite, or we
			// may have to make a new block.
			KeyValues *pNewChild = out_pValues->FindKey( pSubKey->GetName() );
			if ( !pNewChild )
			{
				pNewChild = out_pValues->CreateNewKey();
				pNewChild->SetName( pSubKey->GetName() );
			}

			RecursiveInheritKeyValues( pNewChild, pSubKey );
			break;
		}

		case KeyValues::TYPE_PTR:
		default:
			Assert( !"Unhandled data type for KeyValues inheritance!" );
			break;
		}
	}
}

void MergeDefinitionPrefab( KeyValues *pKVWriteItem, KeyValues *pKVSourceItem )
{
	Assert( pKVWriteItem );
	Assert( pKVSourceItem );

	const char *svPrefabName = pKVSourceItem->GetString( "prefab", NULL );
	
	if ( svPrefabName )
	{
		CUtlStringList vecPrefabs;

		Q_SplitString( svPrefabName, " ", vecPrefabs );

		// Iterate backwards so adjectives get applied over the noun prefab
		// e.g. wet scared cat would apply cat first, then scared and wet.
		FOR_EACH_VEC_BACK( vecPrefabs, i )
		{
			KeyValues *pKVPrefab = GetItemSchema()->FindDefinitionPrefabByName( vecPrefabs[i] );
			AssertMsg1( pKVPrefab, "Unable to find prefab \"%s\".", vecPrefabs[i] );

			if ( pKVPrefab )
			{
				MergeDefinitionPrefab( pKVWriteItem, pKVPrefab );
			}
		}
	}

	RecursiveInheritKeyValues( pKVWriteItem, pKVSourceItem );
}

KeyValues *CEconItemSchema::FindDefinitionPrefabByName( const char *pszPrefabName ) const
{
	int iIndex = m_mapDefinitionPrefabs.Find( pszPrefabName );
	if ( m_mapDefinitionPrefabs.IsValidIndex( iIndex ) )
		return m_mapDefinitionPrefabs[iIndex];

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CEconItemSchema::FindStringTableEntry( const char *pszTableName, int iIndex ) const
{
	SchemaStringTableDict_t::IndexType_t i = m_dictStringTable.Find( pszTableName );
	if ( !m_dictStringTable.IsValidIndex( i ) )
		return NULL;

	const CUtlVector< schema_string_table_entry_t >& vec = *m_dictStringTable[i];
	FOR_EACH_VEC( vec, j )
	{
		if ( vec[j].m_iIndex == iIndex )
			return vec[j].m_pszStr;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:	Initialize the item definition
// Input:	pKVItem - The KeyValues representation of the item
//			schema - The overall item schema for this item
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
#ifdef GC_DLL
	GCConVar gc_steam_payment_rules_kv_key( "gc_steam_payment_rules_kv_key", "payment_rules" );
#endif // GC_DLL


#if defined( WITH_STREAMABLE_WEAPONS )
#if defined( CLIENT_DLL )
    ConVar tf_loadondemand_default("cl_loadondemand_default", "1", FCVAR_ARCHIVE | FCVAR_CLIENTDLL, "The default value for whether items should be delay loaded (1) or loaded now (0).");
#elif defined( GAME_DLL )
    // The server doesn't load on demand by default because it can crash sometimes when this is set. We need to run that down, but in the meantime 
    // we just have it load on demand.
    ConVar tf_loadondemand_default("sv_loadondemand_default", "0", FCVAR_ARCHIVE | FCVAR_GAMEDLL, "The default value for whether items should be delay loaded (1) or loaded now (0).");
#elif defined( GC_DLL )
    GCConVar tf_loadondemand_default("gc_loadondemand_default", "1", FCVAR_ARCHIVE, "The default value for whether items should be delay loaded (1) or loaded now (0).");
#else
#error "Need to add support for streamable weapons to this configuration, or disable streamable weapons here."
#endif
#endif // WITH_STREAMABLE_WEAPONS

bool CEconItemDefinition::BInitFromKV( KeyValues *pKVItem, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	// Set standard members
	m_pKVItem = new KeyValues( pKVItem->GetName() );
	MergeDefinitionPrefab( m_pKVItem, pKVItem );
	m_bEnabled = m_pKVItem->GetBool( "enabled" );

    // initializing this one first so that it will be available for all the errors below
    m_pszDefinitionName = m_pKVItem->GetString( "name", NULL );

#if defined( WITH_STREAMABLE_WEAPONS )
    bool bGotDefault = false;
    m_bLoadOnDemand = m_pKVItem->GetBool( "loadondemand", tf_loadondemand_default.GetBool(), &bGotDefault );

    // This logging is useful for tracking down bugs that crop up because we've (possibly) swapped the default value for loadondemand.
    // But it can be removed once we're satisfied there aren't any bugs as a result of the change (when we cleanup WITH_STREAMABLE_WEAPONS).
    if (bGotDefault)
    {
        DevMsg(10, "Item %s received default value for loadondemand\n", m_pszDefinitionName);
    }
#else
    // Keep the old behavior, which is that loadondemand is defaulted to false.
    m_bLoadOnDemand = m_pKVItem->GetBool("loadondemand");
#endif

	m_nDefIndex = Q_atoi( m_pKVItem->GetName() );
	m_unMinItemLevel = (uint32)m_pKVItem->GetInt( "min_ilevel", GetItemSchema()->GetMinLevel() );
	m_unMaxItemLevel = (uint32)m_pKVItem->GetInt( "max_ilevel", GetItemSchema()->GetMaxLevel() );
	m_nDefaultDropQuantity = m_pKVItem->GetInt( "default_drop_quantity", 1 );

	m_nPopularitySeed = m_pKVItem->GetInt( "popularity_seed", 0 );


#if defined(CLIENT_DLL) || defined(GAME_DLL)
	// We read this manually here in the game dlls. The GC reads it below while checking the global schema.
	GetItemSchema()->BGetItemQualityFromName( m_pKVItem->GetString( "item_quality" ), &m_nItemQuality );
	GetItemSchema()->BGetItemQualityFromName( m_pKVItem->GetString( "forced_item_quality" ), &m_nForcedItemQuality );
#endif

	// Check for required fields
	SCHEMA_INIT_CHECK( 
		NULL != m_pKVItem->FindKey( "name" ), 
		"Item definition %s: Missing required field \"name\"", m_pKVItem->GetName() );

	SCHEMA_INIT_CHECK( 
		NULL != m_pKVItem->FindKey( "item_class" ), 
		"Item definition %s: Missing required field \"item_class\"", m_pKVItem->GetName() );

	// Check value ranges
	SCHEMA_INIT_CHECK( 
		m_pKVItem->GetInt( "min_ilevel" ) >= 0, 
		"Item definition %s: \"min_ilevel\" must be greater than or equal to 0", GetDefinitionName() );

	SCHEMA_INIT_CHECK( 
		m_pKVItem->GetInt( "max_ilevel" ) >= 0, 
		"Item definition %s: \"max_ilevel\" must be greater than or equal to 0", GetDefinitionName() );

	// Check for consistency
#ifdef GC_DLL
	// We don't do these consistency checks in the game, because it doesn't have the data to do them
	SCHEMA_INIT_CHECK( 
		m_unMinItemLevel >= GetItemSchema()->GetMinLevel(), 
		"Item definition %s: min_ilevel (%d) must be greater or equal to Minimum Item Level (%d)", GetDefinitionName(), m_unMinItemLevel, GetItemSchema()->GetMinLevel() );

	SCHEMA_INIT_CHECK( 
		m_unMinItemLevel <= m_unMaxItemLevel,
		"Item definition %s: min_ilevel (%d) must be greater or equal to min_ilevel (%d)", GetDefinitionName(), m_unMaxItemLevel, m_unMinItemLevel );

	SCHEMA_INIT_CHECK( 
		m_unMaxItemLevel <= GetItemSchema()->GetMaxLevel(),
		"Item definition %s: max_ilevel (%d) must be less than or equal to Maximum Item Level (%d)", GetDefinitionName(), m_unMaxItemLevel, GetItemSchema()->GetMaxLevel() );

	// Read the item quality
	if ( m_pKVItem->FindKey( "item_quality" ) )
	{
		SCHEMA_INIT_CHECK( 
			GetItemSchema()->BGetItemQualityFromName( m_pKVItem->GetString( "item_quality" ), &m_nItemQuality ),
			"Item definition %s: Undefined item_quality \"%s\"", GetDefinitionName(), m_pKVItem->GetString( "item_quality" ) );
	}
	if ( m_pKVItem->FindKey( "forced_item_quality" ) )
	{
		SCHEMA_INIT_CHECK( 
			GetItemSchema()->BGetItemQualityFromName( m_pKVItem->GetString( "forced_item_quality" ), &m_nForcedItemQuality ),
			"Item definition %s: Undefined item_quality \"%s\"", GetDefinitionName(), m_pKVItem->GetString( "forced_item_quality" ) );
	}
#endif

	// Rarity
	// Get Index from this string and save the index
	if ( m_pKVItem->FindKey( "item_rarity" ) )
	{
		SCHEMA_INIT_CHECK(
			GetItemSchema()->BGetItemRarityFromName( m_pKVItem->GetString( "item_rarity" ), &m_nItemRarity ),
			"Item definition %s: Undefined item_rarity \"%s\"", GetDefinitionName(), m_pKVItem->GetString( "item_rarity" ) );
	}

	if ( m_pKVItem->FindKey( "item_series" ) )
	{
		// Make sure this is a valid series
		SCHEMA_INIT_CHECK(
			GetItemSchema()->BGetItemSeries( m_pKVItem->GetString( "item_series" ), &m_unItemSeries ),
			"Item definition %s: Undefined item_series \"%s\"", GetDefinitionName(), m_pKVItem->GetString( "item_series" ) );
	}

	// Get the item class
	m_pszItemClassname = m_pKVItem->GetString( "item_class", NULL );

	m_pszClassToken = m_pKVItem->GetString( "class_token_id", NULL );
	m_pszSlotToken = m_pKVItem->GetString( "slot_token_id", NULL );

	// expiration data
	const char *pchExpiration = m_pKVItem->GetString( "expiration_date", NULL );
	if( pchExpiration && pchExpiration[0] )
	{
		if ( pchExpiration[0] == '!' )
		{
			m_rtExpiration = GetItemSchema()->GetCustomExpirationDate( &pchExpiration[1] );
			SCHEMA_INIT_CHECK(
				m_rtExpiration != k_RTime32Nil,
				"Unknown/malformed expiration_date string \"%s\" in item %s.", pchExpiration, m_pszDefinitionName );
		}
		else
		{
			m_rtExpiration = CRTime::RTime32FromFmtString( "YYYY-MM-DD hh:mm:ss" , pchExpiration );

#ifdef GC_DLL
			// Check that if we convert back to a string, we get the same value
			char rtimeBuf[k_RTimeRenderBufferSize];
			SCHEMA_INIT_CHECK(
				Q_strcmp( CRTime::RTime32ToString( m_rtExpiration, rtimeBuf ), pchExpiration ) == 0,
				"Malformed expiration_date \"%s\" for expiration_date in item %s.  Must be of the form \"YYYY-MM-DD hh:mm:ss\".  Input: %s Output: %s InputTime: %u LocalTime: %u Timezone: %lu", pchExpiration, m_pszDefinitionName, pchExpiration,  rtimeBuf, m_rtExpiration, CRTime::RTime32TimeCur(), timezone );
#else
			// Check that if we convert back to a string, we get the same value.  Emit an error, but don't fail in the game code
			char rtimeBuf[k_RTimeRenderBufferSize];
			if ( Q_strcmp( CRTime::RTime32ToString( m_rtExpiration, rtimeBuf ), pchExpiration ) != 0 )
			{
#if ( defined( _MSC_VER ) && _MSC_VER >= 1900 )
#define timezone _timezone
#define daylight _daylight
#endif
				Assert( false );
				Warning( "Malformed expiration_date \"%s\" for expiration_date in item %s.  Must be of the form \"YYYY-MM-DD hh:mm:ss\".  Input: %s Output: %s InputTime: %u LocalTime: %u Timezone: %lu\n", pchExpiration, m_pszDefinitionName, pchExpiration, rtimeBuf, m_rtExpiration, CRTime::RTime32TimeCur(), timezone );
			}				
#endif
		}
	}

	// Display data
	m_pszItemBaseName = m_pKVItem->GetString( "item_name", "" ); // non-NULL to ensure we can sort
	m_pszItemTypeName = m_pKVItem->GetString( "item_type_name", "" ); // non-NULL to ensure we can sort
	m_pszItemDesc = m_pKVItem->GetString( "item_description", NULL );
	m_pszArmoryDesc = m_pKVItem->GetString( "armory_desc", NULL );
	m_pszInventoryModel = m_pKVItem->GetString( "model_inventory", NULL );
	m_pszInventoryImage = m_pKVItem->GetString( "image_inventory", NULL );

	const char* pOverlay = m_pKVItem->GetString( "image_inventory_overlay", NULL );
	if ( pOverlay )
	{
		m_pszInventoryOverlayImages.AddToTail( pOverlay );
	}
	pOverlay = m_pKVItem->GetString( "image_inventory_overlay2", NULL );
	if ( pOverlay )
	{
		m_pszInventoryOverlayImages.AddToTail( pOverlay );
	}

	m_iInventoryImagePosition[0] = atoi( m_pKVItem->GetString( "image_inventory_pos_x", "0" ) );
	m_iInventoryImagePosition[1] = atoi( m_pKVItem->GetString( "image_inventory_pos_y", "0" ) );
	m_iInventoryImageSize[0] = atoi( m_pKVItem->GetString( "image_inventory_size_w", "128" ) );
	m_iInventoryImageSize[1] = atoi( m_pKVItem->GetString( "image_inventory_size_h", "82" ) );
	m_iInspectPanelDistance = m_pKVItem->GetInt( "inspect_panel_dist", 70 );
	m_pszHolidayRestriction = m_pKVItem->GetString( "holiday_restriction", NULL );
	m_nVisionFilterFlags = m_pKVItem->GetInt( "vision_filter_flags", 0 );
	m_iSubType = atoi( m_pKVItem->GetString( "subtype", "0" ) );
	m_pszBaseDisplayModel = m_pKVItem->GetString( "model_player", NULL );
	m_iDefaultSkin = m_pKVItem->GetInt( "default_skin", -1 );
	m_pszWorldDisplayModel = m_pKVItem->GetString( "model_world", NULL ); // Not the ideal method. c_models are better, but this is to solve a retrofit problem with the sticky launcher.
	m_pszWorldExtraWearableModel = m_pKVItem->GetString( "extra_wearable", NULL ); 
	m_pszWorldExtraWearableViewModel = m_pKVItem->GetString( "extra_wearable_vm", NULL );
	m_pszVisionFilteredDisplayModel = pKVItem->GetString( "model_vision_filtered", NULL );
	m_pszBrassModelOverride = m_pKVItem->GetString( "brass_eject_model", NULL );
	m_bHideBodyGroupsDeployedOnly = m_pKVItem->GetBool( "hide_bodygroups_deployed_only" );
	m_bAttachToHands = m_pKVItem->GetInt( "attach_to_hands", 0 ) != 0;
	m_bAttachToHandsVMOnly = m_pKVItem->GetInt( "attach_to_hands_vm_only", 0 ) != 0;
	m_bProperName = m_pKVItem->GetInt( "propername", 0 ) != 0;
	m_bFlipViewModel = m_pKVItem->GetInt( "flip_viewmodel", 0 ) != 0;
	m_bActAsWearable = m_pKVItem->GetInt( "act_as_wearable", 0 ) != 0;
	m_bActAsWeapon = m_pKVItem->GetInt( "act_as_weapon", 0 ) != 0;
	m_bIsTool = m_pKVItem->GetBool( "is_tool", 0 ) || ( GetItemClass() && !V_stricmp( GetItemClass(), "tool" ) );
	m_iDropType = StringFieldToInt( m_pKVItem->GetString("drop_type"), g_szDropTypeStrings, ARRAYSIZE(g_szDropTypeStrings) );
	m_pszCollectionReference = m_pKVItem->GetString( "collection_reference", NULL );

	// Creation data
	m_bHidden = m_pKVItem->GetInt( "hidden", 0 ) != 0;
	m_bShouldShowInArmory = m_pKVItem->GetInt( "show_in_armory", 0 ) != 0;
	m_bBaseItem = m_pKVItem->GetInt( "baseitem", 0 ) != 0;
	m_pszItemLogClassname = m_pKVItem->GetString( "item_logname", NULL );
	m_pszItemIconClassname = m_pKVItem->GetString( "item_iconname", NULL );
	m_pszDatabaseAuditTable = m_pKVItem->GetString( "database_audit_table", NULL );
	m_bImported = m_pKVItem->FindKey( "import_from" ) != NULL;

	// Tool data
	m_pTool = NULL;
	KeyValues *pToolDataKV = m_pKVItem->FindKey( "tool" );
	if ( pToolDataKV )
	{
		const char *pszType = pToolDataKV->GetString( "type", NULL );
		SCHEMA_INIT_CHECK( pszType != NULL, "Tool '%s' missing required type.", m_pKVItem->GetName() );

		// Common-to-all-tools settings.
		const char *pszUseString = pToolDataKV->GetString( "use_string", NULL );
		const char *pszUsageRestriction = pToolDataKV->GetString( "restriction", NULL );
		KeyValues *pToolUsageKV = pToolDataKV->FindKey( "usage" );

		// Common-to-all-tools usage capability flags.
		item_capabilities_t usageCapabilities = (item_capabilities_t)ITEM_CAP_TOOL_DEFAULT;
		KeyValues *pToolUsageCapsKV = pToolDataKV->FindKey( "usage_capabilities" );
		if ( pToolUsageCapsKV )
		{
			KeyValues *pEntry = pToolUsageCapsKV->GetFirstSubKey();
			while ( pEntry )
			{
				ParseCapability( usageCapabilities, pEntry );
				pEntry = pEntry->GetNextKey();
			}
		}

		m_pTool = GetItemSchema()->CreateEconToolImpl( pszType, pszUseString, pszUsageRestriction, usageCapabilities, pToolUsageKV );
		SCHEMA_INIT_CHECK( m_pTool != NULL, "Unable to create tool implementation for '%s', of type '%s'.", m_pKVItem->GetName(), pszType );
	}

	// Bundle
	KeyValues *pBundleDataKV = m_pKVItem->FindKey( "bundle" );
	if ( pBundleDataKV )
	{
		m_BundleInfo = new bundleinfo_t();
		FOR_EACH_SUBKEY( pBundleDataKV, pKVCurItem )
		{
			CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinitionByName( pKVCurItem->GetName() );
			SCHEMA_INIT_CHECK( pItemDef != NULL, "Unable to find item definition '%s' for bundle '%s'.", pKVCurItem->GetName(), m_pszDefinitionName );

			m_BundleInfo->vecItemDefs.AddToTail( pItemDef );
		}

		// Only check for pack bundle if the item is actually a bundle - note that we could do this programatically by checking that all items in the bundle are flagged as a "pack item" - but for now the bundle needs to be explicitly flagged as a pack bundle.
		m_bIsPackBundle = m_pKVItem->GetInt( "is_pack_bundle", 0 ) != 0;
	}

	// capabilities
	m_iCapabilities = (item_capabilities_t)ITEM_CAP_DEFAULT;
	KeyValues *pCapsKV = m_pKVItem->FindKey( "capabilities" );
	if ( pCapsKV )
	{
		KeyValues *pEntry = pCapsKV->GetFirstSubKey();
		while ( pEntry )
		{
			ParseCapability( m_iCapabilities, pEntry );
			pEntry = pEntry->GetNextKey();
		}
	}

	// item_set
	SCHEMA_INIT_CHECK( (!m_pKVItem->GetString( "item_set", NULL )), "Item definition '%s' specifies deprecated \"item_set\" field. Items sets are now specified only in the set itself, not on the definition.", GetDefinitionName() );

	const char *pszSetItemRemapDefIndexName = m_pKVItem->GetString( "set_item_remap", NULL );
	if ( pszSetItemRemapDefIndexName )
	{
		const CEconItemDefinition *pRemapItemDef = GetItemSchema()->GetItemDefinitionByName( pszSetItemRemapDefIndexName );
		m_unSetItemRemapDefIndex = pRemapItemDef ? pRemapItemDef->GetDefinitionIndex() : INVALID_ITEM_DEF_INDEX;

		SCHEMA_INIT_CHECK( m_unSetItemRemapDefIndex != INVALID_ITEM_DEF_INDEX, "Unable to find set item remap definition '%s' for '%s'.", pszSetItemRemapDefIndexName, GetDefinitionName() );
		SCHEMA_INIT_CHECK( m_unSetItemRemapDefIndex != GetDefinitionIndex(), "Unable to set set item remap for definition '%s' to itself.", GetDefinitionName() );
	}
	else
	{
		m_unSetItemRemapDefIndex = GetDefinitionIndex();
	}

	// cache item map names
	m_pszArmoryRemap = m_pKVItem->GetString( "armory_remap", NULL );
	m_pszStoreRemap = m_pKVItem->GetString( "store_remap", NULL );

	m_pszXifierRemapClass = m_pKVItem->GetString( "xifier_class_remap", NULL );
	m_pszBaseFunctionalItemName = m_pKVItem->GetString( "base_item_name", "" );
	m_pszParticleSuffix = m_pKVItem->GetString( "particle_suffix", NULL );

	m_bValidForShuffle = m_pKVItem->GetBool( "valid_for_shuffle", false );
	m_bValidForSelfMade = m_pKVItem->GetBool( "valid_for_self_made", true );

	// Init our visuals blocks.
	BInitVisualBlockFromKV( m_pKVItem, pVecErrors );

	// Calculate our equip region mask.
	{
		m_unEquipRegionMask = 0;
		m_unEquipRegionConflictMask = 0;

		// Our equip region will come from one of two places -- either we have an "equip_regions" (plural) section,
		// in which case we have any number of regions specified; or we have an "equip_region" (singular) section
		// which will have one and exactly one region. If we have "equip_regions" (plural), we ignore whatever is
		// in "equip_region" (singular).
		//
		// Yes, this is sort of dumb.
		CUtlVector<const char *> vecEquipRegionNames;

		KeyValues *pKVMultiEquipRegions = m_pKVItem->FindKey( "equip_regions" ),
				  *pKVSingleEquipRegion = m_pKVItem->FindKey( "equip_region" );

		// Maybe we have multiple entries?
		if ( pKVMultiEquipRegions )
		{
			for ( KeyValues *pKVRegion = pKVMultiEquipRegions->GetFirstSubKey(); pKVRegion; pKVRegion = pKVRegion->GetNextKey() )
			{
				vecEquipRegionNames.AddToTail( pKVRegion->GetName() );
			}
		}
		// This is our one-and-only-one equip region.
		else if ( pKVSingleEquipRegion )
		{
			const char *pEquipRegionName = pKVSingleEquipRegion->GetString( (const char *)NULL, NULL );
			if ( pEquipRegionName )
			{
				vecEquipRegionNames.AddToTail( pEquipRegionName );
			}
		}

		// For each of our regions, add to our conflict mask both ourself and all the regions
		// that we conflict with.
		FOR_EACH_VEC( vecEquipRegionNames, i )
		{
			const char *pszEquipRegionName = vecEquipRegionNames[i];
			equip_region_mask_t unThisRegionMask = GetItemSchema()->GetEquipRegionMaskByName( pszEquipRegionName );

			SCHEMA_INIT_CHECK(
				unThisRegionMask != 0,
				"Item definition %s: Unable to find equip region mask for region named \"%s\"", GetDefinitionName(), vecEquipRegionNames[i] );

			m_unEquipRegionMask |= GetItemSchema()->GetEquipRegionBitMaskByName( pszEquipRegionName );
			m_unEquipRegionConflictMask |= unThisRegionMask;
		}
	}

	// Single-line static attribute parsing.
	{
		KeyValues *pKVStaticAttrsKey = m_pKVItem->FindKey( "static_attrs" );
		if ( pKVStaticAttrsKey )
		{
			FOR_EACH_SUBKEY( pKVStaticAttrsKey, pKVKey )
			{
				static_attrib_t staticAttrib;

				SCHEMA_INIT_SUBSTEP( staticAttrib.BInitFromKV_SingleLine( GetDefinitionName(), pKVKey, pVecErrors, false ) );
				m_vecStaticAttributes.AddToTail( staticAttrib );

				// Does this attribute specify a tag to apply to this item definition?
				Assert( staticAttrib.GetAttributeDefinition() );
			}
		}
	}

	// Old style attribute parsing. Really only useful now for GC-generated attributes.
	KeyValues *pKVAttribKey = m_pKVItem->FindKey( "attributes" );
	if ( pKVAttribKey )
	{
		FOR_EACH_SUBKEY( pKVAttribKey, pKVKey )
		{
			static_attrib_t staticAttrib;

			SCHEMA_INIT_SUBSTEP( staticAttrib.BInitFromKV_MultiLine( GetDefinitionName(), pKVKey, pVecErrors ) );
			m_vecStaticAttributes.AddToTail( staticAttrib );

			// Does this attribute specify a tag to apply to this item definition?
			Assert( staticAttrib.GetAttributeDefinition() );
		}
	}

	// Initialize tags based on all static attributes for this item.
	for ( const static_attrib_t& attr : m_vecStaticAttributes )
	{
		const econ_tag_handle_t tag = attr.GetAttributeDefinition()->GetItemDefinitionTag();
		if ( tag != INVALID_ECON_TAG_HANDLE )
		{
			m_vecTags.AddToTail( tag );
		}
	}

	// Auto-generate tags based on capabilities.
	for ( int i = 0; i < NUM_ITEM_CAPS; i++ )
	{
		if ( m_iCapabilities & (1 << i) )
		{
			m_vecTags.AddToTail( GetItemSchema()->GetHandleForTag( CFmtStr( "auto__cap_%s", g_Capabilities[i] ).Get() ) );
		}
	}

	// Initialize used-specified tags for this item if present.
	KeyValues *pKVTags = m_pKVItem->FindKey( "tags" );
	if ( pKVTags )
	{
		FOR_EACH_SUBKEY( pKVTags, pKVTag )
		{
			m_vecTags.AddToTail( GetItemSchema()->GetHandleForTag( pKVTag->GetName() ) );
		}
	}

#ifdef GC_DLL
	SCHEMA_INIT_SUBSTEP( BCommonInitPropertyGeneratorsFromKV( GetDefinitionName(), &m_vecPropertyGenerators, m_pKVItem->FindKey( "property_generators" ), pVecErrors ) );

	// Parse payment rules on the GC if any exist.
	KeyValues *pKVPaymentRules = m_pKVItem->FindKey( gc_steam_payment_rules_kv_key.GetString() );
	if ( pKVPaymentRules )
	{
		FOR_EACH_TRUE_SUBKEY( pKVPaymentRules, pKVRule ) 
		{
			econ_item_payment_rule_t rule;

			const bool bFoundPaymentRule = BGetPaymentRule( pKVRule, &rule.m_eRuleType, &rule.m_RevenueShare );
			SCHEMA_INIT_CHECK( bFoundPaymentRule, "Item definition '%s': payment rule %s didn't specify a known payment rule type", GetDefinitionName(), pKVRule->GetName() );

			// Allow us to override some of our checks if we want to claim we really know what we're doing.
			if ( !pKVRule->FindKey( "sanity_check_override" ) )
			{
				SCHEMA_INIT_CHECK( rule.m_RevenueShare > 0.0, "Item definition '%s': payment rule %s has invalid revenue share %0.2f", GetDefinitionName(), pKVRule->GetName(), rule.m_RevenueShare );

				// Ordinarily, bundles can only specify the "bundle_revenue_share" payment rule type. However, for backwards compatability
				// with previous payment rules that pre-date the Workshop and had manual percentages set offline, we allow people who know
				// what they're doing to specify the "sanity_check_override" key and then use custom rules.
				SCHEMA_INIT_CHECK( (rule.m_eRuleType == kPaymentRule_Bundle) == IsBundle(), "Item definition '%s': payment rule %s has invalid bundle rules", GetDefinitionName(), pKVRule->GetName() );
			}

			KeyValues *pPaymentRuleForItemdef = pKVRule->FindKey( "payment_rule_for_itemdef" );
			SCHEMA_INIT_CHECK( pPaymentRuleForItemdef && ( pPaymentRuleForItemdef->GetInt() == m_nDefIndex ), "Item definition '%s': payment rule %s has invalid payment_rule_for_itemdef", GetDefinitionName(), pKVRule->GetName() );

			KeyValues *pKVMultiTargets = pKVRule->FindKey( "targets" );
			KeyValues *pKVSingleTarget = pKVRule->FindKey( "target" );
			SCHEMA_INIT_CHECK( pKVMultiTargets == NULL || pKVSingleTarget == NULL, "Item definition '%s': payment rule %s specifies both single- and multi-targets", GetDefinitionName(), pKVRule->GetName() );

			if ( pKVMultiTargets )
			{
				FOR_EACH_SUBKEY( pKVMultiTargets, pKVTarget )
				{
					rule.m_vecValues.AddToTail( (uint64)Q_atoi64( pKVTarget->GetName() ) );
				}
			}

			if ( pKVSingleTarget )
			{
				rule.m_vecValues.AddToTail( pKVSingleTarget->GetUint64() );
			}

			// We expect bundles to have no associated account data at all -- their payment processing
			// is done by splitting the total value of the bundle between each of the items in it. All
			// non-bundle payment rules require at least one data entry.
			SCHEMA_INIT_CHECK( (rule.m_eRuleType == kPaymentRule_Bundle) == (rule.m_vecValues.Count() == 0), "Item definition '%s': payment rule %s has invalid number of target entries %i", GetDefinitionName(), pKVRule->GetName(), rule.m_vecValues.Count() );

			// It doesn't make any sense to have multiple entries for a bundle. We expect their to be one
			// rule, and that's "process this like a bundle".
			if ( rule.m_eRuleType == kPaymentRule_Bundle )
			{
				SCHEMA_INIT_CHECK( m_vecPaymentRules.Count() == 0, "Item definition '%s': only the first payment rule can be specified as 'bundle'", GetDefinitionName() );

				// We only allow bundle payment rules to be applied to actual bundles with contents
				// specified. Without doing this, we would have nowhere to pull the metadata about
				// which items are contained.
				SCHEMA_INIT_CHECK( GetBundleInfo() && GetBundleInfo()->vecItemDefs.Count() > 0, "Item definition '%s': payment rule %s is specified as a bundle but outer item definition has no bundle contents.\n", GetDefinitionName(), pKVRule->GetName() );

				// Bundles rely on sub-items for figuring out payment so the revenue share for the
				// bundle itself is expected to be 100%.
				SCHEMA_INIT_CHECK( rule.m_RevenueShare == 100.0, "Item definition '%s': payment rule %s has invalid bundle revenue share %0.2f", GetDefinitionName(), pKVRule->GetName(), rule.m_RevenueShare );
			}

			const bool bWasNumberedCorrectly = (AddPaymentRule( rule ) == atoi( pKVRule->GetName() ));
			SCHEMA_INIT_CHECK( bWasNumberedCorrectly, "Item definition '%s': misnumbered payment rule %s", GetDefinitionName(), pKVRule->GetName() );
		}
	}
#endif // GC_DLL

	return SCHEMA_INIT_SUCCESS();
}

#ifdef GC_DLL
int CEconItemDefinition::AddPaymentRule( const econ_item_payment_rule_t& newRule )
{
	return m_vecPaymentRules.AddToTail( newRule );
}
#endif // GC_DLL

bool static_attrib_t::BInitFromKV_MultiLine( const char *pszContext, KeyValues *pKVAttribute, CUtlVector<CUtlString> *pVecErrors )
{
	const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinitionByName( pKVAttribute->GetName() );

	SCHEMA_INIT_CHECK( 
		NULL != pAttrDef,
		"Context '%s': Attribute \"%s\" in \"attributes\" did not match any attribute definitions", pszContext, pKVAttribute->GetName() );

	if ( pAttrDef )
	{
		iDefIndex = pAttrDef->GetDefinitionIndex();
			
		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );

		pAttrType->InitializeNewEconAttributeValue( &m_value );

		const char *pszValue = pKVAttribute->GetString( "value", NULL );
		const bool bSuccessfullyLoadedValue = pAttrType->BConvertStringToEconAttributeValue( pAttrDef, pszValue, &m_value, true );

		SCHEMA_INIT_CHECK(
			bSuccessfullyLoadedValue,
			"Context '%s': Attribute \"%s\" could not parse value \"%s\"!", pszContext, pKVAttribute->GetName(), pszValue ? pszValue : "(null)" );

		SCHEMA_INIT_CHECK(
			!pAttrDef->BIsSetBonusAttribute(),
			"Context '%s': Attribute \"%s\" is a set bonus attribute and not supported here", pszContext, pKVAttribute->GetName() );
#ifdef GC_DLL
		bForceGCToGenerate	= pKVAttribute->GetBool( "force_gc_to_generate" );

		KeyValues *pKVLogicData = pKVAttribute->FindKey( "custom_value_logic" );
		if ( pKVLogicData )
		{
			m_pKVCustomData = pKVLogicData->MakeCopy();
		}

		SCHEMA_INIT_CHECK(
			m_pKVCustomData == NULL || bForceGCToGenerate,
			"Context '%s': Attribute \"%s\" is set to have custom logic but is not GC-generated so that logic will never get used!", pszContext, pKVAttribute->GetName() );

		SCHEMA_INIT_CHECK(
			m_pKVCustomData != NULL || pKVAttribute->FindKey( "value" ),
			"Context '%s': Attribute \"%s\" has no value set", pszContext, pKVAttribute->GetName() );

		SCHEMA_INIT_CHECK(
			m_pKVCustomData == NULL || m_pKVCustomData->FindKey( "method" ) != NULL,
			"Context '%s': Attribute \"%s\" custom logic data is set, but custom logic method is not set!", pszContext, pKVAttribute->GetName() );
#endif // GC_DLL
	}

	return SCHEMA_INIT_SUCCESS();
}

bool static_attrib_t::BInitFromKV_SingleLine( const char *pszContext, KeyValues *pKVAttribute, CUtlVector<CUtlString> *pVecErrors, bool bEnableTerribleBackwardsCompatibilitySchemaParsingCode /* = true */ )
{
	const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinitionByName( pKVAttribute->GetName() );

	SCHEMA_INIT_CHECK( 
		NULL != pAttrDef,
		"Context '%s': Attribute \"%s\" in \"attributes\" did not match any attribute definitions", pszContext, pKVAttribute->GetName() );

	if ( pAttrDef )
	{
		iDefIndex = pAttrDef->GetDefinitionIndex();
			
		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );

		pAttrType->InitializeNewEconAttributeValue( &m_value );

		const char *pszValue = pKVAttribute->GetString();
		const bool bSuccessfullyLoadedValue = pAttrType->BConvertStringToEconAttributeValue( pAttrDef, pszValue, &m_value, bEnableTerribleBackwardsCompatibilitySchemaParsingCode );

		SCHEMA_INIT_CHECK(
			bSuccessfullyLoadedValue,
			"Context '%s': Attribute \"%s\" could not parse value \"%s\"!", pszContext, pKVAttribute->GetName(), pszValue ? pszValue : "(null)" );

		SCHEMA_INIT_CHECK(
			!pAttrDef->BIsSetBonusAttribute(),
			"Context '%s': Attribute \"%s\" is a set bonus attribute and not supported here", pszContext, pKVAttribute->GetName() );

#ifdef GC_DLL
		bForceGCToGenerate = false;
		m_pKVCustomData = NULL;
#endif // GC_DLL
	}

	return SCHEMA_INIT_SUCCESS();
}



bool CEconItemDefinition::BInitItemMappings( CUtlVector<CUtlString> *pVecErrors )
{
	// Armory remapping
	if ( m_pszArmoryRemap && m_pszArmoryRemap[0] )
	{
		CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( m_pszArmoryRemap );
		if ( pDef )
		{
			m_iArmoryRemap = pDef->GetDefinitionIndex();
		}

		SCHEMA_INIT_CHECK( 
			pDef != NULL,
			"Item %s: Armory remap definition \"%s\" was not found", m_pszItemBaseName, m_pszArmoryRemap );
	}

	// Store remapping
	if ( m_pszStoreRemap && m_pszStoreRemap[0] )
	{
		CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( m_pszStoreRemap );
		if ( pDef )
		{
			m_iStoreRemap = pDef->GetDefinitionIndex();
		}

		SCHEMA_INIT_CHECK(
			pDef != NULL,
			"Item %s: Store remap definition \"%s\" was not found", m_pszItemBaseName, m_pszStoreRemap );
	}

	return SCHEMA_INIT_SUCCESS();
}

const char* CEconItemDefinition::GetIconURL( const char* pszKey ) const
{
	auto idx = m_pDictIcons->Find( pszKey );
	if ( idx == m_pDictIcons->InvalidIndex() )
	{
		return NULL;
	}

	return (*m_pDictIcons)[ idx ];
}

//-----------------------------------------------------------------------------
// Purpose: Generate and return a random level according to whatever leveling
//			curve this definition uses.
//-----------------------------------------------------------------------------
uint32 CEconItemDefinition::RollItemLevel( void ) const
{
	return RandomInt( GetMinLevel(), GetMaxLevel() );
}

const char *CEconItemDefinition::GetFirstSaleDate() const
{
	return GetDefinitionString( "first_sale_date", "1960/00/00" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEconItemDefinition::IterateAttributes( IEconItemAttributeIterator *pIterator ) const
{
	FOR_EACH_VEC( GetStaticAttributes(), i )
	{
		const static_attrib_t& staticAttrib = GetStaticAttributes()[i];
		
#ifdef GC_DLL
		// we skip over static attributes that the GC will turn into dynamic attributes because otherwise we'll have
		// the appearance of iterating over them twice; for clients these attributes won't even make it into the
		// list
		if ( staticAttrib.bForceGCToGenerate )
			continue;
#endif // GC_DLL

		const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( staticAttrib.iDefIndex );
		if ( !pAttrDef )
			continue;

		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );

		if ( !pAttrType->OnIterateAttributeValue( pIterator, pAttrDef, staticAttrib.m_value ) )
			return;
	}
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Activity CEconItemDefinition::GetActivityOverride( int iTeam, Activity baseAct ) const
{
	int iAnims = GetNumAnimations( iTeam );
	for ( int i = 0; i < iAnims; i++ )
	{
		animation_on_wearable_t *pData = GetAnimationData( iTeam, i );
		if ( !pData )
			continue;
		if ( pData->iActivity == kActivityLookup_Unknown )
		{
			pData->iActivity = ActivityList_IndexForName( pData->pszActivity );
		}

		if ( pData->iActivity == baseAct )
		{
			if ( pData->iReplacement == kActivityLookup_Unknown )
			{
				pData->iReplacement = ActivityList_IndexForName( pData->pszReplacement );
			}

			if ( pData->iReplacement > 0 )
			{
				return (Activity) pData->iReplacement;
			}
		}
	}

	return baseAct;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CEconItemDefinition::GetActivityOverride( int iTeam, const char *pszActivity ) const
{
	int iAnims = GetNumAnimations( iTeam );
	for ( int i = 0; i < iAnims; i++ )
	{
		animation_on_wearable_t *pData = GetAnimationData( iTeam, i );
		if ( Q_stricmp( pszActivity, pData->pszActivity ) == 0 )
			return pData->pszReplacement;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CEconItemDefinition::GetReplacementForActivityOverride( int iTeam, Activity baseAct ) const
{
	int iAnims = GetNumAnimations( iTeam );
	for ( int i = 0; i < iAnims; i++ )
	{
		animation_on_wearable_t *pData = GetAnimationData( iTeam, i );
		if ( pData->iActivity == kActivityLookup_Unknown )
		{
			pData->iActivity = ActivityList_IndexForName( pData->pszActivity );
		}
		if ( pData && pData->iActivity == baseAct )
			return pData->pszReplacement;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the content for this item view should be streamed. If false,
//			it should be preloaded.
//-----------------------------------------------------------------------------

// DO NOT MERGE THIS CONSOLE VARIABLE TO REL WE SHOULD NOT SHIP THIS OH GOD
#ifdef STAGING_ONLY
ConVar item_enable_dynamic_loading( "item_enable_dynamic_loading", "1", FCVAR_REPLICATED, "Enable/disable dynamic streaming of econ content." );
#endif // STAGING_ONLY

bool CEconItemDefinition::IsContentStreamable() const
{
	if ( !BLoadOnDemand() )
		return false;
		
#ifdef STAGING_ONLY
	return item_enable_dynamic_loading.GetBool();
#else
	return true;
#endif
}
#endif // defined(CLIENT_DLL) || defined(GAME_DLL)

RETURN_ATTRIBUTE_STRING_F( CEconItemDefinition::GetIconDisplayModel, "icon display model", m_pszWorldDisplayModel );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTimedItemRewardDefinition::CTimedItemRewardDefinition( void )
:	m_unMinFreq( 0 ),
	m_unMaxFreq( UINT_MAX ),
	m_flChance( 0.0f ),
	m_pLootList( NULL ),
	m_iRequiredItemDef(INVALID_ITEM_DEF_INDEX)
{
}


//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CTimedItemRewardDefinition::CTimedItemRewardDefinition( const CTimedItemRewardDefinition &that )
{
	(*this) = that;
}


//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CTimedItemRewardDefinition &CTimedItemRewardDefinition::operator=( const CTimedItemRewardDefinition &rhs )
{
	m_unMinFreq = rhs.m_unMinFreq;
	m_unMaxFreq = rhs.m_unMaxFreq;
	m_flChance = rhs.m_flChance;
	m_criteria = rhs.m_criteria;
	m_pLootList = rhs.m_pLootList;
	m_iRequiredItemDef = rhs.m_iRequiredItemDef;

	return *this;
}


//-----------------------------------------------------------------------------
// Purpose:	Initialize the attribute definition
// Input:	pKVTimedReward - The KeyValues representation of the timed reward
//			schema - The overall item schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CTimedItemRewardDefinition::BInitFromKV( KeyValues *pKVTimedReward, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	// Parse the basic values
	m_flChance = pKVTimedReward->GetFloat( "pctChance" );
	m_unMinFreq = pKVTimedReward->GetInt( "value_min", 0 );
	m_unMaxFreq = pKVTimedReward->GetInt( "value_max", UINT_MAX );
	m_iRequiredItemDef = INVALID_ITEM_DEF_INDEX;

	const char *pszRequiredItem = pKVTimedReward->GetString( "required_item", NULL );
	if ( pszRequiredItem )
	{
		// Find the ItemDef
		CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pszRequiredItem );
		SCHEMA_INIT_CHECK( pDef != NULL, "Invalid Item Def Required for a for TimedReward Definition");
		m_iRequiredItemDef = pDef->GetDefinitionIndex();
	}

	// Check required fields
	SCHEMA_INIT_CHECK( 
		NULL != pKVTimedReward->FindKey( "value_min" ), 
		"Time reward %s: Missing required field \"value_min\"", pKVTimedReward->GetName() );
	SCHEMA_INIT_CHECK( 
		NULL != pKVTimedReward->FindKey( "value_max" ), 
		"Time reward %s: Missing required field \"value_max\"", pKVTimedReward->GetName() );

	SCHEMA_INIT_CHECK( 
		NULL != pKVTimedReward->FindKey( "pctChance" ), 
		"Time reward %s: Missing required field \"pctChance\"", pKVTimedReward->GetName() );

	SCHEMA_INIT_CHECK(
		NULL == pKVTimedReward->FindKey( "criteria" ),
		"Time reward %s: \"criteria\" is no longer supported. Restructure as \"loot_list\"?", pKVTimedReward->GetName() );
		
	SCHEMA_INIT_CHECK( 
		NULL != pKVTimedReward->FindKey( "loot_list" ), 
		"Time reward %s: Missing required field \"loot_list\" ", pKVTimedReward->GetName() );

	// Parse the loot list
	const char *pszLootList = pKVTimedReward->GetString("loot_list", NULL);
	if ( pszLootList && pszLootList[0] )
	{
		m_pLootList = GetItemSchema()->GetLootListByName( pszLootList );

		// Make sure the item index is correct because we use this index as a reference
		SCHEMA_INIT_CHECK( 
			NULL != m_pLootList,
			"Time Reward %s: loot_list (%s) does not exist", pKVTimedReward->GetName(), pszLootList );
	}

	// Other integrity checks
	SCHEMA_INIT_CHECK(
		m_flChance >= 0.0f,
		"Time Reward %s: pctChance (%f) must be greater or equal to 0.0", pKVTimedReward->GetName(), m_flChance );

	SCHEMA_INIT_CHECK(
		m_flChance <= 1.0f,
		"Time Reward %s: pctChance (%f) must be less than or equal to 1.0", pKVTimedReward->GetName(), m_flChance );

	SCHEMA_INIT_CHECK(
		pKVTimedReward->GetInt( "value_min" ) > 0, 
		"Time Reward %s: value_min (%d) must be greater than 0", pKVTimedReward->GetName(), m_unMinFreq );
	SCHEMA_INIT_CHECK(
		pKVTimedReward->GetInt( "value_max" ) > 0, 
		"Time Reward %s: value_max (%d) must be greater than 0", pKVTimedReward->GetName(), m_unMaxFreq );
	SCHEMA_INIT_CHECK(
		(m_unMaxFreq >= m_unMinFreq), 
		"Time Reward %s: value_max (%d) must be greater than or equal to value_min (%d)", pKVTimedReward->GetName(), m_unMaxFreq, m_unMinFreq );

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Purpose: Adds a foreign item definition to local definition mapping for a 
//			foreign app
//-----------------------------------------------------------------------------
void CForeignAppImports::AddMapping( uint16 unForeignDefIndex, const CEconItemDefinition *pDefn )
{
	m_mapDefinitions.InsertOrReplace( unForeignDefIndex, pDefn );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a foreign item definition to local definition mapping for a 
//			foreign app
//-----------------------------------------------------------------------------
const CEconItemDefinition *CForeignAppImports::FindMapping( uint16 unForeignDefIndex ) const
{
	int i = m_mapDefinitions.Find( unForeignDefIndex );
	if( m_mapDefinitions.IsValidIndex( i ) )
		return m_mapDefinitions[i];
	else
		return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:	Constructor
//-----------------------------------------------------------------------------
CEconItemSchema::CEconItemSchema( )
: 	m_unResetCount( 0 )
,	m_pKVRawDefinition( NULL )
,	m_mapItemSeries( DefLessFunc(int) )
,	m_mapRarities( DefLessFunc(int) )
,	m_mapQualities( DefLessFunc(int) )
,	m_mapAttributes( DefLessFunc(int) )
,	m_mapRecipes( DefLessFunc(int) )
,	m_mapQuestObjectives( DefLessFunc(int) )
,	m_mapItemsSorted( DefLessFunc(int) )
,	m_mapToolsItems( DefLessFunc(int) )
,	m_mapBaseItems( DefLessFunc(int) )
,	m_unVersion( 0 )
#if defined(CLIENT_DLL) || defined(GAME_DLL)
,	m_pDefaultItemDefinition( NULL )
#endif
,	m_mapItemSets( CaselessStringLessThan )
,	m_mapItemCollections( CaselessStringLessThan )
,	m_mapItemPaintKits( CaselessStringLessThan )
,	m_mapOperationDefinitions( CaselessStringLessThan )
,   m_mapLootLists( CaselessStringLessThan )
,	m_mapRevolvingLootLists( DefLessFunc(int) )
,	m_mapDefinitionPrefabs( CaselessStringLessThan )
,	m_mapAchievementRewardsByData( DefLessFunc( uint32 ) )
,	m_mapAttributeControlledParticleSystems( DefLessFunc(int) )
,	m_mapDefaultBodygroupState( CaselessStringLessThan )
#ifdef GC_DLL
,	m_mapForeignImports( DefLessFunc(AppId_t) )
#elif defined(CLIENT_DLL) || defined(GAME_DLL)
,	m_pDelayedSchemaData( NULL )
#endif
,	m_mapKillEaterScoreTypes( DefLessFunc( unsigned int ) )
,	m_mapCommunityMarketDefinitionIndexRemap( DefLessFunc( item_definition_index_t ) )
#ifdef CLIENT_DLL
,	m_mapSteamPackageLocalizationTokens( DefLessFunc( uint32 ) )
#endif
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IEconTool *CEconItemSchema::CreateEconToolImpl( const char *pszToolType, const char *pszUseString, const char *pszUsageRestriction, item_capabilities_t unCapabilities, KeyValues *pUsageKV )
{
	if ( pszToolType )
	{
		if ( !V_stricmp( pszToolType, "duel_minigame" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			if ( unCapabilities != ITEM_CAP_NONE )		return NULL;
			if ( pUsageKV )								return NULL;
				
			return new CEconTool_DuelingMinigame( pszToolType, pszUseString );
		}

		if ( !V_stricmp( pszToolType, "noise_maker" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			if ( unCapabilities != ITEM_CAP_NONE )		return NULL;
			if ( pUsageKV )								return NULL;

			return new CEconTool_Noisemaker( pszToolType, pszUseString );
		}

		if ( !V_stricmp( pszToolType, "wrapped_gift" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;

			return new CEconTool_WrappedGift( pszToolType, pszUseString, unCapabilities, pUsageKV );
		}

		if ( !V_stricmp( pszToolType, "backpack_expander" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			if ( unCapabilities != ITEM_CAP_NONE )		return NULL;

			return new CEconTool_BackpackExpander( pszToolType, pszUseString, pUsageKV );
		}

		if ( !V_stricmp( pszToolType, "account_upgrade_to_premium" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			if ( unCapabilities != ITEM_CAP_NONE )		return NULL;
			if ( pUsageKV )								return NULL;

			return new CEconTool_AccountUpgradeToPremium( pszToolType, pszUseString );
		}

		if ( !V_stricmp( pszToolType, "claimcode" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			if ( unCapabilities != ITEM_CAP_NONE )		return NULL;

			return new CEconTool_ClaimCode( pszToolType, pszUseString, pUsageKV );
		}

		if ( !V_stricmp( pszToolType, "gift" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			if ( unCapabilities != ITEM_CAP_NONE )		return NULL;

			return new CEconTool_Gift( pszToolType, pszUseString, pUsageKV );
		}

		if ( !V_stricmp( pszToolType, "paint_can" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			if ( pUsageKV )								return NULL;

			return new CEconTool_PaintCan( pszToolType, unCapabilities );
		}

		if ( !V_stricmp( pszToolType, "name" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			if ( pUsageKV )								return NULL;

			return new CEconTool_NameTag( pszToolType, unCapabilities );
		}

		if ( !V_stricmp( pszToolType, "desc" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			if ( pUsageKV )								return NULL;

			return new CEconTool_DescTag( pszToolType, unCapabilities );
		}

		if ( !V_stricmp( pszToolType, "decoder_ring" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pUsageKV )								return NULL;

			return new CEconTool_CrateKey( pszToolType, pszUsageRestriction, unCapabilities );
		}

		if ( !V_stricmp( pszToolType, "customize_texture_item" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			if ( pUsageKV )								return NULL;

			return new CEconTool_CustomizeTexture( pszToolType, unCapabilities );
		}

		if ( !V_stricmp( pszToolType, "gift_wrap" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;

			return new CEconTool_GiftWrap( pszToolType, pszUseString, unCapabilities, pUsageKV );
		}

		if ( !V_stricmp( pszToolType, "wedding_ring" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			if ( pUsageKV )								return NULL;

			return new CEconTool_WeddingRing( pszToolType, pszUseString, unCapabilities );
		}

		if ( !V_stricmp( pszToolType, "strange_part" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			
			return new CEconTool_StrangePart( pszToolType, pszUseString, unCapabilities, pUsageKV );
		}

		if ( !V_stricmp( pszToolType, "strange_part_restriction" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			if ( !pUsageKV )							return NULL;		// required
			
			return new CEconTool_StrangePartRestriction( pszToolType, pszUseString, unCapabilities, pUsageKV );
		}

		if ( !V_stricmp( pszToolType, "apply_custom_attrib" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			
			return new CEconTool_UpgradeCard( pszToolType, pszUseString, unCapabilities, pUsageKV );
		}

		if ( !V_stricmp( pszToolType, "strangifier" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;

			return new CEconTool_Strangifier( pszToolType, pszUseString, unCapabilities, pUsageKV );
		}

		if ( !V_stricmp( pszToolType, "killstreakifier" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;

			return new CEconTool_KillStreakifier( pszToolType, pszUseString, unCapabilities, pUsageKV );
		}

		if( !V_stricmp( pszToolType, "dynamic_recipe" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			
			return new CEconTool_ItemDynamicRecipe( pszToolType, pszUseString, unCapabilities, pUsageKV );
		}

		if ( !V_stricmp( pszToolType, "item_eater_recharger" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;

			return new CEconTool_ItemEaterRecharger( pszToolType, pszUseString, unCapabilities, pUsageKV );
		}

		if ( !V_stricmp( pszToolType, "class_transmogrifier" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;

			return new CEconTool_ClassTransmogrifier( pszToolType, pszUseString, unCapabilities, pUsageKV );
		}

		if ( !V_stricmp( pszToolType, "duck_token" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			if ( pUsageKV )								return NULL;

			return new CEconTool_DuckToken( pszToolType, unCapabilities );
		}

		if ( !V_stricmp( pszToolType, "grant_operation_pass" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;

			return new CEconTool_GrantOperationPass( pszToolType, pszUseString, unCapabilities, pUsageKV );
		}

		if ( !V_stricmp( pszToolType, "strange_count_transfer" ) )
		{
			// Error checking -- make sure we aren't setting properties in the schema that we don't support.
			if ( pszUsageRestriction )					return NULL;
			if ( pUsageKV )								return NULL;

			return new CEconTool_StrangeCountTransfer( pszToolType, unCapabilities );
		}

		if ( !V_stricmp( pszToolType, "paintkit_weapon_festivizer" ) )
		{
			return new CEconTool_Festivizer( pszToolType, pszUseString, unCapabilities, pUsageKV );
		}

		if ( !V_stricmp( pszToolType, "unusualifier" ) )
		{
			return new CEconTool_Unusualifier( pszToolType, pszUseString, unCapabilities, pUsageKV );
		}
	}

	// Default behavior.
	return new CEconTool_Default( pszToolType, pszUseString, pszUsageRestriction, unCapabilities );
}

#ifdef GC_DLL
random_attrib_t	*CEconItemSchema::CreateRandomAttribute( const char *pszContext, KeyValues *pRandomAttributesKV, CUtlVector<CUtlString> *pVecErrors /*= NULL*/ )
{
	// We've found the random attribute block. Parse it.
	if ( pRandomAttributesKV->FindKey( "chance" ) == NULL )
	{
		CUtlString msg;													\
		msg.Format( CFmtStr( "Missing required field \"chance\" in the \"random_attributes\" block." ) );	
		if ( pVecErrors )
		{
			pVecErrors->AddToTail( msg );
		}
		else
		{
			AssertMsg( pRandomAttributesKV->FindKey( "chance" ) != NULL, msg.String() );
		}

		return NULL;
	}

	random_attrib_t randomAttrib;

	randomAttrib.m_flChanceOfRandomAttribute = pRandomAttributesKV->GetFloat( "chance" );
	randomAttrib.m_bPickAllAttributes = ( pRandomAttributesKV->GetFloat( "pick_all_attributes" ) != 0 );
	randomAttrib.m_flTotalAttributeWeight = 0;

	FOR_EACH_TRUE_SUBKEY( pRandomAttributesKV, pKVAttribute )
	{
		const char *pszName = pKVAttribute->GetName();

		if ( !Q_strcmp( pszName, "chance" ) )
			continue;

		// Quick block list of attrs that have equal weight
		if ( !Q_strcmp( pszName, "is_even_chance_attr" ) )
		{
			FOR_EACH_VALUE( pKVAttribute, pKVListItem )
			{
				const CEconItemAttributeDefinition *pDef = GetAttributeDefinitionByName( pKVListItem->GetName() );
				if ( pDef == NULL )
				{
					CUtlString msg;													\
					msg.Format( CFmtStr( "Attribute definition \"%s\" was not found", pszName ) );	
					if ( pVecErrors )
					{
						pVecErrors->AddToTail( msg );
					}
					else
					{
						AssertMsg( pDef != NULL, msg.String() );
					}

					return NULL;
				}

				lootlist_attrib_t lootListAttrib;
				if ( !lootListAttrib.m_staticAttrib.BInitFromKV_SingleLine( __FUNCTION__, pKVListItem, pVecErrors, false ) )
				{
					if ( pVecErrors )
					{
						pVecErrors->AddToTail( __FUNCTION__ ": error initializing line-item attribute from lootlist definition (possible attr template).\n" );
					}
					return NULL;
				}
				// Weight is set to 1 for even chance attr
				lootListAttrib.m_flWeight = 1.0f;
				randomAttrib.m_flTotalAttributeWeight += 1.0f;
				randomAttrib.m_RandomAttributes.AddToTail( lootListAttrib );
			}
		}
		else
		{
			const CEconItemAttributeDefinition *pDef = GetAttributeDefinitionByName( pszName );
			if ( pDef == NULL )
			{
				CUtlString msg;													\
				msg.Format( CFmtStr( "Attribute definition \"%s\" was not found", pszName ) );	
				if ( pVecErrors )
				{
					pVecErrors->AddToTail( msg );
				}
				else
				{
					AssertMsg( pDef != NULL, msg.String() );
				}

				return NULL;
			}

			lootlist_attrib_t lootListAttrib;
			if ( !lootListAttrib.BInitFromKV( pszContext, pKVAttribute, *this, pVecErrors ) )
			{
				return NULL;
			}

			randomAttrib.m_flTotalAttributeWeight += lootListAttrib.m_flWeight;
			randomAttrib.m_RandomAttributes.AddToTail( lootListAttrib );
		}
	}

	random_attrib_t *pRandomAttr = new random_attrib_t;
	*pRandomAttr = randomAttrib;
	return pRandomAttr;
}
#endif // GC_DLL


//-----------------------------------------------------------------------------
// Purpose:	Resets the schema to before BInit was called
//-----------------------------------------------------------------------------
void CEconItemSchema::Reset( void )
{
	++m_unResetCount;

	m_unFirstValidClass = 0;
	m_unLastValidClass = 0;
	m_unAccoutClassIndex = 0;
	m_unFirstValidClassItemSlot = 0;
	m_unLastValidClassItemSlot = 0;
	m_unFirstValidAccountItemSlot = 0;
	m_unLastValidAccountItemSlot = 0;
	m_unNumItemPresets = 0;
	m_unMinLevel = 0;
	m_unMaxLevel = 0;
	m_unVersion = 0;
	m_unSumQualityWeights = 0;
	FOR_EACH_VEC( m_vecAttributeTypes, i )
	{
		delete m_vecAttributeTypes[i].m_pAttrType;
	}
	m_vecAttributeTypes.Purge();
	m_mapItems.PurgeAndDeleteElements();
	m_mapItems.Purge();
	m_mapRarities.Purge();
	m_mapQualities.Purge();
	m_mapItemsSorted.Purge();
	m_mapToolsItems.Purge();
	m_mapBaseItems.Purge();
	m_mapRecipes.PurgeAndDeleteElements();
	m_vecTimedRewards.Purge();
	m_mapItemSets.PurgeAndDeleteElements();
	m_mapLootLists.PurgeAndDeleteElements();
#ifdef GC_DLL
	m_dictRandomAttributeTemplates.PurgeAndDeleteElements();
#endif // GC_DLL
	m_mapAttributeControlledParticleSystems.Purge();
	m_vecAttributeControlledParticleSystemsCosmetics.Purge();
	m_vecAttributeControlledParticleSystemsWeapons.Purge();
	m_vecAttributeControlledParticleSystemsTaunts.Purge();

	m_mapAttributes.Purge();
	if ( m_pKVRawDefinition )
	{
		m_pKVRawDefinition->deleteThis();
		m_pKVRawDefinition = NULL;
	}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	delete m_pDefaultItemDefinition;
	m_pDefaultItemDefinition = NULL;
#endif

	FOR_EACH_MAP_FAST( m_mapRecipes, i )
	{
		delete m_mapRecipes[i];
	}

	FOR_EACH_MAP_FAST( m_mapDefinitionPrefabs, i )
	{
		m_mapDefinitionPrefabs[i]->deleteThis();
	}
	m_mapDefinitionPrefabs.Purge();

	m_vecEquipRegionsList.Purge();
	m_vecItemLevelingData.PurgeAndDeleteElements();

	m_dictStringTable.PurgeAndDeleteElements();
	m_mapCommunityMarketDefinitionIndexRemap.Purge();
}


//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CEconItemSchema &CEconItemSchema::operator=( CEconItemSchema &rhs )
{
	Reset();
	BInitSchema( rhs.m_pKVRawDefinition );
	return *this;
}

bool g_bLastSignatureCheck;

bool CheckValveSignature( const void *data, uint32 nDataSize, const void *signature, uint32 nSignatureSize )
{
	// Must match the PUBLIC KEY in src\devtools\valve_source_officialcontent.privatekey.vdf
	static const unsigned char valvePublicKey[] =
		"\x30\x81\x9D\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x01"
		"\x05\x00\x03\x81\x8B\x00\x30\x81\x87\x02\x81\x81\x00\xB1\xC0\xF1"
		"\x1C\xB2\x98\x2F\x29\x25\x95\x07\xA7\x74\xD4\x83\x43\x77\xC5\xB7"
		"\xA3\x8D\x9A\x4B\x38\x92\xB5\x98\x00\x9F\x16\xAA\x10\x95\x65\xCB"
		"\x09\xAD\x25\xDE\x0D\x3D\x1A\x08\x9C\x3C\xB6\x8E\x49\x19\x21\xCC"
		"\x14\x2F\x38\x33\x83\x20\x1D\xE9\x82\x62\xA7\x6E\xD8\xA6\xCC\x78"
		"\xBC\x51\x68\x5A\x0A\x64\xA6\x17\x2C\x67\x12\x7A\xF2\x3E\x78\x73"
		"\x1F\x4A\x82\xC2\x01\xD6\x4C\x9A\xB8\x09\x37\x32\x21\x84\xB6\x42"
		"\x72\x7F\xE1\x42\xD1\x5C\xC0\x45\xF3\x58\x3E\x19\xE3\xE3\xE1\xA9"
		"\xC5\x0C\x0F\xC8\x41\x13\x57\x3A\x52\x0A\x8F\x73\x23\x02\x01\x11";

	// Put into a global var.  Could help with VAC detection, if this
	// code gets detoured
	g_bLastSignatureCheck = CCrypto::RSAVerifySignatureSHA256(
		(const uint8 *)data, nDataSize, 
		(const uint8 *)signature, nSignatureSize,
		valvePublicKey, sizeof(valvePublicKey)
	);

	return g_bLastSignatureCheck;
}

//-----------------------------------------------------------------------------
// Initializes the schema, given KV filename
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInit( const char *fileName, const char *pathID, CUtlVector<CUtlString> *pVecErrors /* = NULL */)
{
	Reset();

	// Read the raw data
	CUtlBuffer bufRawData;
	bool bReadFileOK = g_pFullFileSystem->ReadFile( fileName, pathID, bufRawData );
	SCHEMA_INIT_CHECK( bReadFileOK, "Cannot load file '%s'", fileName );

	// Do we need to check the signature?
	#if defined(TF_DLL) || defined(TF_CLIENT_DLL)
	{

		// Load up the signature
		CUtlString sSignatureFilename( fileName ); sSignatureFilename.Append( ".sig" );
		CUtlBuffer bufSignatureBinary;
		bool bReadSignatureOK = g_pFullFileSystem->ReadFile( sSignatureFilename.String(), pathID, bufSignatureBinary );
		SCHEMA_INIT_CHECK( bReadSignatureOK, "Cannot load file '%s'", sSignatureFilename.String() );

		// Check it with the Valve public key
		bool bSignatureValid = CheckValveSignature(
			bufRawData.Base(), bufRawData.TellPut(), 
			bufSignatureBinary.Base(), bufSignatureBinary.TellPut()
		);

		// If they have a signature for a zero-byte file, that's OK, too.
		// That's the secret code that is checked into P4 internally that
		// let's us run with any items_game file
		if ( !bSignatureValid )
		{
			bSignatureValid = CheckValveSignature(
				"", 0, 
				bufSignatureBinary.Base(), bufSignatureBinary.TellPut()
			);
		}

		SCHEMA_INIT_CHECK( bSignatureValid, "'%s' is corrupt.  Please verify your local game files.  (https://support.steampowered.com/kb_article.php?ref=2037-QEUH-3335)", fileName );
	}
	#endif

	// Compute version hash
	CSHA1 sha1;
	sha1.Update( (unsigned char *)bufRawData.Base(), bufRawData.Size() );
	sha1.Final();
	sha1.GetHash( m_schemaSHA.m_shaDigest );

	// Wrap it with a text buffer reader
	CUtlBuffer bufText( bufRawData.Base(), bufRawData.TellPut(), CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );

	// Use the standard init path
	return BInitTextBuffer( bufText, pVecErrors );
}

//-----------------------------------------------------------------------------
// Initializes the schema, given KV in binary form
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitBinaryBuffer( CUtlBuffer &buffer, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	Reset();
	m_pKVRawDefinition = new KeyValues( "CEconItemSchema" );
	if ( m_pKVRawDefinition->ReadAsBinary( buffer ) )
	{
		return BInitSchema( m_pKVRawDefinition, pVecErrors )
			&& BPostSchemaInit( pVecErrors );
	}
	if ( pVecErrors )
	{
		pVecErrors->AddToTail( "Error parsing keyvalues" );
	}
	return false;
}

unsigned char g_sha1ItemSchemaText[ k_cubHash ];

//-----------------------------------------------------------------------------
// Initializes the schema, given KV in text form
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitTextBuffer( CUtlBuffer &buffer, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	// Save off the hash into a global variable, so VAC can check it
	// later
	GenerateHash( g_sha1ItemSchemaText, buffer.Base(), buffer.TellPut() );

	Reset();
	m_pKVRawDefinition = new KeyValues( "CEconItemSchema" );
	if ( m_pKVRawDefinition->LoadFromBuffer( NULL, buffer ) )
	{
		return BInitSchema( m_pKVRawDefinition, pVecErrors )
			&& BPostSchemaInit( pVecErrors );
	}
	if ( pVecErrors )
	{
		pVecErrors->AddToTail( "Error parsing keyvalues" );
	}
	return false;
}

bool CEconItemSchema::DumpItems ( const char *fileName, const char *pathID )
{
	// create a write file
	FileHandle_t f = g_pFullFileSystem->Open(fileName, "wb", pathID);

	if ( f == FILESYSTEM_INVALID_HANDLE )
	{
		DevMsg(1, "CEconItemSchema::DumpItems: couldn't open file \"%s\" in path \"%s\".\n", 
			fileName?fileName:"NULL", pathID?pathID:"NULL" );
		return false;
	}

	CUtlSortVector< KeyValues*, CUtlSortVectorKeyValuesByName > vecSortedItems;

	FOR_EACH_MAP_FAST( m_mapItems, i )
	{
		vecSortedItems.InsertNoSort( m_mapItems[ i ]->GetRawDefinition() );
	}
	vecSortedItems.RedoSort();

	CUtlBuffer buf;
	FOR_EACH_VEC( vecSortedItems, i )
	{
		vecSortedItems[i]->RecursiveSaveToFile( buf, 0, true );
	}

	int iBufSize = buf.GetBytesRemaining();
	bool bSuccess = false;
	if ( g_pFullFileSystem->Write(buf.PeekGet(), iBufSize, f) == iBufSize )
		bSuccess = true;

	g_pFullFileSystem->Close(f);

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Called once the price sheet's been loaded
//-----------------------------------------------------------------------------
#ifdef GC_DLL
GCConVar econ_orphaned_sold_items_owned_by_account_id( "econ_orphaned_sold_items_owned_by_account_id", "121416792" );

bool CEconItemSchema::DoPostPriceSheetLoadInit( CEconStorePriceSheet *pPriceSheet )
{
	FOR_EACH_MAP_FAST( m_mapItems, iItem )
	{
		CEconItemDefinition *pItemDef = m_mapItems[ iItem ];

		// Is this item being sold?
		const econ_store_entry_t *pStoreEntry = pPriceSheet->GetEntry( pItemDef->GetDefinitionIndex() );
		if ( pStoreEntry )
		{
			// Cache off whether this item is a pack item
			pItemDef->SetIsPackItem( pStoreEntry->m_bIsPackItem );

			// If an item is being sold and it has no payment rules set up, we can optionally force-create
			// a dummy payment rule that will redirect that item revenue to a "hey, these are orphan items!"
			// account.
			if ( pItemDef->GetPaymentRules().Count() == 0 && econ_orphaned_sold_items_owned_by_account_id.GetInt() )
			{
				econ_item_payment_rule_t rule;
				rule.m_RevenueShare = 1.0;
				rule.m_eRuleType = kPaymentRule_PartnerSteamID;
				rule.m_vecValues.AddToTail( econ_orphaned_sold_items_owned_by_account_id.GetInt() );

				DbgVerify( pItemDef->AddPaymentRule( rule ) == 0 );
			}
		}

		// Go through the cache of all bundles...
		FOR_EACH_VEC( m_vecBundles, iBundle )
		{
			const CEconItemDefinition *pBundleItemDef = m_vecBundles[ iBundle ];
			const bundleinfo_t *pBundle = pBundleItemDef->GetBundleInfo();
			const bool bBundleItemIsForSale = pPriceSheet->BItemExistsInPriceSheet( pBundleItemDef->GetDefinitionIndex() ) != NULL;	// Only add bundles that are actually for sale

			bool bAddToContainingBundleItemDefs = false;
			if ( pItemDef->IsPackBundle() )
			{
				// If the current item is a pack bundle, look for the first pack item in the current bundle (pBundle). We can safely assume that all pack items will be
				// in pBundle if the first pack item is, since the GC won't startup otherwise. Don't add self as a containing bundle.
				bAddToContainingBundleItemDefs = pItemDef->GetDefinitionIndex() != pBundleItemDef->GetDefinitionIndex()
											  && pBundle->vecItemDefs.HasElement( pItemDef->GetBundleInfo()->vecItemDefs[0] );
			}
			else
			{
				bAddToContainingBundleItemDefs = pBundle->vecItemDefs.HasElement( pItemDef );
			}

			// Does the current bundle contain the given item?
			if ( bBundleItemIsForSale && bAddToContainingBundleItemDefs )
			{
				pItemDef->m_vecContainingBundleItemDefs.AddToTail( pBundleItemDef );
			}
		}
	}

	return true;
}
#endif


#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Set up the buffer to use to reinitialize our schema next time we can do so safely.
//-----------------------------------------------------------------------------
bool CEconItemSchema::MaybeInitFromBuffer( IDelayedSchemaData *pDelayedSchemaData )
{
	bool bDidInit = false;

	// Use whatever our most current data block is.
	if ( m_pDelayedSchemaData )
	{
		delete m_pDelayedSchemaData;
	}

	m_pDelayedSchemaData = pDelayedSchemaData;

#ifdef CLIENT_DLL
	// If we aren't in a game we can parse immediately now.
	if ( !engine->IsInGame() )
	{
		BInitFromDelayedBuffer();
		bDidInit = true;
	}
#endif // CLIENT_DLL

	return bDidInit;
}

//-----------------------------------------------------------------------------
// We're in a safe place to change the contents of the schema, so do so and clean
// up whatever memory we were using.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitFromDelayedBuffer()
{
	if ( !m_pDelayedSchemaData )
		return true;

	bool bSuccess = m_pDelayedSchemaData->InitializeSchema( this );
	delete m_pDelayedSchemaData;
	m_pDelayedSchemaData = NULL;

	return bSuccess;
}
#endif // !GC_DLL

static void CalculateKeyValuesCRCRecursive( KeyValues *pKV, CRC32_t *crc, bool bIgnoreName = false )
{
	// Hash in the key name in LOWERCASE.  Keyvalues files are not deterministic due
	// to the case insensitivity of the keys and the dependence on the existing
	// state of the name table upon entry.
	if ( !bIgnoreName )
	{
		const char *s = pKV->GetName();  
		for (;;)
		{
			unsigned char x = tolower(*s);
			CRC32_ProcessBuffer( crc, &x, 1 ); // !SPEED! This is slow, but it works.
			if (*s == '\0') break;
			++s;
		}
	}

	// Now hash in value, depending on type
	// !FIXME! This is not byte-order independent!
	switch ( pKV->GetDataType() )
	{
	case KeyValues::TYPE_NONE:
		{
			FOR_EACH_SUBKEY( pKV, pChild )
			{
				CalculateKeyValuesCRCRecursive( pChild, crc );
			}
			break;
		}
	case KeyValues::TYPE_STRING:
		{
			const char *val = pKV->GetString();
			CRC32_ProcessBuffer( crc, val, strlen(val)+1 );
			break;
		}

	case KeyValues::TYPE_INT:
		{
			int val = pKV->GetInt();
			CRC32_ProcessBuffer( crc, &val, sizeof(val) );
			break;
		}

	case KeyValues::TYPE_UINT64:
		{
			uint64 val = pKV->GetUint64();
			CRC32_ProcessBuffer( crc, &val, sizeof(val) );
			break;
		}

	case KeyValues::TYPE_FLOAT:
		{
			float val = pKV->GetFloat();
			CRC32_ProcessBuffer( crc, &val, sizeof(val) );
			break;
		}
	case KeyValues::TYPE_COLOR:
		{
			int val = pKV->GetColor().GetRawColor();
			CRC32_ProcessBuffer( crc, &val, sizeof(val) );
			break;
		}

	default:
	case KeyValues::TYPE_PTR:
	case KeyValues::TYPE_WSTRING:
		{
			Assert( !"Unsupport data type!" );
			break;
		}
	}
}

uint32 CEconItemSchema::CalculateKeyValuesVersion( KeyValues *pKV )
{
	CRC32_t crc;
	CRC32_Init( &crc );

	// Calc CRC recursively.  Ignore the very top-most
	// key name, which isn't set consistently
	CalculateKeyValuesCRCRecursive( pKV, &crc, true );
	CRC32_Final( &crc );
	return crc;
}

EEquipType_t CEconItemSchema::GetEquipTypeFromClassIndex( int iClass ) const
{
	if ( iClass == GetAccountIndex() )
		return EEquipType_t::EQUIP_TYPE_ACCOUNT;

	if ( iClass >= GetFirstValidClass() && iClass <= GetLastValidClass() )
		return EEquipType_t::EQUIP_TYPE_CLASS;

	return EEquipType_t::EQUIP_TYPE_INVALID;
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the schema
// Input:	pKVRawDefinition - The raw KeyValues representation of the schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitSchema( KeyValues *pKVRawDefinition, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	m_unMinLevel = pKVRawDefinition->GetInt( "item_level_min", 0 );
	m_unMaxLevel = pKVRawDefinition->GetInt( "item_level_max", 0 );

#if !defined( GC_DLL )
	m_unVersion = CalculateKeyValuesVersion( pKVRawDefinition );
#endif



#ifdef GC_DLL
	// Validate the integrity of the base data.
	SCHEMA_INIT_CHECK( 0 <= m_unMinLevel, "Minimum Item Level must be at least 0" );
	SCHEMA_INIT_CHECK( m_unMinLevel <= m_unMaxLevel, "Minimum Item Level must be less than or equal to Maximum Item Level" );
#endif // GC_DLL

	// Parse the prefabs block first so the prefabs will be populated in case anything else wants
	// to use them later.
	KeyValues *pKVPrefabs = pKVRawDefinition->FindKey( "prefabs" );
	if ( NULL != pKVPrefabs )
	{
		SCHEMA_INIT_SUBSTEP( BInitDefinitionPrefabs( pKVPrefabs, pVecErrors ) );
	}

	// Initialize the game info block
	KeyValues *pKVGameInfo = pKVRawDefinition->FindKey( "game_info" );
	SCHEMA_INIT_CHECK( NULL != pKVGameInfo, "Required key \"game_info\" missing.\n" );

	if ( NULL != pKVGameInfo )
	{
		SCHEMA_INIT_SUBSTEP( BInitGameInfo( pKVGameInfo, pVecErrors ) );
	}

	// Initialize our attribute types. We don't actually pull this data from the schema right now but it
	// still makes sense to initialize it at this point.
	SCHEMA_INIT_SUBSTEP( BInitAttributeTypes( pVecErrors ) );

	// Initialize the item series block
	KeyValues *pKVItemSeries = pKVRawDefinition->FindKey( "item_series_types" );
	SCHEMA_INIT_CHECK( NULL != pKVItemSeries, "Required key \"item_series_types\" missing.\n" );
	if ( NULL != pKVItemSeries )
	{
		SCHEMA_INIT_SUBSTEP( BInitItemSeries( pKVItemSeries, pVecErrors ) );
	}

	// Initialize the rarity block
	KeyValues *pKVRarities = pKVRawDefinition->FindKey( "rarities" );
	KeyValues *pKVRarityWeights = pKVRawDefinition->FindKey( "rarities_lootlist_weights" );
	SCHEMA_INIT_CHECK( NULL != pKVRarities, "Required key \"rarities\" missing.\n" );
	if ( NULL != pKVRarities )
	{
		SCHEMA_INIT_SUBSTEP( BInitRarities( pKVRarities, pKVRarityWeights, pVecErrors ) );
	}

	// Initialize the qualities block
	KeyValues *pKVQualities = pKVRawDefinition->FindKey( "qualities" );
	SCHEMA_INIT_CHECK( NULL != pKVQualities, "Required key \"qualities\" missing.\n" );

	if ( NULL != pKVQualities )
	{
		SCHEMA_INIT_SUBSTEP( BInitQualities( pKVQualities, pVecErrors ) );
	}

	// Initialize the colors block
	KeyValues *pKVColors = pKVRawDefinition->FindKey( "colors" );
	SCHEMA_INIT_CHECK( NULL != pKVColors, "Required key \"colors\" missing.\n" );

	if ( NULL != pKVColors )
	{
		SCHEMA_INIT_SUBSTEP( BInitColors( pKVColors, pVecErrors ) );
	}

	// Initialize the attributes block
	KeyValues *pKVAttributes = pKVRawDefinition->FindKey( "attributes" );
	SCHEMA_INIT_CHECK( NULL != pKVAttributes, "Required key \"attributes\" missing.\n" );

	if ( NULL != pKVAttributes )
	{
		SCHEMA_INIT_SUBSTEP( BInitAttributes( pKVAttributes, pVecErrors ) );
	}

#ifdef GC
	// Initialize the motd block
	KeyValues *pKVMOTD = pKVRawDefinition->FindKey( "motd_entries" );
	SCHEMA_INIT_CHECK( NULL != pKVMOTD, "Required key \"motd_entries\" missing.\n" );

	if ( NULL != pKVMOTD )
	{
		SCHEMA_INIT_SUBSTEP( GGCGameBase()->GetMOTDManager().BInitMOTDEntries( pKVMOTD, pVecErrors ) );
	}
#endif 

	// Initialize the "equip_regions_list" block -- this is an optional block
	KeyValues *pKVEquipRegions = pKVRawDefinition->FindKey( "equip_regions_list" );
	if ( NULL != pKVEquipRegions )
	{
		SCHEMA_INIT_SUBSTEP( BInitEquipRegions( pKVEquipRegions, pVecErrors ) );
	}

	// Initialize the "equip_conflicts" block -- this is an optional block, though it doesn't
	// make any sense and will probably fail internally if there is no corresponding "equip_regions"
	// block as well
	KeyValues *pKVEquipRegionConflicts = pKVRawDefinition->FindKey( "equip_conflicts" );
	if ( NULL != pKVEquipRegionConflicts )
	{
		SCHEMA_INIT_SUBSTEP( BInitEquipRegionConflicts( pKVEquipRegionConflicts, pVecErrors ) );
	}

	// TF2 Paint Kits
	// No included in schema file (Too Big).  Loaded Seperately
	// Load the KV and add it to the pKVRawDefinition
	KeyValues *pPaintkitKV = new KeyValues( "item_paintkit_definitions" );
	SCHEMA_INIT_CHECK( pPaintkitKV->LoadFromFile( g_pFullFileSystem, "scripts/items/paintkits_master.txt", "GAME" ), "Unable to Load paintkits_master.txt KV File!" );
	pKVRawDefinition->AddSubKey( pPaintkitKV );

	// Init Item Paint Kits
	// Must be BEFORE Item defs
	KeyValues *pKVItemPaintKits = pKVRawDefinition->FindKey( "item_paintkit_definitions" );
	if ( NULL != pKVItemPaintKits )
	{
		SCHEMA_INIT_SUBSTEP( BInitItemPaintKitDefinitions( pKVItemPaintKits, pVecErrors ) );
	}

#ifdef GC_DLL
	// Parse the loot lists block (on the GC)
	// Must be BEFORE Item defs
	KeyValues *pKVRandomAttributeTemplates = pKVRawDefinition->FindKey( "random_attribute_templates" );
	SCHEMA_INIT_SUBSTEP( BInitRandomAttributeTemplates( pKVRandomAttributeTemplates, pVecErrors ) );
#endif // GC_DLL

	// Initialize the items block
	KeyValues *pKVItems = pKVRawDefinition->FindKey( "items" );
	SCHEMA_INIT_CHECK( NULL != pKVItems, "Required key \"items\" missing.\n" );

	if ( NULL != pKVItems )
	{
		SCHEMA_INIT_SUBSTEP( BInitItems( pKVItems, pVecErrors ) );
	}


	// Verify base item names are proper in item schema
	SCHEMA_INIT_SUBSTEP( BVerifyBaseItemNames( pVecErrors ) );

	// Parse the item_sets block.
	KeyValues *pKVItemSets = pKVRawDefinition->FindKey( "item_sets" );
	SCHEMA_INIT_SUBSTEP( BInitItemSets( pKVItemSets, pVecErrors ) );
	
	// Particles
	KeyValues *pKVParticleSystems = pKVRawDefinition->FindKey( "attribute_controlled_attached_particles" );
	SCHEMA_INIT_SUBSTEP( BInitAttributeControlledParticleSystems( pKVParticleSystems, pVecErrors ) );

	// Parse any recipes block
	KeyValues *pKVRecipes = pKVRawDefinition->FindKey( "recipes" );
	SCHEMA_INIT_SUBSTEP( BInitRecipes( pKVRecipes, pVecErrors ) );

	// Reset our loot lists.
	m_mapLootLists.RemoveAll();

	// Init Item Collections - Must be before lootlists since collections are lootlists themselves and are referenced by lootlists
	KeyValues *pKVItemCollections = pKVRawDefinition->FindKey( "item_collections" );
	if ( NULL != pKVItemCollections )
	{
		SCHEMA_INIT_SUBSTEP( BInitItemCollections( pKVItemCollections, pVecErrors ) );
	}

#ifdef GC_DLL
	// Parse the loot lists block (on the GC)
	KeyValues *pKVLootLists = pKVRawDefinition->FindKey( "loot_lists" );
	SCHEMA_INIT_SUBSTEP( BInitLootLists( pKVLootLists, pVecErrors ) );

	// Initialize the periodic score accumulation block (this needs to take place after items)
	KeyValues *pKVPeriodicScoring = pKVRawDefinition->FindKey( "periodic_score_accumulation" );
	if ( NULL != pKVPeriodicScoring )
	{
		SCHEMA_INIT_SUBSTEP( BInitPeriodicScoring( pKVPeriodicScoring, pVecErrors ) );
	}
#endif // GC_DLL

	// Parse the client loot lists block (everywhere)
	KeyValues *pKVClientLootLists = pKVRawDefinition->FindKey( "client_loot_lists" );
	SCHEMA_INIT_SUBSTEP( BInitLootLists( pKVClientLootLists, pVecErrors ) );

	// Parse the revolving loot lists block
	KeyValues *pKVRevolvingLootLists = pKVRawDefinition->FindKey( "revolving_loot_lists" );
	SCHEMA_INIT_SUBSTEP( BInitRevolvingLootLists( pKVRevolvingLootLists, pVecErrors ) );

	// Init Items that may reference Collections
	SCHEMA_INIT_SUBSTEP( BInitCollectionReferences( pVecErrors ) );

	// Validate Operation Pass	
	KeyValues *pKVOperationDefinitions = pKVRawDefinition->FindKey( "operations" );
	if ( NULL != pKVOperationDefinitions )
	{
		SCHEMA_INIT_SUBSTEP( BInitOperationDefinitions( pKVGameInfo, pKVOperationDefinitions, pVecErrors ) );
	}

#if defined( GC_DLL )
	// Parse any time-based rewards
	KeyValues *pKVTimeRewards = pKVRawDefinition->FindKey( "time_rewards" );
	SCHEMA_INIT_SUBSTEP( BInitTimedRewards( pKVTimeRewards, pVecErrors ) );

	KeyValues *pKVExperiments = pKVRawDefinition->FindKey( "experiments" );
	SCHEMA_INIT_SUBSTEP( BInitExperiements( pKVExperiments, pVecErrors ) );

	SCHEMA_INIT_SUBSTEP( BInitForeignImports( pVecErrors ) );
#elif defined( CLIENT_DLL ) || defined( GAME_DLL )
	KeyValues *pKVArmoryData = pKVRawDefinition->FindKey( "armory_data" );
	SCHEMA_INIT_SUBSTEP( BInitArmoryData( pKVArmoryData, pVecErrors ) );
#endif // GC_DLL

	// Parse any achievement rewards
	KeyValues *pKVAchievementRewards = pKVRawDefinition->FindKey( "achievement_rewards" );
	SCHEMA_INIT_SUBSTEP( BInitAchievementRewards( pKVAchievementRewards, pVecErrors ) );

#ifdef TF_CLIENT_DLL
	// Compute the number of concrete items, for each item, and cache for quick access
	SCHEMA_INIT_SUBSTEP( BInitConcreteItemCounts( pVecErrors ) );

	// We don't have access to Steam's full library of app data on the client so initialize whichever packages
	// we want to reference.
	KeyValues *pKVSteamPackages = pKVRawDefinition->FindKey( "steam_packages" );
	SCHEMA_INIT_SUBSTEP( BInitSteamPackageLocalizationToken( pKVSteamPackages, pVecErrors ) );
#endif // TF_CLIENT_DLL

	// Parse the item levels block
	KeyValues *pKVItemLevels = pKVRawDefinition->FindKey( "item_levels" );
	SCHEMA_INIT_SUBSTEP( BInitItemLevels( pKVItemLevels, pVecErrors ) );

	// Parse the kill eater score types
	KeyValues *pKVKillEaterScoreTypes = pKVRawDefinition->FindKey( "kill_eater_score_types" );
	SCHEMA_INIT_SUBSTEP( BInitKillEaterScoreTypes( pKVKillEaterScoreTypes, pVecErrors ) );

	// Initialize the string tables, if present
	KeyValues *pKVStringTables = pKVRawDefinition->FindKey( "string_lookups" );
	SCHEMA_INIT_SUBSTEP( BInitStringTables( pKVStringTables, pVecErrors ) );

	// Initialize the community Market remaps, if present
	KeyValues *pKVCommunityMarketRemaps = pKVRawDefinition->FindKey( "community_market_item_remaps" );
	SCHEMA_INIT_SUBSTEP( BInitCommunityMarketRemaps( pKVCommunityMarketRemaps, pVecErrors ) );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the "game_info" section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitGameInfo( KeyValues *pKVGameInfo, CUtlVector<CUtlString> *pVecErrors )
{
	m_unFirstValidClass = pKVGameInfo->GetInt( "first_valid_class", 0 );
	m_unLastValidClass = pKVGameInfo->GetInt( "last_valid_class", 0 );
	SCHEMA_INIT_CHECK( 0 < m_unFirstValidClass, "First valid class must be greater than 0." );
	SCHEMA_INIT_CHECK( m_unFirstValidClass <= m_unLastValidClass, "First valid class must be less than or equal to last valid class." );
	m_unAccoutClassIndex = pKVGameInfo->GetInt( "account_class_index", 0 );
	SCHEMA_INIT_CHECK( m_unAccoutClassIndex > m_unLastValidClass, "Account class index must be greater than 'last_valid_class'" );

	m_unFirstValidClassItemSlot = pKVGameInfo->GetInt( "first_valid_item_slot", INVALID_EQUIPPED_SLOT );
	m_unLastValidClassItemSlot = pKVGameInfo->GetInt( "last_valid_item_slot", INVALID_EQUIPPED_SLOT );
	SCHEMA_INIT_CHECK( INVALID_EQUIPPED_SLOT != m_unFirstValidClassItemSlot, "first_valid_item_slot not set!" );
	SCHEMA_INIT_CHECK( INVALID_EQUIPPED_SLOT != m_unFirstValidClassItemSlot, "last_valid_item_slot not set!" );
	SCHEMA_INIT_CHECK( m_unFirstValidClassItemSlot <= m_unLastValidClassItemSlot, "First valid item slot must be less than or equal to last valid item slot." );

	m_unFirstValidAccountItemSlot = pKVGameInfo->GetInt( "account_first_valid_item_slot", INVALID_EQUIPPED_SLOT );
	m_unLastValidAccountItemSlot  = pKVGameInfo->GetInt( "account_last_valid_item_slot", INVALID_EQUIPPED_SLOT );
	SCHEMA_INIT_CHECK( INVALID_EQUIPPED_SLOT != m_unFirstValidAccountItemSlot, "account_first_valid_item_slot not set!" );
	SCHEMA_INIT_CHECK( INVALID_EQUIPPED_SLOT != m_unLastValidAccountItemSlot, "account_last_valid_item_slot not set!" );
	SCHEMA_INIT_CHECK( m_unFirstValidAccountItemSlot <= m_unLastValidAccountItemSlot, "First vlid account item slot must be less than or equal to the last valid account item slot." );

	m_unNumItemPresets = pKVGameInfo->GetInt( "num_item_presets", -1 );
	SCHEMA_INIT_CHECK( (uint32)-1 != m_unNumItemPresets, "num_item_presets not set!" );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitAttributeTypes( CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_VEC( m_vecAttributeTypes, i )
	{
		delete m_vecAttributeTypes[i].m_pAttrType;
	}
	m_vecAttributeTypes.Purge();

	m_vecAttributeTypes.AddToTail( attr_type_t( NULL,										new CSchemaAttributeType_Default ) );
	m_vecAttributeTypes.AddToTail( attr_type_t( "float",									new CSchemaAttributeType_Float ) );
	m_vecAttributeTypes.AddToTail( attr_type_t( "uint64",									new CSchemaAttributeType_UInt64 ) );
	m_vecAttributeTypes.AddToTail( attr_type_t( "string",									new CSchemaAttributeType_String ) );
	m_vecAttributeTypes.AddToTail( attr_type_t( "dynamic_recipe_component_defined_item",	new CSchemaAttributeType_DynamicRecipeComponentDefinedItem ) );
	m_vecAttributeTypes.AddToTail( attr_type_t( "item_slot_criteria",						new CSchemaAttributeType_ItemSlotCriteria ) );
	m_vecAttributeTypes.AddToTail( attr_type_t( "item_placement",							new CSchemaAttributeType_WorldItemPlacement ) );

	// Make sure that all attribute types specified have the item ID in the 0th column. We use this
	// when loading items to map between item IDs and the attributes they own.
	FOR_EACH_VEC( m_vecAttributeTypes, i )
	{
#ifdef GC_DLL
		const CColumnSet& cs = m_vecAttributeTypes[i].m_pAttrType->GetFullColumnSet();

		SCHEMA_INIT_CHECK( cs.GetColumnCount() >= 2, "BInitAttributeTypes(): '%s' has invalid column count.\n", cs.GetRecordInfo()->GetName() );

		const CColumnInfo& Column0 = cs.GetColumnInfo( 0 );
		SCHEMA_INIT_CHECK( Column0.GetType() == k_EGCSQLType_int64, "BInitAttributeTypes(): '%s' column 0 has invalid data type %u.\n", cs.GetRecordInfo()->GetName(), Column0.GetType() );
		SCHEMA_INIT_CHECK( Column0.GetName() && !V_stricmp( Column0.GetName(), "ItemID" ), "BInitAttributeTypes(): '%s' has invalid name '%s'.\n", cs.GetRecordInfo()->GetName(), Column0.GetName() ? Column0.GetName() : "[null]" );
		SCHEMA_INIT_CHECK( Column0.BIsPrimaryKey(), "BInitAttributeTypes(): '%s' has an item ID column that isn't in the PK.\n", cs.GetRecordInfo()->GetName() );

		const CColumnInfo& Column1 = cs.GetColumnInfo( 1 );
		SCHEMA_INIT_CHECK( Column1.GetType() == k_EGCSQLType_int16, "BInitAttributeTypes(): '%s' column 1 has invalid data type %u.\n", cs.GetRecordInfo()->GetName(), Column0.GetType() );
		SCHEMA_INIT_CHECK( Column1.GetName() && !V_stricmp( Column1.GetName(), "AttrDefIndex" ), "BInitAttributeTypes(): '%s' has invalid name '%s'.\n", cs.GetRecordInfo()->GetName(), Column1.GetName() ? Column1.GetName() : "[null]" );

		// Make sure two different attribute types don't point to the same DB table. There's nothing
		// technically that would prevent this from working, but right now the way we load from the
		// DB would make this super-inefficient so we'd want to fix that code if we are rolling content
		// that would hit this error.
		for ( int j = i + 1; j < m_vecAttributeTypes.Count(); j++ )
		{
			SCHEMA_INIT_CHECK( cs.GetRecordInfo() != m_vecAttributeTypes[j].m_pAttrType->GetFullColumnSet().GetRecordInfo(),
				"BInitAttributeTypes(): multiple attribute types reference the same table '%s'.\n", cs.GetRecordInfo()->GetName() );
		}
#endif // GC_DLL
	}

	return SCHEMA_INIT_SUCCESS();
}

#ifdef GC_DLL
//-----------------------------------------------------------------------------
// Purpose:	Initializes the "periodic_score_accumulation" section of the schema
//-----------------------------------------------------------------------------
struct periodic_score_event_lookup_entry_t { const char *m_pszName; eEconPeriodicScoreEvents m_eValue; bool m_bGCUpdateOnly; };
static const periodic_score_event_lookup_entry_t sPeriodicScoreEvents[] =
{
	{ "gifts_distributed",				kPeriodicScoreEvent_GiftsDistributed,		true },
	{ "duels_won",						kPeriodicScoreEvent_DuelsWon,				true },
	{ "map_stamps_purchased",			kPeriodicScoreEvent_MapStampsPurchased,		true },
};

struct periodic_score_duration_lookup_entry_t { const char *m_pszName; uint32 m_unValue; };
static const periodic_score_duration_lookup_entry_t sPeriodicScoreDurations[] =
{
	{ "disabled",						0 },
	{ "hourly",							60 * 60 },
	{ "daily",							60 * 60 * 24 },
	{ "weekly",							60 * 60 * 24 * 7 },
	{ "monthly",						60 * 60 * 24 * 7 * 4 },			// four weeks, not necessarily a month boundary
};

template < typename search_entry_type, int search_entry_array_size >
static bool LookupValueFromString( const search_entry_type(&searchArray)[search_entry_array_size], const char *pszSearch, search_entry_type *out_pResult )
{
	Assert( out_pResult );

	for ( int i = 0; i < search_entry_array_size; i++ )
	{
		if ( !V_stricmp( pszSearch, searchArray[i].m_pszName ) )
		{
			*out_pResult = searchArray[i];
			return true;
		}
	}

	return false;
}	

bool CEconItemSchema::BInitPeriodicScoring( KeyValues *pKVPeriodicScoring, CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_TRUE_SUBKEY( pKVPeriodicScoring, pKVScoreType )
	{
		int index = Q_atoi( pKVScoreType->GetName() );
		SCHEMA_INIT_CHECK( index == m_vecPeriodicScoreTypes.Count(), "Invalid or out-of-order periodic score type '%s'", pKVScoreType->GetName() );

		periodic_score_t PeriodicScore;

		// Reward item definition.
		const char *pszRewardItemDefName = pKVScoreType->GetString( "reward_item_def_name", NULL );
		PeriodicScore.m_pRewardItemDefinition = pszRewardItemDefName ? GetItemDefinitionByName( pszRewardItemDefName ) : NULL;
		SCHEMA_INIT_CHECK( PeriodicScore.m_pRewardItemDefinition, "Periodic score type '%s' missing reward item definition name", pKVScoreType->GetName() );

		// Event type via string lookup.
		const char *pszEventName = pKVScoreType->GetString( "event", "" );
		{
			periodic_score_event_lookup_entry_t EventEntry;
			SCHEMA_INIT_CHECK( LookupValueFromString( sPeriodicScoreEvents, pszEventName, &EventEntry ),
							   "Periodic score type '%s' could not find event name '%s'", pKVScoreType->GetName(), pszEventName );
			PeriodicScore.m_eEventType = EventEntry.m_eValue;

			// Note: other parts of the code assume that the event type is associated with the GC-only updatability flag.)
			PeriodicScore.m_bGCUpdateOnly = EventEntry.m_bGCUpdateOnly;
		}

		// Time period via string lookup.
		{
			const char *pszTimePeriodName = pKVScoreType->GetString( "time_period", "" );
			periodic_score_duration_lookup_entry_t DurationEntry;
			SCHEMA_INIT_CHECK( LookupValueFromString( sPeriodicScoreDurations, pszTimePeriodName, &DurationEntry ),
							   "Periodic score type '%s' could not find time period name '%s'", pKVScoreType->GetName(), pszEventName );
			PeriodicScore.m_unTimePeriodLengthInSeconds = DurationEntry.m_unValue;
		}

		// Alternate time period specified for use in internal Steam?
		if ( GGCHost()->GetUniverse() != k_EUniversePublic )
		{
			const char *pszInternalTimePeriodName = pKVScoreType->GetString( "time_period_internal", NULL );
			if ( pszInternalTimePeriodName )
			{
				periodic_score_duration_lookup_entry_t InternalDurationEntry;
				SCHEMA_INIT_CHECK( LookupValueFromString( sPeriodicScoreDurations, pszInternalTimePeriodName, &InternalDurationEntry ),
								   "Periodic score type '%s' could not find internal time period name '%s'", pKVScoreType->GetName(), pszEventName );
				PeriodicScore.m_unTimePeriodLengthInSeconds = InternalDurationEntry.m_unValue;
			}
		}

		m_vecPeriodicScoreTypes.AddToTail( PeriodicScore );
	}

	return SCHEMA_INIT_SUCCESS();
}
#endif // GC_DLL

//-----------------------------------------------------------------------------
// Purpose:	Initializes the "prefabs" section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitDefinitionPrefabs( KeyValues *pKVPrefabs, CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_TRUE_SUBKEY( pKVPrefabs, pKVPrefab )
	{
		const char *pszPrefabName = pKVPrefab->GetName();

		int nMapIndex = m_mapDefinitionPrefabs.Find( pszPrefabName );

		// Make sure the item index is correct because we use this index as a reference
		SCHEMA_INIT_CHECK( 
			!m_mapDefinitionPrefabs.IsValidIndex( nMapIndex ),
			"Duplicate prefab name (%s)", pszPrefabName );

		m_mapDefinitionPrefabs.Insert( pszPrefabName, pKVPrefab->MakeCopy() );
	}

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Purpose:	Initializes the Item Series section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitItemSeries( KeyValues *pKVSeries, CUtlVector<CUtlString> *pVecErrors )
{
	// initialize the item definitions
	if ( NULL != pKVSeries)
	{
		FOR_EACH_TRUE_SUBKEY( pKVSeries, pKVItem )
		{
			int nSeriesIndex = pKVItem->GetInt( "value" );
			int nMapIndex = m_mapItemSeries.Find( nSeriesIndex );

			// Make sure the item index is correct because we use this index as a reference
			SCHEMA_INIT_CHECK(
				!m_mapItemSeries.IsValidIndex( nMapIndex ),
				"Duplicate item series value (%d)", nSeriesIndex );

			nMapIndex = m_mapItemSeries.Insert( nMapIndex );
			SCHEMA_INIT_SUBSTEP( m_mapItemSeries[nMapIndex].BInitFromKV( pKVItem, pVecErrors ) );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the rarity section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitRarities( KeyValues *pKVRarities, KeyValues *pKVRarityWeights, CUtlVector<CUtlString> *pVecErrors )
{
	// initialize the item definitions
	if ( NULL != pKVRarities )
	{
		FOR_EACH_TRUE_SUBKEY( pKVRarities, pKVRarity )
		{
			int nRarityIndex = pKVRarity->GetInt( "value" );
			int nMapIndex = m_mapRarities.Find( nRarityIndex );

			// Make sure the item index is correct because we use this index as a reference
			SCHEMA_INIT_CHECK(
				!m_mapRarities.IsValidIndex( nMapIndex ),
				"Duplicate rarity value (%d)", nRarityIndex );

			nMapIndex = m_mapRarities.Insert( nRarityIndex );
			SCHEMA_INIT_SUBSTEP( m_mapRarities[nMapIndex].BInitFromKV( pKVRarity, pKVRarityWeights, *this, pVecErrors ) );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the qualities section of the schema
// Input:	pKVQualities - The qualities section of the KeyValues 
//				representation of the schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitQualities( KeyValues *pKVQualities, CUtlVector<CUtlString> *pVecErrors )
{
	// initialize the item definitions
	if ( NULL != pKVQualities )
	{
		FOR_EACH_TRUE_SUBKEY( pKVQualities, pKVQuality )
		{
			int nQualityIndex = pKVQuality->GetInt( "value" );
			int nMapIndex = m_mapQualities.Find( nQualityIndex );

			// Make sure the item index is correct because we use this index as a reference
			SCHEMA_INIT_CHECK( 
				!m_mapQualities.IsValidIndex( nMapIndex ),
				"Duplicate quality value (%d)", nQualityIndex );

			nMapIndex = m_mapQualities.Insert( nQualityIndex );
			SCHEMA_INIT_SUBSTEP( m_mapQualities[nMapIndex].BInitFromKV( pKVQuality, pVecErrors ) );
		}
	}

	// Check the integrity of the quality definitions

	// Check for duplicate quality names
	CUtlRBTree<const char *> rbQualityNames( CaselessStringLessThan );
	rbQualityNames.EnsureCapacity( m_mapQualities.Count() );
	FOR_EACH_MAP_FAST( m_mapQualities, i )
	{
		int iIndex = rbQualityNames.Find( m_mapQualities[i].GetName() );
		SCHEMA_INIT_CHECK( 
			!rbQualityNames.IsValidIndex( iIndex ),
			"Quality definition %d: Duplicate quality name %s", m_mapQualities[i].GetDBValue(), m_mapQualities[i].GetName() );

		if( !rbQualityNames.IsValidIndex( iIndex ) )
			rbQualityNames.Insert( m_mapQualities[i].GetName() );
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitColors( KeyValues *pKVColors, CUtlVector<CUtlString> *pVecErrors )
{
	// initialize the color definitions
	if ( NULL != pKVColors )
	{
		FOR_EACH_TRUE_SUBKEY( pKVColors, pKVColor )
		{
			CEconColorDefinition *pNewColorDef = new CEconColorDefinition;

			SCHEMA_INIT_SUBSTEP( pNewColorDef->BInitFromKV( pKVColor, pVecErrors ) );
			m_vecColorDefs.AddToTail( pNewColorDef );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CEconItemSchema::GetEquipRegionIndexByName( const char *pRegionName ) const
{
	FOR_EACH_VEC( m_vecEquipRegionsList, i )
	{
		const char *szEntryRegionName = m_vecEquipRegionsList[i].m_sName.Get();
		if ( !V_stricmp( szEntryRegionName, pRegionName ) )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
equip_region_mask_t CEconItemSchema::GetEquipRegionBitMaskByName( const char *pRegionName ) const
{
	int iRegionIndex = GetEquipRegionIndexByName( pRegionName );
	if ( !m_vecEquipRegionsList.IsValidIndex( iRegionIndex ) )
		return 0;

	equip_region_mask_t unRegionMask = 1 << m_vecEquipRegionsList[iRegionIndex].m_unBitIndex;
	Assert( unRegionMask > 0 );

	return unRegionMask;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CEconItemSchema::SetEquipRegionConflict( int iRegion, unsigned int unBit )
{
	Assert( m_vecEquipRegionsList.IsValidIndex( iRegion ) );

	equip_region_mask_t unRegionMask = 1 << unBit;
	Assert( unRegionMask > 0 );

	m_vecEquipRegionsList[iRegion].m_unMask |= unRegionMask;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
equip_region_mask_t CEconItemSchema::GetEquipRegionMaskByName( const char *pRegionName ) const
{
	int iRegionIdx = GetEquipRegionIndexByName( pRegionName );
	if ( iRegionIdx < 0 )
		return 0;

	return m_vecEquipRegionsList[iRegionIdx].m_unMask;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CEconItemSchema::AssignDefaultBodygroupState( const char *pszBodygroupName, int iValue )
{
	// Flip the value passed in -- if we specify in the schema that a region should be off, we assume that it's
	// on by default.
	// actually the schemas are all authored assuming that the default is 0, so let's use that
	int iDefaultValue = 0; //iValue == 0 ? 1 : 0;

	// Make sure that we're constantly reinitializing our default value to the same default value. This is sort
	// of dumb but it works for everything we've got now. In the event that conflicts start cropping up it would
	// be easy enough to make a new schema section.
	int iIndex = m_mapDefaultBodygroupState.Find( pszBodygroupName );
	if ( (m_mapDefaultBodygroupState.IsValidIndex( iIndex ) && m_mapDefaultBodygroupState[iIndex] != iDefaultValue) ||
		 (iValue < 0 || iValue > 1) )
	{
		EmitWarning( SPEW_GC, 4, "Unable to get accurate read on whether bodygroup '%s' is enabled or disabled by default. (The schema is fine, but the code is confused and could stand to be made smarter.)\n", pszBodygroupName );
	}

	if ( !m_mapDefaultBodygroupState.IsValidIndex( iIndex ) )
	{
		m_mapDefaultBodygroupState.Insert( pszBodygroupName, iDefaultValue );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitEquipRegions( KeyValues *pKVEquipRegions, CUtlVector<CUtlString> *pVecErrors )
{
	CUtlVector<const char *> vecNames;

	FOR_EACH_SUBKEY( pKVEquipRegions, pKVRegion )
	{
		const char *pRegionKeyName = pKVRegion->GetName();

		vecNames.Purge();

		// The "shared" name is special for equip regions -- it means that all of the sub-regions specified
		// will use the same bit to store equipped-or-not data, but that one bit can be accessed by a whole
		// bunch of different names. This is useful in TF where different classes have different regions, but
		// those regions cannot possibly conflict with each other. For example, "scout_backpack" cannot possibly
		// overlap with "pyro_shoulder" because they can't even be equipped on the same character.
		if ( pRegionKeyName && !Q_stricmp( pRegionKeyName, "shared" ) )
		{
			FOR_EACH_SUBKEY( pKVRegion, pKVSharedRegionName )
			{
				vecNames.AddToTail( pKVSharedRegionName->GetName() );
			}
		}
		// We have a standard name -- this one entry is its own equip region.
		else
		{
			vecNames.AddToTail( pRegionKeyName );
		}

		// What bit will this equip region use to mask against conflicts? If we don't have any equip regions
		// at all, we'll use the base bit, otherwise we just grab one higher than whatever we used last.
		unsigned int unNewBitIndex = m_vecEquipRegionsList.Count() <= 0 ? 0 : m_vecEquipRegionsList.Tail().m_unBitIndex + 1;
		
		FOR_EACH_VEC( vecNames, i )
		{
			const char *pRegionName = vecNames[i];

			// Make sure this name is unique.
			if ( GetEquipRegionIndexByName( pRegionName ) >= 0 )
			{
				pVecErrors->AddToTail( CFmtStr( "Duplicate equip region named \"%s\".", pRegionName ).Access() );
				continue;
			}

			// Make a new region.
			EquipRegion newEquipRegion;
			newEquipRegion.m_sName		= pRegionName;
			newEquipRegion.m_unMask		= 0;				// we'll update this mask later
			newEquipRegion.m_unBitIndex	= unNewBitIndex;

			int iIdx = m_vecEquipRegionsList.AddToTail( newEquipRegion );

			// Tag this region to conflict with itself so that if nothing else two items in the same
			// region can't equip over each other.
			SetEquipRegionConflict( iIdx, unNewBitIndex );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitEquipRegionConflicts( KeyValues *pKVEquipRegionConflicts, CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_TRUE_SUBKEY( pKVEquipRegionConflicts, pKVConflict )
	{
		// What region is the base of this conflict?
		const char *pRegionName = pKVConflict->GetName();
		int iRegionIdx = GetEquipRegionIndexByName( pRegionName );
		if ( iRegionIdx < 0 )
		{
			pVecErrors->AddToTail( CFmtStr( "Unable to find base equip region named \"%s\" for conflicts.", pRegionName ).Access() );
			continue;
		}

		FOR_EACH_SUBKEY( pKVConflict, pKVConflictOther )
		{
			const char *pOtherRegionName = pKVConflictOther->GetName();
			int iOtherRegionIdx = GetEquipRegionIndexByName( pOtherRegionName );
			if ( iOtherRegionIdx < 0 )
			{
				pVecErrors->AddToTail( CFmtStr( "Unable to find other equip region named \"%s\" for conflicts.", pOtherRegionName ).Access() );
				continue;
			}

			SetEquipRegionConflict( iRegionIdx,		 m_vecEquipRegionsList[iOtherRegionIdx].m_unBitIndex );
			SetEquipRegionConflict( iOtherRegionIdx, m_vecEquipRegionsList[iRegionIdx].m_unBitIndex );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the attributes section of the schema
// Input:	pKVAttributes - The attributes section of the KeyValues 
//				representation of the schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitAttributes( KeyValues *pKVAttributes, CUtlVector<CUtlString> *pVecErrors )
{
	// Initialize the attribute definitions
	FOR_EACH_TRUE_SUBKEY( pKVAttributes, pKVAttribute )
	{
		int nAttrIndex = Q_atoi( pKVAttribute->GetName() );
		int nMapIndex = m_mapAttributes.Find( nAttrIndex );

		// Make sure the index is positive
		SCHEMA_INIT_CHECK( 
			nAttrIndex >= 0,
			"Attribute definition index %d must be greater than or equal to zero", nAttrIndex );

		// Make sure the attribute index is not repeated
		SCHEMA_INIT_CHECK( 
			!m_mapAttributes.IsValidIndex( nMapIndex ),
			"Duplicate attribute definition index (%d)", nAttrIndex );

		nMapIndex = m_mapAttributes.Insert( nAttrIndex );

		SCHEMA_INIT_SUBSTEP( m_mapAttributes[nMapIndex].BInitFromKV( pKVAttribute, pVecErrors ) );
	}

	// Check the integrity of the attribute definitions

	// Check for duplicate attribute definition names
	CUtlRBTree<const char *> rbAttributeNames( CaselessStringLessThan );
	rbAttributeNames.EnsureCapacity( m_mapAttributes.Count() );
	FOR_EACH_MAP_FAST( m_mapAttributes, i )
	{
		int iIndex = rbAttributeNames.Find( m_mapAttributes[i].GetDefinitionName() );
		SCHEMA_INIT_CHECK( 
			!rbAttributeNames.IsValidIndex( iIndex ),
			"Attribute definition %d: Duplicate name \"%s\"", m_mapAttributes.Key( i ), m_mapAttributes[i].GetDefinitionName() );
		if( !rbAttributeNames.IsValidIndex( iIndex ) )
			rbAttributeNames.Insert( m_mapAttributes[i].GetDefinitionName() );
	}

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Purpose:	Initializes the items section of the schema
// Input:	pKVItems - The items section of the KeyValues 
//				representation of the schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitItems( KeyValues *pKVItems, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapItems.PurgeAndDeleteElements();
	m_mapItemsSorted.Purge();
	m_mapToolsItems.Purge();
	m_mapBaseItems.Purge();
	m_vecBundles.Purge();
	m_mapQuestObjectives.PurgeAndDeleteElements();
	m_vecItemCollectionCrates.Purge();

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	if ( m_pDefaultItemDefinition )
	{
		delete m_pDefaultItemDefinition;
		m_pDefaultItemDefinition = NULL;
	}
#endif

	// initialize the item definitions
	if ( NULL != pKVItems )
	{
		FOR_EACH_TRUE_SUBKEY( pKVItems, pKVItem )
		{
			if ( Q_stricmp( pKVItem->GetName(), "default" ) == 0 )
			{
#if defined(CLIENT_DLL) || defined(GAME_DLL)
				SCHEMA_INIT_CHECK(
					m_pDefaultItemDefinition == NULL,
					"Duplicate 'default' item definition." );

				m_pDefaultItemDefinition = CreateEconItemDefinition();
				SCHEMA_INIT_SUBSTEP( m_pDefaultItemDefinition->BInitFromKV( pKVItem, pVecErrors ) );
#endif
			}
			else
			{
				int nItemIndex = Q_atoi( pKVItem->GetName() );
				int nMapIndex = m_mapItems.Find( nItemIndex );

				// Make sure the item index is correct because we use this index as a reference
				SCHEMA_INIT_CHECK( 
					!m_mapItems.IsValidIndex( nMapIndex ),
					"Duplicate item definition (%d)", nItemIndex );

				// Check to make sure the index is positive
				SCHEMA_INIT_CHECK( 
					nItemIndex >= 0,
					"Item definition index %d must be greater than or equal to zero", nItemIndex );

				CEconItemDefinition *pItemDef = CreateEconItemDefinition();
				nMapIndex = m_mapItems.Insert( nItemIndex, pItemDef );
				m_mapItemsSorted.Insert( nItemIndex, pItemDef );
				SCHEMA_INIT_SUBSTEP( m_mapItems[nMapIndex]->BInitFromKV( pKVItem, pVecErrors ) );

				// Cache off Tools references
				if ( pItemDef->IsTool() )
				{
					m_mapToolsItems.Insert( nItemIndex, pItemDef );
				}

				if ( pItemDef->IsBaseItem() )
				{
					m_mapBaseItems.Insert( nItemIndex, pItemDef );
				}

				// Cache off bundles for the link phase below.
				if ( pItemDef->IsBundle() )
				{
					// Cache off the item def for the bundle, since we'll need both the bundle info and the item def index later.
					m_vecBundles.AddToTail( pItemDef );

					// If the bundle is a pack bundle, mark all the contained items as pack items / link to the owning pack bundle
					if ( pItemDef->IsPackBundle() )
					{
						const bundleinfo_t *pBundleInfo = pItemDef->GetBundleInfo();
						FOR_EACH_VEC( pBundleInfo->vecItemDefs, iCurItem )
						{
							CEconItemDefinition *pCurItemDef = pBundleInfo->vecItemDefs[ iCurItem ];
							SCHEMA_INIT_CHECK( NULL == pCurItemDef->m_pOwningPackBundle, "Pack item \"%s\" included in more than one pack bundle - not allowed!", pCurItemDef->GetDefinitionName() );
							pCurItemDef->m_pOwningPackBundle = pItemDef;
						}
					}
				}

				static CSchemaAttributeDefHandle pAttrDef_ContainsCollection( "contains collection" );
				if ( pAttrDef_ContainsCollection )
				{
					FOR_EACH_VEC( pItemDef->GetStaticAttributes(), i )
					{
						const static_attrib_t& staticAttrib = pItemDef->GetStaticAttributes()[i];
						if ( staticAttrib.iDefIndex == pAttrDef_ContainsCollection->GetDefinitionIndex() )
						{
							// Add to collection crate list
							m_vecItemCollectionCrates.AddToTail( pItemDef->GetDefinitionIndex() );
						}
					}
				}				
			}
		}
	}

	// Check the integrity of the item definitions
	CUtlRBTree<const char *> rbItemNames( CaselessStringLessThan );
	rbItemNames.EnsureCapacity( m_mapItems.Count() );
	FOR_EACH_MAP_FAST( m_mapItems, i )
	{
		CEconItemDefinition *pItemDef = m_mapItems[ i ];

		// Check for duplicate item definition names
		int iIndex = rbItemNames.Find( pItemDef->GetDefinitionName() );
		SCHEMA_INIT_CHECK( 
			!rbItemNames.IsValidIndex( iIndex ),
			"Item definition %s: Duplicate name on index %d", pItemDef->GetDefinitionName(), m_mapItems.Key( i ) );
		if( !rbItemNames.IsValidIndex( iIndex ) )
			rbItemNames.Insert( m_mapItems[i]->GetDefinitionName() );

		// Link up armory and store mappings for the item
		SCHEMA_INIT_SUBSTEP( pItemDef->BInitItemMappings( pVecErrors ) );
	}

#ifdef DOTA
	// Go through all regular (ie non-pack) bundles and ensure that if any pack items are included, *all* pack items in the owning pack bundle are included
	FOR_EACH_VEC( m_vecBundles, iBundle )
	{
		const CEconItemDefinition *pBundleItemDef = m_vecBundles[ iBundle ];
		if ( pBundleItemDef->IsPackBundle() )
			continue;

		// Go through all items in the bundle and look for pack items
		const bundleinfo_t *pBundle = pBundleItemDef->GetBundleInfo();
		if ( pBundle )
		{
			FOR_EACH_VEC( pBundle->vecItemDefs, iContainedBundleItem )
			{
				// Get the associated pack bundle
				const CEconItemDefinition *pContainedBundleItemDef = pBundle->vecItemDefs[ iContainedBundleItem ];

				// Ignore non-pack items
				if ( !pContainedBundleItemDef || !pContainedBundleItemDef->IsPackItem() )
					continue;

				// Get the pack bundle that contains this particular pack item
				const CEconItemDefinition *pOwningPackBundleItemDef = pContainedBundleItemDef->GetOwningPackBundle();

				// Make sure all items in the owning pack bundle are in pBundleItemDef's bundle info (pBundle)
				const bundleinfo_t *pOwningPackBundle = pOwningPackBundleItemDef->GetBundleInfo();
				FOR_EACH_VEC( pOwningPackBundle->vecItemDefs, iCurPackBundleItem )
				{
					CEconItemDefinition *pCurPackBundleItem = pOwningPackBundle->vecItemDefs[ iCurPackBundleItem ];
					if ( !pBundle->vecItemDefs.HasElement( pCurPackBundleItem ) )
					{
						SCHEMA_INIT_CHECK(
							false,
							"The bundle \"%s\" contains some, but not all pack items required specified by pack bundle \"%s.\"",
							pBundleItemDef->GetDefinitionName(),
							pOwningPackBundleItemDef->GetDefinitionName()
					   );
					}
				}
			}
		}
	}
#endif

	return SCHEMA_INIT_SUCCESS();
}

#if 0 // Compiled out until some DotA changes from the item editor are brought over
//-----------------------------------------------------------------------------
// Purpose:	Delete an item definition. Moderately dangerous as cached references will become bad.
// Intended for use by the item editor.
//-----------------------------------------------------------------------------
bool CEconItemSchema::DeleteItemDefinition( int iDefIndex )
{
	m_mapItemsSorted.Remove( iDefIndex );

	int nMapIndex = m_mapItems.Find( iDefIndex );
	if ( m_mapItems.IsValidIndex( nMapIndex ) )
	{
		CEconItemDefinition* pItemDef = m_mapItems[nMapIndex];
		if ( pItemDef )
		{
			m_mapItems.RemoveAt( nMapIndex );
			delete pItemDef;
			return true;
		}
	}
	return false;
}
#endif

//-----------------------------------------------------------------------------
// Purpose:	Parses the Item Sets section.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitItemSets( KeyValues *pKVItemSets, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapItemSets.RemoveAll();

	if ( NULL != pKVItemSets )
	{
		FOR_EACH_TRUE_SUBKEY( pKVItemSets, pKVItemSet )
		{
			const char* setName = pKVItemSet->GetName();

			SCHEMA_INIT_CHECK( setName != NULL, "All itemsets must have names." );
			SCHEMA_INIT_CHECK( m_mapItemSets.Find( setName ) == m_mapItemSets.InvalidIndex(), "Duplicate itemset name (%s) found!", setName );

			int idx = m_mapItemSets.Insert( setName, new CEconItemSetDefinition );
			SCHEMA_INIT_SUBSTEP( m_mapItemSets[idx]->BInitFromKV( pKVItemSet, pVecErrors ) );
		}

		// Once we've initialized all of our item sets, loop through all of our item definitions looking
		// for pseudo set items. For example, the Festive Holy Mackerel is a different item definition from
		// the regular Holy Mackerel, but for set completion and set listing purposes, we want it to show
		// as part of the base set.
		FOR_EACH_MAP_FAST( m_mapItems, i )
		{
			CEconItemDefinition *pItemDef = m_mapItems[i];
			Assert( pItemDef );

			// Items that point to themselves are the base set items and got initialized as part of the
			// set initialization above.
			if ( pItemDef->GetSetItemRemap() == pItemDef->GetDefinitionIndex() )
				continue;

			// Which item are we stealing set information from?
			const CEconItemDefinition *pRemappedSetItemDef = GetItemDefinition( pItemDef->GetSetItemRemap() );
			AssertMsg( pRemappedSetItemDef, "Somehow got through item and set initialization but have a broken set remap item!" );

			pItemDef->SetItemSetDefinition( pRemappedSetItemDef->GetItemSetDefinition() );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
bool CEconItemSchema::BVerifyBaseItemNames( CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_MAP_FAST( m_mapItems, i )
	{
		CEconItemDefinition *pItemDef = m_mapItems[i];

		// get base item name
		const char* pBaseName = pItemDef->GetBaseFunctionalItemName();

		if ( !pBaseName || pBaseName[0] == '\0' )
		{
			continue;
		}
		
		// look up base item name
		SCHEMA_INIT_CHECK( GetItemDefinitionByName( pBaseName ) != NULL, "Base item name not found %s.", pBaseName );
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitItemCollections( KeyValues *pKVItemCollections, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapItemCollections.Purge();

	if ( NULL != pKVItemCollections )
	{
		FOR_EACH_TRUE_SUBKEY( pKVItemCollections, pKVItemCollection )
		{
			const char* setName = pKVItemCollection->GetName();

			SCHEMA_INIT_CHECK( setName != NULL, "All item collections must have names." );
			SCHEMA_INIT_CHECK( m_mapItemCollections.Find( setName ) == m_mapItemCollections.InvalidIndex(), "Duplicate item collection name (%s) found!", setName );

			int idx = m_mapItemCollections.Insert( setName, new CEconItemCollectionDefinition );
			SCHEMA_INIT_SUBSTEP( m_mapItemCollections[idx]->BInitFromKV( pKVItemCollection, pVecErrors ) );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitCollectionReferences( CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_MAP_FAST( m_mapItems, i )
	{
		CEconItemDefinition *pItemDef = m_mapItems[i];
		const char *pszCollectionName = pItemDef->GetCollectionReference();
		if ( pszCollectionName )
		{
			// Find the collection
			bool bFound = false;
			FOR_EACH_MAP_FAST( m_mapItemCollections, iCollectionIndex )
			{
				const char * pszTemp = m_mapItemCollections[iCollectionIndex]->m_pszName;

				if ( !V_strcmp( pszTemp, pszCollectionName) )
				{
					bFound = true;
					pItemDef->SetItemCollectionDefinition( m_mapItemCollections[iCollectionIndex] );
					break;
				}
			}
			SCHEMA_INIT_CHECK( bFound == true, "Collection %s referenced by item %s not found", pszCollectionName, pItemDef->GetDefinitionName() );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}
//-----------------------------------------------------------------------------
const CEconItemCollectionDefinition *CEconItemSchema::GetCollectionByName( const char* pCollectionName )
{
	if ( !pCollectionName )
		return NULL;

	FOR_EACH_MAP_FAST( m_mapItemCollections, iCollectionIndex )
	{
		const char * pszTemp = m_mapItemCollections[iCollectionIndex]->m_pszName;
		if ( !V_strcmp( pszTemp, pCollectionName ) )
		{
			return m_mapItemCollections[iCollectionIndex];
		}
	}
	return NULL;
}



//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitItemPaintKitDefinitions( KeyValues *pKVItemPaintKits, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapItemPaintKits.Purge();

	const char* cWhitespace = " \r\n\t"; // space, end of line, tab.
	cWhitespace; // Compiler happiness for GC build

	if ( NULL != pKVItemPaintKits )
	{
#ifdef CLIENT_DLL
		FOR_EACH_TRUE_SUBKEY( pKVItemPaintKits, pKVPaintKit )
		{
			const char* keyField = pKVPaintKit->GetName();
			SCHEMA_INIT_CHECK( keyField != NULL, "All item collections must have names." );

			if ( V_stristr( keyField, "paintkit_template" ) != NULL )
			{
				static const int cSkipLen = strlen( "paintkit_template" );
				keyField += cSkipLen;
				keyField += strspn( keyField, cWhitespace );

				bool createTmplResult = materials->AddTextureCompositorTemplate( keyField, pKVPaintKit );
				SCHEMA_INIT_CHECK( createTmplResult, "Could Not Create paintkit_template '%s'", keyField );
			}
		}

		// Do post-load validation before moving on to paintkits.
		SCHEMA_INIT_CHECK( materials->VerifyTextureCompositorTemplates(), "Paintkit template post-init validation failed." );
#endif

		// Now do all the paintkits
		FOR_EACH_TRUE_SUBKEY( pKVItemPaintKits, pKVPaintKit )
		{
			// We know the keyField is valid, it was checked above.
			const char* keyField = pKVPaintKit->GetName();

			if ( V_stristr( keyField, "paintkit_template" ) == NULL )
			{
				SCHEMA_INIT_CHECK( m_mapItemPaintKits.Find( keyField ) == m_mapItemPaintKits.InvalidIndex(), "Duplicate paint kit definition name (%s) found!", keyField );

				int idx = m_mapItemPaintKits.Insert( keyField, new CEconItemPaintKitDefinition );
				SCHEMA_INIT_SUBSTEP( m_mapItemPaintKits[ idx ]->BInitFromKV( pKVPaintKit, pVecErrors ) );
			}
		}
	}


	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitOperationDefinitions( KeyValues *pKVGameInfo, KeyValues *pKVOperationDefinitions, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapOperationDefinitions.Purge();

	if ( NULL != pKVOperationDefinitions )
	{
		FOR_EACH_TRUE_SUBKEY( pKVOperationDefinitions, pKVOperation )
		{
			const char* setName = pKVOperation->GetName();
			SCHEMA_INIT_CHECK( setName != NULL, "All operations must have names." );
			SCHEMA_INIT_CHECK( m_mapOperationDefinitions.Find( setName ) == m_mapOperationDefinitions.InvalidIndex(), "Duplicate operation definition name (%s) found!", setName );

			CEconOperationDefinition *pNewOperation = new CEconOperationDefinition();
			SCHEMA_INIT_SUBSTEP( pNewOperation->BInitFromKV( pKVOperation, pVecErrors ) );

			// don't add expired operation to list
			if ( pNewOperation->IsExpired() )
			{
				delete pNewOperation;
				continue;
			}
			
			m_mapOperationDefinitions.Insert( setName, pNewOperation );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Purpose:	Initializes the timed rewards section of the schema
// Input:	pKVTimedRewards - The timed_rewards section of the KeyValues 
//				representation of the schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitTimedRewards( KeyValues *pKVTimedRewards, CUtlVector<CUtlString> *pVecErrors )
{
	m_vecTimedRewards.RemoveAll();

	// initialize the rewards sections
	if ( NULL != pKVTimedRewards )
	{
		FOR_EACH_TRUE_SUBKEY( pKVTimedRewards, pKVTimedReward )
		{
			int index = m_vecTimedRewards.AddToTail();
			SCHEMA_INIT_SUBSTEP( m_vecTimedRewards[index].BInitFromKV( pKVTimedReward, pVecErrors ) );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
const CTimedItemRewardDefinition* CEconItemSchema::GetTimedReward( eTimedRewardType type ) const
{
	if ( (int)type < m_vecTimedRewards.Count() )
	{
		return &m_vecTimedRewards[type];
	}
	return NULL;
}



//-----------------------------------------------------------------------------
// Purpose:	Initializes the loot lists section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitLootLists( KeyValues *pKVLootLists, CUtlVector<CUtlString> *pVecErrors )
{
	if ( NULL != pKVLootLists )
	{
 		FOR_EACH_TRUE_SUBKEY( pKVLootLists, pKVLootList )
		{
			const char* pListName = pKVLootList->GetName();
			SCHEMA_INIT_SUBSTEP( BInsertLootlist( pListName, pKVLootList, pVecErrors ) );
		}
	}

	FOR_EACH_MAP_FAST( m_mapLootLists, i )
	{
		const CEconLootListDefinition *pLootList = m_mapLootLists[i];
		BVerifyLootListItemDropDates( pLootList, pVecErrors );
	}

	return SCHEMA_INIT_SUCCESS();
}
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInsertLootlist( const char *pListName, KeyValues *pKVLootList, CUtlVector<CUtlString> *pVecErrors )
{
	SCHEMA_INIT_CHECK( pListName != NULL, "All lootlists must have names." );

	if ( m_mapLootLists.Count() > 0 )
	{
		SCHEMA_INIT_CHECK( GetLootListByName( pListName ) == NULL, "Duplicate lootlist name (%s) found!", pListName );
	}

	CEconLootListDefinition *pLootList = new CEconLootListDefinition;
	SCHEMA_INIT_SUBSTEP( pLootList->BInitFromKV( pKVLootList, *this, pVecErrors ) );
	m_mapLootLists.Insert( pListName, pLootList );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the revolving loot lists section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitRevolvingLootLists( KeyValues *pKVLootLists, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapRevolvingLootLists.RemoveAll();

	if ( NULL != pKVLootLists )
	{
		FOR_EACH_SUBKEY( pKVLootLists, pKVList )
		{
			int iListIdx = pKVList->GetInt();
			const char* strListName = pKVList->GetName();
			m_mapRevolvingLootLists.Insert( iListIdx, strListName );
		}
	}

 	FOR_EACH_MAP_FAST( m_mapRevolvingLootLists, i )
	{
		const CEconLootListDefinition* pLootList = GetLootListByName(m_mapRevolvingLootLists[i]);
		BVerifyLootListItemDropDates( pLootList, pVecErrors );
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Create and return a new quest objective definition.  Verify that
//			a definition with the same name doesnt alreay exist.
//-----------------------------------------------------------------------------
bool CEconItemSchema::AddQuestObjective( const CQuestObjectiveDefinition **ppQuestObjective, KeyValues *pKVObjective, CUtlVector<CUtlString> *pVecErrors )
{
	// These need to be unique
	int nDefIndex = pKVObjective->GetInt( "defindex", -1 );
	SCHEMA_INIT_CHECK( nDefIndex != -1, "Missing defindex for quest objective" );
	// Verify defindex is unique
	auto nMapIndex = m_mapQuestObjectives.Find( nDefIndex );
	SCHEMA_INIT_CHECK( nMapIndex == m_mapQuestObjectives.InvalidIndex(), "Multiple quest objectives with defindex: %d", nDefIndex );
	// Create the quest def
	nMapIndex = m_mapQuestObjectives.Insert( nDefIndex );
	m_mapQuestObjectives[ nMapIndex ] = CreateQuestDefinition();
	// Init
	SCHEMA_INIT_SUBSTEP( m_mapQuestObjectives[nMapIndex]->BInitFromKV( pKVObjective, pVecErrors ) );

	if ( ppQuestObjective )
	{
		(*ppQuestObjective) = m_mapQuestObjectives[nMapIndex];
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Verify that the contents of visible lootlist do not have drop dates
//			associated with them.  The thinking being that we dont want to have
//			items listed that could potentially not drop, or items disappear/appear
//			in from a list.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BVerifyLootListItemDropDates( const CEconLootListDefinition* pLootList, CUtlVector<CUtlString> *pVecErrors ) const
{
	if ( pLootList && pLootList->BPublicListContents() )
	{
		BRecurseiveVerifyLootListItemDropDates( pLootList, pLootList, pVecErrors );
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Recursively dig through all entries in the passed in lootlist to see
//			if any of the containted items have drop dates.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BRecurseiveVerifyLootListItemDropDates( const CEconLootListDefinition* pLootList, const CEconLootListDefinition* pRootLootList, CUtlVector<CUtlString> *pVecErrors ) const
{
	FOR_EACH_VEC( pLootList->GetLootListContents(), j )
	{
		const CEconLootListDefinition::drop_item_t& item = pLootList->GetLootListContents()[j];
		// 0 and greater means item.  Less than 0 means nested lootlist
		if( item.m_iItemOrLootlistDef >= 0 )
		{
			const CEconItemDefinition* pItemDef = GetItemSchema()->GetItemDefinition( item.m_iItemOrLootlistDef );
			if( pItemDef )
			{
				static CSchemaAttributeDefHandle pAttribDef_StartDropDate( "start drop date" );
				static CSchemaAttributeDefHandle pAttribDef_EndDropDate( "end drop date" );

				CAttribute_String value;

				// Check for start drop date attribute on this item
				SCHEMA_INIT_CHECK( !FindAttribute( pItemDef, pAttribDef_StartDropDate, &value ),
					"Lootlist \"%s\" contains lootlist \"%s\", which contains item \"%s\", which has start drop date.", pRootLootList->GetName(), pLootList->GetName(), pItemDef->GetDefinitionName() );
				// Check for end drop date attribute on this item
				SCHEMA_INIT_CHECK( !FindAttribute( pItemDef, pAttribDef_EndDropDate, &value ),
					"Lootlist \"%s\" contains lootlist \"%s\", which contains item \"%s\", which has end drop date.", pRootLootList->GetName(), pLootList->GetName(), pItemDef->GetDefinitionName() );
			}
		}
		else
		{
			// Get the nested lootlist
			int iLLIndex = (item.m_iItemOrLootlistDef * -1) - 1;
			const CEconLootListDefinition *pNestedLootList = GetItemSchema()->GetLootListByIndex( iLLIndex );
			if ( !pNestedLootList )
				return SCHEMA_INIT_SUCCESS();

			// Dig through all of this lootlist's entries
			BRecurseiveVerifyLootListItemDropDates( pNestedLootList, pRootLootList, pVecErrors );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Purpose:	Initializes the recipes section of the schema
// Input:	pKVRecipes - The recipes section of the KeyValues 
//				representation of the schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitRecipes( KeyValues *pKVRecipes, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapRecipes.RemoveAll();

	// initialize the rewards sections
	if ( NULL != pKVRecipes )
	{
		FOR_EACH_TRUE_SUBKEY( pKVRecipes, pKVRecipe )
		{
			int nRecipeIndex = Q_atoi( pKVRecipe->GetName() );
			int nMapIndex = m_mapRecipes.Find( nRecipeIndex );

			// Make sure the recipe index is correct because we use this index as a reference
			SCHEMA_INIT_CHECK( 
				!m_mapRecipes.IsValidIndex( nMapIndex ),
				"Duplicate recipe definition (%d)", nRecipeIndex );

			// Check to make sure the index is positive
			SCHEMA_INIT_CHECK( 
				nRecipeIndex >= 0,
				"Recipe definition index %d must be greater than or equal to zero", nRecipeIndex );

			CEconCraftingRecipeDefinition *recipeDef = CreateCraftingRecipeDefinition();
			SCHEMA_INIT_SUBSTEP( recipeDef->BInitFromKV( pKVRecipe, pVecErrors ) );

#ifdef _DEBUG
			// Sanity check in debug builds so that we know we aren't putting the same recipe in
			// multiple times.
			FOR_EACH_MAP_FAST( m_mapRecipes, i )
			{
				Assert( i != nRecipeIndex );
				Assert( m_mapRecipes[i] != recipeDef );
			}
#endif // _DEBUG

			// Store this recipe.
			m_mapRecipes.Insert( nRecipeIndex, recipeDef );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Purpose:	Builds the name of a achievement in the form App<ID>.<AchName>
// Input:	unAppID - native app ID
//			pchNativeAchievementName - name of the achievement in its native app
// Returns: The combined achievement name
//-----------------------------------------------------------------------------
CUtlString CEconItemSchema::ComputeAchievementName( AppId_t unAppID, const char *pchNativeAchievementName ) 
{
	return CFmtStr1024( "App%u.%s", unAppID, pchNativeAchievementName ).Access();
}


//-----------------------------------------------------------------------------
// Purpose:	Initializes the achievement rewards section of the schema
// Input:	pKVAchievementRewards - The achievement_rewards section of the KeyValues 
//				representation of the schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitAchievementRewards( KeyValues *pKVAchievementRewards, CUtlVector<CUtlString> *pVecErrors )
{
	m_dictAchievementRewards.RemoveAll();
	m_mapAchievementRewardsByData.PurgeAndDeleteElements();

	// initialize the rewards sections
	if ( NULL != pKVAchievementRewards )
	{
		FOR_EACH_SUBKEY( pKVAchievementRewards, pKVReward )
		{
			AchievementAward_t award;
			if( pKVReward->GetDataType() == KeyValues::TYPE_NONE )
			{
				int32 nItemIndex = pKVReward->GetInt( "DefIndex", -1 );
				if( nItemIndex != -1 )
				{
					award.m_vecDefIndex.AddToTail( (uint16)nItemIndex );			
				}
				else
				{
					KeyValues *pkvItems = pKVReward->FindKey( "Items" );
					SCHEMA_INIT_CHECK( 
						pkvItems != NULL,
						"Complex achievement %s must have an Items key or a DefIndex field", pKVReward->GetName() );
					if( !pkvItems )
					{
						continue;
					}

					FOR_EACH_VALUE( pkvItems, pkvItem )
					{
						award.m_vecDefIndex.AddToTail( (uint16)Q_atoi( pkvItem->GetName() ) );
					}
				}

			}
			else
			{
				award.m_vecDefIndex.AddToTail( (uint16)pKVReward->GetInt("", -1 ) );
			} 
		
			// make sure all the item types are valid
			bool bFoundAllItems = true;
			FOR_EACH_VEC( award.m_vecDefIndex, nItem )
			{
				const CEconItemDefinition *pDefn = GetItemDefinition( award.m_vecDefIndex[nItem] );
				SCHEMA_INIT_CHECK( 
					pDefn != NULL,
					"Item definition index %d in achievement reward %s was not found", award.m_vecDefIndex[nItem], pKVReward->GetName() );
				if( !pDefn )
				{
					bFoundAllItems = false;
				}
			}
			if( !bFoundAllItems )
				continue;

			SCHEMA_INIT_CHECK( 
				award.m_vecDefIndex.Count() > 0,
				"Achievement reward %s has no items!", pKVReward->GetName() );
			if( award.m_vecDefIndex.Count() == 0 )
				continue;

#ifdef GC_DLL
			award.m_unSourceAppId = GGCBase()->GetAppID();
#else		
			award.m_unSourceAppId = k_uAppIdInvalid;
#endif		
			if( pKVReward->GetDataType() == KeyValues::TYPE_NONE )
			{
				// cross game achievement
				award.m_sNativeName = pKVReward->GetName();
				award.m_unAuditData = pKVReward->GetInt( "AuditData", 0 );
				award.m_unSourceAppId = pKVReward->GetInt( "SourceAppID", award.m_unSourceAppId );
			}
			else
			{
				award.m_sNativeName = pKVReward->GetName();
				award.m_unAuditData = 0;
			}


#ifdef GC_DLL
			// Check to make sure the audit data is valid
			SCHEMA_INIT_CHECK( 
				award.m_unSourceAppId >= 0,
				"Source App ID %d in achievement reward %s must be valid", award.m_unSourceAppId, pKVReward->GetName() );
			if( award.m_unSourceAppId == k_uAppIdInvalid )
				continue;

			if( !GGCGameBase()->BYieldingLoadStats( award.m_unSourceAppId ) )
			{
				// this will often fail in a dev universe
				if( GGCHost()->GetUniverse() != k_EUniverseDev )
				{
					SCHEMA_INIT_CHECK( 
						false,
						"Unable to load stats schema for cross-game achievement %s for app %d", pKVReward->GetName(), award.m_unSourceAppId );
				}
				continue;
			}

			const CGCStatsSchema *pStatsSchema = GGCGameBase()->GetStatsSchema( award.m_unSourceAppId );
			if( !pStatsSchema )
			{
				SCHEMA_INIT_CHECK( 
					false,
					"Unable to retrieve stats schema for cross-game achievement %s for app %d", pKVReward->GetName(), award.m_unSourceAppId );
				continue;
			}

			if( award.m_unAuditData == 0 )
			{
				uint16 usStatID, usBitID;
				if( !pStatsSchema->BGetAchievementBit( award.m_sNativeName, &usStatID, &usBitID ) )
				{
					SCHEMA_INIT_CHECK( 
						false,
						"Unable to find achievement %s for app %d", award.m_sNativeName.Get(), award.m_unSourceAppId );
					continue;
				}

				award.m_unAuditData = ( usStatID <<16 ) | usBitID;
			}
#endif // GC_DLL


			AchievementAward_t *pAward = new AchievementAward_t;
			*pAward = award;

			m_dictAchievementRewards.Insert( ComputeAchievementName( pAward->m_unSourceAppId, pAward->m_sNativeName ), pAward );
			m_mapAchievementRewardsByData.Insert( pAward->m_unAuditData, pAward );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}


#ifdef GC_DLL
bool CEconItemSchema::BInitRandomAttributeTemplates( KeyValues *pKVRandomAttributeTemplates, CUtlVector<CUtlString> *pVecErrors )
{
	m_dictRandomAttributeTemplates.PurgeAndDeleteElements();

	FOR_EACH_TRUE_SUBKEY( pKVRandomAttributeTemplates, pKVAttributeTemplate )
	{
		const char *pszAttrName = pKVAttributeTemplate->GetName();

		// try to create random attrib from template
		random_attrib_t *pRandomAttr = CreateRandomAttribute( __FUNCTION__, pKVAttributeTemplate, pVecErrors );
		SCHEMA_INIT_CHECK(
			NULL != pRandomAttr,
			CFmtStr( "%s: Failed to create random_attrib_t '%s'", __FUNCTION__, pszAttrName ) );

		m_dictRandomAttributeTemplates.Insert( pszAttrName, pRandomAttr );
	}

	return true;
}
#endif // GC_DLL


#ifdef TF_CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Go through all items and cache the number of concrete items in each.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitConcreteItemCounts( CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_MAP_FAST( m_mapItems, i )
	{
		CEconItemDefinition *pItemDef = m_mapItems[ i ];
		pItemDef->m_unNumConcreteItems = CalculateNumberOfConcreteItems( pItemDef );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the number of actual "real" items referenced by the item definition
// (i.e. items that would take up space in the inventory)
//-----------------------------------------------------------------------------
int CEconItemSchema::CalculateNumberOfConcreteItems( const CEconItemDefinition *pItemDef )
{
	AssertMsg( pItemDef, "NULL item definition!  This should not happen!" );
	if ( !pItemDef )
		return 0;

	if ( pItemDef->IsBundle() )
	{
		uint32 unNumConcreteItems = 0;
		
		const bundleinfo_t *pBundle = pItemDef->GetBundleInfo();
		Assert( pBundle );

		FOR_EACH_VEC( pBundle->vecItemDefs, i )
		{
			unNumConcreteItems += CalculateNumberOfConcreteItems( pBundle->vecItemDefs[i] );
		}

		return unNumConcreteItems;
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitSteamPackageLocalizationToken( KeyValues *pKVSteamPackages, CUtlVector<CUtlString> *pVecErrors )
{
	if ( NULL != pKVSteamPackages )
	{
		FOR_EACH_TRUE_SUBKEY( pKVSteamPackages, pKVEntry )
		{
			// Check to make sure the index is positive
			int iRawPackageId = atoi( pKVEntry->GetName() );
			SCHEMA_INIT_CHECK( 
				iRawPackageId > 0,
				"Invalid package ID %i for localization", iRawPackageId );

			// Store off our data.
			uint32 unPackageId = (uint32)iRawPackageId;
			const char *pszLocalizationToken = pKVEntry->GetString( "localization_key" );

			m_mapSteamPackageLocalizationTokens.InsertOrReplace( unPackageId, pszLocalizationToken );

		}
	}
	return SCHEMA_INIT_SUCCESS();
}
#endif // TF_CLIENT_DLL

static const char *s_particle_controlpoint_names[] =
{
	"attachment",
	"control_point_1",
	"control_point_2",
	"control_point_3",
	"control_point_4",
	"control_point_5",
	"control_point_6",
};

//-----------------------------------------------------------------------------
// Purpose:	Initializes the attribute-controlled-particle-systems section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitAttributeControlledParticleSystems( KeyValues *pKVParticleSystems, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapAttributeControlledParticleSystems.RemoveAll();
	m_vecAttributeControlledParticleSystemsCosmetics.RemoveAll();
	m_vecAttributeControlledParticleSystemsWeapons.RemoveAll();
	m_vecAttributeControlledParticleSystemsTaunts.RemoveAll();
	
	CUtlVector< int > *pVec = NULL;

	// Addictional groups we are tracking for.
	// "cosmetic_unusual_effects"
	// "weapon_unusual_effects"
	// "taunt_unusual_effects"

	if ( NULL != pKVParticleSystems )
	{
		FOR_EACH_TRUE_SUBKEY( pKVParticleSystems, pKVCategory )
		{
			// There is 3 Categories we want to track with additional info
			if ( !V_strcmp( pKVCategory->GetName(), "cosmetic_unusual_effects" ) )
			{
				pVec = &m_vecAttributeControlledParticleSystemsCosmetics;
			} 
			else if ( !V_strcmp( pKVCategory->GetName(), "weapon_unusual_effects" ) )
			{
				pVec = &m_vecAttributeControlledParticleSystemsWeapons;
			}
			else if ( !V_strcmp( pKVCategory->GetName(), "taunt_unusual_effects" ) )
			{
				pVec = &m_vecAttributeControlledParticleSystemsTaunts;
			}
			else
			{
				pVec = NULL; // reset
			}
			
			FOR_EACH_TRUE_SUBKEY( pKVCategory, pKVEntry )
			{
				int32 nItemIndex = atoi( pKVEntry->GetName() );
				// Check to make sure the index is positive
				SCHEMA_INIT_CHECK( 
					nItemIndex > 0,
					"Particle system index %d greater than zero", nItemIndex );
				if ( nItemIndex <= 0 )
					continue;
				int iIndex = m_mapAttributeControlledParticleSystems.Insert( nItemIndex );
				attachedparticlesystem_t &system = m_mapAttributeControlledParticleSystems[iIndex];
				system.pszSystemName = pKVEntry->GetString( "system", NULL );
				system.bFollowRootBone = pKVEntry->GetInt( "attach_to_rootbone", 0 ) != 0;
				system.iCustomType = 0;
				system.nSystemID = nItemIndex;
				system.fRefireTime = pKVEntry->GetFloat( "refire_time", 0.0f );
				system.bDrawInViewModel = pKVEntry->GetBool( "draw_in_viewmodel", false );
				system.bUseSuffixName = pKVEntry->GetBool( "use_suffix_name", false );
				system.bHasViewModelSpecificEffect = pKVEntry->GetBool( "has_viewmodel_specific_effect", false );

				COMPILE_TIME_ASSERT( ARRAYSIZE( system.pszControlPoints ) == ARRAYSIZE( s_particle_controlpoint_names ) );
				for ( int i=0; i<ARRAYSIZE( system.pszControlPoints ); ++i )
				{
					system.pszControlPoints[i] = pKVEntry->GetString( s_particle_controlpoint_names[i], NULL );
				}

				if ( pVec )
				{
					pVec->AddToTail( nItemIndex );
				}
			}
		}
	}
	return SCHEMA_INIT_SUCCESS();
}

#ifdef CLIENT_DLL
locchar_t *CEconItemSchema::GetParticleSystemLocalizedName( int index ) const
{
	const attachedparticlesystem_t *pSystem = GetItemSchema()->GetAttributeControlledParticleSystem( index );
	if ( !pSystem )
		return NULL;

	char particleNameEntry[128];
	Q_snprintf( particleNameEntry, ARRAYSIZE( particleNameEntry ), "#Attrib_Particle%d", pSystem->nSystemID );

	return g_pVGuiLocalize->Find( particleNameEntry );
}

#endif
//-----------------------------------------------------------------------------
// Purpose: Inits data for items that can level up through kills, etc.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitItemLevels( KeyValues *pKVItemLevels, CUtlVector<CUtlString> *pVecErrors )
{
	m_vecItemLevelingData.RemoveAll();

	// initialize the rewards sections
	if ( NULL != pKVItemLevels )
	{
		FOR_EACH_TRUE_SUBKEY( pKVItemLevels, pKVItemLevelBlock )
		{
			const char *pszLevelBlockName = pKVItemLevelBlock->GetName();
			SCHEMA_INIT_CHECK( GetItemLevelingData( pszLevelBlockName ) == NULL,
				"Duplicate leveling data block named \"%s\".", pszLevelBlockName );

			// Allocate a new structure for this block and assign it. We'll fill in the contents later.
			CUtlVector<CItemLevelingDefinition> *pLevelingData = new CUtlVector<CItemLevelingDefinition>;
			m_vecItemLevelingData.Insert( pszLevelBlockName, pLevelingData );

			FOR_EACH_TRUE_SUBKEY( pKVItemLevelBlock, pKVItemLevel )
			{
				int index = pLevelingData->AddToTail();
				SCHEMA_INIT_SUBSTEP( (*pLevelingData)[index].BInitFromKV( pKVItemLevel, pszLevelBlockName, pVecErrors ) );
			}
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose: Inits data for kill eater types.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitKillEaterScoreTypes( KeyValues *pKVKillEaterScoreTypes, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapKillEaterScoreTypes.RemoveAll();

	// initialize the rewards sections
	if ( NULL != pKVKillEaterScoreTypes )
	{
		FOR_EACH_TRUE_SUBKEY( pKVKillEaterScoreTypes, pKVScoreType )
		{
			unsigned int unIndex = (unsigned int)atoi( pKVScoreType->GetName() );
			SCHEMA_INIT_CHECK( m_mapKillEaterScoreTypes.Find( unIndex ) == KillEaterScoreMap_t::InvalidIndex(),
				"Duplicate kill eater score type index %u.", unIndex );
			
			kill_eater_score_type_t ScoreType;
			ScoreType.m_pszTypeString = pKVScoreType->GetString( "type_name" );
			ScoreType.m_bAllowBotVictims = pKVScoreType->GetBool( "allow_bot_victims", false );
#ifdef GC_DLL
			ScoreType.m_bGCUpdateOnly = pKVScoreType->GetBool( "gc_update_only", false );
			ScoreType.m_AllowIncrementValues = pKVScoreType->GetBool( "gc_allow_increment_values", false );
			ScoreType.m_bIsBaseKillType = pKVScoreType->GetBool( "gc_is_base_kill_type", false );
#endif

			const char *pszLevelBlockName = pKVScoreType->GetString( "level_data", "KillEaterRank" );
			SCHEMA_INIT_CHECK( GetItemLevelingData( pszLevelBlockName ) != NULL,
				"Unable to find leveling data block named \"%s\" for kill eater score type %u.", pszLevelBlockName, unIndex );

			ScoreType.m_pszLevelBlockName = pszLevelBlockName;

			m_mapKillEaterScoreTypes.Insert( unIndex, ScoreType );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitStringTables( KeyValues *pKVStringTables, CUtlVector<CUtlString> *pVecErrors )
{
	m_dictStringTable.PurgeAndDeleteElements();
	
	// initialize the rewards sections
	if ( NULL != pKVStringTables )
	{
		FOR_EACH_SUBKEY( pKVStringTables, pKVTable )
		{
			SCHEMA_INIT_CHECK( !m_dictStringTable.IsValidIndex( m_dictStringTable.Find( pKVTable->GetName() ) ),
				"Duplicate string table name '%s'.", pKVTable->GetName() );

			SchemaStringTableDict_t::IndexType_t i = m_dictStringTable.Insert( pKVTable->GetName(), new CUtlVector< schema_string_table_entry_t > );
			FOR_EACH_SUBKEY( pKVTable, pKVEntry )
			{
				schema_string_table_entry_t s = { atoi( pKVEntry->GetName() ), pKVEntry->GetString() };
				m_dictStringTable[i]->AddToTail( s );
			}
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitCommunityMarketRemaps( KeyValues *pKVCommunityMarketRemaps, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapCommunityMarketDefinitionIndexRemap.Purge();

	if ( NULL != pKVCommunityMarketRemaps )
	{
		FOR_EACH_SUBKEY( pKVCommunityMarketRemaps, pKVRemapBase )
		{
			const char *pszBaseDefName = pKVRemapBase->GetName();
			const CEconItemDefinition *pBaseItemDef = GetItemSchema()->GetItemDefinitionByName( pszBaseDefName );
			SCHEMA_INIT_CHECK( pBaseItemDef != NULL, "Unknown Market remap base definition '%s'.", pszBaseDefName );

			FOR_EACH_SUBKEY( pKVRemapBase, pKVRemap )
			{
				const char *pszDefName = pKVRemap->GetName();
				const CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinitionByName( pszDefName );
				SCHEMA_INIT_CHECK( pItemDef != NULL, "Unknown Market remap definition '%s' (under '%s').", pszDefName, pszBaseDefName );
				SCHEMA_INIT_CHECK( m_mapCommunityMarketDefinitionIndexRemap.Find( pItemDef->GetDefinitionIndex() ) == m_mapCommunityMarketDefinitionIndexRemap.InvalidIndex(), "Duplicate Market remap definition '%s'.\n", pszDefName );

				m_mapCommunityMarketDefinitionIndexRemap.Insert( pItemDef->GetDefinitionIndex(), pBaseItemDef->GetDefinitionIndex() );
			}
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
item_definition_index_t CEconItemSchema::GetCommunityMarketRemappedDefinitionIndex( item_definition_index_t unSearchItemDef ) const
{
	CommunityMarketDefinitionRemapMap_t::IndexType_t index = m_mapCommunityMarketDefinitionIndexRemap.Find( unSearchItemDef );
	if ( index == m_mapCommunityMarketDefinitionIndexRemap.InvalidIndex() )
		return unSearchItemDef;

	return m_mapCommunityMarketDefinitionIndexRemap[index];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const ISchemaAttributeType *CEconItemSchema::GetAttributeType( const char *pszAttrTypeName ) const
{
	FOR_EACH_VEC( m_vecAttributeTypes, i )
	{
		if ( m_vecAttributeTypes[i].m_sName == pszAttrTypeName )
			return m_vecAttributeTypes[i].m_pAttrType;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// CItemLevelingDefinition Accessor
//-----------------------------------------------------------------------------
const CItemLevelingDefinition *CEconItemSchema::GetItemLevelForScore( const char *pszLevelBlockName, uint32 unScore ) const
{
	const CUtlVector<CItemLevelingDefinition> *pLevelingData = GetItemLevelingData( pszLevelBlockName );
	if ( !pLevelingData )
		return NULL;

	if ( pLevelingData->Count() == 0 )
		return NULL;

	FOR_EACH_VEC( (*pLevelingData), i )
	{
		if ( unScore < (*pLevelingData)[i].GetRequiredScore() )
			return &(*pLevelingData)[i];
	}

	return &(*pLevelingData).Tail();
}

//-----------------------------------------------------------------------------
// Kill eater score type accessor
//-----------------------------------------------------------------------------
const kill_eater_score_type_t *CEconItemSchema::FindKillEaterScoreType( uint32 unScoreType ) const
{
	KillEaterScoreMap_t::IndexType_t i = m_mapKillEaterScoreTypes.Find( unScoreType );
	if ( i == KillEaterScoreMap_t::InvalidIndex() )
		return NULL;

	return &m_mapKillEaterScoreTypes[i];
}

//-----------------------------------------------------------------------------
// Kill eater score type accessor
//-----------------------------------------------------------------------------
const char *CEconItemSchema::GetKillEaterScoreTypeLocString( uint32 unScoreType ) const
{
	const kill_eater_score_type_t *pScoreType = FindKillEaterScoreType( unScoreType );

	return pScoreType
		 ? pScoreType->m_pszTypeString
		 : NULL;
}

//-----------------------------------------------------------------------------
// Kill eater score type accessor
//-----------------------------------------------------------------------------
const char *CEconItemSchema::GetKillEaterScoreTypeLevelingDataName( uint32 unScoreType ) const
{
	const kill_eater_score_type_t *pScoreType = FindKillEaterScoreType( unScoreType );

	return pScoreType
		 ? pScoreType->m_pszLevelBlockName
		 : NULL;
}

#if defined(STAGING_ONLY) && ( defined(TF_CLIENT_DLL) || defined(TF_DLL) )
	ConVar tf_allow_strange_bot_kills( "tf_allow_strange_bot_kills", "0", FCVAR_REPLICATED );
#endif
//-----------------------------------------------------------------------------
// Kill eater score type accessor
//-----------------------------------------------------------------------------
bool CEconItemSchema::GetKillEaterScoreTypeAllowsBotVictims( uint32 unScoreType ) const
{
#if defined(STAGING_ONLY) && ( defined(TF_CLIENT_DLL) || defined(TF_DLL) )
	if ( tf_allow_strange_bot_kills.GetBool() )
	{
		return true;
	}
#endif

	const kill_eater_score_type_t *pScoreType = FindKillEaterScoreType( unScoreType );

	return pScoreType
		 ? pScoreType->m_bAllowBotVictims
		 : false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
econ_tag_handle_t CEconItemSchema::GetHandleForTag( const char *pszTagName )
{
	EconTagDict_t::IndexType_t i = m_dictTags.Find( pszTagName );
	if ( m_dictTags.IsValidIndex( i ) )
		return i;

	return m_dictTags.Insert( pszTagName );
}

#ifdef GC_DLL
//-----------------------------------------------------------------------------
// Kill eater score type accessor
//-----------------------------------------------------------------------------
bool CEconItemSchema::GetKillEaterScoreTypeGCOnlyUpdate( uint32 unScoreType ) const
{
	const kill_eater_score_type_t *pScoreType = FindKillEaterScoreType( unScoreType );

	return pScoreType
		 ? pScoreType->m_bGCUpdateOnly
		 : true;								// default to being more restrictive
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconItemSchema::GetKillEaterScoreTypeAllowsIncrementValues( uint32 unScoreType ) const
{
	const kill_eater_score_type_t *pScoreType = FindKillEaterScoreType( unScoreType );

	return pScoreType
		 ? pScoreType->m_AllowIncrementValues
		 : true;								// default to being more restrictive
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const CEconItemSchema::periodic_score_t& CEconItemSchema::GetPeriodicScoreInfo( int iPeriodicScoreIndex ) const
{
	Assert( GetPeriodicScoreTypeList().IsValidIndex( iPeriodicScoreIndex ) );

	return GetPeriodicScoreTypeList()[ iPeriodicScoreIndex ];
}
#endif

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose:	Clones the specified item definition, and returns the new item def.
//-----------------------------------------------------------------------------
void CEconItemSchema::ItemTesting_CreateTestDefinition( int iCloneFromItemDef, int iNewDef, KeyValues *pNewKV )
{
	int nMapIndex = m_mapItems.Find( iNewDef );
	if ( !m_mapItems.IsValidIndex( nMapIndex ) )
	{
		nMapIndex = m_mapItems.Insert( iNewDef, CreateEconItemDefinition() );
		m_mapItemsSorted.Insert( iNewDef, m_mapItems[nMapIndex] );
	}

	// Find & copy the clone item def's data in
	CEconItemDefinition *pCloneDef = GetItemDefinition( iCloneFromItemDef );
	if ( !pCloneDef )
		return;
	m_mapItems[nMapIndex]->CopyPolymorphic( pCloneDef );

	// Then stomp it with the KV test contents
	m_mapItems[nMapIndex]->BInitFromTestItemKVs( iNewDef, pNewKV );
}

//-----------------------------------------------------------------------------
// Purpose:	Discards the specified item definition
//-----------------------------------------------------------------------------
void CEconItemSchema::ItemTesting_DiscardTestDefinition( int iDef )
{
	m_mapItems.Remove( iDef );
	m_mapItemsSorted.Remove( iDef );
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the armory data section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitArmoryData( KeyValues *pKVArmoryData, CUtlVector<CUtlString> *pVecErrors )
{
	m_dictArmoryItemDataStrings.RemoveAll();
	m_dictArmoryAttributeDataStrings.RemoveAll();
	if ( NULL != pKVArmoryData )
	{
		KeyValues *pKVItemTypes = pKVArmoryData->FindKey( "armory_item_types" );
		if ( pKVItemTypes )
		{
			FOR_EACH_SUBKEY( pKVItemTypes, pKVEntry )
			{
				const char *pszDataKey = pKVEntry->GetName();
				const CUtlConstString sLocString( pKVEntry->GetString() );
				m_dictArmoryItemTypesDataStrings.Insert( pszDataKey, sLocString );
			}
		}

		pKVItemTypes = pKVArmoryData->FindKey( "armory_item_classes" );
		if ( pKVItemTypes )
		{
			FOR_EACH_SUBKEY( pKVItemTypes, pKVEntry )
			{
				const char *pszDataKey = pKVEntry->GetName();
				const CUtlConstString sLocString( pKVEntry->GetString() );
				m_dictArmoryItemClassesDataStrings.Insert( pszDataKey, sLocString );
			}
		}

		KeyValues *pKVAttribs = pKVArmoryData->FindKey( "armory_attributes" );
		if ( pKVAttribs )
		{
			FOR_EACH_SUBKEY( pKVAttribs, pKVEntry )
			{
				const char *pszDataKey = pKVEntry->GetName();
				const CUtlConstString sLocString( pKVEntry->GetString() );
				m_dictArmoryAttributeDataStrings.Insert( pszDataKey, sLocString );
			}
		}

		KeyValues *pKVItems = pKVArmoryData->FindKey( "armory_items" );
		if ( pKVItems )
		{
			FOR_EACH_SUBKEY( pKVItems, pKVEntry )
			{
				const char *pszDataKey = pKVEntry->GetName();
				const CUtlConstString sLocString( pKVEntry->GetString() );
				m_dictArmoryItemDataStrings.Insert( pszDataKey, sLocString );
			}
		}
	}
	return SCHEMA_INIT_SUCCESS();
}
#endif


#ifdef GC_DLL
//-----------------------------------------------------------------------------
// Purpose:	Returns the item awarded for an achievement.
// Input:	pchAchievementName - The achievement that was awarded.
// Output:	The achievement struct for this reward.
//-----------------------------------------------------------------------------
const AchievementAward_t * CEconItemSchema::GetAchievementReward( const char *pchAchievementName, AppId_t unAppID ) const
{
	int nRewardIndex = m_dictAchievementRewards.Find( ComputeAchievementName( unAppID, pchAchievementName ) );

	if( m_dictAchievementRewards.IsValidIndex( nRewardIndex ) )
		return m_dictAchievementRewards[ nRewardIndex ];
	else
		return NULL;
}


//-----------------------------------------------------------------------------
// Purpose:	Returns the achievement award that matches the provided data or NULL
//			if there is no such award.
// Input:	unData - The data field that would be stored in ItemAudit
//-----------------------------------------------------------------------------
const AchievementAward_t *CEconItemSchema::GetAchievementRewardByData( uint32 unData ) const
{
	uint nIndex = m_mapAchievementRewardsByData.Find( unData );
	if( m_mapAchievementRewardsByData.IsValidIndex( nIndex ) )
	{
		return m_mapAchievementRewardsByData[nIndex];
	}
	else
	{
		return NULL;
	}
}


#endif // GC_DLL


//-----------------------------------------------------------------------------
// Purpose:	Returns the achievement award that matches the provided defindex or NULL
//			if there is no such award.
// Input:	unData - The data field that would be stored in ItemAudit
//-----------------------------------------------------------------------------
const AchievementAward_t *CEconItemSchema::GetAchievementRewardByDefIndex( uint16 usDefIndex ) const
{
	FOR_EACH_MAP_FAST( m_mapAchievementRewardsByData, nIndex )
	{
		if( m_mapAchievementRewardsByData[nIndex]->m_vecDefIndex.HasElement( usDefIndex ) )
			return m_mapAchievementRewardsByData[nIndex];
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:	Gets a rarity value for a name.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BGetItemRarityFromName( const char *pchName, uint8 *nRarity ) const
{
	if ( 0 == Q_stricmp( "any", pchName ) )
	{
		*nRarity = k_unItemRarity_Any;
		return true;
	}

	FOR_EACH_MAP_FAST( m_mapRarities, i )
	{
		if ( 0 == Q_stricmp( m_mapRarities[i].GetName(), pchName ) )
		{
			*nRarity = m_mapRarities[i].GetDBValue();
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose:	Gets a quality value for a name.
// Input:	pchName - The name to translate.
//			nQuality - (out)The quality number for this name, if found.
// Output:	True if the string matched a quality for this schema, false otherwise.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BGetItemQualityFromName( const char *pchName, uint8 *nQuality ) const
{
	if ( 0 == Q_stricmp( "any", pchName ) )
	{
		*nQuality = k_unItemQuality_Any;
		return true;
	}

	FOR_EACH_MAP_FAST( m_mapQualities, i )
	{
		if ( 0 == Q_stricmp( m_mapQualities[i].GetName(), pchName ) )
		{
			*nQuality = m_mapQualities[i].GetDBValue();
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose:	Gets a quality definition for an index
// Input:	nQuality - The quality to get.
// Output:	A pointer to the desired definition, or NULL if it is not found.
//-----------------------------------------------------------------------------
const CEconItemQualityDefinition *CEconItemSchema::GetQualityDefinition( int nQuality ) const
{ 
	int iIndex = m_mapQualities.Find( nQuality );
	if ( m_mapQualities.IsValidIndex( iIndex ) )
		return &m_mapQualities[iIndex]; 
	return NULL;
}

const CEconItemQualityDefinition *CEconItemSchema::GetQualityDefinitionByName( const char *pszDefName ) const
{
	FOR_EACH_MAP_FAST( m_mapQualities, i )
	{
		if ( V_stricmp( pszDefName, m_mapQualities[i].GetName()) == 0 )
			return &m_mapQualities[i]; 
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// ItemRarity
//-----------------------------------------------------------------------------
const CEconItemRarityDefinition *CEconItemSchema::GetRarityDefinitionByMapIndex( int nRarityIndex ) const
{
	if ( m_mapRarities.IsValidIndex( nRarityIndex ) )
		return &m_mapRarities[nRarityIndex];

	return NULL;
}
//-----------------------------------------------------------------------------
const CEconItemRarityDefinition *CEconItemSchema::GetRarityDefinition( int nRarity ) const
{
	int iIndex = m_mapRarities.Find( nRarity );
	if ( m_mapRarities.IsValidIndex( iIndex ) )
		return &m_mapRarities[iIndex];
	return NULL;
}
//-----------------------------------------------------------------------------
const CEconItemRarityDefinition *CEconItemSchema::GetRarityDefinitionByName( const char *pszDefName ) const
{
	FOR_EACH_MAP_FAST( m_mapRarities, i )
	{
		if ( !strcmp( pszDefName, m_mapRarities[i].GetName() ) )
			return &m_mapRarities[i];
	}
	return NULL;
}
//-----------------------------------------------------------------------------
const char* CEconItemSchema::GetRarityName( uint8 iRarity )
{
	const CEconItemRarityDefinition* pItemRarity = GetRarityDefinition( iRarity );
	if ( !pItemRarity )
		return NULL;
	else
		return pItemRarity->GetName();
}
//-----------------------------------------------------------------------------
const char* CEconItemSchema::GetRarityLocKey( uint8 iRarity )
{
	const CEconItemRarityDefinition* pItemRarity = GetRarityDefinition( iRarity );
	if ( !pItemRarity )
		return NULL;
	else
		return pItemRarity->GetLocKey();
}
//-----------------------------------------------------------------------------
const char* CEconItemSchema::GetRarityColor( uint8 iRarity )
{
	const CEconItemRarityDefinition* pItemRarity = GetRarityDefinition( iRarity );
	if ( !pItemRarity )
		return NULL;
	else
		return GetColorNameForAttribColor( pItemRarity->GetAttribColor() );
}
//-----------------------------------------------------------------------------
int CEconItemSchema::GetRarityIndex( const char* pszRarity )
{
	const CEconItemRarityDefinition* pRarity = GetRarityDefinitionByName( pszRarity );
	if ( pRarity )
		return pRarity->GetDBValue();
	else
		return 0;
}

//-----------------------------------------------------------------------------
bool CEconItemSchema::BGetItemSeries( const char* pchName, uint8 *nItemSeries ) const
{
	FOR_EACH_MAP_FAST( m_mapItemSeries, i )
	{
		if ( 0 == Q_stricmp( m_mapItemSeries[i].GetName(), pchName ) )
		{
			*nItemSeries = m_mapItemSeries[i].GetDBValue();
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
const CEconItemSeriesDefinition *CEconItemSchema::GetItemSeriesDefinition( int iSeries ) const
{
	int iIndex = m_mapItemSeries.Find( iSeries );
	if ( m_mapItemSeries.IsValidIndex( iIndex ) )
		return &m_mapItemSeries[iIndex];
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:	Gets an item definition for the specified definition index
// Input:	iItemIndex - The index of the desired definition.
// Output:	A pointer to the desired definition, or NULL if it is not found.
//-----------------------------------------------------------------------------
CEconItemDefinition *CEconItemSchema::GetItemDefinition( int iItemIndex )
{
#if defined(CLIENT_DLL) || defined(GAME_DLL)
#if !defined(CSTRIKE_DLL)
	AssertMsg( GetDefaultItemDefinition(), "No default item definition set up for item schema." );
#endif // CSTRIKE_DLL
#endif // defined(CLIENT_DLL) || defined(GAME_DLL)

	int iIndex = m_mapItems.Find( iItemIndex );
	if ( m_mapItems.IsValidIndex( iIndex ) )
		return m_mapItems[iIndex]; 

#if defined( GC_DLL ) || defined( EXTERNALTESTS_DLL )
	return NULL;
#else // !GC_DLL
	if ( GetDefaultItemDefinition() )
		return GetDefaultItemDefinition();

#if !defined(CSTRIKE_DLL)
	// We shouldn't ever get down here, but all the same returning a valid pointer is very slightly
	// a better plan than returning an invalid pointer to code that won't check to see if it's valid.
	static CEconItemDefinition *s_pEmptyDefinition = CreateEconItemDefinition();
	return s_pEmptyDefinition;
#else
	return NULL;
#endif // CSTRIKE_DLL

#endif // GC_DLL
}
const CEconItemDefinition *CEconItemSchema::GetItemDefinition( int iItemIndex ) const
{
	return const_cast<CEconItemSchema *>(this)->GetItemDefinition( iItemIndex );
}

//-----------------------------------------------------------------------------
// Purpose:	Gets an item definition that has a name matching the specified name.
// Input:	pszDefName - The name of the desired definition.
// Output:	A pointer to the desired definition, or NULL if it is not found.
//-----------------------------------------------------------------------------
CEconItemDefinition *CEconItemSchema::GetItemDefinitionByName( const char *pszDefName )
{
	// This shouldn't happen, but let's not crash if it ever does.
	Assert( pszDefName != NULL );
	if ( pszDefName == NULL )
		return NULL;

	FOR_EACH_MAP_FAST( m_mapItems, i )
	{
		if ( V_stricmp( pszDefName, m_mapItems[i]->GetDefinitionName()) == 0 )
			return m_mapItems[i]; 
	}
	return NULL;
}

const CEconItemDefinition *CEconItemSchema::GetItemDefinitionByName( const char *pszDefName ) const
{
	return const_cast<CEconItemSchema *>(this)->GetItemDefinitionByName( pszDefName );
}


#ifdef GC_DLL
random_attrib_t *CEconItemSchema::GetRandomAttributeTemplateByName( const char *pszAttrTemplateName ) const
{
	int index = m_dictRandomAttributeTemplates.Find( pszAttrTemplateName );
	if ( index != m_dictRandomAttributeTemplates.InvalidIndex() )
	{
		return m_dictRandomAttributeTemplates[index];
	}

	return NULL;
}
#endif // GC_DLL


//-----------------------------------------------------------------------------
// Purpose:	Gets an attribute definition for an index
// Input:	iAttribIndex - The index of the desired definition.
// Output:	A pointer to the desired definition, or NULL if it is not found.
//-----------------------------------------------------------------------------
CEconItemAttributeDefinition *CEconItemSchema::GetAttributeDefinition( int iAttribIndex )
{ 
	int iIndex = m_mapAttributes.Find( iAttribIndex );
	if ( m_mapAttributes.IsValidIndex( iIndex ) )
		return &m_mapAttributes[iIndex]; 
	return NULL;
}
const CEconItemAttributeDefinition *CEconItemSchema::GetAttributeDefinition( int iAttribIndex ) const
{
	return const_cast<CEconItemSchema *>(this)->GetAttributeDefinition( iAttribIndex );
}

CEconItemAttributeDefinition *CEconItemSchema::GetAttributeDefinitionByName( const char *pszDefName )
{
	Assert( pszDefName );
	if ( !pszDefName )
		return NULL;

	VPROF_BUDGET( "CEconItemSchema::GetAttributeDefinitionByName", VPROF_BUDGETGROUP_STEAM );
	FOR_EACH_MAP_FAST( m_mapAttributes, i )
	{
		Assert( m_mapAttributes[i].GetDefinitionName() );
		if ( !m_mapAttributes[i].GetDefinitionName() )
			continue;

		if ( V_stricmp( pszDefName, m_mapAttributes[i].GetDefinitionName() ) == 0 )
			return &m_mapAttributes[i]; 
	}
	return NULL;
}
const CEconItemAttributeDefinition *CEconItemSchema::GetAttributeDefinitionByName( const char *pszDefName ) const
{
	return const_cast<CEconItemSchema *>(this)->GetAttributeDefinitionByName( pszDefName );
}

//-----------------------------------------------------------------------------
// Purpose:	Gets a recipe definition for an index
// Input:	iRecipeIndex - The index of the desired definition.
// Output:	A pointer to the desired definition, or NULL if it is not found.
//-----------------------------------------------------------------------------
CEconCraftingRecipeDefinition *CEconItemSchema::GetRecipeDefinition( int iRecipeIndex )
{ 
	int iIndex = m_mapRecipes.Find( iRecipeIndex );
	if ( m_mapRecipes.IsValidIndex( iIndex ) )
		return m_mapRecipes[iIndex]; 
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEconColorDefinition *CEconItemSchema::GetColorDefinitionByName( const char *pszDefName )
{
	FOR_EACH_VEC( m_vecColorDefs, i )
	{
		if ( !Q_stricmp( m_vecColorDefs[i]->GetName(), pszDefName ) )
			return m_vecColorDefs[i];
	}
	return NULL;
}
const CEconColorDefinition *CEconItemSchema::GetColorDefinitionByName( const char *pszDefName ) const
{
	return const_cast<CEconItemSchema *>(this)->GetColorDefinitionByName( pszDefName );
}


#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CEconItemSchema::GetSteamPackageLocalizationToken( uint32 unPackageId ) const
{
	SteamPackageLocalizationTokenMap_t::IndexType_t i = m_mapSteamPackageLocalizationTokens.Find( unPackageId );
	if ( m_mapSteamPackageLocalizationTokens.IsValidIndex( i ) )
		return m_mapSteamPackageLocalizationTokens[i]; 
	return NULL;
}
#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose:	Return the attribute specified attachedparticlesystem_t* associated with the given id.
//-----------------------------------------------------------------------------
attachedparticlesystem_t* CEconItemSchema::GetAttributeControlledParticleSystem( int id )
{
	int iIndex = m_mapAttributeControlledParticleSystems.Find( id );
	if ( m_mapAttributeControlledParticleSystems.IsValidIndex( iIndex ) )
		return &m_mapAttributeControlledParticleSystems[iIndex];
	return NULL;
}

attachedparticlesystem_t* CEconItemSchema::FindAttributeControlledParticleSystem( const char *pchSystemName )
{
	FOR_EACH_MAP_FAST( m_mapAttributeControlledParticleSystems, nSystem )
	{
		if( !Q_stricmp( m_mapAttributeControlledParticleSystems[nSystem].pszSystemName, pchSystemName ) )
			return &m_mapAttributeControlledParticleSystems[nSystem];
	}
	return NULL;
}


#if defined(CLIENT_DLL) || defined(GAME_DLL)
bool CEconItemSchema::SetupPreviewItemDefinition( KeyValues *pKV )
{
	int nMapIndex = m_mapItems.Find( PREVIEW_ITEM_DEFINITION_INDEX );
	if ( !m_mapItems.IsValidIndex( nMapIndex ) )
	{
		nMapIndex = m_mapItems.Insert( PREVIEW_ITEM_DEFINITION_INDEX, CreateEconItemDefinition() );
	}

	CEconItemDefinition *pItemDef = m_mapItems[ nMapIndex ];
	return pItemDef->BInitFromKV( pKV );
}
#endif // defined(CLIENT_DLL) || defined(GAME_DLL)

#ifdef GC_DLL
//-----------------------------------------------------------------------------
// Purpose: Returns all the foreign item imports for an app ID
//-----------------------------------------------------------------------------
const CEconItemDefinition *CEconItemSchema::GetAppItemImport( AppId_t unAppID, uint16 usDefIndex ) const
{
	int i = m_mapForeignImports.Find( unAppID );
	if( m_mapForeignImports.IsValidIndex( i ) )
		return m_mapForeignImports[i]->FindMapping( usDefIndex );
	else
		return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Returns all the foreign item imports for an app ID
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitForeignImports( CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_MAP_FAST( m_mapItems, nItem )
	{
		CEconItemDefinition *pDefn = m_mapItems[nItem];

		KeyValues *pkvImport = pDefn->GetDefinitionKey( "import_from" );
		if( !pkvImport )
			continue;

		FOR_EACH_VALUE( pkvImport, pkvApp )
		{
			CForeignAppImports *pAppImports = FindOrAddAppImports( Q_atoi( pkvApp->GetName() ) );
			pAppImports->AddMapping( pkvApp->GetInt(), pDefn );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns all the foreign item imports for an app ID
//-----------------------------------------------------------------------------
CForeignAppImports *CEconItemSchema::FindOrAddAppImports( AppId_t unAppID )
{
	int i = m_mapForeignImports.Find( unAppID );
	if( m_mapForeignImports.IsValidIndex( i ) )
		return m_mapForeignImports[i];
	else
	{
		m_vecForeignApps.AddToTail( unAppID );
		CForeignAppImports *pApp = new CForeignAppImports();
		m_mapForeignImports.Insert( unAppID, pApp );
		return pApp;
	}
}
#endif // GC_DLL

bool CEconItemSchema::BCanStrangeFilterApplyToStrangeSlotInItem( uint32 /*strange_event_restriction_t*/ unRestrictionType, uint32 unRestrictionValue, const IEconItemInterface *pItem, int iStrangeSlot, uint32 *out_pOptionalScoreType ) const
{
	Assert( unRestrictionType != kStrangeEventRestriction_None );

	// Do we have a type for this slot? If not, move on, with the exception: all user-custom scores
	// we expect to have types, but certain weapons may not specify a base type and just use
	// kills implicitly.
	uint32 unStrangeScoreTypeBits = kKillEaterEvent_PlayerKill;
	if ( !pItem->FindAttribute( GetKillEaterAttr_Type( iStrangeSlot ), &unStrangeScoreTypeBits ) && iStrangeSlot != 0 )
		return false;

	// Do we have a restriction already in this slot? If so, move on.
	if ( pItem->FindAttribute( GetKillEaterAttr_Restriction( iStrangeSlot ) ) )
		return false;

	// We've found an open slot. Make sure that adding our restriction to this slot
	// won't result in a duplicate score-type/restriction-type entry.
	for ( int j = 0; j < GetKillEaterAttrCount(); j++ )
	{
		// Don't compare against ourself.
		if ( iStrangeSlot == j )
			continue;

		// Ignore this entry if we don't have a score type or if the score type differs from
		// our search criteria above.
		uint32 unAltStrangeScoreType;
		if ( !pItem->FindAttribute( GetKillEaterAttr_Type( j ), &unAltStrangeScoreType ) ||
			 unAltStrangeScoreType != unStrangeScoreTypeBits )
		{
			continue;
		}

		// This entry does have the same type, so tag us as a duplicate if we also have the same
		// restriction that we're trying to apply.
		uint32 unAltRestrictionType;
		uint32 unAltRestrictionValue;
		if ( pItem->FindAttribute( GetKillEaterAttr_Restriction( j ), &unAltRestrictionType ) &&
			 unAltRestrictionType == unRestrictionType &&
			 pItem->FindAttribute( GetKillEaterAttr_RestrictionValue( j ), &unAltRestrictionValue ) &&
			 unAltRestrictionValue == unRestrictionValue )
		{
			return false;
		}
	}	

	if ( out_pOptionalScoreType )
	{
		*out_pOptionalScoreType = *(float *)&unStrangeScoreTypeBits;
	}

	// Everything seems alright.
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CEconItemSchema::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_mapQualities );

	FOR_EACH_MAP_FAST( m_mapQualities, i )
	{
		ValidateObj( m_mapQualities[i] );
	}

	ValidateObj( m_mapItems );

	FOR_EACH_MAP_FAST( m_mapItems, i )
	{
		ValidateObj( m_mapItems[i] );
	}

	ValidateObj( m_mapUpgradeableBaseItems );

	FOR_EACH_MAP_FAST( m_mapUpgradeableBaseItems, i )
	{
		ValidateObj( m_mapUpgradeableBaseItems[i] );
	}

	ValidateObj( m_mapAttributes );

	FOR_EACH_MAP_FAST( m_mapAttributes, i )
	{
		ValidateObj( m_mapAttributes[i] );
	}

	ValidateObj( m_mapRecipes );

	FOR_EACH_MAP_FAST( m_mapRecipes, i )
	{
		ValidateObj( m_mapRecipes[i] );
	}

	FOR_EACH_VEC( m_vecTimedRewards, i )
	{
		ValidateObj( m_vecTimedRewards[i] );
	}
	ValidateObj( m_vecTimedRewards );

}
#endif // DBGFLAG_VALIDATE

#ifdef GC_DLL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitExperiements( KeyValues *pKVExperiments, CUtlVector<CUtlString> *pVecErrors )
{
	m_vecExperiments.RemoveAll();

	if ( NULL != pKVExperiments )
	{
		FOR_EACH_TRUE_SUBKEY( pKVExperiments, pKVEntry )
		{
			const char *listName = pKVEntry->GetName();

			SCHEMA_INIT_CHECK( listName != NULL, "All experiments must have titles.");

			int idx = m_vecExperiments.AddToTail();
			SCHEMA_INIT_SUBSTEP( m_vecExperiments[idx].BInitFromKV( pKVEntry, pVecErrors ) );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// CExperimentDefinition
//-----------------------------------------------------------------------------
CExperimentDefinition::CExperimentDefinition( void )
	: m_bEnabled( false )
	, m_unExperimentID( 0 )
	, m_unNumParticipants( 0 )
	, m_unMaxParticipants( 0 )
	, m_pKeyValues( NULL )
{
}

CExperimentDefinition::~CExperimentDefinition( void )
{
	if ( m_pKeyValues )
	{
		m_pKeyValues->deleteThis();
	}
}

bool CExperimentDefinition::BInitFromKV( KeyValues *pKVExperiment, CUtlVector<CUtlString> *pVecErrors )
{
	m_pKeyValues = pKVExperiment->MakeCopy();

	m_unExperimentID = Q_atoi( m_pKeyValues->GetName() );
	m_bEnabled = m_pKeyValues->GetBool( "enabled" );
	m_unNumParticipants = 0;
	m_unMaxParticipants = 0;
	
	KeyValues *pKVGroups = m_pKeyValues->FindKey( "groups" );
	if ( pKVGroups )
	{
		FOR_EACH_TRUE_SUBKEY( pKVGroups, pKVEntry )
		{
			int idx = m_vecGroups.AddToTail();
			experiment_group_t &group = m_vecGroups[idx];
			group.m_pKeyValues = pKVEntry;
			group.m_pName = pKVEntry->GetName();
			group.m_unNumParticipants = 0;
			group.m_unMaxParticipants = pKVEntry->GetInt( "num_participants", 0 );
			m_unMaxParticipants += group.m_unMaxParticipants;
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

bool CExperimentDefinition::ChooseGroup( uint32 &unGroup )
{
	CUtlVector< uint32 > vecGroupIndices;
	FOR_EACH_VEC( m_vecGroups, i )
	{
		experiment_group_t &group = m_vecGroups[i];
		if ( group.m_unNumParticipants < group.m_unMaxParticipants )
		{
			vecGroupIndices.AddToTail( i );
		}
	}
	if ( vecGroupIndices.Count() == 0 )
	{
		return false;
	}

	uint32 idx = vecGroupIndices[ RandomInt( 0, vecGroupIndices.Count() - 1 ) ];
	experiment_group_t &group = m_vecGroups[ idx ];
	++group.m_unNumParticipants;
	unGroup = idx;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Give the chance after the schema has been initialized to sanity-check
//			individual parts or interactions before moving forward.
//-----------------------------------------------------------------------------
#ifdef TF_GC_DLL
static bool BTestToolApplicability( CUtlVector<CUtlString> *pVecErrors )
{
	static CSchemaItemDefHandle pItem_TeamPaint				( "Paint Can Team Color" ),			// tools
								pItem_Paint					( "Paint Can 14" ),
								pItem_DescriptionTag		( "Description Tag" ),
								pItem_NameTag				( "Name Tag" ),
								pItem_Key					( "Decoder Ring" ),
								pItem_SummerKey				( "Summer Key" ),
								pItem_GiftWrap				( "Gift Wrap" ),
								pItem_CustomTextureTool		( "Customize Texture Tool" ),
								pItem_SupplyCrateGeneric	( "Supply Crate 2" ),				// tool targets
								pItem_SummerCrate			( "Summer Crate" ),
								pItem_PaintableItem			( "Summer Hat" ),
								//pItem_TeamPaintableItem	( "" ),
								pItem_UnpaintableItem		( "Big Steel Jaw of Summer Fun" ),
								//pItem_UnnameableWeapon	( "" ),
								pItem_NameableWeapon		( "The Axtinguisher" ),
								pItem_GiftWrappableItem		( "Supply Crate 3" ),
								pItem_NonGiftWrappableItem	( "Wrapped Gift" ),
								pItem_StampableObject		( "The Conscientious Objector" ),
								pItem_NonstampableObject	( "Spiral Sallet" );

	struct ToolValidityTest_t
	{
		const CEconItemDefinition *m_pTool;
		const CEconItemDefinition *m_pTarget;
		bool m_bExpectedValidity;
	};

	ToolValidityTest_t definitionTests[] =
	{
		{ pItem_TeamPaint,					pItem_TeamPaint,				false },
		{ pItem_TeamPaint,					pItem_PaintableItem,			true },
		//{ pItem_TeamPaint,				pItem_TeamPaintableItem,		true },
		{ pItem_TeamPaint,					pItem_UnpaintableItem,			false },
		{ pItem_Paint,						pItem_PaintableItem,			true },
		//{ pItem_Paint,					pItem_TeamPaintableItem,		false },
		{ pItem_Paint,						pItem_UnpaintableItem,			false },
		{ pItem_Paint,						pItem_SupplyCrateGeneric,		false },
		{ pItem_Key,						pItem_SupplyCrateGeneric,		true },
		{ pItem_Key,						pItem_SummerCrate,				false },
		{ pItem_SummerKey,					pItem_SupplyCrateGeneric,		true },		// summer keys in staging are now regular keys and should...
		{ pItem_SummerKey,					pItem_SummerCrate,				false },	// ...be able to open regular crates
		{ pItem_NameTag,					pItem_NameableWeapon,			true },
		//{ pItem_NameTag,					pItem_UnnameableWeapon,			false },
		{ pItem_DescriptionTag,				pItem_NameableWeapon,			true },
		//{ pItem_DescriptionTag,			pItem_UnnameableWeapon,			false },
		{ pItem_CustomTextureTool,			pItem_StampableObject,			true },
		{ pItem_CustomTextureTool,			pItem_NonstampableObject,		false },
	};

	bool bAllSuccess = true;
	for ( int i = 0; i < ARRAYSIZE( definitionTests ); i++ )
	{
		const GameItemDefinition_t *pToolDef   = dynamic_cast<const GameItemDefinition_t *>( definitionTests[i].m_pTool ),
								   *pTargetDef = dynamic_cast<const GameItemDefinition_t *>( definitionTests[i].m_pTarget );

		if ( !pToolDef )
		{
			bAllSuccess = false;
			pVecErrors->AddToTail( CFmtStr( "Tool validity test %i failed: Tool is NULL.", i ).Access() );
			continue;
		}

		if ( !pTargetDef )
		{
			bAllSuccess = false;
			pVecErrors->AddToTail( CFmtStr( "Tool validity test %i failed: Target is NULL.", i ).Access() );
			continue;
		}

		if ( CEconSharedToolSupport::ToolCanApplyToDefinition( pToolDef, pTargetDef ) != definitionTests[i].m_bExpectedValidity )
		{
			bAllSuccess = false;
			pVecErrors->AddToTail( CFmtStr( "Tool validity test %i failed: %s %s have been able to apply to %s.",
											i,
											pToolDef->GetDefinitionName(),
											definitionTests[i].m_bExpectedValidity ? "should" : "shouldn't",
											pTargetDef->GetDefinitionName() ).Access() );
		}
	}

	// These tests require actual instances of the item--not just the definitions. 
	ToolValidityTest_t interfaceTests[] = 
	{
		{ pItem_GiftWrap,					pItem_GiftWrappableItem,		true },
		{ pItem_GiftWrap,					pItem_NonGiftWrappableItem,		false },
	};

	// Skip if we already have failures.
	if ( bAllSuccess )
	{
		for ( int i = 0; i < ARRAYSIZE( interfaceTests ); i++ )
		{
			const GameItemDefinition_t *pToolDef = dynamic_cast< const GameItemDefinition_t * >( interfaceTests[ i ].m_pTool ),
									   *pTargetDef = dynamic_cast< const GameItemDefinition_t * >( interfaceTests[ i ].m_pTarget );

			if ( !pToolDef )
			{
				bAllSuccess = false;
				pVecErrors->AddToTail( CFmtStr( "Tool validity test %i failed: Tool is NULL.", i ).Access() );
				continue;
			}

			if ( !pTargetDef )
			{
				bAllSuccess = false;
				pVecErrors->AddToTail( CFmtStr( "Tool validity test %i failed: Target is NULL.", i ).Access() );
				continue;
			}

			CEconItem *pTool = GEconManager()->GetItemFactory().CreateSpecificItem( ( const CEconGameAccount *) NULL, pToolDef->GetDefinitionIndex() );
			CEconItem *pTarget = GEconManager()->GetItemFactory().CreateSpecificItem( ( const CEconGameAccount *) NULL, pTargetDef->GetDefinitionIndex() );

			if ( CEconSharedToolSupport::ToolCanApplyTo( pTool, pTarget ) != interfaceTests[ i ].m_bExpectedValidity )
			{
				bAllSuccess = false;
				pVecErrors->AddToTail( CFmtStr( "Tool validity test %i failed: %s %s have been able to apply to %s.",
									   i,
									   pToolDef->GetDefinitionName(),
									   interfaceTests[ i ].m_bExpectedValidity ? "should" : "shouldn't",
									   pTargetDef->GetDefinitionName() ).Access() );
			}
			
			delete pTool;
			delete pTarget;
		}
	}




	return bAllSuccess;
}
#endif // TF_GC_DLL
#endif // defined(GC_DLL)

bool CEconItemSchema::BPostSchemaInit( CUtlVector<CUtlString> *pVecErrors ) const
{
	bool bAllSuccess = true;

	// Make sure all of our tools are valid. We have to do this after the whole schema is initialized so
	// that we don't run into circular reference problems with items referencing loot lists that reference
	// items, etc.
	FOR_EACH_MAP_FAST( m_mapItems, i )
	{
		const CEconItemDefinition *pItemDef = m_mapItems[i];
		const IEconTool *pTool = pItemDef->GetEconTool();
		
		if ( pTool && !const_cast<IEconTool *>( pTool )->BFinishInitialization() )
		{
#ifdef GC_DLL
			bAllSuccess = false;
			pVecErrors->AddToTail( CFmtStr( "BPostSchemaInit(): tool '%s' is invalid.", pItemDef->GetDefinitionName() ).Get() );
#endif // GC_DLL
		}
#if TF_GC_DLL
		else
		{
			// all cosmetic items should have these two attributes from the
			// cosmetic_killeater_attribs prefab in case we ever try to drop them as Strange
			static CSchemaAttributeDefHandle pAttribDef_KillEaterScoreType( "kill eater score type" );
			static CSchemaAttributeDefHandle pAttribDef_KillEaterKillType( "kill eater kill type" );

			const CTFItemDefinition *pTFItemDef = assert_cast< const CTFItemDefinition* >( pItemDef );
			if ( pTFItemDef )
			{
				int nSlot = pTFItemDef->GetLoadoutSlot( 0 ); // 0 gives use the default slot
				if ( ( nSlot == LOADOUT_POSITION_HEAD ) || ( nSlot == LOADOUT_POSITION_MISC ) || ( nSlot == LOADOUT_POSITION_MISC2 ) )
				{
					bool bFoundScore = false;
					bool bFoundKill = false;

					FOR_EACH_VEC( pTFItemDef->GetStaticAttributes(), iIndex )
					{
						const static_attrib_t& staticAttrib = pTFItemDef->GetStaticAttributes()[iIndex];
						const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( staticAttrib.iDefIndex );
						if ( pAttrDef == pAttribDef_KillEaterScoreType )
						{
							bFoundScore = true;
						}
						else if ( pAttrDef == pAttribDef_KillEaterKillType )
						{
							bFoundKill = true;
						}
					}

					if ( !bFoundScore || !bFoundKill )
					{
						bAllSuccess = false;
						pVecErrors->AddToTail( CFmtStr( "BPostSchemaInit(): '%s' is missing the standard cosmetic killeater attributes.", pItemDef->GetDefinitionName() ).Get() );
					}
				}
			}
		}
#endif // TF_GC_DLL
	}

#if TF_GC_DLL
	// Make sure our tool application code validity works correctly.
	if ( !BTestToolApplicability( pVecErrors ) )
	{
		bAllSuccess = false;
		pVecErrors->AddToTail( "BPostSchemaInit(): error with tool application validity." );
	}

	const CEconLootListDefinition* pUnusualLootlist = GetItemSchema()->GetLootListByName( "all_particle_hats" );
	if ( !pUnusualLootlist )
	{
		bAllSuccess = false;
		pVecErrors->AddToTail( "No lootlist \"all_particle_hats\"" );
	}
	else
	{
		auto& contents = pUnusualLootlist->GetLootListContents();
		FOR_EACH_VEC( contents, i )
		{
			if ( contents[ i ].m_iItemOrLootlistDef > 0 )
			{
				const CEconItemDefinition* pItemDef = GetItemDefinition( contents[ i ].m_iItemOrLootlistDef );
				if ( !( pItemDef->GetEquipRegionMask() & GetItemSchema()->GetEquipRegionBitMaskByName( "hat" ) ) 
				  && !( pItemDef->GetEquipRegionMask() & GetItemSchema()->GetEquipRegionBitMaskByName( "whole_head" ) ) )
				{
					bAllSuccess = false;
					pVecErrors->AddToTail( CFmtStr( "Item \"%s\" is in all_particle_hats, but doesn't have equip region hat or whole_head, meaning it can't become unusual.  REMOVE IT!", pItemDef->GetDefinitionName() ).Get() );
				}
			}
		}
	}
#endif // TF_GC_DLL

	return bAllSuccess;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CItemLevelingDefinition::CItemLevelingDefinition( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CItemLevelingDefinition::CItemLevelingDefinition( const CItemLevelingDefinition &that )
{
	(*this) = that;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CItemLevelingDefinition::~CItemLevelingDefinition()
{
	// Free up strdup() memory.
	free( m_pszLocalizedName_LocalStorage );
}

//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CItemLevelingDefinition &CItemLevelingDefinition::operator=( const CItemLevelingDefinition &other )
{
	m_unLevel = other.m_unLevel;
	m_unRequiredScore = other.m_unRequiredScore;
	m_pszLocalizedName_LocalStorage = strdup( other.m_pszLocalizedName_LocalStorage );

	return *this;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CItemLevelingDefinition::BInitFromKV( KeyValues *pKVItemLevel, const char *pszLevelBlockName, CUtlVector<CUtlString> *pVecErrors )
{
	m_unLevel = Q_atoi( pKVItemLevel->GetName() );
	m_unRequiredScore = pKVItemLevel->GetInt( "score" );
	m_pszLocalizedName_LocalStorage = strdup( pKVItemLevel->GetString( "rank_name", CFmtStr( "%s%i", pszLevelBlockName, m_unLevel ).Access() ) );

	return SCHEMA_INIT_SUCCESS();
}

#ifdef GC_DLL
EUniverse GetUniverse()
{
	return GGCHost()->GetUniverse();
}
#endif // GC_DLL

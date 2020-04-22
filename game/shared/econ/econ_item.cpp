//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CEconItem, a shared object for econ items
//
//=============================================================================

#include "cbase.h"
#include "econ_item.h"
#include "econ_item_schema.h"
#include "rtime.h"
#include "gcsdk/enumutils.h"
#include "smartptr.h"

#ifdef GC_DLL
#include "gcsdk/sqlaccess/sqlaccess.h"
#include "econ/localization_provider.h"
#endif

#if defined( TF_CLIENT_DLL ) || defined( TF_DLL )
#include "tf_gcmessages.h"
#endif

using namespace GCSDK;

#ifdef GC_DLL
IMPLEMENT_CLASS_MEMPOOL( CEconItem, 100 * 1000, UTLMEMORYPOOL_GROW_SLOW );
IMPLEMENT_CLASS_MEMPOOL( CEconItemCustomData, 50 * 1000, UTLMEMORYPOOL_GROW_SLOW );
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern int EconWear_ToIntCategory( float flWear );
/*static*/ const schema_attribute_stat_bucket_t *CSchemaAttributeStats::m_pHead;

//-----------------------------------------------------------------------------
// Purpose: Utility function to convert datafile strings to ints.
//-----------------------------------------------------------------------------
int StringFieldToInt( const char *szValue, const char **pValueStrings, int iNumStrings, bool bDontAssert ) 
{
	if ( !szValue || !szValue[0] )
		return -1;

	for ( int i = 0; i < iNumStrings; i++ )
	{
		if ( !Q_stricmp(szValue, pValueStrings[i]) )
			return i;
	}

	if ( !bDontAssert )
	{
		Assert( !"Missing value in StringFieldToInt()!" );
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Utility function to convert datafile strings to ints.
//-----------------------------------------------------------------------------
int StringFieldToInt( const char *szValue, const CUtlVector<const char *>& vecValueStrings, bool bDontAssert )
{
	return StringFieldToInt( szValue, (const char **)&vecValueStrings[0], vecValueStrings.Count(), bDontAssert );
}

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
CEconItem::CEconItem()
	: BaseClass( )
	, m_pCustomData( NULL )
	, m_ulID( INVALID_ITEM_ID )
	, m_unStyle( 0 )
	, m_pszSmallIcon( NULL )
	, m_pszLargeIcon( NULL )
{
	Init();
}

CEconItem::CEconItem( const CEconItem& rhs )
	: BaseClass( )
	, m_pCustomData( NULL )
	, m_ulID( INVALID_ITEM_ID )
	, m_unStyle( 0 )
	, m_pszSmallIcon( NULL )
	, m_pszLargeIcon( NULL )
{
	Init();
	(*this) = rhs;
}

void CEconItem::Init()
{
	memset( &m_dirtyBits, 0, sizeof( m_dirtyBits ) );

#ifdef GC
	// to set defaults
	CSchItem item;
	item.m_ulID = INVALID_ITEM_ID;
	DeserializeFromSchemaItem( item );

	COMPILE_TIME_ASSERT( sizeof( m_ulID ) == sizeof( item.m_ulID ) );
	COMPILE_TIME_ASSERT( sizeof( m_unAccountID ) == sizeof( item.m_unAccountID ) );
	COMPILE_TIME_ASSERT( sizeof( m_unDefIndex ) == sizeof( item.m_unDefIndex ) );
	COMPILE_TIME_ASSERT( sizeof( m_unLevel ) == sizeof( item.m_unLevel ) );
	COMPILE_TIME_ASSERT( sizeof( m_nQuality ) == sizeof( item.m_nQuality ) );
	COMPILE_TIME_ASSERT( sizeof( m_unInventory ) == sizeof( item.m_unInventory ) );
	COMPILE_TIME_ASSERT( sizeof( m_unFlags ) == sizeof( item.m_unFlags ) );
	COMPILE_TIME_ASSERT( sizeof( m_unOrigin ) == sizeof( item.m_unOrigin ) );
	COMPILE_TIME_ASSERT( sizeof( m_unStyle ) == sizeof( item.m_unStyle ) );
	// @note (Tom Bui): we need to know about any new fields
	// Need to add new fields to:
	// CEconItem::operator=
	// CEconItem::SerializeToSchemaItem
	// CEconItem::DeserializeFromSchemaItem
	// CEconItem::SerializeToProtoBufItem
	// CEconItem::DeserializeFromProtoBufItem
	// CEconManager::BYieldingLoadSOCache
	// CEconGetPlayerItemsJob::BYieldingHandleGetPlayerItemsV001
	COMPILE_TIME_ASSERT( CSchItem::k_iFieldMax == 13 );

	m_bEquippedThisGameServerSession = false;
#endif
}

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
CEconItem::~CEconItem()
{
	// Free up any memory we may have allocated for our singleton attribute. Any other attributes
	// will be cleaned up as part of freeing the custom data object itself.
	if ( m_dirtyBits.m_bHasAttribSingleton )
	{
		CEconItemCustomData::FreeAttributeMemory( &m_CustomAttribSingleton );
	}

	// Free up any custom data we may have allocated. This will catch any attributes not
	// in our singleton.
	if ( m_pCustomData )
	{
		delete m_pCustomData;
	}
}

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
CEconItemCustomData::~CEconItemCustomData()
{
	FOR_EACH_VEC( m_vecAttributes, i )
	{
		FreeAttributeMemory( &m_vecAttributes[i] );
	}

	if ( m_pInteriorItem )
	{
		delete m_pInteriorItem;
	}
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::CopyAttributesFrom( const CEconItem& source )
{
	// Copy attributes -- each new instance needs to be allocated and then copied into by somewhere
	// that knows what the actual type is. Rather than do anything type-specific here, we just have each
	// attribute serialize it's value to a bytestream and then deserialize it. This is as safe as we can
	// make it but sort of silly wasteful.
	for ( int i = 0; i < source.GetDynamicAttributeCountInternal(); i++ )
	{
		const attribute_t& attr = source.GetDynamicAttributeInternal( i );

		const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( attr.m_unDefinitionIndex );
		Assert( pAttrDef );

		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );

		std::string sBytes;
		pAttrType->ConvertEconAttributeValueToByteStream( attr.m_value, &sBytes );
		pAttrType->LoadByteStreamToEconAttributeValue( this, pAttrDef, sBytes );
	}
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
CEconItem &CEconItem::operator=( const CEconItem& rhs )
{
	// We do destructive operations on our local object, including freeing attribute memory, as part of
	// the copy, so we force self-copies to be a no-op.
	if ( &rhs == this )
		return *this;

	m_ulID = rhs.m_ulID;
	SetOriginalID( rhs.GetOriginalID() );
	m_unAccountID = rhs.m_unAccountID;
	m_unDefIndex = rhs.m_unDefIndex;
	m_unLevel = rhs.m_unLevel;
	m_nQuality = rhs.m_nQuality;
	m_unInventory = rhs.m_unInventory;
	SetQuantity( rhs.GetQuantity() );
	m_unFlags = rhs.m_unFlags;
	m_unOrigin = rhs.m_unOrigin;
	m_unStyle = rhs.m_unStyle;
	m_EquipInstanceSingleton = rhs.m_EquipInstanceSingleton;

	// If we have memory allocated for a single attribute we free it manually.
	if ( m_dirtyBits.m_bHasAttribSingleton )
	{
		CEconItemCustomData::FreeAttributeMemory( &m_CustomAttribSingleton );
	}

	// Copy over our dirty bits but manually reset our attribute singleton state -- if we did have one,
	// we just deleted it above (and might replace it below); if we didn't have one, this won't affect
	// anything. Either way, because we have no attribute memory allocated at this point, we need this
	// to be reflected in the dirty bits so that if we do copy attributes, we copy them into the correct
	// place (either the singleton or the custom data, to be allocated later).
	m_dirtyBits = rhs.m_dirtyBits;
	m_dirtyBits.m_bHasAttribSingleton = false;

	// Free any custom memory we've allocated. This will also remove any custom attributes.
	if ( rhs.m_pCustomData == NULL )
	{
		delete m_pCustomData;
		m_pCustomData = NULL;
	}
	else
	{
		// Check for and copy in the equip instances from CustomData
		EnsureCustomDataExists();	
		m_pCustomData->m_vecEquipped = rhs.m_pCustomData->m_vecEquipped;
	}

	CopyAttributesFrom( rhs );

	// Reset our material overrides, they'll be set again on demand as needed.
	ResetMaterialOverrides();

	return *this;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::SetItemID( itemid_t ulID ) 
{
	uint64 ulOldID = m_ulID;
	m_ulID = ulID;
	// only overwrite if we don't have an original id currently and we are a new item cloned off an old item
	if ( ulOldID != INVALID_ITEM_ID && ulOldID != ulID && ( m_pCustomData == NULL || m_pCustomData->m_ulOriginalID == INVALID_ITEM_ID ) && ulID != INVALID_ITEM_ID && ulOldID != INVALID_ITEM_ID )
	{
		SetOriginalID( ulOldID );
	}

	ResetMaterialOverrides();	
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
itemid_t CEconItem::GetOriginalID() const
{
	if ( m_pCustomData != NULL && m_pCustomData->m_ulOriginalID != INVALID_ITEM_ID )
		return m_pCustomData->m_ulOriginalID; 
	return m_ulID;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::SetOriginalID( itemid_t ulOriginalID )
{
	if ( ulOriginalID != m_ulID )
	{
		EnsureCustomDataExists();
		m_pCustomData->m_ulOriginalID = ulOriginalID;
	}
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
int CEconItem::GetQuantity() const
{
	if ( m_pCustomData != NULL )
		return m_pCustomData->m_unQuantity;
	return 1;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::SetQuantity( uint16 unQuantity )
{
	if ( m_pCustomData )
	{
		m_pCustomData->m_unQuantity = unQuantity;
	}
	else if ( unQuantity > 1 )
	{
		EnsureCustomDataExists();
		m_pCustomData->m_unQuantity = unQuantity;
	}
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
static const char *GetCustomNameOrAttributeDesc( const CEconItem *pItem, const CEconItemAttributeDefinition *pAttrDef )
{
	if ( !pAttrDef )
	{
		// If we didn't specify the attribute in the schema we can't possibly have an
		// answer. This isn't really an error in that case.
		return NULL;
	}

	const char *pszStrContents;
	if ( FindAttribute_UnsafeBitwiseCast<CAttribute_String>( pItem, pAttrDef, &pszStrContents ) )
		return pszStrContents;

	return NULL;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
static void SetCustomNameOrDescAttribute( CEconItem *pItem, const CEconItemAttributeDefinition *pAttrDef, const char *pszNewValue )
{
	Assert( pItem );

	if ( !pAttrDef )
	{
		// If we didn't specify the attribute in the schema, that's fine if we're setting
		// the empty name/description string, but it isn't fine if we're trying to set
		// actual content.
		AssertMsg( !pszNewValue, "Attempt to set non-empty value for custom name/desc with no attribute present." );
		return;
	}

	// Removing existing value?
	if ( !pszNewValue || !pszNewValue[0] )
	{
		pItem->RemoveDynamicAttribute( pAttrDef );
		return;
	}

	CAttribute_String attrStr;
	attrStr.set_value( pszNewValue );

	pItem->SetDynamicAttributeValue( pAttrDef, attrStr );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
const char *CEconItem::GetCustomName() const
{
	static CSchemaAttributeDefHandle pAttrDef_CustomName( "custom name attr" );

	return GetCustomNameOrAttributeDesc( this, pAttrDef_CustomName );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::SetCustomName( const char *pName )
{
	static CSchemaAttributeDefHandle pAttrDef_CustomName( "custom name attr" );

	SetCustomNameOrDescAttribute( this, pAttrDef_CustomName, pName );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItem::IsEquipped() const
{
	for ( int i = 0; i < GetEquippedInstanceCount(); i++ )
	{
		const EquippedInstance_t &curEquipInstance = GetEquippedInstance( i );
		Assert( curEquipInstance.m_unEquippedSlot != INVALID_EQUIPPED_SLOT );

		if ( GetItemSchema()->IsValidClass( curEquipInstance.m_unEquippedClass ) )
			return true;
	}

	return false;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItem::IsEquippedForClass( equipped_class_t unClass ) const
{
	return NULL != FindEquippedInstanceForClass( unClass );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
equipped_slot_t CEconItem::GetEquippedPositionForClass( equipped_class_t unClass ) const
{
	const EquippedInstance_t *pInstance = FindEquippedInstanceForClass( unClass );
	if ( pInstance )
		return pInstance->m_unEquippedSlot;

	return INVALID_EQUIPPED_SLOT;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
const CEconItem::EquippedInstance_t *CEconItem::FindEquippedInstanceForClass( equipped_class_t nClass ) const
{
	for ( int i = 0; i < GetEquippedInstanceCount(); i++ )
	{
		const EquippedInstance_t &curEquipInstance = GetEquippedInstance( i );
		if ( curEquipInstance.m_unEquippedClass == nClass )
			return &curEquipInstance;
	}

	return NULL;
}



//----------------------------------------------------------------------------
// Purpose:
//----------------------------------------------------------------------------
void CEconItem::InternalVerifyEquipInstanceIntegrity() const
{
	if ( m_dirtyBits.m_bHasEquipSingleton )
	{
		Assert( !m_pCustomData );
		Assert( m_EquipInstanceSingleton.m_unEquippedSlot != INVALID_EQUIPPED_SLOT );
	}
	else if ( m_pCustomData )
	{
		FOR_EACH_VEC( m_pCustomData->m_vecEquipped, i )
		{
			Assert( m_pCustomData->m_vecEquipped[i].m_unEquippedSlot != INVALID_EQUIPPED_SLOT );

			for ( int j = i + 1; j < m_pCustomData->m_vecEquipped.Count(); j++ )
			{
				Assert( m_pCustomData->m_vecEquipped[i].m_unEquippedClass != m_pCustomData->m_vecEquipped[j].m_unEquippedClass );
			}
		}
	}
	else
	{
		Assert( GetEquippedInstanceCount() == 0 );
	}
}

//----------------------------------------------------------------------------
// Purpose:
//----------------------------------------------------------------------------
void CEconItem::Equip( equipped_class_t unClass, equipped_slot_t unSlot )
{
	Assert( GetItemSchema()->IsValidClass( unClass ) );
	Assert( GetItemSchema()->IsValidItemSlot( unSlot, unClass ) );

	// First, make sure we don't have this item already equipped for this class.
	UnequipFromClass( unClass );

	// If we have no instances of this item equipped, we want to shove this into the
	// first empty slot we can find. If we already have a custom data allocated, we
	// use that. If not, we want to use the singleton if we can. Otherwise, we make
	// a new custom data and fall back to using that.
	if ( m_pCustomData )
	{
		m_pCustomData->m_vecEquipped.AddToTail( EquippedInstance_t( unClass, unSlot ) );
	}
	else if ( !m_dirtyBits.m_bHasEquipSingleton )
	{
		m_EquipInstanceSingleton = EquippedInstance_t( unClass, unSlot );
		m_dirtyBits.m_bHasEquipSingleton = true;
	}
	else
	{
		EnsureCustomDataExists();
		m_pCustomData->m_vecEquipped.AddToTail( EquippedInstance_t( unClass, unSlot ) );
	}

	InternalVerifyEquipInstanceIntegrity();

#ifdef GC_DLL
	m_bEquippedThisGameServerSession = true;
#endif // GC_DLL

}

//----------------------------------------------------------------------------
// Purpose:
//----------------------------------------------------------------------------
void CEconItem::Unequip()
{
	if ( m_dirtyBits.m_bHasEquipSingleton )
	{
		Assert( !m_pCustomData );
		m_dirtyBits.m_bHasEquipSingleton = false;
	}
	else if ( m_pCustomData )
	{
		m_pCustomData->m_vecEquipped.Purge();
	}

	InternalVerifyEquipInstanceIntegrity();
}

//----------------------------------------------------------------------------
// Purpose:
//----------------------------------------------------------------------------
void CEconItem::UnequipFromClass( equipped_class_t unClass )
{
	Assert( GetItemSchema()->IsValidClass( unClass ) );

	// If we only have a single equipped class...
	if ( m_dirtyBits.m_bHasEquipSingleton )
	{
		// ...and that's the class we're trying to remove from...
		if ( m_EquipInstanceSingleton.m_unEquippedClass == unClass )
		{
			// ...we now have no equipped classes!
			m_dirtyBits.m_bHasEquipSingleton = false;
		}
	}
	else if ( m_pCustomData )
	{
		// ...otherwise, if we have multiple equipped classes...
		FOR_EACH_VEC( m_pCustomData->m_vecEquipped, i )
		{
			// ...then look through our list to find out if we have this class...
			if ( m_pCustomData->m_vecEquipped[i].m_unEquippedClass == unClass )
			{
				// ...and if we do, remove it.
				m_pCustomData->m_vecEquipped.FastRemove( i );
				break;
			}
		}
	}

	InternalVerifyEquipInstanceIntegrity();
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
int CEconItem::GetEquippedInstanceCount() const
{
	if ( m_pCustomData )
		return m_pCustomData->m_vecEquipped.Count();
	else 
		return m_dirtyBits.m_bHasEquipSingleton ? 1 : 0;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
const CEconItem::EquippedInstance_t &CEconItem::GetEquippedInstance( int iIdx ) const
{
	Assert( iIdx >= 0  && iIdx < GetEquippedInstanceCount() );

	if ( m_pCustomData )
		return m_pCustomData->m_vecEquipped[iIdx];
	else
		return m_EquipInstanceSingleton;
}
// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
const char *CEconItem::GetCustomDesc() const
{
	static CSchemaAttributeDefHandle pAttrDef_CustomDesc( "custom desc attr" );

	return GetCustomNameOrAttributeDesc( this, pAttrDef_CustomDesc );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::SetCustomDesc( const char *pDesc )
{
	static CSchemaAttributeDefHandle pAttrDef_CustomDesc( "custom desc attr" );

	SetCustomNameOrDescAttribute( this, pAttrDef_CustomDesc, pDesc );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItem::GetInUse() const
{
	return ( m_dirtyBits.m_bInUse ) != 0;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::SetInUse( bool bInUse )
{
	if ( bInUse )
	{
		m_dirtyBits.m_bInUse = 1;
	}
	else
	{
		m_dirtyBits.m_bInUse = 0;
	}
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
const GameItemDefinition_t *CEconItem::GetItemDefinition() const
{
	const CEconItemDefinition  *pRet	  = GetItemSchema()->GetItemDefinition( GetDefinitionIndex() );
	const GameItemDefinition_t *pTypedRet = dynamic_cast<const GameItemDefinition_t *>( pRet );

	AssertMsg( pRet == pTypedRet, "Item definition of inappropriate type." );

	return pTypedRet;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItem::IsTradable() const
{
	return !m_dirtyBits.m_bInUse 
		&& IEconItemInterface::IsTradable();
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::AdoptMoreRestrictedTradabilityFromItem( const CEconItem *pOther, uint32 nTradabilityFlagsToAccept /*= 0xFFFFFFFF*/ )
{
	if ( !pOther )
		return;

	int nOtherUntradability = pOther->GetUntradabilityFlags() & nTradabilityFlagsToAccept;
	RTime32 otherUntradableTime = pOther->GetTradableAfterDateTime();
	// Become untradable if the other item is untradable
	AdoptMoreRestrictedTradability( nOtherUntradability, otherUntradableTime );
}

// --------------------------------------------------------------------------
// Purpose:	Given untradability flags and a untradable time, set this item's
//			untradability.  This does not clear existing untradabilty.
// --------------------------------------------------------------------------
void CEconItem::AdoptMoreRestrictedTradability( uint32 nTradabilityFlags, RTime32 nUntradableTime )
{
	static CSchemaAttributeDefHandle pAttrib_CannotTrade( "cannot trade" );
	static CSchemaAttributeDefHandle pAttrib_TradableAfter( "tradable after date" );

	if ( !pAttrib_CannotTrade || !pAttrib_TradableAfter )
		return;

	// We're already permanently untradable.  We can't get more untradable, so we're done.
	if ( GetUntradabilityFlags() & k_Untradability_Permanent )
		return;

	if( nTradabilityFlags & k_Untradability_Permanent )
	{
		SetDynamicAttributeValue( pAttrib_CannotTrade, 0u );
	}
	else if ( nTradabilityFlags & k_Untradability_Temporary && nUntradableTime > GetTradableAfterDateTime() )
	{
		// Take the "tradable after date" if it's larger than ours
		SetDynamicAttributeValue( pAttrib_TradableAfter, nUntradableTime );
	}
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItem::IsMarketable() const
{
	return !m_dirtyBits.m_bInUse
		&& IEconItemInterface::IsMarketable();
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItem::IsCommodity() const
{
	return !m_dirtyBits.m_bInUse
		&& IEconItemInterface::IsCommodity();
}

void CEconItem::IterateAttributes( IEconItemAttributeIterator *pIterator ) const
{
	Assert( pIterator );

	// custom attributes?
	for ( int i = 0; i < GetDynamicAttributeCountInternal(); i++ )
	{
		const attribute_t &attrib = GetDynamicAttributeInternal( i );
		const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( attrib.m_unDefinitionIndex );
		if ( !pAttrDef )
			continue;

		if ( !pAttrDef->GetAttributeType()->OnIterateAttributeValue( pIterator, pAttrDef, attrib.m_value ) )
			return;
	}

	// in static attributes?
	const CEconItemDefinition *pItemDef = GetItemDefinition();
	if ( !pItemDef )
		return;

	pItemDef->IterateAttributes( pIterator );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
style_index_t CEconItem::GetStyle() const
{
	static CSchemaAttributeDefHandle pAttrDef_ItemStyleOverride( "item style override" );
	float fStyleOverride = 0.f;
	if ( FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, pAttrDef_ItemStyleOverride, &fStyleOverride ) )
	{
		return fStyleOverride;
	}

	static CSchemaAttributeDefHandle pAttrDef_ItemStyleStrange( "style changes on strange level" );
	uint32 iMaxStyle = 0;
	if ( pAttrDef_ItemStyleStrange && FindAttribute( pAttrDef_ItemStyleStrange, &iMaxStyle ) )
	{
		// Use the strange prefix if the weapon has one.
		uint32 unScore = 0;
		if ( !FindAttribute( GetKillEaterAttr_Score( 0 ), &unScore ) )
			return 0;

		// What type of event are we tracking and how does it describe itself?
		uint32 unKillEaterEventType = 0;
		// This will overwrite our default 0 value if we have a value set but leave it if not.
		float fKillEaterEventType;
		if ( FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, GetKillEaterAttr_Type( 0 ), &fKillEaterEventType ) )
		{
			unKillEaterEventType = fKillEaterEventType;
		}

		const char *pszLevelingDataName = GetItemSchema()->GetKillEaterScoreTypeLevelingDataName( unKillEaterEventType );
		if ( !pszLevelingDataName )
		{
			pszLevelingDataName = KILL_EATER_RANK_LEVEL_BLOCK_NAME;
		}

		const CItemLevelingDefinition *pLevelDef = GetItemSchema()->GetItemLevelForScore( pszLevelingDataName, unScore );
		if ( !pLevelDef )
			return 0;

		return Min( pLevelDef->GetLevel(), iMaxStyle );
	}

	return m_unStyle;
}

const char* CEconItem::FindIconURL( bool bLarge ) const
{
	const char* pszSize = bLarge ? "l" : "s";

	static CSchemaAttributeDefHandle pAttrDef_IsFestivized( "is_festivized" );
	bool bIsFestivized = pAttrDef_IsFestivized ? FindAttribute( pAttrDef_IsFestivized ) : false;

	const CEconItemDefinition *pDef = GetItemDefinition();

	// Go through and figure out all the different decorations on
	// this item and construct the key to lookup the icon.
	// NOTE:  These are not currently composable, so they return out when
	//		  a match is found.  Once items are more composable, we'll want
	//		  to keep adding all the components together to get the fully
	//		  composed icon (ie. add the strange token, and the festive token, etc.)
	const CEconItemPaintKitDefinition* pPaintKitDef = pDef->GetCustomPainkKitDefinition();
	if ( pPaintKitDef )
	{
		float flWear = 0;
		GetCustomPaintKitWear( flWear );
		int iWearIndex = EconWear_ToIntCategory( flWear );
		const char* pszFmtStr = bIsFestivized ? "%s%sw%df" : "%s%sw%d";

		const char* pszValue = pDef->GetIconURL( CFmtStr( pszFmtStr, pszSize, pPaintKitDef->GetName(), iWearIndex ) );
		if ( pszValue )
			return pszValue;
	}

	const CEconStyleInfo *pStyle = pDef->GetStyleInfo( GetStyle() );
	if ( pStyle )
	{
		const char* pszValue = pDef->GetIconURL( CFmtStr( "%ss%d", pszSize, GetStyle() ) );
		if ( pszValue )
			return pszValue;
	}

	if ( bIsFestivized )
	{
		const char* pszValue = pDef->GetIconURL( CFmtStr( "%sf", pszSize ) );
		if ( pszValue )
			return pszValue;
	}

	return pDef->GetIconURL( CFmtStr( "%s", pszSize ) );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
const char *CEconItem::GetIconURLSmall() const
{
	if ( m_pszSmallIcon == NULL )
	{
		m_pszSmallIcon = FindIconURL( false );
	}

	return m_pszSmallIcon;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
const char *CEconItem::GetIconURLLarge() const
{
	if ( m_pszLargeIcon == NULL )
	{
		m_pszLargeIcon = FindIconURL( true );
	}

	return m_pszLargeIcon;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItem::IsUsableInCrafting() const
{
	return !m_dirtyBits.m_bInUse
		&& IEconItemInterface::IsUsableInCrafting();
}

#ifdef GC_DLL
// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
RTime32 CEconItem::GetAssetInfoExpirationCacheExpirationTime() const
{
	return GetTradableAfterDateTime();
}
#endif // GC_DLL

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
int CEconItem::GetDynamicAttributeCountInternal() const
{
	if ( m_pCustomData )
		return m_pCustomData->m_vecAttributes.Count();
	else
		return m_dirtyBits.m_bHasAttribSingleton ? 1 : 0;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
CEconItem::attribute_t &CEconItem::GetMutableDynamicAttributeInternal( int iAttrIndexIntoArray )
{
	Assert( iAttrIndexIntoArray >= 0 );
	Assert( iAttrIndexIntoArray < GetDynamicAttributeCountInternal() );

	if ( m_pCustomData )
		return m_pCustomData->m_vecAttributes[ iAttrIndexIntoArray ];
	else
		return m_CustomAttribSingleton;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
CEconItem::attribute_t *CEconItem::FindDynamicAttributeInternal( const CEconItemAttributeDefinition *pAttrDef )
{
	Assert( pAttrDef );

	if ( m_pCustomData )
	{
		FOR_EACH_VEC( m_pCustomData->m_vecAttributes, i )
		{
			if ( m_pCustomData->m_vecAttributes[i].m_unDefinitionIndex == pAttrDef->GetDefinitionIndex() )
				return &m_pCustomData->m_vecAttributes[i];
		}
	}
	else if ( m_dirtyBits.m_bHasAttribSingleton )
	{
		if ( m_CustomAttribSingleton.m_unDefinitionIndex == pAttrDef->GetDefinitionIndex() )
			return &m_CustomAttribSingleton;
	}

	return NULL;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
CEconItem::attribute_t &CEconItem::AddDynamicAttributeInternal()
{
	if ( 0 == GetDynamicAttributeCountInternal() && NULL == m_pCustomData )
	{
		m_dirtyBits.m_bHasAttribSingleton = true;
		return m_CustomAttribSingleton;
	}
	else
	{
		EnsureCustomDataExists();
		return m_pCustomData->m_vecAttributes[ m_pCustomData->m_vecAttributes.AddToTail() ];
	}
}

// --------------------------------------------------------------------------
void CEconItem::SetDynamicMaxTimeAttributeValue( const CEconItemAttributeDefinition *pAttrDef, RTime32 rtTime )
{
	RTime32 rtExistingTime = 0;
	if ( FindAttribute( pAttrDef, &rtExistingTime ) )
	{
		//we have the attribute already, and see if the value exceeds what we are going to set
		if ( rtExistingTime >= rtTime )
			return;
	}

	//it doesn't so we need to update
	SetDynamicAttributeValue( pAttrDef, rtTime );
}

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
void CEconItem::SetTradableAfterDateTime( RTime32 rtTime )
{
	//don't bother if the time is in the past (this also covers the 0 case)
	if( rtTime < CRTime::RTime32TimeCur() )
		return;

	//the attribute we are going to assign
	static CSchemaAttributeDefHandle pAttrib_TradableAfter( "tradable after date" );
	if( !pAttrib_TradableAfter )
		return;

	//see if we have a STATIC cannot trade attribute (ignore dynamic, because that could change and be used
	// to short out the trade restriction). 

	//This is currently disabled so we can measure whether or not this is beneficial and if the savings justifies the corner case risk this exposes - JohnO 1/12/15
	/*
	const GameItemDefinition_t* pItemDef = GetItemDefinition();
	if( pItemDef )
	{
		static CSchemaAttributeDefHandle pAttrib_CannotTrade( "cannot trade" );
		uint32 unCannotTrade = 0;
		if( ::FindAttribute( pItemDef, pAttrib_CannotTrade, &unCannotTrade ) )
		{
			return;
		}
	}
	*/

	//now set it to the maximum time
	SetDynamicMaxTimeAttributeValue( pAttrib_TradableAfter, rtTime );
}

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
void CEconItem::RemoveDynamicAttribute( const CEconItemAttributeDefinition *pAttrDef )
{
	Assert( pAttrDef );
	Assert( pAttrDef->GetDefinitionIndex() != INVALID_ATTRIB_DEF_INDEX );

	if ( m_pCustomData )
	{
		for ( int i = 0; i < m_pCustomData->m_vecAttributes.Count(); i++ )
		{
			if ( m_pCustomData->m_vecAttributes[i].m_unDefinitionIndex == pAttrDef->GetDefinitionIndex() )
			{
				CEconItemCustomData::FreeAttributeMemory( &m_pCustomData->m_vecAttributes[i] );
				m_pCustomData->m_vecAttributes.FastRemove( i );
				return;
			}
		}
	}
	else if ( m_dirtyBits.m_bHasAttribSingleton )
	{
		if ( m_CustomAttribSingleton.m_unDefinitionIndex == pAttrDef->GetDefinitionIndex() )
		{
			CEconItemCustomData::FreeAttributeMemory( &m_CustomAttribSingleton );
			m_dirtyBits.m_bHasAttribSingleton = false;
		}
	}
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
/*static*/ void CEconItemCustomData::FreeAttributeMemory( CEconItem::attribute_t *pAttrib )
{
	Assert( pAttrib );

	const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( pAttrib->m_unDefinitionIndex );
	Assert( pAttrDef );

	const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
	Assert( pAttrType );

	pAttrType->UnloadEconAttributeValue( &pAttrib->m_value );
}

// --------------------------------------------------------------------------
// Purpose: Frees any unused memory in the internal structures
// --------------------------------------------------------------------------
void CEconItem::Compact()
{
	if ( m_pCustomData )
	{
		m_pCustomData->m_vecAttributes.Compact();
		m_pCustomData->m_vecEquipped.Compact();
	}
}

#ifdef GC_DLL
// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
static bool BInsertEquippedInstanceSQL( const CEconItem *pItem, const CEconItem::EquippedInstance_t& equipInst, CSQLAccess& sqlAccess )
{
	Assert( pItem );

	const char *pszUpdateOrInsert = "MERGE EquipInstance AS tblEquipInstance "
									"USING (SELECT ? AS AccountID, ? AS ClassID, ? AS SlotID) AS tblEquipInstance_NewRow "
									"ON (tblEquipInstance.AccountID = tblEquipInstance_NewRow.AccountID AND "
										"tblEquipInstance.ClassID = tblEquipInstance_NewRow.ClassID AND "
										"tblEquipInstance.SlotID = tblEquipInstance_NewRow.SlotID) "
									"WHEN MATCHED THEN "
										"UPDATE SET tblEquipInstance.ItemID = ? "
									"WHEN NOT MATCHED BY TARGET THEN "
										"INSERT (AccountID, ClassID, SlotID, ItemID) VALUES (?, ?, ?, ?);";

	sqlAccess.AddBindParam( pItem->GetAccountID() );			// USING (SELECT ... AS AccountID)
	sqlAccess.AddBindParam( equipInst.m_unEquippedClass );		// USING (SELECT ... AS ClassID)
	sqlAccess.AddBindParam( equipInst.m_unEquippedSlot );		// USING (SELECT ... AS SlotID)
	sqlAccess.AddBindParam( pItem->GetItemID() );				// UPDATE SET ...

	sqlAccess.AddBindParam( pItem->GetAccountID() );			// INSERT (AccountID)
	sqlAccess.AddBindParam( equipInst.m_unEquippedClass );		// INSERT (ClassID)
	sqlAccess.AddBindParam( equipInst.m_unEquippedSlot );		// INSERT (SlotID)
	sqlAccess.AddBindParam( pItem->GetItemID() );				// INSERT (ItemID)

	return sqlAccess.BYieldingExecute( "BYieldingOnHandledPeriodicScoreTimePeriod", pszUpdateOrInsert );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
static bool BInsertAllEquippedInstancesSQL( const CEconItem *pItem, CSQLAccess& sqlAccess )
{
	Assert( pItem );

	for ( int i = 0; i < pItem->GetEquippedInstanceCount(); i++ )
	{
		if ( !BInsertEquippedInstanceSQL( pItem, pItem->GetEquippedInstance( i ), sqlAccess ) )
			return false;
	}

	return true;
}

// --------------------------------------------------------------------------
// Purpose: Adds non-Item-table inserts to the SQL insert for this object
// --------------------------------------------------------------------------
bool CEconItem::BYieldingAddInsertToTransaction( CSQLAccess & sqlAccess )
{
	// @note Tom Bui: This could be simplified greatly, but let's see if it is an issue
	CSchItem item;
	SerializeToSchemaItem( item );
	if( !CSchemaSharedObjectHelper::BYieldingAddInsertToTransaction( sqlAccess, &item ) )
		return false;

	// attributes get to written to any number of joined tables based on type
	for ( int i = 0; i < GetDynamicAttributeCountInternal(); i++ )
	{
		const attribute_t &attrib = GetDynamicAttributeInternal( i );

		const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( attrib.m_unDefinitionIndex );
		if ( !pAttrDef )
		{
			EmitError( SPEW_GC, "CEconItem::BYieldingAddInsertToTransaction(): attempt to insert unknown attribute ID %d.\n", attrib.m_unDefinitionIndex );
			return false;
		}

		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );

		CPlainAutoPtr<CRecordBase> pAttrRecord( pAttrType->CreateTypedSchRecord() );
		pAttrType->ConvertEconAttributeValueToSch( GetItemID(), pAttrDef, attrib.m_value, pAttrRecord.Get() );
		if( !sqlAccess.BYieldingInsertRecord( pAttrRecord.Get() ) )
			return false;
	}

	// currently-equipped positions get written to a joined table
	if ( !BInsertAllEquippedInstancesSQL( this, sqlAccess ) )
		return false;

	// add audit record for the create
	CEconItemDefinition *pItemDef = GEconManager()->GetItemSchema()->GetItemDefinition( GetDefinitionIndex() );
	if ( pItemDef )
	{
		const char* pDatabaseAuditTableName = pItemDef->GetDatabaseAuditTableName();
		if ( pDatabaseAuditTableName )
		{
			CFmtStr1024 sStatement;
			sStatement.sprintf( "INSERT INTO %s (ItemID) VALUES (%llu)", pDatabaseAuditTableName, m_ulID );
			uint32 nRows;
			bool bRet = sqlAccess.BYieldingExecute( sStatement, sStatement, &nRows );
			if ( bRet == false )
			{
				CSteamID steamID( m_unAccountID, GGCHost()->GetUniverse(), k_EAccountTypeIndividual );
				EmitError( SPEW_GC, "Failed to add item audit to table %s for %s and item %llu.\n", pDatabaseAuditTableName, steamID.Render(), m_ulID );
				return false;
			}
		}
	}

	return true;
}

//----------------------------------------------------------------------------
// Purpose:
//----------------------------------------------------------------------------
static bool BRemoveEquipInstancesFromSQL( CSQLAccess& sqlAccess, uint32 unAccountID, itemid_t unItemID )
{
	CSchEquipInstance schEquipInstanceWhere;
	schEquipInstanceWhere.m_unAccountID = unAccountID;
	schEquipInstanceWhere.m_ulItemID = unItemID;

	return sqlAccess.BYieldingDeleteRecords( schEquipInstanceWhere, CSET_2_COL( CSchEquipInstance, k_iField_unAccountID, k_iField_ulItemID ) );
}

//----------------------------------------------------------------------------
// Purpose: Writes the non-PK fields on the object to the database
//----------------------------------------------------------------------------
bool CEconItem::BYieldingAddWriteToTransaction( CSQLAccess & sqlAccess, const CUtlVector< int > &fields ) 
{
	Assert( sqlAccess.BInTransaction() );

	if( IsForeign() )
	{
		CSchForeignItem schForeignItem;
		schForeignItem.m_ulID = GetItemID();
		schForeignItem.m_unInventory = GetInventoryToken();
		return CSchemaSharedObjectHelper::BYieldingAddWriteToTransaction( sqlAccess, &schForeignItem, CSET_1_COL( CSchForeignItem, k_iField_unInventory ) );
	}

	// Non-foreign items.
	CSchItem item;
	SerializeToSchemaItem( item );
	CColumnSet csDatabaseDirty( item.GetPRecordInfo() );
	GetDirtyColumnSet( fields, csDatabaseDirty );

	// Write out base item properties if any are dirty. It's possible to get here with only dirty attributes,
	// in which case we want to neither write to our item properties nor abort here if the write fails.
	if ( !csDatabaseDirty.IsEmpty() && !CSchemaSharedObjectHelper::BYieldingAddWriteToTransaction( sqlAccess, &item, csDatabaseDirty ) )
		return false;
	
	// Write out additional attribute properties?
	FOR_EACH_VEC( fields, i )
	{
		uint32 iFieldID		= fields[i] & 0x0000ffff,	// low 16 bits are the ID
			   iFieldIDType = fields[i] & 0xffff0000;	// high 16 bits are the ID type

		if ( iFieldIDType == kUpdateFieldIDType_FieldID )
		{
			// Most item field IDs will be updated above in SerializeToSchemaItem(). Anything that needs custom
			// writing behavior should handle it here.

			// If we've changed our equip state, remove all of our previous equipped instances (if any) and write
			// out our new rows (if any).
			if ( iFieldID == CSOEconItem::kEquippedStateFieldNumber )
			{
				if ( !BRemoveEquipInstancesFromSQL( sqlAccess, GetAccountID(), GetID() ) )
					return false;

				if ( !BInsertAllEquippedInstancesSQL( this, sqlAccess ) )
					return false;
			}
		}
		else if ( iFieldIDType == kUpdateFieldIDType_AttributeID )
		{
			const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( iFieldID );
			Assert( pAttrDef );

			// Did we dirty an attribute and then remove it entirely?
			const attribute_t *pEconAttr = FindDynamicAttributeInternal( pAttrDef );
			if ( !pEconAttr )
				continue;

			const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
			Assert( pAttrType );

			CPlainAutoPtr<CRecordBase> pAttrRecord( pAttrType->CreateTypedSchRecord() );
			pAttrType->ConvertEconAttributeValueToSch( GetItemID(), pAttrDef, pEconAttr->m_value, pAttrRecord.Get() );

			CColumnSet csWhere( pAttrRecord->GetPRecordInfo() );
			csWhere.BAddColumn( 0 );
			csWhere.BAddColumn( 1 );

			CColumnSet csUpdate( CColumnSet::Inverse( csWhere ) );
			Assert( !csUpdate.IsEmpty() );

			if ( !sqlAccess.BYieldingUpdateRecord( *pAttrRecord.Get(), csWhere, csUpdate ) )
				return false;
		}
		// Our field ID is something we don't recognize.
		else
		{
			AssertMsg( false, "Unknown field ID type in CEconItem::BYieldingAddWriteToTransaction()." );
		}
	}

	return true;
}


// --------------------------------------------------------------------------
// Purpose: "deletes" an object by setting its owner to 0.
// --------------------------------------------------------------------------
bool CEconItem::BYieldingAddRemoveToTransaction( CSQLAccess & sqlAccess )
{
	Assert( sqlAccess.BInTransaction() );

	// we never remove econ items in the server
	sqlAccess.AddBindParam( GetItemID() );
	if( !sqlAccess.BYieldingExecute( NULL, "UPDATE Item SET AccountId = 0 WHERE ID = ?" ) )
		return false;

	if ( !BRemoveEquipInstancesFromSQL( sqlAccess, GetAccountID(), GetItemID() ) )
		return false;

	return true;
}

// --------------------------------------------------------------------------
// Purpose: Load the interior item from the database.
// --------------------------------------------------------------------------
bool CEconItem::BYieldingLoadInteriorItem()
{
	static CSchemaAttributeDefHandle pAttribLow( "referenced item id low" );
	static CSchemaAttributeDefHandle pAttribHigh( "referenced item id high" );

	// see if it's already loaded
	if ( m_pCustomData && m_pCustomData->m_pInteriorItem )
		return true;

	// we require the low 32 bits...
	uint32 unAttribValueBitsLow;
	if ( !FindAttribute( pAttribLow, &unAttribValueBitsLow ) )
		return false;

	// ...but default the high 32 bits to 0 if not present
	uint32 unAttribValueBitsHigh = 0;
	FindAttribute( pAttribHigh, &unAttribValueBitsHigh );		// return value ignored

	COMPILE_TIME_ASSERT( sizeof( uint64 ) == sizeof( itemid_t ) );
	uint64 wrappedItemId = ( uint64( unAttribValueBitsHigh ) << 32 ) | uint64( unAttribValueBitsLow );

	// allocate scratch object to try and fill with DB contents
	CEconItem *pNewItem = new CEconItem;
	if ( !pNewItem->BYieldingSerializeFromDatabase( wrappedItemId ) )
	{
		delete pNewItem;
		return false;
	}

	// DB load succeeded -- make sure if we're treating this item ID as an interior item, it isn't
	// currently owned by someone. this is a non-fatal error (it could happen if support rolls back
	// transactions in weird ways) but it probably means we have something weird going on
	if ( pNewItem->GetAccountID() != 0 )
	{
		// ...
	}

	// everything loaded alright so hook our reference up
	EnsureCustomDataExists();
	m_pCustomData->m_pInteriorItem = pNewItem;
	return true;
}


// --------------------------------------------------------------------------
// Purpose: Directly set the interior item
// --------------------------------------------------------------------------
void CEconItem::SetInteriorItem( CEconItem* pInteriorItem )
{
	EnsureCustomDataExists();
	m_pCustomData->m_pInteriorItem = pInteriorItem;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
CEconItem* CEconItem::YieldingGetInteriorItem()
{
	if ( !m_pCustomData || !m_pCustomData->m_pInteriorItem )
	{
		if ( !BYieldingLoadInteriorItem() )
		{
			Assert( !GetInteriorItem() );
		}
	}

	return GetInteriorItem();
}

#endif // GC_DLL

CEconItem* CEconItem::GetInteriorItem()
{
	return m_pCustomData ? m_pCustomData->m_pInteriorItem : NULL;
}

// --------------------------------------------------------------------------
// Purpose: This item has been traded. Give it an opportunity to update any internal
//			properties in response.
// --------------------------------------------------------------------------
void CEconItem::OnTraded( uint32 unTradabilityDelaySeconds )
{
	// if Steam wants us to impose a tradability delay on the item
	if ( unTradabilityDelaySeconds != 0 )
	{
		RTime32 rtTradableAfter = ( ( CRTime::RTime32TimeCur() / k_nSecondsPerDay ) * k_nSecondsPerDay ) + unTradabilityDelaySeconds;
		SetTradableAfterDateTime( rtTradableAfter );
	}
	else
	{
		// If we have a "tradable after date" attribute and we were just traded, remove the date
		// limit as we're obviously past it.
		static CSchemaAttributeDefHandle pAttrib_TradableAfter( "tradable after date" );
		RemoveDynamicAttribute( pAttrib_TradableAfter );
	}

	OnTransferredOwnership();
}

// --------------------------------------------------------------------------
// Purpose: Ownership of this item has changed, so do whatever things are necessary
// --------------------------------------------------------------------------
void CEconItem::OnTransferredOwnership()
{
	// Reset all our strange scores.
	for ( int i = 0; i < GetKillEaterAttrCount(); i++ )
	{
		const CEconItemAttributeDefinition *pAttrDef = GetKillEaterAttr_Score(i);

		// Skip over any attributes our schema doesn't understand. We ideally wouldn't ever
		// have this happen but if it does we don't want to ignore other valid attributes.
		if ( !pAttrDef )
			continue;

		// Ignore any attributes we don't have on this item.
		if ( !FindAttribute( pAttrDef ) )
			continue;

		// Zero out the value of this stat attribute.
		SetDynamicAttributeValue( pAttrDef, 0u );
	}

	// Free accounts have the ability to trade any item out that they received in a trade.
	SetFlag( kEconItemFlag_CanBeTradedByFreeAccounts );
}

// --------------------------------------------------------------------------
// Purpose: This item has been traded. Give it an opportunity to update any internal
//			properties in response.
// --------------------------------------------------------------------------
void CEconItem::OnReceivedFromMarket( bool bFromRollback )
{
	OnTransferredOwnership();

#ifdef GC_DLL
	static CSchemaAttributeDefHandle pAttrib_TradableAfter( "tradable after date" );
	if ( !bFromRollback && GEconManager()->GetTradableAfterDurationForPurchase() != 0 )
	{	
		// Add "tradable after date" attribute for items received from the market, since the funds 
		// used to purchase the item may be untrusted		
		RTime32 rtTradableAfter = GEconManager()->GetTradableAfterDateForPurchase();
		SetDynamicAttributeValue( pAttrib_TradableAfter, rtTradableAfter );
	}
	// Do we need to remove this attribute on rollback?
	/*else
	{
		RemoveDynamicAttribute( pAttrib_TradableAfter );
	}*/
#endif // GC_DLL
}

// --------------------------------------------------------------------------
// Purpose: Parses the bits required to create a econ item from the message. 
//			Overloaded to include support for attributes.
// --------------------------------------------------------------------------
bool CEconItem::BParseFromMessage( const CUtlBuffer & buffer ) 
{
	CSOEconItem msgItem;
	if( !msgItem.ParseFromArray( buffer.Base(), buffer.TellMaxPut() ) )
		return false;

	DeserializeFromProtoBufItem( msgItem );
	return true;
}

// --------------------------------------------------------------------------
// Purpose: Parses the bits required to create a econ item from the message. 
//			Overloaded to include support for attributes.
// --------------------------------------------------------------------------
bool CEconItem::BParseFromMessage( const std::string &buffer ) 
{
	CSOEconItem msgItem;
	if( !msgItem.ParseFromString( buffer ) )
		return false;

	DeserializeFromProtoBufItem( msgItem );
	return true;
}

//----------------------------------------------------------------------------
// Purpose: Overrides all the fields in msgLocal that are present in the 
//			network message
//----------------------------------------------------------------------------
bool CEconItem::BUpdateFromNetwork( const CSharedObject & objUpdate )
{
	const CEconItem & econObjUpdate = (const CEconItem &)objUpdate;

	*this = econObjUpdate;

	return true;
}

#ifdef GC
//----------------------------------------------------------------------------
// Purpose: Adds the relevant bits to update this object to the message. This
//			must include any relevant information about which fields are being
//			updated. This is called once for all subscribers.
//----------------------------------------------------------------------------
bool CEconItem::BAddToMessage( CUtlBuffer & bufOutput ) const
{
	VPROF_BUDGET( "CEconItem::BAddToMessage::CUtlBuffer", VPROF_BUDGETGROUP_STEAM );
	// StaticAssert( sizeof( int ) >= sizeof( uint32 ) );

	CSOEconItem msg;
	SerializeToProtoBufItem( msg );
	return CProtoBufSharedObjectBase::SerializeToBuffer( msg, bufOutput );
}

//----------------------------------------------------------------------------
// Purpose: Adds the relevant bits to update this object to the message. This
//			must include any relevant information about which fields are being
//			updated. This is called once for all subscribers.
//----------------------------------------------------------------------------
bool CEconItem::BAddToMessage( std::string *pBuffer ) const
{
	VPROF_BUDGET( "CEconItem::BAddToMessage::std::string", VPROF_BUDGETGROUP_STEAM );
	// StaticAssert( sizeof( int ) >= sizeof( uint32 ) );

	CSOEconItem msg;
	SerializeToProtoBufItem( msg );
	return msg.SerializeToString( pBuffer );
}

//----------------------------------------------------------------------------
// Purpose: Adds just the item ID to the message so that the client can find
//			which item to destroy
//----------------------------------------------------------------------------
bool CEconItem::BAddDestroyToMessage( CUtlBuffer & bufDestroy ) const
{
	CSOEconItem msgItem;
	msgItem.set_id( GetItemID() );
	CProtoBufSharedObjectBase::SerializeToBuffer( msgItem, bufDestroy );
	return true;
}

//----------------------------------------------------------------------------
// Purpose: Adds just the item ID to the message so that the client can find
//			which item to destroy
//----------------------------------------------------------------------------
bool CEconItem::BAddDestroyToMessage( std::string *pBuffer ) const
{
	CSOEconItem msgItem;
	msgItem.set_id( GetItemID() );
	return msgItem.SerializeToString( pBuffer );
}
#endif //GC

//----------------------------------------------------------------------------
// Purpose: Returns true if this is less than than the object in soRHS. This
//			comparison is deterministic, but it may not be pleasing to a user
//			since it is just going to compare raw memory. If you need a sort 
//			that is user-visible you will need to do it at a higher level that
//			actually knows what the data in these objects means.
//----------------------------------------------------------------------------
bool CEconItem::BIsKeyLess( const CSharedObject & soRHS ) const
{
	Assert( GetTypeID() == soRHS.GetTypeID() );
	const CEconItem & soSchemaRHS = (const CEconItem &)soRHS;

	return m_ulID < soSchemaRHS.m_ulID;
}

//----------------------------------------------------------------------------
// Purpose: Copy the data from the specified schema shared object into this. 
//			Both objects must be of the same type.
//----------------------------------------------------------------------------
void CEconItem::Copy( const CSharedObject & soRHS )
{
	*this = (const CEconItem &)soRHS;
}

//----------------------------------------------------------------------------
// Purpose: Dumps diagnostic information about the shared object
//----------------------------------------------------------------------------
void CEconItem::Dump() const
{
	CSOEconItem msgItem;
	SerializeToProtoBufItem( msgItem );
	CProtoBufSharedObjectBase::Dump( msgItem );
}


//----------------------------------------------------------------------------
// Purpose: Return short, identifying string about the object
//----------------------------------------------------------------------------
CUtlString CEconItem::GetDebugString() const
{
	CUtlString result;
	result.Format( "[CEconItem: ID=%llu, DefIdx=%d]", GetItemID(), GetDefinitionIndex() );
	return result;
}


#ifdef GC
//-----------------------------------------------------------------------------
// Purpose: Deserializes an item from a KV object
// Input:	pKVItem - Pointer to the KV structure that represents an item
//			schema - Econ item schema used for decoding human readable names
//			pVecErrors - Pointer to a vector where human readable errors will
//				be added
// Output:	True if the item deserialized successfully, false otherwise
//-----------------------------------------------------------------------------
bool CEconItem::BDeserializeFromKV( KeyValues *pKVItem, CUtlVector<CUtlString> *pVecErrors )
{
	Assert( pVecErrors );

	Assert( NULL != pKVItem );
	if ( NULL == pKVItem )
		return false;

	// The basic properties
	SetItemID( pKVItem->GetUint64( "ID", INVALID_ITEM_ID ) );
	SetInventoryToken( pKVItem->GetInt( "InventoryPos", GetUnacknowledgedPositionFor(UNACK_ITEM_DROPPED) ) );	// Start by assuming it's a drop
	SetQuantity( pKVItem->GetInt( "Quantity", 1 ) );

	// Look up the following properties based on names from the schema
	const char *pchDefName = pKVItem->GetString( "DefName" );
	const CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinitionByName( pchDefName );
	if( !pItemDef )
	{
		pVecErrors->AddToTail( CUtlString( CFmtStr( "Item definition \"%s\" not found", pchDefName ) ) );

		// we can't do any reasonable validation with no item def, so just stop here
		return false;
	}

	SetDefinitionIndex( pItemDef->GetDefinitionIndex() );

	const char *pchQualityName = pKVItem->GetString( "QualityName" );
	if( !pchQualityName || ! *pchQualityName )
	{
		// set the default quality for the definition
		if( pItemDef->GetQuality() == k_unItemQuality_Any )
		{
			pVecErrors->AddToTail( CUtlString( CFmtStr( "Quality was not specified and this item def doesn't define one either." ) ) );
		}
		else
		{
			SetQuality( pItemDef->GetQuality() );
		}
	}
	else if ( !GetItemSchema()->BGetItemQualityFromName( pchQualityName, &m_nQuality ) || k_unItemQuality_Any == GetQuality() )
	{
		pVecErrors->AddToTail( CUtlString( CFmtStr( "Quality \"%s\" not found", pchQualityName ) ) );
	}

	// make sure the level is sane
	SetItemLevel( pKVItem->GetInt( "Level", pItemDef->GetMinLevel() ) );

	// read the flags
	uint8 unFlags = GetFlags();
	if( pKVItem->GetInt( "flag_cannot_trade", 0 ) )
	{
		unFlags |= kEconItemFlag_CannotTrade;
	}
	else
	{
		unFlags = unFlags & ~kEconItemFlag_CannotTrade;
	}
	if( pKVItem->GetInt( "flag_cannot_craft", 0 ) )
	{
		unFlags |= kEconItemFlag_CannotBeUsedInCrafting;
	}
	else
	{
		unFlags = unFlags & ~kEconItemFlag_CannotBeUsedInCrafting;
	}
	if( pKVItem->GetInt( "flag_non_economy", 0 ) )
	{
		unFlags |= kEconItemFlag_NonEconomy;
	}
	else
	{
		unFlags = unFlags & ~kEconItemFlag_NonEconomy;
	}
	SetFlag( unFlags );

	// Deserialize the attributes
	KeyValues *pKVAttributes = pKVItem->FindKey( "Attributes" );
	if ( NULL != pKVAttributes )
	{
		FOR_EACH_SUBKEY( pKVAttributes, pKVAttr )
		{
			// Try to load each line into an attribute in memory. It's possible that if we fail to successfully
			// load some attribute contents here we'll leak small amounts of memory, but if that happens we're
			// going to fail to start up anyway so we don't really care.
			static_attrib_t staticAttrib;
			if ( !staticAttrib.BInitFromKV_SingleLine( __FUNCTION__, pKVAttr, pVecErrors ) )
				continue;

			const CEconItemAttributeDefinition *pAttrDef = staticAttrib.GetAttributeDefinition();
			Assert( pAttrDef );

			const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
			Assert( pAttrType );

			// Load the attribute contents into memory on the item.
			pAttrType->LoadEconAttributeValue( this, pAttrDef, staticAttrib.m_value );

			// Free up our temp loading memory.
			pAttrType->UnloadEconAttributeValue( &staticAttrib.m_value );
		}
	}

	return ( NULL == pVecErrors || 0 == pVecErrors->Count() );
}


// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
bool CEconItem::BYieldingSerializeFromDatabase( itemid_t ulItemID )
{
	static CColumnSet csItem( CSET_FULL( CSchItem ) );

	enum
	{
		kQueryResultSet_SingleItem_Item			   = 0,
		kQueryResultSet_SingleItem_Attributes_Base = 1,

		kQueryResultSet_SingleItemCount
	};

	static uint32 sExpectedResults = 0;
	static CFmtStrMax sLoadQuery;
	if ( 0 == sLoadQuery.Length() )
	{
		sLoadQuery.Append( 
			"DECLARE @ItemID BIGINT "
			"SET @ItemID = ? " );

		// Load the item itself.
		TSQLCmdStr sStatement;
		BuildSelectStatementText( &sStatement, csItem );
		sStatement.Append( " WHERE ID = @ItemID " );
		sLoadQuery.Append( sStatement );
		DbgVerify( kQueryResultSet_SingleItem_Item == sExpectedResults++ );

		// Load any associated attributes.
		DbgVerify( kQueryResultSet_SingleItem_Attributes_Base == sExpectedResults++ );
		{
			const CUtlVector<attr_type_t>& vecAttributeTypes = GetItemSchema()->GetAttributeTypes();

			FOR_EACH_VEC( vecAttributeTypes, i )
			{
				// We could actually skip loading the "ItemID" column here because we're only loading one item
				// of data, but we would have to do some work because we don't actually know the CSch types here
				// so it doesn't seem worth it. We'll have to update the LoadSQLAttributesToEconItem() code if
				// we ever do this.
				BuildSelectStatementText( &sStatement, vecAttributeTypes[i].m_pAttrType->GetFullColumnSet() );
				sStatement.Append( " WHERE ItemID = @ItemID " );
				sLoadQuery.Append( sStatement );
			}
		}
	}
	Assert( kQueryResultSet_SingleItemCount == sExpectedResults );

	// Load item and associated attributes from SQL.
	CSQLAccess sqlAccess;
	sqlAccess.AddBindParam( ulItemID );
	if ( !sqlAccess.BYieldingExecute( "CEconItem::BYieldingSerializeFromDatabase", sLoadQuery.Get() ) )
	{
		EmitError( SPEW_GC, __FUNCTION__ ": failed to execute SQL load for item ID %llu\n", ulItemID );
		return false;
	}

	// ...
	if ( sqlAccess.GetResultSetCount() < sExpectedResults )
	{
		EmitError( SPEW_GC, __FUNCTION__ ": unable to interior item ID %llu. Got %d results back, but expected %d!\n", ulItemID, sqlAccess.GetResultSetCount(), sExpectedResults );
		return false;
	}

	// Make sure we only got one item back.
	const uint32 nRowCount = sqlAccess.GetResultSetRowCount( kQueryResultSet_SingleItem_Item );
	if ( nRowCount != 1 )
	{
		EmitError( SPEW_GC, __FUNCTION__ ": invalid number of rows returned from SQL loading item ID %llu: %d\n", ulItemID, nRowCount );
		return false;
	}

	// Make sure we understand what sort of item this is. We can't make any forward progress loading if we don't know
	// what definition its attached to.
	CSchItem schItem;
	sqlAccess.GetResultRecord( kQueryResultSet_SingleItem_Item, 0 ).BWriteToRecord( &schItem, csItem );
	const CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinition( schItem.m_unDefIndex );
	if ( !pItemDef )
	{
		EmitError( SPEW_GC,	__FUNCTION__ ": unable to find item definition %d for item %llu!\n", schItem.m_unDefIndex, schItem.m_ulID );
		return false;
	}

	// We have everything we need to complete the load here so default initialize -- free up attribute memory, current
	// state, etc.
	*this = CEconItem();

	// Deserialize from DB schema.
	DeserializeFromSchemaItem( schItem );

	// Load attributes.
	CEconManager::CEconItemAttributeLoader AttributeLoader;
	AttributeLoader.LoadSQLAttributesToEconItem( this, sqlAccess, kQueryResultSet_SingleItem_Attributes_Base );

	// Done.
	Compact();

	return true;
}

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
void CEconItem::SerializeToSchemaItem( CSchItem &item ) const
{
	VPROF_BUDGET( "CEconItem::SerializeToSchemaItem()", VPROF_BUDGETGROUP_STEAM );

	item.m_ulID = m_ulID;
	item.m_ulOriginalID = GetOriginalID();
	item.m_unAccountID = m_unAccountID;
	item.m_unDefIndex = m_unDefIndex;
	item.m_unLevel = m_unLevel;
	item.m_nQuality = m_nQuality;
	item.m_unInventory = m_unInventory;	
	item.m_unQuantity = GetQuantity();
	item.m_unFlags = m_unFlags;
	item.m_unOrigin = m_unOrigin;
	item.m_unStyle = m_unStyle;
}

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
void CEconItem::DeserializeFromSchemaItem( const CSchItem &item )
{
	VPROF_BUDGET( "CEconItem::DeserializeFromSchemaItem()", VPROF_BUDGETGROUP_STEAM );

	m_ulID = item.m_ulID;
	SetOriginalID( item.m_ulOriginalID ? item.m_ulOriginalID : item.m_ulID );
	m_unAccountID = item.m_unAccountID;
	m_unDefIndex = item.m_unDefIndex;
	m_unLevel = item.m_unLevel;
	m_nQuality = item.m_nQuality;
	m_unInventory = item.m_unInventory;
	SetQuantity( item.m_unQuantity );
	m_unFlags = item.m_unFlags;
	m_unOrigin = item.m_unOrigin;
	m_unStyle = item.m_unStyle;

	// set name if any, or remove if non-existent
	SetCustomName( READ_VAR_CHAR_FIELD( item, m_VarCharCustomName ) );
	
	// set desc if any, or remove if non-existent
	SetCustomDesc( READ_VAR_CHAR_FIELD( item, m_VarCharCustomDesc ) );
}
#endif // GC


// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
void CEconItem::SerializeToProtoBufItem( CSOEconItem &msgItem ) const
{
	VPROF_BUDGET( "CEconItem::SerializeToProtoBufItem()", VPROF_BUDGETGROUP_STEAM );

	msgItem.set_id( m_ulID );
	if( m_ulID != GetOriginalID() )
		msgItem.set_original_id( GetOriginalID() );
	msgItem.set_account_id( m_unAccountID );
	msgItem.set_def_index( m_unDefIndex );
	msgItem.set_level( m_unLevel );
	msgItem.set_quality( m_nQuality );
	msgItem.set_inventory( m_unInventory );	
	msgItem.set_quantity( GetQuantity() );
	msgItem.set_flags( m_unFlags );
	msgItem.set_origin( m_unOrigin );
	msgItem.set_style( m_unStyle );
	msgItem.set_in_use( m_dirtyBits.m_bInUse );

	for( int nAttr = 0; nAttr < GetDynamicAttributeCountInternal(); nAttr++ )
	{
		const attribute_t & attr = GetDynamicAttributeInternal( nAttr );
		
		// skip over attributes we don't understand
		const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( attr.m_unDefinitionIndex );
		if ( !pAttrDef )
			continue;

		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );

		CSOEconItemAttribute *pMsgAttr = msgItem.add_attribute();
		pMsgAttr->set_def_index( attr.m_unDefinitionIndex );

		std::string sBytes;
		pAttrType->ConvertEconAttributeValueToByteStream( attr.m_value, &sBytes );
		pMsgAttr->set_value_bytes( sBytes );
	}

	msgItem.set_contains_equipped_state_v2( true );
	for ( int i = 0; i < GetEquippedInstanceCount(); i++ )
	{
		const EquippedInstance_t &instance = GetEquippedInstance( i );
		CSOEconItemEquipped *pMsgEquipped = msgItem.add_equipped_state();
		pMsgEquipped->set_new_class( instance.m_unEquippedClass );
		pMsgEquipped->set_new_slot( instance.m_unEquippedSlot );
	}

	if ( m_pCustomData )
	{
		const char *pszCustomName = GetCustomName();
		if ( pszCustomName )
		{
			msgItem.set_custom_name( pszCustomName );
		}

		const char *pszCustomDesc = GetCustomDesc();
		if ( pszCustomDesc )
		{
			msgItem.set_custom_desc( pszCustomDesc );
		}

		const CEconItem *pInteriorItem = GetInteriorItem();
		if ( pInteriorItem )
		{
			CSOEconItem *pMsgInteriorItem = msgItem.mutable_interior_item();
			pInteriorItem->SerializeToProtoBufItem( *pMsgInteriorItem );
		}
	}
}

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
void CEconItem::DeserializeFromProtoBufItem( const CSOEconItem &msgItem )
{
	VPROF_BUDGET( "CEconItem::DeserializeFromProtoBufItem()", VPROF_BUDGETGROUP_STEAM );

	// Start by resetting
	SAFE_DELETE( m_pCustomData );
	m_dirtyBits.m_bHasAttribSingleton = false;
	m_dirtyBits.m_bHasEquipSingleton = false;

	// Now copy from the message
	m_ulID = msgItem.id();
	SetOriginalID( msgItem.has_original_id() ? msgItem.original_id() : m_ulID );
	m_unAccountID = msgItem.account_id();
	m_unDefIndex = msgItem.def_index();
	m_unLevel = msgItem.level();
	m_nQuality = msgItem.quality();
	m_unInventory = msgItem.inventory();
	SetQuantity( msgItem.quantity() );
	m_unFlags = msgItem.flags();
	m_unOrigin = msgItem.origin();
	m_unStyle = msgItem.style();

	m_dirtyBits.m_bInUse = msgItem.in_use() ? 1 : 0;

	// set name if any
	if( msgItem.has_custom_name() )
	{
		SetCustomName( msgItem.custom_name().c_str() );
	}

	// set desc if any
	if( msgItem.has_custom_desc() )
	{
		SetCustomDesc( msgItem.custom_desc().c_str() );
	}

	// read the attributes
	for( int nAttr = 0; nAttr < msgItem.attribute_size(); nAttr++ )
	{
		// skip over old-format messages
		const CSOEconItemAttribute& msgAttr = msgItem.attribute( nAttr );
		if ( msgAttr.has_value() || !msgAttr.has_value_bytes() )
			continue;

		// skip over attributes we don't understand
		const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( msgAttr.def_index() );
		if ( !pAttrDef )
			continue;

		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );

		pAttrType->LoadByteStreamToEconAttributeValue( this, pAttrDef, msgAttr.value_bytes() );
	}

	// Check to see if the item has an interior object.
	if ( msgItem.has_interior_item() )
	{
		EnsureCustomDataExists();

		m_pCustomData->m_pInteriorItem = new CEconItem();
		m_pCustomData->m_pInteriorItem->DeserializeFromProtoBufItem( msgItem.interior_item() );
	}

	// update equipped state
	if ( msgItem.has_contains_equipped_state_v2() && msgItem.contains_equipped_state_v2() )
	{
		// unequip from everything...
		Unequip();

		// ...and re-equip to whatever our current state is
		for ( int i = 0; i < msgItem.equipped_state_size(); i++ )
		{
			Equip( msgItem.equipped_state(i).new_class(), msgItem.equipped_state(i).new_slot() );
		}
	}
}

#ifdef GC
#include "econ/localization_provider.h"
// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
void CEconItem::GetDirtyColumnSet( const CUtlVector< int > &fields, CColumnSet &cs ) const
{
	cs.MakeEmpty();
	/* @note Tom Bui: We know only the inventory/position field and the quantity field can be dirty...
	for ( int nField = 0; nField < CSchItem::k_iFieldMax; ++nField )
	{
		if ( bDatabase ? BIsFieldDatabaseDirty( nField ) : BIsFieldNetworkDirty( nField ) )
		{
			cs.BAddColumn( nField );
		}
	}
	*/
	if( fields.HasElement( CSOEconItem::kInventoryFieldNumber ) )
		cs.BAddColumn( CSchItem::k_iField_unInventory );
	if( fields.HasElement( CSOEconItem::kQuantityFieldNumber ) )
		cs.BAddColumn( CSchItem::k_iField_unQuantity );
	if( fields.HasElement( CSOEconItem::kStyleFieldNumber ) )
		cs.BAddColumn( CSchItem::k_iField_unStyle );
}


// --------------------------------------------------------------------------
// Purpose: Exports the item for use in another app
// --------------------------------------------------------------------------
void CEconItem::ExportToAPI( CWebAPIValues *pValues ) const
{
	pValues->CreateChildObject( "id" )->SetUInt64Value( m_ulID );
	pValues->CreateChildObject( "def_index" )->SetUInt32Value( m_unDefIndex );
	pValues->CreateChildObject( "level" )->SetUInt32Value( m_unLevel );
	pValues->CreateChildObject( "quality" )->SetInt32Value( m_nQuality );

	if( GetCustomName() )
		pValues->CreateChildObject( "custom_name" )->SetStringValue( GetCustomName() );
	if( GetCustomDesc() )
		pValues->CreateChildObject( "custom_desc" )->SetStringValue( GetCustomDesc() );
}


// --------------------------------------------------------------------------
// Purpose: Imports a view of the item from another app
// --------------------------------------------------------------------------
bool CEconItem::BImportFromAPI( CWebAPIValues *pValues )
{
	m_unLevel = pValues->GetChildUInt32Value( "level", 1 );
	m_nQuality = pValues->GetChildUInt32Value( "quality", GetItemSchema()->GetDefaultQuality() );

	CUtlString sValue;
	pValues->GetChildStringValue( sValue, "custom_name", "" );
	CGCLocalizationProvider::BEnsureCleanUTF8Truncation( sValue.GetForModify() );
	if( !sValue.IsEmpty() )
		SetCustomName( sValue.Get() );
	pValues->GetChildStringValue( sValue, "custom_desc", "" );
	CGCLocalizationProvider::BEnsureCleanUTF8Truncation( sValue.GetForModify() );
	if( !sValue.IsEmpty() )
		SetCustomDesc( sValue.Get() );

	return true;
}

#endif

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
void CEconItem::EnsureCustomDataExists()
{
	if ( m_pCustomData == NULL )
	{
		m_pCustomData = new CEconItemCustomData();
		
		if ( m_dirtyBits.m_bHasEquipSingleton )
		{
			m_pCustomData->m_vecEquipped.AddToTail( m_EquipInstanceSingleton );
			m_EquipInstanceSingleton = EquippedInstance_t();
			m_dirtyBits.m_bHasEquipSingleton = false;
		}
		if ( m_dirtyBits.m_bHasAttribSingleton )
		{
			m_pCustomData->m_vecAttributes.AddToTail( m_CustomAttribSingleton );
			m_dirtyBits.m_bHasAttribSingleton = false;
		}
	}
}

#ifdef GC_DLL
// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
static bool BYieldingAddAuditRecordImpl( GCSDK::CSQLAccess *sqlAccess, uint64 ulItemID, uint32 unOwnerID, EItemAction eAction, uint32 unData )
{
	CEconUserSession *pUserSession = GGCGameBase()->FindEconUserSession( CSteamID( unOwnerID, GGCHost()->GetUniverse(), k_EAccountTypeIndividual ) );
	CGCGSSession *pGSSession = pUserSession && pUserSession->GetSteamIDGS().IsValid() ? GGCEcon()->FindGSSession( pUserSession->GetSteamIDGS() ) : NULL;

	// Prepare the audit record
	CSchItemAudit schItemAudit;
	schItemAudit.m_ulItemID = ulItemID;	
	schItemAudit.m_RTime32Stamp = CRTime::RTime32TimeCur();
	schItemAudit.m_eAction = eAction;
	schItemAudit.m_unOwnerID = unOwnerID;
	schItemAudit.m_unData = unData;
	schItemAudit.m_unServerIP = pGSSession ? pGSSession->GetAddr() : 0;
	schItemAudit.m_usServerPort = pGSSession ? pGSSession->GetPort() : 0;

	return sqlAccess->BYieldingInsertRecord( &schItemAudit );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItem::CAuditEntry::BAddAuditEntryToTransaction( CSQLAccess& sqlAccess, const CEconItem *pItem ) const
{
	return BYieldingAddAuditRecordImpl( &sqlAccess, pItem->GetID(), pItem->GetAccountID(), m_eAction, m_unData );
}

// --------------------------------------------------------------------------
// Purpose: Adds an audit record to the DB for an item
// --------------------------------------------------------------------------
void YieldingAddAuditRecord( GCSDK::CSQLAccess *sqlAccess, CEconItem *pItem, uint32 unOwnerID, EItemAction eAction, uint32 unData )
{
	YieldingAddAuditRecord( sqlAccess, pItem->GetItemID(), unOwnerID, eAction, unData );
}

// --------------------------------------------------------------------------
// Purpose: Adds an audit record to the DB for an item
// --------------------------------------------------------------------------
void YieldingAddAuditRecord( GCSDK::CSQLAccess *sqlAccess, uint64 ulItemID, uint32 unOwnerID, EItemAction eAction, uint32 unData )
{
	Verify( BYieldingAddAuditRecordImpl( sqlAccess, ulItemID, unOwnerID, eAction, unData ) );
}


// --------------------------------------------------------------------------
// Purpose: Adds an item to the DB
// --------------------------------------------------------------------------
bool YieldingAddItemToDatabase( CEconItem *pItem, const CSteamID & steamID, EItemAction eAction, uint32 unData )
{
	CSQLAccess sqlAccess;
	sqlAccess.BBeginTransaction( "YieldingAddItemToDatabase" );

	pItem->SetAccountID( steamID.GetAccountID() );
	if( !pItem->BYieldingAddInsertToTransaction( sqlAccess ) )
	{
		sqlAccess.RollbackTransaction();
		return false;
	}

	YieldingAddAuditRecord( &sqlAccess, pItem, steamID.GetAccountID(), eAction, unData );

	// commit the item and audit record
	if( !sqlAccess.BCommitTransaction() )
	{
		EmitError( SPEW_GC, "Failed to add item for %s\n", steamID.Render() );
		return false;
	}

	return true;
}

//----------------------------------------------------------------------------
// Purpose:
//----------------------------------------------------------------------------
static CEconItem *FindEquippedItemForClassAndSlot( CEconSharedObjectCache *pSOCache, equipped_class_t unClass, equipped_slot_t unSlot )
{
	VPROF_BUDGET( "FindEquippedItemForClassAndSlot()", VPROF_BUDGETGROUP_STEAM );
	Assert( pSOCache );

	CGCSharedObjectTypeCache *pItemTypeCache = pSOCache->FindTypeCache( k_EEconTypeItem );

	const uint32 unCount = pItemTypeCache ? pItemTypeCache->GetCount() : 0;
	for ( uint32 i = 0; i < unCount; i++ )
	{
		CEconItem *pItem = static_cast<CEconItem *>( pItemTypeCache->GetObject( i ) );
		Assert( pItem );

		for ( int j = 0; j < pItem->GetEquippedInstanceCount(); j++ )
		{
			const CEconItem::EquippedInstance_t& equipInst = pItem->GetEquippedInstance( j );

			if ( equipInst.m_unEquippedClass == unClass && equipInst.m_unEquippedSlot == unSlot )
				return pItem;
		}
	}

	return NULL;
}

//----------------------------------------------------------------------------
// Purpose:
//----------------------------------------------------------------------------
/*static*/ void CEconItemEquipInstanceHelpers::AssignItemToSlot( CEconSharedObjectCache *pSOCache, CEconItem *pItem, equipped_class_t unClass, equipped_slot_t unSlot, CEconUserSession *pOptionalSession /* = NULL */ )
{
	VPROF_BUDGET( "CEconEquipInstance::AssignItemToSlot()", VPROF_BUDGETGROUP_STEAM );

	Assert( pSOCache );
	Assert( GetItemSchema()->IsValidClass( unClass ) );
	Assert( GetItemSchema()->IsValidItemSlot( unSlot, unClass ) );
	
	CEconItem *pPreviousEquippedItem = FindEquippedItemForClassAndSlot( pSOCache, unClass, unSlot );

	// skip out on expensive work if we're going to equip the item that's already in that slot
	if ( pItem == pPreviousEquippedItem )
		return;

	// do we have an item to unequip first
	if ( pPreviousEquippedItem )
	{
		pPreviousEquippedItem->UnequipFromClass( unClass );
		pSOCache->DirtyObjectField( pPreviousEquippedItem, CSOEconItem::kEquippedStateFieldNumber );
		GGCBase()->AddCacheToWritebackQueue( pSOCache );

		const bool bPreviousItemIsNowEquipped = pPreviousEquippedItem->IsEquipped();

		if ( pOptionalSession && !bPreviousItemIsNowEquipped )
		{
			pOptionalSession->OnEquippedStateChange( pPreviousEquippedItem, false );
		}
	}

	// are we equipping a new item to this slot, or were we only responsible for unequipping
	// the existing item?
	if ( pItem )
	{
		// notify the session, in case it wants to do anything; the default implementation
		// will update the equipped state on the item but other types may have additional
		// behavior
		const bool bItemWasEquipped = pItem->IsEquipped();

		pItem->Equip( unClass, unSlot );
		pSOCache->DirtyObjectField( pItem, CSOEconItem::kEquippedStateFieldNumber );
		GGCBase()->AddCacheToWritebackQueue( pSOCache );

		const bool bItemIsEquipped = pItem->IsEquipped();
		Assert( bItemIsEquipped );

		// did our equipped state change? this can happen if we equip ourself for the
		// first time for this account right now. Keep in mind that this can be either
		// a "is newly-equipped" or a "was equipped but is now not" state transition.
		if ( pOptionalSession && (bItemWasEquipped != bItemIsEquipped) )
		{
			pOptionalSession->OnEquippedStateChange( pItem, bItemIsEquipped );
		}
	}
}

#define ITEM_ACTION( x ) { x, #x }

ENUMSTRINGS_START( EItemAction )
	ITEM_ACTION( k_EItemActionGSCreate ),
	ITEM_ACTION( k_EItemActionUnpurchase ),
	ITEM_ACTION( k_EItemActionDelete ),
	ITEM_ACTION( k_EItemActionAwardAchievement ),
	ITEM_ACTION( k_EItemActionBanned ),
	ITEM_ACTION( k_EItemActionQuantityChanged ),
	ITEM_ACTION( k_EItemActionRestored ),
	ITEM_ACTION( k_EItemActionAwardTime ),
	ITEM_ACTION( k_EItemActionManualCreate ),
	ITEM_ACTION( k_EItemActionDrop ),
	ITEM_ACTION( k_EItemActionPickUp ),
	ITEM_ACTION( k_EItemActionCraftDestroy ),
	ITEM_ACTION( k_EItemActionCraftCreate ),
	ITEM_ACTION( k_EItemActionLimitExceeded ),
	ITEM_ACTION( k_EItemActionPurchase ),
	ITEM_ACTION( k_EItemActionNameChanged_Add ),
	ITEM_ACTION( k_EItemActionUnlockCrate_Add ),
	ITEM_ACTION( k_EItemActionPaintItem_Add ),
	ITEM_ACTION( k_EItemActionAutoGrantItem ),
	ITEM_ACTION( k_EItemActionCrossGameAchievement ),
	ITEM_ACTION( k_EItemActionAddItemToSocket_Add ),
	ITEM_ACTION( k_EItemActionAddSocketToItem_Add ),
	ITEM_ACTION( k_EItemActionRemoveSocketItem_Add ),
	ITEM_ACTION( k_EItemActionCustomizeItemTexture_Add ),
	ITEM_ACTION( k_EItemActionItemTraded_Add ),
	ITEM_ACTION( k_EItemActionUseItem ),
	ITEM_ACTION( k_EItemActionAwardGift_Receiver ),
	ITEM_ACTION( k_EItemActionNameChanged_Remove ),
	ITEM_ACTION( k_EItemActionUnlockCrate_Remove ),
	ITEM_ACTION( k_EItemActionPaintItem_Remove ),
	ITEM_ACTION( k_EItemActionAddItemToSocket_Remove ),
	ITEM_ACTION( k_EItemActionAddSocketToItem_Remove ),
	ITEM_ACTION( k_EItemActionRemoveSocketItem_Remove ), 
	ITEM_ACTION( k_EItemActionCustomizeItemTexture_Remove ),
	ITEM_ACTION( k_EItemActionItemTraded_Remove ),
	ITEM_ACTION( k_EItemActionUnpackItemBundle ),
	ITEM_ACTION( k_EItemActionCreateItemFromBundle ),
	ITEM_ACTION( k_EItemActionAwardStorePromotionItem ),
	ITEM_ACTION( k_EItemActionConvertItem ),
	ITEM_ACTION( k_EItemActionEarnedItem ),
	ITEM_ACTION( k_EItemActionAwardGift_Giver ),
	ITEM_ACTION( k_EItemActionRefundedItem ),
	ITEM_ACTION( k_EItemActionAwardThirdPartyPromo ),
	ITEM_ACTION( k_EItemActionRemoveItemName_Remove ),
	ITEM_ACTION( k_EItemActionRemoveItemName_Add ),
	ITEM_ACTION( k_EItemActionRemoveItemPaint_Remove ),
	ITEM_ACTION( k_EItemActionRemoveItemPaint_Add ),
	ITEM_ACTION( k_EItemActionHalloweenDrop ),
	ITEM_ACTION( k_EItemActionSteamWorkshopContributor ),
	ITEM_ACTION( k_EItemActionManualOwnershipChange ),
	ITEM_ACTION( k_EItemActionSupportDelete ),
	ITEM_ACTION( k_EItemActionSupportCreatedByUndo ),
	ITEM_ACTION( k_EItemActionSupportDeletedByUndo ),
	ITEM_ACTION( k_EItemActionSupportQuantityChangedByUndo ),
	ITEM_ACTION( k_EItemActionSupportRename_Add ),
	ITEM_ACTION( k_EItemActionSupportRename_Remove ),
	ITEM_ACTION( k_EItemActionSupportDescribe_Add ),
	ITEM_ACTION( k_EItemActionSupportDescribe_Remove ),

	ITEM_ACTION( k_EItemActionStrangePartApply_Add ),
	ITEM_ACTION( k_EItemActionStrangePartApply_Remove ),
	ITEM_ACTION( k_EItemActionStrangeScoreReset_Add ),
	ITEM_ACTION( k_EItemActionStrangeScoreReset_Remove ),
	ITEM_ACTION( k_EItemActionStrangePartRemove_Add ),
	ITEM_ACTION( k_EItemActionStrangePartRemove_Remove ),

	ITEM_ACTION( k_EItemActionStrangeRestrictionApply_Add ),
	ITEM_ACTION( k_EItemActionStrangeRestrictionApply_Remove ),
	ITEM_ACTION( k_EItemActionTransmogrify_Add ),
	ITEM_ACTION( k_EItemActionTransmogrify_Remove ),
	ITEM_ACTION( k_EItemActionHalloweenSpellPageAdd_Add ),
	ITEM_ACTION( k_EItemActionHalloweenSpellPageAdd_Remove ),
	
	ITEM_ACTION( k_EItemActionSupportStrangify_Add ),
	ITEM_ACTION( k_EItemActionSupportStrangify_Remove ),

	ITEM_ACTION( k_EItemActionUpgradeCardApply_Add ),
	ITEM_ACTION( k_EItemActionUpgradeCardApply_Remove ),
	ITEM_ACTION( k_EItemActionUpgradeCardRemove_Add ),
	ITEM_ACTION( k_EItemActionUpgradeCardRemove_Remove ),

#ifdef STAGING_ONLY
	ITEM_ACTION( k_EItemActionDev_ClientLootListRoll ),
#endif

	ITEM_ACTION( k_EItemActionPeriodicScoreReward_Add ),
	ITEM_ACTION( k_EItemActionPeriodicScoreReward_Remove ),

	ITEM_ACTION( k_EItemActionGiftWrap_Add ),
	ITEM_ACTION( k_EItemActionGiftWrap_Remove ),
	ITEM_ACTION( k_EItemActionGiftDelivery_Add ),
	ITEM_ACTION( k_EItemActionGiftDelivery_Remove ),
	ITEM_ACTION( k_EItemActionGiftUnwrap_Add ),
	ITEM_ACTION( k_EItemActionGiftUnwrap_Remove ),
	ITEM_ACTION( k_EItemActionPackageItem ),
	ITEM_ACTION( k_EItemActionPackageItem_Revoked ),
	ITEM_ACTION( k_EItemActionHandleMapToken ),
	ITEM_ACTION( k_EItemActionCafeOrSchoolItem_Remove ),
	ITEM_ACTION( k_EItemActionVACBanned_Remove ),
	ITEM_ACTION( k_EItemActionUpgradeThirdPartyPromo ),
	ITEM_ACTION( k_EItemActionExpired ),
	ITEM_ACTION( k_EItemActionTradeRollback_Add ),
	ITEM_ACTION( k_EItemActionTradeRollback_Remove ),
	ITEM_ACTION( k_EItemActionCDKeyGrant ),
	ITEM_ACTION( k_EItemActionCDKeyRevoke ),
	ITEM_ACTION( k_EItemActionWeddingRing_Add ),
	ITEM_ACTION( k_EItemActionWeddingRing_Remove ),
	ITEM_ACTION( k_EItemActionWeddingRing_AddPartner ),

	ITEM_ACTION( k_EItemActionEconSetUnowned ),
	ITEM_ACTION( k_EItemActionEconSetOwned ),
	ITEM_ACTION( k_EItemActionStrangifyItem_Add ),
	ITEM_ACTION( k_EItemActionStrangifyItem_Remove ),

	ITEM_ACTION( k_EItemActionRemoveItemCraftIndex_Remove ),
	ITEM_ACTION( k_EItemActionRemoveItemCraftIndex_Add ),
	ITEM_ACTION( k_EItemActionRemoveItemMakersMark_Remove ),
	ITEM_ACTION( k_EItemActionRemoveItemMakersMark_Add ),
	ITEM_ACTION( k_EItemActionRemoveItemKillStreak_Remove ),
	ITEM_ACTION( k_EItemActionRemoveItemKillStreak_Add ),

	ITEM_ACTION( k_EItemActionCollectItem_RemoveCollection ),
	ITEM_ACTION( k_EItemActionCollectItem_UpdateCollection ),
	ITEM_ACTION( k_EItemActionCollectItem_CollectedItem ),
	ITEM_ACTION( k_EItemActionCollectItem_RedeemCollectionReward ),

	ITEM_ACTION( k_EItemActionPreviewItem_BeginPreviewPeriod ),
	ITEM_ACTION( k_EItemActionPreviewItem_EndPreviewPeriodExpired ),
	ITEM_ACTION( k_EItemActionPreviewItem_EndPreviewPeriodItemBought ),

	ITEM_ACTION( k_EItemActionMvM_ChallengeCompleted_RemoveTicket ),
	ITEM_ACTION( k_EItemActionMvM_ChallengeCompleted_GrantBadge ),
	ITEM_ACTION( k_EItemActionMvM_ChallengeCompleted_UpdateBadgeStamps_Remove ),
	ITEM_ACTION( k_EItemActionMvM_ChallengeCompleted_UpdateBadgeStamps_Add ),
	ITEM_ACTION( k_EItemActionMvM_ChallengeCompleted_GrantMissionCompletionLoot ),
	ITEM_ACTION( k_EItemActionMvM_RemoveSquadSurplusVoucher ),
	ITEM_ACTION( k_EItemActionMvM_AwardSquadSurplus_Receiver ),
	ITEM_ACTION( k_EItemActionMvM_AwardSquadSurplus_Giver ),
	ITEM_ACTION( k_EItemActionMvM_ChallengeCompleted_GrantTourCompletionLoot ),
	ITEM_ACTION( k_EItemActionMvM_AwardHelpANoobBonus_Helper ),

	ITEM_ACTION( k_EItemActionHalloween_UpdateMerasmusLootLevel_Add ),
	ITEM_ACTION( k_EItemActionHalloween_UpdateMerasmusLootLevel_Remove ),

	ITEM_ACTION( k_EItemActionConsumeItem_Consume_ToolRemove ),
	ITEM_ACTION( k_EItemActionConsumeItem_Consume_ToolAdd ),
	ITEM_ACTION( k_EItemActionConsumeItem_Consume_InputRemove ),
	ITEM_ACTION( k_EItemActionConsumeItem_Complete_OutputAdd ),
	ITEM_ACTION( k_EItemActionConsumeItem_Complete_ToolRemove ),

	ITEM_ACTION( k_EItemActionItemEaterRecharge_Add ),
	ITEM_ACTION( k_EItemActionItemEaterRecharge_Remove ),

	ITEM_ACTION( k_EItemActionSupportAddOrModifyAttribute_Remove ),
	ITEM_ACTION( k_EItemActionSupportAddOrModifyAttribute_Add ),

	ITEM_ACTION( k_EItemAction_UpdateDuckBadgeLevel_Add ),
	ITEM_ACTION( k_EItemAction_UpdateDuckBadgeLevel_Remove ),

	ITEM_ACTION( k_EItemAction_OperationPass_Add ),

	ITEM_ACTION( k_EItemActionSpyVsEngyWar_JoinedWar ),
	
	ITEM_ACTION( k_EItemActionMarket_Add ),
	ITEM_ACTION( k_EItemActionMarket_Remove ),

	ITEM_ACTION( k_EItemAction_QuestComplete_Reward ),
	ITEM_ACTION( k_EItemAction_QuestComplete_Remove ),

	ITEM_ACTION( k_EItemActionStrangeCountTransfer_Add ),
	ITEM_ACTION( k_EItemActionStrangeCountTransfer_Remove ),

	ITEM_ACTION( k_EItemActionCraftCollectionUpgrade_Add ),
	ITEM_ACTION( k_EItemActionCraftCollectionUpgrade_Remove ),

	ITEM_ACTION( k_EItemActionCraftHalloweenOffering_Add ),
	ITEM_ACTION( k_EItemActionCraftHalloweenOffering_Remove ),

	ITEM_ACTION( k_EItemActionRemoveItemGiftedBy_Remove ),
	ITEM_ACTION( k_EItemActionRemoveItemGiftedBy_Add ),

	ITEM_ACTION( k_EItemActionAddParticleVerticalAttr_Remove ),
	ITEM_ACTION( k_EItemActionAddParticleVerticalAttr_Add ),

	ITEM_ACTION( k_EItemActionAddParticleUseHeadOriginAttr_Remove ),
	ITEM_ACTION( k_EItemActionAddParticleUseHeadOriginAttr_Add ),

	ITEM_ACTION( k_EItemActionRemoveItemDynamicAttr_Remove ),
	ITEM_ACTION( k_EItemActionRemoveItemDynamicAttr_Add ),

	ITEM_ACTION( k_EItemActionCraftStatClockTradeUp_Remove ),
	ITEM_ACTION( k_EItemActionCraftStatClockTradeUp_Add ),

	ITEM_ACTION( k_EItemActionViralCompetitiveBetaPass_Drop ),

	ITEM_ACTION( k_EItemActionSupportDeleteAttribute_Remove ),
	ITEM_ACTION( k_EItemActionSupportDeleteAttribute_Add ),

ENUMSTRINGS_END( EItemAction )

const char *PchFriendlyNameFromEItemAction( EItemAction eItemAction, EItemActionMissingBehavior eMissingBehavior )
{
	Assert( eMissingBehavior == kEItemAction_FriendlyNameLookup_ReturnDummyStringIfMissing || eMissingBehavior == kEItemAction_FriendlyNameLookup_ReturnNULLIfMissing );

	switch( eItemAction )
	{
		
		case  k_EItemActionGSCreate: return "Created by Gameserver";
		case  k_EItemActionUnpurchase: return "Unpurchase";
		case  k_EItemActionDelete: return "Deleted by Owner";
		case  k_EItemActionAwardAchievement: return "Achievement Award";
		case  k_EItemActionBanned: return "Banned";
		case  k_EItemActionQuantityChanged: return "Quantity Changed";
		case  k_EItemActionRestored: return "Restored";
		case  k_EItemActionAwardTime: return "Timed Drop";
		case  k_EItemActionManualCreate: return "Created by Support";
		case  k_EItemActionDrop: return "Dropped";
		case  k_EItemActionPickUp: return "Picked up";
		case  k_EItemActionCraftDestroy: return "Crafted";
		case  k_EItemActionCraftCreate: return "Crafted";
		case  k_EItemActionLimitExceeded: return "Destroyed by Backpack Limit";
		case  k_EItemActionPurchase: return "Purchase";
		case  k_EItemActionNameChanged_Add: return "Added by Name Change";
		case  k_EItemActionUnlockCrate_Add: return "Added by Crate Unlock";
		case  k_EItemActionPaintItem_Add: return "Added by Paint";
		case  k_EItemActionAutoGrantItem: return "Autogrant";
		case  k_EItemActionCrossGameAchievement: return "Cross-Game Achievement Award";
		case  k_EItemActionAddItemToSocket_Add: return "Added to Socket";
		case  k_EItemActionAddSocketToItem_Add: return "Added by Socket";
		case  k_EItemActionRemoveSocketItem_Add: return "Removed by Socket";
		case  k_EItemActionCustomizeItemTexture_Add: return "Added by Custom Texture";
		case  k_EItemActionItemTraded_Add: return "Trade";
		case  k_EItemActionUseItem: return "Used";
		case  k_EItemActionAwardGift_Receiver: return "Gift Received";
		case  k_EItemActionNameChanged_Remove: return "Removed by Name Change";
		case  k_EItemActionUnlockCrate_Remove: return "Removed by Crate Unlock";
		case  k_EItemActionPaintItem_Remove: return "Removed by Paint";
		case  k_EItemActionAddItemToSocket_Remove: return "Removed from Socket";
		case  k_EItemActionAddSocketToItem_Remove: return "Removed by Socket";
		case  k_EItemActionRemoveSocketItem_Remove: return "Removed Socket Item"; 
		case  k_EItemActionCustomizeItemTexture_Remove: return "Removed by Custom Texture";
		case  k_EItemActionItemTraded_Remove: return "Trade";
		case  k_EItemActionUnpackItemBundle: return "Unpacked Bundle";
		case  k_EItemActionCreateItemFromBundle: return "Added from Bundle";
		case  k_EItemActionAwardStorePromotionItem: return "Store Promo Item";
		case  k_EItemActionConvertItem: return "Converted";
		case  k_EItemActionEarnedItem: return "Earned";
		case  k_EItemActionAwardGift_Giver: return "Gift Sent";
		case  k_EItemActionRefundedItem: return "Item Refunded";
		case  k_EItemActionAwardThirdPartyPromo: return "Third-Party Promo";
		case  k_EItemActionRemoveItemName_Remove: return "Removed by Name Removal";
		case  k_EItemActionRemoveItemName_Add: return "Added by Name Removal";
		case  k_EItemActionRemoveItemPaint_Remove: return "Removed by Paint Removal";
		case  k_EItemActionRemoveItemPaint_Add: return "Added by Paint Removal";
		case  k_EItemActionHalloweenDrop: return "Halloween Drop";
		case  k_EItemActionSteamWorkshopContributor: return "Steam Workshop Contributor";
		case  k_EItemActionManualOwnershipChange: return "Support Manual Ownership Change";
		case  k_EItemActionSupportDelete: return "Deleted by Support";
		case  k_EItemActionSupportCreatedByUndo: return "Created by Undo";
		case  k_EItemActionSupportDeletedByUndo: return "Deleted by Undo";
		case  k_EItemActionSupportQuantityChangedByUndo: return "Quantity Changed by Undo";
		case  k_EItemActionSupportRename_Add: return "Added by Support Rename";
		case  k_EItemActionSupportRename_Remove: return "Removed by Support Rename";
		case  k_EItemActionSupportDescribe_Add: return "Added by Support Describe";
		case  k_EItemActionSupportDescribe_Remove: return "Removed by Support Describe";

		case  k_EItemActionStrangePartApply_Add: return "Added by Strange Part Apply";
		case  k_EItemActionStrangePartApply_Remove: return "Removed by Strange Part Apply";
		case  k_EItemActionStrangeScoreReset_Add: return "Added by Strange Score Reset";
		case  k_EItemActionStrangeScoreReset_Remove: return "Removed by Strange Score Reset";
		case  k_EItemActionStrangePartRemove_Add: return "Added by Strange Part Remove";
		case  k_EItemActionStrangePartRemove_Remove: return "Removed by Strange Part Remove";

		case  k_EItemActionStrangeRestrictionApply_Add: return "Added by Strange Restriction Apply";
		case  k_EItemActionStrangeRestrictionApply_Remove: return "Removed by Strange Restriction Apply";
		case  k_EItemActionTransmogrify_Add: return "Added by Transmogrify Apply";
		case  k_EItemActionTransmogrify_Remove: return "Removed by Transmogrify Apply";
		case  k_EItemActionHalloweenSpellPageAdd_Add: return "Added by Halloween Spell Page Apply";
		case  k_EItemActionHalloweenSpellPageAdd_Remove: return "Removed by Halloween Spell Page Apply";

		case  k_EItemActionSupportStrangify_Add: return "Added by Support Strangify";
		case  k_EItemActionSupportStrangify_Remove: return "Removed by Support Strangify";

		case  k_EItemActionUpgradeCardApply_Add: return "Added by Upgrade Card Apply";
		case  k_EItemActionUpgradeCardApply_Remove: return "Removed by Upgrade Card Apply";
		case  k_EItemActionUpgradeCardRemove_Add: return "Added by Upgrade Card Remove";
		case  k_EItemActionUpgradeCardRemove_Remove: return "Removed by Upgrade Card Remove";

#ifdef STAGING_ONLY
		case  k_EItemActionDev_ClientLootListRoll: return "Dev: Client Loot List Roll";
#endif

		case  k_EItemActionPeriodicScoreReward_Add: return "Periodic Score System Reward Grant Add";
		case  k_EItemActionPeriodicScoreReward_Remove: return "Periodic Score System Reward Remove";

		case  k_EItemActionGiftWrap_Add: return "Wrapped Gift - Added";
		case  k_EItemActionGiftWrap_Remove: return "Wrapped Gift - Remove";
		case  k_EItemActionGiftDelivery_Add: return "Gift Delivered - Add";
		case  k_EItemActionGiftDelivery_Remove: return "Gift Delivered - Remove";
		case  k_EItemActionGiftUnwrap_Add: return "Unwrapped Gift - Added";
		case  k_EItemActionGiftUnwrap_Remove: return "Unwrapped Gift - Remove";
		case  k_EItemActionPackageItem: return "Added by Steam Purchase";
		case  k_EItemActionPackageItem_Revoked: return "Revoked by Steam Purchase";
		case  k_EItemActionHandleMapToken: return "Map Token Applied";
		case  k_EItemActionCafeOrSchoolItem_Remove: return "Cafe or School Removal";
		case  k_EItemActionVACBanned_Remove: return "VAC Ban removal";
		case  k_EItemActionUpgradeThirdPartyPromo: return "Third-Party Promo Upgrade";
		case  k_EItemActionExpired: return "Expired";
		case  k_EItemActionTradeRollback_Add: return "Trade Rollback";
		case  k_EItemActionTradeRollback_Remove: return "Trade Rollback";
		case  k_EItemActionCDKeyGrant: return "CD Key Grant";
		case  k_EItemActionCDKeyRevoke: return "CD Key Rollback";
		case  k_EItemActionWeddingRing_Add: return "Wedding Ring Add";
		case  k_EItemActionWeddingRing_Remove: return "Wedding Ring Remove";
		case  k_EItemActionWeddingRing_AddPartner: return "Wedding Ring Add (Partner)";

		case  k_EItemActionEconSetUnowned: return "Econ SetUnowned";
		case  k_EItemActionEconSetOwned: return "Econ SetOwned";

		case  k_EItemActionStrangifyItem_Add:		return "Added by Strangify Apply";
		case  k_EItemActionStrangifyItem_Remove:	return "Removed by Strangify Apply";

		case  k_EItemActionRemoveItemCraftIndex_Remove: return "Removed by Craft Index Removal";
		case  k_EItemActionRemoveItemCraftIndex_Add: return "Added by Craft Index Removal";
		case  k_EItemActionRemoveItemMakersMark_Remove: return "Removed by Maker's Mark Removal";
		case  k_EItemActionRemoveItemMakersMark_Add: return "Added by Maker's Mark Removal";
		case  k_EItemActionRemoveItemKillStreak_Remove: return "Removed by KillStreak Removal";
		case  k_EItemActionRemoveItemKillStreak_Add: return "Added by KillStreak Removal";

		case  k_EItemActionCollectItem_CollectedItem: return "Item Collection: Added to a Collection";
		case  k_EItemActionCollectItem_RemoveCollection: return "Item Collection: Removed Old Collection Item";
		case  k_EItemActionCollectItem_UpdateCollection: return "Item Collection: Created New Collection Item";
		case  k_EItemActionCollectItem_RedeemCollectionReward: return "Item Collection: Redeemed Reward";

		case  k_EItemActionPreviewItem_BeginPreviewPeriod: return "Item Preview: Begin Preview Period";
		case  k_EItemActionPreviewItem_EndPreviewPeriodExpired: return "Item Preview: End Preview Period by Expiration";
		case  k_EItemActionPreviewItem_EndPreviewPeriodItemBought: return "Item Preview: End Preview Period by Item Purchase";

		case  k_EItemActionMvM_ChallengeCompleted_RemoveTicket: return "Removed MvM Ticket";
		case  k_EItemActionMvM_ChallengeCompleted_GrantBadge: return "Granted MvM Badge";
		case  k_EItemActionMvM_ChallengeCompleted_UpdateBadgeStamps_Add: return "Added by MvM Stamp Update";
		case  k_EItemActionMvM_ChallengeCompleted_UpdateBadgeStamps_Remove: return "Removed by MvM Stamp Update";
		case  k_EItemActionMvM_ChallengeCompleted_GrantMissionCompletionLoot: return "Added by MvM Mission Completion";
		case  k_EItemActionMvM_RemoveSquadSurplusVoucher: return "Removed MvM squad surplus voucher";
		case  k_EItemActionMvM_AwardSquadSurplus_Receiver: return "Received MvM squad surplus";
		case  k_EItemActionMvM_AwardSquadSurplus_Giver: return "Generated MvM squad surplus";
		case  k_EItemActionMvM_ChallengeCompleted_GrantTourCompletionLoot: return "Added by MvM Tour Completion";
		case  k_EItemActionMvM_AwardHelpANoobBonus_Helper: return "Added by MvM Help-A-Noob";

		case k_EItemActionHalloween_UpdateMerasmusLootLevel_Add: return "Updating Halloween Merasmus Loot - Adding New item";
		case k_EItemActionHalloween_UpdateMerasmusLootLevel_Remove: return "Updating Halloween Merasmus Loot Level - Removing Old Item";

		case k_EItemActionConsumeItem_Consume_ToolRemove: return "Dynamic Recipe: Consuming Input Item - Removing Old Recipe-Item";
		case k_EItemActionConsumeItem_Consume_ToolAdd: return "Dynamic Recipe: Consuming Input Item - Adding New Recipe-Item";
		case k_EItemActionConsumeItem_Consume_InputRemove: return "Dynamic Recipe: Consuming Input Item - Removing Input item";
		case k_EItemActionConsumeItem_Complete_OutputAdd: return "Dynamic Recipe: Recipe Complete - Adding Output Item";
		case k_EItemActionConsumeItem_Complete_ToolRemove: return "Dynamic Recipe: Recipe Complete - Removing Recipe-Item";

		case  k_EItemActionItemEaterRecharge_Add: return "Added by Item Eater Recharger";
		case  k_EItemActionItemEaterRecharge_Remove: return "Removed by Item Eater Recharger";

		case k_EItemActionSupportAddOrModifyAttribute_Remove: return "Removed by Support Add/Modify Attribute";
		case k_EItemActionSupportAddOrModifyAttribute_Add: return "Added by Support Add/Modify Attribute";

		case k_EItemAction_UpdateDuckBadgeLevel_Add: return "Updating Duck Badge Level - Adding New item";
		case k_EItemAction_UpdateDuckBadgeLevel_Remove: return "Updating Duck Badge Level - Removing Old Item";

		case k_EItemActionSpyVsEngyWar_JoinedWar: return "Added by joining the Spy vs. Engy War";

		case k_EItemAction_OperationPass_Add: return "Adding Operation Pass";

		case k_EItemAction_QuestDrop: return "Quest Drop";

		case k_EItemActionMarket_Add: return "Market - Add";
		case k_EItemActionMarket_Remove: return "Market - Remove";

		case k_EItemAction_QuestComplete_Reward: return "Added from completing a quest";
		case k_EItemAction_QuestComplete_Remove: return "Removed from completing a quest";

		case k_EItemActionStrangeCountTransfer_Add: return "Added by using a Strange Count Transfer tool";
		case k_EItemActionStrangeCountTransfer_Remove: return "Removed by using a Strange Count Transfer tool";

		case k_EItemActionCraftCollectionUpgrade_Add: return "Added by using Craft Collection Upgrade";
		case k_EItemActionCraftCollectionUpgrade_Remove: return "Removed by using Craft Collection Upgrade";

		case k_EItemActionCraftHalloweenOffering_Add: return "Added by using Craft Halloween Offering";
		case k_EItemActionCraftHalloweenOffering_Remove: return "Removed by using Craft Halloween Offering";

		case  k_EItemActionRemoveItemGiftedBy_Remove: return "Removed by Gifted By Removal";
		case  k_EItemActionRemoveItemGiftedBy_Add: return "Added by Gifted By Removal";

		case  k_EItemActionAddParticleVerticalAttr_Remove: return "Removed by Particle Vertical Attr Add";
		case  k_EItemActionAddParticleVerticalAttr_Add: return "Added by Particle Vertical Attr Add";

		case  k_EItemActionAddParticleUseHeadOriginAttr_Remove: return "Removed by Particle Use Head Origin Attr Add";
		case  k_EItemActionAddParticleUseHeadOriginAttr_Add: return "Added by Particle Use Head Origin Attr Add";

		case  k_EItemActionRemoveItemDynamicAttr_Remove: return "Removed by Dynamic Attribute Removal";
		case  k_EItemActionRemoveItemDynamicAttr_Add: return "Added by Dynamic Attribute Removal";

		case k_EItemActionCraftStatClockTradeUp_Add: return "Added through Stat Clock Trade-Up";
		case k_EItemActionCraftStatClockTradeUp_Remove: return "Consumed through Stat Clock Trade-Up";
		case k_EItemActionViralCompetitiveBetaPass_Drop: return "Added via Competitive Beta Invite Drop";

		case k_EItemActionSupportDeleteAttribute_Remove: return "Removed by Support Delete Attribute";
		case k_EItemActionSupportDeleteAttribute_Add: return "Added by Support Delete Attribute";

	default:
		return eMissingBehavior == kEItemAction_FriendlyNameLookup_ReturnDummyStringIfMissing
			 ? PchNameFromEItemAction( eItemAction )
			 : PchNameFromEItemActionUnsafe( eItemAction );
	}

}

const char *PchLocalizedNameFromEItemAction( EItemAction eAction, CLocalizationProvider &localizationProvider )
{
	const char *pchAction = PchNameFromEItemAction( eAction );
	if ( !V_strncmp( pchAction, "k_EItemAction", 13 ) )
	{
		CFmtStr fmtLocKey( "ItemHistory_Action_%s", &pchAction[13] );
		locchar_t *pchLocalizedAction = localizationProvider.Find( fmtLocKey.String() );
		if ( pchLocalizedAction != NULL )
		{
			return pchLocalizedAction;
		}
	}

	if ( BIsActionDestructive( eAction ) )
	{
		return localizationProvider.Find( "ItemHistory_Action_GenericRemove" );
	}
	else if ( BIsActionCreative( eAction ) )
	{
		return localizationProvider.Find( "ItemHistory_Action_GenericAdd" );
	}
	else
	{
		return "Unknown";
	}
}

ENUMSTRINGS_START( eEconItemOrigin )
{ kEconItemOrigin_Drop,                           "Timed Drop"                         },
{ kEconItemOrigin_Achievement,                    "Achievement"                        },
{ kEconItemOrigin_Purchased,                      "Purchased"                          },
{ kEconItemOrigin_Traded,                         "Traded"                             },
{ kEconItemOrigin_Crafted,                        "Crafted"                            },
{ kEconItemOrigin_StorePromotion,                 "Store Promotion"                    },
{ kEconItemOrigin_Gifted,                         "Gifted"                             },
{ kEconItemOrigin_SupportGranted,                 "Support Granted"                    },
{ kEconItemOrigin_FoundInCrate,                   "Found in Crate"                     },
{ kEconItemOrigin_Earned,                         "Earned"                             },
{ kEconItemOrigin_ThirdPartyPromotion,            "Third-Party Promotion"              },
{ kEconItemOrigin_GiftWrapped,                    "Wrapped Gift"                       },
{ kEconItemOrigin_HalloweenDrop,                  "Halloween Drop"                     },
{ kEconItemOrigin_PackageItem,                    "Steam Purchase"                     },
{ kEconItemOrigin_Foreign,                        "Foreign Item"                       },
{ kEconItemOrigin_CDKey,                          "CD Key"                             },
{ kEconItemOrigin_CollectionReward,               "Collection Reward"                  },
{ kEconItemOrigin_PreviewItem,                    "Preview Item"                       },
{ kEconItemOrigin_SteamWorkshopContribution,      "Steam Workshop Contribution"        },
{ kEconItemOrigin_PeriodicScoreReward,            "Periodic score reward"              },
{ kEconItemOrigin_MvMMissionCompletionReward,     "MvM Badge completion reward"        },
{ kEconItemOrigin_MvMSquadSurplusReward,          "MvM Squad surplus reward"           },
{ kEconItemOrigin_RecipeOutput,                   "Recipe output"                      },
{ kEconItemOrigin_QuestDrop,                      "Quest Drop"                         },
{ kEconItemOrigin_QuestLoanerItem,                "Quest Loaner Item"                  },
{ kEconItemOrigin_TradeUp,                        "Trade-Up"                           },
{ kEconItemOrigin_ViralCompetitiveBetaPassSpread, "Viral Competitive Beta Pass Spread" },
ENUMSTRINGS_END( eEconItemOrigin )

#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCrateLootListWrapper::BAttemptCrateSeriesInitialization( const IEconItemInterface *pEconItem )
{
	Assert( m_pLootList == NULL );

	// Find out what series this crate belongs to.
	static CSchemaAttributeDefHandle pAttr_CrateSeries( "set supply crate series" );
	if ( !pAttr_CrateSeries )
		return false;

	int iCrateSeries;
	{
		float fCrateSeries;		// crate series ID is stored as a float internally because we hate ourselves
		if ( !FindAttribute_UnsafeBitwiseCast<attrib_value_t>( pEconItem, pAttr_CrateSeries, &fCrateSeries ) || fCrateSeries == 0.0f )
			return false;

		iCrateSeries = fCrateSeries;
	}

	// Our index is an index into the revolving-loot-lists list. From that list we'll be able to
	// get a loot list name, which we'll use to look up the actual contents.
	const CEconItemSchema::RevolvingLootListDefinitionMap_t& mapRevolvingLootLists = GetItemSchema()->GetRevolvingLootLists();
	int idx = mapRevolvingLootLists.Find( iCrateSeries );
	if ( !mapRevolvingLootLists.IsValidIndex( idx ) )
		return false;

	const char *pszLootList = mapRevolvingLootLists.Element( idx );

	// Get the loot list.
	m_pLootList = GetItemSchema()->GetLootListByName( pszLootList );
	m_unAuditDetailData = iCrateSeries;
		
	return m_pLootList != NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCrateLootListWrapper::BAttemptLootListStringInitialization( const IEconItemInterface *pEconItem )
{
	Assert( m_pLootList == NULL );

	// Find out what series this crate belongs to.
	static CSchemaAttributeDefHandle pAttr_LootListName( "loot list name" );
	if ( !pAttr_LootListName )
		return false;

	CAttribute_String str;
	if ( !pEconItem->FindAttribute( pAttr_LootListName, &str ) )
		return false;

	m_pLootList = GetItemSchema()->GetLootListByName( str.value().c_str() );

	return m_pLootList != NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCrateLootListWrapper::BAttemptLineItemInitialization( const IEconItemInterface *pEconItem )
{
	Assert( m_pLootList == NULL );

	// Do we have at least one line item specified?
	if ( !pEconItem->FindAttribute( CAttributeLineItemLootList::s_pAttrDef_RandomDropLineItems[0] ) )
		return false;

	m_pLootList = new CAttributeLineItemLootList( pEconItem );
	m_bIsDynamicallyAllocatedLootList = true;
		
	return true;
}

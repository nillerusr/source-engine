//========= Copyright Valve Corporation, All rights reserved. ============//
//
//===================================================================

#include "cbase.h"
#include "econ_item_preset.h"
#include "tier1/generichash.h"

#ifdef GC_DLL
#include "gcsdk/sqlaccess/sqlaccess.h"
#endif

using namespace GCSDK;

#ifdef GC_DLL
IMPLEMENT_CLASS_MEMPOOL( CEconItemPerClassPresetData, 10 * 1000, UTLMEMORYPOOL_GROW_SLOW );
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
CEconItemPerClassPresetData::CEconItemPerClassPresetData()
	: m_unAccountID( 0 )
	, m_unClassID( (equipped_class_t)-1 )
	, m_unActivePreset( INVALID_PRESET_INDEX )
{
}

CEconItemPerClassPresetData::CEconItemPerClassPresetData( uint32 unAccountID, equipped_class_t unClassID )
	: m_unAccountID( unAccountID )
	, m_unClassID( unClassID )
	, m_unActivePreset( 0 )
{
}

void CEconItemPerClassPresetData::SerializeToProtoBufItem( CSOClassPresetClientData& msgPresetData ) const
{
	msgPresetData.set_account_id( m_unAccountID );
	msgPresetData.set_class_id( m_unClassID );
	msgPresetData.set_active_preset_id( m_unActivePreset );
}

void CEconItemPerClassPresetData::DeserializeFromProtoBufItem( const CSOClassPresetClientData &msgPresetData )
{
	m_unAccountID	 = msgPresetData.account_id();
	m_unClassID		 = msgPresetData.class_id();
	m_unActivePreset = msgPresetData.active_preset_id();
}

bool CEconItemPerClassPresetData::BIsKeyLess( const CSharedObject& soRHS ) const
{
	const CEconItemPerClassPresetData *soPresetData = assert_cast< const CEconItemPerClassPresetData * >( &soRHS );

	Assert( m_unAccountID == soPresetData->m_unAccountID );

	return m_unClassID < soPresetData->m_unClassID;
}

#ifdef GC
static bool BYieldingAddPresetItemRowsForSpecificPreset( GCSDK::CSQLAccess &sqlAccess, CSchItemPresetInstance& schItemPresetInstance, const CUtlVector<PresetSlotItem_t>& vecPresetData )
{
	FOR_EACH_VEC( vecPresetData, j )
	{
		schItemPresetInstance.m_unSlotID = vecPresetData[j].m_unSlotID;
		schItemPresetInstance.m_ulItemID = vecPresetData[j].m_ulItemOriginalID;
		if ( !sqlAccess.BYieldingInsertRecord( &schItemPresetInstance ) )
			return false;
	}

	return true;
}

bool CEconItemPerClassPresetData::BYieldingAddInsertToTransaction( GCSDK::CSQLAccess &sqlAccess )
{
	// Write out the preset data for our selected items.
	CSchItemPresetInstance schItemPresetInstance;
	schItemPresetInstance.m_unAccountID = m_unAccountID;
	schItemPresetInstance.m_unClassID = m_unClassID;

	for ( int i = 0; i < ARRAYSIZE( m_PresetData ); i++ )
	{
		schItemPresetInstance.m_unPresetID = i;

		if ( !BYieldingAddPresetItemRowsForSpecificPreset( sqlAccess, schItemPresetInstance, m_PresetData[i] ) )
			return false;
	}

	// Write out the data for which preset is active for this class.
	CSchSelectedItemPreset schSelectedItemPreset;
	schSelectedItemPreset.m_unAccountID = m_unAccountID;
	schSelectedItemPreset.m_unClassID = m_unClassID;
	schSelectedItemPreset.m_unPresetID = m_unActivePreset;

	if ( !sqlAccess.BYieldingInsertRecord( &schSelectedItemPreset ) )
		return false;

	return true;
}

bool CEconItemPerClassPresetData::BYieldingAddWriteToTransaction( GCSDK::CSQLAccess &sqlAccess, const CUtlVector< int > &fields )
{
	Assert( sqlAccess.BInTransaction() );

	FOR_EACH_VEC( fields, i )
	{
		const int iField = fields[i];

		if ( iField == kPerClassPresetDataDirtyField_ActivePreset )
		{
			CSchSelectedItemPreset schSelectedItemPreset;
			schSelectedItemPreset.m_unAccountID = m_unAccountID;
			schSelectedItemPreset.m_unClassID = m_unClassID;
			schSelectedItemPreset.m_unPresetID = m_unActivePreset;

			if ( !sqlAccess.BYieldingUpdateRecord( schSelectedItemPreset, CSET_2_COL( CSchSelectedItemPreset, k_iField_unAccountID, k_iField_unClassID ), CSET_1_COL( CSchSelectedItemPreset, k_iField_unPresetID ) ) )
				return false;
		}
		else if ( iField >= kPerClassPresetDataDirtyField_PresetData_Base )
		{
			int iDirtyPreset = iField - kPerClassPresetDataDirtyField_PresetData_Base;
			Assert( iDirtyPreset >= 0 );
			Assert( iDirtyPreset < ARRAYSIZE( m_PresetData ) );

			// First, remove any existing rows for this preset.
			CSchItemPresetInstance schItemPresetInstance;
			schItemPresetInstance.m_unAccountID = m_unAccountID;
			schItemPresetInstance.m_unClassID	= m_unClassID;
			schItemPresetInstance.m_unPresetID	= iDirtyPreset;

			if ( !sqlAccess.BYieldingDeleteRecords( schItemPresetInstance, CSET_3_COL( CSchItemPresetInstance, k_iField_unAccountID, k_iField_unPresetID, k_iField_unClassID ) ) )
				return false;

			// Don't write out data for our currently-equipped items. We'll handle these by
			// writing them out as actually equipped.
			if ( iDirtyPreset != GetActivePreset() )
			{
				// Add our new rows.
				if ( !BYieldingAddPresetItemRowsForSpecificPreset( sqlAccess, schItemPresetInstance, m_PresetData[iDirtyPreset] ) )
					return false;
			}
		}
	}

	return true;
}

bool CEconItemPerClassPresetData::BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess &sqlAccess )
{
	CSchItemPresetInstance schItemPresetInstance;
	schItemPresetInstance.m_unAccountID = m_unAccountID;
	schItemPresetInstance.m_unClassID = m_unClassID;

	if ( !sqlAccess.BYieldingDeleteRecords( schItemPresetInstance, CSET_2_COL( CSchItemPresetInstance, k_iField_unAccountID, k_iField_unClassID ) ) )
		return false;

	CSchSelectedItemPreset schSelectedItemPreset;
	schSelectedItemPreset.m_unAccountID = m_unAccountID;
	schSelectedItemPreset.m_unClassID = m_unClassID;
	
	if ( !sqlAccess.BYieldingDeleteRecords( schSelectedItemPreset, CSET_2_COL( CSchSelectedItemPreset, k_iField_unAccountID, k_iField_unClassID ) ) )
		return false;

	return true;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItemPerClassPresetData::BAddToMessage( CUtlBuffer & bufOutput ) const
{
	CSOClassPresetClientData msgClientPresetData;
	SerializeToProtoBufItem( msgClientPresetData );
	return CProtoBufSharedObjectBase::SerializeToBuffer( msgClientPresetData, bufOutput );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItemPerClassPresetData::BAddToMessage( std::string *pBuffer ) const
{
	CSOClassPresetClientData msgClientPresetData;
	SerializeToProtoBufItem( msgClientPresetData );
	return msgClientPresetData.SerializeToString( pBuffer );
}

//----------------------------------------------------------------------------
// Purpose: Adds just the item ID to the message so that the client can find
//			which item to destroy
//----------------------------------------------------------------------------
bool CEconItemPerClassPresetData::BAddDestroyToMessage( CUtlBuffer & bufDestroy ) const
{
	CSOClassPresetClientData msgClientPresetData;
	msgClientPresetData.set_class_id( m_unClassID );
	return CProtoBufSharedObjectBase::SerializeToBuffer( msgClientPresetData, bufDestroy );
}

//----------------------------------------------------------------------------
// Purpose: Adds just the item ID to the message so that the client can find
//			which item to destroy
//----------------------------------------------------------------------------
bool CEconItemPerClassPresetData::BAddDestroyToMessage( std::string *pBuffer ) const
{
	CSOClassPresetClientData msgClientPresetData;
	msgClientPresetData.set_class_id( m_unClassID );
	return msgClientPresetData.SerializeToString( pBuffer );
}
#endif

bool CEconItemPerClassPresetData::BParseFromMessage( const CUtlBuffer & buffer )
{
	CSOClassPresetClientData msgClientPresetData;
	if( !msgClientPresetData.ParseFromArray( buffer.Base(), buffer.TellMaxPut() ) )
		return false;

	DeserializeFromProtoBufItem( msgClientPresetData );

	return true;
}

bool CEconItemPerClassPresetData::BParseFromMessage( const std::string &buffer )
{
	CSOClassPresetClientData msgClientPresetData;
	if( !msgClientPresetData.ParseFromString( buffer ) )
		return false;

	DeserializeFromProtoBufItem( msgClientPresetData );

	return true;
}

//----------------------------------------------------------------------------
// Purpose:
//----------------------------------------------------------------------------
bool CEconItemPerClassPresetData::BUpdateFromNetwork( const CSharedObject & objUpdate )
{
	Copy( objUpdate );
	return true;
}

void CEconItemPerClassPresetData::Copy( const CSharedObject & soRHS )
{
	const CEconItemPerClassPresetData& rhs = static_cast<const CEconItemPerClassPresetData&>( soRHS );

	m_unAccountID = rhs.m_unAccountID;
	m_unClassID		= rhs.m_unClassID;
	m_unActivePreset = rhs.m_unActivePreset;

	for ( int i = 0; i < ARRAYSIZE( m_PresetData ); i++ )
	{
		m_PresetData[i].CopyArray( rhs.m_PresetData[i].Base(), rhs.m_PresetData[i].Count() );
	}
}

void CEconItemPerClassPresetData::Dump() const
{
#if 0
	EmitInfo( SPEW_GC, SPEW_ALWAYS, LOG_ALWAYS, "preset id=%d  class id=%d   slot id=%d  item id=%llu\n",
		m_unPresetID, m_unClassID, m_unSlotID, m_ulItemID );
#endif
}

#ifdef GC_DLL
//----------------------------------------------------------------------------
// Purpose:
//----------------------------------------------------------------------------
const CUtlVector<PresetSlotItem_t> *CEconItemPerClassPresetData::FindItemsForPresetIndex( equipped_preset_t unPreset ) const
{
	if ( unPreset >= ARRAYSIZE( m_PresetData ) )
		return NULL;

	return &m_PresetData[ unPreset ];
}

//----------------------------------------------------------------------------
// Purpose:
//----------------------------------------------------------------------------
void CEconItemPerClassPresetData::SetActivePreset( equipped_preset_t unPreset )
{
	if ( unPreset >= ARRAYSIZE( m_PresetData ) )
		return;

	m_unActivePreset = unPreset;
}

//----------------------------------------------------------------------------
// Purpose:
//----------------------------------------------------------------------------
void CEconItemPerClassPresetData::EquipItemIntoActivePresetSlot( equipped_slot_t unSlot, itemid_t unOriginalItemID )
{
	Assert( GetItemSchema()->IsValidItemSlot( unSlot, EQUIP_TYPE_CLASS ) );
	Assert( m_unActivePreset < ARRAYSIZE( m_PresetData ) );

	auto& PresetData = m_PresetData[m_unActivePreset];

	FOR_EACH_VEC( PresetData, i )
	{
		if ( PresetData[i].m_unSlotID == unSlot )
		{
			// If we're unequipping, stop tracking this slot.
			if ( unOriginalItemID == INVALID_ITEM_ID )
			{
				PresetData.FastRemove( i );
			}
			// Otherwise store the current reference.
			else
			{
				PresetData[i].m_ulItemOriginalID = unOriginalItemID;
			}
			return;
		}
	}

	// We don't expect to get here without having an item equipped already, but it's possible if
	// items get swapped around on the back end, or if we process messages out of order and/or drop
	// some.
	if ( unOriginalItemID != INVALID_ITEM_ID )
	{
		PresetSlotItem_t PresetSlotItem;
		PresetSlotItem.m_unSlotID = unSlot;
		PresetSlotItem.m_ulItemOriginalID = unOriginalItemID;
		PresetData.AddToTail( PresetSlotItem );
	}
}

//----------------------------------------------------------------------------
// Purpose:
//----------------------------------------------------------------------------
void CEconItemPerClassPresetData::RemoveAllItemsFromPresetIndex( equipped_preset_t unPreset )
{
	if ( unPreset >= ARRAYSIZE( m_PresetData ) )
		return;

	m_PresetData[unPreset].Purge();
}
#endif // GC_DLL
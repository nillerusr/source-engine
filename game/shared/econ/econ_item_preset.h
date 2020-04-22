//========= Copyright Valve Corporation, All rights reserved. ============//
//
//===================================================================

#ifndef ECONITEMPRESET_H
#define ECONITEMPRESET_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/protobufsharedobject.h"
#include "gcsdk/gcclientsdk.h"
#include "base_gcmessages.pb.h"

#include "econ/econ_item_constants.h"

namespace GCSDK
{
	class CSQLAccess;
};

class CSOClassPresetClientData;

typedef uint8	equipped_preset_t;

struct PresetSlotItem_t
{
#ifdef GC_DLL
	DECLARE_CLASS_MEMPOOL( PresetSlotItem_t );
#endif

	equipped_slot_t		m_unSlotID;
	itemid_t			m_ulItemOriginalID;		// Original ID of the item in this slot. We store this instead of the current ID to avoid breaking presets when items get renamed, etc.
};

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
class CEconItemPerClassPresetData : public GCSDK::CSharedObject
{
#ifdef GC_DLL
	DECLARE_CLASS_MEMPOOL( CEconItemPerClassPresetData );
#endif

public:
	typedef GCSDK::CSharedObject BaseClass;

	const static int k_nTypeID = k_EEconTypeItemPresetInstance;
	virtual int GetTypeID() const OVERRIDE { return k_nTypeID; }

	CEconItemPerClassPresetData();
	CEconItemPerClassPresetData( uint32 unAccountID, equipped_class_t unClassID );

	virtual bool BIsKeyLess( const CSharedObject& soRHS ) const;

#ifdef GC
	virtual bool BYieldingAddInsertToTransaction( GCSDK::CSQLAccess &sqlAccess ) OVERRIDE;
	virtual bool BYieldingAddWriteToTransaction( GCSDK::CSQLAccess &sqlAccess, const CUtlVector< int > &fields ) OVERRIDE;
	virtual bool BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess &sqlAccess ) OVERRIDE;
	virtual bool BAddToMessage( CUtlBuffer & bufOutput ) const OVERRIDE;
	virtual bool BAddToMessage( std::string *pBuffer ) const OVERRIDE;
	virtual bool BAddDestroyToMessage( CUtlBuffer & bufDestroy ) const OVERRIDE;
	virtual bool BAddDestroyToMessage( std::string *pBuffer ) const OVERRIDE;
#endif

	virtual bool BParseFromMessage( const CUtlBuffer & buffer ) OVERRIDE;
	virtual bool BParseFromMessage( const std::string &buffer ) OVERRIDE;
	virtual bool BUpdateFromNetwork( const CSharedObject & objUpdate ) OVERRIDE;
	virtual void Copy( const CSharedObject & soRHS );
	virtual void Dump() const;

	void SerializeToProtoBufItem( CSOClassPresetClientData &msgPresetInstance ) const;
	void DeserializeFromProtoBufItem( const CSOClassPresetClientData &msgPresetIntance );

	enum
	{
		kPerClassPresetDataDirtyField_ActivePreset,
		kPerClassPresetDataDirtyField_PresetData_Base,
	};

#ifdef GC_DLL
	const CUtlVector<PresetSlotItem_t> *FindItemsForPresetIndex( equipped_preset_t unPreset ) const;
	void EquipItemIntoActivePresetSlot( equipped_slot_t unSlot, itemid_t unOriginalItemID );
	void RemoveAllItemsFromPresetIndex( equipped_preset_t unPreset );

	void SetActivePreset( equipped_preset_t unPreset );
	equipped_class_t GetClass() const { return m_unClassID; }
#endif // GC_DLL
	equipped_preset_t GetActivePreset() const { return m_unActivePreset; }

private:
	CEconItemPerClassPresetData( const CEconItemPerClassPresetData& ) = delete;
	void operator=( const CEconItemPerClassPresetData& ) = delete;

private:
	uint32							m_unAccountID;
	equipped_class_t				m_unClassID;
	equipped_preset_t				m_unActivePreset;
	CUtlVector<PresetSlotItem_t>	m_PresetData[ CEconItemSchema::kMaxItemPresetCount ];
};

#endif // ECONITEMPRESET_H

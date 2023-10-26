//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Definition of an enumeration used in macro substitution for the layout system.
//
//===============================================================================

#ifndef TILEGEN_ENUM_H
#define TILEGEN_ENUM_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "utlvector.h"
#include "tilegen_core.h"

class KeyValues;
class CTilegenMissionPreprocessor;

//-----------------------------------------------------------------------------
// A single value in the enumeration.
//-----------------------------------------------------------------------------
struct EnumEntry_t
{
	// Integer value
	int m_nValue;
	// String identifier
	const char *m_pString;
};

//-----------------------------------------------------------------------------
// A parsed and extracted enumeration definition.
//-----------------------------------------------------------------------------
class CTilegenEnum
{
public:
	CTilegenEnum();

	//-----------------------------------------------------------------------------
	// Parses the enum and takes ownership of the passed in KeyValues data.
	// NOTE: Caller must not free or modify the KeyValues data after this call.
	//-----------------------------------------------------------------------------
	bool LoadFromKeyValues( KeyValues *pEnumKeyValues );

	const char *GetName() const { return m_pEnumName; }

	int GetEnumCount() const { return m_Entries.Count(); }
	int GetEnumValue( int nEntry ) const { return m_Entries[nEntry].m_nValue; }
	int FindEntry( int nValue ) const;
	const char *GetEnumString( int nEntry ) const { return m_Entries[nEntry].m_pString; }
	const char *GetEnumStringFromValue( int nValue ) const;

private:
	// Ownership of this KeyValues pointer is transferred to CTilegenEnum which is responsible for cleanup.
	KeyValues *m_pEnumKV;

	// No need to delete any of this data, as it is all owned by m_pEnumKV data or automatically destructed.

	const char *m_pEnumName;

	CUtlVector< EnumEntry_t > m_Entries;
};

#endif // TILEGEN_ENUM_H
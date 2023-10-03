//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Definition of an enumeration used in macro substitution for the layout system.
//
//===============================================================================

#include "KeyValues.h"
#include "tilegen_mission_preprocessor.h"
#include "tilegen_enum.h"

//////////////////////////////////////////////////////////////////////////
// Public Implementation
//////////////////////////////////////////////////////////////////////////

CTilegenEnum::CTilegenEnum() :
m_pEnumName( NULL )
{

}

bool CTilegenEnum::LoadFromKeyValues( KeyValues *pEnumKeyValues )
{
	m_pEnumKV = pEnumKeyValues;

	m_pEnumName = m_pEnumKV->GetString( "name", "" );
	if ( m_pEnumName[0] == '\0' )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Enum must have a valid 'name' key.\n" );
		return false;
	}

	KeyValues *pValuesKV = m_pEnumKV->FindKey( "values" );
	if ( pValuesKV == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "No 'values' sub-key found inside enumeration '%s'.\n", m_pEnumName );
		return false;
	}

	for ( KeyValues *pSubKey = pValuesKV->GetFirstSubKey(); pSubKey != NULL; pSubKey = pSubKey->GetNextKey() )
	{
		EnumEntry_t enumEntry;
		enumEntry.m_pString = pSubKey->GetName();
		enumEntry.m_nValue = pSubKey->GetInt();

		for ( int i = 0; i < m_Entries.Count(); ++ i )
		{
			if ( Q_stricmp( m_Entries[i].m_pString, enumEntry.m_pString ) == 0 )
			{
				Log_Warning( LOG_TilegenLayoutSystem, "Duplicate enumeration string found in enum '%s' (names are case insensitive): %s", m_pEnumName, enumEntry.m_pString );
				return false;
			}

			// Duplicate values are explicitly allowed in case multiple names map to the same value.
		}

		m_Entries.AddToTail( enumEntry );		
	}

	return true;
}

int CTilegenEnum::FindEntry( int nValue ) const
{
	for ( int i = 0; i < m_Entries.Count(); ++ i )
	{
		if ( m_Entries[i].m_nValue == nValue )
		{
			return i;
		}
	}

	return -1;
}

const char *CTilegenEnum::GetEnumStringFromValue( int nValue ) const
{
	int nEntry = FindEntry( nValue );
	if ( nEntry != -1 )
	{
		return m_Entries[nEntry].m_pString;
	}
	else
	{
		return NULL;
	}
}
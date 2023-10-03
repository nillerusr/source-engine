//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Definition of a class to pre-process mission files and perform
// rule substitution.
//
//===============================================================================

#include "KeyValues.h"
#include "tilegen_core.h"
#include "tilegen_enum.h"
#include "tilegen_rule.h"
#include "tilegen_mission_preprocessor.h"

//////////////////////////////////////////////////////////////////////////
// Public Implementation
//////////////////////////////////////////////////////////////////////////

CTilegenMissionPreprocessor::~CTilegenMissionPreprocessor()
{
	m_Rules.PurgeAndDeleteElements();
}

const CTilegenRule *CTilegenMissionPreprocessor::FindRule( const char *pRuleName ) const
{
	for ( int i = 0; i < m_Rules.Count(); ++ i )
	{
		if ( Q_stricmp( m_Rules[i]->GetName(), pRuleName ) == 0 )
		{
			return m_Rules[i];
		}
	}
	return NULL;
}

const CTilegenEnum *CTilegenMissionPreprocessor::FindEnum( const char *pEnumName ) const
{
	for ( int i = 0; i < m_Enums.Count(); ++ i )
	{
		if ( Q_stricmp( m_Enums[i]->GetName(), pEnumName ) == 0 )
		{
			return m_Enums[i];
		}
	}
	return NULL;
}

bool CTilegenMissionPreprocessor::ParseAndStripRules( KeyValues *pRulesKV )
{
	for ( KeyValues *pSubKey = pRulesKV->GetFirstSubKey(); pSubKey != NULL; ) // sub-key gets advanced in the loop
	{
		if ( Q_stricmp( pSubKey->GetName(), "rule" ) == 0 )
		{
			KeyValues *pKeyValuesToRemove = pSubKey;
			pSubKey = pSubKey->GetNextKey();
			pRulesKV->RemoveSubKey( pKeyValuesToRemove );

			CTilegenRule *pNewRule = new CTilegenRule();
			if ( !pNewRule->LoadFromKeyValues( pKeyValuesToRemove ) )
			{
				delete pNewRule;
				return false;
			}
			m_Rules.AddToTail( pNewRule );
		}
		else if ( Q_stricmp( pSubKey->GetName(), "enum" ) == 0 )
		{
			KeyValues *pKeyValuesToRemove = pSubKey;
			pSubKey = pSubKey->GetNextKey();
			pRulesKV->RemoveSubKey( pKeyValuesToRemove );

			CTilegenEnum *pNewEnum = new CTilegenEnum();
			if ( !pNewEnum->LoadFromKeyValues( pKeyValuesToRemove ) )
			{
				delete pNewEnum;
				return false;
			}
			m_Enums.AddToTail( pNewEnum );
		}
		else
		{
			pSubKey = pSubKey->GetNextKey();
		}
	}
	return true;
}

bool CTilegenMissionPreprocessor::SubstituteRules( KeyValues *pKeyValues )
{
	m_nUniqueIndex = 0;
	for ( KeyValues *pSubKey = pKeyValues->GetFirstSubKey(); pSubKey != NULL; )
	{
		KeyValues *pNextSubKey = pSubKey->GetNextKey();
		
		// Rules should have been stripped out.
		Assert( Q_stricmp( pSubKey->GetName(), "rule" ) != 0 );

		if ( !SubstituteRules_Recursive( pKeyValues, pSubKey ) )
		{
			return false;
		}

		// pSubKey may have been deleted during the call and is no longer valid
		pSubKey = pNextSubKey;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// Private Implementation
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// During macro substitution, there are some meta-variables (starting with
// a '%' sign, e.g. %UniqueName) which are substituted for values
// using special logic in this function.
//-----------------------------------------------------------------------------
void CTilegenMissionPreprocessor::EvaluateMetaVariables( KeyValues *pKeyValues )
{
	const char *pString = pKeyValues->GetString();
	const char *pFind = "%Unique";
	
	if ( Q_stristr( pString, pFind ) != NULL )
	{
		char uniqueNumber[15];
		Q_snprintf( uniqueNumber, 15, "%02d", m_nUniqueIndex );
		char outputString[MAX_TILEGEN_IDENTIFIER_LENGTH];
		Q_StrSubst( pString, pFind, uniqueNumber, outputString, MAX_TILEGEN_IDENTIFIER_LENGTH );
		pKeyValues->SetString( NULL, outputString );		
	}
}

//-----------------------------------------------------------------------------
// Recursively looks for rule_instance blocks, finds the corresponding rule, 
// performs the substitution recursively, and deletes the old rule_instance
// block.
//-----------------------------------------------------------------------------
bool CTilegenMissionPreprocessor::SubstituteRules_Recursive( KeyValues *pParent, KeyValues *pKeyValues )
{
	if ( Q_stricmp( pKeyValues->GetName(), "rule_instance" ) == 0 )
	{
		if ( pKeyValues->GetBool( "disabled", false ) )
		{
			// The rule_instance has been disabled from the UI, ignore it and generate nothing in its place.
			pParent->RemoveSubKey( pKeyValues );
			pKeyValues->deleteThis();
		}
		else
		{
			const char *pRuleName = pKeyValues->GetString( "name", "" );
			if ( pRuleName[0] == '\0' )
			{
				Log_Warning( LOG_TilegenLayoutSystem, "Rule instance must have a valid 'name' key.\n" );
				return false;
			}
			const CTilegenRule *pRule = FindRule( pRuleName );
			if ( pRule == NULL )
			{
				Log_Warning( LOG_TilegenLayoutSystem, "Rule %s not found in rule list.\n", pRuleName );
				return false;
			}

			// Each instantiated rule needs a unique index to generate unique names.
			++ m_nUniqueIndex;

			KeyValues *pInstantiatedKV = pRule->InstantiateRule( pKeyValues, this );

			if ( pInstantiatedKV == NULL )
			{
				Log_Warning( LOG_TilegenLayoutSystem, "Error instantiating rule %s.\n", pRuleName );
				return false;
			}

			pParent->SwapSubKey( pKeyValues, pInstantiatedKV );

			// Recursively apply rules as needed.
			if ( !SubstituteRules_Recursive( pParent, pInstantiatedKV ) )
			{
				return false;
			}

			pParent->ElideSubKey( pInstantiatedKV );

			// Delete the rule_instance
			pKeyValues->deleteThis();
		}
		
	}
	else
	{
		for ( KeyValues *pSubKey = pKeyValues->GetFirstSubKey(); pSubKey != NULL;  )
		{
			KeyValues *pNextSubKey = pSubKey->GetNextKey();
			if ( !SubstituteRules_Recursive( pKeyValues, pSubKey ) )
			{
				return false;
			}
			// pSubKey may have been deleted during the call and is no longer valid
			pSubKey = pNextSubKey;
		}
	}
	return true;
}

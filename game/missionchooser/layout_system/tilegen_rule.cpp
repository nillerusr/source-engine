//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Definition of a rule used in macro substitution for the layout system.
//
//===============================================================================

#include "KeyValues.h"
#include "tilegen_mission_preprocessor.h"
#include "tilegen_rule.h"

//////////////////////////////////////////////////////////////////////////
// Forward Declarations
//////////////////////////////////////////////////////////////////////////

bool ParseSourceTag( const char *pSourceTag, bool *pAllowLiterals, bool *pAllowExpressions );
bool ParseArrayTag( const char *pArrayTag, bool *pArray, bool *pIsOrdered );
bool ParseTypeTag( const char *pTypeTag, CUtlVector< RuleType_t > *pTypeList );

//////////////////////////////////////////////////////////////////////////
// Public Implementation
//////////////////////////////////////////////////////////////////////////

CTilegenRule::CTilegenRule() :
	m_pRuleKV( NULL ),
	m_pSubstitutionKV( NULL ),
	m_pName( NULL ),
	m_pFriendlyName( NULL ),
	m_pDescription( NULL )
{
}

CTilegenRule::~CTilegenRule()
{
	m_pRuleKV->deleteThis();
}

bool CTilegenRule::LoadFromKeyValues( KeyValues *pRuleKeyValues )
{
	m_pRuleKV = pRuleKeyValues;
	m_pName = m_pRuleKV->GetString( "name", "" );
	if ( m_pName[0] == '\0' )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Rule must have a valid 'name' key.\n" );
		return false;
	}

	m_pFriendlyName = m_pRuleKV->GetString( "friendly_name", m_pName );
	m_pDescription = m_pRuleKV->GetString( "description", "No description." );
	m_bHidden = m_pRuleKV->GetBool( "hidden", true );

	const char *pType = m_pRuleKV->GetString( "type", NULL );
	if ( pType == NULL || !ParseTypeTag( pType, &m_Types ) )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Rule '%s' must have a valid 'type' key.\n", m_pName );
		return false;
	}

	m_pSubstitutionKV = m_pRuleKV->FindKey( "substitute" );
	if ( m_pSubstitutionKV == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "No 'substitute' blocks found in rule.\n" );
		return false;
	}

	// Rename the 'substitute' key value to 'node', which is simply a grouping node of multiple sub-expressions.
	m_pSubstitutionKV->SetName( "node" );

	for ( KeyValues *pSubKey = pRuleKeyValues->GetFirstSubKey(); pSubKey != NULL; pSubKey = pSubKey->GetNextKey() )
	{
		if ( Q_stricmp( pSubKey->GetName(), "param" ) == 0 )
		{
			SubstitutionVariable_t variable;
			variable.m_pName = pSubKey->GetString( "name", NULL );
			if ( variable.m_pName == NULL )
			{
				Log_Warning( LOG_TilegenLayoutSystem, "No 'name' specified for parameter in rule '%s'.\n", m_pName );
				return false;
			}
			else if ( variable.m_pName[0] != '$' )
			{
				Log_Warning( LOG_TilegenLayoutSystem, "First letter of parameter name ('%s') must start with '$'.\n", variable.m_pName );
				return false;
			}

			variable.m_pFriendlyName = pSubKey->GetString( "friendly_name", variable.m_pName );
			variable.m_pDescription = pSubKey->GetString( "description", "No description." );
			variable.m_pTypeName = pSubKey->GetString( "type", "string" );
			variable.m_pEnumName = pSubKey->GetString( "enum", NULL );
			if ( Q_stricmp( variable.m_pTypeName, "enum" ) == 0 )
			{
				if ( variable.m_pEnumName == NULL )
				{
					Log_Warning( LOG_TilegenLayoutSystem, "Parameter 'type' is 'enum' but no 'enum' key is specified.\n" );
					return false;
				}
			}
			
			variable.m_pDefault = pSubKey->FindKey( "default" );
			
			const char *pArrayType = pSubKey->GetString( "array", NULL );
			ParseArrayTag( pArrayType, &variable.m_bArray, &variable.m_bOrderedArray );

			variable.m_pElementContainer = pSubKey->GetString( "element_container", NULL );

			if ( variable.m_pDefault != NULL )
			{
				// Rename the default value key to the parameter name so it can be copied directly into place.
				variable.m_pDefault->SetName( variable.m_pName );
			}

			variable.m_bCanOmit = pSubKey->GetBool( "can_omit", false );
			// "array" implies that the field can be omitted, since a zero element array deletes the list
			variable.m_bCanOmit |= variable.m_bArray;

			const char *pSource = pSubKey->GetString( "source", "literal|expression" );
			if ( !ParseSourceTag( pSource, &variable.m_bAllowLiteral, &variable.m_bAllowExpression ) )
			{
				return false;
			}

			m_SubstitutionVariables.AddToTail( variable );
		}
	}
	return true;
}

KeyValues *CTilegenRule::InstantiateRule( KeyValues *pRuleInstanceKV, CTilegenMissionPreprocessor *pPreprocessor ) const
{
	for ( int i = 0; i < m_SubstitutionVariables.Count(); ++ i )
	{
		if ( !m_SubstitutionVariables[i].IsOptional() && pRuleInstanceKV->FindKey( m_SubstitutionVariables[i].m_pName ) == NULL )
		{
			Log_Warning( LOG_TilegenLayoutSystem, "Non-optional substitution variable '%s' not found in rule_instance.\n", m_SubstitutionVariables[i].m_pName );
			return NULL;
		}
	}

	KeyValues *pNewKeyValues = m_pSubstitutionKV->MakeCopy();

	// Perform fixup recursively
	RecursiveFixup( pNewKeyValues, pRuleInstanceKV, pPreprocessor );

	return pNewKeyValues;
}

//////////////////////////////////////////////////////////////////////////
// Private Implementation
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// Once a rule_instance has been matched to this rule and all non-optional
// parameters have been validated, this function performs
// the actual rule parameter substitution given the parameters from the 
// rule_instance.
//
// pNewInstanceKV - newly stamped out instance on which the 
//   operation is performed
// pRuleInstanceKV - original rule_instance which contains parameters 
//   to substitute
//-----------------------------------------------------------------------------
void CTilegenRule::RecursiveFixup( KeyValues *pNewInstanceKV, KeyValues *pRuleInstanceKV, CTilegenMissionPreprocessor *pPreprocessor ) const
{
	for ( KeyValues *pSubKey = pNewInstanceKV->GetFirstSubKey(); pSubKey != NULL; ) // pSubKey is advanced in the loop
	{
		int i;
		KeyValues *pNextKey = pSubKey->GetNextKey();

		pPreprocessor->EvaluateMetaVariables( pSubKey );

		for ( i = 0; i < m_SubstitutionVariables.Count(); ++ i )
		{
			// Compare the value of the copied rule's sub-key to see if it should be substituted.
			if ( Q_stricmp( pSubKey->GetString(), m_SubstitutionVariables[i].m_pName ) == 0 )
			{
				// This is the value of the parameter specified in the "rule_instance" block.
				// Its contents will replace pSubKey.
				KeyValues *pSubstituteKV = pRuleInstanceKV->FindKey( m_SubstitutionVariables[i].m_pName );

				// This value may only be NULL if the substitution variable is optional
				Assert( m_SubstitutionVariables[i].IsOptional() || pSubstituteKV != NULL );
				
				if ( pSubstituteKV == NULL )
				{
					// If default is NULL, then m_bCanOmit must be true
					pSubstituteKV = m_SubstitutionVariables[i].m_pDefault;
				}

				if ( pSubstituteKV != NULL )
				{
					// We must make a copy of the rule_instance's substitution in case it is inserted multiple times in the rule.
					// (The original rule_instance will be cleaned up after InstantiateRule().)
					pSubstituteKV = pSubstituteKV->MakeCopy();
					pNewInstanceKV->SwapSubKey( pSubKey, pSubstituteKV );
					pSubstituteKV->SetName( pSubKey->GetName() );
					// Nodes called "_elide" are automatically elided, which allows for flattening an array into
					// a parent hierarchy.
					if ( Q_stricmp( pSubKey->GetName(), "_elide" ) == 0 )
					{
						pNewInstanceKV->ElideSubKey( pSubstituteKV );
					}
				}
				else
				{
					Assert( m_SubstitutionVariables[i].m_bCanOmit );
					// Optional parameter not found; simply remove any reference to it.
					pNewInstanceKV->RemoveSubKey( pSubKey );
				}

				pSubKey->deleteThis();
				pSubKey = pNextKey;
				break;
			}
		}
		// Performed a substitution
		if ( i < m_SubstitutionVariables.Count() )
		{
			continue;
		}
		
		// No substitutions made on this value; recursively check it for more substitutions.
		RecursiveFixup( pSubKey, pRuleInstanceKV, pPreprocessor );
		pSubKey = pNextKey;
	}
}

bool ParseSourceTag( const char *pSourceTag, bool *pAllowLiterals, bool *pAllowExpressions )
{
	// @TODO: this is ghetto parsing
	if ( Q_stricmp( pSourceTag, "literal" ) == 0 )
	{
		*pAllowLiterals = true;
		*pAllowExpressions = false;
	}
	else if ( Q_stricmp( pSourceTag, "expression" ) == 0 )
	{
		*pAllowLiterals = false;
		*pAllowExpressions = true;
	}
	else if ( Q_stricmp( pSourceTag, "expression|literal" ) == 0 || Q_stricmp( pSourceTag, "literal|expression" ) == 0 )
	{
		*pAllowLiterals = true;
		*pAllowExpressions = true;
	}
	else
	{
		*pAllowLiterals = false;
		*pAllowExpressions = false;
		Log_Warning( LOG_TilegenLayoutSystem, "Unrecognized 'source' tag in rule parameter: %s.\n", pSourceTag );
		return false;
	}
	return true;
}

bool ParseArrayTag( const char *pArrayTag, bool *pArray, bool *pIsOrdered )
{
	if ( pArrayTag == NULL )
	{
		*pArray = false;
		*pIsOrdered = false;
	}
	else if ( Q_stricmp( pArrayTag, "ordered" ) == 0 )
	{
		*pArray = true;
		*pIsOrdered = true;
	}
	else if ( Q_stricmp( pArrayTag, "unordered" ) == 0 )
	{
		*pArray = true;
		*pIsOrdered = false;
	}
	else
	{
		*pArray = false;
		*pIsOrdered = false;
		Log_Warning( LOG_TilegenLayoutSystem, "Unrecognized 'array' tag in rule parameter: %s.\n", pArray );
		return false;
	}
	return true;
}

bool ParseTypeTag( const char *pTypeTag, CUtlVector< RuleType_t > *pTypeList )
{
	int nCurrentIndex = 0;
	const char *pCurrentTypeName = pTypeTag;
	do
	{
		if ( pTypeTag[nCurrentIndex] == '|' || pTypeTag[nCurrentIndex] == '\0' )
		{
			int nChars = ( pTypeTag + nCurrentIndex ) - pCurrentTypeName;
			if ( nChars <= 0 || nChars > MAX_TILEGEN_IDENTIFIER_LENGTH - 1 )
			{
				// Invalid number of characters
				Log_Warning( LOG_TilegenLayoutSystem, "Malformed 'type' tag in rule parameter: %s.\n", pTypeTag );
				return false;
			}
			RuleType_t *pNewRule = &pTypeList->Element( pTypeList->AddToTail() );
			Q_memcpy( pNewRule->m_Name, pCurrentTypeName, nChars );
			pNewRule->m_Name[nChars] = '\0';
			
			// Advance past the delimiter
			pCurrentTypeName = pTypeTag + nCurrentIndex + 1;
		}
	}
	while ( pTypeTag[nCurrentIndex ++] != '\0' );

	return true;
}

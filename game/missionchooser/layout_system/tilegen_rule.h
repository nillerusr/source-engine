//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Definition of a rule used in macro substitution for the layout system.
//
//===============================================================================

#ifndef TILEGEN_RULE_H
#define TILEGEN_RULE_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "utlvector.h"
#include "tilegen_core.h"

class KeyValues;
class CTilegenMissionPreprocessor;

//-----------------------------------------------------------------------------
// A type name for a rule, e.g. 'action', 'bool', 'int', 'state'
// Rules may have multiple types if they can fit in multiple places.
//-----------------------------------------------------------------------------
struct RuleType_t
{
	char m_Name[MAX_TILEGEN_IDENTIFIER_LENGTH];
};

//-----------------------------------------------------------------------------
// A parsed and extracted rule.
//-----------------------------------------------------------------------------
class CTilegenRule
{
public:
	CTilegenRule();
	~CTilegenRule();

	const char *GetName() const { return m_pName; }
	const char *GetFriendlyName() const { return m_pFriendlyName; }
	const char *GetDescription() const { return m_pDescription; }
	const RuleType_t *GetTypes() const { return m_Types.Base(); }
	int GetTypeCount() const { return m_Types.Count(); }
	bool IsHidden() const { return m_bHidden; }

	int GetParameterCount() const { return m_SubstitutionVariables.Count(); }
	bool IsParameterOptional( int i ) const { return m_SubstitutionVariables[i].IsOptional(); }
	bool IsArrayParameter( int i ) const { return m_SubstitutionVariables[i].m_bArray; }
	bool IsOrderedArrayParameter( int i ) const { return m_SubstitutionVariables[i].m_bOrderedArray; }
	bool IsExpressionAllowedForParameter( int i ) const { return m_SubstitutionVariables[i].m_bAllowExpression; }
	bool IsLiteralAllowedForParameter( int i ) const { return m_SubstitutionVariables[i].m_bAllowLiteral; }
	const char *GetParameterName( int i ) const { return m_SubstitutionVariables[i].m_pName; }
	const char *GetParameterFriendlyName( int i ) const { return m_SubstitutionVariables[i].m_pFriendlyName; }
	const char *GetParameterDescription( int i ) const { return m_SubstitutionVariables[i].m_pDescription; }
	const char *GetParameterTypeName( int i ) const { return m_SubstitutionVariables[i].m_pTypeName; }
	const char *GetParameterEnumName( int i ) const { return m_SubstitutionVariables[i].m_pEnumName; }
	const char *GetParameterElementContainer( int i ) const { return m_SubstitutionVariables[i].m_pElementContainer; }
	const KeyValues *GetDefaultValue( int i ) const { return m_SubstitutionVariables[i].m_pDefault; }

	//-----------------------------------------------------------------------------
	// Parses the rule and takes ownership of the passed in KeyValues data.
	// NOTE: Caller must not free or modify the KeyValues data after this call.
	//-----------------------------------------------------------------------------
	bool LoadFromKeyValues( KeyValues *pRuleKeyValues );

	//-----------------------------------------------------------------------------
	// Given a KeyValues rule_instance block, returns a new KeyValues block
	// (wrapped in a "node" block) which contains the substituted key-values.
	//
	// The caller should swap the returned block with the existing rule_instance
	// block, recursively apply rules (since this only performs one rule 
	// substitution), and delete the old block.
	//-----------------------------------------------------------------------------
	KeyValues *InstantiateRule( KeyValues *pRuleInstanceKV, CTilegenMissionPreprocessor *pPreprocessor ) const;

private:
	void RecursiveFixup( KeyValues *pNewInstanceKV, KeyValues *pRuleInstanceKV, CTilegenMissionPreprocessor *pPreprocessor ) const;

	// Ownership of this KeyValues pointer is transferred to CTilegenRule which is responsible for cleanup.
	KeyValues *m_pRuleKV;

	// No need to delete any of this data, as it is all owned by m_pRuleKV data or automatically destructed.

	// "substitute" tag, contents of which are substituted in place of a "rule_instance"
	KeyValues *m_pSubstitutionKV;
	// "name" tag, unique identifier
	// @TODO: enforce uniqueness
	const char *m_pName;
	// "friendly_name" tag, displayed in the UI
	const char *m_pFriendlyName;
	// "description" tag, more detailed description for the UI
	const char *m_pDescription;
	// "type" tag, a set of OR'd flags to describe the scope/return value of the rule (e.g. 'action', 'state', 'bool', 'exit_filter')
	CUtlVector< RuleType_t > m_Types;
	// "hidden" tag, indicates that the rule should not be exposed in UI (all rules default to hidden)
	bool m_bHidden;

	struct SubstitutionVariable_t 
	{
		// "name" tag, used as a unique identifier for the rule parameter.
		// @TODO: enforce uniqueness
		const char *m_pName;
		// "friendly_name" tag, used for UI labeling.
		const char *m_pFriendlyName;
		// "description" tag, used for detailed UI info.
		const char *m_pDescription;
		// "type" tag, used to determine which UI controls to instantiate.
		const char *m_pTypeName;
		// "enum" tag, required to be non-NULL if type is "enum".  Determines valid values and strings.
		const char *m_pEnumName;
		// "default" tag, used for substitution if omitted
		KeyValues *m_pDefault;
		// "can_omit" tag, used in place of the "default" tag if the parameter can be omitted altogether. 
		bool m_bCanOmit;
		// "array" tag, used to indicate that this value can appear 0 to N times.  
		// The tag is set to a value of "ordered" or "unordered" indicating whether or not order matters.
		// Arrays are always omittable by definition, since a 0 element array is culled away
		bool m_bArray;
		// true if the "array" tag is "ordered", false if "unordered"
		bool m_bOrderedArray;
		// "element_container" tag, used to contain each array element in a Key with the provided name.
		const char *m_pElementContainer;
		// Flags come from the "source" tag, used to indicate what sources are valid for this parameter.
		// Possibilities are: "literal", "expression", and "literal|expression" (default)
		bool m_bAllowLiteral;
		bool m_bAllowExpression;

		bool IsOptional() const { return m_pDefault != NULL || m_bCanOmit; }
	};

	CUtlVector< SubstitutionVariable_t > m_SubstitutionVariables;
};

#endif // TILEGEN_RULE_H
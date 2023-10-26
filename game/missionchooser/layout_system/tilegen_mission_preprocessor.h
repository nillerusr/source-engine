//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Definition of a class to pre-process mission files and perform
// rule substitution.
//
//===============================================================================

#ifndef TILEGEN_MISSION_PREPROCESSOR_H
#define TILEGEN_MISSION_PREPROCESSOR_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "utlvector.h"

class KeyValues;
class CTilegenRule;
class CTilegenEnum;

//-----------------------------------------------------------------------------
// Rule-based pre-processor for mission key-values.
// Substitutes "rule_instance" nodes with the contents of the corresponding 
// "rule" node and fixes up instance values.
//-----------------------------------------------------------------------------
class CTilegenMissionPreprocessor
{
public:
	friend class CTilegenRule;

	CTilegenMissionPreprocessor() : m_nUniqueIndex( 0 ) { }
	~CTilegenMissionPreprocessor();

	int GetRuleCount() const { return m_Rules.Count(); }
	const CTilegenRule *GetRule( int i ) const { return m_Rules[i]; }
	const CTilegenRule *FindRule( const char *pRuleName ) const;

	int GetEnumCount() const { return m_Enums.Count(); }
	const CTilegenEnum *GetEnum( int i ) const { return m_Enums[i]; }
	const CTilegenEnum *FindEnum( const char *pEnumName ) const;

	//-----------------------------------------------------------------------------
	// Searches the top level KeyValues file for 'rule' blocks, extracts
	// the rule definition from the KeyValues data, and creates
	// an internal CTilegenRule for each.
	// This is a destructive operation; the rule blocks are stripped from the file.
	// 
	// NOTE: Caller is still responsible for cleaning up root-level key-values
	// data.
	//
	// Returns true on success, false otherwise.
	//-----------------------------------------------------------------------------
	bool ParseAndStripRules( KeyValues *pRulesKV );

	//-----------------------------------------------------------------------------
	// Recursively walks the KeyValues file looking for rule_instances to 
	// replace with a corresponding rule.
	//
	// Returns true on success, false otherwise.
	//-----------------------------------------------------------------------------
	bool SubstituteRules( KeyValues *pKeyValues );

private:
	// Called by CTilegenRule
	void EvaluateMetaVariables( KeyValues *pKeyValues );

	bool SubstituteRules_Recursive( KeyValues *pParent, KeyValues *pKeyValues );
	
	CUtlVector< CTilegenEnum * > m_Enums;
	CUtlVector< CTilegenRule * > m_Rules;
	// Index used to generate unique names when pre-processing a file.
	int m_nUniqueIndex;
};

#endif // TILEGEN_MISSION_PREPROCESSOR_H
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef DEPENDENCYGRAPH_H
#define DEPENDENCYGRAPH_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "tier1/utlhash.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmAttribute;
class IDmeOperator;
struct COperatorNode;
class CAttributeNode;


//-----------------------------------------------------------------------------
// CDependencyGraph class - sorts operators based upon the input/output graph
//-----------------------------------------------------------------------------
class CDependencyGraph
{
public:
	CDependencyGraph();
	~CDependencyGraph();

	void Reset( const CUtlVector< IDmeOperator * > &operators );

	// caches only the operators that need to be evaluated, sorted by dependencies
	// returns true if a cycle found - in this case, an arbitrary link of the cycle will be ignored
	bool CullAndSortOperators();

	const CUtlVector< IDmeOperator* > &GetSortedOperators() const { return m_operators; }

private:
	static bool GetOperatorOrdering( CUtlVector< COperatorNode* > &pOpNodes, CUtlVector< IDmeOperator * > &operators );
	static void DBG_PrintOperator( const char *pIndent, IDmeOperator *pOp );

	friend class CDmElementFramework;

	void Cleanup();
	void FindRoots();
	CAttributeNode *FindAttrNode( CDmAttribute *pAttr );

	CUtlVector< COperatorNode* > m_opRoots;
//	CUtlVector< COperatorNode* > m_opLeaves;

	CUtlVector< COperatorNode* > m_opNodes;

	CUtlHash< CAttributeNode* > m_attrNodes;

	CUtlVector< IDmeOperator* > m_operators;
};

#endif // DEPENDENCYGRAPH_H

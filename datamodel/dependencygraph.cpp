//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dependencygraph.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmelement.h"
#include "mathlib/mathlib.h" // for swap

#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"

#include "tier1/mempool.h"

#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
//-----------------------------------------------------------------------------
// Misc helper enums and classes for CDependencyGraph class
//-----------------------------------------------------------------------------
enum TraversalState_t
{
	TS_NOT_VISITED,
	TS_VISITING,
	TS_VISITED,
};

struct COperatorNode
{
	COperatorNode( IDmeOperator *pOp = NULL ) :
		m_state( TS_NOT_VISITED ), 
		m_operator( pOp ),
		m_bInList( false )
	{
	}

	TraversalState_t m_state;
	IDmeOperator *m_operator;
	CUtlVector< CAttributeNode * > m_OutputAttributes;
	bool			m_bInList;
};

class CAttributeNode
{
public:
	CAttributeNode( CDmAttribute *attribute = NULL ) : 
		m_attribute( attribute ),
		m_bIsOutputToOperator( false )
	{
	}

	CDmAttribute *m_attribute;
	CUtlVector< COperatorNode * > m_InputDependentOperators;
	bool		m_bIsOutputToOperator;
};

CClassMemoryPool< CAttributeNode >	g_AttrNodePool( 1000 );
CClassMemoryPool< COperatorNode >	g_OperatorNodePool( 1000 );

bool HashEntryCompareFunc( CAttributeNode *const& lhs, CAttributeNode *const& rhs )
{
	return lhs->m_attribute == rhs->m_attribute;
}

uint HashEntryKeyFunc( CAttributeNode *const& keyinfo )
{
	uint i = (uint)keyinfo->m_attribute;
	return i >> 2; // since memory is allocated on a 4-byte (at least!) boundary
}

//-----------------------------------------------------------------------------
// CDependencyGraph constructor - builds dependency graph from operators
//-----------------------------------------------------------------------------
CDependencyGraph::CDependencyGraph() :
	m_attrNodes( 4096, 0, 0, HashEntryCompareFunc, HashEntryKeyFunc )
{
}

void CDependencyGraph::Reset( const CUtlVector< IDmeOperator * > &operators )
{
	VPROF_BUDGET( "CDependencyGraph::Reset", VPROF_BUDGETGROUP_TOOLS );

	Cleanup();

	CUtlVector< CDmAttribute * > attrs; // moved outside the loop to function as a temporary memory pool for performance
	int on = operators.Count();
	CUtlRBTree< IDmeOperator * > operatorDict( 0, on * 2, DefLessFunc(IDmeOperator *) );
	for ( int i = 0; i < on; ++i )
	{
		operatorDict.Insert( operators[i] );
	}

	m_opNodes.EnsureCapacity( on );
	for ( int oi = 0; oi < on; ++oi )
	{
		IDmeOperator *pOp = operators[ oi ];
		Assert( pOp );
		if ( pOp == NULL )
			continue;

		COperatorNode *pOpNode = g_OperatorNodePool.Alloc();
		pOpNode->m_operator = pOp;

		attrs.RemoveAll();
		pOp->GetInputAttributes( attrs );
		int an = attrs.Count();
		for ( int ai = 0; ai < an; ++ai )
		{
			CAttributeNode *pAttrNode = FindAttrNode( attrs[ ai ] );
			pAttrNode->m_InputDependentOperators.AddToTail( pOpNode );
		}

		attrs.RemoveAll();
		pOp->GetOutputAttributes( attrs );
		an = attrs.Count();
		for ( int ai = 0; ai < an; ++ai )
		{
            CAttributeNode *pAttrNode = FindAttrNode( attrs[ ai ] );
			pAttrNode->m_bIsOutputToOperator = true;
			pOpNode->m_OutputAttributes.AddToTail( pAttrNode );

#ifdef _DEBUG
			// Look for dependent operators, add them if they are not in the array
			// FIXME: Should this happen for input attributes too?
			CDmElement* pElement = pAttrNode->m_attribute->GetOwner();
			IDmeOperator *pOperator = dynamic_cast< IDmeOperator* >( pElement );
			if ( pOperator )
			{
				// Look for the operator in the existing list
				if ( operatorDict.Find( pOperator ) == operatorDict.InvalidIndex() )
				{
					CDmElement *pOp1 = dynamic_cast< CDmElement* >( pOperator );
					CDmElement *pOp2 = dynamic_cast< CDmElement* >( pOp );
					Warning( "Found dependent operator '%s' referenced by operator '%s' that wasn't in the scene or trackgroups!\n", pOp1->GetName(), pOp2->GetName() );
				}
			}
#endif
		}

		m_opNodes.AddToTail( pOpNode );
	}
}

//-----------------------------------------------------------------------------
// CDependencyGraph destructor - releases temporary opNodes and attrNodes
//-----------------------------------------------------------------------------
CDependencyGraph::~CDependencyGraph()
{
	Cleanup();
}

void CDependencyGraph::Cleanup()
{
	VPROF_BUDGET( "CDependencyGraph::Cleanup", VPROF_BUDGETGROUP_TOOLS );

	int on = m_opNodes.Count();
	for ( int oi = 0; oi < on; ++oi )
	{
		g_OperatorNodePool.Free( m_opNodes[ oi ] );
	}

	UtlHashHandle_t h = m_attrNodes.GetFirstHandle();
	for ( ; h != m_attrNodes.InvalidHandle(); h = m_attrNodes.GetNextHandle( h ) )
	{
		g_AttrNodePool.Free( m_attrNodes[ h ] );
	}

	m_opRoots.RemoveAll();
	m_opNodes.RemoveAll();
	m_attrNodes.RemoveAll();
	m_operators.RemoveAll();
}


//-----------------------------------------------------------------------------
// caches changed operators as roots - typically once per frame, every frame
//-----------------------------------------------------------------------------
void CDependencyGraph::FindRoots()
{
	m_opRoots.RemoveAll();

	uint oi;
	uint on = m_opNodes.Count();

	for ( oi = 0; oi < on; ++oi )
	{
		COperatorNode *pOpNode = m_opNodes[ oi ];
		pOpNode->m_bInList = false;
		pOpNode->m_state = TS_NOT_VISITED;

		IDmeOperator *pOp = pOpNode->m_operator;
		if ( !pOp->IsDirty() )
			continue;

		m_opRoots.AddToTail( pOpNode );
		pOpNode->m_bInList = true;
	}

	

	// Do we have an attribute which is an input to us which is not an output to some other op?
	UtlHashHandle_t h = m_attrNodes.GetFirstHandle();
	for ( ; h != m_attrNodes.InvalidHandle(); h = m_attrNodes.GetNextHandle( h ) )
	{
		CAttributeNode *pAttrNode = m_attrNodes[ h ];
		//Msg( "attrib %s %p\n", pAttrNode->m_attribute->GetName(), pAttrNode->m_attribute );
		if ( !pAttrNode->m_bIsOutputToOperator &&
			pAttrNode->m_attribute->IsFlagSet( FATTRIB_OPERATOR_DIRTY ) )
		{
			on = pAttrNode->m_InputDependentOperators.Count();
			for ( oi = 0; oi < on; ++oi )
			{
				COperatorNode *pOpNode = pAttrNode->m_InputDependentOperators[ oi ];
				if ( !pOpNode->m_bInList )
				{
					m_opRoots.AddToTail( pOpNode );
					pOpNode->m_bInList = true;
				}
			}
		}

		pAttrNode->m_attribute->RemoveFlag( FATTRIB_OPERATOR_DIRTY );
	}
}


//-----------------------------------------------------------------------------
// returns only the operators that need to be evaluated, sorted by dependencies
//-----------------------------------------------------------------------------
bool CDependencyGraph::CullAndSortOperators()
{
	FindRoots();

	m_operators.RemoveAll();

	bool cycle = GetOperatorOrdering( m_opRoots, m_operators ); // leaves to roots (outputs to inputs)

	int on = m_operators.Count();
	int oh = on / 2;
	for ( int oi = 0; oi < oh; ++oi )
	{
		V_swap( m_operators[ oi ], m_operators[ on - oi - 1 ] );
	}
	return cycle;

//	return GetOperatorOrdering( m_opLeaves, operators ); // roots to leaves (inputs to outputs)
}

//-----------------------------------------------------------------------------
// GetOperatorOrdering is just a recursive post-order depth-first-search
//   since we have two types of nodes, we manually traverse to the opnodes grandchildren, which are opnodes
//   (skipping children, which are attrnodes)
//   returns true if a cycle found - in this case, an arbitrary link of the cycle will be ignored
//-----------------------------------------------------------------------------
bool CDependencyGraph::GetOperatorOrdering( CUtlVector< COperatorNode * > &pOpNodes, CUtlVector< IDmeOperator * > &operators )
{
	bool cycle = false;

	uint on = pOpNodes.Count();
	for ( uint oi = 0; oi < on; ++oi )
	{
		COperatorNode *pOpNode = pOpNodes[ oi ];
		if ( pOpNode->m_state != TS_NOT_VISITED )
		{
			if ( pOpNode->m_state == TS_VISITING )
			{
				cycle = true;
			}
			continue;
		}
		pOpNode->m_state = TS_VISITING; // mark as in being visited

		// DBG_PrintOperator( pIndent, pOpNode->m_operator );

		// leaves to roots (outputs to inputs)
		uint an = pOpNode->m_OutputAttributes.Count();
		for ( uint ai = 0; ai < an; ++ai )
		{
			CAttributeNode *pAttrNode = pOpNode->m_OutputAttributes[ ai ];
			if ( GetOperatorOrdering( pAttrNode->m_InputDependentOperators, operators ) )
			{
				cycle = true;
			}
		}

		operators.AddToTail( pOpNode->m_operator );
		pOpNode->m_state = TS_VISITED; // mark as done visiting
	}
	return cycle;
}

//-----------------------------------------------------------------------------
// internal helper method - finds attrNode corresponding to pAttr
//-----------------------------------------------------------------------------
CAttributeNode *CDependencyGraph::FindAttrNode( CDmAttribute *pAttr )
{
	VPROF_BUDGET( "CDependencyGraph::FindAttrNode", VPROF_BUDGETGROUP_TOOLS );

	Assert( pAttr );

	CAttributeNode search( pAttr );
	UtlHashHandle_t idx = m_attrNodes.Find( &search );
	if ( idx != m_attrNodes.InvalidHandle() )
	{
		return m_attrNodes.Element( idx );
	}

	CAttributeNode *pAttrNode = 0;
	{
		VPROF( "CDependencyGraph::FindAttrNode_Alloc" );
		pAttrNode = g_AttrNodePool.Alloc();
		pAttrNode->m_attribute = pAttr;
	}
	{
		VPROF( "CDependencyGraph::FindAttrNode_Alloc2" );
		m_attrNodes.Insert( pAttrNode );
	}
	return pAttrNode;
}

//-----------------------------------------------------------------------------
// temporary internal debugging function
//-----------------------------------------------------------------------------
void CDependencyGraph::DBG_PrintOperator( const char *pIndent, IDmeOperator *pOp )
{
	CDmElement *pElement = dynamic_cast< CDmElement* >( pOp );
	Msg( "%s%s <%s> {\n", pIndent, pElement->GetName(), pElement->GetTypeString() );
}

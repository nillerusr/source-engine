//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#ifndef UTLGRAPH_H
#define UTLGRAPH_H

#include "tier1/utlmap.h"
#include "tier1/utlvector.h"
#include <limits.h>



//-------------------------------------


//-----------------------------------------------------------------------------
// A Graph class
//
// Nodes must have a unique Node ID. 
//
// Edges are unidirectional, specified from the beginning node.
//
//-----------------------------------------------------------------------------

template <class T, class C > 
class CUtlGraphVisitor;

template <class T, class C = int > 
class CUtlGraph
{
public:
	typedef int I;
	typedef I IndexType_t;
	typedef T NodeID_t;
	typedef C CostType_t;

	typedef CUtlGraphVisitor<T,C> Visitor_t;

	struct Edge_t
	{
		IndexType_t m_DestinationNode;
		CostType_t m_EdgeCost;

		Edge_t( IndexType_t i = 0 )
		{
			m_DestinationNode = i;
			m_EdgeCost = 0;
		}

		bool operator==(const Edge_t &that ) const
		{
			return m_DestinationNode == that.m_DestinationNode;
		}

		static int SortFn( const Edge_t *plhs, const Edge_t *prhs )
		{
			if ( plhs->m_EdgeCost < prhs->m_EdgeCost )
				return -1;
			else if ( plhs->m_EdgeCost > prhs->m_EdgeCost )
				return 1;
			else return 0;
		}
	};

	typedef CUtlVector<Edge_t> vecEdges_t;

	// constructor, destructor
	CUtlGraph( );
	~CUtlGraph();

	// Add an edge
	bool AddEdge( T SourceNode, T DestNode, C nCost );

	// Remove an edge
	bool RemoveEdge( T SourceNode, T DestNode );
	
	// gets particular elements
	T&			Element( I i );
	T const		&Element( I i ) const;
	T&			operator[]( I i );
	T const		&operator[]( I i ) const;

	// Find a node
	I Find( T Node )										{ return m_Nodes.Find( Node ); }
	I Find( T Node ) const									{ return m_Nodes.Find( Node ); }

	// Num elements
	unsigned int Count() const								{ return m_Nodes.Count() ; }
	
	// Max "size" of the vector
	I  MaxElement() const									{ return m_Nodes.MaxElement(); }
	
	// Checks if a node is valid and in the graph
	bool  IsValidIndex( I i ) const							{ return m_Nodes.IsValidIndex( i ); }
	
	// Checks if the graph as a whole is valid
	bool  IsValid() const									{ return m_Nodes.IsValid(); }
	
	// Invalid index
	static I InvalidIndex()									{ return CUtlMap< NodeID_t, vecEdges_t*>::InvalidIndex(); }
	
	// Remove methods
	void     RemoveAt( I i );
	void     RemoveAll();

	// Makes sure we have enough memory allocated to store a requested # of elements
	void EnsureCapacity( int num );

	// Create Path Matrix once you've added all nodes and edges
	void CreatePathMatrix();

	// For Visitor classes
	vecEdges_t *GetEdges( I i );

	// shortest path costs
	vecEdges_t *GetPathCosts( I i );

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif // DBGFLAG_VALIDATE

protected:

	struct Node_t
	{
		vecEdges_t *m_pvecEdges;
		vecEdges_t *m_pvecPaths;

		Node_t()
		{
			m_pvecEdges = NULL;
			m_pvecPaths = NULL;
		}
	};

	CUtlMap< NodeID_t, Node_t > m_Nodes;
};


//-----------------------------------------------------------------------------
// A Graph "visitor" class
//
// From the specified beginning point, visits each node in an expanding radius
//
//-----------------------------------------------------------------------------
template <class T, class C = int > 
class CUtlGraphVisitor
{
public:
	CUtlGraphVisitor( CUtlGraph<T, C> &graph );

	bool Begin( T StartNode );
	bool Advance();

	T CurrentNode();
	C AccumulatedCost();
	int CurrentRadius();
		
private:

	typedef typename CUtlGraph<T,C>::IndexType_t IndexType_t;
	typedef typename CUtlGraph<T,C>::Edge_t Edge_t;
	typedef CUtlVector<Edge_t> vecEdges_t;
	
	CUtlGraph<T, C> &m_Graph;

	vecEdges_t m_vecVisitQueue;
	int m_iVisiting;
	int m_nCurrentRadius;

	vecEdges_t m_vecFringeQueue;

	CUtlVector<T> m_vecNodesVisited;
};


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

template <class T, class C > 
inline CUtlGraph<T, C>::CUtlGraph()
{
	SetDefLessFunc( m_Nodes );
}

template <class T, class C >  
inline CUtlGraph<T, C>::~CUtlGraph()
{
	RemoveAll();
}

//-----------------------------------------------------------------------------
// gets particular elements
//-----------------------------------------------------------------------------

template <class T, class C > 
inline T &CUtlGraph<T, C>::Element( I i )        
{ 
	return m_Nodes.Key( i ); 
}

template <class T, class C > 
inline T const &CUtlGraph<T, C>::Element( I i ) const  
{ 
	return m_Nodes.Key( i ); 
}

template <class T, class C > 
inline T &CUtlGraph<T, C>::operator[]( I i )        
{ 
	return Element(i); 
}

template <class T, class C > 
inline T const &CUtlGraph<T, C>::operator[]( I i ) const  
{ 
	return Element(i); 
}

//-----------------------------------------------------------------------------
//
// various accessors
//
//-----------------------------------------------------------------------------
	
//-----------------------------------------------------------------------------
// Removes all nodes from the tree
//-----------------------------------------------------------------------------

template <class T, class C > 
void CUtlGraph<T, C>::RemoveAll()
{
	FOR_EACH_MAP_FAST( m_Nodes, iNode )
	{
		delete m_Nodes[iNode].m_pvecEdges;
		delete m_Nodes[iNode].m_pvecPaths;
	}

	m_Nodes.RemoveAll();
}

//-----------------------------------------------------------------------------
// Makes sure we have enough memory allocated to store a requested # of elements
//-----------------------------------------------------------------------------
template <class T, class C >  
void CUtlGraph<T, C>::EnsureCapacity( int num )
{
	m_Nodes.EnsureCapacity(num);
}

//-----------------------------------------------------------------------------
// Add an edge
//-----------------------------------------------------------------------------
template <class T, class C >  
bool CUtlGraph<T, C>::AddEdge( T SourceNode, T DestNode, C nCost )
{
	I iSrcNode = m_Nodes.Find( SourceNode );
	if ( !m_Nodes.IsValidIndex( iSrcNode ) )
	{
		Node_t Node;
		Node.m_pvecEdges = new vecEdges_t();
		Node.m_pvecPaths = new vecEdges_t();
		iSrcNode = m_Nodes.Insert( SourceNode, Node );
	}

	I iDstNode = m_Nodes.Find( DestNode );
	if ( !m_Nodes.IsValidIndex( iDstNode ) )
	{
		Node_t Node;
		Node.m_pvecEdges = new vecEdges_t();
		Node.m_pvecPaths = new vecEdges_t();
		iDstNode = m_Nodes.Insert( DestNode, Node );
	}

	vecEdges_t &vecEdges = *m_Nodes[iSrcNode].m_pvecEdges;

#ifdef _DEBUG
	FOR_EACH_VEC( vecEdges, iEdge )
	{
		if ( vecEdges[iEdge].m_DestinationNode == iDstNode )
			return false;
	}
#endif

	Edge_t newEdge;
	newEdge.m_DestinationNode = iDstNode;
	newEdge.m_EdgeCost = nCost;

	vecEdges[ vecEdges.AddToTail() ] = newEdge;

	return true;
}

//-----------------------------------------------------------------------------
// Remove an edge
//-----------------------------------------------------------------------------
template <class T, class C >  
bool CUtlGraph<T, C>::RemoveEdge( T SourceNode, T DestNode )
{
	I iSrcNode = m_Nodes.Find( SourceNode );
	if ( !m_Nodes.IsValidIndex( iSrcNode ) )
		return false;

	I iDstNode = m_Nodes.Find( DestNode );
	if ( !m_Nodes.IsValidIndex( iDstNode ) )
		return false;

	vecEdges_t &vecEdges = *m_Nodes[iSrcNode].m_pvecEdges;

	FOR_EACH_VEC( vecEdges, iEdge )
	{
		if ( vecEdges[iEdge].m_DestinationNode == iDstNode )
		{
			// could use FastRemove, but nodes won't have that
			// many edges, and the elements are small, and
			// preserving the original ordering is nice
			vecEdges.Remove( iEdge );
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Get all of a Node's edges
//-----------------------------------------------------------------------------
template <class T, class C >  
typename CUtlGraph<T, C>::vecEdges_t *CUtlGraph<T, C>::GetEdges( I i )
{
	return m_Nodes[i].m_pvecEdges;
}

//-----------------------------------------------------------------------------
// Get all of a Node's edges
//-----------------------------------------------------------------------------
template <class T, class C >  
typename CUtlGraph<T, C>::vecEdges_t *CUtlGraph<T, C>::GetPathCosts( I i )
{
	return m_Nodes[i].m_pvecPaths;
}


//-----------------------------------------------------------------------------
// Data and memory validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
template <class T, class C > 
void CUtlGraph<T, C>::Validate( CValidator &validator, const char *pchName )
{
#ifdef _WIN32
	validator.Push( typeid(*this).raw_name(), this, pchName );
#else
	validator.Push( typeid(*this).name(), this, pchName );
#endif

	ValidateObj( m_Nodes );

	FOR_EACH_MAP_FAST( m_Nodes, iNode )
	{
		validator.ClaimMemory( m_Nodes[iNode].m_pvecEdges );
		ValidateObj( *m_Nodes[iNode].m_pvecEdges );
		validator.ClaimMemory( m_Nodes[iNode].m_pvecPaths );
		ValidateObj( *m_Nodes[iNode].m_pvecPaths );
	}

	validator.Pop();
}
#endif // DBGFLAG_VALIDATE


//-----------------------------------------------------------------------------
// Get all of a Node's edges
//-----------------------------------------------------------------------------
template <class T, class C >  
void CUtlGraph<T, C>::CreatePathMatrix()
{
	int n = MaxElement();

	// Notes
	// Because CUtlMap stores its nodes in essentially a vector,
	// we know that we can use its indices in our own path matrix
	// vectors safely (they will be numbers in the range (0,N) where
	// N is largest number of nodes ever present in the graph).
	// 
	// This lets us very quickly access previous best-path estimates
	// by indexing into a node's vecPaths directly.
	// 
	// When we are all done, we can then compact the vector, removing
	// "null" paths, and then sorting by cost.

	// Initialize matrix with all edges
	FOR_EACH_MAP_FAST( m_Nodes, iNode )
	{
		vecEdges_t &vecEdges = *m_Nodes.Element( iNode ).m_pvecEdges;
		vecEdges_t &vecPaths = *m_Nodes.Element( iNode ).m_pvecPaths;
		
		vecPaths.RemoveAll();
		vecPaths.AddMultipleToTail( n );
		FOR_EACH_VEC( vecPaths, iPath )
		{
			vecPaths[iPath].m_DestinationNode = InvalidIndex();
		}

		// Path to self
		vecPaths[iNode].m_DestinationNode = iNode;
		// zero cost to self
		vecPaths[iNode].m_EdgeCost = 0;

		FOR_EACH_VEC( vecEdges, iEdge )
		{
			// Path to a neighbor node - we know exactly what the cost is
			Edge_t &edge = vecEdges[iEdge];
			vecPaths[ edge.m_DestinationNode ].m_DestinationNode = edge.m_DestinationNode;
			vecPaths[ edge.m_DestinationNode ].m_EdgeCost = edge.m_EdgeCost;
		}
	}

	// Floyd-Warshall
	// for k:= 0 to n-1
	//		for each (i,j) in (0..n-1)
	//			path[i][j] = min( path[i][j], path[i][k]+path[k][j] );
	for ( int k = 0; k < n; ++ k )
	{
		if ( !m_Nodes.IsValidIndex( k ) )
			continue;

		// All current known paths from K
		vecEdges_t &destMapFromK = *m_Nodes[k].m_pvecPaths;

		for ( int i = 0; i < n; ++i )
		{
			if ( !m_Nodes.IsValidIndex( i ) )
				continue;

			// All current known paths from J
			vecEdges_t &destMapFromI = *m_Nodes[i].m_pvecPaths;

			// Path from I to K?
			int iFromIToK = k;
			bool bFromIToK = destMapFromI[iFromIToK].m_DestinationNode != InvalidIndex();
			CostType_t cIToK = ( bFromIToK ) ? destMapFromI[iFromIToK].m_EdgeCost : INT_MAX;

			for ( int j = 0; j < n; ++ j )
			{
				if ( !m_Nodes.IsValidIndex( j ) )
					continue;

				// Path from I to J already?
				int iFromIToJ = j;
				bool bFromIToJ = destMapFromI[iFromIToJ].m_DestinationNode != InvalidIndex();
				CostType_t cIToJ = ( bFromIToJ ) ? destMapFromI[iFromIToJ].m_EdgeCost : INT_MAX;

				// Path from K to J?
				int iFromKToJ = j;
				bool bFromKToJ = destMapFromK[iFromKToJ].m_DestinationNode != InvalidIndex();
				CostType_t cKToJ = ( bFromKToJ ) ? destMapFromK[iFromKToJ].m_EdgeCost : INT_MAX;
				
				// Is the new path valid?
				bool bNewPathFound = ( bFromIToK && bFromKToJ );

				if ( bNewPathFound )
				{
					if ( bFromIToJ )
					{
						// Pick min of previous best and current path
						destMapFromI[iFromIToJ].m_EdgeCost = min( cIToJ, cIToK + cKToJ );
					}
					else
					{
						// Current path is the first, hence the best so far
						destMapFromI[iFromIToJ].m_DestinationNode = iFromIToJ;
						destMapFromI[iFromIToJ].m_EdgeCost = cIToK + cKToJ;
					}
				}
			}
		}
	}

	// Clean up and sort the paths
	FOR_EACH_MAP_FAST( m_Nodes, iNode )
	{
		vecEdges_t &vecPaths = *m_Nodes.Element( iNode ).m_pvecPaths;
		FOR_EACH_VEC( vecPaths, iPath )
		{
			Edge_t &edge = vecPaths[iPath];
			if ( edge.m_DestinationNode == InvalidIndex() )
			{
				// No path to this destination was found.
				// Remove this entry from the vector.
				vecPaths.FastRemove( iPath );
				--iPath; // adjust for the removal
			}
		}

		// Sort the vector by cost, given that it
		// is likely consumers will want to 
		// iterate destinations in that order.
		vecPaths.Sort( Edge_t::SortFn );
	}
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
template <class T, class C > 
CUtlGraphVisitor<T, C>::CUtlGraphVisitor( CUtlGraph<T,C> &graph )
: m_Graph( graph )
{
	m_iVisiting = 0;
	m_nCurrentRadius = 0;
}


//-----------------------------------------------------------------------------
// Begin visiting the nodes in the graph. Returns false if the start node
//	does not exist
//-----------------------------------------------------------------------------
template <class T, class C > 
bool CUtlGraphVisitor<T, C>::Begin( T StartNode )
{
	m_vecVisitQueue.RemoveAll();
	m_vecFringeQueue.RemoveAll();
	m_vecNodesVisited.RemoveAll();
	m_iVisiting = 0;
	m_nCurrentRadius = 0;

	IndexType_t iStartNode = m_Graph.Find( StartNode );

	if ( !m_Graph.IsValidIndex( iStartNode ) )
		return false;

	vecEdges_t *pvecEdges = m_Graph.GetEdges( iStartNode );

	Edge_t edge;
	edge.m_DestinationNode = iStartNode;
	edge.m_EdgeCost = 0;

	m_vecVisitQueue[ m_vecVisitQueue.AddToTail() ] = edge;

	m_vecNodesVisited[ m_vecNodesVisited.AddToTail() ] = iStartNode;

	m_vecFringeQueue = *pvecEdges;

	// cells actually get marked as "visited" as soon as we put
	// them in the fringe queue, so we don't put them in the *next* 
	// fringe queue (we build the fringe queue before we actually visit
	// the nodes in the new visit queue).
	FOR_EACH_VEC( m_vecFringeQueue, iFringe )
	{
		m_vecNodesVisited[ m_vecNodesVisited.AddToTail() ] = m_vecFringeQueue[iFringe].m_DestinationNode;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Advance to the next node. Returns false when all nodes have been visited
//-----------------------------------------------------------------------------
template <class T, class C> 
bool CUtlGraphVisitor<T, C>::Advance()
{
	m_iVisiting++;

	// Is the VisitQueue empty? move outward one radius if so

	if ( m_iVisiting >= m_vecVisitQueue.Count() )
	{
		m_nCurrentRadius++;
		m_iVisiting = 0;
		m_vecVisitQueue = m_vecFringeQueue;
		m_vecFringeQueue.RemoveAll();

		if ( !m_vecVisitQueue.Count() )
			return false;

		// create new fringe queue
		FOR_EACH_VEC( m_vecVisitQueue, iNode )
		{
			Edge_t &node = m_vecVisitQueue[iNode];
			vecEdges_t &vecEdges = *m_Graph.GetEdges( node.m_DestinationNode );
			FOR_EACH_VEC( vecEdges, iEdge )
			{
				Edge_t &edge = vecEdges[iEdge];
				if ( m_vecNodesVisited.InvalidIndex() == m_vecNodesVisited.Find( edge.m_DestinationNode ) )
				{
					m_vecNodesVisited[ m_vecNodesVisited.AddToTail() ] = edge.m_DestinationNode;

					int iNewFringeNode = m_vecFringeQueue.AddToTail();
					m_vecFringeQueue[ iNewFringeNode ] = edge;
					// Accumulate the cost to get to the current point
					m_vecFringeQueue[ iNewFringeNode ].m_EdgeCost += node.m_EdgeCost;
				}
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Get the current node in the visit sequence
//-----------------------------------------------------------------------------
template <class T, class C> 
T CUtlGraphVisitor<T, C>::CurrentNode()
{
	if ( m_iVisiting >= m_vecVisitQueue.Count() )
	{
		AssertMsg( false, "Visitor invalid" );
		return T();
	}

	return m_Graph[ m_vecVisitQueue[ m_iVisiting ].m_DestinationNode ];
}


//-----------------------------------------------------------------------------
// Get the accumulated cost to traverse the graph to the current node
//-----------------------------------------------------------------------------
template <class T, class C> 
C CUtlGraphVisitor<T, C>::AccumulatedCost()
{
	if ( m_iVisiting >= m_vecVisitQueue.Count() )
	{
		AssertMsg( false, "Visitor invalid" );
		return C();
	}

	return m_vecVisitQueue[ m_iVisiting ].m_EdgeCost;
}


//-----------------------------------------------------------------------------
// Get the current radius from the start point to this node
//-----------------------------------------------------------------------------
template <class T, class C> 
int CUtlGraphVisitor<T, C>::CurrentRadius()
{
	if ( m_iVisiting >= m_vecVisitQueue.Count() )
	{
		AssertMsg( false, "Visitor invalid" );
		return 0;
	}

	return m_nCurrentRadius;
}

#endif // UTLGRAPH_H

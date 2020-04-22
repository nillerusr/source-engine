//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// nav_node.h
// Navigation Nodes are used when generating a Navigation Mesh by point sampling the map
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

#ifndef _CS_NAV_NODE_H_
#define _CS_NAV_NODE_H_

#include "cs_nav.h"

// If DEBUG_NAV_NODES is true, nav_show_nodes controls drawing node positions, and
// nav_show_node_id allows you to show the IDs of nodes that didn't get used to create areas.
#ifdef _DEBUG
#define DEBUG_NAV_NODES 1
#else
#define DEBUG_NAV_NODES 0
#endif

//--------------------------------------------------------------------------------------------------------------
/**
 * Navigation Nodes.
 * These Nodes encapsulate world locations, and ways to get from one location to an adjacent one.
 * Note that these links are not necessarily commutative (falling off of a ledge, for example).
 */
class CSNavNode
{
public:
	CSNavNode( const Vector &pos, const Vector &normal, CSNavNode *parent = NULL );

	static CSNavNode *GetNode( const Vector &pos );					///< return navigation node at the position, or NULL if none exists

	CSNavNode *GetConnectedNode( NavDirType dir ) const;				///< get navigation node connected in given direction, or NULL if cant go that way
	const Vector *GetPosition( void ) const;
	const Vector *GetNormal( void ) const		{ return &m_normal; }
	unsigned int GetID( void ) const			{ return m_id; }

	static CSNavNode *GetFirst( void )			{ return m_list; }
	static unsigned int GetListLength( void )	{ return m_listLength; }
	CSNavNode *GetNext( void )					{ return m_next; }

	void Draw( void );

	void ConnectTo( CSNavNode *node, NavDirType dir );				///< create a connection FROM this node TO the given node, in the given direction
	CSNavNode *GetParent( void ) const;

	void MarkAsVisited( NavDirType dir );							///< mark the given direction as having been visited
	BOOL HasVisited( NavDirType dir );								///< return TRUE if the given direction has already been searched
	BOOL IsBiLinked( NavDirType dir ) const;						///< node is bidirectionally linked to another node in the given direction
	BOOL IsClosedCell( void ) const;								///< node is the NW corner of a bi-linked quad of nodes

	void Cover( void )				{ m_isCovered = true; }			///< @todo Should pass in area that is covering
	BOOL IsCovered( void ) const	{ return m_isCovered; }			///< return true if this node has been covered by an area

	void AssignArea( CNavArea *area );								///< assign the given area to this node
	CNavArea *GetArea( void ) const;								///< return associated area

	void SetAttributes( unsigned char bits )		{ m_attributeFlags = bits; }
	unsigned char GetAttributes( void ) const		{ return m_attributeFlags; }

private:
	friend class CNavMesh;

	void CheckCrouch( void );

	Vector m_pos;													///< position of this node in the world
	Vector m_normal;												///< surface normal at this location
	CSNavNode *m_to[ NUM_DIRECTIONS ];								///< links to north, south, east, and west. NULL if no link
	unsigned int m_id;												///< unique ID of this node
	unsigned char m_attributeFlags;									///< set of attribute bit flags (see NavAttributeType)

	static CSNavNode *m_list;										///< the master list of all nodes for this map
	static unsigned int m_listLength;
	static unsigned int m_nextID;
	CSNavNode *m_next;												///< next link in master list

	// below are only needed when generating
	unsigned char m_visited;										///< flags for automatic node generation. If direction bit is clear, that direction hasn't been explored yet.
	CSNavNode *m_parent;												///< the node prior to this in the search, which we pop back to when this node's search is done (a stack)
	bool m_isCovered;												///< true when this node is "covered" by a CNavArea
	CNavArea *m_area;												///< the area this node is contained within

	bool m_crouch[ NUM_CORNERS ];
};

//--------------------------------------------------------------------------------------------------------------
//
// Inlines
//

inline CSNavNode *CSNavNode::GetConnectedNode( NavDirType dir ) const
{
	return m_to[ dir ];
}

inline const Vector *CSNavNode::GetPosition( void ) const
{
	return &m_pos;
}

inline CSNavNode *CSNavNode::GetParent( void ) const
{
	return m_parent;
}

inline void CSNavNode::MarkAsVisited( NavDirType dir )
{
	m_visited |= (1 << dir);
}

inline BOOL CSNavNode::HasVisited( NavDirType dir )
{
	if (m_visited & (1 << dir))
		return true;

	return false;
}

inline void CSNavNode::AssignArea( CNavArea *area )
{
	m_area = area;
}

inline CNavArea *CSNavNode::GetArea( void ) const
{
	return m_area;
}


#endif // _CS_NAV_NODE_H_

//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// nav_node.cpp
// AI Navigation Nodes
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

#include "cbase.h"
#include "cs_nav_node.h"
#include "nav_colors.h"
#include "nav_mesh.h"

NavDirType Opposite[ NUM_DIRECTIONS ] = { SOUTH, WEST, NORTH, EAST };

CSNavNode *CSNavNode::m_list = NULL;
unsigned int CSNavNode::m_listLength = 0;
unsigned int CSNavNode::m_nextID = 1;

ConVar nav_show_nodes( "nav_show_nodes", "0" );


//--------------------------------------------------------------------------------------------------------------
class LookAtTarget
{
public:
	LookAtTarget( const Vector &target )
	{
		m_target = target;
	}

	bool operator()( CBasePlayer *player )
	{
		QAngle angles;
		Vector to = m_target - player->GetAbsOrigin();
		VectorAngles( to, angles );

		player->SetLocalAngles( angles );
		player->SnapEyeAngles( angles );
		return true;
	}

private:
	Vector m_target;
};


//--------------------------------------------------------------------------------------------------------------
/**
 * Constructor
 */
CSNavNode::CSNavNode( const Vector &pos, const Vector &normal, CSNavNode *parent )
{
	m_pos = pos;
	m_normal = normal;

	m_id = m_nextID++;

	int i;
	for( i=0; i<NUM_DIRECTIONS; ++i )
	{
		m_to[ i ] = NULL;
	}

	for ( i=0; i<NUM_CORNERS; ++i )
	{
		m_crouch[ i ] = false;
	}

	m_visited = 0;
	m_parent = parent;

	m_next = m_list;
	m_list = this;
	m_listLength++;

	m_isCovered = false;
	m_area = NULL;

	m_attributeFlags = 0;

	if ( nav_show_nodes.GetBool() )
	{
		NDebugOverlay::Cross3D( m_pos, 10.0f, 128, 128, 128, true, 10.0f );
		NDebugOverlay::Cross3D( m_pos, 10.0f, 255, 255, 255, false, 10.0f );

		LookAtTarget lookAt( m_pos );
		ForEachPlayer( lookAt );
	}
}


//--------------------------------------------------------------------------------------------------------------
#if DEBUG_NAV_NODES
ConVar nav_show_node_id( "nav_show_node_id", "0" );
ConVar nav_test_node( "nav_test_node", "0" );
ConVar nav_test_node_crouch( "nav_test_node_crouch", "0" );
ConVar nav_test_node_crouch_dir( "nav_test_node_crouch_dir", "4" );
#endif // DEBUG_NAV_NODES


//--------------------------------------------------------------------------------------------------------------
void CSNavNode::Draw( void )
{
#if DEBUG_NAV_NODES
	if ( !nav_show_nodes.GetBool() )
		return;

	int r = 0, g = 0, b = 0;

	if ( m_isCovered )
	{
		if ( GetAttributes() & NAV_MESH_CROUCH )
		{
			b = 255;
		}
		else
		{
			r = 255;
		}
	}
	else
	{
		if ( GetAttributes() & NAV_MESH_CROUCH )
		{
			b = 255;
		}
		g = 255;
	}

	NDebugOverlay::Cross3D( m_pos, 2, r, g, b, true, 0.1f );

	if ( (!m_isCovered && nav_show_node_id.GetBool()) || (m_isCovered && nav_show_node_id.GetInt() < 0) )
	{
		char text[16];
		Q_snprintf( text, sizeof( text ), "%d", m_id );
		NDebugOverlay::Text( m_pos, text, true, 0.1f );
	}

	if ( (unsigned int)(nav_test_node.GetInt()) == m_id )
	{
		TheNavMesh->TestArea( this, 1, 1 );
		nav_test_node.SetValue( 0 );
	}

	if ( (unsigned int)(nav_test_node_crouch.GetInt()) == m_id )
	{
		CheckCrouch();
		nav_test_node_crouch.SetValue( 0 );
	}

	if ( GetAttributes() & NAV_MESH_CROUCH )
	{
		int i;
		for( i=0; i<NUM_CORNERS; i++ )
		{
			if ( m_crouch[i] )
			{
				Vector2D dir;
				CornerToVector2D( (NavCornerType)i, &dir );

				const float scale = 3.0f;
				Vector scaled( dir.x * scale, dir.y * scale, 0 );

				NDebugOverlay::HorzArrow( m_pos, m_pos + scaled, 0.5, 0, 0, 255, 255, true, 0.1f );
			}
		}
	}

#endif // DEBUG_NAV_NODES
}


//--------------------------------------------------------------------------------------------------------------
void CSNavNode::CheckCrouch( void )
{
	CTraceFilterWalkableEntities filter( NULL, COLLISION_GROUP_PLAYER_MOVEMENT, WALK_THRU_EVERYTHING );
	trace_t tr;

	// Trace downward from duck height to find the max floor height for the node's surroundings
	Vector mins( -HalfHumanWidth, -HalfHumanWidth, 0 );
	Vector maxs( HalfHumanWidth, HalfHumanWidth, 0 );
	Vector start( m_pos.x, m_pos.y, m_pos.z + VEC_DUCK_HULL_MAX.z - 0.1f );
	UTIL_TraceHull(
		start,
		m_pos,
		mins,
		maxs,
		MASK_PLAYERSOLID_BRUSHONLY,
		&filter,
		&tr );

	Vector groundPos = tr.endpos;

	if ( tr.startsolid && !tr.allsolid )
	{
		// Try going down out of the solid and re-check for the floor height
		start.z -= tr.endpos.z - 0.1f;

		UTIL_TraceHull(
			start,
			m_pos,
			mins,
			maxs,
			MASK_PLAYERSOLID_BRUSHONLY,
			&filter,
			&tr );

		groundPos = tr.endpos;
	}

	if ( tr.startsolid )
	{
		// we don't even have duck height clear.  try a simple check to find floor height.
		float x, y;

		// Find the highest floor z - for a player to stand in this area, we need a full
		// VEC_HULL_MAX.z of clearance above this height at all points.
		float maxFloorZ = m_pos.z;
		for( y = -HalfHumanWidth; y <= HalfHumanWidth + 0.1f; y += HalfHumanWidth )
		{
			for( x = -HalfHumanWidth; x <= HalfHumanWidth + 0.1f; x += HalfHumanWidth )
			{
				float floorZ;
				if ( TheNavMesh->GetGroundHeight( m_pos, &floorZ ) )
				{
					maxFloorZ = MAX( maxFloorZ, floorZ + 0.1f );
				}
			}
		}

		groundPos.Init( m_pos.x, m_pos.y, maxFloorZ );
	}

	// For each direction, trace upwards from our best ground height to VEC_HULL_MAX.z to see if we have standing room.
	for ( int i=0; i<NUM_CORNERS; ++i )
	{
#if DEBUG_NAV_NODES
		if ( nav_test_node_crouch_dir.GetInt() != NUM_CORNERS && i != nav_test_node_crouch_dir.GetInt() )
			continue;
#endif // DEBUG_NAV_NODES

		NavCornerType corner = (NavCornerType)i;
		Vector2D cornerVec;
		CornerToVector2D( corner, &cornerVec );

		Vector actualGroundPos = groundPos; // we might need to adjust this if the tracehull failed above and we fell back to m_pos.z

		// Build a mins/maxs pair for the HumanWidth x HalfHumanWidth box facing the appropriate direction
		mins.Init();
		maxs.Init( cornerVec.x * HalfHumanWidth, cornerVec.y * HalfHumanWidth, 0 );

		// now make sure that mins is smaller than maxs
		for ( int j=0; j<3; ++j )
		{
			if ( mins[j] > maxs[j] )
			{
				float tmp = mins[j];
				mins[j] = maxs[j];
				maxs[j] = tmp;
			}
		}

		UTIL_TraceHull(
			actualGroundPos + Vector( 0, 0, 0.1f ),
			actualGroundPos + Vector( 0, 0, VEC_HULL_MAX.z - 0.2f ),
			mins,
			maxs,
			MASK_PLAYERSOLID_BRUSHONLY,
			&filter,
			&tr );
		actualGroundPos.z += tr.fractionleftsolid * VEC_HULL_MAX.z;
		float maxHeight = actualGroundPos.z + VEC_DUCK_HULL_MAX.z;
		for ( ; tr.startsolid && actualGroundPos.z <= maxHeight; actualGroundPos.z += 1.0f )
		{
			// In case we didn't find a good ground pos above, we could start in the ground.  Move us up some.
			UTIL_TraceHull(
				actualGroundPos + Vector( 0, 0, 0.1f ),
				actualGroundPos + Vector( 0, 0, VEC_HULL_MAX.z - 0.2f ),
				mins,
				maxs,
				MASK_PLAYERSOLID_BRUSHONLY,
				&filter,
				&tr );
		}
		if (tr.startsolid || tr.fraction != 1.0f)
		{
			SetAttributes( NAV_MESH_CROUCH );
			m_crouch[corner] = true;
		}

#if DEBUG_NAV_NODES
		if ( nav_show_nodes.GetBool() )
		{
			if ( nav_test_node_crouch_dir.GetInt() == i || nav_test_node_crouch_dir.GetInt() == NUM_CORNERS  )
			{
				if ( tr.startsolid )
				{
					NDebugOverlay::Box( actualGroundPos, mins, maxs+Vector( 0, 0, VEC_HULL_MAX.z), 255, 0, 0, 10, 20.0f );
				}
				else if ( m_crouch[corner] )
				{
					NDebugOverlay::Box( actualGroundPos, mins, maxs+Vector( 0, 0, VEC_HULL_MAX.z), 0, 0, 255, 10, 20.0f );
				}
				else
				{
					NDebugOverlay::Box( actualGroundPos, mins, maxs+Vector( 0, 0, VEC_HULL_MAX.z), 0, 255, 0, 10, 10.0f );
				}
			}
		}
#endif // DEBUG_NAV_NODES
	}
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Create a connection FROM this node TO the given node, in the given direction
 */
void CSNavNode::ConnectTo( CSNavNode *node, NavDirType dir )
{
	m_to[ dir ] = node;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return node at given position.
 * @todo Need a hash table to make this lookup fast
 */
CSNavNode *CSNavNode::GetNode( const Vector &pos )
{
	const float tolerance = 0.45f * GenerationStepSize;			// 1.0f

	for( CSNavNode *node = m_list; node; node = node->m_next )
	{
		float dx = fabs( node->m_pos.x - pos.x );
		float dy = fabs( node->m_pos.y - pos.y );
		float dz = fabs( node->m_pos.z - pos.z );

		if (dx < tolerance && dy < tolerance && dz < tolerance)
			return node;
	}

	return NULL;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this node is bidirectionally linked to 
 * another node in the given direction
 */
BOOL CSNavNode::IsBiLinked( NavDirType dir ) const
{
	if (m_to[ dir ] && m_to[ dir ]->m_to[ Opposite[dir] ] == this)
	{
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this node is the NW corner of a quad of nodes
 * that are all bidirectionally linked.
 */
BOOL CSNavNode::IsClosedCell( void ) const
{
	if (IsBiLinked( SOUTH ) &&
		IsBiLinked( EAST ) &&
		m_to[ EAST ]->IsBiLinked( SOUTH ) &&
		m_to[ SOUTH ]->IsBiLinked( EAST ) &&
		m_to[ EAST ]->m_to[ SOUTH ] == m_to[ SOUTH ]->m_to[ EAST ])
	{
		return true;
	}

	return false;
}


//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// nav_area.cpp
// AI Navigation areas
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

#include "cbase.h"
#include "cs_nav_mesh.h"
#include "cs_nav_area.h"
#include "nav_pathfind.h"
#include "nav_colors.h"
#include "fmtstr.h"
#include "props_shared.h"
#include "func_breakablesurf.h"
#include "Color.h"
#include "collisionutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#ifdef _WIN32
#pragma warning (disable:4701)				// disable warning that variable *may* not be initialized 
#endif


//--------------------------------------------------------------------------------------------------------------
/**
 * Constructor used during normal runtime.
 */
CCSNavArea::CCSNavArea( void )
{
	m_approachCount = 0;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Destructor
 */
CCSNavArea::~CCSNavArea()
{
}


void CCSNavArea::OnServerActivate( void )
{
	CNavArea::OnServerActivate();

}

void CCSNavArea::OnRoundRestart( void )
{
	CNavArea::OnRoundRestart();
}


void CCSNavArea::Save( CUtlBuffer &fileBuffer, unsigned int version ) const
{
	CNavArea::Save( fileBuffer, version );

	//
	// Save the approach areas for this area
	//

	// save number of approach areas
	fileBuffer.PutUnsignedChar(m_approachCount);

	// save approach area info
	for( int a=0; a<m_approachCount; ++a )
	{
		if (m_approach[a].here.area)
			fileBuffer.PutUnsignedInt(m_approach[a].here.area->GetID());
		else
			fileBuffer.PutUnsignedInt(0);

		if (m_approach[a].prev.area)
			fileBuffer.PutUnsignedInt(m_approach[a].prev.area->GetID());
		else
			fileBuffer.PutUnsignedInt(0);
		fileBuffer.PutUnsignedChar(m_approach[a].prevToHereHow);

		if (m_approach[a].next.area)
			fileBuffer.PutUnsignedInt(m_approach[a].next.area->GetID());
		else
			fileBuffer.PutUnsignedInt(0);
		fileBuffer.PutUnsignedChar(m_approach[a].hereToNextHow);
	}
}

NavErrorType CCSNavArea::Load( CUtlBuffer &fileBuffer, unsigned int version, unsigned int subVersion )
{
	if ( version < 15 )
		return LoadLegacy(fileBuffer, version, subVersion);

	// load base class data
	NavErrorType error = CNavArea::Load( fileBuffer, version, subVersion );

	switch ( subVersion )
	{
	case 1:
		//
		// Load number of approach areas
		//
		m_approachCount = fileBuffer.GetUnsignedChar();

		// load approach area info (IDs)
		for( int a = 0; a < m_approachCount; ++a )
		{
			m_approach[a].here.id = fileBuffer.GetUnsignedInt();

			m_approach[a].prev.id = fileBuffer.GetUnsignedInt();
			m_approach[a].prevToHereHow = (NavTraverseType)fileBuffer.GetUnsignedChar();

			m_approach[a].next.id = fileBuffer.GetUnsignedInt();
			m_approach[a].hereToNextHow = (NavTraverseType)fileBuffer.GetUnsignedChar();
		}

		if ( !fileBuffer.IsValid() )
			error = NAV_INVALID_FILE;

		// fall through

	case 0:
		// legacy version
		break;

	default:
		Warning( "Unknown NavArea sub-version number\n" );
		error = NAV_INVALID_FILE;
	}

	return error;
}


NavErrorType CCSNavArea::PostLoad( void )
{
	NavErrorType error = CNavArea::PostLoad();

	// resolve approach area IDs
	for ( int a = 0; a < m_approachCount; ++a )
	{
		m_approach[a].here.area = TheNavMesh->GetNavAreaByID( m_approach[a].here.id );
		if (m_approach[a].here.id && m_approach[a].here.area == NULL)
		{
			Msg( "CNavArea::PostLoad: Corrupt navigation data. Missing Approach Area (here).\n" );
			error = NAV_CORRUPT_DATA;
		}

		m_approach[a].prev.area = TheNavMesh->GetNavAreaByID( m_approach[a].prev.id );
		if (m_approach[a].prev.id && m_approach[a].prev.area == NULL)
		{
			Msg( "CNavArea::PostLoad: Corrupt navigation data. Missing Approach Area (prev).\n" );
			error = NAV_CORRUPT_DATA;
		}

		m_approach[a].next.area = TheNavMesh->GetNavAreaByID( m_approach[a].next.id );
		if (m_approach[a].next.id && m_approach[a].next.area == NULL)
		{
			Msg( "CNavArea::PostLoad: Corrupt navigation data. Missing Approach Area (next).\n" );
			error = NAV_CORRUPT_DATA;
		}
	}
	return error;
}


void CCSNavArea::Draw( void ) const
{
	CNavArea::Draw();
}

void CCSNavArea::CustomAnalysis( bool isIncremental /*= false */ )
{
	ComputeApproachAreas();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Load legacy navigation area from the file
 */
NavErrorType CCSNavArea::LoadLegacy( CUtlBuffer &fileBuffer, unsigned int version, unsigned int subVersion )
{
	// load ID
	m_id = fileBuffer.GetUnsignedInt();

	// update nextID to avoid collisions
	if (m_id >= m_nextID)
		m_nextID = m_id+1;

	// load attribute flags
	if ( version <= 8 )
	{
		m_attributeFlags = fileBuffer.GetUnsignedChar();
	}
	else if ( version < 13 )
	{
		m_attributeFlags = fileBuffer.GetUnsignedShort();
	}
	else
	{
		m_attributeFlags = fileBuffer.GetInt();
	}

	// load extent of area
	fileBuffer.Get( &m_nwCorner, 3*sizeof(float) );
	fileBuffer.Get( &m_seCorner, 3*sizeof(float) );

	m_center.x = (m_nwCorner.x + m_seCorner.x)/2.0f;
	m_center.y = (m_nwCorner.y + m_seCorner.y)/2.0f;
	m_center.z = (m_nwCorner.z + m_seCorner.z)/2.0f;

	if ( ( m_seCorner.x - m_nwCorner.x ) > 0.0f && ( m_seCorner.y - m_nwCorner.y ) > 0.0f )
	{
		m_invDxCorners = 1.0f / ( m_seCorner.x - m_nwCorner.x );
		m_invDyCorners = 1.0f / ( m_seCorner.y - m_nwCorner.y );
	}
	else
	{
		m_invDxCorners = m_invDyCorners = 0;

		DevWarning( "Degenerate Navigation Area #%d at setpos %g %g %g\n", 
			m_id, m_center.x, m_center.y, m_center.z );
	}

	// load heights of implicit corners
	m_neZ = fileBuffer.GetFloat();
	m_swZ = fileBuffer.GetFloat();

	CheckWaterLevel();

	// load connections (IDs) to adjacent areas
	// in the enum order NORTH, EAST, SOUTH, WEST
	for( int d=0; d<NUM_DIRECTIONS; d++ )
	{
		// load number of connections for this direction
		unsigned int count = fileBuffer.GetUnsignedInt();
		Assert( fileBuffer.IsValid() );

		m_connect[d].EnsureCapacity( count );
		for( unsigned int i=0; i<count; ++i )
		{
			NavConnect connect;
			connect.id = fileBuffer.GetUnsignedInt();
			Assert( fileBuffer.IsValid() );

			// don't allow self-referential connections
			if ( connect.id != m_id )
			{
				m_connect[d].AddToTail( connect );
			}
		}
	}

	//
	// Load hiding spots
	//

	// load number of hiding spots
	unsigned char hidingSpotCount = fileBuffer.GetUnsignedChar();

	if (version == 1)
	{
		// load simple vector array
		Vector pos;
		for( int h=0; h<hidingSpotCount; ++h )
		{
			fileBuffer.Get( &pos, 3 * sizeof(float) );

			// create new hiding spot and put on master list
			HidingSpot *spot = TheNavMesh->CreateHidingSpot();
			spot->SetPosition( pos );
			spot->SetFlags( HidingSpot::IN_COVER );
			m_hidingSpots.AddToTail( spot );
		}
	}
	else
	{
		// load HidingSpot objects for this area
		for( int h=0; h<hidingSpotCount; ++h )
		{
			// create new hiding spot and put on master list
			HidingSpot *spot = TheNavMesh->CreateHidingSpot();

			spot->Load( fileBuffer, version );
			
			m_hidingSpots.AddToTail( spot );
		}
	}

	if ( version < 15 )
	{
		//
		// Load number of approach areas
		//
		m_approachCount = fileBuffer.GetUnsignedChar();

		// load approach area info (IDs)
		for( int a = 0; a < m_approachCount; ++a )
		{
			m_approach[a].here.id = fileBuffer.GetUnsignedInt();

			m_approach[a].prev.id = fileBuffer.GetUnsignedInt();
			m_approach[a].prevToHereHow = (NavTraverseType)fileBuffer.GetUnsignedChar();

			m_approach[a].next.id = fileBuffer.GetUnsignedInt();
			m_approach[a].hereToNextHow = (NavTraverseType)fileBuffer.GetUnsignedChar();
		}
	}


	//
	// Load encounter paths for this area
	//
	unsigned int count = fileBuffer.GetUnsignedInt();

	if (version < 3)
	{
		// old data, read and discard
		for( unsigned int e=0; e<count; ++e )
		{
			SpotEncounter encounter;

			encounter.from.id = fileBuffer.GetUnsignedInt();
			encounter.to.id = fileBuffer.GetUnsignedInt();

			fileBuffer.Get( &encounter.path.from.x, 3 * sizeof(float) );
			fileBuffer.Get( &encounter.path.to.x, 3 * sizeof(float) );

			// read list of spots along this path
			unsigned char spotCount = fileBuffer.GetUnsignedChar();
		
			for( int s=0; s<spotCount; ++s )
			{
				fileBuffer.GetFloat();
				fileBuffer.GetFloat();
				fileBuffer.GetFloat();
				fileBuffer.GetFloat();
			}
		}
		return NAV_OK;
	}

	for( unsigned int e=0; e<count; ++e )
	{
		SpotEncounter *encounter = new SpotEncounter;

		encounter->from.id = fileBuffer.GetUnsignedInt();

		unsigned char dir = fileBuffer.GetUnsignedChar();
		encounter->fromDir = static_cast<NavDirType>( dir );

		encounter->to.id = fileBuffer.GetUnsignedInt();

		dir = fileBuffer.GetUnsignedChar();
		encounter->toDir = static_cast<NavDirType>( dir );

		// read list of spots along this path
		unsigned char spotCount = fileBuffer.GetUnsignedChar();
	
		SpotOrder order;
		for( int s=0; s<spotCount; ++s )
		{
			order.id = fileBuffer.GetUnsignedInt();

			unsigned char t = fileBuffer.GetUnsignedChar();

			order.t = (float)t/255.0f;

			encounter->spots.AddToTail( order );
		}

		m_spotEncounters.AddToTail( encounter );
	}

	if (version < 5)
		return NAV_OK;

	//
	// Load Place data
	//
	PlaceDirectory::IndexType entry = fileBuffer.GetUnsignedShort();

	// convert entry to actual Place
	SetPlace( placeDirectory.IndexToPlace( entry ) );

	if ( version < 7 )
		return NAV_OK;

	// load ladder data
	for ( int dir=0; dir<CNavLadder::NUM_LADDER_DIRECTIONS; ++dir )
	{
		count = fileBuffer.GetUnsignedInt();
		for( unsigned int i=0; i<count; ++i )
		{
			NavLadderConnect connect;
			connect.id = fileBuffer.GetUnsignedInt();

			bool alreadyConnected = false;
			FOR_EACH_VEC( m_ladder[dir], j )
			{
				if ( m_ladder[dir][j].id == connect.id )
				{
					alreadyConnected = true;
					break;
				}
			}

			if ( !alreadyConnected )
			{
				m_ladder[dir].AddToTail( connect );
			}
		}
	}

	if ( version < 8 )
		return NAV_OK;

	// load earliest occupy times
	for( int i=0; i<MAX_NAV_TEAMS; ++i )
	{
		// no spot in the map should take longer than this to reach
		m_earliestOccupyTime[i] = fileBuffer.GetFloat();
	}

	if ( version < 11 )
		return NAV_OK;

	// load light intensity
	for ( int i=0; i<NUM_CORNERS; ++i )
	{
		m_lightIntensity[i] = fileBuffer.GetFloat();
	}

	if ( version < 16 )
		return NAV_OK;

	// load visibility information
	unsigned int visibleAreaCount = fileBuffer.GetUnsignedInt();
	if ( !IsX360() )
	{
		m_potentiallyVisibleAreas.EnsureCapacity( visibleAreaCount );
	}
	else
	{
/* TODO: Re-enable when latest 360 code gets integrated (MSB 5/5/09)
		size_t nBytes = visibleAreaCount * sizeof( AreaBindInfo ); 
		m_potentiallyVisibleAreas.~CAreaBindInfoArray();
		new ( &m_potentiallyVisibleAreas ) CAreaBindInfoArray( (AreaBindInfo *)engine->AllocLevelStaticData( nBytes ), visibleAreaCount );
*/
	}

	for( unsigned int j=0; j<visibleAreaCount; ++j )
	{
		AreaBindInfo info;
		info.id = fileBuffer.GetUnsignedInt();
		info.attributes = fileBuffer.GetUnsignedChar();

		m_potentiallyVisibleAreas.AddToTail( info );
	}

	// read area from which we inherit visibility
	m_inheritVisibilityFrom.id = fileBuffer.GetUnsignedInt();

	return NAV_OK;
}



//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// nav_file.cpp
// Reading and writing nav files
// Author: Michael S. Booth (mike@turtlerockstudios.com), January-September 2003

#include "cbase.h"
#include "nav_mesh.h"

#ifdef CSTRIKE_DLL
#include "cs_shareddefs.h"
#include "cs_nav_pathfind.h"
#endif

#if 0
//--------------------------------------------------------------------------------------------------------------
/// The current version of the nav file format
const int NavCurrentVersion = 9;

//--------------------------------------------------------------------------------------------------------------
//
// The 'place directory' is used to save and load places from
// nav files in a size-efficient manner that also allows for the 
// order of the place ID's to change without invalidating the
// nav files.
//
// The place directory is stored in the nav file as a list of 
// place name strings.  Each nav area then contains an index
// into that directory, or zero if no place has been assigned to 
// that area.
//
class PlaceDirectory
{
public:

	typedef unsigned short IndexType;

	void Reset( void )
	{
		m_directory.RemoveAll();
	}

	/// return true if this place is already in the directory
	bool IsKnown( Place place ) const
	{
		return m_directory.HasElement( place );
	}

	/// return the directory index corresponding to this Place (0 = no entry)
	IndexType GetIndex( Place place ) const
	{
		if (place == UNDEFINED_PLACE)
			return 0;

		int i = m_directory.Find( place );

		if (i < 0)
		{
			Assert( false && "PlaceDirectory::GetIndex failure" );
			return 0;
		}

		return (IndexType)(i+1);
	}

	/// add the place to the directory if not already known
	void AddPlace( Place place )
	{
		if (place == UNDEFINED_PLACE)
			return;

		Assert( place < 1000 );

		if (IsKnown( place ))
			return;

		m_directory.AddToTail( place );
	}

	/// given an index, return the Place
	Place IndexToPlace( IndexType entry ) const
	{
		if (entry == 0)
			return UNDEFINED_PLACE;

		int i = entry-1;

		if (i >= m_directory.Count())
		{
			Assert( false && "PlaceDirectory::IndexToPlace: Invalid entry" );
			return UNDEFINED_PLACE;
		}

		return m_directory[ i ];
	}

	/// store the directory
	void Save( FileHandle_t file )
	{
		// store number of entries in directory
		IndexType count = (IndexType)m_directory.Count();
		filesystem->Write( &count, sizeof(IndexType), file );

		// store entries		
		for( int i=0; i<m_directory.Count(); ++i )
		{
			const char *placeName = TheNavMesh->PlaceToName( m_directory[i] );

			// store string length followed by string itself
			unsigned short len = (unsigned short)(strlen( placeName ) + 1);
			filesystem->Write( &len, sizeof(unsigned short), file );
			filesystem->Write( placeName, len, file );
		}
	}

	/// load the directory
	void Load( FileHandle_t file )
	{
		// read number of entries
		IndexType count;
		filesystem->Read( &count, sizeof(IndexType), file );

		m_directory.RemoveAll();

		// read each entry
		char placeName[256];
		unsigned short len;
		for( int i=0; i<count; ++i )
		{
			filesystem->Read( &len, sizeof(unsigned short), file );
			filesystem->Read( placeName, len, file );

			AddPlace( TheNavMesh->NameToPlace( placeName ) );
		}
	}

private:
	CUtlVector< Place > m_directory;
};

static PlaceDirectory placeDirectory;


//--------------------------------------------------------------------------------------------------------------
/**
 * Replace extension with "bsp"
 */
char *GetBspFilename( const char *navFilename )
{
	static char bspFilename[256];

	Q_snprintf( bspFilename, sizeof( bspFilename ), "maps\\%s.bsp", STRING( gpGlobals->mapname ) );

	int len = strlen( bspFilename );
	if (len < 3)
		return NULL;

	bspFilename[ len-3 ] = 'b';
	bspFilename[ len-2 ] = 's';
	bspFilename[ len-1 ] = 'p';

	return bspFilename;
}

//--------------------------------------------------------------------------------------------------------------
/*
void CNavArea::Save( FILE *fp ) const
{
	fprintf( fp, "v  %f %f %f\n", m_extent.lo.x, m_extent.lo.y, m_extent.lo.z );
	fprintf( fp, "v  %f %f %f\n", m_extent.hi.x, m_extent.lo.y, m_neZ );
	fprintf( fp, "v  %f %f %f\n", m_extent.hi.x, m_extent.hi.y, m_extent.hi.z );
	fprintf( fp, "v  %f %f %f\n", m_extent.lo.x, m_extent.hi.y, m_swZ );

	static int base = 1;
	fprintf( fp, "\n\ng %04dArea%s%s%s%s\n", m_id, 
				(GetAttributes() & BOT_NAV_CROUCH) ? "CROUCH" : "",
				(GetAttributes() & BOT_NAV_JUMP) ? "JUMP" : "",
				(GetAttributes() & BOT_NAV_PRECISE) ? "PRECISE" : "",
				(GetAttributes() & BOT_NAV_NO_JUMP) ? "NO_JUMP" : "" );
	fprintf( fp, "f %d %d %d %d\n\n", base, base+1, base+2, base+3 );
	base += 4;
}
*/

//--------------------------------------------------------------------------------------------------------------
/**
 * Save a navigation area to the opened binary stream
 */
void CNavArea::Save( FileHandle_t file, unsigned int version ) const
{
	// save ID
	filesystem->Write( &m_id, sizeof(unsigned int), file );

	// save attribute flags
	filesystem->Write( &m_attributeFlags, sizeof(unsigned short), file );

	// save extent of area
	filesystem->Write( &m_extent, 6*sizeof(float), file );

	// save heights of implicit corners
	filesystem->Write( &m_neZ, sizeof(float), file );
	filesystem->Write( &m_swZ, sizeof(float), file );

	// save connections to adjacent areas
	// in the enum order NORTH, EAST, SOUTH, WEST
	for( int d=0; d<NUM_DIRECTIONS; d++ )
	{
		// save number of connections for this direction
		unsigned int count = m_connect[d].Count();
		filesystem->Write( &count, sizeof(unsigned int), file );

		FOR_EACH_LL( m_connect[d], it )
		{
			NavConnect connect = m_connect[d][ it ];
			filesystem->Write( &connect.area->m_id, sizeof(unsigned int), file );
		}
	}

	//
	// Store hiding spots for this area
	//
	unsigned char count;
	if (m_hidingSpotList.Count() > 255)
	{
		count = 255;
		Warning( "Warning: NavArea #%d: Truncated hiding spot list to 255\n", m_id );
	}
	else
	{
		count = (unsigned char)m_hidingSpotList.Count();
	}
	filesystem->Write( &count, sizeof(unsigned char), file );

	// store HidingSpot objects
	unsigned int saveCount = 0;
	FOR_EACH_LL( m_hidingSpotList, hit )
	{
		HidingSpot *spot = m_hidingSpotList[ hit ];
		
		spot->Save( file, version );

		// overflow check
		if (++saveCount == count)
			break;
	}

	//
	// Save the approach areas for this area
	//

	// save number of approach areas
	filesystem->Write( &m_approachCount, sizeof(unsigned char), file );

	// save approach area info
	unsigned char type;
	unsigned int zero = 0;
	for( int a=0; a<m_approachCount; ++a )
	{
		if (m_approach[a].here.area)
			filesystem->Write( &m_approach[a].here.area->m_id, sizeof(unsigned int), file );
		else
			filesystem->Write( &zero, sizeof(unsigned int), file );

		if (m_approach[a].prev.area)
			filesystem->Write( &m_approach[a].prev.area->m_id, sizeof(unsigned int), file );
		else
			filesystem->Write( &zero, sizeof(unsigned int), file );
		type = (unsigned char)m_approach[a].prevToHereHow;
		filesystem->Write( &type, sizeof(unsigned char), file );

		if (m_approach[a].next.area)
			filesystem->Write( &m_approach[a].next.area->m_id, sizeof(unsigned int), file );
		else
			filesystem->Write( &zero, sizeof(unsigned int), file );
		type = (unsigned char)m_approach[a].hereToNextHow;
		filesystem->Write( &type, sizeof(unsigned char), file );
	}

	//
	// Save encounter spots for this area
	//
	{
		// save number of encounter paths for this area
		unsigned int count = m_spotEncounterList.Count();
		filesystem->Write( &count, sizeof(unsigned int), file );

		SpotEncounter *e;
		FOR_EACH_LL( m_spotEncounterList, it )
		{
			e = m_spotEncounterList[ it ];

			if (e->from.area)
				filesystem->Write( &e->from.area->m_id, sizeof(unsigned int), file );
			else
				filesystem->Write( &zero, sizeof(unsigned int), file );

			unsigned char dir = (unsigned char)e->fromDir;
			filesystem->Write( &dir, sizeof(unsigned char), file );

			if (e->to.area)
				filesystem->Write( &e->to.area->m_id, sizeof(unsigned int), file );
			else
				filesystem->Write( &zero, sizeof(unsigned int), file );

			dir = (unsigned char)e->toDir;
			filesystem->Write( &dir, sizeof(unsigned char), file );

			// write list of spots along this path
			unsigned char spotCount;
			if (e->spotList.Count() > 255)
			{
				spotCount = 255;
				Warning( "Warning: NavArea #%d: Truncated encounter spot list to 255\n", m_id );
			}
			else
			{
				spotCount = (unsigned char)e->spotList.Count();
			}
			filesystem->Write( &spotCount, sizeof(unsigned char), file );
		
			saveCount = 0;
			FOR_EACH_LL( e->spotList, sit )
			{
				SpotOrder *order = &e->spotList[ sit ];

				// order->spot may be NULL if we've loaded a nav mesh that has been edited but not re-analyzed
				unsigned int id = (order->spot) ? order->spot->GetID() : 0;
				filesystem->Write( &id, sizeof(unsigned int), file );

				unsigned char t = (unsigned char)(255 * order->t);
				filesystem->Write( &t, sizeof(unsigned char), file );

				// overflow check
				if (++saveCount == spotCount)
					break;
			}
		}
	}

	// store place dictionary entry
	PlaceDirectory::IndexType entry = placeDirectory.GetIndex( GetPlace() );
	filesystem->Write( &entry, sizeof(entry), file );

	// write out ladder info
	int i;
	for ( i=0; i<CSNavLadder::NUM_LADDER_DIRECTIONS; ++i )
	{
		// save number of encounter paths for this area
		unsigned int count = m_ladder[i].Count();
		filesystem->Write( &count, sizeof(unsigned int), file );

		NavLadderConnect ladder;
		FOR_EACH_LL( m_ladder[i], it )
		{
			ladder = m_ladder[i][it];

			unsigned int id = ladder.ladder->GetID();
			filesystem->Write( &id, sizeof( id ), file );
		}
	}

	// save earliest occupy times
	for( i=0; i<MAX_NAV_TEAMS; ++i )
	{
		// no spot in the map should take longer than this to reach
		filesystem->Write( &m_earliestOccupyTime[i], sizeof(m_earliestOccupyTime[i]), file );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Load a navigation area from the file
 */
void CNavArea::Load( FileHandle_t file, unsigned int version )
{
	// load ID
	filesystem->Read( &m_id, sizeof(unsigned int), file );

	// update nextID to avoid collisions
	if (m_id >= m_nextID)
		m_nextID = m_id+1;

	// load attribute flags
	if ( version <= 8 )
	{
		unsigned char flags = 0;
		filesystem->Read( &flags, sizeof(unsigned char), file );
		m_attributeFlags = flags;
	}
	else
	{
		filesystem->Read( &m_attributeFlags, sizeof(unsigned short), file );
	}

	// load extent of area
	filesystem->Read( &m_extent, 6*sizeof(float), file );

	m_center.x = (m_extent.lo.x + m_extent.hi.x)/2.0f;
	m_center.y = (m_extent.lo.y + m_extent.hi.y)/2.0f;
	m_center.z = (m_extent.lo.z + m_extent.hi.z)/2.0f;

	// load heights of implicit corners
	filesystem->Read( &m_neZ, sizeof(float), file );
	filesystem->Read( &m_swZ, sizeof(float), file );

	CheckWaterLevel();

	// load connections (IDs) to adjacent areas
	// in the enum order NORTH, EAST, SOUTH, WEST
	for( int d=0; d<NUM_DIRECTIONS; d++ )
	{
		// load number of connections for this direction
		unsigned int count;
		int result = filesystem->Read( &count, sizeof(unsigned int), file );
		Assert( result == sizeof(unsigned int) );

		for( unsigned int i=0; i<count; ++i )
		{
			NavConnect connect;
			result = filesystem->Read( &connect.id, sizeof(unsigned int), file );
			Assert( result == sizeof(unsigned int) );

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
	unsigned char hidingSpotCount;
	filesystem->Read( &hidingSpotCount, sizeof(unsigned char), file );

	if (version == 1)
	{
		// load simple vector array
		Vector pos;
		for( int h=0; h<hidingSpotCount; ++h )
		{
			filesystem->Read( &pos, 3 * sizeof(float), file );

			// create new hiding spot and put on master list
			HidingSpot *spot = TheNavMesh->CreateHidingSpot();
			spot->SetPosition( pos );
			spot->SetFlags( HidingSpot::IN_COVER );
			m_hidingSpotList.AddToTail( spot );
		}
	}
	else
	{
		// load HidingSpot objects for this area
		for( int h=0; h<hidingSpotCount; ++h )
		{
			// create new hiding spot and put on master list
			HidingSpot *spot = TheNavMesh->CreateHidingSpot();

			spot->Load( file, version );
			
			m_hidingSpotList.AddToTail( spot );
		}
	}

	//
	// Load number of approach areas
	//
	filesystem->Read( &m_approachCount, sizeof(unsigned char), file );

	// load approach area info (IDs)
	unsigned char type;
	for( int a=0; a<m_approachCount; ++a )
	{
		filesystem->Read( &m_approach[a].here.id, sizeof(unsigned int), file );

		filesystem->Read( &m_approach[a].prev.id, sizeof(unsigned int), file );
		filesystem->Read( &type, sizeof(unsigned char), file );
		m_approach[a].prevToHereHow = (NavTraverseType)type;

		filesystem->Read( &m_approach[a].next.id, sizeof(unsigned int), file );
		filesystem->Read( &type, sizeof(unsigned char), file );
		m_approach[a].hereToNextHow = (NavTraverseType)type;
	}


	//
	// Load encounter paths for this area
	//
	unsigned int count;
	filesystem->Read( &count, sizeof(unsigned int), file );

	if (version < 3)
	{
		// old data, read and discard
		for( unsigned int e=0; e<count; ++e )
		{
			SpotEncounter encounter;

			filesystem->Read( &encounter.from.id, sizeof(unsigned int), file );
			filesystem->Read( &encounter.to.id, sizeof(unsigned int), file );

			filesystem->Read( &encounter.path.from.x, 3 * sizeof(float), file );
			filesystem->Read( &encounter.path.to.x, 3 * sizeof(float), file );

			// read list of spots along this path
			unsigned char spotCount;
			filesystem->Read( &spotCount, sizeof(unsigned char), file );
		
			for( int s=0; s<spotCount; ++s )
			{
				Vector pos;
				filesystem->Read( &pos, 3*sizeof(float), file );
				filesystem->Read( &pos, sizeof(float), file );
			}
		}
		return;
	}

	for( unsigned int e=0; e<count; ++e )
	{
		SpotEncounter *encounter = new SpotEncounter;

		filesystem->Read( &encounter->from.id, sizeof(unsigned int), file );

		unsigned char dir;
		filesystem->Read( &dir, sizeof(unsigned char), file );
		encounter->fromDir = static_cast<NavDirType>( dir );

		filesystem->Read( &encounter->to.id, sizeof(unsigned int), file );

		filesystem->Read( &dir, sizeof(unsigned char), file );
		encounter->toDir = static_cast<NavDirType>( dir );

		// read list of spots along this path
		unsigned char spotCount;
		filesystem->Read( &spotCount, sizeof(unsigned char), file );
	
		SpotOrder order;
		for( int s=0; s<spotCount; ++s )
		{
			filesystem->Read( &order.id, sizeof(unsigned int), file );

			unsigned char t;
			filesystem->Read( &t, sizeof(unsigned char), file );

			order.t = (float)t/255.0f;

			encounter->spotList.AddToTail( order );
		}

		m_spotEncounterList.AddToTail( encounter );
	}

	if (version < 5)
		return;

	//
	// Load Place data
	//
	PlaceDirectory::IndexType entry;
	filesystem->Read( &entry, sizeof(entry), file );

	// convert entry to actual Place
	SetPlace( placeDirectory.IndexToPlace( entry ) );

	if ( version < 7 )
		return;

	// load ladder data
	for ( int dir=0; dir<CSNavLadder::NUM_LADDER_DIRECTIONS; ++dir )
	{
		filesystem->Read( &count, sizeof(unsigned int), file );
		{
			for( unsigned int i=0; i<count; ++i )
			{
				NavLadderConnect connect;
				filesystem->Read( &connect.id, sizeof(unsigned int), file );

				bool alreadyConnected = false;
				FOR_EACH_LL( m_ladder[dir], j )
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
	}

	if ( version < 8 )
		return;

	// load earliest occupy times
	for( int i=0; i<MAX_NAV_TEAMS; ++i )
	{
		// no spot in the map should take longer than this to reach
		filesystem->Read( &m_earliestOccupyTime[i], sizeof(m_earliestOccupyTime[i]), file );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Convert loaded IDs to pointers
 * Make sure all IDs are converted, even if corrupt data is encountered.
 */
NavErrorType CNavArea::PostLoad( void )
{
	NavErrorType error = NAV_OK;

	for ( int dir=0; dir < CNavLadder::NUM_LADDER_DIRECTIONS; ++dir )
	{
		FOR_EACH_VEC( m_ladder[dir], it )
		{
			NavLadderConnect& connect = m_ladder[dir][it];

			unsigned int id = connect.id;

			if ( TheNavMesh->GetLadders().Find( connect.ladder ) == TheNavMesh->GetLadders().InvalidIndex() )
			{
				connect.ladder = TheNavMesh->GetLadderByID( id );
			}

			if (id && connect.ladder == NULL)
			{
				Msg( "CNavArea::PostLoad: Corrupt navigation ladder data. Cannot connect Navigation Areas.\n" );
				error = NAV_CORRUPT_DATA;
			}
		}
	}

	// connect areas together
	for( int d=0; d<NUM_DIRECTIONS; d++ )
	{
		FOR_EACH_VEC( m_connect[d], it )
		{
			NavConnect *connect = &m_connect[ d ][ it ];

			unsigned int id = connect->id;
			connect->area = TheNavMesh->GetNavAreaByID( id );
			if (id && connect->area == NULL)
			{
				Msg( "CNavArea::PostLoad: Corrupt navigation data. Cannot connect Navigation Areas.\n" );
				error = NAV_CORRUPT_DATA;
			}
		}
	}

	// resolve approach area IDs
	for( int a=0; a<m_approachCount; ++a )
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

	// resolve spot encounter IDs
	SpotEncounter *e;
	FOR_EACH_VEC( m_spotEncounters, it )
	{
		e = m_spotEncounters[ it ];

		e->from.area = TheNavMesh->GetNavAreaByID( e->from.id );
		if (e->from.area == NULL)
		{
			Msg( "CNavArea::PostLoad: Corrupt navigation data. Missing \"from\" Navigation Area for Encounter Spot.\n" );
			error = NAV_CORRUPT_DATA;
		}

		e->to.area = TheNavMesh->GetNavAreaByID( e->to.id );
		if (e->to.area == NULL)
		{
			Msg( "CNavArea::PostLoad: Corrupt navigation data. Missing \"to\" Navigation Area for Encounter Spot.\n" );
			error = NAV_CORRUPT_DATA;
		}

		if (e->from.area && e->to.area)
		{
			// compute path
			float halfWidth;
			ComputePortal( e->to.area, e->toDir, &e->path.to, &halfWidth );
			ComputePortal( e->from.area, e->fromDir, &e->path.from, &halfWidth );

			const float eyeHeight = HalfHumanHeight;
			e->path.from.z = e->from.area->GetZ( e->path.from ) + eyeHeight;
			e->path.to.z = e->to.area->GetZ( e->path.to ) + eyeHeight;
		}

		// resolve HidingSpot IDs
		FOR_EACH_VEC( e->spots, sit )
		{
			SpotOrder *order = &e->spots[ sit ];

			order->spot = GetHidingSpotByID( order->id );
			if (order->spot == NULL)
			{
				Msg( "CNavArea::PostLoad: Corrupt navigation data. Missing Hiding Spot\n" );
				error = NAV_CORRUPT_DATA;
			}
		}
	}

	// build overlap list
	/// @todo Optimize this
	FOR_EACH_VEC( TheNavAreas, nit )
	{
		CNavArea *area = TheNavAreas[ nit ];

		if (area == this)
			continue;

		if (IsOverlapping( area ))
			m_overlappingAreas.AddToTail( area );
	}

	return error;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Determine the earliest time this hiding spot can be reached by either team
 */
void CNavArea::ComputeEarliestOccupyTimes( void )
{
#ifdef CSTRIKE_DLL
	/// @todo Derive cstrike-specific navigation classes

	for( int i=0; i<MAX_NAV_TEAMS; ++i )
	{
		// no spot in the map should take longer than this to reach
		m_earliestOccupyTime[i] = 120.0f;
	}

	if (nav_quicksave.GetBool())
		return;

	// maximum player speed in units/second
	const float playerSpeed = 240.0f;

	ShortestPathCost cost;
	CBaseEntity *spot;

	// determine the shortest time it will take a Terrorist to reach this area
	int team = TEAM_TERRORIST % MAX_NAV_TEAMS;
	for( spot = gEntList.FindEntityByClassname( NULL, "info_player_terrorist" );
		 spot;
		 spot = gEntList.FindEntityByClassname( spot, "info_player_terrorist" ) )
	{
		float travelDistance = NavAreaTravelDistance( spot->GetAbsOrigin(), m_center, cost );
		if (travelDistance < 0.0f)
			continue;

		float travelTime = travelDistance / playerSpeed;
		if (travelTime < m_earliestOccupyTime[ team ])
		{
			m_earliestOccupyTime[ team ] = travelTime;
		}
	}


	// determine the shortest time it will take a CT to reach this area
	team = TEAM_CT % MAX_NAV_TEAMS;
	for( spot = gEntList.FindEntityByClassname( NULL, "info_player_counterterrorist" );
		 spot;
		 spot = gEntList.FindEntityByClassname( spot, "info_player_counterterrorist" ) )
	{
		float travelDistance = NavAreaTravelDistance( spot->GetAbsOrigin(), m_center, cost );
		if (travelDistance < 0.0f)
			continue;

		float travelTime = travelDistance / playerSpeed;
		if (travelTime < m_earliestOccupyTime[ team ])
		{
			m_earliestOccupyTime[ team ] = travelTime;
		}
	}

#else
	for( int i=0; i<MAX_NAV_TEAMS; ++i )
	{
		m_earliestOccupyTime[i] = 0.0f;
	}
#endif
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Determine if this area is a "battlefront" area - where two rushing teams first meet.
 */
void CNavMesh::ComputeBattlefrontAreas( void )
{
#if 0
#ifdef CSTRIKE_DLL
	ShortestPathCost cost;
	CBaseEntity *tSpawn, *ctSpawn;

	for( tSpawn = gEntList.FindEntityByClassname( NULL, "info_player_terrorist" );
		 tSpawn;
		 tSpawn = gEntList.FindEntityByClassname( tSpawn, "info_player_terrorist" ) )
	{
		CNavArea *tArea = TheNavMesh->GetNavArea( tSpawn->GetAbsOrigin() );
		if (tArea == NULL)
			continue;

		for( ctSpawn = gEntList.FindEntityByClassname( NULL, "info_player_counterterrorist" );
			 ctSpawn;
			 ctSpawn = gEntList.FindEntityByClassname( ctSpawn, "info_player_counterterrorist" ) )
		{
			CNavArea *ctArea = TheNavMesh->GetNavArea( ctSpawn->GetAbsOrigin() );

			if (ctArea == NULL)
				continue;

			if (tArea == ctArea)
			{
				m_isBattlefront = true;
				return;
			}

			// build path between these two spawn points - assume if path fails, it at least got close
			// (ie: imagine spawn points that you jump down from - can't path to)
			CNavArea *goalArea = NULL;
			NavAreaBuildPath( tArea, ctArea, NULL, cost, &goalArea );

			if (goalArea == NULL)
				continue;


/**
 * @todo Need to enumerate ALL paths between all pairs of spawn points to find all battlefront areas
 */

			// find the area with the earliest overlapping occupy times
			CNavArea *battlefront = NULL;
			float earliestTime = 999999.9f;

			const float epsilon = 1.0f;
			CNavArea *area;
			for( area = goalArea; area; area = area->GetParent() )
			{
				if (fabs(area->GetEarliestOccupyTime( TEAM_TERRORIST ) - area->GetEarliestOccupyTime( TEAM_CT )) < epsilon)
				{
				}
				
			}
		}
	}
#endif
#endif
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return the filename for this map's "nav map" file
 */
const char *CNavMesh::GetFilename( void ) const
{
	// filename is local to game dir for Steam, so we need to prepend game dir for regular file save
	char gamePath[256];
	engine->GetGameDir( gamePath, 256 );

	static char filename[256];
	Q_snprintf( filename, sizeof( filename ), "%s\\maps\\%s.nav", gamePath, STRING( gpGlobals->mapname ) );

	return filename;
}

//--------------------------------------------------------------------------------------------------------------
/*
============
COM_FixSlashes

Changes all '/' characters into '\' characters, in place.
============
*/
inline void COM_FixSlashes( char *pname )
{
#ifdef _WIN32
	while ( *pname ) 
	{
		if ( *pname == '/' )
			*pname = '\\';
		pname++;
	}
#else
	while ( *pname ) 
	{
		if ( *pname == '\\' )
			*pname = '/';
		pname++;
	}
#endif
}

static void WarnIfMeshNeedsAnalysis( void )
{
	// Quick check to warn about needing to analyze: nav_strip, nav_delete, etc set
	// every CNavArea's m_approachCount to 0, and delete their m_spotEncounterList.
	// So, if no area has either, odds are good we need an analyze.
	{
		bool hasApproachAreas = false;
		bool hasSpotEncounters = false;

		FOR_EACH_VEC( TheNavAreas, it )
		{
			CNavArea *area = TheNavAreas[ it ];
			if ( area->GetApproachInfoCount() )
			{
				hasApproachAreas = true;
			}

			if ( area->GetSpotEncounterCount() )
			{
				hasSpotEncounters = true;
			}
		}

		if ( !hasApproachAreas || !hasSpotEncounters )
		{
			Warning( "The nav mesh needs a full nav_analyze\n" );
		}
	}
}

/**
 * Store Navigation Mesh to a file
 */
bool CNavMesh::Save( void ) const
{
	WarnIfMeshNeedsAnalysis();

	const char *filename = GetFilename();
	if (filename == NULL)
		return false;

	//
	// Store the NAV file
	//
	COM_FixSlashes( const_cast<char *>(filename) );

	// get size of source bsp file for later (before we open the nav file for writing, in
	// case of failure)
	char *bspFilename = GetBspFilename( filename );
	if (bspFilename == NULL)
	{
		return false;
	}

	FileHandle_t file = filesystem->Open( filename, "wb" );

	if (!file)
	{
		return false;
	}

	// store "magic number" to help identify this kind of file
	unsigned int magic = NAV_MAGIC_NUMBER;
	filesystem->Write( &magic, sizeof(unsigned int), file );

	// store version number of file
	// 1 = hiding spots as plain vector array
	// 2 = hiding spots as HidingSpot objects
	// 3 = Encounter spots use HidingSpot ID's instead of storing vector again
	// 4 = Includes size of source bsp file to verify nav data correlation
	// ---- Beta Release at V4 -----
	// 5 = Added Place info
	// ---- Conversion to Src ------
	// 6 = Added Ladder info
	// 7 = Areas store ladder ID's so ladders can have one-way connections
	// 8 = Added earliest occupy times (2 floats) to each area
	// 9 = Promoted CNavArea's attribute flags to a short
	unsigned int version = NavCurrentVersion;
	filesystem->Write( &version, sizeof(unsigned int), file );

	// store the size of source bsp file in the nav file
	// so we can test if the bsp changed since the nav file was made
	unsigned int bspSize = filesystem->Size( bspFilename );
	DevMsg( "Size of bsp file '%s' is %u bytes.\n", bspFilename, bspSize );

	filesystem->Write( &bspSize, sizeof(unsigned int), file );


	//
	// Build a directory of the Places in this map
	//
	placeDirectory.Reset();

	FOR_EACH_VEC( TheNavAreas, nit )
	{
		CNavArea *area = TheNavAreas[ nit ];

		Place place = area->GetPlace();

		if (place)
		{
			placeDirectory.AddPlace( place );
		}
	}

	placeDirectory.Save( file );


	//
	// Store navigation areas
	//
	{
		// store number of areas
		unsigned int count = TheNavAreas.Count();
		filesystem->Write( &count, sizeof(unsigned int), file );

		// store each area
		FOR_EACH_VEC( TheNavAreas, it )
		{
			CNavArea *area = TheNavAreas[ it ];

			area->Save( file, version );
		}
	}

	//
	// Store ladders
	//
	{
		// store number of ladders
		unsigned int count = m_ladders.Count();
		filesystem->Write( &count, sizeof(unsigned int), file );

		// store each ladder
		FOR_EACH_VEC( m_ladders, it )
		{
			CNavLadder *ladder = m_ladders[ it ];

			ladder->Save( file, version );
		}
	}

	filesystem->Flush( file );
	filesystem->Close( file );

	unsigned int navSize = filesystem->Size( filename );
	DevMsg( "Size of nav file '%s' is %u bytes.\n", filename, navSize );

	return true;
}


//--------------------------------------------------------------------------------------------------------------
static NavErrorType CheckNavFile( const char *bspFilename )
{
	if ( !bspFilename )
		return NAV_CANT_ACCESS_FILE;

	char bspPathname[256];
	char filename[256];
	Q_strncpy( bspPathname, "maps/", sizeof( bspPathname ) );
	Q_strncat( bspPathname, bspFilename, sizeof( bspPathname ), COPY_ALL_CHARACTERS );
	Q_strncpy( filename, bspPathname, sizeof( filename ) );
	Q_SetExtension( filename, ".nav", sizeof( filename ) );

	bool navIsInBsp = false;
	FileHandle_t file = filesystem->Open( filename, "rb", "MOD" );	// this ignores .nav files embedded in the .bsp ...
	if ( !file )
	{
		navIsInBsp = true;
		file = filesystem->Open( filename, "rb", "GAME" );	// ... and this looks for one if it's the only one around.
	}

	if (!file)
	{
		return NAV_CANT_ACCESS_FILE;
	}

	// check magic number
	int result;
	unsigned int magic;
	result = filesystem->Read( &magic, sizeof(unsigned int), file );
	if (!result || magic != NAV_MAGIC_NUMBER)
	{
		filesystem->Close( file );
		return NAV_INVALID_FILE;
	}

	// read file version number
	unsigned int version;
	result = filesystem->Read( &version, sizeof(unsigned int), file );
	if (!result || version > NavCurrentVersion || version < 4)
	{
		filesystem->Close( file );
		return NAV_BAD_FILE_VERSION;
	}

	// get size of source bsp file and verify that the bsp hasn't changed
	unsigned int saveBspSize;
	filesystem->Read( &saveBspSize, sizeof(unsigned int), file );

	// verify size
	unsigned int bspSize = filesystem->Size( bspPathname );

	if (bspSize != saveBspSize && !navIsInBsp)
	{
		return NAV_FILE_OUT_OF_DATE;
	}

	return NAV_OK;
}


//--------------------------------------------------------------------------------------------------------------
void CommandNavCheckFileConsistency( void )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	FileFindHandle_t findHandle;
	const char *bspFilename = filesystem->FindFirstEx( "maps/*.bsp", "MOD", &findHandle );
	while ( bspFilename )
	{
		switch ( CheckNavFile( bspFilename ) )
		{
		case NAV_CANT_ACCESS_FILE:
			Warning( "Missing nav file for %s\n", bspFilename );
			break;
		case NAV_INVALID_FILE:
			Warning( "Invalid nav file for %s\n", bspFilename );
			break;
		case NAV_BAD_FILE_VERSION:
			Warning( "Old nav file for %s\n", bspFilename );
			break;
		case NAV_FILE_OUT_OF_DATE:
			Warning( "The nav file for %s is built from an old version of the map\n", bspFilename );
			break;
		case NAV_OK:
			Msg( "The nav file for %s is up-to-date\n", bspFilename );
			break;
		}

		bspFilename = filesystem->FindNext( findHandle );
	}
	filesystem->FindClose( findHandle );
}
static ConCommand nav_check_file_consistency( "nav_check_file_consistency", CommandNavCheckFileConsistency, "Scans the maps directory and reports any missing/out-of-date navigation files.", FCVAR_GAMEDLL | FCVAR_CHEAT );


//--------------------------------------------------------------------------------------------------------------
/**
 * Load AI navigation data from a file
 */
NavErrorType CNavMesh::Load( void )
{
	// free previous navigation mesh data
	Reset();
	placeDirectory.Reset();

	CNavArea::m_nextID = 1;

	// nav filename is derived from map filename
	char filename[256];
	Q_snprintf( filename, sizeof( filename ), "maps\\%s.nav", STRING( gpGlobals->mapname ) );

	bool navIsInBsp = false;
	FileHandle_t file = filesystem->Open( filename, "rb", "MOD" );	// this ignores .nav files embedded in the .bsp ...
	if ( !file )
	{
		navIsInBsp = true;
		file = filesystem->Open( filename, "rb", "GAME" );	// ... and this looks for one if it's the only one around.
	}

	if (!file)
	{
		return NAV_CANT_ACCESS_FILE;
	}

	// check magic number
	int result;
	unsigned int magic;
	result = filesystem->Read( &magic, sizeof(unsigned int), file );
	if (!result || magic != NAV_MAGIC_NUMBER)
	{
		Msg( "Invalid navigation file '%s'.\n", filename );
		filesystem->Close( file );
		return NAV_INVALID_FILE;
	}

	// read file version number
	unsigned int version;
	result = filesystem->Read( &version, sizeof(unsigned int), file );
	if (!result || version > NavCurrentVersion)
	{
		Msg( "Unknown navigation file version.\n" );
		filesystem->Close( file );
		return NAV_BAD_FILE_VERSION;
	}

	if (version >= 4)
	{
		// get size of source bsp file and verify that the bsp hasn't changed
		unsigned int saveBspSize;
		filesystem->Read( &saveBspSize, sizeof(unsigned int), file );

		// verify size
		char *bspFilename = GetBspFilename( filename );
		if ( bspFilename == NULL )
		{
			filesystem->Close( file );
			return NAV_INVALID_FILE;
		}

		unsigned int bspSize = filesystem->Size( bspFilename );

		if (bspSize != saveBspSize && !navIsInBsp)
		{
			if ( engine->IsDedicatedServer() )
			{
				// Warning doesn't print to the dedicated server console, so we'll use Msg instead
				Msg( "The Navigation Mesh was built using a different version of this map.\n" );
			}
			else
			{
				Warning( "The Navigation Mesh was built using a different version of this map.\n" );
			}
			m_isFromCurrentMap = false;
		}
	}

	// load Place directory
	if (version >= 5)
	{
		placeDirectory.Load( file );
	}

	// get number of areas
	unsigned int count;
	unsigned int i;
	result = filesystem->Read( &count, sizeof(unsigned int), file );

	Extent extent;
	extent.lo.x = 9999999999.9f;
	extent.lo.y = 9999999999.9f;
	extent.hi.x = -9999999999.9f;
	extent.hi.y = -9999999999.9f;

	// load the areas and compute total extent
	for( i=0; i<count; ++i )
	{
		CNavArea *area = new CNavArea;
		area->Load( file, version );
		TheNavAreas.AddToTail( area );

		Extent areaExtent;
		area->GetExtent(&areaExtent);

		// check validity of nav area
		if (areaExtent.lo.x >= areaExtent.hi.x || areaExtent.lo.y >= areaExtent.hi.y)
			Warning( "WARNING: Degenerate Navigation Area #%d at ( %g, %g, %g )\n", 
						area->GetID(), area->m_center.x, area->m_center.y, area->m_center.z );

		if (areaExtent.lo.x < extent.lo.x)
			extent.lo.x = areaExtent.lo.x;
		if (areaExtent.lo.y < extent.lo.y)
			extent.lo.y = areaExtent.lo.y;
		if (areaExtent.hi.x > extent.hi.x)
			extent.hi.x = areaExtent.hi.x;
		if (areaExtent.hi.y > extent.hi.y)
			extent.hi.y = areaExtent.hi.y;
	}

	// add the areas to the grid
	AllocateGrid( extent.lo.x, extent.hi.x, extent.lo.y, extent.hi.y );

	FOR_EACH_VEC( TheNavAreas, it )
	{
		AddNavArea( TheNavAreas[ it ] );
	}


	//
	// Set up all the ladders
	//
	if (version >= 6)
	{
		result = filesystem->Read( &count, sizeof(unsigned int), file );

		// load the ladders
		for( i=0; i<count; ++i )
		{
			CNavLadder *ladder = new CNavLadder;
			ladder->Load( file, version );
			m_ladders.AddToTail( ladder );
		}
	}
	else
	{
		BuildLadders();
	}

	// allow areas to connect to each other, etc
	FOR_EACH_VEC( TheNavAreas, pit )
	{
		CNavArea *area = TheNavAreas[ pit ];
		area->PostLoad();
	}

	// allow hiding spots to compute information
	FOR_EACH_VEC( TheHidingSpots, hit )
	{
		HidingSpot *spot = TheHidingSpots[ hit ];
		spot->PostLoad();
	}

	if ( version < 8 )
	{
		// Old nav meshes need to compute earliest occupy times
		FOR_EACH_VEC( TheNavAreas, nit )
		{
			CNavArea *area = TheNavAreas[ nit ];
			area->ComputeEarliestOccupyTimes();
		}
	}

	ComputeBattlefrontAreas();

	// the Navigation Mesh has been successfully loaded
	m_isLoaded = true;

	filesystem->Close( file );

	WarnIfMeshNeedsAnalysis();

	return NAV_OK;
}
#endif
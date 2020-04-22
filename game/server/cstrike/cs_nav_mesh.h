//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// cs_nav_mesh.h
// The Navigation Mesh interface
// Author: Michael S. Booth (mike@turtlerockstudios.com), January 2003

//
// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003
//
// NOTE: The Navigation code uses Doxygen-style comments. If you run Doxygen over this code, it will 
// auto-generate documentation.  Visit www.doxygen.org to download the system for free.
//

#ifndef _CS_NAV_MESH_H_
#define _CS_NAV_MESH_H_

#include "filesystem.h"

#include "nav_mesh.h"

#include "cs_nav.h"
#include "nav_area.h"
#include "nav_colors.h"

class CNavArea;
class CCSNavArea;
class CBaseEntity; 

//--------------------------------------------------------------------------------------------------------
/**
 * The CSNavMesh is the global interface to the Navigation Mesh.
 * @todo Make this an abstract base class interface, and derive mod-specific implementations.
 */
class CSNavMesh : public CNavMesh
{
public:
	CSNavMesh( void );
	virtual ~CSNavMesh();

	virtual CNavArea *CreateArea( void ) const;							// CNavArea factory

	virtual unsigned int GetSubVersionNumber( void ) const;									// returns sub-version number of data format used by derived classes
	virtual void SaveCustomData( CUtlBuffer &fileBuffer ) const;							// store custom mesh data for derived classes
	virtual void LoadCustomData( CUtlBuffer &fileBuffer, unsigned int subVersion );			// load custom mesh data for derived classes

	virtual void Reset( void );											///< destroy Navigation Mesh data and revert to initial state
	virtual void Update( void );										///< invoked on each game frame

	virtual NavErrorType Load( void );									// load navigation data from a file
	virtual NavErrorType PostLoad( unsigned int version );				// (EXTEND) invoked after all areas have been loaded - for pointer binding, etc
	virtual bool Save( void ) const;									///< store Navigation Mesh to a file

	void ClearPlayerCounts( void );										///< zero player counts in all areas

protected:
	virtual void BeginCustomAnalysis( bool bIncremental );
	virtual void PostCustomAnalysis( void );							// invoked when custom analysis step is complete
	virtual void EndCustomAnalysis();

private:
};

#endif // _CS_NAV_MESH_H_

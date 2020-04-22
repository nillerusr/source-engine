//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// NavMesh.cpp
// Implementation of Navigation Mesh interface
// Author: Michael S. Booth, 2003-2004

#include "cbase.h"
#include "filesystem.h"
#include "cs_nav_mesh.h"
#include "cs_nav_node.h"
#include "cs_nav_area.h"
#include "fmtstr.h"
#include "utlbuffer.h"
#include "tier0/vprof.h"

//--------------------------------------------------------------------------------------------------------------
CSNavMesh::CSNavMesh( void )
{
}

//--------------------------------------------------------------------------------------------------------------
CSNavMesh::~CSNavMesh()
{
}

CNavArea * CSNavMesh::CreateArea( void ) const
{
	return new CCSNavArea;
}

//-------------------------------------------------------------------------
void CSNavMesh::BeginCustomAnalysis( bool bIncremental )
{

}


//-------------------------------------------------------------------------
// invoked when custom analysis step is complete
void CSNavMesh::PostCustomAnalysis( void )
{

}


//-------------------------------------------------------------------------
void CSNavMesh::EndCustomAnalysis()
{

}


//-------------------------------------------------------------------------
/**
 * Returns sub-version number of data format used by derived classes
 */
unsigned int CSNavMesh::GetSubVersionNumber( void ) const
{
	// 1: initial implementation - added ApproachArea data
	return 1;
}

//-------------------------------------------------------------------------
/** 
 * Store custom mesh data for derived classes
 */
void CSNavMesh::SaveCustomData( CUtlBuffer &fileBuffer ) const
{

}

//-------------------------------------------------------------------------
/**
 * Load custom mesh data for derived classes
 */
void CSNavMesh::LoadCustomData( CUtlBuffer &fileBuffer, unsigned int subVersion )
{

}

//--------------------------------------------------------------------------------------------------------------
/**
 * Reset the Navigation Mesh to initial values
 */
void CSNavMesh::Reset( void )
{
	CNavMesh::Reset();
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Zero player counts in all areas
 */
void CSNavMesh::ClearPlayerCounts( void )
{
	FOR_EACH_VEC( TheNavAreas, it )
	{
		CCSNavArea *area = (CCSNavArea*)TheNavAreas[ it ];
		area->ClearPlayerCount();
	}
}

void CSNavMesh::Update( void )
{
	CNavMesh::Update();
}

NavErrorType CSNavMesh::Load( void )
{
	return CNavMesh::Load();
}

bool CSNavMesh::Save( void ) const
{
	return CNavMesh::Save();
}

NavErrorType CSNavMesh::PostLoad( unsigned int version )
{
	return CNavMesh::PostLoad(version);
}
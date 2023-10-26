//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose : Singleton manager for color correction on the client
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"

#include "spatialentitymgr.h"
#include "c_spatialentity.h"


// Gets called each frame
void CSpatialEntityMgr::Update( float frametime )
{
	if ( m_SpatialEntities.Count() <= 0 )
		return;

	m_SpatialEntities[ 0 ]->ResetAccumulation();

	for ( int i = 0; i < m_SpatialEntities.Count(); ++i )
	{
		m_SpatialEntities[ i ]->Accumulate();
	}

	m_SpatialEntities[ 0 ]->ApplyAccumulation();
}

//------------------------------------------------------------------------------
// Creates, destroys spatial entities
//------------------------------------------------------------------------------
void CSpatialEntityMgr::AddSpatialEntity( C_SpatialEntity *pSpatialEntity )
{
	m_SpatialEntities.AddToTail( pSpatialEntity );
}

void CSpatialEntityMgr::RemoveSpatialEntity( C_SpatialEntity *pSpatialEntity )
{
	m_SpatialEntities.FindAndRemove( pSpatialEntity );
}

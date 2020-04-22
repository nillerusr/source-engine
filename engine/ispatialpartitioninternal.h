//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

#ifndef ISPATIALPARTITIONINTERNAL_H
#define ISPATIALPARTITIONINTERNAL_H

#include "ispatialpartition.h"


//-----------------------------------------------------------------------------
// These methods of the spatial partition manager are only used in the engine
//-----------------------------------------------------------------------------
abstract_class ISpatialPartitionInternal : public ISpatialPartition
{
public:
	// Call this to clear out the spatial partition and to re-initialize
	// it given a particular world size
	virtual void Init( const Vector& worldmin, const Vector& worldmax ) = 0;

	virtual void DrawDebugOverlays() = 0;
};


//-----------------------------------------------------------------------------
// Method to get at the singleton implementation of the spatial partition mgr
//-----------------------------------------------------------------------------
ISpatialPartitionInternal* SpatialPartition();

//-----------------------------------------------------------------------------
// Create/destroy a custom spatial partition
//-----------------------------------------------------------------------------
ISpatialPartition *CreateSpatialPartition( const Vector& worldmin, const Vector& worldmax );
void DestroySpatialPartition( ISpatialPartition * );


//-----------------------------------------------------------------------------
// Method to get at the singleton implementation of the spatial partition mgr
//-----------------------------------------------------------------------------
ISpatialPartitionInternal* SpatialPartitionOld();

//-----------------------------------------------------------------------------
// Create/destroy a custom spatial partition
//-----------------------------------------------------------------------------
ISpatialPartition *CreateSpatialPartitionOld( const Vector& worldmin, const Vector& worldmax );
void DestroySpatialPartitionOld( ISpatialPartition * );


#endif	// ISPATIALPARTITIONINTERNAL_H




//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Deals with singleton  
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#if !defined( DETAILOBJECTSYSTEM_H )
#define DETAILOBJECTSYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "IGameSystem.h"
#include "IClientEntityInternal.h"
#include "engine/IVModelRender.h"
#include "mathlib/vector.h"
#include "IVRenderView.h"

struct model_t;
struct WorldListLeafData_t;	
struct DistanceFadeInfo_t;


//-----------------------------------------------------------------------------
// Info used when building lists of detail objects to render
//-----------------------------------------------------------------------------
struct DetailRenderableInfo_t
{
	IClientRenderable *m_pRenderable;
	int m_nLeafIndex;
	RenderGroup_t m_nRenderGroup;
	RenderableInstance_t m_InstanceData;
};


typedef CUtlVectorFixedGrowable< DetailRenderableInfo_t, 2048 > DetailRenderableList_t;


//-----------------------------------------------------------------------------
// Responsible for managing detail objects
//-----------------------------------------------------------------------------
abstract_class IDetailObjectSystem : public IGameSystem
{
public:
	// How many detail models (as opposed to sprites) are there in the level?
	virtual int GetDetailModelCount() const = 0;

	// Gets a particular detail object
	virtual IClientRenderable* GetDetailModel( int idx ) = 0;

	// Computes the detail prop fade info
	virtual float ComputeDetailFadeInfo( DistanceFadeInfo_t *pInfo ) = 0;

	// Builds a list of renderable info for all detail objects to render
	virtual void BuildRenderingData( DetailRenderableList_t &list, const SetupRenderInfo_t &info, float flDetailDist, const DistanceFadeInfo_t &fadeInfo ) = 0;

	// Call this before rendering translucent detail objects
	virtual void BeginTranslucentDetailRendering( ) = 0;

	// Renders all translucent detail objects in a particular set of leaves
	virtual void RenderTranslucentDetailObjects( const DistanceFadeInfo_t &info, const Vector &viewOrigin, const Vector &viewForward, const Vector &viewRight, const Vector &viewUp, int nLeafCount, LeafIndex_t *pLeafList ) =0;

	// Renders all translucent detail objects in a particular leaf up to a particular point
	virtual void RenderTranslucentDetailObjectsInLeaf( const DistanceFadeInfo_t &info, const Vector &viewOrigin, const Vector &viewForward, const Vector &viewRight, const Vector &viewUp, int nLeaf, const Vector *pVecClosestPoint ) = 0;
};


//-----------------------------------------------------------------------------
// System for dealing with detail objects
//-----------------------------------------------------------------------------
extern IDetailObjectSystem *g_pDetailObjectSystem;
inline IDetailObjectSystem* DetailObjectSystem()
{
	return g_pDetailObjectSystem;
}


#endif // DETAILOBJECTSYSTEM_H


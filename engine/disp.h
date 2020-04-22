//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef DISPINFO_H
#define DISPINFO_H

#ifdef _WIN32
#pragma once
#endif

//=============================================================================

#include <assert.h>
#include "idispinfo.h"
#include "bspfile.h"
#include "mathlib/vmatrix.h"
#include "dispnode.h"
#include "builddisp.h"
#include "utlvector.h"
#include "disp_helpers.h"
#include "tier0/fasttimer.h"
#include "dlight.h"
#include "utllinkedlist.h"
#include "zone.h"

struct model_t;
class IMesh;
class CMeshBuilder;
struct ShadowInfo_t;

class CDecalNodeSetupCache;



class CDispInfo : public IDispInfo, public CDispUtilsHelper
{
// IDispInfo overrides.
public:
	virtual				~CDispInfo();

	virtual void		GetIntersectingSurfaces( GetIntersectingSurfaces_Struct *pStruct );
	virtual void		RenderWireframeInLightmapPage( int pageId );
	
	virtual void		GetBoundingBox( Vector& bbMin, Vector& bbMax );

	virtual void		SetParent( SurfaceHandle_t surfID );
	virtual SurfaceHandle_t GetParent(); // returns surfID

	// add the dlights on this surface to the lightmap buffer for updload
	virtual void		AddDynamicLights( dlight_t *pLights, unsigned int lightMask );
	// compute which dlights affect this surface
	virtual unsigned int ComputeDynamicLightMask( dlight_t *pLights );

	virtual DispDecalHandle_t	NotifyAddDecal( decal_t *pDecal, float flSize );
	virtual void				NotifyRemoveDecal( DispDecalHandle_t h );
	virtual DispShadowHandle_t	AddShadowDecal( ShadowHandle_t shadowHandle );
	virtual void				RemoveShadowDecal( DispShadowHandle_t handle );

	// Compute shadow fragments for a particular shadow
	virtual bool		ComputeShadowFragments( DispShadowHandle_t h, int& vertexCount, int& indexCount );

	virtual bool		GetTag();
	virtual void		SetTag();
	
public:

	//=========================================================================
	//
	// Construction/Decontruction
	//
	CDispInfo();

	// Used for indexing displacements.
	CDispInfo*	GetDispByIndex( int index )		{ return index == 0xFFFF ? 0 : &m_pDispArray->m_pDispInfos[index]; }
	
	// Get this displacement's index into the main array.
	int			GetDispIndex()					{ return this - m_pDispArray->m_pDispInfos; }


	//=========================================================================
	//
	// Flags
	//
	void		SetTouched( bool bTouched );
	bool		IsTouched( void );

	// Rendering.
	void		ClearLOD();
	
	void		DrawDispAxes();
	bool		Render( CGroupMesh *pGroup, bool bAllowDebugModes );
	
	// Add in the contribution of a dynamic light.
	void		AddSingleDynamicLight( dlight_t& dl );
	void		AddSingleDynamicLightBumped( dlight_t& dl );

	// Add in the contribution of a dynamic alpha light.
	void		AddSingleDynamicAlphaLight( dlight_t& dl );

	// Cast a ray against this surface
	bool		TestRay( Ray_t const& ray, float start, float end, float& dist, 
						Vector2D* lightmapUV, Vector2D* texureUV );

// CDispUtilsHelper implementation.
public:

	virtual const CPowerInfo*		GetPowerInfo() const;
	virtual CDispNeighbor*			GetEdgeNeighbor( int index );
	virtual CDispCornerNeighbors*	GetCornerNeighbors( int index );
	virtual CDispUtilsHelper*		GetDispUtilsByIndex( int index );


// Initialization functions.
public:

	// These are used to mess with indices.
	int			VertIndex( int x, int y ) const;
	int			VertIndex( CVertIndex const &vert ) const;
	CVertIndex	IndexToVert( int index ) const;

	// Helpers to test decal intersection bit on decals.
	void		SetNodeIntersectsDecal( CDispDecal *pDispDecal, CVertIndex const &nodeIndex );
	int			GetNodeIntersectsDecal( CDispDecal *pDispDecal, CVertIndex const &nodeIndex );


public:

	// Copy data from a ddispinfo_t.
	void		CopyMapDispData( const ddispinfo_t *pBuildDisp );

	// This is called from CreateStaticBuffers_All after the CCoreDispInfo is fully
	// initialized. It just copies the data that it needs.
	bool		CopyCoreDispData( 
		model_t *pWorld,
		const MaterialSystem_SortInfo_t *pSortInfos,
		const CCoreDispInfo *pInfo,
		bool bRestoring );

	// called by CopyCoreDispData, just copies the vert data
	void CopyCoreDispVertData( const CCoreDispInfo *pInfo, float bumpSTexCoordOffset );

	// Checks the SURFDRAW_BUMPLIGHT flag and returns NUM_BUMP_VECTS+1 if it's set
	// and 1 if not.
	int			NumLightMaps();

	// This calculates the vertex's position on the base surface.
	// (Same result as CCoreDisp::GetFlatVert).
	Vector		GetFlatVert( int iVertex );


// Rendering functions.
public:

	// Set m_BBoxMin and m_BBoxMax. Uses g_TempDispVerts and assumes m_LODs have been validated.
	void		UpdateBoundingBox();

	// Number of verts per side.
	int			GetSideLength() const;

	// Return the total number of vertices.
	int			NumVerts() const;

	// Figure out the vector's projection in the decal's space.
	void		DecalProjectVert( Vector const &vPos, CDispDecalBase *pDispDecal, ShadowInfo_t const* pInfo, Vector &out );

	void		CullDecals( 
		int iNodeBit,
		CDispDecal **decals, 
		int nDecals, 
		CDispDecal **childDecals, 
		int &nChildDecals );

	void		TesselateDisplacement();

	// Pass all the mesh data in to the material system.
	void		SpecifyDynamicMesh();
	void		SpecifyWalkableDynamicMesh( void );
	void		SpecifyBuildableDynamicMesh( void );

	// Clear all active verts except the corner verts.
	void		InitializeActiveVerts();
	
	// Returns a particular vertex
	CDispRenderVert* GetVertex( int i );

	// Methods to compute lightmap coordinates, texture coordinates,
	// and lightmap color based on displacement u,v
	void ComputeLightmapAndTextureCoordinate( RayDispOutput_t const& output, 
		Vector2D* luv, Vector2D* tuv );

	// This little beastie generate decal fragments
	void GenerateDecalFragments( CVertIndex const &nodeIndex, 
		int iNodeBitIndex, unsigned short decalHandle, CDispDecalBase *pDispDecal );

private:
	// Two functions for adding decals
	void TestAddDecalTri( int iIndexStart, unsigned short decalHandle, CDispDecal *pDispDecal );
	void TestAddDecalTri( int iIndexStart, unsigned short decalHandle, CDispShadowDecal *pDispDecal );

	// Allocates fragments...
	CDispDecalFragment* AllocateDispDecalFragment( DispDecalHandle_t h, int nVerts = 6);

	// Clears decal fragment lists
	void ClearDecalFragments( DispDecalHandle_t h );
	void ClearAllDecalFragments();

	// Allocates fragments...
	CDispShadowFragment* AllocateShadowDecalFragment( DispShadowHandle_t h, int nCount );

	// Clears shadow decal fragment lists
	void ClearShadowDecalFragments( DispShadowHandle_t h );
	void ClearAllShadowDecalFragments();

	// Used by GenerateDecalFragments
	void GenerateDecalFragments_R( CVertIndex const &nodeIndex, 
		int iNodeBitIndex, unsigned short decalHandle, CDispDecalBase *pDispDecal, int iLevel );

	// Used to create a bitfield to help cull decal tests
	void SetupDecalNodeIntersect( CVertIndex const &nodeIndex, int iNodeBitIndex,
		CDispDecalBase *pDispDecal,	ShadowInfo_t const* pInfo );

	// Used by SetupDecalNodeIntersect
	bool SetupDecalNodeIntersect_R( CVertIndex const &nodeIndex, int iNodeBitIndex, 
		CDispDecalBase *pDispDecal, ShadowInfo_t const* pInfo, int iLevel, CDecalNodeSetupCache* pCache );


// Vertex/index data access.
public:

	// bounding box
	Vector			m_BBoxMin;
	Vector			m_BBoxMax;

	int m_nIndices;		// The actual # of indices being used (it can be less than m_Indices.Count() if 
						// our LOD is reducing the triangle count).
	int				m_iIndexOffset;
	// Used to get material..
	CGroupMesh		*m_pMesh;
	int				m_iVertOffset;
	float			m_BumpSTexCoordOffset;

	// This can be used to access the vertex data in a platform-independent way.
	// XBOX will get them directly out of the static vertex buffer.
	// PC gets them out of CDispRenderVerts.
	CMeshReader m_MeshReader;

	// List of all indices in the displacement in the current tesselation.
	// These indices are straight into the static buffer (ie: they're not relative
	// to m_iVertOffset).
	CUtlVector<unsigned short>	m_Indices;

	CUtlVector<CDispRenderVert, CHunkMemory<CDispRenderVert> > m_Verts;	// vectors that define the surface (size is NumVerts()).

public:

	// These bits tell which vertices and nodes are currently active.
	// These start out the same as m_ErrorVerts but have verts added if 
	// a neighbor needs to activate some verts.
	CBitVec<CCoreDispInfo::MAX_VERT_COUNT>	m_ActiveVerts;

	// These are set to 1 if the vert is allowed to become active.
	// This is what takes care of different-sized neighbors.
	CBitVec<CCoreDispInfo::MAX_VERT_COUNT>	m_AllowedVerts;

	int				m_idLMPage;						// lightmap page id

	SurfaceHandle_t	m_ParentSurfID;					// parent surfaceID
	int				m_iPointStart;					// starting point (orientation) on base face

	int				m_iLightmapAlphaStart;

	int             m_Contents;                     // surface contents

public:

	bool            m_bTouched;                     // touched flag

	int             m_fSurfProp;                    // surface properties flag - bump-mapping, etc.

	int				m_Power;			// surface size (sides are 2^n+1).

	unsigned short	*m_pTags;			// property tags

	// Position and texcoordinates at the four corner points
	Vector			m_BaseSurfacePositions[4];
	Vector2D		m_BaseSurfaceTexCoords[4];

	// Precalculated data for displacements of this size.
	const CPowerInfo	*m_pPowerInfo;	

	// Neighbor info for each side, indexed by NEIGHBOREDGE_ enums.
	// First 4 are edge neighbors, the rest are corners.
	CDispNeighbor			m_EdgeNeighbors[4];
	CDispCornerNeighbors	m_CornerNeighbors[4];

	// Copied from the ddispinfo. Speciifes where in g_DispLightmapSamplePositions the (compressed)
	// lightmap sample positions start.
	int				m_iLightmapSamplePositionStart;
	
	// The current triangulation for visualizing tag data.
	int				m_nWalkIndexCount;
	unsigned short  *m_pWalkIndices;

	int				m_nBuildIndexCount;
	unsigned short  *m_pBuildIndices;

	// This here's a bunch of per-node information
	DispNodeInfo_t	*m_pNodeInfo;

	// Where the viewer was when we last tesselated.
	// When the viewer moves out of the sphere, UpdateLOD is called.
	Vector			m_ViewerSphereCenter;

	// Used to make sure it doesn't activate verts in the wrong dispinfos.
	bool			m_bInUse;

	// Decals + Shadow decals
	DispDecalHandle_t	m_FirstDecal;
	DispShadowHandle_t	m_FirstShadowDecal;

	unsigned short	m_Index;	// helps in debugging
	
	// Current tag value.		
	unsigned short	m_Tag;
	CDispArray		*m_pDispArray;
};


extern int g_nDispTris;
extern CCycleCount g_DispRenderTime;
extern bool DispInfoRenderDebugModes();

// --------------------------------------------------------------------------------- //
// CDispInfo functions.
// --------------------------------------------------------------------------------- //

inline int CDispInfo::GetSideLength() const
{
	return m_pPowerInfo->m_SideLength;
}


inline int CDispInfo::NumVerts() const
{
	Assert( m_Verts.Count() == m_pPowerInfo->m_MaxVerts );
	return m_pPowerInfo->m_MaxVerts;
}

//-----------------------------------------------------------------------------
// Returns a particular vertex
//-----------------------------------------------------------------------------

inline CDispRenderVert* CDispInfo::GetVertex( int i )
{
	Assert( i < NumVerts() );
	return &m_Verts[i];
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CDispInfo::SetTouched( bool bTouched )
{
	m_bTouched = bTouched;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool CDispInfo::IsTouched( void )
{
	return m_bTouched; 
}


inline int CDispInfo::VertIndex( int x, int y ) const
{
	Assert( x >= 0 && x < GetSideLength() && y >= 0 && y < GetSideLength() );
	return y * GetSideLength() + x;
}


inline int CDispInfo::VertIndex( CVertIndex const &vert ) const
{
	Assert( vert.x >= 0 && vert.x < GetSideLength() && vert.y >= 0 && vert.y < GetSideLength() );
	return vert.y * GetSideLength() + vert.x;
}


void DispInfo_BatchDecals( CDispInfo **pVisibleDisps, int nVisibleDisps );
void DispInfo_DrawDecals( CDispInfo **pVisibleDisps, int nVisibleDisps );

#endif // DISPINFO_H

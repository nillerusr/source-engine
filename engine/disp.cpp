//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#include "render_pch.h"
#include "gl_cvars.h"
#include "gl_model_private.h"
#include "gl_lightmap.h"
#include "disp.h"
#include "mathlib/mathlib.h"
#include "gl_rsurf.h"
#include "gl_matsysiface.h"
#include "zone.h"
#include "materialsystem/imesh.h"
#include "iscratchpad3d.h"
#include "decal_private.h"
#include "con_nprint.h"
#include "dispcoll_common.h"
#include "cmodel_private.h"
#include "collisionutils.h"
#include "tier0/dbg.h"
#include "gl_rmain.h"
#include "lightcache.h"
#include "disp_tesselate.h"
#include "shadowmgr.h"
#include "debugoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Globals.
//-----------------------------------------------------------------------------
Vector modelorg;
ConVar r_DispDrawAxes( "r_DispDrawAxes", "0" );


//-----------------------------------------------------------------------------
// CEngineTesselateHelper implements the abstract parts of the tesselation code.
// We're only interested in the final triangles anyway, right??
//-----------------------------------------------------------------------------
class CEngineTesselateHelper : public CBaseTesselateHelper
{
public:
	void EndTriangle()
	{
		// Put all triangles in here.
		int iVertOffset = m_pDisp->m_iVertOffset;

		// Add this tri to our mesh.
		m_IndexMesh.Index( m_TempIndices[0] + iVertOffset );
		m_IndexMesh.AdvanceIndex();

		m_IndexMesh.Index( m_TempIndices[1] + iVertOffset );
		m_IndexMesh.AdvanceIndex();

		m_IndexMesh.Index( m_TempIndices[2] + iVertOffset );
		m_IndexMesh.AdvanceIndex();

		// Store off the indices...
		m_pDisp->m_Indices[m_nIndices]   = m_TempIndices[0] + iVertOffset;
		m_pDisp->m_Indices[m_nIndices+1] = m_TempIndices[1] + iVertOffset;
		m_pDisp->m_Indices[m_nIndices+2] = m_TempIndices[2] + iVertOffset;
		
		m_nIndices += 3;
	}

	DispNodeInfo_t& GetNodeInfo( int iNodeBit )
	{
		return m_pDisp->m_pNodeInfo[iNodeBit];
	}


public:

	// The mesh that we specify indices into while tesselating.
	CMeshBuilder m_IndexMesh;
	CDispInfo *m_pDisp;
};




//-----------------------------------------------------------------------------
// CDispInfo implementation.
//-----------------------------------------------------------------------------

inline CVertIndex CDispInfo::IndexToVert( int index ) const
{
	if( index == -1 )
		return CVertIndex( -1, -1 );
	else
		return CVertIndex( index % GetSideLength(), index / GetSideLength() );
}


void CDispInfo::UpdateBoundingBox()
{
	m_BBoxMin.Init( 1e24, 1e24, 1e24 );
	m_BBoxMax.Init( -1e24, -1e24, -1e24 );

	for( int i=0; i < NumVerts(); i++ )
	{
		const Vector &pos = m_MeshReader.Position( i );
		VectorMin( pos, m_BBoxMin, m_BBoxMin );
		VectorMax( pos, m_BBoxMax, m_BBoxMax );
	}
}


inline void CDispInfo::DecalProjectVert( Vector const &vPos, CDispDecalBase *pDecalBase, ShadowInfo_t const* pInfo, Vector &out )
{
	if (!pInfo)
	{
		CDispDecal* pDispDecal = static_cast<CDispDecal*>(pDecalBase);
		out.x = vPos.Dot( pDispDecal->m_TextureSpaceBasis[0] ) - pDispDecal->m_pDecal->dx + .5f;
		out.y = vPos.Dot( pDispDecal->m_TextureSpaceBasis[1] ) - pDispDecal->m_pDecal->dy + .5f;
		out.z = 0;
	}
	else
	{
		Vector3DMultiplyPosition( pInfo->m_WorldToShadow, vPos, out );
	}
}


// ----------------------------------------------------------------------------- //
// This version works for normal decals
// ----------------------------------------------------------------------------- //
void CDispInfo::TestAddDecalTri( int iIndexStart, unsigned short decalHandle, CDispDecal *pDispDecal )
{
	decal_t *pDecal = pDispDecal->m_pDecal;

	// If the decal is too far away from the plane of this triangle, reject it.
	unsigned short tempIndices[3] = 
	{
		(unsigned short)(m_MeshReader.Index( iIndexStart+0 ) - m_iVertOffset),
		(unsigned short)(m_MeshReader.Index( iIndexStart+1 ) - m_iVertOffset),
		(unsigned short)(m_MeshReader.Index( iIndexStart+2 ) - m_iVertOffset)
	};
	
	const Vector &v0 = m_MeshReader.Position( tempIndices[0] );
	const Vector &v1 = m_MeshReader.Position( tempIndices[1] );
	const Vector &v2 = m_MeshReader.Position( tempIndices[2] );
	
	Vector vNormal = (v2 - v0).Cross( v1 - v0 );
	VectorNormalize( vNormal );
	if ( vNormal.Dot( pDecal->position - v0 ) >= pDispDecal->m_flSize )
		return;

	// Setup verts.
	CDecalVert verts[3];
	int iVert;
	for( iVert=0; iVert < 3; iVert++ )
	{
		CDecalVert *pOutVert = &verts[iVert];
		
		pOutVert->m_vPos = m_MeshReader.Position( tempIndices[iVert] );

		{
			float x = pOutVert->m_cLMCoords.x;
			float y = pOutVert->m_cLMCoords.y;

			m_MeshReader.TexCoord2f( tempIndices[iVert], 1, x, y );

			pOutVert->m_cLMCoords.x = x;
			pOutVert->m_cLMCoords.y = y;
		}
		// garymcthack - what about m_ParentTexCoords?
		Vector tmp;
		DecalProjectVert( pOutVert->m_vPos, pDispDecal, 0, tmp );
		pOutVert->m_ctCoords.x = tmp.x;
		pOutVert->m_ctCoords.y = tmp.y;
	}

	// Clip them.
	CDecalVert *pClipped;
	CDecalVert *pOutVerts = NULL;
	pClipped = R_DoDecalSHClip( &verts[0], pOutVerts, pDecal, 3, vec3_origin );
	int outCount = pDecal->clippedVertCount;

	if ( outCount > 2 ) 
	{
		outCount = min( outCount, (int)CDispDecalFragment::MAX_VERTS );

		// Allocate a new fragment...
		CDispDecalFragment* pFragment = AllocateDispDecalFragment( decalHandle, outCount );

		// Alrighty, store the triangles!
		for( iVert=0; iVert < outCount; iVert++ )
		{
			pFragment->m_pVerts[iVert].m_vPos = pClipped[iVert].m_vPos;
			// garymcthack - need to make this work for displacements
			//				pFragment->m_tCoords[iVert] = pClipped[iVert].m_tCoords;
			// garymcthack - need to change m_TCoords to m_ParentTexCoords
			pFragment->m_pVerts[iVert].m_ctCoords = pClipped[iVert].m_ctCoords;
			pFragment->m_pVerts[iVert].m_cLMCoords = pClipped[iVert].m_cLMCoords;
		}
/*
		static int three = 0;
		static int total = 0;

		total++;
		if( outCount == 3 )
		{
			three++;
		}

		//if( )
		{
			char buffer[256];
			sprintf(buffer, "Verts: 3:%i 4+:%i (%i)\n",three, total, sizeof(CDecalVert));
			Msg(buffer);
		}
		*/
		pFragment->m_pDecal = pDecal;
		pFragment->m_nVerts = outCount;
		pDispDecal->m_nVerts += pFragment->m_nVerts;
		pDispDecal->m_nTris += pFragment->m_nVerts - 2;
	}
}


// ----------------------------------------------------------------------------- //
// This version works for shadow decals
// ----------------------------------------------------------------------------- //
void CDispInfo::TestAddDecalTri( int iIndexStart, unsigned short decalHandle, CDispShadowDecal *pDecal )
{
	unsigned short tempIndices[3] = 
	{
		(unsigned short)(m_MeshReader.Index( iIndexStart+0 ) - m_iVertOffset),
		(unsigned short)(m_MeshReader.Index( iIndexStart+1 ) - m_iVertOffset),
		(unsigned short)(m_MeshReader.Index( iIndexStart+2 ) - m_iVertOffset)
	};
#ifndef SWDS
	// Setup verts.
	Vector vPositions[3] ={
		GetOverlayPos( &m_MeshReader, tempIndices[0] ),
		GetOverlayPos( &m_MeshReader, tempIndices[1] ),
		GetOverlayPos( &m_MeshReader, tempIndices[2] )
	};
	Vector* ppPosition[3] = { &vPositions[0], &vPositions[1], &vPositions[2] };

	ShadowVertex_t** ppClipVertex;
	int count = g_pShadowMgr->ProjectAndClipVertices( pDecal->m_Shadow, 3, ppPosition, &ppClipVertex );
	if (count < 3)
		return;

	// Ok, clipping happened; lets create a decal fragment.
	Assert( count <= CDispShadowFragment::MAX_VERTS );

	// Allocate a new fragment...
	CDispShadowFragment* pFragment = AllocateShadowDecalFragment( decalHandle, count );

	// Copy the fragment data in place
	pFragment->m_nVerts = count;

	for (int i = 0; i < count; ++i )
	{
		VectorCopy( ppClipVertex[i]->m_Position, pFragment->m_ShadowVerts[i].m_Position );
		VectorCopy( ppClipVertex[i]->m_ShadowSpaceTexCoord, pFragment->m_ShadowVerts[i].m_ShadowSpaceTexCoord );

		// Make sure it's been clipped
		Assert( pFragment->m_ShadowVerts[i].m_ShadowSpaceTexCoord[0] >= -1e-3f );
		Assert( pFragment->m_ShadowVerts[i].m_ShadowSpaceTexCoord[0] - 1.0f <= 1e-3f );
		Assert( pFragment->m_ShadowVerts[i].m_ShadowSpaceTexCoord[1] >= -1e-3f );
		Assert( pFragment->m_ShadowVerts[i].m_ShadowSpaceTexCoord[1] - 1.0f <= 1e-3f );
	}

	// Update the number of triangles in the decal
	pDecal->m_nVerts += pFragment->m_nVerts;
	pDecal->m_nTris += pFragment->m_nVerts - 2;
	Assert( pDecal->m_nTris != 0 );
#endif
}


void CDispInfo::CullDecals( 
	int iNodeBit,
	CDispDecal **decals, 
	int nDecals, 
	CDispDecal **childDecals, 
	int &nChildDecals )
{
	// Only let the decals through that can affect this node or its children.
	nChildDecals = 0;
	for( int iDecal=0; iDecal < nDecals; iDecal++ )
	{
		if( decals[iDecal]->m_NodeIntersect.Get( iNodeBit ) )
		{
			childDecals[nChildDecals] = decals[iDecal];
			++nChildDecals;
		}
	}
}


//-----------------------------------------------------------------------------
// Retesselates a displacement
//-----------------------------------------------------------------------------
void CDispInfo::TesselateDisplacement()
{
	// Clear decals. They get regenerated in TesselateDisplacement_R.
	ClearAllDecalFragments();

	// Blow away cached shadow decals
	ClearAllShadowDecalFragments();

	int nMaxIndices = Square( GetSideLength() - 1 ) * 6;

	CEngineTesselateHelper helper;
	helper.m_pDisp = this;
	helper.m_IndexMesh.BeginModify( m_pMesh->m_pMesh, 0, 0, m_iIndexOffset, nMaxIndices );
	helper.m_pActiveVerts = m_ActiveVerts.Base();
	helper.m_pPowerInfo = GetPowerInfo();


	// Generate the indices.
	::TesselateDisplacement<CEngineTesselateHelper>( &helper ); // (implemented in disp_tesselate.h)


	helper.m_IndexMesh.EndModify();
	m_nIndices = helper.m_nIndices;
}


void CDispInfo::SpecifyDynamicMesh()
{
	CMatRenderContextPtr pRenderContext( materials );

	// Specify the vertices and indices.
	IMesh *pMesh = pRenderContext->GetDynamicMesh( true );
	CMeshBuilder builder;
	builder.Begin( pMesh, MATERIAL_TRIANGLES, NumVerts(), m_nIndices );

		// This should mirror how FillStaticBuffer works.
		int nVerts = NumVerts();
		for( int iVert=0; iVert < nVerts; iVert++ )
		{
			CDispRenderVert *pVert = &m_Verts[iVert];

			builder.Position3fv( pVert->m_vPos.Base() );

			builder.TexCoord2fv( 0, pVert->m_vTexCoord.Base() );
			builder.TexCoord2fv( 1, pVert->m_LMCoords.Base() );
			builder.TexCoord2f( 2, m_BumpSTexCoordOffset, 0 );
			
			builder.Normal3fv( pVert->m_vNormal.Base() );
			builder.TangentS3fv( pVert->m_vSVector.Base() );
			builder.TangentT3fv( pVert->m_vTVector.Base() );
			
			builder.AdvanceVertex();
		}

		for( int iIndex=0; iIndex < m_nIndices; iIndex++ )
		{
			builder.Index( m_Indices[iIndex] - m_iVertOffset );
			builder.AdvanceIndex();
		}

	builder.End( false, true );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispInfo::SpecifyWalkableDynamicMesh( void )
{
	// Specify the vertices and indices.
	CMatRenderContextPtr pRenderContext( materials );

#ifdef SWDS
	IMesh *pMesh = pRenderContext->GetDynamicMesh( false, NULL, NULL, NULL );
#else
	IMesh *pMesh = pRenderContext->GetDynamicMesh( false, NULL, NULL, g_materialTranslucentSingleColor );
	g_materialTranslucentSingleColor->ColorModulate( 1.0f, 1.0f, 0.0f );
	g_materialTranslucentSingleColor->AlphaModulate( 0.33f );
#endif
	CMeshBuilder builder;
	builder.Begin( pMesh, MATERIAL_TRIANGLES, NumVerts(), m_nWalkIndexCount );

		int nVerts = NumVerts();
		for( int iVert=0; iVert < nVerts; iVert++ )
		{
			builder.Position3fv( m_Verts[iVert].m_vPos.Base() );
			builder.AdvanceVertex();
		}
		
		for( int iIndex=0; iIndex < m_nWalkIndexCount; iIndex++ )
		{
			builder.Index( m_pWalkIndices[iIndex] );
			builder.AdvanceIndex();
		}

	builder.End( false, true );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDispInfo::SpecifyBuildableDynamicMesh( void )
{
	// Specify the vertices and indices.
	CMatRenderContextPtr pRenderContext( materials );

#ifdef SWDS
	IMesh *pMesh = pRenderContext->GetDynamicMesh( false, NULL, NULL, NULL );
#else
	g_materialTranslucentSingleColor->ColorModulate( 0.0f, 1.0f, 1.0f );
	g_materialTranslucentSingleColor->AlphaModulate( 0.33f );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( false, NULL, NULL, g_materialTranslucentSingleColor );
#endif
	CMeshBuilder builder;
	builder.Begin( pMesh, MATERIAL_TRIANGLES, NumVerts(), m_nBuildIndexCount );

		int nVerts = NumVerts();
		for( int iVert=0; iVert < nVerts; iVert++ )
		{
			builder.Position3fv( m_Verts[iVert].m_vPos.Base() );
			builder.AdvanceVertex();
		}

		for( int iIndex=0; iIndex < m_nBuildIndexCount; iIndex++ )
		{
			builder.Index( m_pBuildIndices[iIndex] );
			builder.AdvanceIndex();
		}

	builder.End( false, true );
}


void CDispInfo::InitializeActiveVerts()
{
	// Mark the corners vertices and root node by default..
	m_ActiveVerts.ClearAll();
	
	m_ActiveVerts.Set( VertIndex( 0, 0 ) );
	m_ActiveVerts.Set( VertIndex( GetSideLength()-1, 0 ) );
	m_ActiveVerts.Set( VertIndex( GetSideLength()-1, GetSideLength()-1 ) );
	m_ActiveVerts.Set( VertIndex( 0, GetSideLength()-1 ) );

	m_ActiveVerts.Set( VertIndex( m_pPowerInfo->m_RootNode ) );

	// Force the midpoint active on any edges where there are sub displacements.
	for( int iSide=0; iSide < 4; iSide++ )
	{
		CDispNeighbor *pSide = &m_EdgeNeighbors[iSide];

		if( (pSide->m_SubNeighbors[0].IsValid() && pSide->m_SubNeighbors[0].m_Span != CORNER_TO_CORNER) ||
			(pSide->m_SubNeighbors[1].IsValid() && pSide->m_SubNeighbors[1].m_Span != CORNER_TO_CORNER) )
		{
			int iEdgeDim = g_EdgeDims[iSide];
			
			CVertIndex nodeIndex;
			nodeIndex[iEdgeDim] = g_EdgeSideLenMul[iSide] * m_pPowerInfo->m_SideLengthM1;
			nodeIndex[!iEdgeDim] = m_pPowerInfo->m_MidPoint;
			m_ActiveVerts.Set( VertIndex( nodeIndex ) );
		}
	}
}


void CDispInfo::ClearLOD()
{
	// First, everything as inactive.
	m_ActiveVerts.ClearAll();
}

extern ConVar mat_surfaceid;
extern ConVar mat_surfacemat;

bool DispInfoRenderDebugModes()
{
	if( ShouldDrawInWireFrameMode() || mat_luxels.GetInt() || r_DispWalkable.GetInt() || 
		r_DispBuildable.GetInt() 
#if !defined( SWDS )
		|| mat_surfaceid.GetInt() || mat_surfacemat.GetInt()
#endif // SWDS
		)
		return true;

	return false;
}

bool CDispInfo::Render( CGroupMesh *pGroup, bool bAllowDebugModes )
{
#ifndef SWDS
	if( !m_pMesh )
	{
		Assert( !"CDispInfo::Render: m_pMesh == NULL" );
		return false;
	}

	// Trivial reject?
	if( R_CullBox(m_BBoxMin, m_BBoxMax, g_Frustum) )
		return false; 

	bool bNormalRender = true;
	if ( bAllowDebugModes )
	{
		CMatRenderContextPtr pRenderContext( materials );

		// Wireframe? 
		if( ShouldDrawInWireFrameMode() )
		{
			pRenderContext->Bind( g_materialWireframe );
			SpecifyDynamicMesh();
			bNormalRender = false;
		}
		
		if( mat_luxels.GetInt() )
		{
			pRenderContext->Bind( MSurf_TexInfo( m_ParentSurfID )->material );
			//SpecifyDynamicMesh();

			pGroup->m_pMesh->Draw( m_iIndexOffset, m_nIndices );

			pRenderContext->Bind( g_materialDebugLuxels );
			SpecifyDynamicMesh();
			bNormalRender = false;
		}

		if ( r_DispWalkable.GetInt() || r_DispBuildable.GetInt() )
		{
			pRenderContext->Bind( MSurf_TexInfo( m_ParentSurfID )->material );
			pGroup->m_pMesh->Draw( m_iIndexOffset, m_nIndices );

			if ( r_DispWalkable.GetInt() )
				SpecifyWalkableDynamicMesh();

			if ( r_DispBuildable.GetInt() )
				SpecifyBuildableDynamicMesh();

			bNormalRender = false;
		}

#if !defined( SWDS )
		if ( mat_surfaceid.GetInt() )
		{
			Vector bbMin, bbMax, vecCenter;
			GetBoundingBox( bbMin, bbMax );
			VectorAdd( bbMin, bbMax, vecCenter );
			vecCenter *= 0.5f;

			int nInt = ( mat_surfaceid.GetInt() != 2 ) ? (int)m_ParentSurfID : (msurface2_t*)m_ParentSurfID - host_state.worldbrush->surfaces2;
			char buf[32];
			Q_snprintf( buf, sizeof( buf ), "%d", nInt );
			CDebugOverlay::AddTextOverlay( vecCenter, 0, buf );
		}

		if ( mat_surfacemat.GetInt() )
		{
			Vector bbMin, bbMax, vecCenter;
			GetBoundingBox( bbMin, bbMax );
			VectorAdd( bbMin, bbMax, vecCenter );
			vecCenter *= 0.5f;

			mtexinfo_t * pTexInfo = MSurf_TexInfo(m_ParentSurfID);

			const char *pFullMaterialName = pTexInfo->material ? pTexInfo->material->GetName() : "no material";
			const char *pSlash = strrchr( pFullMaterialName, '/' );
			const char *pMaterialName = strrchr( pFullMaterialName, '\\' );
			if (pSlash > pMaterialName)
				pMaterialName = pSlash;
			if (pMaterialName)
				++pMaterialName;
			else
				pMaterialName = pFullMaterialName;

			CDebugOverlay::AddTextOverlay( vecCenter, 0, pMaterialName );
		}
#endif // SWDS
	}

	// Mark it visible.
	if( bNormalRender )
	{
		if( pGroup->m_nVisible < pGroup->m_Visible.Size() )
		{
			// Don't bother if all faces are backfacing, or somesuch...
			if (m_nIndices)
			{
				pGroup->m_Visible[pGroup->m_nVisible].m_FirstIndex = m_iIndexOffset;
				pGroup->m_Visible[pGroup->m_nVisible].m_NumIndices = m_nIndices;
				pGroup->m_VisibleDisps[pGroup->m_nVisible] = this;
				pGroup->m_nVisible++;
				pGroup->m_pGroup->m_nVisible++;
			}
		}
		else
		{
			Assert( !"Overflowed visible mesh list" );
		}
	}
#endif

	return true;
}


struct ProcessLightmapSampleData_t;

typedef void ProcessLightmapSampleFunc_t( const ProcessLightmapSampleData_t &data, const Vector &vPos, const Vector &vNormal, const Vector &vTangentS, const Vector &vTangentT, int t, int s, int tmax, int smax );

struct ProcessLightmapSampleData_t
{
	float		m_ooQuadraticAttn;
	float		m_ooRadiusSq;
	Vector		m_Intensity;
	float		m_LightDistSqr;
	Vector		m_vLightOrigin;
	ProcessLightmapSampleFunc_t *pProcessLightmapSampleDataFunc;
};

#ifndef DEDICATED
static void	ProcessLightmapSample( const ProcessLightmapSampleData_t &data, const Vector &vPos, const Vector &vNormal, const Vector &vTangentS, const Vector &vTangentT, int t, int s, int tmax, int smax )
{
	float distSqr = data.m_vLightOrigin.DistToSqr( vPos );
	if( distSqr < data.m_LightDistSqr )
	{
		float scale = (distSqr != 0.0f) ? data.m_ooQuadraticAttn / distSqr : 1.0f;

		// Apply a little extra attenuation
		scale *= (1.0f - distSqr * data.m_ooRadiusSq);

		if (scale > 2.0f)
			scale = 2.0f;

		int index = t*smax + s;
		VectorMA( blocklights[0][index].AsVector3D(), 
			scale, data.m_Intensity,
			blocklights[0][index].AsVector3D() );
	}
}

static void	ProcessLightmapSampleBumped( const ProcessLightmapSampleData_t &data, const Vector &vPos, const Vector &vNormal, const Vector &vTangentS, const Vector &vTangentT, int t, int s, int tmax, int smax )
{
	float distSqr = data.m_vLightOrigin.DistToSqr( vPos );
	if( distSqr < data.m_LightDistSqr )
	{
		float scale = (distSqr != 0.0f) ? data.m_ooQuadraticAttn / distSqr : 1.0f;
		
		// Get the vector from the surface to the light in world space
		Vector vLightVecWorld;
		VectorSubtract( data.m_vLightOrigin, vPos, vLightVecWorld );
		VectorNormalize( vLightVecWorld );

		// Transform the vector from the surface to the light into tangent space
		Vector vLightVecTangent;
		vLightVecTangent.x = DotProduct( vTangentS, vLightVecWorld );
		vLightVecTangent.y = DotProduct( vTangentT, vLightVecWorld );
		vLightVecTangent.z = DotProduct( vNormal, vLightVecWorld );

		// Apply a little extra attenuation
		scale *= (1.0f - distSqr * data.m_ooRadiusSq);

		if (scale > 2.0f)
			scale = 2.0f;

		int index = t*smax + s;
		float directionalAtten;
		directionalAtten = fpmax( 0.0f, vLightVecTangent.z );
		VectorMA( blocklights[0][index].AsVector3D(), scale * directionalAtten, 
			data.m_Intensity,
			blocklights[0][index].AsVector3D() );
		directionalAtten = fpmax( 0.0f, DotProduct( vLightVecTangent, g_localBumpBasis[0] ) );
		VectorMA( blocklights[1][index].AsVector3D(), scale * directionalAtten, 
			data.m_Intensity, 
			blocklights[1][index].AsVector3D() );
		directionalAtten = fpmax( 0.0f, DotProduct( vLightVecTangent, g_localBumpBasis[1] ) );
		VectorMA( blocklights[2][index].AsVector3D(), scale * directionalAtten,
			data.m_Intensity, 
			blocklights[2][index].AsVector3D() );
		directionalAtten = fpmax( 0.0f, DotProduct( vLightVecTangent, g_localBumpBasis[2] ) );
		VectorMA( blocklights[3][index].AsVector3D(), scale * directionalAtten,
			data.m_Intensity, 
			blocklights[3][index].AsVector3D() );
	}
}

//-----------------------------------------------------------------------------
// Alpha channel modulation
//-----------------------------------------------------------------------------
static void	ProcessLightmapSampleAlpha( const ProcessLightmapSampleData_t &data, const Vector &vPos, const Vector &vNormal, const Vector &vTangentS, const Vector &vTangentT, int t, int s, int tmax, int smax )
{
	float distSqr = data.m_vLightOrigin.DistToSqr( vPos );
	if( distSqr < data.m_LightDistSqr )
	{
		float scale = (distSqr != 0.0f) ? data.m_ooQuadraticAttn / distSqr : 1.0f;

		// Apply a little extra attenuation
		scale *= (1.0f - distSqr * data.m_ooRadiusSq);

		if (scale > 1.0f)
			scale = 1.0f;

		int index = t*smax + s;
		blocklights[0][index][3] += scale * data.m_Intensity[0];
	}
}
#endif

// This iterates over all the lightmap samples and for each one, calls:
// T::ProcessLightmapSample( Vector const &vPos, int t, int s, int tmax, int smax );
void IterateLightmapSamples( CDispInfo *pDisp, const ProcessLightmapSampleData_t &data )
{
	ASSERT_SURF_VALID( pDisp->m_ParentSurfID );

	int smax = MSurf_LightmapExtents( pDisp->m_ParentSurfID )[0] + 1;
	int tmax = MSurf_LightmapExtents( pDisp->m_ParentSurfID )[1] + 1;

	unsigned char *pCurSample = &g_DispLightmapSamplePositions[pDisp->m_iLightmapSamplePositionStart];

	for( int t = 0 ; t<tmax ; t++ )
	{
		for( int s=0 ; s<smax ; s++ )
		{
			// Figure out what triangle this sample is on.
			// NOTE: this usually stores 4 bytes per lightmap sample.
			// It's a lot simpler and faster to just store the position but then it's
			// 16 bytes instead of 4.
			int iTri;
			if( *pCurSample == 255 )
			{
				++pCurSample;
				iTri = *pCurSample + 255;
			}
			else
			{
				iTri = *pCurSample;
			}
			++pCurSample;

			float a = (float)*(pCurSample++) / 255.0f;
			float b = (float)*(pCurSample++) / 255.0f;
			float c = (float)*(pCurSample++) / 255.0f;

			CTriInfo *pTri = &pDisp->m_pPowerInfo->m_pTriInfos[iTri];
			Vector vPos = 
				pDisp->m_MeshReader.Position( pTri->m_Indices[0] ) * a +
				pDisp->m_MeshReader.Position( pTri->m_Indices[1] ) * b +
				pDisp->m_MeshReader.Position( pTri->m_Indices[2] ) * c;
			Vector vNormal, vTangentS, vTangentT;
			if( pDisp->NumLightMaps() > 1 )
			{
				vNormal = 
					pDisp->m_MeshReader.Normal( pTri->m_Indices[0] ) * a +
					pDisp->m_MeshReader.Normal( pTri->m_Indices[1] ) * b +
					pDisp->m_MeshReader.Normal( pTri->m_Indices[2] ) * c;
				vTangentS = 
					pDisp->m_MeshReader.TangentS( pTri->m_Indices[0] ) * a +
					pDisp->m_MeshReader.TangentS( pTri->m_Indices[1] ) * b +
					pDisp->m_MeshReader.TangentS( pTri->m_Indices[2] ) * c;
				vTangentT = 
					pDisp->m_MeshReader.TangentT( pTri->m_Indices[0] ) * a +
					pDisp->m_MeshReader.TangentT( pTri->m_Indices[1] ) * b +
					pDisp->m_MeshReader.TangentT( pTri->m_Indices[2] ) * c;
			}

			(*data.pProcessLightmapSampleDataFunc)( data, vPos, vNormal, vTangentS, vTangentT, t, s, tmax, smax );
		}
	}
}

void CDispInfo::AddSingleDynamicLight( dlight_t& dl )
{
#ifndef SWDS
	ProcessLightmapSampleData_t data;
	data.m_LightDistSqr = dl.GetRadiusSquared();

	float lightStyleValue = LightStyleValue( dl.style );
	data.m_Intensity[0] = TexLightToLinear( dl.color.r, dl.color.exponent ) * lightStyleValue;
	data.m_Intensity[1] = TexLightToLinear( dl.color.g, dl.color.exponent ) * lightStyleValue;
	data.m_Intensity[2] = TexLightToLinear( dl.color.b, dl.color.exponent ) * lightStyleValue;

	float minlight = fpmax( g_flMinLightingValue, dl.minlight );
	float ooQuadraticAttn = data.m_LightDistSqr * minlight; // / maxIntensity;

	data.m_ooQuadraticAttn = ooQuadraticAttn;
	data.m_vLightOrigin = dl.origin;
	data.m_ooRadiusSq = 1.0f / dl.GetRadiusSquared();;
	data.pProcessLightmapSampleDataFunc = &ProcessLightmapSample;

	// Touch all the lightmap samples.
	IterateLightmapSamples( this, data );
#endif
}

void CDispInfo::AddSingleDynamicLightBumped( dlight_t& dl )
{
#ifndef SWDS
	ProcessLightmapSampleData_t data;

	data.m_LightDistSqr = dl.GetRadiusSquared();

	float lightStyleValue = LightStyleValue( dl.style );
	data.m_Intensity[0] = TexLightToLinear( dl.color.r, dl.color.exponent ) * lightStyleValue;
	data.m_Intensity[1] = TexLightToLinear( dl.color.g, dl.color.exponent ) * lightStyleValue;
	data.m_Intensity[2] = TexLightToLinear( dl.color.b, dl.color.exponent ) * lightStyleValue;

	float minlight = fpmax( g_flMinLightingValue, dl.minlight );
	float ooQuadraticAttn = data.m_LightDistSqr * minlight; // / maxIntensity;

	data.m_ooQuadraticAttn = ooQuadraticAttn;
	data.m_vLightOrigin = dl.origin;
	data.m_ooRadiusSq = 1.0f / dl.GetRadiusSquared();
	data.pProcessLightmapSampleDataFunc = &ProcessLightmapSampleBumped;

	// Touch all the lightmap samples.
	IterateLightmapSamples( this, data );
#endif
}

void CDispInfo::AddSingleDynamicAlphaLight( dlight_t& dl )
{
#ifndef SWDS
	ProcessLightmapSampleData_t data;

	data.m_LightDistSqr = dl.GetRadiusSquared();

	float lightStyleValue = LightStyleValue( dl.style );
	data.m_Intensity[0] = TexLightToLinear( dl.color.r, dl.color.exponent ) * lightStyleValue;
	if ( dl.flags & DLIGHT_SUBTRACT_DISPLACEMENT_ALPHA )
		data.m_Intensity *= -1.0f;

	float minlight = max( g_flMinLightingValue, dl.minlight );
	float ooQuadraticAttn = data.m_LightDistSqr * minlight; // / maxIntensity;

	data.m_ooQuadraticAttn = ooQuadraticAttn;
	data.m_vLightOrigin = dl.origin;
	data.m_ooRadiusSq = 1.0f / dl.GetRadiusSquared();
	data.pProcessLightmapSampleDataFunc = &ProcessLightmapSampleAlpha;

	// Touch all the lightmap samples.
	IterateLightmapSamples( this, data );
#endif
}



//-----------------------------------------------------------------------------
// A little cache to help us not project vertices multiple times
//-----------------------------------------------------------------------------
class CDecalNodeSetupCache
{
public:
	CDecalNodeSetupCache() : m_CurrentCacheIndex(0) {}

	Vector	m_ProjectedVert[MAX_DISPVERTS];
	int		m_CacheIndex[MAX_DISPVERTS];

	bool IsCached( int v )		{ return m_CacheIndex[v] == m_CurrentCacheIndex; }
	void MarkCached( int v )	{ m_CacheIndex[v] = m_CurrentCacheIndex; }
	
	void ResetCache() { ++m_CurrentCacheIndex; }

private:
	int m_CurrentCacheIndex;
};


//-----------------------------------------------------------------------------
// Check to see which nodes are hit by a decal
//-----------------------------------------------------------------------------
bool CDispInfo::SetupDecalNodeIntersect_R( CVertIndex const &nodeIndex,
	int iNodeBitIndex, CDispDecalBase *pDispDecal, ShadowInfo_t const* pInfo, 
	int iLevel, CDecalNodeSetupCache* pCache )
{	
	int iNodeIndex = VertIndex( nodeIndex );

	if( iLevel+1 < m_Power )
	{
		// Recurse into child nodes.
		bool anyChildIntersected = false;
		int iChildNodeBit = iNodeBitIndex + 1;
		for( int iChild=0; iChild < 4; iChild++ )
		{
			CVertIndex const &childNode = m_pPowerInfo->m_pChildVerts[iNodeIndex].m_Verts[iChild];

			// If any of our children intersect, then we do too...
			if (SetupDecalNodeIntersect_R( childNode, iChildNodeBit, pDispDecal, pInfo, iLevel + 1, pCache ) )
				anyChildIntersected = true;
			iChildNodeBit += m_pPowerInfo->m_NodeIndexIncrements[iLevel];
		}

		if (anyChildIntersected)
		{
			pDispDecal->m_NodeIntersect.Set( iNodeBitIndex );
			return true;
		}

		// None of our children intersect this decal, so neither does the node
		return false;
	}

	// Expand our box by the node and by its side verts.
	Vector vMin, vMax;
	if (!pCache->IsCached(iNodeIndex))
	{
		DecalProjectVert( m_MeshReader.Position( iNodeIndex ), pDispDecal, pInfo, pCache->m_ProjectedVert[iNodeIndex] );
		pCache->MarkCached(iNodeIndex);
	}
	vMin = pCache->m_ProjectedVert[iNodeIndex];
	vMax = pCache->m_ProjectedVert[iNodeIndex];

	// Now test each neighbor + child vert to see if it should exist.
	for( int i=0; i < 4; i++ )
	{
		CVertIndex const &sideVert = m_pPowerInfo->m_pSideVerts[iNodeIndex].m_Verts[i];
		CVertIndex const &cornerVert = m_pPowerInfo->m_pSideVertCorners[iNodeIndex].m_Verts[i];

		int iSideIndex = VertIndex(sideVert);
		if (!pCache->IsCached(iSideIndex))
		{
			DecalProjectVert( m_MeshReader.Position( iSideIndex ), pDispDecal, pInfo, pCache->m_ProjectedVert[iSideIndex] );
			pCache->MarkCached(iSideIndex);
		}

		VectorMin( pCache->m_ProjectedVert[iSideIndex], vMin, vMin );
		VectorMax( pCache->m_ProjectedVert[iSideIndex], vMax, vMax );

		int iCornerIndex = VertIndex(cornerVert);
		if (!pCache->IsCached(iCornerIndex))
		{
			DecalProjectVert( m_MeshReader.Position( iCornerIndex ), pDispDecal, pInfo, pCache->m_ProjectedVert[iCornerIndex] );
			pCache->MarkCached(iCornerIndex);
		}

		VectorMin( pCache->m_ProjectedVert[iCornerIndex], vMin, vMin );
		VectorMax( pCache->m_ProjectedVert[iCornerIndex], vMax, vMax );
	}

	// Now just see if our bbox intersects the [0,0] - [1,1] bbox, which is where this
	// decal sits.
	if( vMin.x <= 1 && vMax.x >= 0 && vMin.y <= 1 && vMax.y >= 0 )
	{
		// Z cull for shadows...
		if( pInfo )
		{
			if ((vMax.z < 0) || (vMin.z > pInfo->m_MaxDist))
				return false;
		}

		// Ok, this node is needed and its children may be needed as well.
		pDispDecal->m_NodeIntersect.Set( iNodeBitIndex );
		return true;
	}

	return false;
}

void CDispInfo::SetupDecalNodeIntersect( CVertIndex const &nodeIndex, int iNodeBitIndex,
	CDispDecalBase *pDispDecal,	ShadowInfo_t const* pInfo )
{
	pDispDecal->m_NodeIntersect.ClearAll();

	// Generate a vertex cache, so we're not continually reprojecting vertices...
	static CDecalNodeSetupCache cache;
	cache.ResetCache();

	bool anyIntersection = SetupDecalNodeIntersect_R( 
		nodeIndex, iNodeBitIndex, pDispDecal, pInfo, 0, &cache );

	pDispDecal->m_Flags |= CDispDecalBase::NODE_BITFIELD_COMPUTED;
	if (anyIntersection)
		pDispDecal->m_Flags &= ~CDispDecalBase::NO_INTERSECTION;
	else
		pDispDecal->m_Flags |= CDispDecalBase::NO_INTERSECTION;
}


Vector CDispInfo::GetFlatVert( int iVertex )
{
	int sideLength = m_pPowerInfo->GetSideLength();
	int x = iVertex % sideLength;
	int y = iVertex / sideLength;
	
	float ooInt = 1.0f / ( float )( sideLength - 1 );

	// Lerp between the left and right edges to get a line along 'x'.
	Vector endPts[2];
	VectorLerp( m_BaseSurfacePositions[0], m_BaseSurfacePositions[1], y*ooInt, endPts[0] );
	VectorLerp( m_BaseSurfacePositions[3], m_BaseSurfacePositions[2], y*ooInt, endPts[1] );
	
	// Lerp along the X line.
	Vector vOutputPos;
	VectorLerp( endPts[0], endPts[1], x*ooInt, vOutputPos );

	// This can be used to verify that the position generated here is correct.
	// It should be the same as CCoreDispInfo::GetFlatVert.
	// Assert( vOutputPos.DistTo( m_Verts[iVertex].m_vFlatPos ) < 0.1f );
	
	// Voila!
	return vOutputPos;
}


//-----------------------------------------------------------------------------
// Computes the texture + lightmap coordinate given a displacement uv
//-----------------------------------------------------------------------------

void CDispInfo::ComputeLightmapAndTextureCoordinate( RayDispOutput_t const& output, 
													Vector2D* luv, Vector2D* tuv )
{	
#ifndef SWDS
	// lightmap coordinate
	if( luv )
	{
		ComputePointFromBarycentric( 
			m_MeshReader.TexCoordVector2D( output.ndxVerts[0], DISP_LMCOORDS_STAGE ),
			m_MeshReader.TexCoordVector2D( output.ndxVerts[1], DISP_LMCOORDS_STAGE ),
			m_MeshReader.TexCoordVector2D( output.ndxVerts[2], DISP_LMCOORDS_STAGE ),
			output.u, output.v, *luv );

		// luv is in the space of the accumulated lightmap page; we need to convert
		// it to be in the space of the surface
		int lightmapPageWidth, lightmapPageHeight;
		materials->GetLightmapPageSize( 
			SortInfoToLightmapPage(MSurf_MaterialSortID( m_ParentSurfID ) ),
			&lightmapPageWidth, &lightmapPageHeight );

		luv->x *= lightmapPageWidth;
		luv->y *= lightmapPageHeight;

		luv->x -= 0.5f + MSurf_OffsetIntoLightmapPage( m_ParentSurfID )[0];
		luv->y -= 0.5f + MSurf_OffsetIntoLightmapPage( m_ParentSurfID )[1];
	}

	// texture coordinate
	if( tuv )
	{
		// Compute base face (u,v) at each of the three vertices
		int size = (1 << m_Power) + 1; 

		Vector2D baseUV[3];
		for (int i = 0; i < 3; ++i )
		{
			baseUV[i].y = (int)(output.ndxVerts[i] / size);
			baseUV[i].x = output.ndxVerts[i] - size * baseUV[i].y;
			baseUV[i] /= size - 1;
		}

		Vector2D basefaceUV;
		ComputePointFromBarycentric( baseUV[0], baseUV[1], baseUV[2],
			output.u, output.v, basefaceUV );

		// Convert the base face uv to a texture uv based on the base face texture coords
		TexCoordInQuadFromBarycentric( m_BaseSurfaceTexCoords[0],
			m_BaseSurfaceTexCoords[3], m_BaseSurfaceTexCoords[2], m_BaseSurfaceTexCoords[1],
			basefaceUV, *tuv ); 
	}
#endif
}



//-----------------------------------------------------------------------------
// Cast a ray against this surface
//-----------------------------------------------------------------------------

bool CDispInfo::TestRay( Ray_t const& ray, float start, float end, float& dist, 
						Vector2D* luv, Vector2D* tuv )
{
	// Get the index associated with this disp info....
	int idx = DispInfo_ComputeIndex( host_state.worldbrush->hDispInfos, this );
	CDispCollTree* pTree = CollisionBSPData_GetCollisionTree( idx );
	if (!pTree)
		return false;

	CBaseTrace tr;
	tr.fraction = 1.0f;

	// Only test the portion of the ray between start and end
	Vector startpt, endpt,endpt2;
	VectorMA( ray.m_Start, start, ray.m_Delta, startpt );
	VectorMA( ray.m_Start, end, ray.m_Delta, endpt );

	Ray_t shortenedRay;
	shortenedRay.Init( startpt, endpt );

	RayDispOutput_t	output;
	output.dist = 1.0f;
	if (pTree->AABBTree_Ray( shortenedRay, output ))
	{
		Assert( (output.u <= 1.0f) && (output.v <= 1.0f ));
		Assert( (output.u >= 0.0f) && (output.v >= 0.0f ));

		// Compute the actual distance along the ray
		dist = start * (1.0f - output.dist) + end * output.dist;

		// Compute lightmap + texture coordinates
		ComputeLightmapAndTextureCoordinate( output, luv, tuv );
		return true;
	}

	return false;
}

const CPowerInfo* CDispInfo::GetPowerInfo() const
{
	return m_pPowerInfo;
}


CDispNeighbor* CDispInfo::GetEdgeNeighbor( int index )
{
	Assert( index >= 0 && index < ARRAYSIZE( m_EdgeNeighbors ) );
	return &m_EdgeNeighbors[index];
}


CDispCornerNeighbors* CDispInfo::GetCornerNeighbors( int index )
{
	Assert( index >= 0 && index < ARRAYSIZE( m_CornerNeighbors ) );
	return &m_CornerNeighbors[index];
}


CDispUtilsHelper* CDispInfo::GetDispUtilsByIndex( int index )
{
	return GetDispByIndex( index );	
}



//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "studiorender.h"
#include "studiorendercontext.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "mathlib/mathlib.h"
#include "optimize.h"
#include "cmodel.h"
#include "materialsystem/imaterialvar.h"
#include "convar.h"

#include "tier0/vprof.h"
#include "tier0/minidump.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static int g_nTotalDecalVerts;

//-----------------------------------------------------------------------------
// Decal triangle clip flags
//-----------------------------------------------------------------------------
enum
{
	DECAL_CLIP_MINUSU	= 0x1,
	DECAL_CLIP_MINUSV	= 0x2,
	DECAL_CLIP_PLUSU	= 0x4,
	DECAL_CLIP_PLUSV	= 0x8,
};


#define MAX_DECAL_INDICES_PER_MODEL 2048


//-----------------------------------------------------------------------------
// Triangle clipping state
//-----------------------------------------------------------------------------
struct DecalClipState_t
{
	// Number of used vertices
	int m_VertCount;

	// Indices into the clip verts array of the used vertices
	int m_Indices[2][7];

	// Helps us avoid copying the m_Indices array by using double-buffering
	bool m_Pass;

	// Add vertices we've started with and had to generate due to clipping
	int m_ClipVertCount;
	DecalVertex_t	m_ClipVerts[16];

	// Union of the decal triangle clip flags above for each vert
	int m_ClipFlags[16];

	DecalClipState_t() {}

private:
	// Copy constructors are not allowed
	DecalClipState_t( const DecalClipState_t& src );
};


//-----------------------------------------------------------------------------
//
// Lovely decal code begins here... ABANDON ALL HOPE YE WHO ENTER!!!
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Functions to make vertex opaque
//-----------------------------------------------------------------------------

#ifdef COMPACT_DECAL_VERT
#define GetVecTexCoord( v ) (v.operator Vector2D())
#define GetVecNormal( v ) (v.operator Vector())
#else
#define GetVecTexCoord( v ) v
#define GetVecNormal( v ) v
#endif


//-----------------------------------------------------------------------------
// Remove decal from LRU
//-----------------------------------------------------------------------------
void CStudioRender::RemoveDecalListFromLRU( StudioDecalHandle_t h )
{
	DecalLRUListIndex_t i, next;
	for ( i = m_DecalLRU.Head(); i != m_DecalLRU.InvalidIndex(); i = next )
	{
		next = m_DecalLRU.Next(i);
		if ( m_DecalLRU[i].m_hDecalHandle == h )
		{
			m_DecalLRU.Remove( i );
		}
	}
}


//-----------------------------------------------------------------------------
// Create, destroy list of decals for a particular model
//-----------------------------------------------------------------------------
StudioDecalHandle_t CStudioRender::CreateDecalList( studiohwdata_t *pHardwareData )
{
	if ( !pHardwareData || pHardwareData->m_NumLODs <= 0 )
		return STUDIORENDER_DECAL_INVALID;

	// NOTE: This function is called directly without queueing 
	m_DecalMutex.Lock();
	int handle = m_DecalList.AddToTail();
	m_DecalMutex.Unlock();

	m_DecalList[handle].m_pHardwareData = pHardwareData;
	m_DecalList[handle].m_pLod = new DecalLod_t[pHardwareData->m_NumLODs];
	m_DecalList[handle].m_nLods = pHardwareData->m_NumLODs;

	for (int i = 0; i < pHardwareData->m_NumLODs; i++)
	{
		m_DecalList[handle].m_pLod[i].m_FirstMaterial = m_DecalMaterial.InvalidIndex();
	}

	return (StudioDecalHandle_t)handle;
}

void CStudioRender::DestroyDecalList( StudioDecalHandle_t hDecal )
{
	if ( hDecal == STUDIORENDER_DECAL_INVALID )
		return;

	RemoveDecalListFromLRU( hDecal );

	int h = (int)hDecal;
	// Clean up 
	for (int i = 0; i < m_DecalList[h].m_nLods; i++ )
	{
		// Blat out all geometry associated with all materials
		unsigned short mat = m_DecalList[h].m_pLod[i].m_FirstMaterial;
		unsigned short next;
		while (mat != m_DecalMaterial.InvalidIndex())
		{
			next = m_DecalMaterial.Next(mat);

			g_nTotalDecalVerts -= m_DecalMaterial[mat].m_Vertices.Count();

			m_DecalMaterial.Free(mat);

			mat = next;
		}
	}

	delete[] m_DecalList[h].m_pLod;
	m_DecalList[h].m_pLod = NULL;

	m_DecalMutex.Lock();
	m_DecalList.Remove( h );
	m_DecalMutex.Unlock();
}


//-----------------------------------------------------------------------------
// Transformation/Rotation for decals
//-----------------------------------------------------------------------------
#define FRONTFACING_EPS	0.1f

inline bool CStudioRender::IsFrontFacing( const Vector * pnorm, const mstudioboneweight_t * pboneweight )
{
	// NOTE: This only works to rotate normals if there's no scale in the
	// pose to world transforms. If we ever add scale, we'll need to
	// multiply by the inverse transpose of the pose to decal

	float z;
	if (pboneweight->numbones == 1)
	{
		z = DotProduct( pnorm->Base(), m_PoseToDecal[(unsigned)pboneweight->bone[0]][2] );
	}
	else
	{
		float zbone;

		z = 0;
		for (int i = 0; i < pboneweight->numbones; i++)
		{
			zbone = DotProduct( pnorm->Base(), m_PoseToDecal[(unsigned)pboneweight->bone[i]][2] );
			z += zbone * pboneweight->weight[i];
		}
	}

	return ( z >= FRONTFACING_EPS );
}

inline bool CStudioRender::TransformToDecalSpace( DecalBuildInfo_t& build, const Vector& pos, 
						mstudioboneweight_t *pboneweight, Vector2D& uv )
{
	// NOTE: This only works to rotate normals if there's no scale in the
	// pose to world transforms. If we ever add scale, we'll need to
	// multiply by the inverse transpose of the pose to world

	if (pboneweight->numbones == 1)
	{
		uv.x = DotProduct( pos.Base(), m_PoseToDecal[(unsigned)pboneweight->bone[0]][0] ) + 
			m_PoseToDecal[(unsigned)pboneweight->bone[0]][0][3];
		uv.y = DotProduct( pos.Base(), m_PoseToDecal[(unsigned)pboneweight->bone[0]][1] ) + 
			m_PoseToDecal[(unsigned)pboneweight->bone[0]][1][3];
	}
	else
	{
		uv.x = uv.y = 0;
		float ubone, vbone;
		for (int i = 0; i < pboneweight->numbones; i++)
		{
			ubone = DotProduct( pos.Base(), m_PoseToDecal[(unsigned)pboneweight->bone[i]][0] ) + 
				m_PoseToDecal[(unsigned)pboneweight->bone[i]][0][3];
			vbone = DotProduct( pos.Base(), m_PoseToDecal[(unsigned)pboneweight->bone[i]][1] ) + 
				m_PoseToDecal[(unsigned)pboneweight->bone[i]][1][3];

			uv.x += ubone * pboneweight->weight[i];
			uv.y += vbone * pboneweight->weight[i];
		}
	}

	if (!build.m_NoPokeThru)
		return true;

	// No poke thru? do culling....
	float z;
	if (pboneweight->numbones == 1)
	{
		z = DotProduct( pos.Base(), m_PoseToDecal[(unsigned)pboneweight->bone[0]][2] ) + 
			m_PoseToDecal[(unsigned)pboneweight->bone[0]][2][3];
	}
	else
	{
		z = 0;
		float zbone;
		for (int i = 0; i < pboneweight->numbones; i++)
		{
			zbone = DotProduct( pos.Base(), m_PoseToDecal[(unsigned)pboneweight->bone[i]][2] ) + 
				m_PoseToDecal[(unsigned)pboneweight->bone[i]][2][3];
			z += zbone * pboneweight->weight[i];
		}
	}

	return (fabs(z) < build.m_Radius );
}


//-----------------------------------------------------------------------------
// Projects a decal onto a mesh
//-----------------------------------------------------------------------------
bool CStudioRender::ProjectDecalOntoMesh( DecalBuildInfo_t& build, DecalBuildVertexInfo_t* pVertexInfo, mstudiomesh_t *pMesh )
{
	float invRadius = (build.m_Radius != 0.0f) ? 1.0f / build.m_Radius : 1.0f;

	const mstudio_meshvertexdata_t	*vertData		= pMesh->GetVertexData( build.m_pStudioHdr );
	const thinModelVertices_t		*thinVertData	= NULL;

	if ( !vertData )
	{
		// For most models (everything that's not got flex data), the vertex data is 'thinned' on load to save memory
		thinVertData = pMesh->GetThinVertexData( build.m_pStudioHdr );
		if ( !thinVertData )
			return false;
	}

	// For this to work, the plane and intercept must have been transformed
	// into pose space. Also, we'll not be bothering with flexes.
	for ( int j=0; j < pMesh->numvertices; ++j )
	{
		mstudioboneweight_t	localBoneWeights;
		Vector				localPosition;
		Vector				localNormal;
		Vector				* vecPosition;
		Vector				* vecNormal;
		mstudioboneweight_t	* boneWeights;

		if ( vertData )
		{
			mstudiovertex_t &vert = *vertData->Vertex( j );
			vecPosition = &vert.m_vecPosition;
			vecNormal   = &vert.m_vecNormal;
			boneWeights = &vert.m_BoneWeights;
		}
		else
		{
			thinVertData->GetMeshPosition( pMesh, j, &localPosition );
			vecPosition = &localPosition;
			thinVertData->GetMeshNormal( pMesh, j, &localNormal );
			vecNormal = &localNormal;
			thinVertData->GetMeshBoneWeights( pMesh, j, &localBoneWeights );
			boneWeights = &localBoneWeights;
		}

		// No decal vertex yet...
		pVertexInfo[j].m_VertexIndex = 0xFFFF;
		pVertexInfo[j].m_UniqueID = 0xFF;
		pVertexInfo[j].m_Flags = 0;

		// We need to know if the normal is pointing in the negative direction
		// if so, blow off all triangles connected to that vertex.
		if ( !IsFrontFacing( vecNormal, boneWeights ) )
			continue;

		pVertexInfo[j].m_Flags |= DecalBuildVertexInfo_t::FRONT_FACING;

		bool inValidArea = TransformToDecalSpace( build, *vecPosition, boneWeights, pVertexInfo[j].m_UV );
		pVertexInfo[j].m_Flags |= ( inValidArea << 1 );

		pVertexInfo[j].m_UV *= invRadius * 0.5f;
		pVertexInfo[j].m_UV[0] += 0.5f;
		pVertexInfo[j].m_UV[1] += 0.5f;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Computes clip flags
//-----------------------------------------------------------------------------
inline int ComputeClipFlags( Vector2D const& uv )
{
	// Otherwise we gotta do the test
	int flags = 0;

	if (uv.x < 0.0f)
		flags |= DECAL_CLIP_MINUSU;
	else if (uv.x > 1.0f)
		flags |= DECAL_CLIP_PLUSU;

	if (uv.y < 0.0f)
		flags |= DECAL_CLIP_MINUSV;
	else if (uv.y > 1.0f )
		flags |= DECAL_CLIP_PLUSV;

	return flags;
}

inline int CStudioRender::ComputeClipFlags( DecalBuildVertexInfo_t* pVertexInfo, int i )
{
	return ::ComputeClipFlags( pVertexInfo[i].m_UV );
}


//-----------------------------------------------------------------------------
// Creates a new vertex where the edge intersects the plane
//-----------------------------------------------------------------------------
static int IntersectPlane( DecalClipState_t& state, int start, int end, 
						    int normalInd, float val )
{
	DecalVertex_t& startVert = state.m_ClipVerts[start];
	DecalVertex_t& endVert = state.m_ClipVerts[end];

	Vector2D dir;
	Vector2DSubtract( endVert.m_TexCoord, startVert.m_TexCoord, dir );
	Assert( dir[normalInd] != 0.0f );
	float t = (val - GetVecTexCoord( startVert.m_TexCoord )[normalInd]) / dir[normalInd];
				 
	// Allocate a clipped vertex
	DecalVertex_t& out = state.m_ClipVerts[state.m_ClipVertCount];
	int newVert = state.m_ClipVertCount++;

	// The clipped vertex has no analogue in the original mesh
	out.m_MeshVertexIndex = 0xFFFF;
	out.m_Mesh = 0xFFFF;
	out.m_Model = ( sizeof(out.m_Model) == 1 ) ? 0xFF : 0xFFFF;
	out.m_Body = ( sizeof(out.m_Body) == 1 ) ? 0xFF : 0xFFFF;

	// Interpolate position
	out.m_Position[0] = startVert.m_Position[0] * (1.0 - t) + endVert.m_Position[0] * t;
	out.m_Position[1] = startVert.m_Position[1] * (1.0 - t) + endVert.m_Position[1] * t;
	out.m_Position[2] = startVert.m_Position[2] * (1.0 - t) + endVert.m_Position[2] * t;

	// Interpolate normal
	Vector vNormal;
	// FIXME: this is a bug (it's using position data to compute interpolated normals!)... not seeing any obvious artifacts, though
	vNormal[0] = startVert.m_Position[0] * (1.0 - t) + endVert.m_Position[0] * t;
	vNormal[1] = startVert.m_Position[1] * (1.0 - t) + endVert.m_Position[1] * t;
	vNormal[2] = startVert.m_Position[2] * (1.0 - t) + endVert.m_Position[2] * t;
	VectorNormalize( vNormal );
	out.m_Normal = vNormal;

	// Interpolate texture coord
	Vector2D vTexCoord;
	Vector2DLerp( GetVecTexCoord( startVert.m_TexCoord ), GetVecTexCoord( endVert.m_TexCoord ), t, vTexCoord );
	out.m_TexCoord = vTexCoord;

	// Compute the clip flags baby...
	state.m_ClipFlags[newVert] = ComputeClipFlags( out.m_TexCoord );

	return newVert;
}

//-----------------------------------------------------------------------------
// Clips a triangle against a plane, use clip flags to speed it up
//-----------------------------------------------------------------------------

static void ClipTriangleAgainstPlane( DecalClipState_t& state, int normalInd, int flag, float val )
{
	// FIXME: Could compute the & of all the clip flags of all the verts
	// as we go through the loop to do another early out

	// Ye Olde Sutherland-Hodgman clipping algorithm
	int outVertCount = 0;
	int start = state.m_Indices[state.m_Pass][state.m_VertCount - 1];
	bool startInside = (state.m_ClipFlags[start] & flag) == 0;
	for (int i = 0; i < state.m_VertCount; ++i)
	{
		int end = state.m_Indices[state.m_Pass][i];

		bool endInside = (state.m_ClipFlags[end] & flag) == 0;
		if (endInside)
		{
			if (!startInside)
			{
				int clipVert = IntersectPlane( state, start, end, normalInd, val );
				state.m_Indices[!state.m_Pass][outVertCount++] = clipVert;
			}
			state.m_Indices[!state.m_Pass][outVertCount++] = end;
		}
		else
		{
			if (startInside)
			{
				int clipVert = IntersectPlane( state, start, end, normalInd, val );
				state.m_Indices[!state.m_Pass][outVertCount++] = clipVert;
			}
		}
		start = end;
		startInside = endInside;
	}

	state.m_Pass = !state.m_Pass;
	state.m_VertCount = outVertCount;
}


//-----------------------------------------------------------------------------
// Converts a mesh index to a DecalVertex_t
//-----------------------------------------------------------------------------
void CStudioRender::ConvertMeshVertexToDecalVertex( DecalBuildInfo_t& build, 
	int meshIndex, DecalVertex_t& decalVertex, int nGroupIndex )
{
	// Copy over the data;
	// get the texture coords from the decal planar projection

	Assert( meshIndex < MAXSTUDIOVERTS );

	if ( build.m_pMeshVertexData )
	{
		VectorCopy( *build.m_pMeshVertexData->Position( meshIndex ), decalVertex.m_Position );
		VectorCopy( *build.m_pMeshVertexData->Normal( meshIndex ), GetVecNormal( decalVertex.m_Normal ) );	
	}
	else
	{
		// At this point in the code, we should definitely have either compressed or uncompressed vertex data
		Assert( build.m_pMeshThinVertexData );
		Vector position;
		Vector normal;
		build.m_pMeshThinVertexData->GetMeshPosition( build.m_pMesh, meshIndex, &position );
		build.m_pMeshThinVertexData->GetMeshNormal( build.m_pMesh, meshIndex, &normal );
		VectorCopy( position, decalVertex.m_Position );
		VectorCopy( normal, GetVecNormal( decalVertex.m_Normal ) );	
	}
	Vector2DCopy( build.m_pVertexInfo[meshIndex].m_UV, GetVecTexCoord( decalVertex.m_TexCoord ) );
	decalVertex.m_MeshVertexIndex = meshIndex;
	decalVertex.m_Mesh = build.m_Mesh;
	Assert( decalVertex.m_Mesh < 100 );
	decalVertex.m_Model = build.m_Model;
	decalVertex.m_Body = build.m_Body;
	decalVertex.m_Group = build.m_Group;
	decalVertex.m_GroupIndex = nGroupIndex;
}


//-----------------------------------------------------------------------------
// Adds a vertex to the list of vertices for this material
//-----------------------------------------------------------------------------
inline unsigned short CStudioRender::AddVertexToDecal( DecalBuildInfo_t& build, int nMeshIndex, int nGroupIndex )
{
	DecalBuildVertexInfo_t* pVertexInfo = build.m_pVertexInfo;

	// If we've never seen this vertex before, we need to add a new decal vert
	if ( pVertexInfo[nMeshIndex].m_UniqueID != build.m_nGlobalMeshIndex )
	{
		pVertexInfo[nMeshIndex].m_UniqueID = build.m_nGlobalMeshIndex;
		DecalVertexList_t& decalVertexList = build.m_pDecalMaterial->m_Vertices;

		DecalVertexList_t::IndexType_t v;
		v = decalVertexList.AddToTail();
		g_nTotalDecalVerts++;

		// Copy over the data;
		ConvertMeshVertexToDecalVertex( build, nMeshIndex, build.m_pDecalMaterial->m_Vertices[v], nGroupIndex );

#ifdef _DEBUG
		// Make sure clipped vertices are in the right range...
		if (build.m_UseClipVert)
		{
			Assert( (decalVertexList[v].m_TexCoord[0] >= -1e-3) && (decalVertexList[v].m_TexCoord[0] - 1.0f < 1e-3) );
			Assert( (decalVertexList[v].m_TexCoord[1] >= -1e-3) && (decalVertexList[v].m_TexCoord[1] - 1.0f < 1e-3) );
		}
#endif

		// Store off the index of this vertex so we can reference it again
		pVertexInfo[nMeshIndex].m_VertexIndex = build.m_VertexCount;
		++build.m_VertexCount;
		if (build.m_FirstVertex == decalVertexList.InvalidIndex())
		{
			build.m_FirstVertex = v;
		}
	}

	return pVertexInfo[nMeshIndex].m_VertexIndex;
}


//-----------------------------------------------------------------------------
// Adds a vertex to the list of vertices for this material
//-----------------------------------------------------------------------------
inline unsigned short CStudioRender::AddVertexToDecal( DecalBuildInfo_t& build, DecalVertex_t& vert )
{
	// This creates a unique vertex
	DecalVertexList_t& decalVertexList = build.m_pDecalMaterial->m_Vertices;

	// Try to see if the clipped vertex already exists in our decal list...
	// Only search for matches with verts appearing in the current decal
	DecalVertexList_t::IndexType_t i;
	unsigned short vertexCount = 0;
	for ( i = build.m_FirstVertex; i != decalVertexList.InvalidIndex(); 
		i = decalVertexList.Next(i), ++vertexCount )
	{
		// Only bother to check against clipped vertices
		if ( decalVertexList[i].GetMesh( build.m_pStudioHdr ) )
			continue;

		// They must have the same position, and normal
		// texcoord will fall right out if the positions match
		Vector temp;
		VectorSubtract( decalVertexList[i].m_Position, vert.m_Position, temp );
		if ( (fabs(temp[0]) > 1e-3) || (fabs(temp[1]) > 1e-3) || (fabs(temp[2]) > 1e-3) )
			continue;

		VectorSubtract( decalVertexList[i].m_Normal, vert.m_Normal, temp );
		if ( (fabs(temp[0]) > 1e-3) || (fabs(temp[1]) > 1e-3) || (fabs(temp[2]) > 1e-3) )
			continue;

		return vertexCount;
	}

	// This path is the path taken by clipped vertices
	Assert( (vert.m_TexCoord[0] >= -1e-3) && (vert.m_TexCoord[0] - 1.0f < 1e-3) );
	Assert( (vert.m_TexCoord[1] >= -1e-3) && (vert.m_TexCoord[1] - 1.0f < 1e-3) );

	// Must create a new vertex...
	DecalVertexList_t::IndexType_t idx = decalVertexList.AddToTail(vert);
	g_nTotalDecalVerts++;
	if (build.m_FirstVertex == decalVertexList.InvalidIndex())
		build.m_FirstVertex = idx;
	Assert( vertexCount == build.m_VertexCount );
	return build.m_VertexCount++;
}


//-----------------------------------------------------------------------------
// Adds the clipped triangle to the decal
//-----------------------------------------------------------------------------
void CStudioRender::AddClippedDecalToTriangle( DecalBuildInfo_t& build, DecalClipState_t& clipState )
{
	// FIXME: Clipped vertices will almost always be shared. We
	// need a way of associating clipped vertices with edges so we can share
	// the clipped vertices quickly
	Assert( clipState.m_VertCount <= 7 );

	// Yeah baby yeah!!	Add this sucka
	int i;
	unsigned short indices[7];
	for ( i = 0; i < clipState.m_VertCount; ++i)
	{
		// First add the vertices
		int vertIdx = clipState.m_Indices[clipState.m_Pass][i];
		if (vertIdx < 3)
		{
			indices[i] = AddVertexToDecal( build, clipState.m_ClipVerts[vertIdx].m_MeshVertexIndex );
		}
		else
		{
			indices[i] = AddVertexToDecal( build, clipState.m_ClipVerts[vertIdx] );
		}
	}

	// Add a trifan worth of triangles
	for ( i = 1; i < clipState.m_VertCount - 1; ++i)
	{
		MEM_ALLOC_CREDIT();
		build.m_pDecalMaterial->m_Indices.AddToTail( indices[0] );
		build.m_pDecalMaterial->m_Indices.AddToTail( indices[i] );
		build.m_pDecalMaterial->m_Indices.AddToTail( indices[i+1] );
	}
}


//-----------------------------------------------------------------------------
// Clips the triangle to +/- radius
//-----------------------------------------------------------------------------
bool CStudioRender::ClipDecal( DecalBuildInfo_t& build, int i1, int i2, int i3, int *pClipFlags )
{
	int i;

	DecalClipState_t clipState;
	clipState.m_VertCount = 3;
	ConvertMeshVertexToDecalVertex( build, i1, clipState.m_ClipVerts[0] );
	ConvertMeshVertexToDecalVertex( build, i2, clipState.m_ClipVerts[1] );
	ConvertMeshVertexToDecalVertex( build, i3, clipState.m_ClipVerts[2] );
	clipState.m_ClipVertCount = 3;

	for ( i = 0; i < 3; ++i)
	{
		clipState.m_ClipFlags[i] = pClipFlags[i];
		clipState.m_Indices[0][i] = i;
	}
	clipState.m_Pass = 0;

	// Clip against each plane
	ClipTriangleAgainstPlane( clipState, 0, DECAL_CLIP_MINUSU, 0.0f );
	if (clipState.m_VertCount < 3)
		return false;

	ClipTriangleAgainstPlane( clipState, 0, DECAL_CLIP_PLUSU, 1.0f );
	if (clipState.m_VertCount < 3)
		return false;

	ClipTriangleAgainstPlane( clipState, 1, DECAL_CLIP_MINUSV, 0.0f );
	if (clipState.m_VertCount < 3)
		return false;

	ClipTriangleAgainstPlane( clipState, 1, DECAL_CLIP_PLUSV, 1.0f );
	if (clipState.m_VertCount < 3)
		return false;

	// Only add the clipped decal to the triangle if it's one bone
	// otherwise just return if it was clipped
	if ( build.m_UseClipVert )
	{
		AddClippedDecalToTriangle( build, clipState );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Adds a decal to a triangle, but only if it should
//-----------------------------------------------------------------------------
void CStudioRender::AddTriangleToDecal( DecalBuildInfo_t& build, int i1, int i2, int i3, int gi1, int gi2, int gi3 )
{
	DecalBuildVertexInfo_t* pVertexInfo = build.m_pVertexInfo;

	// All must be front-facing for a decal to be added
	// FIXME: Could make it work if not all are front-facing, need clipping for that
	int nAllFrontFacing = pVertexInfo[i1].m_Flags & pVertexInfo[i2].m_Flags & pVertexInfo[i3].m_Flags;
	if ( ( nAllFrontFacing & DecalBuildVertexInfo_t::FRONT_FACING ) == 0 )
		return;

	// This is used to prevent poke through; if the points are too far away
	// from the contact point, then don't add the decal
	int nAllNotInValidArea = pVertexInfo[i1].m_Flags | pVertexInfo[i2].m_Flags | pVertexInfo[i3].m_Flags;
	if ( ( nAllNotInValidArea & DecalBuildVertexInfo_t::VALID_AREA ) == 0 )
		return;

	// Clip to +/- radius
	int clipFlags[3];

	clipFlags[0] = ComputeClipFlags( pVertexInfo, i1 );
	clipFlags[1] = ComputeClipFlags( pVertexInfo, i2 );
	clipFlags[2] = ComputeClipFlags( pVertexInfo, i3 );

	// Cull... The result is non-zero if they're all outside the same plane
	if ( (clipFlags[0] & (clipFlags[1] & clipFlags[2]) ) != 0)
		return;

	bool doClip = true;
	
	// Trivial accept for skinned polys... if even one vert is inside
	// the draw region, accept
	if ((!build.m_UseClipVert) && ( !clipFlags[0] || !clipFlags[1] || !clipFlags[2] ))
	{
		doClip = false;
	}

	// Trivial accept... no clip flags set means all in
	// Don't clip if we have more than one bone... we'll need to do skinning
	// and we can't clip the bone indices
	// We *do* want to clip in the one bone case though; useful for large
	// static props.
	if ( doClip && ( clipFlags[0] || clipFlags[1] || clipFlags[2] ))
	{
		bool validTri = ClipDecal( build, i1, i2, i3, clipFlags );

		// Don't add the triangle if we culled the triangle or if 
		// we had one or less bones
		if (build.m_UseClipVert || (!validTri))
			return;
	}

	// Add the vertices to the decal since there was no clipping
	i1 = AddVertexToDecal( build, i1, gi1 );
	i2 = AddVertexToDecal( build, i2, gi2 );
	i3 = AddVertexToDecal( build, i3, gi3 );

	MEM_ALLOC_CREDIT();
	build.m_pDecalMaterial->m_Indices.AddToTail(i1);
	build.m_pDecalMaterial->m_Indices.AddToTail(i2);
	build.m_pDecalMaterial->m_Indices.AddToTail(i3);
}


//-----------------------------------------------------------------------------
// Adds a decal to a mesh 
//-----------------------------------------------------------------------------
void CStudioRender::AddDecalToMesh( DecalBuildInfo_t& build )
{
	MeshVertexInfo_t &vertexInfo = build.m_pMeshVertices[ build.m_nGlobalMeshIndex ];
	if ( vertexInfo.m_nIndex < 0 )
		return;

	build.m_pVertexInfo = &build.m_pVertexBuffer[ vertexInfo.m_nIndex ];

	// Draw all the various mesh groups...
	for ( int j = 0; j < build.m_pMeshData->m_NumGroup; ++j )
	{
		build.m_Group = j;
		studiomeshgroup_t* pGroup = &build.m_pMeshData->m_pMeshGroup[j];

		// Must add decal to each strip in the strip group
		// We do this so we can re-use all of the bone state change
		// info associated with the strips
		for (int k = 0; k < pGroup->m_NumStrips; ++k)
		{
			OptimizedModel::StripHeader_t* pStrip = &pGroup->m_pStripData[k];
			if (pStrip->flags & OptimizedModel::STRIP_IS_TRISTRIP)
			{
				for (int i = 0; i < pStrip->numIndices - 2; ++i)
				{
					bool ccw = (i & 0x1) == 0;
					int ti1 = pStrip->indexOffset + i;
					int ti2 = ti1+1+ccw;
					int ti3	= ti1+2-ccw;
					int i1 = pGroup->MeshIndex(ti1);
					int i2 = pGroup->MeshIndex(ti2);
					int i3 = pGroup->MeshIndex(ti3);

					AddTriangleToDecal( build, i1, i2, i3, pGroup->m_pIndices[ti1], pGroup->m_pIndices[ti2], pGroup->m_pIndices[ti3] );
				}
			}
			else
			{
				Assert( pStrip->flags & OptimizedModel::STRIP_IS_TRILIST );
				for (int i = 0; i < pStrip->numIndices; i += 3)
				{
					int idx = pStrip->indexOffset + i;

					int i1 = pGroup->MeshIndex(idx);
					int i2 = pGroup->MeshIndex(idx+1);
					int i3 = pGroup->MeshIndex(idx+2);

					AddTriangleToDecal( build, i1, i2, i3, pGroup->m_pIndices[idx], pGroup->m_pIndices[idx+1], pGroup->m_pIndices[idx+2] );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Adds a decal to a mesh 
//-----------------------------------------------------------------------------
bool CStudioRender::AddDecalToModel( DecalBuildInfo_t& buildInfo )
{
	// FIXME: We need to do some high-level culling to figure out exactly
	// which meshes we need to add the decals to
	// Turns out this solution may also be good for mesh sorting
	// we need to know the center of each mesh, could also store a
	// bounding radius for each mesh and test the ray against each sphere.

	for ( int i = 0; i < m_pSubModel->nummeshes; ++i)
	{
		buildInfo.m_Mesh = i;
		buildInfo.m_pMesh = m_pSubModel->pMesh(i);
		buildInfo.m_pMeshData = &m_pStudioMeshes[buildInfo.m_pMesh->meshid];
		Assert(buildInfo.m_pMeshData);
		// Grab either fat or thin vertex data
		buildInfo.m_pMeshVertexData = buildInfo.m_pMesh->GetVertexData( buildInfo.m_pStudioHdr );
		if ( buildInfo.m_pMeshVertexData == NULL )
		{
			buildInfo.m_pMeshThinVertexData = buildInfo.m_pMesh->GetThinVertexData( buildInfo.m_pStudioHdr );
			if ( !buildInfo.m_pMeshThinVertexData )
				return false;
		}

		AddDecalToMesh( buildInfo );
		++buildInfo.m_nGlobalMeshIndex;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Computes the pose to decal plane transform 
//-----------------------------------------------------------------------------
bool CStudioRender::ComputePoseToDecal( const Ray_t& ray, const Vector& up )
{
	// Create a transform that projects world coordinates into a 
	// basis for the decal
	matrix3x4_t worldToDecal;
	Vector decalU, decalV, decalN;

	// Get the z axis
	VectorMultiply( ray.m_Delta, -1.0f, decalN );
	if (VectorNormalize( decalN ) == 0.0f)
		return false;

	// Deal with the u axis
	CrossProduct( up, decalN, decalU );
	if ( VectorNormalize( decalU ) < 1e-3 )
	{
		// if up parallel or antiparallel to ray, deal...
		Vector fixup( up.y, up.z, up.x );
		CrossProduct( fixup, decalN, decalU );
		if ( VectorNormalize( decalU ) < 1e-3 )
			return false;
	}

	CrossProduct( decalN, decalU, decalV );

	// Since I want world-to-decal, I gotta take the inverse of the decal
	// to world. Assuming post-multiplying column vectors, the decal to world = 
	//		[ Ux Vx Nx | ray.m_Start[0] ]
	//		[ Uy Vy Ny | ray.m_Start[1] ]
	//		[ Uz Vz Nz | ray.m_Start[2] ]

	VectorCopy( decalU.Base(), worldToDecal[0] );
	VectorCopy( decalV.Base(), worldToDecal[1] );
	VectorCopy( decalN.Base(), worldToDecal[2] );

	worldToDecal[0][3] = -DotProduct( ray.m_Start.Base(), worldToDecal[0] );
	worldToDecal[1][3] = -DotProduct( ray.m_Start.Base(), worldToDecal[1] );
	worldToDecal[2][3] = -DotProduct( ray.m_Start.Base(), worldToDecal[2] );

	// Compute transforms from pose space to decal plane space
	for ( int i = 0; i < m_pStudioHdr->numbones; i++)
	{
		ConcatTransforms( worldToDecal, m_PoseToWorld[i], m_PoseToDecal[i] );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Gets the list of triangles for a particular material and lod
//-----------------------------------------------------------------------------

int CStudioRender::GetDecalMaterial( DecalLod_t& decalLod, IMaterial* pDecalMaterial )
{
	// Grab the material for this lod...
	unsigned short j;
	for ( j = decalLod.m_FirstMaterial; j != m_DecalMaterial.InvalidIndex(); j = m_DecalMaterial.Next(j) )
	{
		if (m_DecalMaterial[j].m_pMaterial == pDecalMaterial)
		{
			return j;
		}
	}

	// If we got here, this must be the first time we saw this material
	j = m_DecalMaterial.Alloc( true );
	
	// Link it into the list of data for this lod
	if (decalLod.m_FirstMaterial != m_DecalMaterial.InvalidIndex() )
		m_DecalMaterial.LinkBefore( decalLod.m_FirstMaterial, j );
	decalLod.m_FirstMaterial = j;

	m_DecalMaterial[j].m_pMaterial = pDecalMaterial;

	return j;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CStudioRender::RetireDecal( DecalModelList_t &list, DecalId_t nRetireID, int iLOD, int iMaxLOD )
{
	// Remove it from the global LRU...
	DecalLRUListIndex_t i;
	for ( i = m_DecalLRU.Head(); i != m_DecalLRU.InvalidIndex(); i = m_DecalLRU.Next( i ) )
	{
		if ( nRetireID == m_DecalLRU[i].m_nDecalId )
		{
			m_DecalLRU.Remove( i );
			break;
		}
	}
	Assert( i != m_DecalLRU.InvalidIndex() );

	// Find the id to retire and retire all the decals with this id across all LODs.
	DecalHistoryList_t *pHistoryList = &list.m_pLod[iLOD].m_DecalHistory;
	Assert( pHistoryList->Count() );
	if ( !pHistoryList->Count() )
		return;

	DecalHistory_t *pDecalHistory = &pHistoryList->Element( pHistoryList->Head() );

	// Retire this decal in all lods.
	for ( int iLod = ( iMaxLOD - 1 ); iLod >= list.m_pHardwareData->m_RootLOD; --iLod )
	{
		pHistoryList = &list.m_pLod[iLod].m_DecalHistory;
		if ( !pHistoryList )
			continue;
	
		unsigned short iList = pHistoryList->Head();
		unsigned short iNext = pHistoryList->InvalidIndex();

		while ( iList != pHistoryList->InvalidIndex() )
		{
			iNext = pHistoryList->Next( iList );

			pDecalHistory = &pHistoryList->Element( iList );
			if ( !pDecalHistory || pDecalHistory->m_nId != nRetireID )
			{
				iList = iNext;
				continue;
			}

			// Find the decal material for the decal to remove
			DecalMaterial_t *pMaterial = &m_DecalMaterial[pDecalHistory->m_Material];			
			if ( pMaterial )
			{
				// @Note!! Decals must be removed in the reverse order they are added. This code
				// assumes that the decal to remove is the oldest one on the model, and therefore
				// its vertices start at the head of the list
				DecalVertexList_t &vertices = pMaterial->m_Vertices;
				Decal_t &decalToRemove = pMaterial->m_Decals[pDecalHistory->m_Decal];
				
				// Now clear out the vertices referenced by the indices....
				DecalVertexList_t::IndexType_t next; 
				DecalVertexList_t::IndexType_t vert = vertices.Head();
				Assert( vertices.Count() >= decalToRemove.m_VertexCount );
				int vertsToRemove = decalToRemove.m_VertexCount;
				while ( vertsToRemove > 0 )
				{
					// blat out the vertices
					next = vertices.Next( vert );
					vertices.Remove( vert );
					vert = next;
					g_nTotalDecalVerts--;
					
					--vertsToRemove;
				}

				if ( vertices.Count() == 0 )
				{
					vertices.Purge();
				}
				
				// FIXME: This does a memmove. How expensive is it?
				pMaterial->m_Indices.RemoveMultiple( 0, decalToRemove.m_IndexCount );
				if ( pMaterial->m_Indices.Count() == 0)
				{
					pMaterial->m_Indices.Purge();
				}
				
				// Remove the decal
				pMaterial->m_Decals.Remove( pDecalHistory->m_Decal );
				if ( pMaterial->m_Decals.Count() == 0)
				{
#if 1
					pMaterial->m_Decals.Purge();
#else
					if ( list.m_pLod[iLOD].m_FirstMaterial == pDecalHistory->m_Material )
					{
						list.m_pLod[iLOD].m_FirstMaterial = m_DecalMaterial.Next( pDecalHistory->m_Material );
					}
					m_DecalMaterial.Free( pDecalHistory->m_Material );
#endif
				}
			}

			// Clear the decal out of the history
			pHistoryList->Remove( iList );

			// Next element.
			iList = iNext;
		}
	}
}

//-----------------------------------------------------------------------------
// Adds a decal to the history list
//-----------------------------------------------------------------------------
int CStudioRender::AddDecalToMaterialList( DecalMaterial_t* pMaterial )
{
	DecalList_t& decalList = pMaterial->m_Decals;
	return decalList.AddToTail();
}


//-----------------------------------------------------------------------------
// Total number of meshes we have to deal with
//-----------------------------------------------------------------------------
int CStudioRender::ComputeTotalMeshCount( int iRootLOD, int iMaxLOD, int body ) const
{
	int nMeshCount = 0;
	for ( int k=0 ; k < m_pStudioHdr->numbodyparts ; k++) 
	{
		mstudiomodel_t *pSubModel;
		R_StudioSetupModel( k, body, &pSubModel, m_pStudioHdr );
		nMeshCount += pSubModel->nummeshes;
	}

	nMeshCount *= iMaxLOD-iRootLOD+1;

	return nMeshCount;
}


//-----------------------------------------------------------------------------
// Set up the locations for vertices to use
//-----------------------------------------------------------------------------
int CStudioRender::ComputeVertexAllocation( int iMaxLOD, int body, studiohwdata_t *pHardwareData, MeshVertexInfo_t *pMeshVertices )
{
	bool bSuppressTlucDecal = (m_pStudioHdr->flags & STUDIOHDR_FLAGS_TRANSLUCENT_TWOPASS) != 0;

	int nCurrMesh = 0;
	int nVertexCount = 0;
	for ( int i = iMaxLOD-1; i >= pHardwareData->m_RootLOD; i--)
	{
		IMaterial **ppMaterials = pHardwareData->m_pLODs[i].ppMaterials;

		for ( int k=0 ; k < m_pStudioHdr->numbodyparts ; k++) 
		{
			mstudiomodel_t *pSubModel;
			R_StudioSetupModel( k, body, &pSubModel, m_pStudioHdr );

			for ( int meshID = 0; meshID < pSubModel->nummeshes; ++meshID, ++nCurrMesh)
			{
				mstudiomesh_t *pMesh = pSubModel->pMesh(meshID);

				pMeshVertices[nCurrMesh].m_pMesh = pMesh;

				int n;
				for ( n = nCurrMesh; --n >= 0; )
				{
					if ( pMeshVertices[n].m_pMesh == pMesh ) 
					{
						pMeshVertices[nCurrMesh].m_nIndex = pMeshVertices[n].m_nIndex;
						break;
					}
				}
				if ( n >= 0 )
					continue;

				// Don't add to the mesh if the mesh has a translucent material
				short *pSkinRef	= m_pStudioHdr->pSkinref( 0 );
				IMaterial *pMaterial = ppMaterials[pSkinRef[pMesh->material]];
				if (bSuppressTlucDecal)
				{
					if (pMaterial->IsTranslucent())
					{
						pMeshVertices[nCurrMesh].m_nIndex = -1;
						continue;
					}
				}

				if ( pMaterial->GetMaterialVarFlag( MATERIAL_VAR_SUPPRESS_DECALS ) )
				{
					pMeshVertices[nCurrMesh].m_nIndex = -1;
					continue;
				}

				pMeshVertices[nCurrMesh].m_nIndex = nVertexCount;
				nVertexCount += pMesh->numvertices;
			}
		}
	}

	return nVertexCount;
}


//-----------------------------------------------------------------------------
// Project decals onto all meshes
//-----------------------------------------------------------------------------
void CStudioRender::ProjectDecalsOntoMeshes( DecalBuildInfo_t& build, int nMeshCount )
{
	int nMaxVertexIndex = -1;

	for ( int i = 0; i < nMeshCount; ++i )
	{
		int nIndex = build.m_pMeshVertices[i].m_nIndex;

		// No mesh, or have we already projected this?
		if (( nIndex < 0 ) || ( nIndex <= nMaxVertexIndex ))
			continue;

		nMaxVertexIndex = nIndex;

		// Project all vertices for this group into decal space
		ProjectDecalOntoMesh( build, &build.m_pVertexBuffer[ nIndex ], build.m_pMeshVertices[i].m_pMesh );
	}
}


	
//-----------------------------------------------------------------------------
// Add decals to a decal list by doing a planar projection along the ray
//-----------------------------------------------------------------------------
void CStudioRender::AddDecal( StudioDecalHandle_t hDecal, const StudioRenderContext_t& rc, matrix3x4_t *pBoneToWorld, 
	studiohdr_t *pStudioHdr, const Ray_t& ray, const Vector& decalUp, IMaterial* pDecalMaterial, 
	float radius, int body, bool noPokethru, int maxLODToDecal )
{
	VPROF( "CStudioRender::AddDecal" );

	if ( hDecal == STUDIORENDER_DECAL_INVALID )
		return;

	// For each lod, build the decal list
	int h = (int)hDecal;
	DecalModelList_t& list = m_DecalList[h];

	if ( list.m_pHardwareData->m_NumStudioMeshes == 0 )
		return;

	m_pRC = const_cast< StudioRenderContext_t* >( &rc );
	m_pStudioHdr = pStudioHdr;
	m_pBoneToWorld = pBoneToWorld;

	// Bone to world must be set before calling AddDecal; it uses that here
	// UNDONE: Use current LOD to cull matrices here?
	ComputePoseToWorld( m_PoseToWorld, pStudioHdr, BONE_USED_BY_ANYTHING, m_pRC->m_ViewOrigin, m_pBoneToWorld );

	// Compute transforms from pose space to decal plane space
	if (!ComputePoseToDecal( ray, decalUp ))
	{
		m_pStudioHdr = NULL;
		m_pRC = NULL;
		m_pBoneToWorld = NULL;
		return;
	}

	// Get dynamic information from the material (fade start, fade time)
	float fadeStartTime	= 0.0f;
	float fadeDuration = 0.0f;
	int flags = 0;

	// This sucker is state needed only when building decals
	DecalBuildInfo_t buildInfo;
	buildInfo.m_Radius = radius;
	buildInfo.m_NoPokeThru = noPokethru;
	buildInfo.m_pStudioHdr = pStudioHdr;
 	buildInfo.m_UseClipVert = ( m_pStudioHdr->numbones <= 1 ) && ( m_pStudioHdr->numflexdesc == 0 );
	buildInfo.m_nGlobalMeshIndex = 0;
	buildInfo.m_pMeshVertexData = NULL;

	// Find out which LODs we're defacing
	int iMaxLOD;
	if ( maxLODToDecal == ADDDECAL_TO_ALL_LODS )
	{
		iMaxLOD = list.m_pHardwareData->m_NumLODs;
	}
	else 
	{
		iMaxLOD = min( list.m_pHardwareData->m_NumLODs, maxLODToDecal );
	}

	// Allocate space for all projected mesh vertices. We do this to prevent
	// re-projection of the same meshes when they appear in multiple LODs
	int nMeshCount = ComputeTotalMeshCount( list.m_pHardwareData->m_RootLOD, iMaxLOD-1, body );

	// NOTE: This is a consequence of the sizeof (m_UniqueID)
	if ( nMeshCount >= 255 )
	{
		Warning("Unable to apply decals to model (%s), it has more than 255 unique meshes!\n", m_pStudioHdr->pszName() );
		m_pStudioHdr = NULL;
		m_pRC = NULL;
		m_pBoneToWorld = NULL;
		return;
	}

	if ( !IsX360() )
	{
		buildInfo.m_pMeshVertices = (MeshVertexInfo_t*)stackalloc( nMeshCount * sizeof(MeshVertexInfo_t) );	
		int nVertexCount = ComputeVertexAllocation( iMaxLOD, body, list.m_pHardwareData, buildInfo.m_pMeshVertices );
		buildInfo.m_pVertexBuffer = (DecalBuildVertexInfo_t*)stackalloc( nVertexCount * sizeof(DecalBuildVertexInfo_t) );
	}
	else
	{
		// Don't allocate on the stack
		buildInfo.m_pMeshVertices = (MeshVertexInfo_t*)malloc( nMeshCount * sizeof(MeshVertexInfo_t) );	
		int nVertexCount = ComputeVertexAllocation( iMaxLOD, body, list.m_pHardwareData, buildInfo.m_pMeshVertices );
		buildInfo.m_pVertexBuffer = (DecalBuildVertexInfo_t*)malloc( nVertexCount * sizeof(DecalBuildVertexInfo_t) );
	}

	// Project all mesh vertices
	ProjectDecalsOntoMeshes( buildInfo, nMeshCount );

	if ( IsX360() )
	{
		while ( g_nTotalDecalVerts * sizeof(DecalVertex_t) > 256*1024 && m_DecalLRU.Head() != m_DecalLRU.InvalidIndex() )
		{
			DecalId_t nRetireID = m_DecalLRU[ m_DecalLRU.Head() ].m_nDecalId;
			StudioDecalHandle_t hRetire = m_DecalLRU[ m_DecalLRU.Head() ].m_hDecalHandle;
			DecalModelList_t &modelList = m_DecalList[(int)hRetire];
			RetireDecal( modelList, nRetireID, modelList.m_pHardwareData->m_RootLOD, modelList.m_pHardwareData->m_NumLODs );
		}
	}

	// Check to see if we have too many decals on this model
	// This assumes that every decal is applied to the root lod at least 
	int nRootLOD = list.m_pHardwareData->m_RootLOD;
	int nFinalLOD = list.m_pHardwareData->m_NumLODs;
	DecalHistoryList_t *pHistoryList = &list.m_pLod[list.m_pHardwareData->m_RootLOD].m_DecalHistory;
	if ( m_DecalLRU.Count() >= m_pRC->m_Config.maxDecalsPerModel * 1.5 )
	{
		DecalId_t nRetireID = m_DecalLRU[ m_DecalLRU.Head() ].m_nDecalId;
		StudioDecalHandle_t hRetire = m_DecalLRU[ m_DecalLRU.Head() ].m_hDecalHandle;
		DecalModelList_t &modelList = m_DecalList[(int)hRetire];
		RetireDecal( modelList, nRetireID, modelList.m_pHardwareData->m_RootLOD, modelList.m_pHardwareData->m_NumLODs );
	}

	if ( pHistoryList->Count() >= m_pRC->m_Config.maxDecalsPerModel )
	{
		DecalHistory_t *pDecalHistory = &pHistoryList->Element( pHistoryList->Head() );
		DecalId_t nRetireID = pDecalHistory->m_nId;
		StudioDecalHandle_t hRetire = hDecal;
		RetireDecal( m_DecalList[(int)hRetire], nRetireID, nRootLOD, nFinalLOD );
	}

	// Search all LODs for an overflow condition and retire those also
	for ( int i = iMaxLOD-1; i >= list.m_pHardwareData->m_RootLOD; i-- )
	{
		// Grab the list of all decals using the same material for this lod...
		int materialIdx = GetDecalMaterial( list.m_pLod[i], pDecalMaterial );

		// Check to see if we should retire the decal
		DecalMaterial_t *pDecalMaterial = &m_DecalMaterial[materialIdx];
		while ( pDecalMaterial->m_Indices.Count() > MAX_DECAL_INDICES_PER_MODEL )
		{
			DecalHistoryList_t *pHistoryList = &list.m_pLod[i].m_DecalHistory;
			DecalHistory_t *pDecalHistory = &pHistoryList->Element( pHistoryList->Head() );
			RetireDecal( list, pDecalHistory->m_nId, nRootLOD, nFinalLOD );
		}
	}	

	// Gotta do this for all LODs
	bool bAddedDecals = false;
	for ( int i = iMaxLOD-1; i >= list.m_pHardwareData->m_RootLOD; i-- )
	{
		// Grab the list of all decals using the same material for this lod...
		int materialIdx = GetDecalMaterial( list.m_pLod[i], pDecalMaterial );
		buildInfo.m_pDecalMaterial = &m_DecalMaterial[materialIdx];

		// Grab the meshes for this lod
		m_pStudioMeshes = list.m_pHardwareData->m_pLODs[i].m_pMeshData;

		// Don't decal on meshes that are translucent if it's twopass
		buildInfo.m_ppMaterials = list.m_pHardwareData->m_pLODs[i].ppMaterials;

		// Set up info needed for vertex sharing
		buildInfo.m_FirstVertex = buildInfo.m_pDecalMaterial->m_Vertices.InvalidIndex();
		buildInfo.m_VertexCount = 0;

		int prevIndexCount = buildInfo.m_pDecalMaterial->m_Indices.Count();

		// Step over all body parts + add decals to em all!
		int k;
		for ( k=0 ; k < m_pStudioHdr->numbodyparts ; k++) 
		{
			// Grab the model for this body part
			int model = R_StudioSetupModel( k, body, &m_pSubModel, m_pStudioHdr );
			buildInfo.m_Body = k;
			buildInfo.m_Model = model;
			if ( !AddDecalToModel( buildInfo ) )
				break;
		}

		if ( k != m_pStudioHdr->numbodyparts )
			continue;

		// Add this to the list of decals in this material
		if ( buildInfo.m_VertexCount )
		{
			bAddedDecals = true;

			int decalIndexCount = buildInfo.m_pDecalMaterial->m_Indices.Count() - prevIndexCount;
			Assert(decalIndexCount > 0);

			int decalIndex = AddDecalToMaterialList( buildInfo.m_pDecalMaterial );
			Decal_t& decal = buildInfo.m_pDecalMaterial->m_Decals[decalIndex];
			decal.m_VertexCount = buildInfo.m_VertexCount;
			decal.m_IndexCount = decalIndexCount;
			decal.m_FadeStartTime = fadeStartTime;
			decal.m_FadeDuration = fadeDuration; 
			decal.m_Flags = flags;

			// Add this decal to the history...
			int h = list.m_pLod[i].m_DecalHistory.AddToTail();
			list.m_pLod[i].m_DecalHistory[h].m_Material = materialIdx;
			list.m_pLod[i].m_DecalHistory[h].m_Decal = decalIndex;
			list.m_pLod[i].m_DecalHistory[h].m_nId = m_nDecalId;
			list.m_pLod[i].m_DecalHistory[h].m_nPad = 0;
		}
	}

	// Add to LRU
	if ( bAddedDecals )
	{
		DecalLRUListIndex_t h = m_DecalLRU.AddToTail();
		m_DecalLRU[h].m_nDecalId = m_nDecalId;
		m_DecalLRU[h].m_hDecalHandle = hDecal;

		// Increment count.
		++m_nDecalId;
	}

	if ( IsX360() )
	{
		free( buildInfo.m_pMeshVertices );
		free( buildInfo.m_pVertexBuffer );
	}

	m_pStudioHdr = NULL;
	m_pRC = NULL;
	m_pBoneToWorld = NULL;
}


//-----------------------------------------------------------------------------
//
// This code here is all about rendering the decals
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Inner loop for rendering decals that have a single bone
//-----------------------------------------------------------------------------

void CStudioRender::DrawSingleBoneDecals( CMeshBuilder& meshBuilder, DecalMaterial_t& decalMaterial )
{
	// We don't got no bones, so yummy yummy yum, just copy the data out
	// Static props should go though this code path

	DecalVertexList_t& verts = decalMaterial.m_Vertices; 
	for ( DecalVertexList_t::IndexLocalType_t i = verts.Head(); i != verts.InvalidIndex(); i = verts.Next(i) )
	{
		DecalVertex_t& vertex = verts[i];
		
		meshBuilder.Position3fv( vertex.m_Position.Base() );
		meshBuilder.Normal3fv( GetVecNormal( vertex.m_Normal ).Base() );
#if 0
		if ( decalMaterial.m_pMaterial->InMaterialPage() )
		{
			float offset[2], scale[2];
			decalMaterial.m_pMaterial->GetMaterialOffset( offset );
			decalMaterial.m_pMaterial->GetMaterialScale( scale );

			Vector2D vecTexCoord( vertex.m_TexCoord.x, vertex.m_TexCoord.y );
			vecTexCoord.x = clamp( vecTexCoord.x, 0.0f, 1.0f );
			vecTexCoord.y = clamp( vecTexCoord.y, 0.0f, 1.0f );
			meshBuilder.TexCoordSubRect2f( 0, vecTexCoord.x, vecTexCoord.y, offset[0], offset[1], scale[0], scale[1] );

//			meshBuilder.TexCoordSubRect2f( 0, vertex.m_TexCoord.x, vertex.m_TexCoord.y, offset[0], offset[1], scale[0], scale[1] );
		}
		else
#endif
		{
			meshBuilder.TexCoord2fv( 0, GetVecTexCoord(vertex.m_TexCoord).Base() );
		}
		meshBuilder.Color4ub( 255, 255, 255, 255 );

		if ( meshBuilder.NumBoneWeights() > 0 )	// bone weight of 0 will not write anything, so these calls would be wasted
		{	
			meshBuilder.BoneWeight( 0, 1.0f );
			meshBuilder.BoneWeight( 1, 0.0f );
			meshBuilder.BoneWeight( 2, 0.0f );
			meshBuilder.BoneWeight( 3, 0.0f );
		}

		meshBuilder.BoneMatrix( 0, 0 );
		meshBuilder.BoneMatrix( 1, 0 );
		meshBuilder.BoneMatrix( 2, 0 );
		meshBuilder.BoneMatrix( 3, 0 );

		meshBuilder.AdvanceVertex();
	}
}

void CStudioRender::DrawSingleBoneFlexedDecals( IMatRenderContext *pRenderContext, CMeshBuilder& meshBuilder, DecalMaterial_t& decalMaterial )
{
	// We don't got no bones, so yummy yummy yum, just copy the data out
	// Static props should go though this code path
	DecalVertexList_t& verts = decalMaterial.m_Vertices; 
	for ( DecalVertexList_t::IndexLocalType_t i = verts.Head(); i != verts.InvalidIndex(); i = verts.Next(i) )
	{
		DecalVertex_t& vertex = verts[i];

		// Clipped verts shouldn't come through here, only static props should use clipped
		Assert ( vertex.m_MeshVertexIndex >= 0 );

		m_VertexCache.SetBodyModelMesh( vertex.m_Body, vertex.m_Model, vertex.m_Mesh );
		if (m_VertexCache.IsVertexFlexed( vertex.m_MeshVertexIndex ))
		{
			CachedPosNormTan_t* pFlexedVertex = m_VertexCache.GetFlexVertex( vertex.m_MeshVertexIndex );
			meshBuilder.Position3fv( pFlexedVertex->m_Position.Base() );
			meshBuilder.Normal3fv( pFlexedVertex->m_Normal.Base() );
		}
		else
		{
			meshBuilder.Position3fv( vertex.m_Position.Base() );
			meshBuilder.Normal3fv( GetVecNormal( vertex.m_Normal ).Base() );
		}

#if 0
		if ( decalMaterial.m_pMaterial->InMaterialPage() )
		{
			float offset[2], scale[2];
			decalMaterial.m_pMaterial->GetMaterialOffset( offset );
			decalMaterial.m_pMaterial->GetMaterialScale( scale );

			Vector2D vecTexCoord( vertex.m_TexCoord.x, vertex.m_TexCoord.y );
			vecTexCoord.x = clamp( vecTexCoord.x, 0.0f, 1.0f );
			vecTexCoord.y = clamp( vecTexCoord.y, 0.0f, 1.0f );
			meshBuilder.TexCoordSubRect2f( 0, vecTexCoord.x, vecTexCoord.y, offset[0], offset[1], scale[0], scale[1] );

//			meshBuilder.TexCoordSubRect2f( 0, vertex.m_TexCoord.x, vertex.m_TexCoord.y, offset[0], offset[1], scale[0], scale[1] );
		}
		else
#endif
		{
			meshBuilder.TexCoord2fv( 0, GetVecTexCoord(vertex.m_TexCoord).Base() );
		}

		meshBuilder.Color4ub( 255, 255, 255, 255 );

		if ( meshBuilder.NumBoneWeights() > 0 )	// bone weight of 0 will not write anything, so these calls would be wasted
		{	
			meshBuilder.BoneWeight( 0, 1.0f );
			meshBuilder.BoneWeight( 1, 0.0f );
			meshBuilder.BoneWeight( 2, 0.0f );
			meshBuilder.BoneWeight( 3, 0.0f );
		}
		
		meshBuilder.BoneMatrix( 0, 0 );
		meshBuilder.BoneMatrix( 1, 0 );
		meshBuilder.BoneMatrix( 2, 0 );
		meshBuilder.BoneMatrix( 3, 0 );

		meshBuilder.AdvanceVertex();
	}
}

//-----------------------------------------------------------------------------
// Inner loop for rendering decals that have multiple bones
//-----------------------------------------------------------------------------
bool CStudioRender::DrawMultiBoneDecals( CMeshBuilder& meshBuilder, DecalMaterial_t& decalMaterial, studiohdr_t *pStudioHdr )
{
	const thinModelVertices_t		*thinVertData	= NULL;
	const mstudio_meshvertexdata_t	*vertData		= NULL;
	mstudiomesh_t					*pLastMesh		= NULL;

	DecalVertexList_t& verts = decalMaterial.m_Vertices; 
	for ( DecalVertexList_t::IndexLocalType_t i = verts.Head(); i != verts.InvalidIndex(); i = verts.Next(i) )
	{
		DecalVertex_t& vertex = verts[i];
		
		int n = vertex.m_MeshVertexIndex;

		Assert( n < MAXSTUDIOVERTS );

		mstudiomesh_t *pMesh = vertex.GetMesh( pStudioHdr );
		Assert( pMesh );

		m_VertexCache.SetBodyModelMesh( vertex.m_Body, vertex.m_Model, vertex.m_Mesh );
		if (m_VertexCache.IsVertexPositionCached( n ))
		{
			CachedPosNorm_t* pCachedVert = m_VertexCache.GetWorldVertex( n );
			meshBuilder.Position3fv( pCachedVert->m_Position.Base() );
			meshBuilder.Normal3fv( pCachedVert->m_Normal.Base() );
		}
		else
		{
			// Prevent the computation of this again....
			m_VertexCache.SetupComputation(pMesh);
			CachedPosNorm_t* pCachedVert = m_VertexCache.CreateWorldVertex( n );

			if ( pLastMesh != pMesh )
			{
				// only if the mesh changes
				pLastMesh = pMesh;
				vertData  = pMesh->GetVertexData( pStudioHdr );
				if ( vertData )
					thinVertData = NULL;
				else
					thinVertData = pMesh->GetThinVertexData( pStudioHdr );
			}

			if ( vertData )
			{
				mstudioboneweight_t* pBoneWeights = vertData->BoneWeights( n );
				// FIXME: could be faster to blend the matrices and then transform the pos+norm by the same matrix
				R_StudioTransform( *vertData->Position( n ), pBoneWeights, pCachedVert->m_Position.AsVector3D() );
				R_StudioRotate( *vertData->Normal( n ), pBoneWeights, pCachedVert->m_Normal.AsVector3D() );
			}
			else if ( thinVertData )
			{
				// Using compressed vertex data
				mstudioboneweight_t boneWeights;
				Vector position;
				Vector normal;
				thinVertData->GetMeshBoneWeights( pMesh, n, &boneWeights );
				thinVertData->GetMeshPosition( pMesh, n, &position );
				thinVertData->GetMeshNormal( pMesh, n, &normal );
				R_StudioTransform( position, &boneWeights, pCachedVert->m_Position.AsVector3D() );
				R_StudioRotate( normal, &boneWeights, pCachedVert->m_Normal.AsVector3D() );
			}
			else
			{
				return false;
			}

			// Add a little extra offset for hardware skinning; in that case
			// we're doing software skinning for decals and it might not be quite right
			VectorMA( pCachedVert->m_Position.AsVector3D(), 0.1, pCachedVert->m_Normal.AsVector3D(), pCachedVert->m_Position.AsVector3D() );

			meshBuilder.Position3fv( pCachedVert->m_Position.Base() );
			meshBuilder.Normal3fv( pCachedVert->m_Normal.Base() );
		}

#if 0
		if ( decalMaterial.m_pMaterial->InMaterialPage() )
		{
			float offset[2], scale[2];
			decalMaterial.m_pMaterial->GetMaterialOffset( offset );
			decalMaterial.m_pMaterial->GetMaterialScale( scale );

			Vector2D vecTexCoord( vertex.m_TexCoord.x, vertex.m_TexCoord.y );
			vecTexCoord.x = clamp( vecTexCoord.x, 0.0f, 1.0f );
			vecTexCoord.y = clamp( vecTexCoord.y, 0.0f, 1.0f );
			meshBuilder.TexCoordSubRect2f( 0, vecTexCoord.x, vecTexCoord.y, offset[0], offset[1], scale[0], scale[1] );

//			meshBuilder.TexCoordSubRect2f( 0, vertex.m_TexCoord.x, vertex.m_TexCoord.y, offset[0], offset[1], scale[0], scale[1] );
		}
		else
#endif
		{
			meshBuilder.TexCoord2fv( 0, GetVecTexCoord(vertex.m_TexCoord).Base() );
		}

		meshBuilder.Color4ub( 255, 255, 255, 255 );

		if ( meshBuilder.NumBoneWeights() > 0 )	// bone weight of 0 will not write anything, so these calls would be wasted
		{	
			meshBuilder.BoneWeight( 0, 1.0f );
			meshBuilder.BoneWeight( 1, 0.0f );
			meshBuilder.BoneWeight( 2, 0.0f );
			meshBuilder.BoneWeight( 3, 0.0f );
		}
		
		meshBuilder.BoneMatrix( 0, 0 );
		meshBuilder.BoneMatrix( 1, 0 );
		meshBuilder.BoneMatrix( 2, 0 );
		meshBuilder.BoneMatrix( 3, 0 );

		meshBuilder.AdvanceVertex();
	}
	return true;
}

bool CStudioRender::DrawMultiBoneFlexedDecals( IMatRenderContext *pRenderContext, CMeshBuilder& meshBuilder, 
	DecalMaterial_t& decalMaterial, studiohdr_t *pStudioHdr, studioloddata_t *pStudioLOD )
{
	int *pBoneRemap = pStudioLOD ? pStudioLOD->m_pHWMorphDecalBoneRemap : NULL;

	mstudiomesh_t *pLastMesh = NULL;
	const mstudio_meshvertexdata_t *vertData = NULL;

	DecalVertexList_t& verts = decalMaterial.m_Vertices; 
	for ( DecalVertexList_t::IndexLocalType_t i = verts.Head(); i != verts.InvalidIndex(); i = verts.Next(i) )
	{
		DecalVertex_t& vertex = verts[i];
		
		int n = vertex.m_MeshVertexIndex;

		mstudiomesh_t *pMesh = vertex.GetMesh( pStudioHdr );
		Assert( pMesh );

		if ( pLastMesh != pMesh )
		{
			// only if the mesh changes
			pLastMesh = pMesh;
			vertData  = pMesh->GetVertexData( pStudioHdr );
		}

		if ( !vertData )
			return false;

		IMorph *pMorph = pBoneRemap ? vertex.GetMorph( m_pStudioHdr, m_pStudioMeshes ) : NULL;
		Vector2D morphUV;
		if ( pMorph )
		{
			Assert( pBoneRemap );
			Assert( vertex.m_GroupIndex != 0xFFFF );
			if ( !pRenderContext->GetMorphAccumulatorTexCoord( &morphUV, pMorph, vertex.m_GroupIndex ) )
			{
				pMorph = NULL;
			}
		}

		if ( !pMorph )
		{
			mstudioboneweight_t* pBoneWeights = vertData->BoneWeights( n );
			m_VertexCache.SetBodyModelMesh( vertex.m_Body, vertex.m_Model, vertex.m_Mesh );

			if ( m_VertexCache.IsVertexPositionCached( n ) )
			{
				CachedPosNorm_t* pCachedVert = m_VertexCache.GetWorldVertex( n );
				meshBuilder.Position3fv( pCachedVert->m_Position.Base() );
				meshBuilder.Normal3fv( pCachedVert->m_Normal.Base() );
			}
			else
			{
				// Prevent the computation of this again....
				m_VertexCache.SetupComputation(pMesh);
				CachedPosNorm_t* pCachedVert = m_VertexCache.CreateWorldVertex( n );

				if (m_VertexCache.IsThinVertexFlexed( n ))
				{
					CachedPosNorm_t* pFlexedVertex = m_VertexCache.GetThinFlexVertex( n );
					Vector vecPosition, vecNormal;
					VectorAdd( *vertData->Position( n ), pFlexedVertex->m_Position.AsVector3D(), vecPosition );
					VectorAdd( *vertData->Normal( n ), pFlexedVertex->m_Normal.AsVector3D(), vecNormal );
					R_StudioTransform( vecPosition, pBoneWeights, pCachedVert->m_Position.AsVector3D() );
					R_StudioRotate( vecNormal, pBoneWeights, pCachedVert->m_Normal.AsVector3D() );
					VectorNormalize( pCachedVert->m_Normal.AsVector3D() );
				}
				else if (m_VertexCache.IsVertexFlexed( n ))
				{
					CachedPosNormTan_t* pFlexedVertex = m_VertexCache.GetFlexVertex( n );
					R_StudioTransform( pFlexedVertex->m_Position, pBoneWeights, pCachedVert->m_Position.AsVector3D() );
					R_StudioRotate( pFlexedVertex->m_Normal, pBoneWeights, pCachedVert->m_Normal.AsVector3D() );
				}
				else
				{
					Assert( pMesh );
					R_StudioTransform( *vertData->Position( n ), pBoneWeights, pCachedVert->m_Position.AsVector3D() );
					R_StudioRotate( *vertData->Normal( n ), pBoneWeights, pCachedVert->m_Normal.AsVector3D() );
				}

				// Add a little extra offset for hardware skinning; in that case
				// we're doing software skinning for decals and it might not be quite right
				VectorMA( pCachedVert->m_Position.AsVector3D(), 0.1, pCachedVert->m_Normal.AsVector3D(), pCachedVert->m_Position.AsVector3D() );

				meshBuilder.Position3fv( pCachedVert->m_Position.Base() );
				meshBuilder.Normal3fv( pCachedVert->m_Normal.Base() );
			}

			meshBuilder.Color4ub( 255, 255, 255, 255 );
			meshBuilder.TexCoord2fv( 0, GetVecTexCoord( vertex.m_TexCoord ).Base() );
			meshBuilder.TexCoord3f( 2, 0.0f, 0.0f, 0.0f );

			// NOTE: Even if HW morphing is active, since we're using bone 0, it will multiply by identity in the shader
			if ( meshBuilder.NumBoneWeights() > 0 )	// bone weight of 0 will not write anything, so these calls would be wasted
			{	
				meshBuilder.BoneWeight( 0, 1.0f );	
				meshBuilder.BoneWeight( 1, 0.0f );
				meshBuilder.BoneWeight( 2, 0.0f );
				meshBuilder.BoneWeight( 3, 0.0f );
			}
			
			meshBuilder.BoneMatrix( 0, 0 );
			meshBuilder.BoneMatrix( 1, 0 );
			meshBuilder.BoneMatrix( 2, 0 );
			meshBuilder.BoneMatrix( 3, 0 );
		}
		else
		{
			meshBuilder.Position3fv( vertData->Position( n )->Base() );
			meshBuilder.Normal3fv( vertData->Normal( n )->Base() );
			meshBuilder.Color4ub( 255, 255, 255, 255 );
			meshBuilder.TexCoord2fv( 0, GetVecTexCoord( vertex.m_TexCoord ).Base() );
			meshBuilder.TexCoord3f( 2, morphUV.x, morphUV.y, 1.0f );

			// NOTE: We should be renormalizing bone weights here like R_AddVertexToMesh does.. 
			// It's too expensive. Tough noogies.
			mstudioboneweight_t* pBoneWeights = vertData->BoneWeights( n );
			Assert( pBoneWeights->numbones <= 3 );
			meshBuilder.BoneWeight( 0, pBoneWeights->weight[ 0 ] );	
			meshBuilder.BoneWeight( 1, pBoneWeights->weight[ 1 ] );
			meshBuilder.BoneWeight( 2, 1.0f - pBoneWeights->weight[ 1 ] - pBoneWeights->weight[ 0 ] );
			meshBuilder.BoneWeight( 3, 0.0f );
			meshBuilder.BoneMatrix( 0, pBoneRemap[ (unsigned)pBoneWeights->bone[0] ] );
			meshBuilder.BoneMatrix( 1, pBoneRemap[ (unsigned)pBoneWeights->bone[1] ] );
			meshBuilder.BoneMatrix( 2, pBoneRemap[ (unsigned)pBoneWeights->bone[2] ] );
			meshBuilder.BoneMatrix( 3, BONE_MATRIX_INDEX_INVALID );
		}

		meshBuilder.AdvanceVertex();
	}
	return true;
}

//-----------------------------------------------------------------------------
// Draws all the decals using a particular material
//-----------------------------------------------------------------------------
void CStudioRender::DrawDecalMaterial( IMatRenderContext *pRenderContext, DecalMaterial_t& decalMaterial, studiohdr_t *pStudioHdr, studioloddata_t *pStudioLOD )
{
	// Performance analysis.
//	VPROF_BUDGET( "Decals", "Decals" );
	VPROF( "DecalsDrawStudio" );

	// It's possible for the index count to become zero due to decal retirement
	int indexCount = decalMaterial.m_Indices.Count();
	if ( indexCount == 0 )
		return;

	if ( !m_pRC->m_Config.m_bEnableHWMorph )
	{
		pStudioLOD = NULL;
	}

	bool bUseHWMorphing = ( pStudioLOD && ( pStudioLOD->m_pHWMorphDecalBoneRemap != NULL ) );
	if ( bUseHWMorphing )
	{
		pRenderContext->BindMorph( MATERIAL_MORPH_DECAL );
	}

	// Bind the decal material
	if ( !m_pRC->m_Config.bWireframeDecals )
	{
		pRenderContext->Bind( decalMaterial.m_pMaterial );
	}
	else
	{
		pRenderContext->Bind( m_pMaterialMRMWireframe );
	}

	// Use a dynamic mesh...
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	int vertexCount = decalMaterial.m_Vertices.Count();

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, vertexCount, indexCount );

	// FIXME: Could make static meshes for these?
	// But don't make no static meshes for decals that fade, at least

	// Two possibilities: no/one bones, we let the hardware do all transformation
	// or, more than one bone, we do software skinning.
	bool bDraw = true;
	if ( m_pStudioHdr->numbones <= 1 )
	{
		if ( m_pStudioHdr->numflexdesc != 0 )
		{
			DrawSingleBoneFlexedDecals( pRenderContext, meshBuilder, decalMaterial );
		}
		else
		{
			DrawSingleBoneDecals( meshBuilder, decalMaterial );
		}
	}
	else
	{
		if ( m_pStudioHdr->numflexdesc != 0 )
		{
			if ( !DrawMultiBoneFlexedDecals( pRenderContext, meshBuilder, decalMaterial, pStudioHdr, pStudioLOD ) )
			{
				bDraw = false;
			}
		}
		else
		{
			if ( !DrawMultiBoneDecals( meshBuilder, decalMaterial, pStudioHdr ) )
			{
				bDraw = false;
			}
		}
	}

	// Set the indices
	// This is a little tricky. Because we can retire decals, the indices
	// for each decal start at 0. We output all the vertices in order of
	// each decal, and then fix up the indices based on how many vertices
	// we wrote out for the decals
	unsigned short decal = decalMaterial.m_Decals.Head();
	int indicesRemaining = decalMaterial.m_Decals[decal].m_IndexCount;
	int vertexOffset = 0;
	for ( int i = 0; i < indexCount; ++i)
	{
		meshBuilder.Index( decalMaterial.m_Indices[i] + vertexOffset ); 
		meshBuilder.AdvanceIndex();
		if (--indicesRemaining <= 0)
		{
			vertexOffset += decalMaterial.m_Decals[decal].m_VertexCount;
			decal = decalMaterial.m_Decals.Next(decal); 
			if (decal != decalMaterial.m_Decals.InvalidIndex())
			{
				indicesRemaining = decalMaterial.m_Decals[decal].m_IndexCount;
			}
#ifdef _DEBUG
			else
			{
				Assert( i + 1 == indexCount );
			}
#endif
		}
	}

	meshBuilder.End();
	if ( bDraw )
	{
		pMesh->Draw();
	}
	else
	{
		pMesh->MarkAsDrawn();
	}

	if ( bUseHWMorphing )
	{
		pRenderContext->BindMorph( NULL );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Setup the render state for decals if object has lighting baked.
//-----------------------------------------------------------------------------
static Vector s_pWhite[6] = 
{
	Vector( 1.0, 1.0, 1.0 ),
	Vector( 1.0, 1.0, 1.0 ),
	Vector( 1.0, 1.0, 1.0 ),
	Vector( 1.0, 1.0, 1.0 ),
	Vector( 1.0, 1.0, 1.0 ),
	Vector( 1.0, 1.0, 1.0 )
};

bool CStudioRender::PreDrawDecal( IMatRenderContext *pRenderContext, const DrawModelInfo_t &drawInfo )
{
	if ( !drawInfo.m_bStaticLighting )
		return false;

	// FIXME: This is incredibly bogus, 
	// it's overwriting lighting state in the context without restoring it!
	const Vector *pAmbient;
	if ( m_pRC->m_Config.fullbright )
	{			
		pAmbient = s_pWhite;
		m_pRC->m_NumLocalLights = 0;
	}
	else
	{
		pAmbient = drawInfo.m_vecAmbientCube;
		m_pRC->m_NumLocalLights = CopyLocalLightingState( MAXLOCALLIGHTS, m_pRC->m_LocalLights,
			drawInfo.m_nLocalLightCount, drawInfo.m_LocalLightDescs );
	}

	for( int i = 0; i < 6; i++ )
	{
		VectorCopy( pAmbient[i], m_pRC->m_LightBoxColors[i].AsVector3D() );
		m_pRC->m_LightBoxColors[i][3] = 1.0f;
	}

	SetLightingRenderState();
	return true;
}


//-----------------------------------------------------------------------------
// Draws all the decals on a particular model
//-----------------------------------------------------------------------------
void CStudioRender::DrawDecal( const DrawModelInfo_t &drawInfo, int lod, int body )
{
	StudioDecalHandle_t handle = drawInfo.m_Decals;
	if ( handle == STUDIORENDER_DECAL_INVALID )
		return;

	VPROF("CStudioRender::DrawDecal");

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	PreDrawDecal( pRenderContext, drawInfo );

	// All decal vertex data is are stored in pose space
	// So as long as the pose-to-world transforms are set, we're all ready!

	// FIXME: Body stuff isn't hooked in at all for decals

	// Get the decal list for this lod
	const DecalModelList_t& list = m_DecalList[(int)handle];
	m_pStudioHdr = drawInfo.m_pStudioHdr;

	// Add this fix after I fix the other problem.
	studioloddata_t *pStudioLOD = NULL;
	if ( m_pStudioHdr->numbones <= 1 )
	{
		pRenderContext->SetNumBoneWeights( m_pStudioHdr->numbones );
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->LoadMatrix( m_PoseToWorld[0] );
	}
	else
	{
		pStudioLOD = &drawInfo.m_pHardwareData->m_pLODs[lod];
		if ( !m_pRC->m_Config.m_bEnableHWMorph || !pStudioLOD->m_pHWMorphDecalBoneRemap )
		{
			pRenderContext->SetNumBoneWeights( 0 );
			pRenderContext->MatrixMode( MATERIAL_MODEL );
			pRenderContext->LoadIdentity( );
		}
		else
		{
			// Set up skinning for decal rendering with hw morphs
			pRenderContext->SetNumBoneWeights( pStudioLOD->m_nDecalBoneCount );

			// Bone 0 is always identity; necessary to multiple against non hw-morphed verts
			matrix3x4_t identity;
			SetIdentityMatrix( identity );
			pRenderContext->LoadBoneMatrix( 0, identity );

			// Set up the bone state from the mapping computed in ComputeHWMorphDecalBoneRemap
			for ( int i = 0; i < m_pStudioHdr->numbones; ++i )
			{
				int nHWBone = pStudioLOD->m_pHWMorphDecalBoneRemap[i];
				if ( nHWBone <= 0 )
					continue;

				pRenderContext->LoadBoneMatrix( nHWBone, m_PoseToWorld[i] );
			}
		}
	}

	// Gotta do this for all LODs
	// Draw each set of decals using a particular material
	unsigned short mat = list.m_pLod[lod].m_FirstMaterial;
	for ( ; mat != m_DecalMaterial.InvalidIndex(); mat = m_DecalMaterial.Next(mat))
	{
		DecalMaterial_t& decalMaterial = m_DecalMaterial[mat];
		DrawDecalMaterial( pRenderContext, decalMaterial, m_pStudioHdr, pStudioLOD );
	}
}


void CStudioRender::DrawStaticPropDecals( const DrawModelInfo_t &drawInfo, const StudioRenderContext_t &rc, const matrix3x4_t &modelToWorld )
{
	StudioDecalHandle_t handle = drawInfo.m_Decals;
	if (handle == STUDIORENDER_DECAL_INVALID)
		return;

	m_pRC = const_cast< StudioRenderContext_t* >( &rc );

	VPROF("CStudioRender::DrawStaticPropDecals");
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	PreDrawDecal( pRenderContext, drawInfo );

	// All decal vertex data is are stored in pose space
	// So as long as the pose-to-world transforms are set, we're all ready!

	// FIXME: Body stuff isn't hooked in at all for decals

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->LoadMatrix( modelToWorld );

	const DecalModelList_t& list = m_DecalList[(int)handle];
	// Gotta do this for all LODs
	// Draw each set of decals using a particular material
	unsigned short mat = list.m_pLod[drawInfo.m_Lod].m_FirstMaterial;
	for ( ; mat != m_DecalMaterial.InvalidIndex(); mat = m_DecalMaterial.Next(mat))
	{
		DecalMaterial_t& decalMaterial = m_DecalMaterial[mat];
		DrawDecalMaterial( pRenderContext, decalMaterial, drawInfo.m_pStudioHdr, NULL );
	}

	m_pRC = NULL;
}


//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#pragma warning( disable : 4786 )

#pragma warning( disable : 4748 )		// buffer overrun with optimizations off
// This file has tons of problems with global optimizations. . turn 'em off.
// NOTE: Would be nice to have a test case for this! - not verified in vs2005
#pragma optimize( "g", off )

// use this much memory to build an output file in memory.
#define FILEBUFFER_SIZE ( 4 * 1024 * 1024 )

//#define IGNORE_BONES

#define NVTRISTRIP

#define EMIT_TRILISTS

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include "mathlib/mathlib.h"
#include "cmdlib.h"
#include "studio.h"
#include "studiomdl.h"
#include "HardwareMatrixState.h"
#include "HardwareVertexCache.h"
#include "optimize.h"
#include <malloc.h>
#include <nvtristrip.h>
#include "FileBuffer.h"
#include "tier1/utlvector.h"
#include "materialsystem/imaterial.h"
#include "tier1/utllinkedlist.h"

#include "tier1/smartptr.h"
#include "tier2/p4helpers.h"

bool g_bDumpGLViewFiles;
extern bool g_IHVTest;

// flush bones between strips rather than deallocating the LRU.
#define USE_FLUSH

extern int FindMaterialByName( const char *pMaterialName );

namespace OptimizedModel
{

//-----------------------------------------------------------------------------
// Defines which debugging output file we will see
//-----------------------------------------------------------------------------

enum 
{ 
	WRITEGLVIEW_SHOWMESH			= 0x00000001,
	WRITEGLVIEW_SHOWSTRIPGROUP		= 0x00000002,
	WRITEGLVIEW_SHOWSTRIP			= 0x00000004,
	WRITEGLVIEW_SHOWSUBSTRIP		= 0x00000008,
	WRITEGLVIEW_SHOWFLEXED			= 0x00000010,
	WRITEGLVIEW_SHOWSW				= 0x00000020,
	WRITEGLVIEW_SHOWMESHPROPS		= 0x00000040,
	WRITEGLVIEW_SHOWVERTNUMBONES	= 0x00000080,
	WRITEGLVIEW_SHOWSTRIPNUMBONES	= 0x00000100,
	WRITEGLVIEW_FORCEUINT			= 0xffffffff
};

//-----------------------------------------------------------------------------
// This is used to help us figure out where in the file data should go
// and also to display stats
//-----------------------------------------------------------------------------
struct TotalMeshStats_t
{
	int m_TotalBodyParts;
	int m_TotalModels;
	int m_TotalModelLODs;
	int m_TotalMeshes;
	int m_TotalStrips;
	int m_TotalStripGroups;
	int m_TotalVerts;
	int m_TotalIndices;
	int m_TotalBoneStateChanges;
	int m_TotalMaterialReplacements;
};

struct Triangle_t
{
	Triangle_t()
	{
		touched = false;
		neighborTriID[0] = neighborTriID[1] = neighborTriID[2] = -1;
	}
	int vertID[3];
	int neighborTriID[3];
	int boneID[MAX_NUM_BONES_PER_TRI];
	int numBones;
	bool touched;
};


//-----------------------------------------------------------------------------
// Associates software bone indices with hardware bone indices
//-----------------------------------------------------------------------------
struct BoneStateChange_t
{
	int hardwareID;
	int newBoneID;
};

struct Strip_t
{
	// these are the verts and indices that are used while building.
	// (there may be verts in here that aren't use in the stripGroup.)
	CUtlVector<Vertex_t> verts;
	int numIndices;
	unsigned short *pIndices;

	unsigned int flags;

	int numBones;

	// These are the final, sorted verts as they appear in the strip group.
	int stripGroupIndexOffset;  // offset into stripGroup's indices
	int numStripGroupIndices;
	int stripGroupVertexOffset; // offset into stripGroup's verts
	int numStripGroupVerts;

	int numBoneStateChanges;
	BoneStateChange_t boneStateChanges[MAX_NUM_BONES_PER_STRIP];
};


//-----------------------------------------------------------------------------
// a list of vertices + triangles for a strip group
//-----------------------------------------------------------------------------

typedef CUtlVector<Vertex_t>	VertexList_t;
typedef CUtlVector<Triangle_t>	TriangleList_t;
typedef CUtlVector<unsigned short>	VertexIndexList_t;
typedef CUtlVector<Strip_t>		StripList_t;
typedef CUtlVector<bool>		TriangleProcessedList_t;

//-----------------------------------------------------------------------------
// String table
//-----------------------------------------------------------------------------
class CStringTable
{
public:
	int StringTableOffset( const char *string )
	{
		int i;
		int size = 0;
		for( i = 0; i < m_Strings.Size(); i++ )
		{
			if( stricmp( m_Strings[i].Base(), string ) == 0 )
			{
				return size;
			}
			size += m_Strings[i].Size();
		}
		return -1;
	}
	bool StringPresent( const char *string )
	{
		int i;
		for( i = 0; i < m_Strings.Size(); i++ )
		{
			if( stricmp( m_Strings[i].Base(), string ) == 0 )
			{
				return true;
			}
		}
		return false;
	}
	void AddString( const char *newString )
	{
		if( StringPresent( newString ) )
		{
			return;
		}
		CUtlVector<char> &s = m_Strings[m_Strings.AddToTail()];
		int size = strlen( newString ) + 1;
		s.AddMultipleToTail( size );
		strcpy( s.Base(), newString );
	}
	void Purge()
	{
		m_Strings.Purge();
	}
	int CalcSize( void )
	{
		int size = 0;
		int i;
		for( i = 0; i < m_Strings.Size(); i++ )
		{
			size += m_Strings[i].Size();
		}
		return size;
	}
	void WriteToMem( char *pDst )
	{
		int size = 0;
		int i;
		for( i = 0; i < m_Strings.Size(); i++ )
		{
#ifdef _DEBUG
			int j = Q_strlen( m_Strings[i].Base() ) + 1;
			int k = m_Strings[i].Size();
			Assert( j == k );
#endif
			memcpy( pDst + size, m_Strings[i].Base(), m_Strings[i].Size() );
			size += m_Strings[i].Size();
		}
	}
private:
	typedef CUtlVector<char> CharVector_t;
	CUtlVector<CharVector_t> m_Strings;
};

// global string table for the whole vtx file.
static CStringTable s_StringTable;

//-----------------------------------------------------------------------------
// This is all the indices, vertices, and strips that make up this group
// a group can be rendered all in one call to the material system
//-----------------------------------------------------------------------------

struct StripGroup_t
{
	VertexIndexList_t	indices;
	VertexList_t		verts;
	StripList_t			strips;
	unsigned int flags;
};

struct Mesh_t
{
	CUtlVector<StripGroup_t> stripGroups;
	unsigned int flags;
};

struct ModelLOD_t
{
	CUtlVector<Mesh_t> meshes;
	float switchPoint;
};

struct Model_t
{
	CUtlVector<ModelLOD_t> modelLODs;
};

//-----------------------------------------------------------------------------
// Main class that does all the dirty work to stripy + groupify
//-----------------------------------------------------------------------------

class COptimizedModel
{
public:
	bool OptimizeFromStudioHdr( studiohdr_t *phdr, s_bodypart_t *pSrcBodyParts, int vertCacheSize, 
		bool usesFixedFunction, bool bForceSoftwareSkin, bool bHWFlex, int maxBonesPerVert, int maxBonesPerTri, 
		int maxBonesPerStrip, const char *fileName, const char *glViewFileName );

private:
	void CleanupEverything();

	// Setup to get the ball rolling
	void SetupMeshProcessing( studiohdr_t *phdr, int vertCacheSize,  
			bool usesFixedFunction, int maxBonesPerVert, int maxBonesPerTri, 
			int maxBonesPerStrip, const char *fileName );

	// 
	// Methods associated with pre-processing the mesh
	//

	//
	// Methods associated with mesh processing
	//

	void SourceMeshToTriangleList( s_model_t *pSrcModel, s_mesh_t *pSrcMesh, CUtlVector<mstudioiface_t> &meshTriangleList );
	void CreateLODTriangleList( s_model_t *pSrcModel, int nLodID, s_source_t* pLODSource,
											mstudiomodel_t *pStudioModel,
											mstudiomesh_t *pStudioMesh,
											CUtlVector<mstudioiface_t> &meshTriangleList, bool writeDebug );

	// This processes the model + breaks it into strips
	void ProcessModel( studiohdr_t *phdr, s_bodypart_t *pSrcBodyParts, TotalMeshStats_t& stats, 
		bool bForceSoftwareSkin, bool bHWFlex );

	// processes a single mesh within the model
	void ProcessMesh( Mesh_t *pMesh, studiohdr_t *pStudioHeader, CUtlVector<mstudioiface_t> &srcFaces,
						mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, bool ForceNoFlex, 
						bool bForceSoftwareSkin, bool bHWFlex );

	// Processes a single strip group
	void ProcessStripGroup( StripGroup_t *pStripGroup, bool isHWSkinned, bool isFlexed, 
							mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh,
							CUtlVector<mstudioiface_t> &srcFaces,
							TriangleProcessedList_t& trianglesProcessed,
							int maxBonesPerVert, int maxBonesPerTri, int maxBonesPerStrip,
							bool forceNoFlex, bool bHWFlex );

	// Constructs vertices appropriate for a strip group based on source face data
	bool GenerateStripGroupVerticesFromFace( mstudioiface_t* pFace, 
		mstudiomesh_t *pStudioMesh, int maxPreferredBones, Vertex_t* pStripGroupVert );

	// Count the number of unique bones in a set of vertices
	int CountUniqueBones( int count, Vertex_t *pVertex ) const;
	int CountMaxVertBones( int count, Vertex_t *pVertex ) const;

	// Counts the unique # of bones in a strip
	int CountUniqueBonesInStrip( StripGroup_t *pStripGroup, Strip_t *pStrip );

	// Builds SW + HW skinned strips
	void BuildSWSkinnedStrips( TriangleList_t& tris, 
		VertexList_t const& verts, StripGroup_t *pStripGroup );
	void BuildHWSkinnedStrips( TriangleList_t& tris, VertexList_t& verts,
		StripGroup_t *pStripGroup, int maxBonesPerStrip );

	// These methods deal with finding another triangle to batch together
	// in a similar matrix state group
	int ComputeNewBonesNeeded( Triangle_t const& triangle ) const;
	bool AllocateHardwareBonesForTriangle( Triangle_t *tri );
	Triangle_t* GetNextTriangle( TriangleList_t& triangles, bool allowNewStrip );
	Triangle_t* GetNextUntouchedWithoutBoneStateChange( TriangleList_t& triangles );
	Triangle_t* GetNextUntouchedWithLeastBoneStateChanges( TriangleList_t& triangles );

	// Actually does the stripification
	void Stripify( VertexIndexList_t const& sourceIndices, bool isHWSkinned,
					int* pNumIndices, unsigned short** ppIndices );

	// Makes sure our vertices are using the correct bones
	void SanityCheckVertBones( VertexIndexList_t const& list, VertexList_t const& vertices );

	// Sets the flags associated with a particular strip group + mesh
	void ComputeStripGroupFlags( StripGroup_t *pStripGroup, bool isHWSkinned, bool isFlexed );
	void ComputeMeshFlags( Mesh_t *pMesh, studiohdr_t *pStudioHeader, mstudiomesh_t *pStudioMesh );

	bool MeshIsTeeth( studiohdr_t *pStudioHeader, mstudiomesh_t *pStudioMesh );

	// Tries to add neighboring vertices that'll fit into the matrix transform state
	void BuildStripsRecursive( VertexIndexList_t& indices, TriangleList_t& list, Triangle_t *triangle );

	// Figures out all bones affecting a particular triangle
	void BuildTriangleBoneData( VertexList_t& list, Triangle_t& tri );

	// Memory optimize the strip data
	void PostProcessStripGroup( mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, StripGroup_t *pStripGroup );

	void COptimizedModel::ZeroNumBones( void );

	//
	// Methods associated with writing VTX files
	//

	// This writes the strip data out to a VTX file
	void WriteVTXFile( studiohdr_t *pHdr, char const* pFileName, 
						TotalMeshStats_t const& stats );


	//
	// Methods associated with writing GL View files
	//


	// This writes the GL debugging files
	void WriteGLViewFiles( studiohdr_t *pHdr, const char *glViewFileName );

	void OutputMemoryUsage( void );
	bool IsVertexFlexed( mstudiomesh_t *pStudioMesh, int vertID ) const;
	void BuildNeighborInfo( TriangleList_t& list, int nMaxVertexId );
	void ClearTouched( void );
	void PrintVert( Vertex_t *v, mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh );
	void SanityCheckAgainstStudioHDR( studiohdr_t *phdr );
	void WriteStringTable( int stringTableOffset );
	void WriteMaterialReplacements( int materialReplacementsOffset );
	void WriteMaterialReplacementLists( int materialReplacementsOffset, int materialReplacementListOffset );
	void WriteHeader( int vertCacheSize, int maxBonesPerVert, 
		int maxBonesPerTri, int maxBonesPerStrip, int numBodyParts, long checkSum );
	void WriteBodyPart( int bodyPartID, mstudiobodyparts_t *pBodyPart, int modelID );
	void WriteModel( int modelID, mstudiomodel_t *pModel, int lodID );
	void WriteModelLOD( int lodID, ModelLOD_t *pLOD, int meshID );
	void WriteMesh( int meshID, Mesh_t *pMesh, int stripGroupID );
	void WriteStripGroup( int stripGroupID, StripGroup_t *pStripGroup,
						  int vertID, int indexID, int stripID );
	int WriteVerts( int vertID, StripGroup_t *pStripGroup );
	int WriteIndices( int indexID, StripGroup_t *pStripGroup );
	void WriteStrip( int stripID, Strip_t *pStrip, int indexID, int vertID, int boneID );
	void WriteBoneStateChange( int boneID, BoneStateChange_t *boneStateChange );
	
	void DrawGLViewTriangle( FILE *fp, Vector& pos1, Vector& pos2, Vector& pos3,
										 Vector& color1, Vector& color2, Vector& color3 );
	void WriteGLViewFile( studiohdr_t *phdr, const char *pFileName, unsigned int flags, float shrinkFactor );
	void ShrinkVerts( float shrinkFactor );
	void GLViewDrawBegin( int mode );
	void CheckVertBoneWeights( Vertex_t *pVert, mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh );
	void GLViewVert( FILE *fp, Vertex_t vert, int index, Vector& color, mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, bool showSubStrips, float shrinkFraction );
	void GLViewDrawEnd( void );
	void SetMeshPropsColor( unsigned int meshFlags, Vector& color );
	void SetFlexedAndSkinColor( unsigned int glViewFlags, unsigned int stripGroupFlags, Vector& color );
	void SetColorFromNumVertexBones( int numBones, Vector& color );
	Vector& GetOrigVertPosition( mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, Vertex_t *pVert );
	float GetOrigVertBoneWeightValue( mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, Vertex_t *pVert, int boneID );
	mstudioboneweight_t &GetOrigVertBoneWeight( mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, Vertex_t *pVert );
	int GetOrigVertBoneIndex( mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, Vertex_t *pVert, int boneID );
	void ShowStats( void );
	void MapGlobalBonesToHardwareBoneIDsAndSortBones( studiohdr_t *phdr );
	void RemoveRedundantBoneStateChanges( void );
	void CheckVert( Vertex_t *pVert, int maxBonesPerTri, int maxBonesPerVert );
	void CheckAllVerts( int maxBonesPerTri, int maxBonesPerVert );
	void SortBonesWithinVertex( bool flexed, Vertex_t *vert, mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, int *globalToHardwareBoneIndex, int *hardwareToGlobalBoneIndex, int maxBonesPerTri, int maxBonesPerVert );
	int GetTotalVertsForMesh( Mesh_t *pMesh );
	int GetTotalIndicesForMesh( Mesh_t *pMesh );
	int GetTotalStripsForMesh( Mesh_t *pMesh );
	int GetTotalStripGroupsForMesh( Mesh_t *pMesh );
	int GetTotalBoneStateChangesForMesh( Mesh_t *pMesh );
	bool MeshNeedsRemoval( studiohdr_t *pHdr, mstudiomesh_t *pStudioMesh,
												   LodScriptData_t& scriptLOD );

	void PrintBoneStateChanges( studiohdr_t *pHdr, int lod );
	void PrintVerts( studiohdr_t *phdr, int lod );
	void SanityCheckVertexBoneLODFlags( studiohdr_t *pStudioHdr, FileHeader_t *pVtxHeader );

	//
	// SOURCE DATA
	//

	// These are all the built models
	CUtlVector<Model_t> m_Models;
	
	// total number of bones in the studio model
	int m_NumBones;

	// information about the hardware
	int m_VertexCacheSize;
	int m_MaxBonesPerTri;
	int m_MaxBonesPerVert;
	int	m_MaxBonesPerStrip;
	bool m_UsesFixedFunction;

	// stats
	int m_NumSkinnedAndFlexedVerts;

	CHardwareMatrixState m_HardwareMatrixState;

	// a place to stick file output.
	CFileBuffer *m_FileBuffer;

	// offset for different items in the output file.
	int m_BodyPartsOffset;
	int m_ModelsOffset;
	int m_ModelLODsOffset;
	int m_MeshesOffset;
	int m_StripGroupsOffset;
	int m_StripsOffset;
	int m_VertsOffset;
	int m_IndicesOffset;
	int m_BoneStageChangesOffset;
	int m_StringTableOffset;
	int m_MaterialReplacementsOffset;
	int m_MaterialReplacementsListOffset;
	int m_EndOfFileOffset;
};

//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------

static COptimizedModel s_OptimizedModel;


//-----------------------------------------------------------------------------
// Cleanup method
//-----------------------------------------------------------------------------

void COptimizedModel::CleanupEverything()
{
}



void COptimizedModel::OutputMemoryUsage( void )
{
	printf( "body parts:   %7d bytes\n", ( int )( m_ModelsOffset - m_BodyPartsOffset ) );
	printf( "models:       %7d bytes\n", ( int )( m_MeshesOffset - m_ModelsOffset ) );
	printf( "model LODs:   %7d bytes\n", ( int )( m_MeshesOffset - m_ModelLODsOffset ) );
	printf( "meshes:       %7d bytes\n", ( int )( m_StripGroupsOffset - m_MeshesOffset ) );
	printf( "strip groups: %7d bytes\n", ( int )( m_StripsOffset - m_StripGroupsOffset ) );
	printf( "strips:       %7d bytes\n", ( int )( m_VertsOffset - m_StripsOffset ) );
	printf( "verts:        %7d bytes\n", ( int )( m_IndicesOffset - m_VertsOffset ) );
	printf( "indices:      %7d bytes\n", ( int )( m_BoneStageChangesOffset - m_IndicesOffset ) );
	printf( "bone changes: %7d bytes\n", ( int )( m_EndOfFileOffset - m_BoneStageChangesOffset ) );
	printf( "everything:   %7d bytes\n", ( int )( m_EndOfFileOffset ) );
}

void COptimizedModel::SanityCheckAgainstStudioHDR( studiohdr_t *phdr )
{
#if 0 // garymcthack
	printf( "SanityCheckAgainstStudioHDR\n" );
	FileHeader_t *header = ( FileHeader_t * )m_FileBuffer->GetPointer( 0 );
	Assert( header->numBodyParts == phdr->numbodyparts );
	for( int bodyPartID = 0; bodyPartID < header->numBodyParts; bodyPartID++ )
	{
		BodyPartHeader_t *bodyPart = header->pBodyPart( bodyPartID );
		mstudiobodyparts_t *pStudioBodyPart = phdr->pBodypart( bodyPartID );
		Assert( bodyPart->numModels == pStudioBodyPart->nummodels );
		for( int modelID = 0; modelID < bodyPart->numModels; modelID++ )
		{
			ModelHeader_t *model = bodyPart->pModel( modelID );
			mstudiomodel_t *pStudioModel = pStudioBodyPart->pModel( modelID );
			Assert( model->numMeshes == pStudioModel->nummeshes );
			for( int meshID = 0; meshID < model->numMeshes; meshID++ )
			{
				MeshHeader_t *mesh = model->pMesh( meshID );
				mstudiomesh_t *pStudioMesh = pStudioModel->pMesh( meshID );
				for( int stripGroupID = 0; stripGroupID < mesh->numStripGroups; stripGroupID++ )
				{
					StripGroupHeader_t *stripGroup = mesh->pStripGroup( stripGroupID );
					for( int stripID = 0; stripID < stripGroup->numStrips; stripID++ )
					{
						StripHeader_t *strip = stripGroup->pStrip( stripID );
					}
				}
			}
		}
	}
#endif
}


//-----------------------------------------------------------------------------
//
// The following methods are all related to creating groups of meshes to strip
//
//-----------------------------------------------------------------------------



//-----------------------------------------------------------------------------
// pick any triangle that hasn't been used yet to start with.
//-----------------------------------------------------------------------------

static Triangle_t *GetNextUntouched( TriangleList_t& triangles )
{
	int i;
	for( i = 0; i < triangles.Size(); i++ )
	{
		if( !triangles[i].touched )
			return &triangles[i];
	}
	return 0;
}


//-----------------------------------------------------------------------------
// Returns the number of bones that are not represented in the current hardware state
//-----------------------------------------------------------------------------

inline int COptimizedModel::ComputeNewBonesNeeded( Triangle_t const& triangle )	const
{
	int numNewBones = 0;
	for( int i = 0; i < triangle.numBones; ++i )
	{
		if( !m_HardwareMatrixState.IsMatrixAllocated( triangle.boneID[i] ) )
			++numNewBones;
	}
	return numNewBones;
}


//-----------------------------------------------------------------------------
// returns triangle index
// Find the next triangle without a bone state change
// May add new hardware bones if there is space and is necessary.
//-----------------------------------------------------------------------------

Triangle_t* COptimizedModel::GetNextUntouchedWithoutBoneStateChange( TriangleList_t& triangles )
{
	Triangle_t *bestTriangle = 0;
	int bestNumNewBones = MAX_NUM_BONES_PER_TRI + 1;
	
	int i;
	for( i = 0; i < triangles.Size(); i++ )
	{
		// We haven't processed this one, so let's try it
		if( !triangles[i].touched )
		{
			// How many bones are not represented in the current state?
			int numNewBones = ComputeNewBonesNeeded( triangles[i] );

			// if this triangle fit and if it's the best so far, save it.
			if ( (numNewBones <= m_HardwareMatrixState.FreeMatrixCount()) && 
				 (numNewBones < bestNumNewBones ) )
			{
				bestNumNewBones = numNewBones;
				bestTriangle = &triangles[i];

				// Can't get any better than this!
				if (bestNumNewBones == 0)
					break;
			}
		}
	}
	return bestTriangle;
}


//-----------------------------------------------------------------------------
// This will returns the triangle that requires the last number of bone
// state changes
//-----------------------------------------------------------------------------

Triangle_t *COptimizedModel::GetNextUntouchedWithLeastBoneStateChanges( TriangleList_t& triangles )
{
	Triangle_t *bestTriangle = 0;
	int bestNumNewBones = MAX_NUM_BONES_PER_TRI + 1;
	int i;
	
	// For this one, just find the triangle that needs the least number
	// of new bones. That way, we'll not have to change too many states
	for( i = 0; i < triangles.Size(); i++ )
	{
		if( !triangles[i].touched )
		{
			int numNewBones = ComputeNewBonesNeeded( triangles[i] );

			if( numNewBones < bestNumNewBones )
			{
				bestNumNewBones = numNewBones;
				bestTriangle = &triangles[i];
			}
		}
	}
	
	// This only happens if there are no triangles untouched
	if( !bestTriangle )
		return 0;

#ifdef USE_FLUSH
	m_HardwareMatrixState.DeallocateAll();
#else
	// Remove bones until we have enough space...
	int numToRemove = bestNumNewBones - m_HardwareMatrixState.FreeMatrixCount();
	Assert( numToRemove > 0 );
	m_HardwareMatrixState.DeallocateLRU(numToRemove);
#endif

	return bestTriangle;
}


//-----------------------------------------------------------------------------
// Allocate bones for a triangle from the hardware matrix state
//-----------------------------------------------------------------------------

bool COptimizedModel::AllocateHardwareBonesForTriangle( Triangle_t *tri )
{
	for( int i = 0; i < tri->numBones; ++i )
	{
		int bone = tri->boneID[i];
		if( !m_HardwareMatrixState.IsMatrixAllocated( bone ) )
		{
			if( !m_HardwareMatrixState.AllocateMatrix( bone ) )
				return false;
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Get the next triangle.
// Try to find one that doesn't cause a bone state change by jumping to 
// a new location in the model that works with the bones that we have allocated.
// If we can't find one, and allowNewStrip is true, then flush and pick the next
// best triangle that is close to the current hardware bone state.
//-----------------------------------------------------------------------------

Triangle_t *COptimizedModel::GetNextTriangle( TriangleList_t& tris, bool allowNewStrip )
{
	// First try to get a triangle that doesn't involve changing matrix state
	Triangle_t *tri;
	tri = GetNextUntouchedWithoutBoneStateChange( tris );

	// If that didn't work, pick the triangle that changes the state the least
	if( !tri && allowNewStrip )
	{
		tri = GetNextUntouchedWithLeastBoneStateChanges( tris );
	}

	// Return the triangle we found
	return tri;
}


//-----------------------------------------------------------------------------
// Make sure all vertices we've added up to now use bones in the matrix list
//-----------------------------------------------------------------------------

void COptimizedModel::SanityCheckVertBones( VertexIndexList_t const& list, VertexList_t const& vertices )
{
#ifdef _DEBUG
	Vertex_t const *pVert;
	int i;
	for( i = 0; i < list.Size(); i++ )
	{
		pVert = &vertices[list[i]];
		if( !g_staticprop )
		{
			Assert( pVert->numBones != 0 );
		}
		int j;
		for( j = 0; j < pVert->numBones; j++ )
		{
			if( pVert->boneID[j] == -1 )
			{
				continue;
			}
			if( !m_HardwareMatrixState.IsMatrixAllocated( pVert->boneID[j] ) )
			{
				Assert( 0 );
			}
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Make sure all vertices we've added up to now use bones in the matrix list
//-----------------------------------------------------------------------------
void COptimizedModel::Stripify( VertexIndexList_t const& sourceIndices, bool isHWSkinned,
							    int* pNumIndices, unsigned short** ppIndices ) 
{
	if( sourceIndices.Size() == 0 )
	{
		*ppIndices = 0;
		*pNumIndices = 0;
		return;
	}

	// Skip the tristripping phase if we're building in preview mode
	if ( g_bBuildPreview || isHWSkinned == false )
	{
		*pNumIndices = sourceIndices.Count();
		*ppIndices = new unsigned short[*pNumIndices];
		memcpy( *ppIndices, sourceIndices.Base(), (*pNumIndices) * sizeof(unsigned short) );
		return;
	}

/*
	printf( "Stripify\n" );
	int i;
	for( i = 0; i < sourceIndices.Size(); i++ )
	{
		printf( "stripindex: %d\n", sourceIndices[i] );
	}
*/

#ifdef NVTRISTRIP
	PrimitiveGroup *primGroups;
	unsigned short numPrimGroups;

	// Be sure to call delete[] on the returned primGroups to avoid leaking mem
	GenerateStrips( &sourceIndices[0], sourceIndices.Size(),
		&primGroups, &numPrimGroups );
	Assert( numPrimGroups == 1 );
	*pNumIndices = primGroups->numIndices;
	*ppIndices = new unsigned short[*pNumIndices];
	memcpy( *ppIndices, primGroups->indices, sizeof( unsigned short ) * *pNumIndices );
	delete [] primGroups;
#endif
}

//-----------------------------------------------------------------------------
// eat up triangle recursively by flood-filling around the model until
// we run out of bones on the hardware.
//-----------------------------------------------------------------------------
void COptimizedModel::BuildStripsRecursive( VertexIndexList_t& indices,
								TriangleList_t& tris, Triangle_t *triangle )
{
	Assert( triangle );

	// Don't process the triangle if it's already been processed
	if( triangle->touched )
		return;

	// Only suck in triangles that need no state change
	if ( ComputeNewBonesNeeded( *triangle ) )
		return;

	// We've got enough hardware bones. Lets add this triangle's vertices, and 
	// then add the vertices of all the neighboring triangles.
	triangle->touched = true;

	indices.AddToTail( ( unsigned short )triangle->vertID[0] );
	indices.AddToTail( ( unsigned short )triangle->vertID[1] );
	indices.AddToTail( ( unsigned short )triangle->vertID[2] );

	// Try to add our neighbors
	if( triangle->neighborTriID[0] != -1 )
	{
		BuildStripsRecursive( indices, tris, &tris[triangle->neighborTriID[0]] );
	}
	if( triangle->neighborTriID[1] != -1 )
	{
		BuildStripsRecursive( indices, tris, &tris[triangle->neighborTriID[1]] );
	}
	if( triangle->neighborTriID[2] != -1 )
	{
		BuildStripsRecursive( indices, tris, &tris[triangle->neighborTriID[2]] );
	}
}


//-----------------------------------------------------------------------------
// Processes a HW-skinned strip group
//-----------------------------------------------------------------------------
void COptimizedModel::BuildHWSkinnedStrips( TriangleList_t& triangles, 
	VertexList_t& vertices, StripGroup_t *pStripGroup, int maxBonesPerStrip )
{
	// Set up the hardware matrix state
	m_HardwareMatrixState.Init( maxBonesPerStrip );

	// Empty out the list of triangles to be stripified.
	VertexIndexList_t trianglesToStrip;
	trianglesToStrip.EnsureCapacity( triangles.Size() * 3 );

	// pick any old unused triangle to start with.
	Triangle_t *pSeedTri = GetNextUntouched( triangles );
	while( pSeedTri )
	{
		// Make sure we've got out transforms allocated
#ifdef DBGFLAG_ASSERT
		bool ok = 
#endif
			AllocateHardwareBonesForTriangle( pSeedTri );
		Assert( ok );

		// eat up triangle recursively by flood-filling around the model until
		// we run out of bones on the hardware.
		BuildStripsRecursive( trianglesToStrip, triangles, pSeedTri );

		// Try to jump to a new location in the mesh without 
		// causing a hardware bone state overflow or flush.
		pSeedTri = GetNextTriangle( triangles, false );
		if( pSeedTri )
			continue;

		// Save the results of the generated strip.
		int stripIdx = pStripGroup->strips.AddToTail( );
		Strip_t& newStrip = pStripGroup->strips[stripIdx];

		// Compute the strip flags
		newStrip.flags = 0;
#ifdef EMIT_TRILISTS
		newStrip.flags |= STRIP_IS_TRILIST;
#else
		newStrip.flags |= STRIP_IS_TRISTRIP;
#endif

		// Sanity check the indices of the bones.
		SanityCheckVertBones( trianglesToStrip, vertices );

		// There are no more triangles to eat up without causing a flush, so 
		// go ahead and stripify what we have and flush.
		// NOTE: This allocates space for stripIndices.pIndices.
		Stripify( trianglesToStrip, true, &newStrip.numIndices, &newStrip.pIndices );

		// hack - should just build directly into newStrip.verts instead of using a global.
		int i;
		for( i = 0; i < vertices.Size(); i++ )
		{
			newStrip.verts.AddToTail( vertices[i] );
		}

		// Compute the number of bones in this strip
		newStrip.numBoneStateChanges = m_HardwareMatrixState.AllocatedMatrixCount();
		Assert( newStrip.numBoneStateChanges <= maxBonesPerStrip );

		// Save off the bones used for this strip.
		for( i = 0; i < m_HardwareMatrixState.AllocatedMatrixCount(); i++ )
		{
			newStrip.boneStateChanges[i].hardwareID = i;
			newStrip.boneStateChanges[i].newBoneID = m_HardwareMatrixState.GetNthBoneGlobalID( i );
		}

		// Empty out the triangles to strip so that we can start again with a new strip.
		trianglesToStrip.RemoveAll();

		// Get the next best triangle, allowing for a bone state flushes.
		pSeedTri = GetNextTriangle( triangles, true );
	}
}


//-----------------------------------------------------------------------------
// Processes a SW-skinned strip group
//-----------------------------------------------------------------------------
void COptimizedModel::BuildSWSkinnedStrips( TriangleList_t& triangles, 
	VertexList_t const& vertices, StripGroup_t *pStripGroup )
{
	// Save the results of the generated strip.
	int stripIdx = pStripGroup->strips.AddToTail( );
	Strip_t& newStrip = pStripGroup->strips[stripIdx];

	// Set the strip flags
	newStrip.flags = 0;
#ifdef EMIT_TRILISTS
	newStrip.flags |= STRIP_IS_TRILIST;
#else
	newStrip.flags |= STRIP_IS_TRISTRIP;
#endif

	int nTriangleCount = triangles.Count();

	VertexIndexList_t indices;
	indices.EnsureCapacity( nTriangleCount * 3 );

	for( int i = 0; i < nTriangleCount; i++ )
	{
		Triangle_t* triangle = &triangles[i];
		triangle->touched = true;
		indices.AddToTail( ( unsigned short )triangle->vertID[0] );
		indices.AddToTail( ( unsigned short )triangle->vertID[1] );
		indices.AddToTail( ( unsigned short )triangle->vertID[2] );
	}
	
	Stripify( indices, false, &newStrip.numIndices, &newStrip.pIndices );

	// hack - should just build directly into newStrip.verts instead of using a global.
	for( int i = 0; i < vertices.Size(); i++ )
	{
		newStrip.verts.AddToTail( vertices[i] );
	}

	newStrip.numBoneStateChanges = 0;
	for( int i = 0; i < MAX_NUM_BONES_PER_STRIP; i++ )
	{
		newStrip.boneStateChanges[i].hardwareID = -1;
		newStrip.boneStateChanges[i].newBoneID = -1;
	}
}


//-----------------------------------------------------------------------------
// Returns true if a particular vertex is part of a flex
//-----------------------------------------------------------------------------
bool COptimizedModel::IsVertexFlexed( mstudiomesh_t *pStudioMesh, int vertID ) const
{
	mstudioflex_t	*pflex = pStudioMesh->pFlex( 0 );
	
	int i, j, n;

	// Iterate through all the flexes
	// figure out if the vertex is part of the flex
	for (i = 0; i < pStudioMesh->numflexes; i++)
	{
		byte *pvanim = pflex[i].pBaseVertanim();
		int nVAnimSizeBytes = pflex[i].VertAnimSizeBytes();

		for (j = 0; j < pflex[i].numverts; j++, pvanim += nVAnimSizeBytes )
		{
			mstudiovertanim_t *pAnim = (mstudiovertanim_t*)( pvanim );

			n = pAnim->index;
			if ( n == vertID )
				return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Computes flags for the strip group
//-----------------------------------------------------------------------------

void COptimizedModel::ComputeStripGroupFlags( StripGroup_t *pStripGroup,
											  bool isHWSkinned, bool isFlexed )
{
	pStripGroup->flags = 0;
	if( isFlexed )
	{
		pStripGroup->flags |= STRIPGROUP_IS_FLEXED;
		pStripGroup->flags |= STRIPGROUP_IS_DELTA_FLEXED;	// Going forward, DX9 models are delta flexed
	}
	if( isHWSkinned )
	{
		pStripGroup->flags |= STRIPGROUP_IS_HWSKINNED;
	}
}


//-----------------------------------------------------------------------------
// Try to reduce the number of bones affecting this vert so hardware can deal
//-----------------------------------------------------------------------------

#define MIN_BONE_INFLUENCE 1.0f

static void	TryToReduceBoneInfluence( Vertex_t& stripGroupVert, 
							mstudioboneweight_t const& boneWeights, int maxBones )
{
	int i;
	while (	stripGroupVert.numBones > maxBones)
	{
		// Find the minimum bone weight...
		float minWeight = 2.0;
		int minIndex = -1;
		for( i = 0; i < MAX_NUM_BONES_PER_VERT; ++i )
		{
			if (stripGroupVert.boneID[i] != -1)
			{
				float weight = boneWeights.weight[ stripGroupVert.boneWeightIndex[i] ];
				if (weight < minWeight)
				{
					minWeight = weight;
					minIndex = i;
				}
			}
		}
		Assert( minIndex >= 0 );
		
		// Now that we got it, remove that bone influence if it's small enough
		if (minWeight >= MIN_BONE_INFLUENCE)
			return;

		// Blat out that bone!
		for( i = minIndex; i < MAX_NUM_BONES_PER_VERT - 1; ++i )
		{
			stripGroupVert.boneID[i] = stripGroupVert.boneID[i+1];
			stripGroupVert.boneWeightIndex[i] = stripGroupVert.boneWeightIndex[i+1];
		}
		stripGroupVert.boneID[ MAX_NUM_BONES_PER_VERT - 1] = -1;
		stripGroupVert.boneWeightIndex[ MAX_NUM_BONES_PER_VERT - 1] = 0;

		--stripGroupVert.numBones;
	}
}

//-----------------------------------------------------------------------------
// Generate three strip-group ready vertices from source mesh data
// returns true if the face contants a flexed vertex
//-----------------------------------------------------------------------------

bool COptimizedModel::GenerateStripGroupVerticesFromFace(	mstudioiface_t* pFace, 
															mstudiomesh_t *pStudioMesh, 
															int maxPreferredBones,
															Vertex_t* pStripGroupVert )
{
	int vertIDs[3];
	vertIDs[0] = pFace->a;
	vertIDs[1] = pFace->b;
	vertIDs[2] = pFace->c;
	
	const mstudio_meshvertexdata_t *vertData = pStudioMesh->GetVertexData();
	Assert( vertData ); // This can only return NULL on X360 for now

	bool triangleIsFlexed = false;
	for( int faceIndex = 0; faceIndex < 3; ++faceIndex )
	{
		// Get the original source vertex id
		int vertex = vertIDs[faceIndex];

		// Check the verts of the triangle to see if they are flexed
		triangleIsFlexed = triangleIsFlexed || IsVertexFlexed( pStudioMesh, vertex );

		// How many bones affect this vertex
		mstudioboneweight_t *pBoneWeight = vertData->BoneWeights(vertex);

		int bonesAffectingVertex = pBoneWeight->numbones;
		if( !g_staticprop && bonesAffectingVertex <= 0 )
		{
			MdlWarning( "too few bones/vert (%d) : it has no bones!\n", bonesAffectingVertex );
		}
		else if( bonesAffectingVertex > MAX_NUM_BONES_PER_VERT )
		{
			MdlError( "too many bones/vert (%d) : MAX_NUM_BONES_PER_VERT needs to be upped\n", bonesAffectingVertex );
		}

		// Set the fields of the strip group's vert
		pStripGroupVert[faceIndex].origMeshVertID = vertex;

		pStripGroupVert[faceIndex].numBones = bonesAffectingVertex;

		int boneID;

#ifndef IGNORE_BONES
		for( boneID = 0; boneID < bonesAffectingVertex; boneID++ )
		{
			pStripGroupVert[faceIndex].boneID[boneID] = pBoneWeight->bone[boneID];
			pStripGroupVert[faceIndex].boneWeightIndex[boneID] = boneID;
		}
		for( ; boneID < MAX_NUM_BONES_PER_VERT; boneID++ )
		{
			pStripGroupVert[faceIndex].boneID[boneID] = -1;
			pStripGroupVert[faceIndex].boneWeightIndex[boneID] = boneID;
		}
#else
		// don't let bones have any influence.
		for( boneID = 0; boneID < MAX_NUM_BONES_PER_VERT; boneID++ )
		{
			pStripGroupVert[faceIndex].boneID[boneID] = -1;
			pStripGroupVert[faceIndex].boneWeights[boneID] = 0;
		}
#endif

		// For hardware skinning, we want to try to reduce the number of
		// bone influences, which we'll do here if we can.
		// At the moment, we can't do this for fixed function because
		// we get seam problems at transitions between SW and HW rendered
		if (!m_UsesFixedFunction)
		{
			if ((maxPreferredBones > 0) && (bonesAffectingVertex > maxPreferredBones))
			{
				TryToReduceBoneInfluence( pStripGroupVert[faceIndex], *pBoneWeight, 
					maxPreferredBones );
			}
		}
	}

	return triangleIsFlexed;
}


//-----------------------------------------------------------------------------
// Computes the unique number of bones in a specified set of vertices
//-----------------------------------------------------------------------------

int COptimizedModel::CountUniqueBones( int count, Vertex_t *pVertex ) const
{
	int uniqueBoneCount = 0;
	int uniqueBoneList[MAX_NUM_BONES_PER_STRIP];
	while ( --count >= 0 )
	{
		for (int i = 0; i < pVertex[count].numBones; ++i)
		{
			int boneID = pVertex[count].boneID[i];
			int j = uniqueBoneCount;
			while ( --j >= 0 )
			{
				if (uniqueBoneList[j] == boneID)
					break;
			}

			// Didn't find a match!
			if (j < 0)
			{
				Assert( uniqueBoneCount < MAX_NUM_BONES_PER_STRIP );
				uniqueBoneList[uniqueBoneCount++] = boneID;
			}
		}
	}

	return uniqueBoneCount;
}

int COptimizedModel::CountMaxVertBones( int count, Vertex_t *pVertex ) const
{
	int maxBones = 0;
	while ( --count >= 0 )
	{
		if (maxBones < pVertex[count].numBones)
			maxBones = pVertex[count].numBones;
	}

	return maxBones;
}

//-----------------------------------------------------------------------------
// Adds a vertex to the list of vertices to be added to the strip group
//-----------------------------------------------------------------------------

static int FindOrCreateVertex( VertexList_t& list, Vertex_t const& vert )
{
	int i;
	for( i = 0; i < list.Size(); i++ )
	{
		if (list[i].origMeshVertID == vert.origMeshVertID)
		{
			// If this is the case, then everything else should be too!
			Assert( !memcmp( &list[i], &vert, sizeof( vert )) );
			return i;
		}
	}

	list.AddToTail( vert );
	return i;
}


//-----------------------------------------------------------------------------
// Computes the bones used by the triangle
//-----------------------------------------------------------------------------

void COptimizedModel::BuildTriangleBoneData( VertexList_t& list, Triangle_t& tri )
{
	int j, k, l;

	// Blat out the bone ID state
	tri.numBones = 0;
	for( j = 0; j < MAX_NUM_BONES_PER_TRI; j++ )
	{
		tri.boneID[j] = -1;
	}

	// Iterate through the vertices in the triangle
	for( j = 0; j < 3; j++ )
	{
		// Iterate over the bones influencing the vertex
		Vertex_t& vert = list[tri.vertID[j]];
		for( k = 0; k < vert.numBones; ++k )
		{
			int bone = vert.boneID[k];
			Assert( (bone >= 0) && (bone < m_NumBones) );

			// Look for matches with previously found bones
			for ( l = tri.numBones; --l >= 0; )
			{
				if ( bone == tri.boneID[l] )
					break;
			}

			// No match, add it to our list
			if ( l < 0 )
				tri.boneID[tri.numBones++] = bone;
		}
	}
}


//-----------------------------------------------------------------------------
// Computes neighboring triangles along each face of a triangle
//-----------------------------------------------------------------------------
struct EdgeInfo_t
{
	int m_nConnectedVertId;
	int m_nEdgeIndex;
	int m_nTriangleId;
};

static void FindMatchingEdge( TriangleList_t& list, int nTriangleId, int nEdgeIndex, 
	const int *pVertIds, CUtlVector< int >& vertexToEdges, CUtlFixedLinkedList< EdgeInfo_t >& edges )
{
	// Have we already attached this edge to something?
	Triangle_t &tri = list[nTriangleId];
	if ( tri.neighborTriID[nEdgeIndex] != -1 )
		return;

	int nVertIndex = ( pVertIds[0] < pVertIds[1] ) ? 0 : 1;
	int nConnectedVertId = pVertIds[ 1-nVertIndex ];
	int hFirstEdge = vertexToEdges[nVertIndex];
	for ( int hEdge = hFirstEdge; hEdge != edges.InvalidIndex(); hEdge = edges.Next(hEdge) )
	{
		EdgeInfo_t &edge = edges[hEdge];
		if ( edge.m_nConnectedVertId != nConnectedVertId )
			continue;

		// Can't attach triangles to themselves
		if ( edge.m_nTriangleId == nTriangleId )
			continue;

		// Found a match! Mark the two triangles as sharing an edge
		tri.neighborTriID[nEdgeIndex] = edge.m_nTriangleId;
		list[ edge.m_nTriangleId ].neighborTriID[ edge.m_nEdgeIndex ] = nTriangleId;
		if ( hEdge == hFirstEdge )
		{
			vertexToEdges[nVertIndex] = edges.Next( hFirstEdge );
		}
		edges.Free( hEdge );
		return;
	}

	// No match! Insert the disconnected edge into the edge list
	int hNewEdge = edges.Alloc( true );
	EdgeInfo_t &newEdge = edges[hNewEdge];
	newEdge.m_nConnectedVertId = nConnectedVertId;
	newEdge.m_nEdgeIndex = nEdgeIndex;
	newEdge.m_nTriangleId = nTriangleId;
	edges.LinkBefore( hFirstEdge, hNewEdge );
	vertexToEdges[nVertIndex] = hNewEdge;
}

void COptimizedModel::BuildNeighborInfo( TriangleList_t& list, int nMaxVertexId )
{
	// NOTE: vertexToEdges[vertId] contains the index of the head of a linked list stored in edges
	CUtlFixedLinkedList< EdgeInfo_t > edges;
	CUtlVector< int > vertexToEdges;
	vertexToEdges.SetCount( nMaxVertexId );
	memset( vertexToEdges.Base(), 0, nMaxVertexId * sizeof(int) );

	int pEdgeVertIds[2];
	int nTriCount = list.Count();
	for ( int i = 0; i < nTriCount; ++i )
	{
		Triangle_t &tri = list[i];

		// Add the three edges for this tri into a lookup table indexed by the lower vertID
		pEdgeVertIds[0] = tri.vertID[0]; pEdgeVertIds[1] = tri.vertID[1];
		FindMatchingEdge( list, i, 0, pEdgeVertIds, vertexToEdges, edges );

		pEdgeVertIds[0] = tri.vertID[1]; pEdgeVertIds[1] = tri.vertID[2];
		FindMatchingEdge( list, i, 1, pEdgeVertIds, vertexToEdges, edges );

		pEdgeVertIds[0] = tri.vertID[2]; pEdgeVertIds[1] = tri.vertID[0];
		FindMatchingEdge( list, i, 2, pEdgeVertIds, vertexToEdges, edges );
	}
}


//-----------------------------------------------------------------------------
// Processes a single strip group
//-----------------------------------------------------------------------------
void COptimizedModel::ProcessStripGroup( StripGroup_t *pStripGroup, bool isHWSkinned, 
										bool isFlexed, mstudiomodel_t *pStudioModel, 
										mstudiomesh_t *pStudioMesh,	
										CUtlVector<mstudioiface_t> &srcFaces,
										TriangleProcessedList_t& trianglesProcessed,
										int maxBonesPerVert, int maxBonesPerTri, 
										int maxBonesPerStrip, bool forceNoFlex, bool bHWFlex )
{
	ComputeStripGroupFlags( pStripGroup, isHWSkinned, isFlexed );

	// all of the triangles before stripping for the current stripgroup.
	TriangleList_t	stripGroupSourceTriangles;
	VertexList_t	stripGroupVertices;

	// FIXME: Flexed/HWSkinned state of faces don't change with each pass.
	// We could precompute those flags just once (instead of doing it 4 times)

	// Add each face to the stripgroup, if it's appropriate
	for( int n=0; n < srcFaces.Size(); ++n )
	{
		// Don't bother processing a triangle that's already been done
		if (trianglesProcessed[n])
			continue;

		mstudioiface_t *pFace = &srcFaces[n];

		int preferredBones = isHWSkinned && (bHWFlex || !isFlexed) ? maxBonesPerVert : 0;
		// start a new strip group header.
		Vertex_t stripGroupVert[3];
		bool triangleIsFlexed = GenerateStripGroupVerticesFromFace( 
			pFace, pStudioMesh, 
			preferredBones, 
			stripGroupVert );

		if( forceNoFlex )
		{
			triangleIsFlexed = false;
		}
		
		// Don't bother to add the vertices to our strip-group vertex list
		// if we don't need to.
		if ( triangleIsFlexed != isFlexed )
			continue;

		// If we're doing software skinning, then always accept, even if 
		// this triangle should have been a HW skinned triangle.
		if (isHWSkinned)
		{
			// Check how many unique bones were in that face, and how many vertices
			// maximally in a vertex
			int numVertexBones = CountMaxVertBones( 3, stripGroupVert );
			int numTriangleBones = CountUniqueBones( 3, stripGroupVert );

			// If we have too many, we'll be skinning in software
			bool triangleIsHWSkinned = ( numTriangleBones <= maxBonesPerTri ) &&
										( numVertexBones <= maxBonesPerVert );

			// Don't bother to add vertices which aren't gonna be used in this pass
			if (!triangleIsHWSkinned)
				continue;
		}

		// Add a new triangle to our list of triangles
		int triIndex = stripGroupSourceTriangles.AddToTail( );

		Triangle_t& newTri = stripGroupSourceTriangles[triIndex];
		newTri.vertID[0] = FindOrCreateVertex( stripGroupVertices, stripGroupVert[0] );
		newTri.vertID[1] = FindOrCreateVertex( stripGroupVertices, stripGroupVert[1] );
		newTri.vertID[2] = FindOrCreateVertex( stripGroupVertices, stripGroupVert[2] );
		BuildTriangleBoneData( stripGroupVertices, newTri );

		// By default, this processes a triangle
		// Later on below, we may decide to un-process triangles
		// if tristrips that are too small are generated
		trianglesProcessed[n] = true;
	}
	
	// No mesh? bye bye
	if (stripGroupSourceTriangles.Size() == 0)
		return;

	// Figure out neighboring triangles
	BuildNeighborInfo( stripGroupSourceTriangles, stripGroupVertices.Count() );

	// Build the actual strips
	if( isHWSkinned )
	{
		BuildHWSkinnedStrips( stripGroupSourceTriangles, stripGroupVertices, pStripGroup, maxBonesPerStrip );

		// Check to see if any strips were produced that are too small
		// If so, remove them, and let the software pass take care of them.
		//if (stripGroupSourceTriangles.Size() < m_MinimumGroupSize
		//	trianglesProcessed[n] = false;
	}
	else
	{
		BuildSWSkinnedStrips( stripGroupSourceTriangles, stripGroupVertices, pStripGroup );
	}
}


//-----------------------------------------------------------------------------
// How many unique bones are in a strip?
//-----------------------------------------------------------------------------

int COptimizedModel::CountUniqueBonesInStrip( StripGroup_t *pStripGroup, Strip_t *pStrip )
{
	int *boneUsageCounts = ( int * )_alloca( m_NumBones * sizeof( int ) );
	memset( boneUsageCounts, 0, sizeof( int ) * m_NumBones );
	int i;
	for( i = 0; i < pStrip->numStripGroupVerts; i++ )
	{
		Vertex_t *pVert = &pStripGroup->verts[i+pStrip->stripGroupVertexOffset];
		if( !g_staticprop )
		{
			Assert( pVert->numBones != 0 );
		}
		int j;
		for( j = 0; j < pVert->numBones; j++ )
		{
			int boneID;
			boneID = pVert->boneID[j];
			Assert( boneID != -1 );
			boneUsageCounts[boneID]++;
		}
	}
	int numBones = 0;
	for( i = 0; i < m_NumBones; i++ )
	{
		if( boneUsageCounts[i] )
		{
			numBones++;
		}
	}
	return numBones;
}


//-----------------------------------------------------------------------------
// A little work to be done after we construct the strip groups
//-----------------------------------------------------------------------------

void COptimizedModel::PostProcessStripGroup( mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, StripGroup_t *pStripGroup )
{
	int i;
	
	// We're gonna compile all of the vertices in the current strip into
	// the current strip group's vertex list
	for( i = 0; i < pStripGroup->strips.Size(); i++ )
	{
		// create sorted strip verts and indices in the stripgroup
		Strip_t *pStrip = &pStripGroup->strips[i];
		int vertOffset = pStripGroup->verts.Size();
		pStrip->stripGroupVertexOffset = vertOffset;
		pStrip->stripGroupIndexOffset = pStripGroup->indices.Size();

		// make sure we have enough memory allocated
		pStripGroup->indices.EnsureCapacity( pStripGroup->indices.Size() + pStrip->numIndices );

		// Try to find each of the strip's vertices in the strip group
		int maxNumBones = 0;
		int j;
		for( j = 0; j < pStrip->numIndices; j++ )
		{
			int newIndex = -1;
			int index = pStrip->pIndices[j];
			Vertex_t *pVert = &pStrip->verts[index];

			// Does this vertex exist in the strip group?
			int k;
			for( k = vertOffset; k < pStripGroup->verts.Size(); k++ )
			{
				if( pVert->origMeshVertID == pStripGroup->verts[k].origMeshVertID )
				{
					newIndex = k;
					break;
				}
			}

			// Didn't find it? Add the vertex to the list
			if( newIndex == -1 )
			{
				newIndex = pStripGroup->verts.AddToTail( *pVert );
			}
			pStripGroup->indices.AddToTail( newIndex );

#ifdef _DEBUG
			//	float GetOrigVertBoneWeightValue( mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, Vertex_t *pVert, int boneID );
			int i;
			float *pWeight = ( float * )_alloca( sizeof( float ) * pVert->numBones );
			for( i = 0; i < pVert->numBones; i++ )
			{
				pWeight[i] = GetOrigVertBoneWeightValue( pStudioModel, pStudioMesh, pVert, i );
			}
#endif
			
			// Keep track of the max # of bones in a vert
			if (pVert->numBones > maxNumBones)
				maxNumBones = pVert->numBones;
		}
		pStrip->numStripGroupIndices = pStripGroup->indices.Size() - pStrip->stripGroupIndexOffset;
		pStrip->numStripGroupVerts = pStripGroup->verts.Size() - pStrip->stripGroupVertexOffset;

		// The number of bones in a strip is the max number of 
		// bones in a vertex in this strip for vertex shaders, and it's the
		// number of unique bones in the strip for fixed-function
		if (!m_UsesFixedFunction)
			pStrip->numBones = maxNumBones;
		else
			pStrip->numBones = CountUniqueBonesInStrip(pStripGroup, pStrip);
	}
}


//-----------------------------------------------------------------------------
// Returns true if a particular mesh is teeth
//-----------------------------------------------------------------------------

bool COptimizedModel::MeshIsTeeth( studiohdr_t *pStudioHeader, mstudiomesh_t *pStudioMesh )
{
	// The mesh is teeth if it's got a skin whose material has a non-zero flags
	int i;
	for( i = 0; i < pStudioHeader->numskinfamilies; i++ )
	{
		short *pskinref	= pStudioHeader->pSkinref( 0 );
		pskinref += ( i * pStudioHeader->numskinref );
		mstudiotexture_t *ptexture;
		ptexture = pStudioHeader->pTexture( pskinref[ pStudioMesh->material ] );
		if( ptexture->flags )
		{
			return true;
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Computes the flags for a mesh
//-----------------------------------------------------------------------------

void COptimizedModel::ComputeMeshFlags( Mesh_t *pMesh, studiohdr_t *pStudioHeader, 
										mstudiomesh_t *pStudioMesh )
{
	// eyeball?
	pMesh->flags = 0;
	if( pStudioMesh->materialtype != 0 )
	{
		pMesh->flags |= MESH_IS_EYES;
	}

	// teeth?
	if( MeshIsTeeth( pStudioHeader, pStudioMesh ) )
	{
		pMesh->flags |= MESH_IS_TEETH;
	}
}


//-----------------------------------------------------------------------------
// Creates 4 strip groups for a mesh, combinations of flexed + hwskinned
// A mesh has a single material
//-----------------------------------------------------------------------------

void COptimizedModel::ProcessMesh( Mesh_t *pMesh, studiohdr_t *pStudioHeader, 
								  CUtlVector<mstudioiface_t> &srcFaces,
								  mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh,
								  bool forceNoFlex, bool bForceSoftwareSkin, bool bHWFlex )
{
	// Compute the mesh flags
	ComputeMeshFlags( pMesh, pStudioHeader, pStudioMesh );

	// We're gonna keep track of which ones we haven't processed
	// because we're gonna add all unprocessed faces to the software
	// lists if for some reason they don't get added to the hardware lists
	TriangleProcessedList_t	trianglesProcessed;
	trianglesProcessed.AddMultipleToTail( srcFaces.Size() ); 
	memset( trianglesProcessed.Base(), 0, trianglesProcessed.Size() );
	
	// there are up to 4 stripgroups per mesh
	// Note that we're gonna do the HW skinned versions first
	// because they have the option of deciding not to be hardware after all
	int isHWSkinned, isFlexed;
	for( isHWSkinned = bForceSoftwareSkin ? 0 : 1; isHWSkinned >= 0; --isHWSkinned )
	{
		for( isFlexed = 1; isFlexed >= 0; --isFlexed )
		{
			int realMaxBonesPerTri, realMaxBonesPerVert, realMaxBonesPerStrip;

			if( isFlexed && !bHWFlex )
			{
				realMaxBonesPerTri = 1;
				realMaxBonesPerVert = 1;
				realMaxBonesPerStrip = 1;
			}
			else
			{
				realMaxBonesPerTri = m_MaxBonesPerTri;
				realMaxBonesPerVert = m_MaxBonesPerVert;
				realMaxBonesPerStrip = m_MaxBonesPerStrip;
			}

			int newStripGroupIndex = pMesh->stripGroups.AddToTail( );
			StripGroup_t& newStripGroup = pMesh->stripGroups[newStripGroupIndex];
			ProcessStripGroup( &newStripGroup, 
				isHWSkinned ? true : false, 
				isFlexed ? true : false, 
				pStudioModel, pStudioMesh, srcFaces, trianglesProcessed,
				realMaxBonesPerVert, realMaxBonesPerTri, realMaxBonesPerStrip, forceNoFlex, bHWFlex );

			PostProcessStripGroup( pStudioModel, pStudioMesh, &newStripGroup );

			// Clear out the strip group if there wasn't anything in it
			if( !newStripGroup.indices.Size() )
				pMesh->stripGroups.FastRemove( newStripGroupIndex );
		}
	}
}

//-----------------------------------------------------------------------------
// some setup required before we really get into it
//-----------------------------------------------------------------------------

void COptimizedModel::SetupMeshProcessing( studiohdr_t *pHdr, int vertexCacheSize,  
		bool usesFixedFunction, int maxBonesPerVert, int maxBonesPerTri, 
		int maxBonesPerStrip, const char *fileName )
{
#ifdef NVTRISTRIP
	// tell nvtristrip all of it's params
	SetCacheSize( vertexCacheSize );
	SetStitchStrips( true );
	SetMinStripSize( 0 );
#	ifdef EMIT_TRILISTS
	SetListsOnly( true );
#	else
	SetListsOnly( false );
#	endif
#endif // NVTRISTRIP
	
	if( !g_quiet )
	{
		printf( "---------------------\n" );
		printf( "Generating optimized mesh \"%s\":\n", fileName );
#ifdef _DEBUG
		printf( "\tvertex cache size: %d\n", vertexCacheSize );
		printf( "\tmax bones/tri:     %d\n", maxBonesPerTri );
		printf( "\tmax bones/vert:    %d\n", maxBonesPerVert );
		printf( "\tmax bones/strip:   %d\n", maxBonesPerStrip );
#endif
	}

	CleanupEverything();

	// Total number of bones in the original model
	m_NumBones = pHdr->numbones;

	// Hardware limitations
	m_MaxBonesPerVert = maxBonesPerVert;
	m_MaxBonesPerTri = maxBonesPerTri;
	m_MaxBonesPerStrip = maxBonesPerStrip;
	m_UsesFixedFunction = usesFixedFunction; 
	m_VertexCacheSize = vertexCacheSize;

	// stats
	m_NumSkinnedAndFlexedVerts = 0;
}


//-----------------------------------------------------------------------------
// Process the entire model, return stats...
//-----------------------------------------------------------------------------

int COptimizedModel::GetTotalVertsForMesh( Mesh_t *pMesh )
{
	int numVerts = 0;
	int i;
	for( i = 0; i < pMesh->stripGroups.Size(); i++ )
	{
		StripGroup_t *pStripGroup = &pMesh->stripGroups[i];
		numVerts += pStripGroup->verts.Size();
	}
	return numVerts;
}

int COptimizedModel::GetTotalIndicesForMesh( Mesh_t *pMesh )
{
	int numIndices = 0;
	int i;
	for( i = 0; i < pMesh->stripGroups.Size(); i++ )
	{
		StripGroup_t *pStripGroup = &pMesh->stripGroups[i];
		numIndices += pStripGroup->indices.Size();
	}
	return numIndices;
}

int COptimizedModel::GetTotalStripsForMesh( Mesh_t *pMesh )
{
	int numStrips = 0;
	int i;
	for( i = 0; i < pMesh->stripGroups.Size(); i++ )
	{
		StripGroup_t *pStripGroup = &pMesh->stripGroups[i];
		numStrips += pStripGroup->strips.Size();
	}
	return numStrips;
}

int COptimizedModel::GetTotalStripGroupsForMesh( Mesh_t *pMesh )
{
	return pMesh->stripGroups.Size();
}

int COptimizedModel::GetTotalBoneStateChangesForMesh( Mesh_t *pMesh )
{
	int numBoneStateChanges = 0;
	int i;
	for( i = 0; i < pMesh->stripGroups.Size(); i++ )
	{
		StripGroup_t *pStripGroup = &pMesh->stripGroups[i];
		int j;
		for( j = 0; j < pStripGroup->strips.Size(); j++ )
		{
			Strip_t *pStrip = &pStripGroup->strips[j];
			numBoneStateChanges += pStrip->numBoneStateChanges;
		}
	}
	return numBoneStateChanges;
}



/*
static void WriteDebugFile( const char *fileName, const char *outFileName, float red, float grn, float blu )
{
	char *tmpName = ( char * )_alloca( strlen( fileName ) + 1 );
	strcpy( tmpName, fileName );
	
	s_source_t *pSrc = Load_Source( tmpName, "SMD" );
	Assert( pSrc );

	int i, j;

	FILE *fp;
	fp = fopen( outFileName, "w" );
	Assert( fp );
	
	for( i = 0; i < pSrc->nummeshes; i++ )
	{
		s_mesh_t *pMesh = &pSrc->mesh[i];
		for( j = 0; j < pMesh->numfaces; j++ )
		{
			s_face_t *pFace = &pSrc->face[pMesh->faceoffset + j];
			Vector &a = pSrc->vertex[pMesh->vertexoffset + pFace->a];
			Vector &b = pSrc->vertex[pMesh->vertexoffset + pFace->b];
			Vector &c = pSrc->vertex[pMesh->vertexoffset + pFace->c];
			fprintf( fp, "3\n" );
			fprintf( fp, "%f %f %f %f %f %f\n", ( float )( a[0] ), ( float )( a[1] ), ( float )( a[2] ), 
				( float )red, ( float )grn, ( float )blu );
			fprintf( fp, "%f %f %f %f %f %f\n", ( float )( b[0] ), ( float )( b[1] ), ( float )( b[2] ), 
				( float )red, ( float )grn, ( float )blu );
			fprintf( fp, "%f %f %f %f %f %f\n", ( float )( c[0] ), ( float )( c[1] ), ( float )( c[2] ), 
				( float )red, ( float )grn, ( float )blu );
		}

	}
	fclose( fp );
}
*/

void COptimizedModel::SourceMeshToTriangleList( s_model_t *pSrcModel, s_mesh_t *pSrcMesh, 
										  CUtlVector<mstudioiface_t> &meshTriangleList )
{
	s_face_t *pFaces = pSrcModel->source->face + pSrcMesh->faceoffset;
	int i;
	for ( i = 0; i < pSrcMesh->numfaces; i++ )
	{
		const s_face_t &pFace = pFaces[i];
		int j = meshTriangleList.AddToTail();
		mstudioiface_t &newTriangle = meshTriangleList[j];
		newTriangle.a = pFace.a;
		newTriangle.b = pFace.b;
		newTriangle.c = pFace.c;
	}
}

/*
static void WriteSourceMesh( s_model_t *pSrcModel, s_mesh_t *pSrcMesh, int red_, int grn_, int blu_ )
{
	FILE *fp;
	fp = fopen( "blah.glv", "a+" );
	float red, grn, blu;

	red = ( float )rand() / ( float )VALVE_RAND_MAX;
	grn = ( float )rand() / ( float )VALVE_RAND_MAX;
	blu = ( float )rand() / ( float )VALVE_RAND_MAX;
	float len = red * red + grn * grn + blu * blu;
	len = sqrt( len );
	red *= 255.0f / len;
	grn *= 255.0f / len;
	blu *= 255.0f / len;
	
	s_face_t *pFaces = pSrcModel->source->face + pSrcMesh->faceoffset;
	int i;
	for( i = 0; i < pSrcMesh->numfaces; i++ )
	{
		const s_face_t &face = pFaces[i];
		Vector a, b, c;
		a = pSrcModel->source->vertex[pSrcMesh->vertexoffset + face.a];
		b = pSrcModel->source->vertex[pSrcMesh->vertexoffset + face.b];
		c = pSrcModel->source->vertex[pSrcMesh->vertexoffset + face.c];
		fprintf( fp, "3\n" );
		fprintf( fp, "%f %f %f %f %f %f\n", a[0], a[1], a[2], red, grn, blu );
		fprintf( fp, "%f %f %f %f %f %f\n", c[0], c[1], c[2], red, grn, blu );
		fprintf( fp, "%f %f %f %f %f %f\n", b[0], b[1], b[2], red, grn, blu );
	}
	fclose( fp );
}
*/

static void RandomColor( Vector& color )
{
	color[0] = ( ( float )rand() ) / ( float )VALVE_RAND_MAX;
	color[1] = ( ( float )rand() ) / ( float )VALVE_RAND_MAX;
	color[2] = ( ( float )rand() ) / ( float )VALVE_RAND_MAX;
	VectorNormalize( color );
}

/*
static void GLViewCube( Vector pos, float size, FILE *fp )
{
	fprintf( fp, "4\n" );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] + size, pos[1] + size, pos[2] + size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] - size, pos[1] + size, pos[2] + size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] + size, pos[1] - size, pos[2] + size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] - size, pos[1] - size, pos[2] + size );

	fprintf( fp, "4\n" );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] + size, pos[1] + size, pos[2] - size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] - size, pos[1] + size, pos[2] - size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] + size, pos[1] - size, pos[2] - size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] - size, pos[1] - size, pos[2] - size );

	fprintf( fp, "4\n" );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] + size, pos[1] - size, pos[2] - size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] - size, pos[1] - size, pos[2] - size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] + size, pos[1] - size, pos[2] + size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] - size, pos[1] - size, pos[2] + size );

	fprintf( fp, "4\n" );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] + size, pos[1] + size, pos[2] - size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] - size, pos[1] + size, pos[2] - size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] + size, pos[1] + size, pos[2] + size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] - size, pos[1] + size, pos[2] + size );

	fprintf( fp, "4\n" );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] - size, pos[1] + size, pos[2] - size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] - size, pos[1] - size, pos[2] - size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] - size, pos[1] + size, pos[2] + size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] - size, pos[1] - size, pos[2] + size );

	fprintf( fp, "4\n" );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] + size, pos[1] + size, pos[2] - size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] + size, pos[1] - size, pos[2] - size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] + size, pos[1] + size, pos[2] + size );
	fprintf( fp, "%f %f %f 255 0 0\n", pos[0] + size, pos[1] - size, pos[2] + size );
}
*/

#if 0
static void MStudioBoneWeightToSBoneWeight( s_boneweight_t &sbone, const mstudioboneweight_t &mbone, 
										   const s_source_t *pSrc )
{
	int i;
	for( i = 0; i < mbone.numbones; i++ )
	{
		sbone.bone[i] = pSrc->boneimap[mbone.bone[i]];
//		sbone.bone[i] = mbone.bone[i];
		sbone.weight[i] = mbone.weight[i];
	}
	sbone.numbones = mbone.numbones;
}
#endif

void COptimizedModel::CreateLODTriangleList( s_model_t *pSrcModel, int nLodID, s_source_t* pSrc,
											mstudiomodel_t *pStudioModel,
											mstudiomesh_t *pStudioMesh,
											CUtlVector<mstudioiface_t> &meshTriangleList, 
											bool writeDebug )
{
	if ( !pSrc || !pSrcModel )
		return;

#ifdef _DEBUG
//	const mstudio_modelvertexdata_t *vertData = pStudioModel->GetVertexData();
//	Assert( vertData ); // This can only return NULL on X360 for now
//	mstudiovertex_t *modelFirstVert = vertData->Vertex( 0 );
//	mstudiovertex_t *modelLastVert = vertData->Vertex( pStudioModel->numvertices - 1 );
//	mstudiovertex_t *meshFirstVert = vertData->Vertex( 0 );
//	mstudiovertex_t *meshLastVert = vertData->Vertex( pStudioMesh->numvertices - 1 );
#endif
	
/*
	if( writeDebug )
	{
		printf( "MODEL VERTS:\n" );
		int i;
		for( i = 0; i < pStudioModel->numvertices; i++ )
		{
			Vector &v = *pStudioModel->pVertex( i );
			Vector &n = *pStudioModel->pNormal( i );
			Vector2D &t = *pStudioModel->pTexcoord( i );
			printf( "model %d: p %f %f %f n: %f %f %f t: %f %f\n",
				i, v[0], v[1], v[2], n[0], n[1], n[2], t[0], t[1] );
		}
		printf( "MESH VERTS:\n" );
		for( i = 0; i < pStudioMesh->numvertices; i++ )
		{
			Vector &v = *pStudioMesh->pVertex( i );
			Vector &n = *pStudioMesh->pNormal( i );
			Vector2D &t = *pStudioMesh->pTexcoord( i );
			printf( "mesh %d: p %f %f %f n: %f %f %f t: %f %f\n",
				i, v[0], v[1], v[2], n[0], n[1], n[2], t[0], t[1] );
		}
	}
*/
	// need to find the mesh in the lod model that matches the original model.
	int i;
	int textureSearchID = MaterialToTexture( pStudioMesh->material );

	// this is icky.. . pSrc->nummeshes really refers to the total number of non-empty meshes
	// in the model.  In pSrc->meshes, there are some empty meshes in the middle if they 
	// don't exist for the material, so you have to go through all of the meshes to find
	// the non-empty ones.
	s_mesh_t *pSrcMesh = NULL;
	for ( i = 0; i < pStudioModel->nummeshes; i++ )
	{
		if ( pSrc->texmap[pSrc->meshindex[i]] == textureSearchID )
		{
			// woo hoo!  found it.
			pSrcMesh = &pSrc->mesh[pSrc->meshindex[i]];
			break;
		}
	}
	if ( !pSrcMesh )
	{
		//printf( "%s doesn't have material %d\n", lodSMDName, textureSearchID );
		// There aren't any triangles in this lower lod with this material on it.
		return;
	}

	CUtlVector<int> indexMapping;
	indexMapping.AddMultipleToTail( pSrcMesh->numvertices );

	for ( i = 0; i < pSrcMesh->numvertices; i++ )
	{
		// get the mapping between indices in the lod and their real pool location.
		indexMapping[i] = pSrcModel->m_pLodData->pMeshVertIndexMaps[nLodID][pSrcMesh->vertexoffset + i];
	}

	// build the lod's faces so indexes map to remapped vertexes
	for ( i = 0; i < pSrcMesh->numfaces; i++ )
	{
		int index = meshTriangleList.AddToTail();
		mstudioiface_t& newFace = meshTriangleList[index];
		const s_face_t &srcFace = pSrc->face[pSrcMesh->faceoffset + i];
		newFace.a = indexMapping[srcFace.a];
		newFace.b = indexMapping[srcFace.b];
		newFace.c = indexMapping[srcFace.c];
	}
}


bool ComparePath( const char *a, const char *b )
{
	if ( strlen( a ) != strlen( b ) )
	{
		return false;
	}

	// case and seperator invariant
	for ( ; *a; a++, b++ )
	{
		if ( *a == *b )
		{
			continue;
		}
		if ( tolower( *a ) == tolower( *b ) )
		{
			continue;
		}
		if ( ( *a == '/' || *a == '\\' ) && 
			( *b == '/' || *b == '\\' ) )
		{
			continue;
		}
		return false;
	}
	return true;
}

bool COptimizedModel::MeshNeedsRemoval( studiohdr_t *pHdr, mstudiomesh_t *pStudioMesh,
											   LodScriptData_t& scriptLOD )
{
	mstudiotexture_t *ptexture;
	short *pskinref	= pHdr->pSkinref( 0 );
	ptexture = pHdr->pTexture( pskinref[ pStudioMesh->material ] );
	const char *meshName = ptexture->material->GetName();
	int i;
	for( i = 0; i < scriptLOD.meshRemovals.Size(); i++ )
	{
		const char *meshRemovalName = scriptLOD.meshRemovals[i].GetSrcName();
		if ( ComparePath( meshName, meshRemovalName ) )
		{
			return true;
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Process the entire model, return stats...
//-----------------------------------------------------------------------------
void COptimizedModel::ProcessModel( studiohdr_t *pHdr, s_bodypart_t *pSrcBodyParts, 
										  TotalMeshStats_t& stats, bool bForceSoftwareSkin, bool bHWFlex )
{
	memset( &stats, 0, sizeof(stats) );
	m_Models.RemoveAll();

	int bodyPartID, modelID, meshID, lodID;
	for ( bodyPartID = 0; bodyPartID < pHdr->numbodyparts; bodyPartID++, stats.m_TotalBodyParts++ )
	{
		mstudiobodyparts_t *pBodyPart = pHdr->pBodypart( bodyPartID );
		s_bodypart_t *pSrcBodyPart = &pSrcBodyParts[bodyPartID];
		for ( modelID = 0; modelID < pBodyPart->nummodels; modelID++, stats.m_TotalModels++ )
		{
			int i = m_Models.AddToTail();
			Model_t& newModel = m_Models[i];
			mstudiomodel_t *pStudioModel = pBodyPart->pModel( modelID );
			s_model_t *pSrcModel = pSrcBodyPart->pmodel[modelID];
			for ( lodID = 0; lodID < g_ScriptLODs.Count(); lodID++, stats.m_TotalModelLODs++ )
			{
				LodScriptData_t& scriptLOD = g_ScriptLODs[lodID];
				s_source_t *pLODSource = pSrcModel->m_LodSources[lodID];

				int i = newModel.modelLODs.AddToTail();
				Assert( i == lodID );
				ModelLOD_t& newLOD = newModel.modelLODs[i];
				newLOD.switchPoint = scriptLOD.switchValue;

				// In this case, we've been told to remove the model
				if ( !pLODSource )
				{
					if ( pSrcModel && stricmp( pSrcModel->name, "blank" ) != 0)
					{
						// This is nonsensical
						Assert( lodID != 0 );
					}
					continue;
				}

				for ( meshID = 0; meshID < pStudioModel->nummeshes; meshID++, stats.m_TotalMeshes++ )
				{
#ifdef _DEBUG
//					printf( "bodyPart: %d model: %d modellod: %d mesh: %d\n", bodyPartID, modelID, lodID, meshID );
#endif
					mstudiomesh_t *pStudioMesh = pStudioModel->pMesh( meshID );
					s_mesh_t *pSrcMesh = &pSrcModel->source->mesh[pSrcModel->source->meshindex[meshID]];

					int i = newLOD.meshes.AddToTail();
					Assert( i == meshID );
					Mesh_t& newMesh = newLOD.meshes[i];
					
					if ( MeshNeedsRemoval( pHdr, pStudioMesh, scriptLOD ) )
						continue;				
#ifdef _DEBUG
//					int textureSearchID = material_to_texture( pStudioMesh->material );
//					const char *pDebugName = pHdr->pTexture( textureSearchID )->pszName( );
#endif
					CUtlVector<mstudioiface_t> meshTriangleList;
					if ( pLODSource )
					{
						// map the lod data to triangles
						// uses the original mesh redirected through a mapping table
						// this expects built per lod-to-root mapping tables to generate faces
						CreateLODTriangleList( pSrcModel, lodID, pLODSource, pStudioModel, pStudioMesh, meshTriangleList, false );
					}
					else
					{
						// build the triangle list from the unmapped source
						SourceMeshToTriangleList( pSrcModel, pSrcMesh, meshTriangleList );
					}
					
					ProcessMesh( &newMesh, pHdr, meshTriangleList, pStudioModel, pStudioMesh, 
						!scriptLOD.GetFacialAnimationEnabled(), bForceSoftwareSkin, bHWFlex );
					
					stats.m_TotalVerts += GetTotalVertsForMesh( &newMesh );
					stats.m_TotalIndices += GetTotalIndicesForMesh( &newMesh );
					stats.m_TotalStrips += GetTotalStripsForMesh( &newMesh );
					stats.m_TotalStripGroups += GetTotalStripGroupsForMesh( &newMesh );
					stats.m_TotalBoneStateChanges += GetTotalBoneStateChangesForMesh( &newMesh );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Some processing that happens at the end
//-----------------------------------------------------------------------------

void COptimizedModel::MapGlobalBonesToHardwareBoneIDsAndSortBones( studiohdr_t *phdr )
{
	// garymcthack: holy jesus is this function long!
#ifdef IGNORE_BONES
	return;
#endif
	
	int *globalToHardwareBoneIndex;
	int hardwareToGlobalBoneIndex[MAX_NUM_BONES_PER_STRIP];
	
	globalToHardwareBoneIndex = ( int * )_alloca( m_NumBones * sizeof( int ) );
	Assert( globalToHardwareBoneIndex );
	FileHeader_t *header = ( FileHeader_t * )m_FileBuffer->GetPointer( 0 );
	for( int bodyPartID = 0; bodyPartID < header->numBodyParts; bodyPartID++ )
	{
		BodyPartHeader_t *bodyPart = header->pBodyPart( bodyPartID );
		mstudiobodyparts_t *pStudioBodyPart = phdr->pBodypart( bodyPartID );
		for( int lodID = 0; lodID < header->numLODs; lodID++ )
		{
			// Dear LORD!  This needs to be in another function! (garymcthack)
			int i;
			for( i = 0; i < m_NumBones; i++ )
			{
				globalToHardwareBoneIndex[i] = -1;
			}
			for( i = 0; i < MAX_NUM_BONES_PER_STRIP; i++ )
			{
				hardwareToGlobalBoneIndex[i] = -1;
			}
			
			for( int modelID = 0; modelID < bodyPart->numModels; modelID++ )
			{
				ModelHeader_t *model = bodyPart->pModel( modelID );
				mstudiomodel_t *pStudioModel = pStudioBodyPart->pModel( modelID );
				ModelLODHeader_t *pLOD = model->pLOD( lodID );
				for( int meshID = 0; meshID < pLOD->numMeshes; meshID++ )
				{
					MeshHeader_t *mesh = pLOD->pMesh( meshID );
					mstudiomesh_t *pStudioMesh = pStudioModel->pMesh( meshID );
					for( int stripGroupID = 0; stripGroupID < mesh->numStripGroups; stripGroupID++ )
					{
						// Only do this to HW-skinned meshes
						StripGroupHeader_t *pStripGroup = mesh->pStripGroup( stripGroupID );
						if( !( pStripGroup->flags & STRIPGROUP_IS_HWSKINNED ) )
						{
							continue;
						}
						for( int stripID = 0; stripID < pStripGroup->numStrips; stripID++ )
						{
							//						printf( "UPDATING BONE STATE\n" );
							StripHeader_t *pStrip = pStripGroup->pStrip( stripID );
							
							// Generate mapping back and forth between hardware IDs and original bone IDs
							int boneStateChangeID;
							for( boneStateChangeID = 0; boneStateChangeID < pStrip->numBoneStateChanges; boneStateChangeID++ )
							{
								BoneStateChangeHeader_t *pBoneStateChange = pStrip->pBoneStateChange( boneStateChangeID );
								globalToHardwareBoneIndex[pBoneStateChange->newBoneID] = pBoneStateChange->hardwareID;
								if( pBoneStateChange->hardwareID != -1 )
								{
									hardwareToGlobalBoneIndex[pBoneStateChange->hardwareID] = pBoneStateChange->newBoneID;
								}
							}
							int vertID;
							for( vertID = 0; vertID < pStrip->numVerts; vertID++ )
							{
								Vertex_t *vert = pStripGroup->pVertex( vertID + pStrip->vertOffset );
								int boneID;
								for( boneID = 0; boneID < header->maxBonesPerVert; boneID++ )
								{
									int globalBoneID = vert->boneID[boneID];
									if( globalBoneID == -1 )
									{
										// this index isn't used.
										vert->boneID[boneID] = 0;
										// make sure it's associated weight is zero.
										//									Assert( vert->boneWeights[boneID] == 0 );
										continue;
									}
									Assert( globalBoneID >= 0 && globalBoneID < m_NumBones );
									vert->boneID[boneID] = globalToHardwareBoneIndex[globalBoneID];
									Assert( vert->boneID[boneID] >= 0 && vert->boneID[boneID] < header->maxBonesPerStrip );
								}
								
								// We only do index palette skinning when we're not doing fixed function
								if (m_UsesFixedFunction)
								{
//									bool flexed = pStripGroup->flags & STRIPGROUP_IS_FLEXED;
									SortBonesWithinVertex( false /* flexed */, vert, pStudioModel, 
										pStudioMesh, globalToHardwareBoneIndex, hardwareToGlobalBoneIndex, 
										m_MaxBonesPerTri, m_MaxBonesPerVert );
								}
							}
						}
					}
				}
			}
		}
	}
}



//-----------------------------------------------------------------------------
//
// The following methods are associated with writing a .vtx file
//
//-----------------------------------------------------------------------------

void COptimizedModel::WriteHeader( int vertCacheSize, int maxBonesPerVert, 
	int maxBonesPerTri, int maxBonesPerStrip, int numBodyParts, long checkSum )
{
	FileHeader_t fileHeader;

	fileHeader.version = OPTIMIZED_MODEL_FILE_VERSION;
	fileHeader.vertCacheSize = vertCacheSize;
	fileHeader.maxBonesPerTri = IsUShort( maxBonesPerTri );
	fileHeader.maxBonesPerVert = maxBonesPerVert;
	fileHeader.maxBonesPerStrip = IsUShort( maxBonesPerStrip );
	fileHeader.numBodyParts = numBodyParts;
	fileHeader.bodyPartOffset = sizeof( FileHeader_t );
	fileHeader.checkSum = checkSum;
	fileHeader.numLODs = g_ScriptLODs.Size();
	fileHeader.materialReplacementListOffset = m_MaterialReplacementsListOffset;
	m_FileBuffer->WriteAt( 0, &fileHeader, sizeof( FileHeader_t ), "header" );
#ifdef _DEBUG
//	FileHeader_t *debug = ( FileHeader_t * )m_FileBuffer->GetPointer( 0 );
//	BodyPartHeader_t *pBodyPart = debug->pBodyPart( 0 );
#endif
}


void COptimizedModel::WriteBodyPart( int bodyPartID, mstudiobodyparts_t *pBodyPart, int modelID )
{
	BodyPartHeader_t bodyPart;
	bodyPart.numModels = pBodyPart->nummodels;
	int bodyPartOffset = m_BodyPartsOffset + bodyPartID * sizeof( BodyPartHeader_t );
	int modelFileOffset = m_ModelsOffset + modelID * sizeof( ModelHeader_t );
	bodyPart.modelOffset = modelFileOffset - bodyPartOffset;
	m_FileBuffer->WriteAt( bodyPartOffset, &bodyPart, sizeof( BodyPartHeader_t ), "bodypart" );
#ifdef _DEBUG
//	BodyPartHeader_t *debug = ( BodyPartHeader_t * )m_FileBuffer->GetPointer( bodyPartOffset );
//	ModelHeader_t *pModel = debug->pModel( 0 );
#endif
}

void COptimizedModel::WriteModel( int modelID, mstudiomodel_t *pModel, int lodID )
{
	ModelHeader_t model;
	model.numLODs = IsChar( g_ScriptLODs.Size() );
	int modelFileOffset = m_ModelsOffset + modelID * sizeof( ModelHeader_t );
	int lodFileOffset = m_ModelLODsOffset + lodID * sizeof( ModelLODHeader_t );
	model.lodOffset = lodFileOffset - modelFileOffset;
	m_FileBuffer->WriteAt( modelFileOffset, &model, sizeof( ModelHeader_t ), "model" );
#ifdef _DEBUG
//	ModelHeader_t *debug = ( ModelHeader_t * )m_FileBuffer->GetPointer( modelFileOffset );
//	ModelLODHeader_t *pLOD = debug->pLOD( 0 );
#endif
}

void COptimizedModel::WriteModelLOD( int lodID, ModelLOD_t *pLOD, int meshID )
{
	ModelLODHeader_t lod;
	int lodFileOffset = m_ModelLODsOffset + lodID * sizeof( ModelLODHeader_t );
	int meshFileOffset = m_MeshesOffset + meshID * sizeof( MeshHeader_t );
	lod.meshOffset = meshFileOffset - lodFileOffset;
	lod.numMeshes = pLOD->meshes.Size();
	lod.switchPoint = pLOD->switchPoint;
	m_FileBuffer->WriteAt( lodFileOffset, &lod, sizeof( ModelLODHeader_t ), "modellod" );
#ifdef _DEBUG
//	ModelLODHeader_t *debug = ( ModelLODHeader_t * )m_FileBuffer->GetPointer( lodFileOffset );
//	MeshHeader_t *pMesh = debug->pMesh( 0 );
#endif
}

void COptimizedModel::WriteMesh( int meshID, Mesh_t *pMesh, int stripGroupID )
{
	MeshHeader_t mesh;
	mesh.numStripGroups = IsChar( pMesh->stripGroups.Size() );
	int meshFileOffset = m_MeshesOffset + meshID * sizeof( MeshHeader_t );
	int stripGroupFileOffset = m_StripGroupsOffset + stripGroupID * sizeof( StripGroupHeader_t );
	mesh.stripGroupHeaderOffset = stripGroupFileOffset - meshFileOffset;
	mesh.flags = pMesh->flags;
	m_FileBuffer->WriteAt( meshFileOffset, &mesh, sizeof( MeshHeader_t ), "mesh" );
#ifdef _DEBUG
//	MeshHeader_t *debug = ( MeshHeader_t * )m_FileBuffer->GetPointer( meshFileOffset );
//	StripGroupHeader_t *pStripGroup = debug->pStripGroup( 0 );
#endif
}

void COptimizedModel::WriteStripGroup( int stripGroupID, StripGroup_t *pStripGroup,
									   int vertID, int indexID, int stripID )
{
	StripGroupHeader_t stripGroup;
	stripGroup.numVerts = pStripGroup->verts.Size();
	stripGroup.numIndices = pStripGroup->indices.Size();
	stripGroup.numStrips = pStripGroup->strips.Size();
	stripGroup.flags = IsByte( pStripGroup->flags );
	int stripGroupFileOffset = m_StripGroupsOffset + stripGroupID * sizeof( StripGroupHeader_t );
	int vertsFileOffset = m_VertsOffset + vertID * sizeof( Vertex_t );
	int indicesFileOffset = m_IndicesOffset + indexID * sizeof( unsigned short );
	int stripsFileOffset = m_StripsOffset + stripID * sizeof( StripHeader_t );
	stripGroup.vertOffset = vertsFileOffset - stripGroupFileOffset;
	stripGroup.indexOffset = indicesFileOffset - stripGroupFileOffset;
	stripGroup.stripOffset = stripsFileOffset - stripGroupFileOffset;
	m_FileBuffer->WriteAt( stripGroupFileOffset, &stripGroup, sizeof( StripGroupHeader_t), "strip group" );
#ifdef _DEBUG
//	StripGroupHeader_t *debug = ( StripGroupHeader_t * )m_FileBuffer->GetPointer( stripGroupFileOffset );
//	unsigned short *pIndex = debug->pIndex( 0 );
//	Vertex_t *pVert = debug->pVertex( 0 );
//	StripHeader_t *pStripHeader = debug->pStrip( 0 );
#endif
}

int COptimizedModel::WriteVerts( int vertID, StripGroup_t *pStripGroup )
{
	int vertFileOffset = m_VertsOffset + vertID * sizeof( Vertex_t );
	int numVerts = pStripGroup->verts.Size();
	m_FileBuffer->WriteAt( vertFileOffset, &pStripGroup->verts[0], sizeof( Vertex_t ) * numVerts, "verts" );
#ifdef _DEBUG
//	Vertex_t *debug = ( Vertex_t * )m_FileBuffer->GetPointer( vertFileOffset );
#endif
	return numVerts;
}

int COptimizedModel::WriteIndices( int indexID, StripGroup_t *pStripGroup )
{
	int indexFileOffset = m_IndicesOffset + indexID * sizeof( unsigned short );
	int numIndices = pStripGroup->indices.Size();
	m_FileBuffer->WriteAt( indexFileOffset, &pStripGroup->indices[0], sizeof( unsigned short ) * numIndices, "indices" );
#ifdef _DEBUG
//	unsigned short *debug = ( unsigned short * )m_FileBuffer->GetPointer( indexFileOffset );
#endif
	return numIndices;
}

void COptimizedModel::WriteStrip( int stripID, Strip_t *pStrip, int indexID, int vertID, int boneID )
{
	StripHeader_t stripHeader;

	stripHeader.numIndices = pStrip->numStripGroupIndices;
	stripHeader.indexOffset = pStrip->stripGroupIndexOffset;
	stripHeader.numVerts = pStrip->numStripGroupVerts;
	stripHeader.vertOffset = pStrip->stripGroupVertexOffset;
	stripHeader.numBoneStateChanges = pStrip->numBoneStateChanges;
	stripHeader.numBones = IsShort( pStrip->numBones );
	stripHeader.flags = IsByte( pStrip->flags );
	int boneFileOffset = m_BoneStageChangesOffset + boneID * sizeof( BoneStateChangeHeader_t );
	int stripFileOffset = m_StripsOffset + stripID * sizeof( StripHeader_t );
	stripHeader.boneStateChangeOffset = IsInt24( boneFileOffset - stripFileOffset );
	m_FileBuffer->WriteAt( stripFileOffset, &stripHeader, sizeof( StripHeader_t ), "strip" );
#ifdef _DEBUG
//	StripHeader_t *debug = ( StripHeader_t* )m_FileBuffer->GetPointer( stripFileOffset );
//	BoneStateChangeHeader_t *pBoneStateChange = debug->pBoneStateChange( 0 );
#endif
}

void COptimizedModel::WriteBoneStateChange( int boneID, BoneStateChange_t *boneStateChange )
{
	BoneStateChangeHeader_t boneHeader;
	boneHeader.hardwareID = boneStateChange->hardwareID;
	boneHeader.newBoneID = boneStateChange->newBoneID;
	int boneFileOffset = m_BoneStageChangesOffset + boneID * sizeof( BoneStateChangeHeader_t );
#if 0
	printf( "\tboneStateChange: hwid: %d boneID %d\n", ( int )boneHeader.hardwareID, 
		( int )boneHeader.newBoneID );
#endif
	m_FileBuffer->WriteAt( boneFileOffset, &boneHeader, sizeof( BoneStateChangeHeader_t ), "bone" );
#ifdef _DEBUG
//	BoneStateChangeHeader_t *debug = ( BoneStateChangeHeader_t * )m_FileBuffer->GetPointer( boneFileOffset );
#endif
}
	
void COptimizedModel::ZeroNumBones( void )
{
	FileHeader_t *header = ( FileHeader_t * )m_FileBuffer->GetPointer( 0 );
	int bodyPartID, modelID, lodID, meshID, stripGroupID, vertID, stripID;
	for( bodyPartID = 0; bodyPartID < header->numBodyParts; bodyPartID++ )
	{
		BodyPartHeader_t *pBodyPart = header->pBodyPart( bodyPartID );
		for( modelID = 0; modelID < pBodyPart->numModels; modelID++ )
		{
			ModelHeader_t *pModel = pBodyPart->pModel( modelID );
			for( lodID = 0; lodID < pModel->numLODs; lodID++ )
			{
				ModelLODHeader_t *pLOD = pModel->pLOD( lodID );
				for( meshID = 0; meshID < pLOD->numMeshes; meshID++ )
				{
					MeshHeader_t *pMesh = pLOD->pMesh( meshID );
					for( stripGroupID = 0; stripGroupID < pMesh->numStripGroups; stripGroupID++ )
					{
						StripGroupHeader_t *pStripGroup = pMesh->pStripGroup( stripGroupID );
						for( vertID = 0; vertID < pStripGroup->numVerts; vertID++ )
						{
							Vertex_t *pVert = pStripGroup->pVertex( vertID );
							pVert->numBones = 0;
						}
						for( stripID = 0; stripID < pStripGroup->numStrips; stripID++ )
						{
							StripHeader_t *pStrip = pStripGroup->pStrip( stripID );
							pStrip->numBones = 0;
							pStrip->numBoneStateChanges = 0;
						}
					}
				}
			}
		}
	}
}

void COptimizedModel::WriteStringTable( int stringTableOffset )
{
	int stringTableSize = s_StringTable.CalcSize();
	if( stringTableSize == 0 )
	{
		return;
	}
	char *pTmp = new char[stringTableSize];
	s_StringTable.WriteToMem( pTmp );
	m_FileBuffer->WriteAt( stringTableOffset, pTmp, stringTableSize, "string table" );
//	char *pDebug = ( char * )m_FileBuffer->GetPointer( stringTableOffset );
	delete [] pTmp;
}

void COptimizedModel::WriteMaterialReplacements( int materialReplacementsOffset )
{
	int offset = materialReplacementsOffset;
 	int i, j;
	int numLODs = g_ScriptLODs.Size();
	for( i = 0; i < numLODs; i++ )
	{
		LodScriptData_t &scriptLOD = g_ScriptLODs[i];
		for( j = 0; j < scriptLOD.materialReplacements.Size(); j++ )
		{
			CLodScriptReplacement_t &materialReplacement = scriptLOD.materialReplacements[j];
			MaterialReplacementHeader_t tmpHeader;
			tmpHeader.materialID = FindMaterialByName( materialReplacement.GetSrcName() );
			tmpHeader.replacementMaterialNameOffset = m_StringTableOffset + 
				s_StringTable.StringTableOffset( materialReplacement.GetDstName() ) - offset;
			m_FileBuffer->WriteAt( offset, &tmpHeader, sizeof( tmpHeader ), "material replacements" );
			offset += sizeof( MaterialReplacementHeader_t );
		}
	}
}

void COptimizedModel::WriteMaterialReplacementLists( int materialReplacementsOffset, int materialReplacementListOffset )
{
	int replacementOffset = materialReplacementsOffset;
	int offset = materialReplacementListOffset;
	int i;
	int numLODs = g_ScriptLODs.Size();
	for( i = 0; i < numLODs; i++ )
	{
		LodScriptData_t &scriptLOD = g_ScriptLODs[i];
		MaterialReplacementListHeader_t tmpHeader;
		tmpHeader.numReplacements = IsChar( scriptLOD.materialReplacements.Size() );
		tmpHeader.replacementOffset = IsInt24( replacementOffset - offset );
		m_FileBuffer->WriteAt( offset, &tmpHeader, sizeof( tmpHeader ), "material replacement headers" );
		MaterialReplacementListHeader_t *pDebugList = 
			( MaterialReplacementListHeader_t * )m_FileBuffer->GetPointer( offset );
		if( pDebugList->numReplacements )
		{
//			MaterialReplacementHeader_t *pDebug = pDebugList->pMaterialReplacement( 0 );
//			const char *string = pDebug->pMaterialReplacementName();
		}
		replacementOffset += tmpHeader.numReplacements * sizeof( MaterialReplacementHeader_t );
		offset += sizeof( MaterialReplacementListHeader_t );
	}
}

void COptimizedModel::SanityCheckVertexBoneLODFlags( studiohdr_t *pStudioHdr, FileHeader_t *pVtxHeader )
{
	int bodyPartID;
	for( bodyPartID = 0; bodyPartID < pStudioHdr->numbodyparts; bodyPartID++ )
	{
		mstudiobodyparts_t *pBodyPart = pStudioHdr->pBodypart( bodyPartID );
		BodyPartHeader_t *pVtxBodyPart = pVtxHeader->pBodyPart( bodyPartID );
		int modelID;
		for( modelID = 0; modelID < pBodyPart->nummodels; modelID++ )
		{
			mstudiomodel_t *pModel = pBodyPart->pModel( modelID );
			ModelHeader_t *pVtxModel = pVtxBodyPart->pModel( modelID );
			int lodID;
			for( lodID = 0; lodID < pVtxModel->numLODs; lodID++ )
			{
				ModelLODHeader_t *pVtxLOD = pVtxModel->pLOD( lodID );
				int meshID;
				Assert( pVtxLOD->numMeshes == pModel->nummeshes );
				for( meshID = 0; meshID < pVtxLOD->numMeshes; meshID++ )
				{
					MeshHeader_t *pVtxMesh = pVtxLOD->pMesh( meshID );
					mstudiomesh_t *pMesh = pModel->pMesh( meshID );
					int stripGroupID;
					for( stripGroupID = 0; stripGroupID < pVtxMesh->numStripGroups; stripGroupID++ )
					{
						StripGroupHeader_t *pStripGroup = pVtxMesh->pStripGroup( stripGroupID );
						int vertID;
						for( vertID = 0; vertID < pStripGroup->numVerts; vertID++ )
						{
							Vertex_t *pVertex = pStripGroup->pVertex( vertID );
							Vector pos = GetOrigVertPosition( pModel, pMesh, pVertex );
							const mstudioboneweight_t &boneWeight = GetOrigVertBoneWeight( pModel, pMesh, pVertex );
							int i;

							for( i = 0; i < boneWeight.numbones; i++ )
							{
								mstudiobone_t *pBone = pStudioHdr->pBone( boneWeight.bone[i] );
//								const char *pBoneName = pBone->pszName();
								if (!( pBone->flags & ( BONE_USED_BY_VERTEX_LOD0 << lodID ) ))
								{
									MdlError("Mismarked Bone flag");
								}
							}
						}
					}
				}
			}
		}
	}
}

void COptimizedModel::WriteVTXFile( studiohdr_t *pHdr, const char *pFileName, 
								    TotalMeshStats_t const& stats )
{

	// calculate file offsets
	m_FileBuffer = new CFileBuffer( FILEBUFFER_SIZE );
	m_BodyPartsOffset = sizeof( FileHeader_t );
	m_ModelsOffset = m_BodyPartsOffset + sizeof( BodyPartHeader_t ) * stats.m_TotalBodyParts;
	m_ModelLODsOffset = m_ModelsOffset + sizeof( ModelHeader_t ) * stats.m_TotalModels;
	m_MeshesOffset = m_ModelLODsOffset + sizeof( ModelLODHeader_t ) * stats.m_TotalModelLODs;
	m_StripGroupsOffset = m_MeshesOffset + sizeof( MeshHeader_t ) * stats.m_TotalMeshes;
	m_StripsOffset = m_StripGroupsOffset + sizeof( StripGroupHeader_t ) * stats.m_TotalStripGroups;
	m_VertsOffset = m_StripsOffset + sizeof( StripHeader_t ) * stats.m_TotalStrips;
	m_IndicesOffset = m_VertsOffset + sizeof( Vertex_t ) * stats.m_TotalVerts;
	m_BoneStageChangesOffset = m_IndicesOffset + sizeof( unsigned short ) * stats.m_TotalIndices;
	m_StringTableOffset = m_BoneStageChangesOffset + sizeof( BoneStateChangeHeader_t ) * stats.m_TotalBoneStateChanges;
	m_MaterialReplacementsOffset = m_StringTableOffset + s_StringTable.CalcSize();
	m_MaterialReplacementsListOffset = m_MaterialReplacementsOffset + stats.m_TotalMaterialReplacements * sizeof( MaterialReplacementHeader_t );
	m_EndOfFileOffset = m_MaterialReplacementsListOffset + g_ScriptLODs.Count() * sizeof( MaterialReplacementListHeader_t );
	
	int curModel = 0;
	int curLOD = 0;
	int curMesh = 0;
	int curStrip = 0;
	int curStripGroup = 0;
	int curVert = 0;
	int curIndex = 0;
	int curBoneStateChange = 0;
	int deltaModel = 0;
	int deltaLOD = 0;
	int deltaMesh = 0;
	int deltaStrip = 0;
	int deltaStripGroup = 0;
	int deltaVert = 0;
	int deltaIndex = 0;
	int deltaBoneStateChange = 0;
	
	WriteStringTable( m_StringTableOffset );
	WriteMaterialReplacements( m_MaterialReplacementsOffset );
	WriteMaterialReplacementLists( m_MaterialReplacementsOffset, m_MaterialReplacementsListOffset );
	
	for( int bodyPartID = 0; bodyPartID < pHdr->numbodyparts; bodyPartID++ )
	{
		mstudiobodyparts_t *pBodyPart = pHdr->pBodypart( bodyPartID );
		for( int modelID = 0; modelID < pBodyPart->nummodels; modelID++ )
		{
			mstudiomodel_t *pStudioModel = pBodyPart->pModel( modelID );
			Model_t *pModel = &m_Models[curModel + deltaModel];
			for( int lodID = 0; lodID < g_ScriptLODs.Count(); lodID++ )
			{
//				printf( "lod: %d\n", lodID );
				ModelLOD_t *pLOD = &pModel->modelLODs[lodID];
				for( int meshID = 0; meshID < pStudioModel->nummeshes; meshID++ )
				{
					Mesh_t *pMesh = &pLOD->meshes[meshID];
					for( int stripGroupID = 0; stripGroupID < pMesh->stripGroups.Count(); stripGroupID++ )
					{
						StripGroup_t *pStripGroup = &pMesh->stripGroups[stripGroupID];
						deltaVert += WriteVerts( curVert + deltaVert, pStripGroup );
						deltaIndex += WriteIndices( curIndex + deltaIndex, pStripGroup );

						int nStripCount = pStripGroup->strips.Count();
						for( int stripID = 0; stripID < nStripCount; stripID++ )
						{
							Strip_t *pStrip = &pStripGroup->strips[stripID];
							for( int boneStateChangeID = 0; boneStateChangeID < pStrip->numBoneStateChanges; boneStateChangeID++ )
							{
								WriteBoneStateChange( curBoneStateChange + deltaBoneStateChange, &pStrip->boneStateChanges[boneStateChangeID] );
								deltaBoneStateChange++;
							}
							WriteStrip( curStrip + deltaStrip, pStrip, curIndex, curVert, curBoneStateChange );
							deltaStrip++;
							curBoneStateChange += deltaBoneStateChange;
							deltaBoneStateChange = 0;
						}
						WriteStripGroup( curStripGroup + deltaStripGroup, pStripGroup, curVert, curIndex, curStrip );
						deltaStripGroup++;
						curStrip += deltaStrip;
						deltaStrip = 0;
						curVert += deltaVert;
						deltaVert = 0;
						curIndex += deltaIndex;
						deltaIndex = 0;
					}
					WriteMesh( curMesh + deltaMesh, pMesh, curStripGroup );
					deltaMesh++;
					curStripGroup += deltaStripGroup;
					deltaStripGroup = 0;
				}
				WriteModelLOD( curLOD + deltaLOD, pLOD, curMesh );
				deltaLOD++;
				curMesh += deltaMesh;
				deltaMesh = 0;
			}
			WriteModel( curModel + deltaModel, pStudioModel, curLOD );
			deltaModel++;
			curLOD += deltaLOD;
			deltaLOD = 0;
		}
		WriteBodyPart( bodyPartID, pBodyPart, curModel );
		curModel += deltaModel;
		deltaModel = 0;
	}

	WriteHeader( m_VertexCacheSize, m_MaxBonesPerVert, m_MaxBonesPerTri, 
		m_MaxBonesPerStrip, pHdr->numbodyparts, pHdr->checksum );

#ifdef _DEBUG
	m_FileBuffer->TestWritten( m_EndOfFileOffset );
#endif
	
	MapGlobalBonesToHardwareBoneIDsAndSortBones( pHdr );

//	DebugCompareVerts( phdr );
	SanityCheckAgainstStudioHDR( pHdr );

	if ( !g_quiet )
	{
		OutputMemoryUsage();
	}
		
	RemoveRedundantBoneStateChanges();
	if( g_staticprop )
	{
		ZeroNumBones();
	}

	// Show statistics
#ifdef _DEBUG
//	ShowStats();
#endif
		
	m_FileBuffer->WriteToFile( pFileName, m_EndOfFileOffset );

	FileHeader_t *pVtxHeader = ( FileHeader_t * )m_FileBuffer->GetPointer( 0 );
	SanityCheckVertexBoneLODFlags( pHdr, pVtxHeader );
}
	


static void MergeLikeBoneIndicesWithinVert( mstudioboneweight_t *pBoneWeight )
{
	if( pBoneWeight->numbones == 1 )
	{
		return;
	}
	
	int i, j;
	int realNumBones = pBoneWeight->numbones;
	for( i = 0; i < pBoneWeight->numbones; i++ )
	{
		for( j = i+1; j < pBoneWeight->numbones; j++ )
		{
			if( ( pBoneWeight->bone[i] == pBoneWeight->bone[j] ) && ( pBoneWeight->weight[i] != 0.0f ) )
			{
				pBoneWeight->weight[i] += pBoneWeight->weight[j];
				pBoneWeight->weight[j] = 0.0f;
				realNumBones--;
			}
		}
	}

	// force all of the -1's at the end with a bubble sort
	
	float tmpWeight;
	int tmpIndex;
	// bubble sort the bones.
	for( j = pBoneWeight->numbones; j > 1; j-- )
	{
		int k;
		for( k = 0; k < j - 1; k++ )
		{
			if( ( pBoneWeight->weight[k] == 0.0f ) && ( pBoneWeight->weight[k+1] != 0.0f ) )
			{
				// swap
				tmpIndex = pBoneWeight->bone[k];
				tmpWeight = pBoneWeight->weight[k];
				pBoneWeight->bone[k] = pBoneWeight->bone[k+1];
				pBoneWeight->weight[k] = pBoneWeight->weight[k+1];
				pBoneWeight->bone[k+1] = tmpIndex;
				pBoneWeight->weight[k+1] = tmpWeight;
			}
		}
	}
	pBoneWeight->numbones = realNumBones;
}

static void MergeLikeBoneIndicesWithinVerts( studiohdr_t *pHdr )
{
	int bodyPartID, modelID, vertID;
	for( bodyPartID = 0; bodyPartID < pHdr->numbodyparts; bodyPartID++ )
	{
		mstudiobodyparts_t *pBodyPart = pHdr->pBodypart( bodyPartID );
		for( modelID = 0; modelID < pBodyPart->nummodels; modelID++ )
		{
			mstudiomodel_t *pModel = pBodyPart->pModel( modelID );
			for( vertID = 0; vertID < pModel->numvertices; vertID++ )
			{
				const mstudio_modelvertexdata_t *vertData = pModel->GetVertexData();
				Assert( vertData ); // This can only return NULL on X360 for now
				mstudioboneweight_t *pBoneWeight = vertData->BoneWeights( vertID );
				MergeLikeBoneIndicesWithinVert( pBoneWeight );
			}
		}
	}
}

void COptimizedModel::PrintBoneStateChanges( studiohdr_t *phdr, int lod )
{
	FileHeader_t *header = ( FileHeader_t * )m_FileBuffer->GetPointer( 0 );
	for( int bodyPartID = 0; bodyPartID < header->numBodyParts; bodyPartID++ )
	{
		BodyPartHeader_t *bodyPart = header->pBodyPart( bodyPartID );
//		mstudiobodyparts_t *pStudioBodyPart = phdr->pBodypart( bodyPartID );
//		for( int lodID = 0; lodID < header->numLODs; lodID++ )
		int lodID = lod;
		{
			for( int modelID = 0; modelID < bodyPart->numModels; modelID++ )
			{
				ModelHeader_t *model = bodyPart->pModel( modelID );
//				mstudiomodel_t *pStudioModel = pStudioBodyPart->pModel( modelID );
				ModelLODHeader_t *pLOD = model->pLOD( lodID );
				for( int meshID = 0; meshID < pLOD->numMeshes; meshID++ )
				{
					MeshHeader_t *mesh = pLOD->pMesh( meshID );
//					mstudiomesh_t *pStudioMesh = pStudioModel->pMesh( meshID );
					for( int stripGroupID = 0; stripGroupID < mesh->numStripGroups; stripGroupID++ )
					{
						StripGroupHeader_t *pStripGroup = mesh->pStripGroup( stripGroupID );
						for( int stripID = 0; stripID < pStripGroup->numStrips; stripID++ )
						{
							StripHeader_t *pStrip = pStripGroup->pStrip( stripID );
							for( int boneStateChangeID = 0; boneStateChangeID < pStrip->numBoneStateChanges; boneStateChangeID++ )
							{
								BoneStateChangeHeader_t *pBoneStateChange = pStrip->pBoneStateChange( boneStateChangeID );
								printf( "bone change: hwid: %d boneid: %d (%s)\n", 
									( int )pBoneStateChange->hardwareID, 
									( int )pBoneStateChange->newBoneID,
									g_bonetable[pBoneStateChange->newBoneID].name);
							}
						}
					}
				}
			}
		}
	}
}

void COptimizedModel::PrintVerts( studiohdr_t *phdr, int lod )
{
	FileHeader_t *header = ( FileHeader_t * )m_FileBuffer->GetPointer( 0 );
	for( int bodyPartID = 0; bodyPartID < header->numBodyParts; bodyPartID++ )
	{
		BodyPartHeader_t *bodyPart = header->pBodyPart( bodyPartID );
		mstudiobodyparts_t *pStudioBodyPart = phdr->pBodypart( bodyPartID );
//		for( int lodID = 0; lodID < header->numLODs; lodID++ )
		int lodID = lod;
		{
			for( int modelID = 0; modelID < bodyPart->numModels; modelID++ )
			{
				ModelHeader_t *model = bodyPart->pModel( modelID );
				mstudiomodel_t *pStudioModel = pStudioBodyPart->pModel( modelID );
				ModelLODHeader_t *pLOD = model->pLOD( lodID );
				for( int meshID = 0; meshID < pLOD->numMeshes; meshID++ )
				{
					MeshHeader_t *mesh = pLOD->pMesh( meshID );
					mstudiomesh_t *pStudioMesh = pStudioModel->pMesh( meshID );
					for( int stripGroupID = 0; stripGroupID < mesh->numStripGroups; stripGroupID++ )
					{
						StripGroupHeader_t *pStripGroup = mesh->pStripGroup( stripGroupID );
						for( int vertID = 0; vertID < pStripGroup->numVerts; vertID++ )
						{
							PrintVert( pStripGroup->pVertex( vertID ), pStudioModel, pStudioMesh );
						}
					}
				}
			}
		}
	}
}

static int CalcNumMaterialReplacements()
{
 	int i;
	int numReplacements = 0;
	int numLODs = g_ScriptLODs.Size();
	for( i = 0; i < numLODs; i++ )
	{
		LodScriptData_t &scriptLOD = g_ScriptLODs[i];
		numReplacements += scriptLOD.materialReplacements.Size();
	}
	return numReplacements;
}

//-----------------------------------------------------------------------------
//
// Main entry point
//
//-----------------------------------------------------------------------------

bool COptimizedModel::OptimizeFromStudioHdr( studiohdr_t *pHdr, s_bodypart_t *pSrcBodyParts, 
		int vertCacheSize, 
		bool usesFixedFunction, bool bForceSoftwareSkin, bool bHWFlex, int maxBonesPerVert, int maxBonesPerTri, 
		int maxBonesPerStrip, const char *pFileName, const char *glViewFileName )
{
	Assert( maxBonesPerVert <= MAX_NUM_BONES_PER_VERT );
	Assert( maxBonesPerTri <= MAX_NUM_BONES_PER_TRI );
	Assert( maxBonesPerStrip <= MAX_NUM_BONES_PER_STRIP );
	
	MergeLikeBoneIndicesWithinVerts( pHdr );
	
	// Some initialization shite
	SetupMeshProcessing( pHdr, vertCacheSize, usesFixedFunction, maxBonesPerVert,
		maxBonesPerTri, maxBonesPerStrip, pFileName );

	// The dude that does it all
	TotalMeshStats_t stats;
	ProcessModel( pHdr, pSrcBodyParts, stats, bForceSoftwareSkin, bHWFlex );
	stats.m_TotalMaterialReplacements = CalcNumMaterialReplacements();

	// Write it out to disk
	WriteVTXFile( pHdr, pFileName, stats );
	
	// Write out debugging files....
	WriteGLViewFiles( pHdr, glViewFileName );
	
	//	DebugCrap( pHdr );

//	PrintBoneStateChanges( pHdr, 1 );
//	PrintVerts( pHdr, 1 );

	delete m_FileBuffer;
	m_FileBuffer = NULL;

	if( m_NumSkinnedAndFlexedVerts != 0 )
	{
		MdlWarning( "!!!!WARNING!!!!: %d flexed verts had more than one bone influence. . will use SLOW path in engine\n", m_NumSkinnedAndFlexedVerts );
	}
	
	CleanupEverything();

	return true;
}


static int numGLViewTrangles = 0;
static int numGLViewSWDegenerates = 0;
static int numGLViewHWDegenerates = 0;


enum 
{ 
	GLVIEWDRAW_TRILIST, 
	GLVIEWDRAW_TRISTRIP, 
	GLVIEWDRAW_NONE 
};

static int s_DrawMode;
static int s_LastThreeIndices[3];
static Vertex_t s_LastThreeVerts[3];
static Vector s_LastThreePositions[3];
static int s_ListID = 0;
static bool s_Shrunk[3];

//-----------------------------------------------------------------------------
//
// The following methods are used in writing out GL View files
//
//-----------------------------------------------------------------------------

void COptimizedModel::PrintVert( Vertex_t *v, mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh )
{
	printf( "vert:\n" );
#if 0
	printf( "\tposition: %f %f %f\n",
		v->position[0], v->position[1], v->position[2] );
	printf( "\tnormal: %f %f %f\n", 
		v->normal[0], v->normal[1], v->normal[2] );
	printf( "\ttexcoord: %f %f\n",
		v->texCoord[0], v->texCoord[1] );
#endif
	printf( "\torigMeshVertID: %d\n", v->origMeshVertID );
	printf( "\tnumBones: %d\n", v->numBones );
	int i;
//	for( i = 0; i < MAX_NUM_BONES_PER_VERT; i++ )
	for( i = 0; i < v->numBones; i++ )
	{
		float boneWeight = GetOrigVertBoneWeightValue( pStudioModel, pStudioMesh, v, i );
		printf( "\tboneID[%d]: %d weight: %f (%s)\n", i, ( int )v->boneID[i], boneWeight, 
		         g_bonetable[v->boneID[i]].name );
	}
}


static float RandomFloat( float min, float max )
{
	float ret;

	ret = ( ( float )rand() ) / ( float )VALVE_RAND_MAX;
	ret *= max - min;
	ret += min;
	return ret;
}

Vector& COptimizedModel::GetOrigVertPosition( mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, Vertex_t *pVert )
{
	Assert( pStudioMesh->pModel() == pStudioModel );

	const mstudio_meshvertexdata_t *vertData = pStudioMesh->GetVertexData();
	Assert( vertData ); // This can only return NULL on X360 for now
	return *vertData->Position( pVert->origMeshVertID );
}

float COptimizedModel::GetOrigVertBoneWeightValue( mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, Vertex_t *pVert, int boneID )
{
	Assert( pStudioMesh->pModel() == pStudioModel );

	const mstudio_meshvertexdata_t *vertData = pStudioMesh->GetVertexData();
	Assert( vertData ); // This can only return NULL on X360 for now
	return vertData->BoneWeights( pVert->origMeshVertID )->weight[pVert->boneWeightIndex[boneID]];
}

mstudioboneweight_t &COptimizedModel::GetOrigVertBoneWeight( mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, Vertex_t *pVert )
{
	Assert( pStudioMesh->pModel() == pStudioModel );

	const mstudio_meshvertexdata_t *vertData = pStudioMesh->GetVertexData();
	Assert( vertData ); // This can only return NULL on X360 for now
	return *vertData->BoneWeights( pVert->origMeshVertID );
}

int COptimizedModel::GetOrigVertBoneIndex( mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, Vertex_t *pVert, int boneID )
{
	Assert( pStudioMesh->pModel() == pStudioModel );

	const mstudio_meshvertexdata_t *vertData = pStudioMesh->GetVertexData();
	Assert( vertData ); // This can only return NULL on X360 for now
	return vertData->BoneWeights( pVert->origMeshVertID )->bone[pVert->boneWeightIndex[boneID]];
}

void COptimizedModel::SetMeshPropsColor( unsigned int meshFlags, Vector& color )
{
	if( meshFlags & MESH_IS_TEETH )
	{
		color[0] = 1.0f; color[1] = 0.0f; color[2] = 0.0f;
	}
	else if( meshFlags & MESH_IS_EYES )
	{
		color[0] = 1.0f; color[1] = 1.0f; color[2] = 0.0f;
	}
	else
	{
		color[0] = 0.0f; color[1] = 1.0f; color[2] = 0.0f;
	}
}

void COptimizedModel::SetFlexedAndSkinColor( unsigned int glViewFlags, unsigned int stripGroupFlags, Vector& color )
{
	if( ( glViewFlags & WRITEGLVIEW_SHOWFLEXED ) && ( glViewFlags & WRITEGLVIEW_SHOWSW ) )
	{
		if( ( stripGroupFlags & STRIPGROUP_IS_FLEXED ) && 
			( stripGroupFlags & STRIPGROUP_IS_HWSKINNED ) )
		{
			// flexed and hw skinned = yellow
			color[0] = 1.0f; color[1] = 1.0f; color[2] = 0.0f;
		}
		else if( !( stripGroupFlags & STRIPGROUP_IS_FLEXED ) && 
				 ( stripGroupFlags & STRIPGROUP_IS_HWSKINNED ) )
		{
			// not flexed and hw skinned = green
			color[0] = 0.0f; color[1] = 1.0f; color[2] = 0.0f;
		}
		else if( !( stripGroupFlags & STRIPGROUP_IS_FLEXED ) && 
				 !( stripGroupFlags & STRIPGROUP_IS_HWSKINNED ) )
		{
			// not flexed and sw skinned = blue
			color[0] = 0.0f; color[1] = 0.0f; color[2] = 1.0f;
		}
		else if( ( stripGroupFlags & STRIPGROUP_IS_FLEXED ) && 
				 !( stripGroupFlags & STRIPGROUP_IS_HWSKINNED ) )
		{
			// flexed and sw skinned = red
			color[0] = 1.0f; color[1] = 0.0f; color[2] = 0.0f;
		}
		else
		{
			Assert( 0 );
		}

	}
	else if( glViewFlags & WRITEGLVIEW_SHOWFLEXED )
	{
		if( stripGroupFlags & STRIPGROUP_IS_FLEXED )
		{
			color[0] = 1.0f; color[1] = 0.0f; color[2] = 0.0f;
		}
		else
		{
			color[0] = 0.0f; color[1] = 1.0f; color[2] = 0.0f;
		}
	}
	else if( glViewFlags & WRITEGLVIEW_SHOWSW )
	{
		if( stripGroupFlags & STRIPGROUP_IS_HWSKINNED )
		{
			color[0] = 0.0f; color[1] = 1.0f; color[2] = 0.0f;
		}
		else
		{
			color[0] = 1.0f; color[1] = 0.0f; color[2] = 0.0f;
		}
	}
}

void COptimizedModel::SetColorFromNumVertexBones( int numBones, Vector& color )
{
	Vector numBonesColor[5] = {
		Vector( 0.0f, 0.0f, 0.0f ), // 0 bones = black
		Vector( 0.0f, 1.0f, 0.0f ), // 1 bone  = green
		Vector( 1.0f, 1.0f, 0.0f ), // 2 bones = yellow
		Vector( 0.0f, 0.0f, 1.0f ), // 3 bones = blue
		Vector( 1.0f, 0.0f, 0.0f )  // 4 bones = red
	};
	Assert( numBones >= 0 && numBones <= 4 );
	VectorCopy( numBonesColor[numBones], color );	
}


void COptimizedModel::DrawGLViewTriangle( FILE *fp, Vector& pos1, Vector& pos2, Vector& pos3,
										 Vector& color1, Vector& color2, Vector& color3 )
{
	numGLViewTrangles++;
	fprintf( fp, "3\n" );
	fprintf( fp, "%f %f %f %f %f %f\n", pos1[0], pos1[1], pos1[2], color1[0], color1[1], color1[2] );
	fprintf( fp, "%f %f %f %f %f %f\n", pos2[0], pos2[1], pos2[2], color2[0], color2[1], color2[2] );
	fprintf( fp, "%f %f %f %f %f %f\n", pos3[0], pos3[1], pos3[2], color3[0], color3[1], color3[2] );
}

//-----------------------------------------------------------------------------
// use index to test for degenerates. . isn't used for anything else.
//-----------------------------------------------------------------------------

void COptimizedModel::GLViewVert( FILE *fp, Vertex_t vert, int index, 
	Vector& color, mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, 
	bool showSubStrips, float shrinkFactor )
{
//	CheckVertBoneWeights( &vert, pStudioModel, pStudioMesh );
	Assert( s_DrawMode != GLVIEWDRAW_NONE );
	int id = s_ListID % 3;
	s_LastThreeIndices[id] = index;
	s_LastThreeVerts[id] = vert;
	s_Shrunk[id] = false;
	VectorCopy( GetOrigVertPosition( pStudioModel, pStudioMesh, &s_LastThreeVerts[id] ), s_LastThreePositions[id] );
	if( s_DrawMode == GLVIEWDRAW_TRILIST )
	{
		// trilist
		if( id == 2 )
		{
			ShrinkVerts( shrinkFactor );
			DrawGLViewTriangle( fp,
				s_LastThreePositions[0],
				s_LastThreePositions[1],
				s_LastThreePositions[2],
				color, color, color );
		}
	}
	else
	{
		// tristrip
		if( s_ListID >= 2 )
		{
			// spit out the triangle with both facings. . doesn't matter if we
			// get the facing right for glview
			if( s_LastThreeIndices[0] == s_LastThreeIndices[1] ||
				s_LastThreeIndices[1] == s_LastThreeIndices[2] ||
				s_LastThreeIndices[0] == s_LastThreeIndices[2] )
			{
				// skip degenerate triangles
				numGLViewHWDegenerates++;
				if( showSubStrips )
				{
					RandomColor( color );
				}
			}
			else if( !( s_ListID & 1 ) )
			{
				ShrinkVerts( shrinkFactor );
				DrawGLViewTriangle( fp,
					s_LastThreePositions[(id+0-2+3)%3],
					s_LastThreePositions[(id+1-2+3)%3],
					s_LastThreePositions[(id+2-2+3)%3],
					color, color, color );
			}
			else
			{
				ShrinkVerts( shrinkFactor );
				DrawGLViewTriangle( fp,
					s_LastThreePositions[(id+2-2+3)%3],
					s_LastThreePositions[(id+1-2+3)%3],
					s_LastThreePositions[(id+0-2+3)%3],
					color, color, color );
			}
		}
	}

	s_ListID++;
}

void COptimizedModel::GLViewDrawEnd( void )
{
	s_DrawMode = GLVIEWDRAW_NONE;
}

/*
void COptimizedModel::DebugCrap( studiohdr_t *phdr )
{
	FileHeader_t *header = ( FileHeader_t * )m_FileBuffer->GetPointer( 0 );
	for( int bodyPartID = 0; bodyPartID < header->numBodyParts; bodyPartID++ )
	{
		BodyPartHeader_t *bodyPart = header->pBodyPart( bodyPartID );
		mstudiobodyparts_t *pStudioBodyPart = phdr->pBodypart( bodyPartID );
		for( int modelID = 0; modelID < bodyPart->numModels; modelID++ )
		{
			ModelHeader_t *model = bodyPart->pModel( modelID );
			mstudiomodel_t *pStudioModel = pStudioBodyPart->pModel( modelID );
			for( int lodID = 0; lodID < model->numLODs; lodID++ )
			{
				char tmp[256];
				sprintf( tmp, "crap.lod%d", lodID );
				printf( "writing %s\n", tmp );
				FILE *fp = fopen( tmp, "w" );
				if( !fp )
				{
					printf( "can't write crap file %s\n", tmp );
					return;
				}
				
				ModelLODHeader_t *pLOD = model->pLOD( lodID );
				for( int meshID = 0; meshID < pLOD->numMeshes; meshID++ )
				{
					MeshHeader_t *mesh = pLOD->pMesh( meshID );
					mstudiomesh_t *pStudioMesh = pStudioModel->pMesh( meshID );
					for( int stripGroupID = 0; stripGroupID < mesh->numStripGroups; stripGroupID++ )
					{
						StripGroupHeader_t *pStripGroup = mesh->pStripGroup( stripGroupID );
						for( int stripID = 0; stripID < pStripGroup->numStrips; stripID++ )
						{
							StripHeader_t *pStrip = pStripGroup->pStrip( stripID );
							for( int indexID = 0; indexID < pStrip->numIndices; indexID++ )
							{
								int id = *pStripGroup->pIndex( indexID + pStrip->indexOffset );
								Vertex_t& vert = *pStripGroup->pVertex( id );
								Vector& vertPos = GetOrigVertPosition( pStudioModel, pStudioMesh, &vert );
								fprintf( fp, "mesh: %04d origvertid: %04d pos: %0.2f %0.2f %0.2f ", meshID, vert.origMeshVertID, vertPos[0], vertPos[1], vertPos[2] );
								int i;
								for( i = 0; i < vert.numBones; i++ )
								{
									float boneWeight;
									boneWeight = GetOrigVertBoneWeightValue( pStudioModel, pStudioMesh, &vert, i );
									int boneID;
									boneID = GetOrigVertBoneIndex( pStudioModel, pStudioMesh, &vert, i );
									fprintf( fp, "bone: %d %0.1f ", boneID, boneWeight );
								}
								fprintf( fp, "\n" );
							}
						}
					}
				}
				fclose( fp );
			}
		}
	}
}
*/


void COptimizedModel::WriteGLViewFile( studiohdr_t *phdr, const char *pFileName, unsigned int flags, float shrinkFactor )
{
	Vector color;
	RandomColor( color );
	FileHeader_t *header = ( FileHeader_t * )m_FileBuffer->GetPointer( 0 );
	for( int bodyPartID = 0; bodyPartID < header->numBodyParts; bodyPartID++ )
	{
		BodyPartHeader_t *bodyPart = header->pBodyPart( bodyPartID );
		mstudiobodyparts_t *pStudioBodyPart = phdr->pBodypart( bodyPartID );
		for( int modelID = 0; modelID < bodyPart->numModels; modelID++ )
		{
			ModelHeader_t *model = bodyPart->pModel( modelID );
			mstudiomodel_t *pStudioModel = pStudioBodyPart->pModel( modelID );
			for( int lodID = 0; lodID < model->numLODs; lodID++ )
			{
				char tmp[256];
				sprintf( tmp, "%s.lod%d", pFileName, lodID );
				printf( "writing %s\n", tmp );
				CPlainAutoPtr< CP4File > spFile( g_p4factory->AccessFile( tmp ) );
				spFile->Edit();
				FILE *fp = fopen( tmp, "w" );
				if( !fp )
				{
					printf( "can't write glview file %s\n", tmp );
					return;
				}

/*
				// write out tangent space vectors
				int vertID;
				for( vertID = 0; vertID < pStudioModel->numvertices; vertID++ )
				{
					const Vector &pos = *pStudioModel->pVertex( vertID );
					const Vector &norm = *pStudioModel->pNormal( vertID );
					const Vector4D &sVect = *pStudioModel->pTangentS( vertID );
					Vector tmpVect;
					tmpVect = pos + norm * .15f;
					fprintf( fp, "2\n" );
					fprintf( fp, "%f %f %f 0.0 0.0 1.0\n", pos[0], pos[1], pos[2] );
					fprintf( fp, "%f %f %f 0.0 0.0 1.0\n", tmpVect[0], tmpVect[1], tmpVect[2] );

					Vector tmpSVect( sVect[0], sVect[1], sVect[2] );
					tmpVect = pos + tmpSVect * .15f;
					fprintf( fp, "2\n" );
					fprintf( fp, "%f %f %f 1.0 0.0 0.0\n", pos[0], pos[1], pos[2] );
					fprintf( fp, "%f %f %f 1.0 0.0 0.0\n", tmpVect[0], tmpVect[1], tmpVect[2] );

					Vector tmpTVect;
					CrossProduct( norm, tmpSVect, tmpTVect );
					tmpTVect *= sVect[3];
					tmpVect = pos + tmpTVect * .15f;

					fprintf( fp, "2\n" );
					fprintf( fp, "%f %f %f 0.0 1.0 0.0\n", pos[0], pos[1], pos[2] );
					fprintf( fp, "%f %f %f 0.0 1.0 0.0\n", tmpVect[0], tmpVect[1], tmpVect[2] );

				}
				continue;
				*/
  
				ModelLODHeader_t *pLOD = model->pLOD( lodID );
				for( int meshID = 0; meshID < pLOD->numMeshes; meshID++ )
				{
					MeshHeader_t *mesh = pLOD->pMesh( meshID );
					mstudiomesh_t *pStudioMesh = pStudioModel->pMesh( meshID );
					if( flags & WRITEGLVIEW_SHOWMESH )
					{
						RandomColor( color );
					}
					if( flags & WRITEGLVIEW_SHOWMESHPROPS )
					{
						SetMeshPropsColor( mesh->flags, color );
					}
					for( int stripGroupID = 0; stripGroupID < mesh->numStripGroups; stripGroupID++ )
					{
						if( flags & WRITEGLVIEW_SHOWSTRIPGROUP )
						{
							RandomColor( color );
						}
						StripGroupHeader_t *pStripGroup = mesh->pStripGroup( stripGroupID );
						
						SetFlexedAndSkinColor( flags, pStripGroup->flags, color );	
						for( int stripID = 0; stripID < pStripGroup->numStrips; stripID++ )
						{
							StripHeader_t *pStrip = pStripGroup->pStrip( stripID );
							if( flags & WRITEGLVIEW_SHOWSTRIP )
							{
								RandomColor( color );
							}
							
							if( flags & WRITEGLVIEW_SHOWSTRIPNUMBONES )
							{
								switch (pStrip->numBones)
								{
								case 0:
								case 1:
									color.Init( 0, 0, 255 );
									break;
									
								case 2:
									color.Init( 0, 255, 0 );
									break;
									
								case 3:
									color.Init( 255, 255, 0 );
									break;
									
								case 4:
									color.Init( 255, 0, 0 );
									break;
								}
							}
							
							GLViewDrawBegin( ( pStrip->flags & STRIP_IS_TRILIST ) ? GLVIEWDRAW_TRILIST : GLVIEWDRAW_TRISTRIP );
							for( int indexID = 0; indexID < pStrip->numIndices; indexID++ )
							{
								int id = *pStripGroup->pIndex( indexID + pStrip->indexOffset );
								Vertex_t& vert = *pStripGroup->pVertex( id );
								
								if( flags & WRITEGLVIEW_SHOWVERTNUMBONES )
								{
									switch (vert.numBones)
									{
									case 0:
									case 1:
										color.Init( 0, 0, 255 );
										break;
										
									case 2:
										color.Init( 0, 255, 0 );
										break;
										
									case 3:
										color.Init( 255, 255, 0 );
										break;
										
									case 4:
										color.Init( 255, 0, 0 );
										break;
									}
								}
								
								GLViewVert( fp, vert, id, color, pStudioModel, pStudioMesh, 
									( flags & WRITEGLVIEW_SHOWSUBSTRIP ) ? true : false, shrinkFactor );
							}
							GLViewDrawEnd();
						}
					}
				}
				fclose( fp );
				spFile->Add();
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Write out all GL View files
//-----------------------------------------------------------------------------

void COptimizedModel::WriteGLViewFiles( studiohdr_t *pHdr, char const* glViewFileName )
{
	if( !g_bDumpGLViewFiles )
		return;

	char tmpFileName[128];
	strcpy( tmpFileName, glViewFileName );
	strcat( tmpFileName, ".mesh" );
	WriteGLViewFile( pHdr, tmpFileName, WRITEGLVIEW_SHOWMESH, .8f );
	strcpy( tmpFileName, glViewFileName );
	strcat( tmpFileName, ".stripgroup" );
	WriteGLViewFile( pHdr, tmpFileName, WRITEGLVIEW_SHOWSTRIPGROUP, .8f );
	strcpy( tmpFileName, glViewFileName );
	strcat( tmpFileName, ".strip" );
	WriteGLViewFile( pHdr, tmpFileName, WRITEGLVIEW_SHOWSTRIP, .8f );
	strcpy( tmpFileName, glViewFileName );
	strcat( tmpFileName, ".substrip" );
	WriteGLViewFile( pHdr, tmpFileName, WRITEGLVIEW_SHOWSUBSTRIP, .97f );
	strcpy( tmpFileName, glViewFileName );
	strcat( tmpFileName, ".flexed" );
	WriteGLViewFile( pHdr, tmpFileName, WRITEGLVIEW_SHOWFLEXED, .8f );
	strcpy( tmpFileName, glViewFileName );
	strcat( tmpFileName, ".sw" );
	WriteGLViewFile( pHdr, tmpFileName, WRITEGLVIEW_SHOWSW, .8f );
	strcpy( tmpFileName, glViewFileName );
	strcat( tmpFileName, ".flexedandsw" );
	WriteGLViewFile( pHdr, tmpFileName, WRITEGLVIEW_SHOWSW | WRITEGLVIEW_SHOWFLEXED, .8f );
	strcpy( tmpFileName, glViewFileName );
	strcat( tmpFileName, ".meshprops" );
	WriteGLViewFile( pHdr, tmpFileName, WRITEGLVIEW_SHOWMESHPROPS, .8f );
	strcpy( tmpFileName, glViewFileName );
	strcat( tmpFileName, ".vertnumbones" );
	WriteGLViewFile( pHdr, tmpFileName, WRITEGLVIEW_SHOWVERTNUMBONES, 1.0f );
	strcpy( tmpFileName, glViewFileName );
	strcat( tmpFileName, ".stripnumbones" );
	WriteGLViewFile( pHdr, tmpFileName, WRITEGLVIEW_SHOWSTRIPNUMBONES, 1.0f );
}


void COptimizedModel::GLViewDrawBegin( int mode )
{
	s_DrawMode = mode;
	s_ListID = 0;
}

void COptimizedModel::ShrinkVerts( float shrinkFactor )
{
	Vector center;
	Vector delta;

	VectorCopy( s_LastThreePositions[0], center );
	VectorAdd( center, s_LastThreePositions[1], center );
	VectorAdd( center, s_LastThreePositions[2], center );
	VectorScale( center, 1.0f / 3.0f, center );

	int i;
	for( i = 0; i < 3; i++ )
	{
		if( s_Shrunk[i] )
		{
			continue;
		}
		VectorSubtract( s_LastThreePositions[i], center, delta );
		VectorScale( delta, shrinkFactor, delta );
		VectorAdd( center, delta, s_LastThreePositions[i] );
		s_Shrunk[i] = true;
	}
}

void COptimizedModel::CheckVertBoneWeights( Vertex_t *pVert, mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh )
{
	int i;
	float sum = 0;
	for( i = 0; i < MAX_NUM_BONES_PER_VERT; i++ )
	{
		float boneWeight = GetOrigVertBoneWeightValue( pStudioModel, pStudioMesh, pVert, i );
		sum += boneWeight;
	}
	Assert( sum > 0.95f && sum < 1.1f );
}


#define CACHE_INEFFICIENCY 6
void COptimizedModel::ShowStats( void )
{
	int totalHWTriangles = 0;
	int totalHWDegenerates = 0;
	int totalHWIndices = 0;
	int totalHWVertexCacheHits = 0;
	int totalHWVertexCacheMisses = 0;
	int totalSWTriangles = 0;
	int totalSWDegenerates = 0;
	int totalSWIndices = 0;
	int totalSWVertexCacheHits = 0;
	int totalSWVertexCacheMisses = 0;
	
	CHardwareVertexCache hardwareVertexCache;
	hardwareVertexCache.Init( m_VertexCacheSize - CACHE_INEFFICIENCY );
	
	FileHeader_t *header = ( FileHeader_t * )m_FileBuffer->GetPointer( 0 );
	printf( "header: %d body parts\n", header->numBodyParts );
	for( int bodyPartID = 0; bodyPartID < header->numBodyParts; bodyPartID++ )
	{
		BodyPartHeader_t *bodyPart = header->pBodyPart( bodyPartID );
		printf( "  bodyPart %d: %d models\n", bodyPartID, bodyPart->numModels );
		for( int modelID = 0; modelID < bodyPart->numModels; modelID++ )
		{
			ModelHeader_t *model = bodyPart->pModel( modelID );
			printf( "    model: %d lods\n", model->numLODs );
			for( int lodID = 0; lodID < 1; lodID++ )
			{
				ModelLODHeader_t *pLOD = model->pLOD( lodID );
				printf( "     lod: %d meshes\n", pLOD->numMeshes );
				for( int meshID = 0; meshID < pLOD->numMeshes; meshID++ )
				{
					MeshHeader_t *mesh = pLOD->pMesh( meshID );
					printf( "       mesh %d: %d stripsgroups\n", meshID, mesh->numStripGroups );
					for( int stripGroupID = 0; stripGroupID < mesh->numStripGroups; stripGroupID++ )
					{
						StripGroupHeader_t *pStripGroup = mesh->pStripGroup( stripGroupID );
						for( int stripID = 0; stripID < pStripGroup->numStrips; stripID++ )
						{
							for( int stripID = 0; stripID < pStripGroup->numStrips; stripID++ )
							{
								int lastThreeIndices[3];
								
								StripHeader_t *pStrip = pStripGroup->pStrip( stripID );
								printf( "         strip: %d numIndices: %d indexOffset: %d\n", stripID, pStrip->numIndices, pStrip->indexOffset );
								
								hardwareVertexCache.Flush();
								if( pStrip->flags & STRIP_IS_TRISTRIP )
								{
									for( int indexID = 0; indexID < pStrip->numIndices; indexID++ )
									{
										totalHWIndices++;
										int newVertOffset = indexID % 3;
										lastThreeIndices[newVertOffset] = *pStripGroup->pIndex( indexID + pStrip->indexOffset );
										if( !hardwareVertexCache.IsPresent( lastThreeIndices[newVertOffset] ) )
										{
											totalHWVertexCacheMisses++;
											hardwareVertexCache.Insert( lastThreeIndices[newVertOffset] );
										}
										else
										{
											totalHWVertexCacheHits++;
										}
										if( indexID >= 2 )
										{
											totalHWTriangles++;
											if( lastThreeIndices[0] != lastThreeIndices[1] &&
												lastThreeIndices[1] != lastThreeIndices[2] )
											{
												
											}
											else
											{
												totalHWDegenerates++;
											}
										}
									}
								}
								else
								{
									Assert( pStrip->numIndices % 3 == 0 );
									totalHWTriangles += pStrip->numIndices / 3;
									for( int indexID = 0; indexID < pStrip->numIndices; indexID++ )
									{
										int newVertOffset = indexID % 3;
										int index = *pStripGroup->pIndex( indexID + pStrip->indexOffset );
//										printf( "%d\n", index );
										lastThreeIndices[newVertOffset] = index;
										if( newVertOffset == 2 )
										{
											if( lastThreeIndices[0] == lastThreeIndices[1] ||
												lastThreeIndices[1] == lastThreeIndices[2] ||
												lastThreeIndices[0] == lastThreeIndices[2] )
											{
//												printf( "degenerate triangle!!!! %d %d %d\n", lastThreeIndices[0], lastThreeIndices[1], lastThreeIndices[2] );
												totalHWDegenerates++;
											}
										}
										totalHWIndices++;
										if( !hardwareVertexCache.IsPresent( index ) )
										{
											totalHWVertexCacheMisses++;
											hardwareVertexCache.Insert( index );
										}
										else
										{
											totalHWVertexCacheHits++;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	int totalRealHWTriangles = totalHWTriangles - totalHWDegenerates;
	int totalRealSWTriangles = totalSWTriangles - totalSWDegenerates;
	printf( "TotalHWTriangles: %d\n", totalHWTriangles );
	printf( "TotalHWDegenerates: %d\n", totalHWDegenerates );
	printf( "TotalRealHWTriangles: %d\n", totalRealHWTriangles );
	printf( "TotalHWIndices: %d\n", totalHWIndices );
	printf( "HW real tris/index: %f\n", ( float )totalRealHWTriangles / ( float )totalHWIndices );
	printf( "totalHWVertexCacheHits: %d\n", totalHWVertexCacheHits );
	printf( "totalHWVertexCacheMisses: %d\n", totalHWVertexCacheMisses );
	printf( "HW vertex cache hit/miss ratio: %f\n", ( float )totalHWVertexCacheHits / ( float )totalHWVertexCacheMisses );

	printf( "TotalSWTriangles: %d\n", totalSWTriangles );
	printf( "TotalSWDegenerates: %d\n", totalSWDegenerates );
	printf( "TotalRealSWTriangles: %d\n", totalRealSWTriangles );
	printf( "TotalSWIndices: %d\n", totalSWIndices );
	printf( "SW real tris/index: %f\n", ( float )totalRealSWTriangles / ( float )totalSWIndices );
	printf( "totalSWVertexCacheHits: %d\n", totalSWVertexCacheHits );
	printf( "totalSWVertexCacheMisses: %d\n", totalSWVertexCacheMisses );
	printf( "SW vertex cache hit/miss ratio: %f\n", ( float )totalSWVertexCacheHits / ( float )totalSWVertexCacheMisses );
}

void COptimizedModel::CheckVert( Vertex_t *pVert, int maxBonesPerTri, int maxBonesPerVert )
{
#ifndef IGNORE_BONES

#ifdef _DEBUG
	int offset = ( int )( ( unsigned char * )pVert - ( unsigned char * )m_FileBuffer->GetPointer( 0 ) );
	Assert( offset >= m_VertsOffset && offset < m_IndicesOffset );
	Assert( ( ( offset - m_VertsOffset ) % sizeof( Vertex_t ) ) == 0 );
#endif

	int j;
	for( j = 0; j < maxBonesPerVert; j++ )
	{
		if( pVert->boneID[j] != -1 )
		{
			Assert( pVert->boneID[j] >= 0 && pVert->boneID[j] < maxBonesPerTri );
		}
#if 0
		if( pVert->boneWeights[j] != 0 )
		{
			Assert( pVert->boneID[j] != -1 );
		}
#endif
	}
	// Test to make sure we are sorted.
	for( j = 0; j < maxBonesPerVert-1; j++ )
	{
#if 1
//		if( pVert->boneWeights[j] != 0 && pVert->boneWeights[j+1] != 0 )
		{
			Assert( pVert->boneID[j] < pVert->boneID[j+1] );
		}
#endif
	}
#if 0
	// Make sure that all the non-zero weights are first.
	bool foundZero = false;
	for( j = 0; j < maxBonesPerVert; j++ )
	{
		if( !foundZero )
		{
			if( pVert->boneWeights[j] == 0.0f )
			{
				foundZero = true;
			}
		}
		else
		{
			Assert( pVert->boneWeights[j] == 0.0f );
		}
	}
#endif
#endif
}

void COptimizedModel::CheckAllVerts( int maxBonesPerTri, int maxBonesPerVert )
{
	int i;
	for( i = m_VertsOffset; i < m_IndicesOffset; i += sizeof( Vertex_t ) )
	{
		Vertex_t *vert = ( Vertex_t * )m_FileBuffer->GetPointer( i );
		CheckVert( vert, maxBonesPerTri, maxBonesPerVert );
	}
}

void COptimizedModel::SortBonesWithinVertex( bool flexed, Vertex_t *vert, mstudiomodel_t *pStudioModel, mstudiomesh_t *pStudioMesh, int *globalToHardwareBoneIndex, int *hardwareToGlobalBoneIndex, int maxBonesPerTri, int maxBonesPerVert )
{
	int i;
/*
	for( i = 0; i < m_NumBones; i++ )
	{
		if( globalToHardwareBoneIndex[i] != -1 )
		{
			if( flexed )
			{
				printf( "global bone id: %d hardware bone id: %d\n",
					i, globalToHardwareBoneIndex[i] );
			}
		}
	}
*/
#if 0
	unsigned char tmpWeightIndex;
	int tmpIndex;
	int j;
	// bubble sort the bones.
	for( j = m_MaxBonesPerVert; j > 1; j-- )
	{
		int k;
		for( k = 0; k < j - 1; k++ )
		{
			if( vert->boneID[k] > vert->boneID[k+1] )
			{
				// swap
				tmpIndex = vert->boneID[k];
				tmpWeightIndex = vert->boneWeightIndex[k];
				vert->boneID[k] = vert->boneID[k+1];
				vert->boneWeightIndex[k] = vert->boneWeightIndex[k+1];
				vert->boneID[k+1] = tmpIndex;
				vert->boneWeightIndex[k+1] = tmpWeightIndex;
			}
		}
	}
#else
	
	int origBoneWeightIndex[MAX_NUM_BONES_PER_VERT];
	int zeroWeightIndex = -1;
	// find a orig vert bone index that has a zero weight
	for( i = 0; i < MAX_NUM_BONES_PER_VERT; i++ )
	{
		float boneWeight = GetOrigVertBoneWeightValue( pStudioModel, pStudioMesh, vert, i );
		if( boneWeight == 0.0f )
		{
			zeroWeightIndex = i;
			break;
		}
	}

	for( i = 0; i < MAX_NUM_BONES_PER_VERT; i++ )
	{
		origBoneWeightIndex[i] = zeroWeightIndex;
	}
	for( i = 0; i < vert->numBones; i++ )
	{
		float boneWeight = GetOrigVertBoneWeightValue( pStudioModel, pStudioMesh, vert, i );
		int globalBoneIndex = GetOrigVertBoneIndex( pStudioModel, pStudioMesh, vert, i );
//		if( vert->numBones > 1 )
		{
			if( flexed )
			{
				printf( "boneWeight: %f\n", boneWeight );
				printf( "globalBoneIndex: %d\n", globalBoneIndex );
			}
		}
		if( boneWeight > 0.0f )
		{
			int hardwareBoneIndex = globalToHardwareBoneIndex[globalBoneIndex];
			Assert( globalBoneIndex != -1 );
			origBoneWeightIndex[hardwareBoneIndex] = vert->boneWeightIndex[i];
		}
		else
		{
			int hardwareBoneIndex = globalToHardwareBoneIndex[globalBoneIndex];
			origBoneWeightIndex[hardwareBoneIndex] = zeroWeightIndex;
			Assert( zeroWeightIndex != -1 );
			Assert( globalBoneIndex == -1 );
		}
	}
//	if( vert->numBones > 1 )
	for( i = 0; i < maxBonesPerTri; i++ )
	{
		vert->boneID[i] = i;
		vert->boneWeightIndex[i] = origBoneWeightIndex[i];
		float boneWeight = GetOrigVertBoneWeightValue( pStudioModel, pStudioMesh, vert, i );
		int globalBoneIndex = GetOrigVertBoneIndex( pStudioModel, pStudioMesh, vert, i );
		if( flexed )
		{
			Assert( boneWeight >= 0.0f && boneWeight <= 1.0f );
			printf( "boneWeight: %f ", boneWeight );
			printf( "globalBoneIndex: %d ", globalBoneIndex );
			printf( "hardwareBoneID: %d\n", i );
		}
	}
	vert->numBones = maxBonesPerTri; // this may be different for software t&l stuff
#endif	
}

void COptimizedModel::RemoveRedundantBoneStateChanges( void )
{
	FileHeader_t *header = ( FileHeader_t * )m_FileBuffer->GetPointer( 0 );
	for( int bodyPartID = 0; bodyPartID < header->numBodyParts; bodyPartID++ )
	{
		BodyPartHeader_t *bodyPart = header->pBodyPart( bodyPartID );

		bool allocated[MAX_NUM_BONES_PER_STRIP];
		int hardwareBoneState[MAX_NUM_BONES_PER_STRIP];
		bool changed[MAX_NUM_BONES_PER_STRIP];
		
		// start anew with each body part
//		printf( "START BODY PARTY - RESETTING BONE MATRIX STATE\n" );
		int i;
		for( i = 0; i < MAX_NUM_BONES_PER_STRIP; i++ )
		{
			hardwareBoneState[i] = -1;
			allocated[i] = false;
		}
		
		for( int modelID = 0; modelID < bodyPart->numModels; modelID++ )
		{
			ModelHeader_t *model = bodyPart->pModel( modelID );
			for( int lodID = 0; lodID < model->numLODs; lodID++ )
			{
				ModelLODHeader_t *pLOD = model->pLOD( lodID );
				for( int meshID = 0; meshID < pLOD->numMeshes; meshID++ )
				{
					MeshHeader_t *mesh = pLOD->pMesh( meshID );
					for( int stripGroupID = 0; stripGroupID < mesh->numStripGroups; stripGroupID++ )
					{
						StripGroupHeader_t *pStripGroup = mesh->pStripGroup( stripGroupID );
						if( !( pStripGroup->flags & STRIPGROUP_IS_HWSKINNED ) )
						{
//							printf( "skipping! software skinned stripgroup\n" );
							continue;
						}
						for( int stripID = 0; stripID < pStripGroup->numStrips; stripID++ )
						{
							StripHeader_t *pStrip = pStripGroup->pStrip( stripID );
//							int startNumBoneChanges = pStrip->numBoneStateChanges;
/*
							printf( "HARDWARE BONE STATE\n" );
							for( i = 0; i < MAX_NUM_BONES_PER_STRIP; i++ )
							{
								if( allocated[i] )
								{
									printf( "\thw: %d global: %d\n", i, hardwareBoneState[i] );
								}
							}
							
							printf( "before optimization\n" );
							for( i = 0; i < pStrip->numBoneStateChanges; i++ )
							{
								printf( "\thw: %d global: %d\n", 
									( int )pStrip->pBoneStateChange( i )->hardwareID,
									( int )pStrip->pBoneStateChange( i )->newBoneID );
							}
*/
							
							for( i = 0; i < MAX_NUM_BONES_PER_STRIP; i++ )
							{
								changed[i] = false;
							}
							for( int boneStateChangeID = 0; boneStateChangeID < pStrip->numBoneStateChanges; boneStateChangeID++ )
							{
								BoneStateChangeHeader_t *boneStateChange = pStrip->pBoneStateChange( boneStateChangeID );
								Assert( boneStateChange->hardwareID >= 0 && boneStateChange->hardwareID < MAX_NUM_BONES_PER_STRIP );
								if( allocated[boneStateChange->hardwareID] && 
									hardwareBoneState[boneStateChange->hardwareID] == boneStateChange->newBoneID )
								{
									// already got this one!
								}
								else
								{
									changed[boneStateChange->hardwareID] = true;
									allocated[boneStateChange->hardwareID] = true;
									hardwareBoneState[boneStateChange->hardwareID] = boneStateChange->newBoneID;
								}
							}
/*
							// now "changed" should tell us which ones we care about.
							int curOutBoneID = 0;
							for( i = 0; i < pStrip->numBoneStateChanges; i++ )
							{
								// hack . . going to stomp over what is already there with new data.
								if( changed[i] )
								{
									BoneStateChangeHeader_t *boneStateChange = pStrip->pBoneStateChange( curOutBoneID );
									boneStateChange->hardwareID = i;
									boneStateChange->newBoneID = hardwareBoneState[i];
									curOutBoneID++;
								}
							}
							pStrip->numBoneStateChanges = curOutBoneID;
							printf( "start bone changes: %d end bone changes: %d\n", startNumBoneChanges, pStrip->numBoneStateChanges );
							printf( "after optimization\n" );
							for( i = 0; i < pStrip->numBoneStateChanges; i++ )
							{
								printf( "\thw: %d global: %d\n", 
										( int )pStrip->pBoneStateChange( i )->hardwareID,
										( int )pStrip->pBoneStateChange( i )->newBoneID );
							}

  */
						}
					}
				}
			}
		}
	}
}

static void AddMaterialReplacementsToStringTable( void )
{
 	int i, j;
	int numLODs = g_ScriptLODs.Size();
	for( i = 0; i < numLODs; i++ )
	{
		LodScriptData_t &scriptLOD = g_ScriptLODs[i];
		for( j = 0; j < scriptLOD.materialReplacements.Size(); j++ )
		{
			CLodScriptReplacement_t &materialReplacement = scriptLOD.materialReplacements[j];
			s_StringTable.AddString( materialReplacement.GetDstName() );
		}
	}
}


// Check that all replacematerial/removemesh commands map to valid source materials
void ValidateLODReplacements( studiohdr_t *pHdr )
{
	bool failed = false;
	int lodID;
	for( lodID = 0; lodID < g_ScriptLODs.Size(); lodID++ )
	{
		LodScriptData_t& scriptLOD = g_ScriptLODs[lodID];
		int j;
		for( j = 0; j < scriptLOD.meshRemovals.Count(); j++ )
		{
			const char *pName1 = scriptLOD.meshRemovals[j].GetSrcName();
			int i;
			for( i = 0; i < pHdr->numtextures; i++ )
			{
				const char *pName2 = pHdr->pTexture( i )->material->GetName();
				if( ComparePath( pName1, pName2 ) )
				{
					goto got_one;
				}
			}

			// no match
			MdlWarning( "\"%s\" doesn't match any of the materals in the model\n", pName1 );
			failed = true;
got_one:
			;
		}
	}
	if( failed )
	{
		MdlWarning( "possible materials in model:\n" );
		int i;
		for( i = 0; i < pHdr->numtextures; i++ )
		{
			MdlWarning( "\t\"%s\"\n", pHdr->pTexture( i )->material->GetName() );
		}
		MdlError( "Exiting due to errors\n" );
	}
}

void WriteOptimizedFiles( studiohdr_t *phdr, s_bodypart_t *pSrcBodyParts )
{
	char		filename[MAX_PATH];
	char		tmpFileName[MAX_PATH];
	char		glViewFilename[MAX_PATH];
	
	ValidateLODReplacements( phdr );
	
	s_StringTable.Purge();
	
	// hack!  This should really go in the mdl file since it's common to all LODs.
	AddMaterialReplacementsToStringTable();
	
	V_strcpy_safe( filename, gamedir );
//	if( *g_pPlatformName )
//	{
//		strcat( filename, "platform_" );
//		strcat( filename, g_pPlatformName );
//		strcat( filename, "/" );	
//	}
	V_strcat_safe( filename, "models/" );	
	V_strcat_safe( filename, outname );
	Q_StripExtension( filename, filename, sizeof( filename ) );

	V_strcpy_safe( tmpFileName, filename );
	V_strcat_safe( tmpFileName, ".sw.vtx" );
	V_strcpy_safe( glViewFilename, filename );
	V_strcat_safe( glViewFilename, ".sw.glview" );
	bool bForceSoftwareSkinning = phdr->numbones > 0 && !g_staticprop;
	s_OptimizedModel.OptimizeFromStudioHdr( phdr, pSrcBodyParts,
											512,	//vert cache size FIXME: figure out the correct size for L1
											false, /* doesn't use fixed function */
											bForceSoftwareSkinning,	// force software skinning if not static prop
											false, // No hardware flex
											3,		// bones/vert
											3*3,		// bones/tri
											512,	// bones/strip
											tmpFileName, glViewFilename );

	V_strcpy_safe( tmpFileName, filename );
	V_strcat_safe( tmpFileName, ".dx80.vtx" );
	V_strcpy_safe( glViewFilename, filename );
	V_strcat_safe( glViewFilename, ".dx80.glview" );
	s_OptimizedModel.OptimizeFromStudioHdr( phdr, pSrcBodyParts,
											24 /* vert cache size (real size, not effective!)*/, 
											false, /* doesn't use fixed function */
											false, // don't force software skinning
											false, // No hardware flex
											3  /* bones/vert */, 
											9  /* bones/tri */, 
											16  /* bones/strip */,
											tmpFileName, glViewFilename );

	V_strcpy_safe( tmpFileName, filename );
	V_strcat_safe( tmpFileName, ".dx90.vtx" );
	V_strcpy_safe( glViewFilename, filename );
	V_strcat_safe( glViewFilename, ".dx90.glview" );
	s_OptimizedModel.OptimizeFromStudioHdr( phdr, pSrcBodyParts,
											24 /* vert cache size (real size, not effective!)*/, 
											false, /* doesn't use fixed function */
											false, // don't force software skinning
											true, // Hardware flex on DX9 parts
											3  /* bones/vert */, 
											9  /* bones/tri */, 
											53  /* bones/strip */,
											tmpFileName, glViewFilename );

	s_StringTable.Purge();
}

}; // namespace OptimizedModel
#pragma optimize( "", on )

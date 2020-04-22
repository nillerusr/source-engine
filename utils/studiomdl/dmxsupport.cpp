//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Loads mesh data from dmx files 
//
// $NoKeywords: $
//
//===========================================================================//

#include "studiomdl.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmematerial.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeattachment.h"
#include "movieobjects/dmeanimationlist.h"
#include "movieobjects/dmecombinationoperator.h"


void UnifyIndices( s_source_t *psource );


//-----------------------------------------------------------------------------
// Mapping of bone transforms
//-----------------------------------------------------------------------------
struct BoneTransformMap_t 
{
	// Number of bones
	int m_nBoneCount;

	// The order in which transforms appear in this list specifies their bone indices
	CDmeTransform *m_ppTransforms[MAXSTUDIOSRCBONES];

	// BoneRemap[bone index in file] == bone index in studiomdl
	int	m_pBoneRemap[MAXSTUDIOSRCBONES];
};


//-----------------------------------------------------------------------------
// Index into an s_node_t array for the default root node
//-----------------------------------------------------------------------------
static int s_nDefaultRootNode;


//-----------------------------------------------------------------------------
// Balance/speed data
//-----------------------------------------------------------------------------
static CUtlVector<float> s_Balance;
static CUtlVector<float> s_Speed;


//-----------------------------------------------------------------------------
// List of unique vertices
//-----------------------------------------------------------------------------
struct VertIndices_t
{
	int v;
	int n;
	int t;
	int balance;
	int speed;
};

static CUtlVector< VertIndices_t > s_UniqueVertices;	// A list of the unique vertices in the mesh

// Given the non-unique vertex index, return the unique vertex index
// The indices are absolute indices into s_UniqueVertices
// But as both arrays contain information for all meshes in the DMX
// The proper offset for the desired mesh must be added to the lookup
// into the map but the value returned has the offset already built in
static CUtlVector< int > s_UniqueVerticesMap;


//-----------------------------------------------------------------------------
// Delta state intermediate data [used for positions, normals, etc.]
//-----------------------------------------------------------------------------
struct DeltaIndex_t
{
	DeltaIndex_t() : m_nPositionIndex(-1), m_nNormalIndex(-1), m_nNextDelta(-1), m_nWrinkleIndex(-1), m_bInList(false) {}
	int m_nPositionIndex;	// Index into DeltaState_t::m_PositionDeltas
	int m_nNormalIndex;		// Index into DeltaState_t::m_NormalDeltas
	int m_nWrinkleIndex;	// Index into DeltaState_t::m_WrinkleDeltas
	int m_nNextDelta;		// Index into DeltaState_t::m_DeltaIndices;
	bool m_bInList;
};

struct DeltaState_t
{
	DeltaState_t() : m_nDeltaCount( 0 ), m_nFirstDelta( -1 ) {}

	CUtlString m_Name;
	CUtlVector< Vector > m_PositionDeltas;
	CUtlVector< Vector > m_NormalDeltas;
	CUtlVector< float > m_WrinkleDeltas;
	CUtlVector< DeltaIndex_t > m_DeltaIndices;
	int m_nDeltaCount;
	int m_nFirstDelta;
};


// NOTE: This is a temporary which loses its state once Load_DMX is exited.
static CUtlVector<DeltaState_t> s_DeltaStates;


//-----------------------------------------------------------------------------
// Finds or adds delta states. These pointers are invalidated by calling FindOrAddDeltaState again
//-----------------------------------------------------------------------------
static DeltaState_t* FindOrAddDeltaState( const char *pDeltaStateName, int nBaseStateVertexCount )
{
	int nCount = s_DeltaStates.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( s_DeltaStates[i].m_Name, pDeltaStateName ) )
		{
			MdlWarning( "Unsupported duplicate delta state named \"%s\" in DMX file\n", pDeltaStateName );

			s_DeltaStates[i].m_DeltaIndices.EnsureCount( nBaseStateVertexCount );
			return &s_DeltaStates[i];
		}
	}

	int j = s_DeltaStates.AddToTail();
	s_DeltaStates[j].m_Name = pDeltaStateName;
	s_DeltaStates[j].m_DeltaIndices.SetCount( nBaseStateVertexCount );
	return &s_DeltaStates[j];
}


//-----------------------------------------------------------------------------
// Loads the vertices from the model
//-----------------------------------------------------------------------------
static bool DefineUniqueVertices( CDmeVertexData *pBindState, int nStartingUniqueCount )
{
	const CUtlVector<int> &positionIndices = pBindState->GetVertexIndexData( CDmeVertexData::FIELD_POSITION );
	const CUtlVector<int> &normalIndices = pBindState->GetVertexIndexData( CDmeVertexData::FIELD_NORMAL );
	const CUtlVector<int> &texcoordIndices = pBindState->GetVertexIndexData( CDmeVertexData::FIELD_TEXCOORD );
	const CUtlVector<int> &balanceIndices = pBindState->GetVertexIndexData( CDmeVertexData::FIELD_BALANCE );
	const CUtlVector<int> &speedIndices = pBindState->GetVertexIndexData( CDmeVertexData::FIELD_MORPH_SPEED );

	int nPositionCount = positionIndices.Count();
	int nNormalCount = normalIndices.Count();
	int nTexcoordCount = texcoordIndices.Count();
	int nBalanceCount = balanceIndices.Count();
	int nSpeedCount = speedIndices.Count();
	if ( nNormalCount && nPositionCount != nNormalCount )
	{
		MdlError( "Encountered a mesh with invalid geometry (different number of indices for various data fields)\n" );
		return false;
	}
	if ( nTexcoordCount && nPositionCount != nTexcoordCount )
	{
		MdlError( "Encountered a mesh with invalid geometry (different number of indices for various data fields)\n" );
		return false;
	}
	if ( nBalanceCount && nPositionCount != nBalanceCount )
	{
		MdlError( "Encountered a mesh with invalid geometry (different number of indices for various data fields)\n" );
		return false;
	}
	if ( nSpeedCount && nPositionCount != nSpeedCount )
	{
		MdlError( "Encountered a mesh with invalid geometry (different number of indices for various data fields)\n" );
		return false;
	}

	// Only add unique vertices to the list as in UnifyIndices

	for ( int i = 0; i < nPositionCount; ++i )
	{
		VertIndices_t vert;
		vert.v = g_numverts + positionIndices[i]; 
		vert.n = ( nNormalCount > 0 ) ? g_numnormals + normalIndices[i] : -1; 
		vert.t = ( nTexcoordCount > 0 ) ? g_numtexcoords + texcoordIndices[i] : -1; 
		vert.balance = s_Balance.Count() + ( ( nBalanceCount > 0 ) ? balanceIndices[i] : 0 ); 
		vert.speed = s_Speed.Count() + ( ( nSpeedCount > 0 ) ? speedIndices[i] : 0 );

		bool unique( true );
		for ( int j = nStartingUniqueCount; j < s_UniqueVertices.Count(); ++j )
		{
			const VertIndices_t &tmpVert( s_UniqueVertices[j] );

			if ( vert.v != tmpVert.v )
				continue;
			if ( vert.n != tmpVert.n )
				continue;
			if ( vert.t != tmpVert.t )
				continue;

			unique = false;
			s_UniqueVerticesMap.AddToTail( j );
			break;
		}

		if ( !unique )
			continue;

		int k = s_UniqueVertices.AddToTail();
		s_UniqueVertices[k] = vert;
		s_UniqueVerticesMap.AddToTail( k );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Loads the vertices from the model
//-----------------------------------------------------------------------------
static bool LoadVertices( CDmeVertexData *pBindState, const matrix3x4_t& mat, float flScale, int nBoneAssign, int *pBoneRemap, int nStartingUniqueCount )
{
	if ( nBoneAssign < 0 )
	{
		nBoneAssign = s_nDefaultRootNode;
	}

	// Used by the morphing system to set up delta states
	DefineUniqueVertices( pBindState, nStartingUniqueCount );

	matrix3x4_t normalMat;
	MatrixInverseTranspose( mat, normalMat );

	const CUtlVector<Vector> &positions = pBindState->GetPositionData( );
	const CUtlVector<Vector> &normals = pBindState->GetNormalData( );
	const CUtlVector<Vector2D> &texcoords = pBindState->GetTextureCoordData( );
	const CUtlVector<float> &balances = pBindState->GetBalanceData( );
	const CUtlVector<float> &speeds = pBindState->GetMorphSpeedData( );

	int nCount = positions.Count();
	int nJointCount = pBindState->HasSkinningData() ? pBindState->JointCount() : 0;
	if ( nJointCount > MAXSTUDIOBONEWEIGHTS )
	{
		MdlError( "Too many bone influences per vertex!\n" );
		return false;
	}

	// Copy positions + bone info
	for ( int i = 0; i < nCount; ++i )
	{
		// NOTE: The transform transforms the positions into the bind space
		VectorTransform( positions[i], mat, g_vertex[g_numverts] );
		g_vertex[g_numverts] *= flScale;
		if ( nJointCount == 0 )
		{
			g_bone[g_numverts].numbones = 1;
			g_bone[g_numverts].bone[0] = pBoneRemap[ nBoneAssign ];
			g_bone[g_numverts].weight[0] = 1.0;
		}
		else
		{
			const float *pJointWeights = pBindState->GetJointWeightData( i );
			const int *pJointIndices = pBindState->GetJointIndexData( i );

			float *pWeightBuf = (float*)_alloca( nJointCount * sizeof(float) );
			int *pIndexBuf = (int*)_alloca( nJointCount * sizeof(int) );
			memcpy( pWeightBuf, pJointWeights, nJointCount * sizeof(float) );
			memcpy( pIndexBuf, pJointIndices, nJointCount * sizeof(int) );

			int nBoneCount = SortAndBalanceBones( nJointCount, MAXSTUDIOBONEWEIGHTS, pIndexBuf, pWeightBuf );

			g_bone[g_numverts].numbones = nBoneCount;
			for ( int j = 0; j < nBoneCount; ++j )
			{
				g_bone[g_numverts].bone[j] = pBoneRemap[ pIndexBuf[j] ];
				g_bone[g_numverts].weight[j] = pWeightBuf[j];
			}
		}
		++g_numverts;
	}

	// Copy normals
	nCount = normals.Count();
	if ( nCount + g_numnormals > MAXSTUDIOVERTS )
	{
		MdlError( "Too many normals in model!\n" );
		return false;
	}
	for ( int i = 0; i < nCount; ++i )
	{
		VectorRotate( normals[i], normalMat, g_normal[g_numnormals] );
		VectorNormalize( g_normal[g_numnormals] );
		++g_numnormals;
	}

	// Copy texcoords
	nCount = texcoords.Count();
	if ( nCount + g_numtexcoords > MAXSTUDIOVERTS )
	{
		MdlError( "Too many texture coordinates in model!\n" );
		return false;
	}
	bool bFlipVCoordinate = pBindState->IsVCoordinateFlipped();
	for ( int i = 0; i < nCount; ++i )
	{
		g_texcoord[g_numtexcoords].x = texcoords[i].x;
		g_texcoord[g_numtexcoords].y = bFlipVCoordinate ? 1.0f - texcoords[i].y : texcoords[i].y;
		++g_numtexcoords;
	}

	// In the event of no speed or balance map, use the same value of 1 for all vertices
	if ( balances.Count() )
	{
		s_Balance.AddMultipleToTail( balances.Count(), balances.Base() );
	}
	else
	{
		s_Balance.AddToTail( 1.0f );
	}

	if ( speeds.Count() )
	{
		s_Speed.AddMultipleToTail( speeds.Count(), speeds.Base() );
	}
	else
	{
		s_Speed.AddToTail( 1.0f );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Hook delta into delta list
//-----------------------------------------------------------------------------
static void AddToDeltaList( DeltaState_t *pDeltaStateData, int nUniqueVertex )
{
	DeltaIndex_t &index = pDeltaStateData->m_DeltaIndices[ nUniqueVertex ];
	if ( !index.m_bInList )
	{
		index.m_nNextDelta = pDeltaStateData->m_nFirstDelta;
		pDeltaStateData->m_nFirstDelta = nUniqueVertex;
		pDeltaStateData->m_nDeltaCount++;
		index.m_bInList = true;
	}
}


//-----------------------------------------------------------------------------
// Loads the vertices from the delta state
//-----------------------------------------------------------------------------

static bool LoadDeltaState(
	CDmeVertexDeltaData *pDeltaState,
	CDmeVertexData *pBindState,
	const matrix3x4_t& mat,
	float flScale,
	int nStartingUniqueVertex,
	int nStartingUniqueVertexMap )
{
	DeltaState_t *pDeltaStateData = FindOrAddDeltaState( pDeltaState->GetName(), nStartingUniqueVertex + pBindState->VertexCount() );

	matrix3x4_t normalMat;
	MatrixInverseTranspose( mat, normalMat );

	const CUtlVector<Vector> &positions = pDeltaState->GetPositionData( );
	const CUtlVector<int> &positionIndices = pDeltaState->GetVertexIndexData( CDmeVertexDataBase::FIELD_POSITION );
	const CUtlVector<Vector> &normals = pDeltaState->GetNormalData( );
	const CUtlVector<int> &normalIndices = pDeltaState->GetVertexIndexData( CDmeVertexDataBase::FIELD_NORMAL );
	const CUtlVector<float> &wrinkle = pDeltaState->GetWrinkleData( );
	const CUtlVector<int> &wrinkleIndices = pDeltaState->GetVertexIndexData( CDmeVertexDataBase::FIELD_WRINKLE );

	if ( positions.Count() != positionIndices.Count() )
	{
		MdlError( "DeltaState %s contains a different number of positions + position indices!\n", pDeltaState->GetName() );
		return false;
	}

	if ( normals.Count() != normalIndices.Count() )
	{
		MdlError( "DeltaState %s contains a different number of normals + normal indices!\n", pDeltaState->GetName() );
		return false;
	}

	if ( wrinkle.Count() != wrinkleIndices.Count() )
	{
		MdlError( "DeltaState %s contains a different number of wrinkles + wrinkle indices!\n", pDeltaState->GetName() );
		return false;
	}

	// Copy position delta
	int nCount = positions.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		Vector vecDelta;

		// NOTE NOTE!!: This is VectorRotate, *not* VectorTransform. This is because
		// we're transforming a delta, which is basically a direction vector. To
		// move it into the new space, we must rotate it
		VectorRotate( positions[i], mat, vecDelta );
		vecDelta *= flScale;

		int nPositionIndex = pDeltaStateData->m_PositionDeltas.AddToTail( vecDelta );

		// Indices 
		const CUtlVector< int > &baseVerts = pBindState->FindVertexIndicesFromDataIndex( CDmeVertexData::FIELD_POSITION, positionIndices[i] );
		int nBaseVertCount = baseVerts.Count();
		for ( int k = 0; k < nBaseVertCount; ++k )
		{
			int nUniqueVertexIndex = s_UniqueVerticesMap[ nStartingUniqueVertexMap + baseVerts[k] ];
			AddToDeltaList( pDeltaStateData, nUniqueVertexIndex );
			DeltaIndex_t &index = pDeltaStateData->m_DeltaIndices[ nUniqueVertexIndex ];
			index.m_nPositionIndex = nPositionIndex;
		}
	}

	// Copy normals
	nCount = normals.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		Vector vecDelta;
		VectorRotate( normals[i], normalMat, vecDelta );
		int nNormalIndex = pDeltaStateData->m_NormalDeltas.AddToTail( vecDelta );

		// Indices 
		const CUtlVector< int > &baseVerts = pBindState->FindVertexIndicesFromDataIndex( CDmeVertexData::FIELD_NORMAL, normalIndices[i] );
		int nBaseVertCount = baseVerts.Count();
		for ( int k = 0; k < nBaseVertCount; ++k )
		{
			int nUniqueVertexIndex = s_UniqueVerticesMap[ nStartingUniqueVertexMap + baseVerts[k] ];
			AddToDeltaList( pDeltaStateData, nUniqueVertexIndex );
			DeltaIndex_t &index = pDeltaStateData->m_DeltaIndices[ nUniqueVertexIndex ];
			index.m_nNormalIndex = nNormalIndex;
		}
	}

	// Copy wrinkle
	nCount = wrinkle.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		int nWrinkleIndex = pDeltaStateData->m_WrinkleDeltas.AddToTail( wrinkle[i] );

		// Indices 
		const CUtlVector< int > &baseVerts = pBindState->FindVertexIndicesFromDataIndex( CDmeVertexData::FIELD_WRINKLE, wrinkleIndices[i] );
		int nBaseVertCount = baseVerts.Count();
		for ( int k = 0; k < nBaseVertCount; ++k )
		{
			int nUniqueVertexIndex = s_UniqueVerticesMap[ nStartingUniqueVertexMap + baseVerts[k] ];
			AddToDeltaList( pDeltaStateData, nUniqueVertexIndex );
			DeltaIndex_t &index = pDeltaStateData->m_DeltaIndices[ nUniqueVertexIndex ];
			index.m_nWrinkleIndex = nWrinkleIndex;
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Reads the face data from the DMX data
//-----------------------------------------------------------------------------
static void ParseFaceData( CDmeVertexData *pVertexData, int material, int v1, int v2, int v3, int vi, int ni, int ti )
{
	s_tmpface_t f;
	f.material = material;

	int p, n, t;
	p = pVertexData->GetPositionIndex(v1); n = pVertexData->GetNormalIndex(v1); t = pVertexData->GetTexCoordIndex(v1);
	f.a = ( p >= 0 ) ? vi + p : 0; f.na = ( n >= 0 ) ? ni + n : 0; f.ta = ( t >= 0 ) ? ti + t : 0;
	p = pVertexData->GetPositionIndex(v2); n = pVertexData->GetNormalIndex(v2); t = pVertexData->GetTexCoordIndex(v2);
	f.b = ( p >= 0 ) ? vi + p : 0; f.nb = ( n >= 0 ) ? ni + n : 0; f.tb = ( t >= 0 ) ? ti + t : 0;
	p = pVertexData->GetPositionIndex(v3); n = pVertexData->GetNormalIndex(v3); t = pVertexData->GetTexCoordIndex(v3);
	f.c = ( p >= 0 ) ? vi + p : 0; f.nc = ( n >= 0 ) ? ni + n : 0; f.tc = ( t >= 0 ) ? ti + t : 0;

	Assert( f.a <= (unsigned long)g_numverts && f.b <= (unsigned long)g_numverts && f.c <= (unsigned long)g_numverts );
	Assert( f.na <= (unsigned long)g_numnormals && f.nb <= (unsigned long)g_numnormals && f.nc <= (unsigned long)g_numnormals );
	Assert( f.ta <= (unsigned long)g_numtexcoords && f.tb <= (unsigned long)g_numtexcoords && f.tc <= (unsigned long)g_numtexcoords );

	Assert( g_numfaces < MAXSTUDIOTRIANGLES-1 );
	if ( g_numfaces >= MAXSTUDIOTRIANGLES-1 )
		return;

	int i = g_numfaces++;
	g_face[i] = f;
}


//-----------------------------------------------------------------------------
// Reads the mesh data from the DMX data
//-----------------------------------------------------------------------------
static bool LoadMesh( CDmeMesh *pMesh, CDmeVertexData *pBindState, const matrix3x4_t& mat, float flScale, 
	int nBoneAssign, int *pBoneRemap, s_source_t *pSource )
{
	pMesh->CollapseRedundantNormals( normal_blend );

	// Load the vertices
	int nStartingVertex = g_numverts;
	int nStartingNormal = g_numnormals;
	int nStartingTexCoord = g_numtexcoords;
	int nStartingUniqueCount = s_UniqueVertices.Count();
	int nStartingUniqueMapCount = s_UniqueVerticesMap.Count();

	// This defines s_UniqueVertices & s_UniqueVerticesMap
	LoadVertices( pBindState, mat, flScale, nBoneAssign, pBoneRemap, nStartingUniqueCount );

	// Load the deltas
	int nDeltaStateCount = pMesh->DeltaStateCount();
	for ( int i = 0; i < nDeltaStateCount; ++i )
	{
		CDmeVertexDeltaData *pDeltaState = pMesh->GetDeltaState( i );
		if ( !LoadDeltaState( pDeltaState, pBindState, mat, flScale, nStartingUniqueCount, nStartingUniqueMapCount ) )
			return false;
	}

	// load the base triangles
	int texture;
	int material;
	char pTextureName[MAX_PATH];

	int nFaceSetCount = pMesh->FaceSetCount();
	for ( int i = 0; i < nFaceSetCount; ++i )
	{
		CDmeFaceSet *pFaceSet = pMesh->GetFaceSet( i );
		CDmeMaterial *pMaterial = pFaceSet->GetMaterial();

		// Get the material name
		Q_strncpy( pTextureName, pMaterial->GetMaterialName(), sizeof(pTextureName) );

		// funky texture overrides (specified with the -t command-line argument) 
		for ( int j = 0; j < numrep; j++ )  
		{
			if ( sourcetexture[j][0] == '\0' ) 
			{
				Q_strncpy( pTextureName, defaulttexture[j], sizeof(pTextureName) );
				break;
			}
			if ( Q_stricmp( pTextureName, sourcetexture[j]) == 0 ) 
			{
				Q_strncpy( pTextureName, defaulttexture[j], sizeof(pTextureName) );
				break;
			}
		}

		// skip all faces with the null texture on them.
		char pPathNoExt[MAX_PATH];
		Q_StripExtension( pTextureName, pPathNoExt, sizeof(pPathNoExt) );
		if ( !Q_stricmp( pPathNoExt, "null" ) )
			continue;

		texture = LookupTexture( pTextureName, true );
		pSource->texmap[texture] = texture;	// hack, make it 1:1
		material = UseTextureAsMaterial( texture );

		// prepare indices
		int nFirstIndex = 0;
		int nIndexCount = pFaceSet->NumIndices();
		while ( nFirstIndex < nIndexCount )
		{
			int nVertexCount = pFaceSet->GetNextPolygonVertexCount( nFirstIndex );
			if ( nVertexCount >= 3 )
			{
				int nOutCount = (nVertexCount-2) * 3;
				int *pIndices = (int*)_alloca( nOutCount * sizeof(int)	);
				pMesh->ComputeTriangulatedIndices( pBindState, pFaceSet, nFirstIndex, pIndices, nOutCount );
				for ( int ii = 0; ii < nOutCount; ii +=3 )
				{
					ParseFaceData( pBindState, material, pIndices[ii], pIndices[ii+2], pIndices[ii+1], nStartingVertex, nStartingNormal, nStartingTexCoord );
				}
			}
			nFirstIndex += nVertexCount + 1;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Method used to add mesh data
//-----------------------------------------------------------------------------
struct LoadMeshInfo_t
{
	s_source_t *m_pSource;
	CDmeModel *m_pModel;
	float m_flScale;
	int *m_pBoneRemap;
	matrix3x4_t m_pBindPose[MAXSTUDIOSRCBONES];
};

static bool LoadMeshes( const LoadMeshInfo_t &info, CDmeDag *pDag, const matrix3x4_t &parentToBindPose, int nBoneAssign )
{
	// We want to create an aggregate matrix transforming from this dag to its closest
	// parent which actually is an animated joint. This is done so we can autoskin
	// meshes to their closest parents if they have not been skinned.
	matrix3x4_t dagToBindPose;
	CDmeTransform *pDagTransform = pDag->GetTransform();
	int nFoundIndex = info.m_pModel->GetJointTransformIndex( pDagTransform );

	// Update autoskin to autoskin to non-DmeJoint's if they are children of the DmeModel (i.e. they have no parent bone)
	if ( nFoundIndex >= 0 )
	{
		if ( pDag == info.m_pModel || CastElement< CDmeJoint >( pDag ) )
		{
			nBoneAssign = nFoundIndex;
		}
		else
		{
			for ( int i = 0; i < info.m_pModel->GetChildCount(); ++i )
			{
				if ( info.m_pModel->GetChild( i ) == pDag )
				{
					nBoneAssign = nFoundIndex;
					break;
				}
			}
		}
	}

	if ( nFoundIndex >= 0 )
	{
		ConcatTransforms( parentToBindPose, info.m_pBindPose[nFoundIndex], dagToBindPose );
	}
	else
	{
		// NOTE: This isn't particularly kosher; we're using the current pose instead of the bind pose
		// because there's no transform in the bind pose
		matrix3x4_t dagToParent;
		pDagTransform->GetTransform( dagToParent );
		ConcatTransforms( parentToBindPose, dagToParent, dagToBindPose );
	}

	CDmeMesh *pMesh = CastElement< CDmeMesh >( pDag->GetShape() );
	if ( pMesh )
	{
		CDmeVertexData *pBindState = pMesh->FindBaseState( "bind" );
		if ( !pBindState )
			return false;

		if ( !LoadMesh( pMesh, pBindState, dagToBindPose, info.m_flScale, nBoneAssign, info.m_pBoneRemap, info.m_pSource ) )
			return false;
	}

	int nCount = pDag->GetChildCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeDag *pChild = pDag->GetChild( i );
		if ( !LoadMeshes( info, pChild, dagToBindPose, nBoneAssign ) )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Method used to add mesh data
//-----------------------------------------------------------------------------
static bool LoadMeshes( CDmeModel *pModel, float flScale, int *pBoneRemap, s_source_t *pSource )
{
	matrix3x4_t mat;
	SetIdentityMatrix( mat );

	LoadMeshInfo_t info;
	info.m_pModel = pModel;
	info.m_flScale = flScale;
	info.m_pBoneRemap = pBoneRemap;
	info.m_pSource = pSource;

	CDmeTransformList *pBindPose = pModel->FindBaseState( "bind" );
	int nCount = pBindPose ? pBindPose->GetTransformCount() : pModel->GetJointTransformCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeTransform *pTransform = pBindPose ? pBindPose->GetTransform(i) : pModel->GetJointTransform(i);

		matrix3x4_t jointTransform;
		pTransform->GetTransform( info.m_pBindPose[i] );
	}

	int nChildCount = pModel->GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmeDag *pChild = pModel->GetChild( i );
		if ( !LoadMeshes( info, pChild, mat, -1 ) )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Builds s_vertanim_ts 
//-----------------------------------------------------------------------------
static void BuildVertexAnimations( s_source_t *pSource )
{
	int nCount = s_DeltaStates.Count();
	if ( nCount == 0 )
		return;

	Assert( s_Speed.Count() > 0 );
	Assert( s_Balance.Count() > 0 );

	Assert( s_UniqueVertices.Count() == numvlist );
	s_vertanim_t *pVertAnim = (s_vertanim_t *)_alloca( numvlist * sizeof( s_vertanim_t ) );
	for ( int i = 0; i < nCount; ++i )
	{
		DeltaState_t &state = s_DeltaStates[i];

		s_sourceanim_t *pSourceAnim = FindOrAddSourceAnim( pSource, state.m_Name );
		pSourceAnim->numframes = 1;
		pSourceAnim->startframe = 0;
		pSourceAnim->endframe = 0;
		pSourceAnim->newStyleVertexAnimations = true;

		// Traverse the linked list of unique vertex indices j that has a delta
		int nVertAnimCount = 0;
		for ( int j = state.m_nFirstDelta; j >= 0; j = state.m_DeltaIndices[j].m_nNextDelta )
		{
			// The Delta Indices array is a parallel array to s_UniqueVertices
			// j is used to index into both
			DeltaIndex_t &delta = state.m_DeltaIndices[j];
			Assert( delta.m_nPositionIndex >= 0 || delta.m_nNormalIndex >= 0 || delta.m_nWrinkleIndex >= 0 );

			VertIndices_t &uniqueVert = s_UniqueVertices[j];

			const v_unify_t *pList = v_list[uniqueVert.v];
			for( ; pList; pList = pList->next )
			{
				if ( pList->n != uniqueVert.n || pList->t != uniqueVert.t )
					continue;
				
				s_vertanim_t& vertanim = pVertAnim[nVertAnimCount++];
				vertanim.vertex = pList - v_listdata;
				vertanim.speed = s_Speed[ s_UniqueVertices[j].speed ];
				vertanim.side = s_Balance[ s_UniqueVertices[j].balance ];
				if ( delta.m_nPositionIndex >= 0 )
				{
					vertanim.pos = state.m_PositionDeltas[ delta.m_nPositionIndex ];
				}
				else
				{
					vertanim.pos = vec3_origin;
				}
				if ( delta.m_nNormalIndex >= 0 )
				{
					vertanim.normal = state.m_NormalDeltas[ delta.m_nNormalIndex ];
				}
				else
				{
					vertanim.normal = vec3_origin;
				}

				if ( delta.m_nWrinkleIndex >= 0 )
				{
					vertanim.wrinkle = state.m_WrinkleDeltas[ delta.m_nWrinkleIndex ];
				}
				else
				{
					vertanim.wrinkle = 0.0f;
				}
			}
		}
		pSourceAnim->numvanims[0] = nVertAnimCount;
		pSourceAnim->vanim[0] = (s_vertanim_t *)kalloc( nVertAnimCount, sizeof( s_vertanim_t ) );
		memcpy( pSourceAnim->vanim[0], pVertAnim, nVertAnimCount * sizeof( s_vertanim_t ) );
	}
}

	
//-----------------------------------------------------------------------------
// Loads the skeletal hierarchy from the game model, returns bone count
//-----------------------------------------------------------------------------
static bool AddDagJoint( CDmeModel *pModel, CDmeDag *pDag, s_node_t *pNodes, int nParentIndex, BoneTransformMap_t &boneMap )
{
	CDmeTransform *pDmeTransform = pDag->GetTransform();
	const char *pTransformName = pDmeTransform->GetName();
	int nJointIndex = boneMap.m_nBoneCount++;
	if ( nJointIndex >= MAXSTUDIOSRCBONES )
	{
		MdlWarning( "DMX Model has too many bones [%d, max can be %d]!\n", boneMap.m_nBoneCount, MAXSTUDIOSRCBONES );
		return false;
	}
	
	boneMap.m_ppTransforms[ nJointIndex ] = pDmeTransform;
	int nFoundIndex = 0;
	if ( pModel )
	{
		nFoundIndex = pModel->GetJointTransformIndex( pDmeTransform );
		if ( nFoundIndex >= 0 )
		{
			boneMap.m_pBoneRemap[nFoundIndex] = nJointIndex;
		}
	}

	Q_strncpy( pNodes[ nJointIndex ].name, pTransformName, sizeof( pNodes[ nJointIndex ].name ) );
	pNodes[ nJointIndex ].parent = nParentIndex;

	// Now deal with children
	int nChildCount = pDag->GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmeDag *pChild = pDag->GetChild( i );
		if ( !pChild )
			continue;

		int nCurrentBoneCount = boneMap.m_nBoneCount;
		if ( !AddDagJoint( pModel, pChild, pNodes, nJointIndex, boneMap ) )
			return false;

		if ( ( nCurrentBoneCount != boneMap.m_nBoneCount ) && ( nFoundIndex < 0 ) )
		{
			MdlWarning( "DMX Model has a joint \"%s\" which is not in its joint transform list.\n"
				"This joint has children which are in the joint transform list, which is illegal.\n",
				pDag->GetName() );
			return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Main entry point for loading the skeleton
//-----------------------------------------------------------------------------
static int LoadSkeleton( CDmeDag *pRoot, CDmeModel *pModel, s_node_t *pNodes, BoneTransformMap_t &map )
{
	// Initialize bone indices
	map.m_nBoneCount = 0;
	for ( int i = 0; i < MAXSTUDIOSRCBONES; ++i )
	{
		pNodes[i].name[0] = 0;
		pNodes[i].parent = -1;
		map.m_pBoneRemap[i] = -1;
		map.m_ppTransforms[i] = NULL;
	}

	// Don't create joints for the the root dag ever.. just deal with the children
	int nChildCount = pRoot->GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmeDag *pChild = pRoot->GetChild( i );
		if ( !pChild )
			continue;

		if ( !AddDagJoint( pModel, pChild, pNodes, -1, map ) )
			return 0;
	}

	// Add a default identity bone used for autoskinning if no joints are specified
	s_nDefaultRootNode = map.m_nBoneCount;
	Q_strncpy( pNodes[s_nDefaultRootNode].name, "defaultRoot", sizeof( pNodes[ s_nDefaultRootNode ].name ) );
	pNodes[s_nDefaultRootNode].parent = -1;

	if ( !pModel )
		return map.m_nBoneCount + 1;

	// Look for joints listed in the transform list which aren't in the hierarchy
	int nInitialBoneCount = pModel->GetJointTransformCount();
	for ( int i = 0; i < nInitialBoneCount; ++i )
	{
		int nIndex = map.m_pBoneRemap[i];
		if ( nIndex < 0 )
		{
			map.m_pBoneRemap[i] = map.m_nBoneCount++;
			nIndex = map.m_pBoneRemap[i];
		}
		if ( pNodes[nIndex].name[0] == 0 )
		{
			CDmeTransform *pTransform = pModel->GetJointTransform( i );
			Q_strncpy( pNodes[ nIndex ].name, pTransform->GetName(), sizeof( pNodes[ nIndex ].name ) );
			MdlWarning( "Joint %s specified in the joint transform list but doesn't appear in the dag hierarchy!\n", pTransform->GetName() );
		}
	}
	return map.m_nBoneCount + 1;
}


//-----------------------------------------------------------------------------
// Loads the skeletal hierarchy from the game model, returns bone count
//-----------------------------------------------------------------------------
static void LoadAttachments( CDmeDag *pRoot, CDmeDag *pDag, s_source_t *pSource )
{
	CDmeAttachment *pAttachment = CastElement< CDmeAttachment >( pDag->GetShape() );
	if ( pAttachment && ( pDag != pRoot ) )
	{
		int i = pSource->m_Attachments.AddToTail();
		s_attachment_t &attachment = pSource->m_Attachments[i];
		memset( &attachment, 0, sizeof(s_attachment_t) );
		Q_strncpy( attachment.name, pAttachment->GetName(), sizeof( attachment.name ) );
		Q_strncpy( attachment.bonename, pDag->GetName(), sizeof( attachment.bonename ) );
		SetIdentityMatrix( attachment.local );
		if ( pAttachment->m_bIsRigid )
		{
			attachment.type |= IS_RIGID;
		}
		if ( pAttachment->m_bIsWorldAligned )
		{
			attachment.flags |= ATTACHMENT_FLAG_WORLD_ALIGN;
		}
	}

	// Don't create joints for the the root dag ever.. just deal with the children
	int nChildCount = pDag->GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmeDag *pChild = pDag->GetChild( i );
		if ( !pChild )
			continue;

		LoadAttachments( pRoot, pChild, pSource );
	}
}


//-----------------------------------------------------------------------------
// Loads the bind pose
//-----------------------------------------------------------------------------
static void LoadBindPose( CDmeModel *pModel, float flScale, int *pBoneRemap, s_source_t *pSource )
{
	s_sourceanim_t *pSourceAnim = FindOrAddSourceAnim( pSource, "BindPose" );
	pSourceAnim->startframe = 0;
	pSourceAnim->endframe = 0;
	pSourceAnim->numframes = 1;

	// Default all transforms to identity
	pSourceAnim->rawanim[0] = (s_bone_t *)kalloc( pSource->numbones, sizeof(s_bone_t) );
	for ( int i = 0; i < pSource->numbones; ++i )
	{
		pSourceAnim->rawanim[0][i].pos.Init();
		pSourceAnim->rawanim[0][i].rot.Init();
	}							 
	 
	// Override those bones in the bind pose with the real values
	// NOTE: This means that bones that are not in the bind pose are set to identity!
	// Is this correct? I think it shouldn't matter, but we may need to fix this.
	CDmeTransformList *pBindPose = pModel->FindBaseState( "bind" );
	int nCount = pBindPose ? pBindPose->GetTransformCount() : pModel->GetJointTransformCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeTransform *pTransform = pBindPose ? pBindPose->GetTransform(i) : pModel->GetJointTransform(i);

		matrix3x4_t jointTransform;
		pTransform->GetTransform( jointTransform );

		int nActualBoneIndex = pBoneRemap[i]; 
		s_bone_t &bone = pSourceAnim->rawanim[0][nActualBoneIndex];
		MatrixAngles( jointTransform, bone.rot, bone.pos );
		bone.pos *= flScale;
	}

	Build_Reference( pSource, "BindPose" );
}


//-----------------------------------------------------------------------------
// Main entry point for loading DMX files
//-----------------------------------------------------------------------------
static void PrepareChannels( CDmeChannelsClip *pAnimation ) 
{
	int nChannelsCount = pAnimation->m_Channels.Count();
	for ( int i = 0; i < nChannelsCount; ++i )
	{
		pAnimation->m_Channels[i]->SetMode( CM_PLAY );
	}
}


//-----------------------------------------------------------------------------
// Update channels so they are in position for the next frame
//-----------------------------------------------------------------------------
static void UpdateChannels( CDmeChannelsClip *pAnimation, DmeTime_t clipTime ) 
{
	int nChannelsCount = pAnimation->m_Channels.Count();
	DmeTime_t channelTime = pAnimation->ToChildMediaTime( clipTime );
	CUtlVector< IDmeOperator* > operators( 0, nChannelsCount );
	for ( int i = 0; i < nChannelsCount; ++i )
	{
		pAnimation->m_Channels[i]->SetCurrentTime( channelTime );
		operators.AddToTail( pAnimation->m_Channels[i] );
	}

	// Recompute the position of the joints
	{
		CDisableUndoScopeGuard guard;
		g_pDmElementFramework->SetOperators( operators );
		g_pDmElementFramework->Operate( true );
	}
	g_pDmElementFramework->BeginEdit();
}


//-----------------------------------------------------------------------------
// Initialize the pose for this frame
//-----------------------------------------------------------------------------
static void ComputeFramePose( s_sourceanim_t *pSourceAnim, int nFrame, float flScale, BoneTransformMap_t& boneMap ) 
{
	pSourceAnim->rawanim[nFrame] = (s_bone_t *)kalloc( boneMap.m_nBoneCount, sizeof( s_bone_t ) );

	for ( int i = 0; i < boneMap.m_nBoneCount; ++i )
	{
		matrix3x4_t jointTransform;
		boneMap.m_ppTransforms[i]->GetTransform( jointTransform );
		MatrixAngles( jointTransform, pSourceAnim->rawanim[nFrame][i].rot, pSourceAnim->rawanim[nFrame][i].pos );
		pSourceAnim->rawanim[nFrame][i].pos *= flScale;
	}
}


//-----------------------------------------------------------------------------
// Main entry point for loading animations
//-----------------------------------------------------------------------------
static void LoadAnimations( s_source_t *pSource, CDmeAnimationList *pAnimationList, float flScale, BoneTransformMap_t &boneMap )
{
	int nAnimationCount = pAnimationList->GetAnimationCount();
	for ( int i = 0; i < nAnimationCount; ++i )
	{
		CDmeChannelsClip *pAnimation = pAnimationList->GetAnimation( i );

		if ( !Q_stricmp( pAnimationList->GetName(), "BindPose" ) )
		{
			MdlError( "Error: Cannot use \"BindPose\" as an animation name!\n" );
			break;
		}

		s_sourceanim_t *pSourceAnim = FindOrAddSourceAnim( pSource, pAnimation->GetName() );

		DmeTime_t nStartTime = pAnimation->GetStartTime();
		DmeTime_t nEndTime = pAnimation->GetEndTime();
		int nFrameRateVal = pAnimation->GetValue<int>( "frameRate" );
		if ( nFrameRateVal <= 0 )
		{
			nFrameRateVal = 30;
		}
		DmeFramerate_t nFrameRate = nFrameRateVal;
		pSourceAnim->startframe = nStartTime.CurrentFrame( nFrameRate );
		pSourceAnim->endframe = nEndTime.CurrentFrame( nFrameRate );
		pSourceAnim->numframes = pSourceAnim->endframe - pSourceAnim->startframe + 1;

		// Prepare channels for playback
		PrepareChannels( pAnimation );

		float flOOFrameRate = 1.0f / (float)nFrameRateVal;
		DmeTime_t nTime = nStartTime;
		int nFrame = 0;
		while ( nFrame < pSourceAnim->numframes )
		{
			int nSecond = nFrame / nFrameRateVal;
			int nFraction = nFrame - nSecond * nFrameRateVal;
			DmeTime_t t = nStartTime + DmeTime_t( nSecond * 10000 ) + DmeTime_t( (float)nFraction * flOOFrameRate );

			// Update the current time
			UpdateChannels( pAnimation, t );

			// Initialize the pose for this frame
			ComputeFramePose( pSourceAnim, nFrame, flScale, boneMap );

			++nFrame;
		}
	}
}


//-----------------------------------------------------------------------------
// Loads the skeletal hierarchy from the game model, returns bone count
//-----------------------------------------------------------------------------
static void AddFlexKeys( CDmeDag *pRoot, CDmeDag *pDag, CDmeCombinationOperator *pComboOp, s_source_t *pSource )
{
	CDmeMesh *pMesh = CastElement< CDmeMesh >( pDag->GetShape() );
	if ( pMesh && ( pDag != pRoot ) )
	{
		int nDeltaStateCount = pMesh->DeltaStateCount();
		for ( int i = 0; i < nDeltaStateCount; ++i )
		{
			CDmeVertexDeltaData *pDeltaState = pMesh->GetDeltaState( i );
			AddFlexKey( pSource, pComboOp, pDeltaState->GetName() );
		}
	}

	// Don't create joints for the the root dag ever.. just deal with the children
	int nChildCount = pDag->GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmeDag *pChild = pDag->GetChild( i );
		if ( !pChild )
			continue;

		AddFlexKeys( pRoot, pChild, pComboOp, pSource );
	}
}

//-----------------------------------------------------------------------------
// Loads all auxilliary model info:
//
//	*	Determine original source files used to generate
//		the current DMX file and schedule them for processing.
//-----------------------------------------------------------------------------
void LoadModelInfo( CDmElement *pRoot, char const *pFullPath )
{
	// Determine original source files and schedule them for processing
	if ( CDmElement *pMakeFile = pRoot->GetValueElement< CDmElement >( "makefile" ) )
	{
		if ( CDmAttribute *pSources = pMakeFile->GetAttribute( "sources" ) )
		{
			CDmrElementArray< CDmElement > arrSources( pSources );
			for ( int kk = 0; kk < arrSources.Count(); ++ kk )
			{
				if ( CDmElement *pModelSource = arrSources.Element( kk ) )
				{
					if ( char const *szName = pModelSource->GetName() )
					{
						ProcessOriginalContentFile( pFullPath, szName );
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Main entry point for loading DMX files
//-----------------------------------------------------------------------------
int Load_DMX( s_source_t *pSource )
{
	DmFileId_t fileId;
	s_DeltaStates.RemoveAll();
	s_Balance.RemoveAll();
	s_Speed.RemoveAll();
	s_UniqueVertices.RemoveAll();
	s_UniqueVerticesMap.RemoveAll();

	// use the full search tree, including mod hierarchy to find the file
	char pFullPath[MAX_PATH];
	if ( !GetGlobalFilePath( pSource->filename, pFullPath, sizeof(pFullPath) ) )
		return 0;

	// When reading, keep the CRLF; this will make ReadFile read it in binary format
	// and also append a couple 0s to the end of the buffer.
	CDmElement *pRoot;
	if ( g_pDataModel->RestoreFromFile( pFullPath, NULL, NULL, &pRoot ) == DMFILEID_INVALID )
		return 0;

	if ( !g_quiet )
	{
		Msg( "DMX Model %s\n", pFullPath );
	}

	// Load model info
	LoadModelInfo( pRoot, pFullPath );

	// Extract out the skeleton
	CDmeDag *pSkeleton = pRoot->GetValueElement< CDmeDag >( "skeleton" );
	CDmeModel *pModel = pRoot->GetValueElement< CDmeModel >( "model" );
	CDmeCombinationOperator *pCombinationOperator = pRoot->GetValueElement< CDmeCombinationOperator >( "combinationOperator" );
	if ( !pSkeleton )
		goto dmxError;

	// BoneRemap[bone index in file] == bone index in studiomdl
	BoneTransformMap_t boneMap;
	pSource->numbones = LoadSkeleton( pSkeleton, pModel, pSource->localBone, boneMap );
	if ( pSource->numbones == 0 )
		goto dmxError;

	LoadAttachments( pSkeleton, pSkeleton, pSource );

	g_numfaces = 0;
	if ( pModel )
	{
		if ( pCombinationOperator )
		{
			pCombinationOperator->GenerateWrinkleDeltas( false );
		}
		LoadBindPose( pModel, g_currentscale, boneMap.m_pBoneRemap, pSource );
		if ( !LoadMeshes( pModel, g_currentscale, boneMap.m_pBoneRemap, pSource ) )
			goto dmxError;

		UnifyIndices( pSource );
		BuildVertexAnimations( pSource );
		BuildIndividualMeshes( pSource );
	}

	if ( pCombinationOperator )
	{
		AddFlexKeys( pModel, pModel, pCombinationOperator, pSource );
		AddCombination( pSource, pCombinationOperator );
	}

	CDmeAnimationList *pAnimationList = pRoot->GetValueElement< CDmeAnimationList >( "animationList" );
	if ( pAnimationList )
	{
		LoadAnimations( pSource, pAnimationList, g_currentscale, boneMap );
	}

	fileId = pRoot->GetFileId();
	g_pDataModel->RemoveFileId( fileId );
	return 1;

dmxError:
	fileId = pRoot->GetFileId();
	g_pDataModel->RemoveFileId( fileId );
	return 0;
}






















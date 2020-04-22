//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

// Standard includes
#include <limits.h>

// Valve includes
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmevertexdata.h"
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmematerial.h"
#include "movieobjects/dmetransform.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects_interfaces.h"
#include "movieobjects/dmecombinationoperator.h"
#include "movieobjects/dmeselection.h"
#include "movieobjects/dmedrawsettings.h"
#include "movieobjects/dmmeshcomp.h"
#include "tier3/tier3.h"
#include "tier1/KeyValues.h"
#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imorph.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialvar.h"
#include "istudiorender.h"
#include "studio.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Normal rendering materials
//-----------------------------------------------------------------------------
bool CDmeMesh::s_bNormalMaterialInitialized;
CMaterialReference CDmeMesh::s_NormalMaterial;
CMaterialReference CDmeMesh::s_NormalErrorMaterial;



//-----------------------------------------------------------------------------
// Computes a skin matrix 
//-----------------------------------------------------------------------------
static const matrix3x4_t *ComputeSkinMatrix( int nBoneCount, const float *pJointWeight, const int *pJointIndices, const matrix3x4_t *pPoseToWorld, matrix3x4_t &result )
{
	float flWeight0, flWeight1, flWeight2, flWeight3;

	switch( nBoneCount )
	{
	default:
	case 1:
		return &pPoseToWorld[pJointIndices[0]];

	case 2:
		{
			const matrix3x4_t &boneMat0 = pPoseToWorld[pJointIndices[0]];
			const matrix3x4_t &boneMat1 = pPoseToWorld[pJointIndices[1]];
			flWeight0 = pJointWeight[0];
			flWeight1 = pJointWeight[1];

			// NOTE: Inlining here seems to make a fair amount of difference
			result[0][0] = boneMat0[0][0] * flWeight0 + boneMat1[0][0] * flWeight1;
			result[0][1] = boneMat0[0][1] * flWeight0 + boneMat1[0][1] * flWeight1;
			result[0][2] = boneMat0[0][2] * flWeight0 + boneMat1[0][2] * flWeight1;
			result[0][3] = boneMat0[0][3] * flWeight0 + boneMat1[0][3] * flWeight1;
			result[1][0] = boneMat0[1][0] * flWeight0 + boneMat1[1][0] * flWeight1;
			result[1][1] = boneMat0[1][1] * flWeight0 + boneMat1[1][1] * flWeight1;
			result[1][2] = boneMat0[1][2] * flWeight0 + boneMat1[1][2] * flWeight1;
			result[1][3] = boneMat0[1][3] * flWeight0 + boneMat1[1][3] * flWeight1;
			result[2][0] = boneMat0[2][0] * flWeight0 + boneMat1[2][0] * flWeight1;
			result[2][1] = boneMat0[2][1] * flWeight0 + boneMat1[2][1] * flWeight1;
			result[2][2] = boneMat0[2][2] * flWeight0 + boneMat1[2][2] * flWeight1;
			result[2][3] = boneMat0[2][3] * flWeight0 + boneMat1[2][3] * flWeight1;
		}
		return &result;

	case 3:
		{
			const matrix3x4_t &boneMat0 = pPoseToWorld[pJointIndices[0]];
			const matrix3x4_t &boneMat1 = pPoseToWorld[pJointIndices[1]];
			const matrix3x4_t &boneMat2 = pPoseToWorld[pJointIndices[2]];
			flWeight0 = pJointWeight[0];
			flWeight1 = pJointWeight[1];
			flWeight2 = pJointWeight[2];

			result[0][0] = boneMat0[0][0] * flWeight0 + boneMat1[0][0] * flWeight1 + boneMat2[0][0] * flWeight2;
			result[0][1] = boneMat0[0][1] * flWeight0 + boneMat1[0][1] * flWeight1 + boneMat2[0][1] * flWeight2;
			result[0][2] = boneMat0[0][2] * flWeight0 + boneMat1[0][2] * flWeight1 + boneMat2[0][2] * flWeight2;
			result[0][3] = boneMat0[0][3] * flWeight0 + boneMat1[0][3] * flWeight1 + boneMat2[0][3] * flWeight2;
			result[1][0] = boneMat0[1][0] * flWeight0 + boneMat1[1][0] * flWeight1 + boneMat2[1][0] * flWeight2;
			result[1][1] = boneMat0[1][1] * flWeight0 + boneMat1[1][1] * flWeight1 + boneMat2[1][1] * flWeight2;
			result[1][2] = boneMat0[1][2] * flWeight0 + boneMat1[1][2] * flWeight1 + boneMat2[1][2] * flWeight2;
			result[1][3] = boneMat0[1][3] * flWeight0 + boneMat1[1][3] * flWeight1 + boneMat2[1][3] * flWeight2;
			result[2][0] = boneMat0[2][0] * flWeight0 + boneMat1[2][0] * flWeight1 + boneMat2[2][0] * flWeight2;
			result[2][1] = boneMat0[2][1] * flWeight0 + boneMat1[2][1] * flWeight1 + boneMat2[2][1] * flWeight2;
			result[2][2] = boneMat0[2][2] * flWeight0 + boneMat1[2][2] * flWeight1 + boneMat2[2][2] * flWeight2;
			result[2][3] = boneMat0[2][3] * flWeight0 + boneMat1[2][3] * flWeight1 + boneMat2[2][3] * flWeight2;
		}
		return &result;

	case 4:
		{
			const matrix3x4_t &boneMat0 = pPoseToWorld[pJointIndices[0]];
			const matrix3x4_t &boneMat1 = pPoseToWorld[pJointIndices[1]];
			const matrix3x4_t &boneMat2 = pPoseToWorld[pJointIndices[2]];
			const matrix3x4_t &boneMat3 = pPoseToWorld[pJointIndices[3]];
			flWeight0 = pJointWeight[0];
			flWeight1 = pJointWeight[1];
			flWeight2 = pJointWeight[2];
			flWeight3 = pJointWeight[3];

			result[0][0] = boneMat0[0][0] * flWeight0 + boneMat1[0][0] * flWeight1 + boneMat2[0][0] * flWeight2 + boneMat3[0][0] * flWeight3;
			result[0][1] = boneMat0[0][1] * flWeight0 + boneMat1[0][1] * flWeight1 + boneMat2[0][1] * flWeight2 + boneMat3[0][1] * flWeight3;
			result[0][2] = boneMat0[0][2] * flWeight0 + boneMat1[0][2] * flWeight1 + boneMat2[0][2] * flWeight2 + boneMat3[0][2] * flWeight3;
			result[0][3] = boneMat0[0][3] * flWeight0 + boneMat1[0][3] * flWeight1 + boneMat2[0][3] * flWeight2 + boneMat3[0][3] * flWeight3;
			result[1][0] = boneMat0[1][0] * flWeight0 + boneMat1[1][0] * flWeight1 + boneMat2[1][0] * flWeight2 + boneMat3[1][0] * flWeight3;
			result[1][1] = boneMat0[1][1] * flWeight0 + boneMat1[1][1] * flWeight1 + boneMat2[1][1] * flWeight2 + boneMat3[1][1] * flWeight3;
			result[1][2] = boneMat0[1][2] * flWeight0 + boneMat1[1][2] * flWeight1 + boneMat2[1][2] * flWeight2 + boneMat3[1][2] * flWeight3;
			result[1][3] = boneMat0[1][3] * flWeight0 + boneMat1[1][3] * flWeight1 + boneMat2[1][3] * flWeight2 + boneMat3[1][3] * flWeight3;
			result[2][0] = boneMat0[2][0] * flWeight0 + boneMat1[2][0] * flWeight1 + boneMat2[2][0] * flWeight2 + boneMat3[2][0] * flWeight3;
			result[2][1] = boneMat0[2][1] * flWeight0 + boneMat1[2][1] * flWeight1 + boneMat2[2][1] * flWeight2 + boneMat3[2][1] * flWeight3;
			result[2][2] = boneMat0[2][2] * flWeight0 + boneMat1[2][2] * flWeight1 + boneMat2[2][2] * flWeight2 + boneMat3[2][2] * flWeight3;
			result[2][3] = boneMat0[2][3] * flWeight0 + boneMat1[2][3] * flWeight1 + boneMat2[2][3] * flWeight2 + boneMat3[2][3] * flWeight3;
		}
		return &result;
	}

	Assert(0);
	return NULL;
}


//-----------------------------------------------------------------------------
// Helper class to deal with software skinning 
//-----------------------------------------------------------------------------
class CRenderInfo
{
public:
	CRenderInfo( const CDmeVertexData *pBaseState );

	void ComputeVertex( int vi, const matrix3x4_t *pPoseToWorld, CDmeMesh::RenderVertexDelta_t *pDelta, Vector *pPosition, Vector *pNormal, Vector4D *pTangent );
	void ComputeVertex( int vi, const matrix3x4_t *pPoseToWorld, Vector *pDeltaPosition, int nStride, Vector *pPosition );
	void ComputePosition( int posIndex, const matrix3x4_t *pPoseToWorld, Vector *pDeltaPosition, Vector *pPosition );

	inline bool HasPositionData() const { return m_bHasPositionData; }
	inline bool HasNormalData() const { return m_bHasNormalData; }
	inline bool HasTangentData() const { return m_bHasTangentData; }
private:
	const CUtlVector<int>& m_PositionIndices;
	const CUtlVector<Vector>& m_PositionData;
	const CUtlVector<int>& m_NormalIndices;
	const CUtlVector<Vector>& m_NormalData;
	const CUtlVector<int>& m_TangentIndices;
	const CUtlVector<Vector4D>& m_TangentData;
	const CDmeVertexData *m_pBaseState;
	int m_nJointCount;
	bool m_bHasPositionData;
	bool m_bHasNormalData;
	bool m_bHasTangentData;
	bool m_bHasSkinningData;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CRenderInfo::CRenderInfo( const CDmeVertexData *pBaseState ) :
	m_PositionIndices( pBaseState->GetVertexIndexData( CDmeVertexData::FIELD_POSITION ) ),
	m_PositionData( pBaseState->GetPositionData() ),
	m_NormalIndices( pBaseState->GetVertexIndexData( CDmeVertexData::FIELD_NORMAL ) ),
	m_NormalData( pBaseState->GetNormalData() ),
	m_TangentIndices( pBaseState->GetVertexIndexData( CDmeVertexData::FIELD_TANGENT ) ),
	m_TangentData( pBaseState->GetTangentData() )
{
	m_pBaseState = pBaseState;
	m_bHasPositionData = m_PositionIndices.Count() > 0;
	m_bHasNormalData = m_NormalIndices.Count() > 0;
	m_bHasTangentData = m_TangentIndices.Count() > 0;
	m_nJointCount = pBaseState->JointCount();
	m_bHasSkinningData = pBaseState->HasSkinningData() && m_nJointCount > 0;
}
	

//-----------------------------------------------------------------------------
// Computes where a vertex is
//-----------------------------------------------------------------------------
void CRenderInfo::ComputeVertex( int vi, const matrix3x4_t *pPoseToWorld, Vector *pDeltaPosition, int nDeltaStride, Vector *pPosition )
{
	matrix3x4_t result;
	Vector vecMorphPosition, vecMorphNormal;
	const matrix3x4_t *pSkinMatrix = pPoseToWorld;
	if ( m_bHasSkinningData )
	{
		const float *pJointWeight = m_pBaseState->GetJointWeights( vi );
		const int *pJointIndices = m_pBaseState->GetJointIndices( vi );
		pSkinMatrix = ComputeSkinMatrix( m_nJointCount, pJointWeight, pJointIndices, pPoseToWorld, result );
	}

	int pi = m_PositionIndices[ vi ];
	const Vector *pPositionData = &m_PositionData[ pi ];
	if ( pDeltaPosition )
	{
		Vector *pDelta = (Vector*)( (unsigned char *)pDeltaPosition + nDeltaStride * pi );
		VectorAdd( *pPositionData, *pDelta, vecMorphPosition );
		pPositionData = &vecMorphPosition;
	}
	VectorTransform( *pPositionData, *pSkinMatrix, *pPosition );
}


//-----------------------------------------------------------------------------
// Computes where a vertex is
//-----------------------------------------------------------------------------
void CRenderInfo::ComputePosition( int posIndex, const matrix3x4_t *pPoseToWorld, Vector *pDeltaPosition, Vector *pPosition )
{
	matrix3x4_t result;
	Vector vecMorphPosition;
	const matrix3x4_t *pSkinMatrix = pPoseToWorld;

	if ( m_bHasSkinningData )
	{
		const float *pJointWeight = m_pBaseState->GetJointPositionWeights( posIndex );
		const int *pJointIndices = m_pBaseState->GetJointPositionIndices( posIndex );
		pSkinMatrix = ComputeSkinMatrix( m_nJointCount, pJointWeight, pJointIndices, pPoseToWorld, result );
	}

	const Vector *pPositionData = &m_PositionData[ posIndex ];

	if ( pDeltaPosition )
	{
		VectorAdd( *pPositionData, *( pDeltaPosition + posIndex ), vecMorphPosition );
		pPositionData = &vecMorphPosition;
	}

	VectorTransform( *pPositionData, *pSkinMatrix, *( pPosition + posIndex ) );
}


//-----------------------------------------------------------------------------
// Computes where a vertex is
//-----------------------------------------------------------------------------
void CRenderInfo::ComputeVertex( int vi, const matrix3x4_t *pPoseToWorld, CDmeMesh::RenderVertexDelta_t *pDelta, Vector *pPosition, Vector *pNormal, Vector4D *pTangent )
{
	matrix3x4_t result;
	Vector vecMorphPosition, vecMorphNormal;
	const matrix3x4_t *pSkinMatrix = pPoseToWorld;

	if ( m_bHasSkinningData )
	{
		const float *pJointWeight = m_pBaseState->GetJointWeights( vi );
		const int *pJointIndices = m_pBaseState->GetJointIndices( vi );
		pSkinMatrix = ComputeSkinMatrix( m_nJointCount, pJointWeight, pJointIndices, pPoseToWorld, result );
	}

	int pi = m_PositionIndices[ vi ];
	const Vector *pPositionData = &m_PositionData[ pi ];
	if ( pDelta )
	{
		VectorAdd( *pPositionData, pDelta[ pi ].m_vecDeltaPosition, vecMorphPosition );
		pPositionData = &vecMorphPosition;
	}
	VectorTransform( *pPositionData, *pSkinMatrix, *pPosition );

	if ( m_bHasNormalData )
	{
		int ni = m_NormalIndices[ vi ];
		const Vector *pNormalData = &m_NormalData[ ni ];
		if ( pDelta )
		{
			VectorAdd( *pNormalData, pDelta[ni].m_vecDeltaNormal, vecMorphNormal );
			pNormalData = &vecMorphNormal;
		}
		VectorRotate( *pNormalData, *pSkinMatrix, *pNormal );
		VectorNormalize( *pNormal );
	}
	else
	{
		pNormal->Init( 0.0f, 0.0f, 1.0f );
	}

	if ( m_bHasTangentData )
	{
		const Vector4D &tangentData = m_TangentData[ m_TangentIndices[ vi ] ];
		VectorRotate( tangentData.AsVector3D(), *pSkinMatrix, pTangent->AsVector3D() );
		VectorNormalize( pTangent->AsVector3D() );
		pTangent->w = tangentData.w;
	}
	else
	{
		pTangent->Init( 1.0f, 0.0f, 0.0f, 1.0f );
	}
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeMesh, CDmeMesh );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeMesh::OnConstruction()
{
	m_BindBaseState.Init( this, "bindState" );
	m_CurrentBaseState.Init( this, "currentState" );
	m_BaseStates.Init( this, "baseStates", FATTRIB_MUSTCOPY );
	m_DeltaStates.Init( this, "deltaStates", FATTRIB_MUSTCOPY | FATTRIB_HAS_CALLBACK );
	m_FaceSets.Init( this, "faceSets", FATTRIB_MUSTCOPY );
	m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL].Init( this, "deltaStateWeights" );
	m_DeltaStateWeights[MESH_DELTA_WEIGHT_LAGGED].Init( this, "deltaStateWeightsLagged" );
}

void CDmeMesh::OnDestruction()
{
	if ( g_pMaterialSystem )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		int nCount = m_hwFaceSets.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			if ( !m_hwFaceSets[i].m_bBuilt )
				continue;

			if ( m_hwFaceSets[i].m_pMesh )
			{
				pRenderContext->DestroyStaticMesh( m_hwFaceSets[i].m_pMesh );
			}
		}
		m_hwFaceSets.RemoveAll();
	}

	DeleteAttributeVarElementArray( m_BaseStates );
	DeleteAttributeVarElementArray( m_DeltaStates );
	DeleteAttributeVarElementArray( m_FaceSets );
}


//-----------------------------------------------------------------------------
// Initializes the normal material
//-----------------------------------------------------------------------------
void CDmeMesh::InitializeNormalMaterial()
{
	if ( !s_bNormalMaterialInitialized )
	{
		s_bNormalMaterialInitialized = true;

		KeyValues *pVMTKeyValues = new KeyValues( "wireframe" );
		pVMTKeyValues->SetInt( "$vertexcolor", 1 );
		pVMTKeyValues->SetInt( "$decal", 1 );
//		pVMTKeyValues->SetInt( "$ignorez", 0 );
		s_NormalMaterial.Init( "__DmeMeshNormalMaterial", pVMTKeyValues );

		pVMTKeyValues = new KeyValues( "unlitgeneric" );
		pVMTKeyValues->SetInt( "$vertexcolor", 1 );
		s_NormalErrorMaterial.Init( "__DmeMeshNormalErrorMaterial", pVMTKeyValues );
	}
}

//-----------------------------------------------------------------------------
// resolve internal data from changed attributes
//-----------------------------------------------------------------------------
void CDmeMesh::OnAttributeChanged( CDmAttribute *pAttribute )
{
	BaseClass::OnAttributeChanged( pAttribute );

	if ( pAttribute == m_DeltaStates.GetAttribute() )
	{
		int nDeltaStateCount = m_DeltaStates.Count();
		for ( int i = 0; i < MESH_DELTA_WEIGHT_TYPE_COUNT; ++i )
		{
			// Make sure we have the correct number of weights
			int nWeightCount = m_DeltaStateWeights[i].Count();
			if ( nWeightCount < nDeltaStateCount )
			{
				for ( int j = nWeightCount; j < nDeltaStateCount; ++j )
				{
					m_DeltaStateWeights[i].AddToTail( Vector2D( 0.0f, 0.0f ) );
				}
			}
			else if ( nDeltaStateCount > nWeightCount )
			{
				m_DeltaStateWeights[i].RemoveMultiple( nWeightCount, nWeightCount - nDeltaStateCount );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Adds deltas into a delta mesh
//-----------------------------------------------------------------------------
template< class T > bool CDmeMesh::AddVertexDelta(
	CDmeVertexData *pBaseState,
	void *pVertexData, int nStride, CDmeVertexDataBase::StandardFields_t fieldId, int nIndex, bool bDoLag )
{
	CDmeVertexDeltaData *pDeltaState = GetDeltaState( nIndex );

	if ( !pBaseState || !pDeltaState )
		return false;

	const FieldIndex_t nBaseFieldIndex = pBaseState->FindFieldIndex( fieldId == CDmeVertexData::FIELD_WRINKLE ? CDmeVertexData::FIELD_TEXCOORD : fieldId );
	const FieldIndex_t nDeltaFieldIndex = pDeltaState->FindFieldIndex( fieldId );
	if ( nBaseFieldIndex < 0 || nDeltaFieldIndex < 0 )
		return false;

	const CDmrArray<int> indices = pDeltaState->GetIndexData( nDeltaFieldIndex );
	const CDmrArray<T> delta = pDeltaState->GetVertexData( nDeltaFieldIndex );
	const int nDeltaCount = indices.Count();

	const float flWeight = m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL][nIndex].x;

	const FieldIndex_t nSpeedFieldIndex = pBaseState->FindFieldIndex( CDmeVertexData::FIELD_MORPH_SPEED );

	if ( !bDoLag || nSpeedFieldIndex < 0 )
	{
		for ( int j = 0; j < nDeltaCount; ++j )
		{
			int nDataIndex = indices.Get( j );
			T* pDeltaData = (T*)( (char*)pVertexData + nStride * nDataIndex );
			*pDeltaData += delta.Get( j ) * flWeight;
		}

		return true;
	}

	const float flLaggedWeight = m_DeltaStateWeights[MESH_DELTA_WEIGHT_LAGGED][nIndex].x;

	const CDmrArrayConst<int> speedIndices = pBaseState->GetIndexData( nSpeedFieldIndex );
	const CDmrArrayConst<float> speedDelta = pBaseState->GetVertexData( nSpeedFieldIndex );
	for ( int j = 0; j < nDeltaCount; ++j )
	{
		int nDataIndex = indices.Get( j );
		const CUtlVector<int> &list = pBaseState->FindVertexIndicesFromDataIndex( nBaseFieldIndex, nDataIndex );
		Assert( list.Count() > 0 );
		// FIXME: Average everything in the list.. shouldn't be necessary though
		float flSpeed = speedDelta.Get( speedIndices.Get( list[0] ) );
		float flActualWeight = Lerp( flSpeed, flLaggedWeight, flWeight );

		T* pDeltaData = (T*)( (char*)pVertexData + nStride * nDataIndex );
		*pDeltaData += delta.Get( j ) * flActualWeight;
	}
	return true;
}
	

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMesh::AddTexCoordDelta( RenderVertexDelta_t *pRenderDelta, float flWeight, CDmeVertexDeltaData *pDeltaState )
{
	FieldIndex_t nFieldIndex = pDeltaState->FindFieldIndex( CDmeVertexDeltaData::FIELD_TEXCOORD );
	if ( nFieldIndex < 0 )
		return;

	bool bIsVCoordinateFlipped = pDeltaState->IsVCoordinateFlipped();
	const CDmrArray<int> indices = pDeltaState->GetIndexData( nFieldIndex );
	const CDmrArray<Vector2D> delta = pDeltaState->GetVertexData( nFieldIndex );
	int nDeltaCount = indices.Count();
	for ( int j = 0; j < nDeltaCount; ++j )
	{
		Vector2D uvDelta = delta.Get( j );
		if ( bIsVCoordinateFlipped )
		{
			uvDelta.y = -uvDelta.y;
		}
		Vector2D &vec2D = pRenderDelta[ indices.Get( j ) ].m_vecDeltaUV;
		Vector2DMA( vec2D, flWeight, uvDelta, vec2D );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMesh::AddColorDelta( RenderVertexDelta_t *pRenderDelta, float flWeight, CDmeVertexDeltaData *pDeltaState )
{
	FieldIndex_t nFieldIndex = pDeltaState->FindFieldIndex( CDmeVertexDeltaData::FIELD_COLOR );
	if ( nFieldIndex < 0 )
		return;

	const CDmrArray<int> indices = pDeltaState->GetIndexData( nFieldIndex );
	const CDmrArray<Color> delta = pDeltaState->GetVertexData( nFieldIndex );
	int nDeltaCount = indices.Count();
	for ( int j = 0; j < nDeltaCount; ++j )
	{
		const Color &srcDeltaColor = delta[ j ];
		Vector4D &vecDelta	= pRenderDelta[ indices[ j ] ].m_vecDeltaColor;
		vecDelta[0] += flWeight * srcDeltaColor.r();
		vecDelta[1] += flWeight * srcDeltaColor.g();
		vecDelta[2] += flWeight * srcDeltaColor.b();
		vecDelta[3] += flWeight * srcDeltaColor.a();
	}
}

template< class T > bool CDmeMesh::AddStereoVertexDelta(
	CDmeVertexData *pBaseState,
	void *pVertexData, int nStride, CDmeVertexDataBase::StandardFields_t fieldId, int nIndex, bool bDoLag )
{
	CDmeVertexDeltaData *pDeltaState = GetDeltaState( nIndex );
	if ( !pBaseState || !pDeltaState )
		return false;

	const FieldIndex_t nBaseFieldIndex = pBaseState->FindFieldIndex( fieldId == CDmeVertexData::FIELD_WRINKLE ? CDmeVertexData::FIELD_TEXCOORD : fieldId );
	const FieldIndex_t nDeltaFieldIndex = pDeltaState->FindFieldIndex( fieldId );
	if ( nBaseFieldIndex < 0 || nDeltaFieldIndex < 0 )
		return false;

	float flLeftWeight = m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL][nIndex].x;
	float flRightWeight = m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL][nIndex].y;

	const CDmrArray<int> indices = pDeltaState->GetIndexData( nDeltaFieldIndex );
	const CDmrArray<T> delta = pDeltaState->GetVertexData( nDeltaFieldIndex );
	const CUtlVector<int>& balanceIndices = pBaseState->GetVertexIndexData( CDmeVertexData::FIELD_BALANCE );
	const CUtlVector<float> &balanceDelta = pBaseState->GetBalanceData();
	const int nDeltaCount = indices.Count();

	const FieldIndex_t nSpeedFieldIndex = pBaseState->FindFieldIndex( CDmeVertexData::FIELD_MORPH_SPEED );

	if ( !bDoLag || nSpeedFieldIndex < 0 )
	{
		for ( int j = 0; j < nDeltaCount; ++j )
		{
			int nDataIndex = indices.Get( j );
			const CUtlVector<int> &list = pBaseState->FindVertexIndicesFromDataIndex( nBaseFieldIndex, nDataIndex );
			Assert( list.Count() > 0 );
			// FIXME: Average everything in the list.. shouldn't be necessary though
			float flRightAmount = balanceDelta[ balanceIndices[ list[0] ] ];
			float flWeight = Lerp( flRightAmount, flLeftWeight, flRightWeight );	

			T* pDeltaData = (T*)( (char*)pVertexData + nStride * nDataIndex );
			*pDeltaData += delta.Get( j ) * flWeight;
		}

		return true;
	}

	float flLeftWeightLagged = m_DeltaStateWeights[MESH_DELTA_WEIGHT_LAGGED][nIndex].x;
	float flRightWeightLagged = m_DeltaStateWeights[MESH_DELTA_WEIGHT_LAGGED][nIndex].y;

	const CDmrArray<int> pSpeedIndices = pBaseState->GetIndexData( nSpeedFieldIndex );
	const CDmrArray<float> pSpeedDelta = pBaseState->GetVertexData( nSpeedFieldIndex );
	for ( int j = 0; j < nDeltaCount; ++j )
	{   
		int nDataIndex = indices.Get( j );
		const CUtlVector<int> &list = pBaseState->FindVertexIndicesFromDataIndex( nBaseFieldIndex, nDataIndex );
		Assert( list.Count() > 0 );
		// FIXME: Average everything in the list.. shouldn't be necessary though
		float flRightAmount = balanceDelta[ balanceIndices[ list[0] ] ];
		float flWeight = Lerp( flRightAmount, flLeftWeight, flRightWeight );	
		float flLaggedWeight = Lerp( flRightAmount, flLeftWeightLagged, flRightWeightLagged );	
		float flSpeed = pSpeedDelta.Get( pSpeedIndices.Get( list[0] ) );
		float flActualWeight = Lerp( flSpeed, flLaggedWeight, flWeight );

		T* pDeltaData = (T*)( (char*)pVertexData + nStride * nDataIndex );
		*pDeltaData += delta.Get( j ) * flActualWeight;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Draws the mesh when it uses too many bones
//-----------------------------------------------------------------------------
bool CDmeMesh::BuildDeltaMesh( int nVertices, RenderVertexDelta_t *pRenderDelta )
{
	bool bHasWrinkleDelta = false;

	memset( pRenderDelta, 0, nVertices * sizeof( RenderVertexDelta_t ) );
	int nCount = m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL].Count();
	Assert( m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL].Count() == m_DeltaStateWeights[MESH_DELTA_WEIGHT_LAGGED].Count() );

	CDmeVertexData *pBindState = GetBindBaseState();

	const FieldIndex_t nBalanceFieldIndex = pBindState->FindFieldIndex( CDmeVertexDeltaData::FIELD_BALANCE );
	const FieldIndex_t nSpeedFieldIndex = pBindState->FindFieldIndex( CDmeVertexDeltaData::FIELD_MORPH_SPEED );
	const bool bDoLag = nSpeedFieldIndex >= 0;
	if ( nBalanceFieldIndex < 0 )
	{
		for ( int i = 0; i < nCount; ++i )
		{
			float flWeight = m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL][i].x;
			float flLaggedWeight = m_DeltaStateWeights[MESH_DELTA_WEIGHT_LAGGED][i].x;
			if ( flWeight <= 0.0f && flLaggedWeight <= 0.0f )
				continue;

			// prepare vertices
			CDmeVertexDeltaData *pDeltaState = GetDeltaState(i);
			AddVertexDelta<Vector>( pBindState, &pRenderDelta->m_vecDeltaPosition, sizeof(RenderVertexDelta_t), CDmeVertexDeltaData::FIELD_POSITION, i, bDoLag );
			AddVertexDelta<Vector>( pBindState, &pRenderDelta->m_vecDeltaNormal, sizeof(RenderVertexDelta_t), CDmeVertexDeltaData::FIELD_NORMAL, i, bDoLag );
			AddTexCoordDelta( pRenderDelta, flWeight, pDeltaState );
			AddColorDelta( pRenderDelta, flWeight, pDeltaState );
			bool bWrinkle = AddVertexDelta<float>( pBindState, &pRenderDelta->m_flDeltaWrinkle, sizeof(RenderVertexDelta_t), CDmeVertexDeltaData::FIELD_WRINKLE, i, bDoLag );
			bHasWrinkleDelta = bHasWrinkleDelta || bWrinkle;
		}

		return bHasWrinkleDelta;
	}

	for ( int i = 0; i < nCount; ++i )
	{
		float flLeftWeight = m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL][i].x;
		float flRightWeight = m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL][i].y;
		float flLeftWeightLagged = m_DeltaStateWeights[MESH_DELTA_WEIGHT_LAGGED][i].x;
		float flRightWeightLagged = m_DeltaStateWeights[MESH_DELTA_WEIGHT_LAGGED][i].y;
		if ( flLeftWeight <= 0.0f && flRightWeight <= 0.0f && flLeftWeightLagged <= 0.0f && flRightWeightLagged <= 0.0f )
			continue;

		// FIXME: Need to make balanced versions of texcoord + color
		bool bWrinkle;
		CDmeVertexDeltaData *pDeltaState = GetDeltaState(i);
		AddStereoVertexDelta<Vector>( pBindState, &pRenderDelta->m_vecDeltaPosition, sizeof(RenderVertexDelta_t), CDmeVertexDeltaData::FIELD_POSITION, i, bDoLag );
		AddStereoVertexDelta<Vector>( pBindState, &pRenderDelta->m_vecDeltaNormal, sizeof(RenderVertexDelta_t), CDmeVertexDeltaData::FIELD_NORMAL, i, bDoLag );
		bWrinkle = AddStereoVertexDelta<float>( pBindState, &pRenderDelta->m_flDeltaWrinkle, sizeof(RenderVertexDelta_t), CDmeVertexDeltaData::FIELD_WRINKLE, i, bDoLag);
		bHasWrinkleDelta = bHasWrinkleDelta || bWrinkle;
		AddTexCoordDelta( pRenderDelta, flLeftWeight, pDeltaState );
		AddColorDelta( pRenderDelta, flLeftWeight, pDeltaState );
	}

	return bHasWrinkleDelta;
}


//-----------------------------------------------------------------------------
// Writes triangulated indices for a face set into a meshbuilder
//-----------------------------------------------------------------------------
void CDmeMesh::WriteTriangluatedIndices( const CDmeVertexData *pBaseState, CDmeFaceSet *pFaceSet, CMeshBuilder &meshBuilder )
{
	// prepare indices
	int nFirstIndex = 0;
	int nIndexCount = pFaceSet->NumIndices();
	while ( nFirstIndex < nIndexCount )
	{
		int nVertexCount = pFaceSet->GetNextPolygonVertexCount( nFirstIndex );
		if ( nVertexCount >= 3 )
		{
			int nOutCount = ( nVertexCount-2 ) * 3;
			int *pIndices = (int*)_alloca( nOutCount * sizeof(int)	);
			ComputeTriangulatedIndices( pBaseState, pFaceSet, nFirstIndex, pIndices, nOutCount );
			for ( int ii = 0; ii < nOutCount; ++ii )
			{
				meshBuilder.FastIndex( pIndices[ii] );
			}
		}
		nFirstIndex += nVertexCount + 1;
	}
}


//-----------------------------------------------------------------------------
// Draws the mesh when it uses too many bones
//-----------------------------------------------------------------------------
void CDmeMesh::DrawDynamicMesh( CDmeFaceSet *pFaceSet, matrix3x4_t *pPoseToWorld, bool bHasActiveDeltaStates, CDmeDrawSettings *pDrawSettings /* = NULL */ )
{
	CDmeVertexData *pBindBase = GetCurrentBaseState();
	if ( !pBindBase )
		return;

	// NOTE: This is inherently inefficient; we re-skin the *entire* mesh,
	// even if it's not being used by the entire model. This is because we can't
	// guarantee the various materials from the various face sets use the 
	// same vertex format (even though they should), and we don't want to
	// spend the work to detemine the sub-part of the mesh used by this face set.

	// Compute vertex deltas for rendering
	const int nVertices = pBindBase->VertexCount();

	// NOTE: The Delta Data is actually indexed by the pPositionIndices, pNormalIndices, etc.
	// The fact that we're storing one delta per final vertex nVertices
	// is a waste of memory and simply implementational convenience.
	bool bHasActiveWrinkle = false;
	RenderVertexDelta_t *pVertexDelta = (RenderVertexDelta_t*)_alloca( nVertices * sizeof(RenderVertexDelta_t) );
	if ( bHasActiveDeltaStates )
	{
		bHasActiveWrinkle = BuildDeltaMesh( nVertices, pVertexDelta );
	}
	else
	{
		pVertexDelta = NULL;
	}

	CRenderInfo renderInfo( pBindBase );
	Assert( renderInfo.HasPositionData() );

	// prepare vertices
	FieldIndex_t uvField = pBindBase->FindFieldIndex( CDmeVertexData::FIELD_TEXCOORD );
	FieldIndex_t colorField = pBindBase->FindFieldIndex( CDmeVertexData::FIELD_COLOR );

	bool bHasTexCoords = ( uvField >= 0 );
	bool bHasColors = ( colorField >= 0 );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( );

	const CDmrArrayConst<int> pUVIndices = bHasTexCoords ? pBindBase->GetIndexData( uvField ) : NULL;

	if ( bHasActiveWrinkle && bHasTexCoords )
	{
		// Create the wrinkle flex mesh
		IMesh *pFlexDelta = pRenderContext->GetFlexMesh();
		int nFlexVertexOffset = 0;

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pFlexDelta, MATERIAL_HETEROGENOUS, nVertices, 0, &nFlexVertexOffset );
		for ( int j=0; j < nVertices; j++ )
		{
			// NOTE: The UV indices are also used to index into wrinkle data
			int nUVIndex = pUVIndices.Get( j );
			meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
			meshBuilder.NormalDelta3f( 0.0f, 0.0f, 0.0f );
			meshBuilder.Wrinkle1f( pVertexDelta[nUVIndex].m_flDeltaWrinkle );
			meshBuilder.AdvanceVertex();
		}
		meshBuilder.End( false, false );
		pMesh->SetFlexMesh( pFlexDelta, nFlexVertexOffset );
	}

	// build the mesh
	int nIndices = pFaceSet->GetTriangulatedIndexCount();
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nVertices, nIndices );

	const CDmrArrayConst<Vector2D> pUVData = bHasTexCoords ? pBindBase->GetVertexData( uvField ) : NULL;
	const CDmrArrayConst<int> pColorIndices = bHasColors ? pBindBase->GetIndexData( colorField ) : NULL;
	const CDmrArrayConst<Color> pColorData = bHasColors ? pBindBase->GetVertexData( colorField ) : NULL;

	Vector vecPosition, vecNormal;
	Vector4D vecTangent;
	for ( int vi = 0; vi < nVertices; ++vi )
	{
		renderInfo.ComputeVertex( vi, pPoseToWorld, pVertexDelta, &vecPosition, &vecNormal, &vecTangent );
		meshBuilder.Position3fv( vecPosition.Base() );
		meshBuilder.Normal3fv( vecNormal.Base() );
		meshBuilder.UserData( vecTangent.Base() );

		if ( pUVData.IsValid() )
		{
			int uvi = pUVIndices.Get( vi );
			Vector2D uv = pUVData.Get( uvi );
			if ( pBindBase->IsVCoordinateFlipped() )
			{
				uv.y = 1.0f - uv.y;
			}

			if ( bHasActiveDeltaStates )
			{
				uv += pVertexDelta[uvi].m_vecDeltaUV;
			}
			meshBuilder.TexCoord2fv( 0, uv.Base() );
		}
		else
		{
			meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		}

		if ( pColorIndices.IsValid() )
		{
			int ci = pColorIndices.Get( vi );
			int color = pColorData.Get( ci ).GetRawColor();
			meshBuilder.Color4ubv( (unsigned char*)&color );
		}
		else
		{
			meshBuilder.Color4ub( 255, 255, 255, 255 );
		}

		meshBuilder.AdvanceVertex();
	}

	WriteTriangluatedIndices( pBindBase, pFaceSet, meshBuilder );

	meshBuilder.End();

	pMesh->Draw();

	if ( pDrawSettings && pDrawSettings->GetNormals() )
	{
		RenderNormals( pPoseToWorld, bHasActiveDeltaStates ? pVertexDelta : NULL );
	}
}


//-----------------------------------------------------------------------------
// Renders normals 
//-----------------------------------------------------------------------------
#define NORMAL_LINE_SIZE 0.25f

void CDmeMesh::RenderNormals( matrix3x4_t *pPoseToWorld, RenderVertexDelta_t *pDelta )
{
	CDmeVertexData *pBind = GetBindBaseState();
	if ( !pBind )
		return;

	CRenderInfo renderInfo( pBind );

	Assert( renderInfo.HasPositionData() );
	if ( !renderInfo.HasNormalData() )
		return;
	bool bHasTangents = renderInfo.HasTangentData();

	// build the mesh
	InitializeNormalMaterial();
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( s_NormalMaterial );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( );

	int nMaxIndices, nMaxVertices;
	pRenderContext->GetMaxToRender( pMesh, false, &nMaxVertices, &nMaxIndices );
	int nFirstVertex = 0;
	int nVerticesRemaining = pBind->VertexCount();;
	int nFactor = bHasTangents ? 6 : 2;

	while ( nVerticesRemaining > 0 )
	{
		int nVertices = nVerticesRemaining;
		if ( nVertices > nMaxVertices / nFactor )
		{
			nVertices = nMaxVertices / nFactor;
		}
		if ( nVertices > nMaxIndices / nFactor )
		{
			nVertices = nMaxIndices / nFactor;
		}
		nVerticesRemaining -= nVertices;

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_LINES, bHasTangents ? nVertices * 3 : nVertices );

		Vector vecPosition, vecNormal, vecEndPoint, vecTangentS, vecTangentT;
		Vector4D vecTangent;
		for ( int vi = nFirstVertex; vi < nVertices; ++vi )
		{
			renderInfo.ComputeVertex( vi, pPoseToWorld, pDelta, &vecPosition, &vecNormal, &vecTangent );

			meshBuilder.Position3fv( vecPosition.Base() );
			meshBuilder.Color4ub( 0, 0, 255, 255 );
			meshBuilder.AdvanceVertex();

			VectorMA( vecPosition, NORMAL_LINE_SIZE, vecNormal, vecEndPoint );
			meshBuilder.Position3fv( vecEndPoint.Base() );
			meshBuilder.Color4ub( 0, 0, 255, 255 );
			meshBuilder.AdvanceVertex();

			continue;

			if ( !bHasTangents )
				continue;

			CrossProduct( vecNormal, vecTangent.AsVector3D(), vecTangentT );
			VectorNormalize( vecTangentT );
			// NOTE: This is the new, desired tangentS morphing behavior
			//		CrossProduct( vecTangentT, vecNormal, vecTangentS );
			VectorCopy( vecTangent.AsVector3D(), vecTangentS );
			vecTangentT *= vecTangent.w;

			meshBuilder.Position3fv( vecPosition.Base() );
			meshBuilder.Color4ub( 255, 0, 0, 255 );
			meshBuilder.AdvanceVertex();

			VectorMA( vecPosition, NORMAL_LINE_SIZE, vecTangentS, vecEndPoint );
			meshBuilder.Position3fv( vecEndPoint.Base() );
			meshBuilder.Color4ub( 255, 0, 0, 255 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Position3fv( vecPosition.Base() );
			meshBuilder.Color4ub( 0, 255, 0, 255 );
			meshBuilder.AdvanceVertex();

			VectorMA( vecPosition, NORMAL_LINE_SIZE, vecTangentT, vecEndPoint );
			meshBuilder.Position3fv( vecEndPoint.Base() );
			meshBuilder.Color4ub( 0, 255, 0, 255 );
			meshBuilder.AdvanceVertex();
		}

		meshBuilder.End();
		pMesh->Draw();

		nFirstVertex += nVertices;
	}
}


//-----------------------------------------------------------------------------
// Draws the passed DmeFaceSet in wireframe mode
//-----------------------------------------------------------------------------
void CDmeMesh::DrawWireframeFaceSet( CDmeFaceSet *pFaceSet, matrix3x4_t *pPoseToWorld, bool bHasActiveDeltaStates, CDmeDrawSettings *pDrawSettings )
{ 
	CDmeVertexData *pBind = GetBindBaseState();
	if ( !pBind )
		return;

	const FieldIndex_t posField = pBind->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	if ( posField < 0 )
		return;

	const CUtlVector< Vector > &posData( CDmrArrayConst< Vector >( pBind->GetVertexData( posField ) ).Get() );
	const int nVertices = posData.Count();

	const CUtlVector< int > &posIndices( CDmrArrayConst< int >( pBind->GetIndexData( posField ) ).Get() );

	Vector *pDeltaVertices = bHasActiveDeltaStates ? pDeltaVertices = reinterpret_cast< Vector * >( alloca( nVertices * sizeof( Vector ) ) ) : NULL;

	if ( bHasActiveDeltaStates )
	{
		memset( pDeltaVertices, 0, sizeof( Vector ) * nVertices );
		const int nCount = m_DeltaStateWeights[ MESH_DELTA_WEIGHT_NORMAL ].Count();

		const FieldIndex_t nBalanceFieldIndex = pBind->FindFieldIndex( CDmeVertexDeltaData::FIELD_BALANCE );
		const FieldIndex_t nSpeedFieldIndex = pBind->FindFieldIndex( CDmeVertexDeltaData::FIELD_MORPH_SPEED );
		const bool bDoLag = ( nSpeedFieldIndex >= 0 );

		if ( nBalanceFieldIndex < 0 )
		{
			for ( int i = 0; i < nCount; ++i )
			{
				float flWeight = m_DeltaStateWeights[ MESH_DELTA_WEIGHT_NORMAL ][ i ].x;
				float flLaggedWeight = m_DeltaStateWeights[ MESH_DELTA_WEIGHT_LAGGED ][ i ].x;
				if ( flWeight <= 0.0f && ( !bDoLag || flLaggedWeight <= 0.0f ) )
					continue;

				AddVertexDelta< Vector >( pBind, pDeltaVertices, sizeof( Vector ), CDmeVertexDeltaData::FIELD_POSITION, i, bDoLag );
			}
		}
		else
		{
			for ( int i = 0; i < nCount; ++i )
			{
				float flLeftWeight = m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL][i].x;
				float flRightWeight = m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL][i].y;
				float flLeftWeightLagged = m_DeltaStateWeights[MESH_DELTA_WEIGHT_LAGGED][i].x;
				float flRightWeightLagged = m_DeltaStateWeights[MESH_DELTA_WEIGHT_LAGGED][i].y;
				if ( flLeftWeight <= 0.0f && flRightWeight <= 0.0f && ( !bDoLag || ( flLeftWeightLagged <= 0.0f && flRightWeightLagged <= 0.0f ) ) )
					continue;

				AddStereoVertexDelta< Vector >( pBind, pDeltaVertices, sizeof( Vector ), CDmeVertexDeltaData::FIELD_POSITION, i, bDoLag );
			}
		}
	}

	Vector *pVertices = reinterpret_cast< Vector * >( alloca( nVertices * sizeof( Vector ) ) );

	CRenderInfo renderInfo( pBind );
	Assert( renderInfo.HasPositionData() );

	for ( int pi = 0; pi < nVertices; ++pi )
	{
		renderInfo.ComputePosition( pi, pPoseToWorld, pDeltaVertices, pVertices );
	}

	InitializeNormalMaterial();
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( s_NormalMaterial );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();

	// build the mesh
	CMeshBuilder meshBuilder;

	// Draw the polygons in the face set
	const int nFaceSetIndices = pFaceSet->NumIndices();
	const int *pFaceSetIndices = pFaceSet->GetIndices();

	int vR = 0;
	int vG = 0;
	int vB = 0;

	if ( pDrawSettings )
	{
		const Color &vColor = pDrawSettings->GetColor();
		vR = vColor.r();
		vG = vColor.g();
		vB = vColor.b();
	}

	int nFaceIndices;
	for ( int i = 0; i < nFaceSetIndices; )
	{
		nFaceIndices = pFaceSet->GetNextPolygonVertexCount( i );
		meshBuilder.Begin( pMesh, MATERIAL_LINES, nFaceIndices );

		for ( int j = 0; j < nFaceIndices; ++j )
		{
			Assert( i < nFaceSetIndices );

			int vIndex0 = posIndices[ pFaceSetIndices[ i + j ] ];
			Assert( vIndex0 < nVertices );
			meshBuilder.Position3fv( reinterpret_cast< float * >( pVertices + vIndex0 ) );
			meshBuilder.Color3ub( vR, vG, vB );
			meshBuilder.AdvanceVertex();

			int vIndex1 = posIndices[ pFaceSetIndices[ i + ( ( j + 1 ) % nFaceIndices ) ] ];
			Assert( vIndex1 < nVertices );
			meshBuilder.Position3fv( reinterpret_cast< float * >( pVertices + vIndex1) );
			meshBuilder.Color3ub( vR, vG, vB );
			meshBuilder.AdvanceVertex();
		}

		meshBuilder.End();

		i += nFaceIndices + 1;
	}

	pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Do we have active delta state data?
//-----------------------------------------------------------------------------
bool CDmeMesh::HasActiveDeltaStates() const
{
	for ( int t = 0; t < MESH_DELTA_WEIGHT_TYPE_COUNT; ++t )
	{
		int nCount = m_DeltaStateWeights[t].Count();
		for ( int i = 0; i < nCount; ++i )
		{
			if ( m_DeltaStateWeights[t][i].x != 0.0f || m_DeltaStateWeights[t][i].y != 0.0f )
				return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Draws the mesh
//-----------------------------------------------------------------------------
void CDmeMesh::Draw( const matrix3x4_t &shapeToWorld, CDmeDrawSettings *pDrawSettings /* = NULL */ )
{
	const CDmeVertexData *pBind = GetBindBaseState();

	if ( !pBind || !g_pMaterialSystem || !g_pMDLCache || !g_pStudioRender )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	const bool bHasActiveDeltaStates = HasActiveDeltaStates();
	const bool bDrawNormals = pDrawSettings ? pDrawSettings->GetNormals() : false;
	const CDmeDrawSettings::DrawType_t drawType = pDrawSettings ? pDrawSettings->GetDrawType() : CDmeDrawSettings::DRAW_SMOOTH;

	const bool bShaded = ( drawType == CDmeDrawSettings::DRAW_SMOOTH || drawType == CDmeDrawSettings::DRAW_FLAT );
	const bool bWireframe = ( drawType == CDmeDrawSettings::DRAW_WIREFRAME );
//	const bool bBoundingBox = ( drawType == CDmeDrawSettings::DRAW_BOUNDINGBOX );

	const bool bSoftwareSkinning = bHasActiveDeltaStates | bDrawNormals | bWireframe;

	matrix3x4_t *pPoseToWorld = CDmeModel::SetupModelRenderState( shapeToWorld, pBind->HasSkinningData(), bSoftwareSkinning );

	pRenderContext->SetNumBoneWeights( pPoseToWorld ? 0 : pBind->JointCount() );

	int nFaceSets = FaceSetCount();
	m_hwFaceSets.EnsureCount( nFaceSets );

	const bool bindMaterial = pDrawSettings ? !pDrawSettings->IsAMaterialBound() : true;

	for ( int fi = 0; fi < nFaceSets; ++fi )
	{
		CDmeFaceSet *pFaceSet = GetFaceSet( fi );

		if ( bWireframe )
		{
			DrawWireframeFaceSet( pFaceSet, pPoseToWorld, bHasActiveDeltaStates, pDrawSettings );
			continue;
		}

		if ( bindMaterial )
		{
			pRenderContext->Bind( pFaceSet->GetMaterial()->GetCachedMTL() );
		}

		if ( pPoseToWorld || ( bShaded && bSoftwareSkinning ) )
		{
			DrawDynamicMesh( pFaceSet, pPoseToWorld, bHasActiveDeltaStates, pDrawSettings );
			continue;
		}

		// TODO: figure out how to tell the mesh when the faceset's indices change
		if ( !m_hwFaceSets[fi].m_bBuilt )
		{
			m_hwFaceSets[fi].m_pMesh = CreateHwMesh( pFaceSet );
			m_hwFaceSets[fi].m_bBuilt = true;
		}

		if ( m_hwFaceSets[fi].m_pMesh )
		{
			m_hwFaceSets[fi].m_pMesh->Draw();
		}
	}

	pRenderContext->SetNumBoneWeights( 0 );
	CDmeModel::CleanupModelRenderState();
}


//-----------------------------------------------------------------------------
// Face sets
//-----------------------------------------------------------------------------
int CDmeMesh::FaceSetCount() const
{
	return m_FaceSets.Count();
}

CDmeFaceSet *CDmeMesh::GetFaceSet( int faceSetIndex )
{
	return m_FaceSets[ faceSetIndex ];
}

const CDmeFaceSet *CDmeMesh::GetFaceSet( int faceSetIndex ) const
{
	return m_FaceSets[ faceSetIndex ];
}

void CDmeMesh::AddFaceSet( CDmeFaceSet *faceSet )
{
	m_FaceSets.AddToTail( faceSet );
}

void CDmeMesh::RemoveFaceSet( int faceSetIndex )
{
	m_FaceSets.Remove( faceSetIndex );
}


//-----------------------------------------------------------------------------
// Find a base state by name
//-----------------------------------------------------------------------------
CDmeVertexData *CDmeMesh::FindBaseState( const char *pStateName ) const
{
	const int nBaseStateCount = BaseStateCount();
	for ( int i = 0; i < nBaseStateCount; ++i )
	{
		CDmeVertexData *pBaseState = GetBaseState( i );
		if ( !Q_stricmp( pStateName, pBaseState->GetName() ) )
			return pBaseState;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Find a base state by name, add a new one if not found
//-----------------------------------------------------------------------------
CDmeVertexData *CDmeMesh::FindOrCreateBaseState( const char *pStateName )
{
	CDmeVertexData *pBaseState = FindBaseState( pStateName );
	if ( pBaseState )
		return pBaseState;

	pBaseState = CreateElement< CDmeVertexData >( pStateName, GetFileId() );
	m_BaseStates.AddToTail( pBaseState );

	return pBaseState;
}


//-----------------------------------------------------------------------------
// Remove a base state by name
//-----------------------------------------------------------------------------
bool CDmeMesh::DeleteBaseState( const char *pStateName )
{
	const int nBaseStateCount = BaseStateCount();
	for ( int i = 0; i < nBaseStateCount; ++i )
	{
		const CDmeVertexData *pBaseState = GetBaseState( i );
		if ( !Q_stricmp( pStateName, pBaseState->GetName() ) )
		{
			m_BaseStates.Remove( i );
			g_pDataModel->DestroyElement( pBaseState->GetHandle() );

			// TODO: Fix up all dependent states
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Selects a particular base state to be current state
//-----------------------------------------------------------------------------
void CDmeMesh::SetCurrentBaseState( const char *pStateName )
{
	m_CurrentBaseState = FindBaseState( pStateName );
}


//-----------------------------------------------------------------------------
// Selects a particular base state to be current state
//-----------------------------------------------------------------------------
CDmeVertexData *CDmeMesh::GetCurrentBaseState()
{
	return m_CurrentBaseState;
}


//-----------------------------------------------------------------------------
// Selects a particular base state to be current state
//-----------------------------------------------------------------------------
const CDmeVertexData *CDmeMesh::GetCurrentBaseState() const
{
	return m_CurrentBaseState;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeMesh::SetBindBaseState( CDmeVertexData *pBaseState )
{
	if ( !pBaseState )
		return false;

	CDmeVertexData *pCheckState = FindBaseState( pBaseState->GetName() );
	if ( pCheckState != pBaseState )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeVertexData *CDmeMesh::GetBindBaseState()
{
	if ( m_BindBaseState.GetElement() )
		return m_BindBaseState;

	// Backwards compatibility
	return FindBaseState( "bind" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const CDmeVertexData *CDmeMesh::GetBindBaseState() const
{
	if ( m_BindBaseState.GetElement() )
		return m_BindBaseState;

	// Backwards compatibility
	return FindBaseState( "bind" );
}


//-----------------------------------------------------------------------------
// Delta states
//-----------------------------------------------------------------------------
int CDmeMesh::DeltaStateCount() const
{
	return m_DeltaStates.Count();
}


//-----------------------------------------------------------------------------
// Returns the delta 
//-----------------------------------------------------------------------------
CDmeVertexDeltaData *CDmeMesh::GetDeltaState( int nDeltaIndex ) const
{
	if ( nDeltaIndex < 0 || nDeltaIndex >= m_DeltaStates.Count() )
		return NULL;

	return m_DeltaStates[ nDeltaIndex ];
}


//-----------------------------------------------------------------------------
// Finds a delta state by name.  If it isn't found, return NULL
//-----------------------------------------------------------------------------
CDmeVertexDeltaData *CDmeMesh::FindDeltaState( const char *pDeltaName ) const
{
	return GetDeltaState( FindDeltaStateIndex( pDeltaName ) );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int SortDeltaNameFunc( const void *a, const void *b )
{
	return Q_strcmp( *( const char ** )( a ), *( const char ** )( b ) );
}


//-----------------------------------------------------------------------------
// If the name doe
//-----------------------------------------------------------------------------
const char *SortDeltaName( const char *pInDeltaName, char *pOutDeltaName, int nOutDeltaNameBufLen )
{
	if ( !pInDeltaName || !strchr( pInDeltaName, '_' ) )
		return pInDeltaName;

	char **ppDeltaNames = reinterpret_cast< char ** >( stackalloc( nOutDeltaNameBufLen * sizeof( char * ) ) );
	memset( ppDeltaNames, 0, nOutDeltaNameBufLen * sizeof( char * ) );

	const char *pStart = pInDeltaName;
	int nDimensionCount = 0;
	while ( pStart )
	{
		const char *pUnderBar = strchr( pStart, '_' );
		const int nControlNameBufLen = ( pUnderBar ? pUnderBar - pStart : Q_strlen( pStart ) ) + 1;

		if ( nControlNameBufLen )
		{
			ppDeltaNames[ nDimensionCount ] = reinterpret_cast< char * >( stackalloc( nControlNameBufLen * sizeof( char ) ) );
			Q_strncpy( ppDeltaNames[ nDimensionCount ], pStart, nControlNameBufLen );
			++nDimensionCount;
		}

		pStart = pUnderBar;
		if ( pStart )
		{
			++pStart;
		}
	}

	// This should only happen if the input name is all _'s
	if ( nDimensionCount <= 0 )
		return pInDeltaName;

	qsort( ppDeltaNames, nDimensionCount, sizeof( char * ), SortDeltaNameFunc );

	char *pDst = pOutDeltaName;
	for ( int i = 0; i < nDimensionCount; ++i )
	{
		if ( i != 0 )
		{
			Q_strncpy( pDst, "_", nOutDeltaNameBufLen );
			++pDst;
			--nOutDeltaNameBufLen;
		}

		const int nControlNameLen = Q_strlen( ppDeltaNames[ i ] );
		Q_strncpy( pDst, ppDeltaNames[ i ], nOutDeltaNameBufLen );
		pDst += nControlNameLen;
		nOutDeltaNameBufLen -= nControlNameLen;
	}

	return pOutDeltaName;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeVertexDeltaData *CDmeMesh::FindOrCreateDeltaState( const char *pInDeltaName )
{
	CDmeVertexDeltaData *pDeltaState = FindDeltaState( pInDeltaName );
	if ( pDeltaState )
		return pDeltaState;

	const int nDeltaNameBufLen = Q_strlen( pInDeltaName ) + 1;
	char *pDeltaNameBuf = reinterpret_cast< char * >( stackalloc( nDeltaNameBufLen * sizeof( char ) ) );
	const char *pDeltaName = SortDeltaName( pInDeltaName, pDeltaNameBuf, nDeltaNameBufLen );

	pDeltaState = CreateElement< CDmeVertexDeltaData >( pDeltaName, GetFileId() );
	if ( pDeltaState )
	{
		m_DeltaStates.AddToTail( pDeltaState );
	}

	return pDeltaState;
}


//-----------------------------------------------------------------------------
// Finds a delta state index by comparing names, if it can't be found
// searches for all permutations of the delta name
//-----------------------------------------------------------------------------
int CDmeMesh::FindDeltaStateIndex( const char *pInDeltaName ) const
{
	const char *pDeltaName = pInDeltaName;

	if ( strchr( pInDeltaName, '_' ) )
	{
		const int nDeltaNameBufLen = Q_strlen( pInDeltaName ) + 1;
		char *pDeltaNameBuf = reinterpret_cast< char * >( stackalloc( nDeltaNameBufLen * sizeof( char ) ) );
		pDeltaName = SortDeltaName( pInDeltaName, pDeltaNameBuf, nDeltaNameBufLen );
	}

	int dn = DeltaStateCount();
	for ( int di = 0; di < dn; ++di )
	{
		CDmeVertexDeltaData *pDeltaState = GetDeltaState( di );
		if ( !Q_stricmp( pDeltaName, pDeltaState->GetName() ) )
			return di;
	}

	return -1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMesh::SetDeltaStateWeight( int nDeltaIndex, MeshDeltaWeightType_t type, float flMorphWeight )
{
	if ( nDeltaIndex < m_DeltaStateWeights[type].Count() )
	{
		m_DeltaStateWeights[type].Set( nDeltaIndex, Vector2D( flMorphWeight, flMorphWeight ) );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMesh::SetDeltaStateWeight( int nDeltaIndex, MeshDeltaWeightType_t type, float flLeftWeight, float flRightWeight )
{
	if ( nDeltaIndex < m_DeltaStateWeights[type].Count() )
	{
		m_DeltaStateWeights[type].Set( nDeltaIndex, Vector2D( flLeftWeight, flRightWeight ) );
	}
}

//-----------------------------------------------------------------------------
// Determines the appropriate vertex format for hardware meshes
//-----------------------------------------------------------------------------
VertexFormat_t CDmeMesh::ComputeHwMeshVertexFormat( void )
{
	bool           bIsDX7       = !g_pMaterialSystemHardwareConfig->SupportsVertexAndPixelShaders();
	VertexFormat_t vertexFormat = VERTEX_POSITION | VERTEX_COLOR | VERTEX_NORMAL | VERTEX_TEXCOORD_SIZE(0,2) | VERTEX_BONEWEIGHT(2) | VERTEX_BONE_INDEX
									| ( bIsDX7 ? 0 : VERTEX_USERDATA_SIZE(4) );

	// FIXME: set VERTEX_FORMAT_COMPRESSED if there are no artifacts and if it saves enough memory (use 'mem_dumpvballocs')
	// vertexFormat |= VERTEX_FORMAT_COMPRESSED;
	// FIXME: check for and strip unused vertex elements (see 'bHasNormals', etc, in CreateHwMesh below)

	return vertexFormat;
}

//-----------------------------------------------------------------------------
// Builds a hardware mesh
//-----------------------------------------------------------------------------
IMesh *CDmeMesh::CreateHwMesh( CDmeFaceSet *pFaceSet )
{
	const CDmeVertexData *pBind = GetBindBaseState();
	if ( !pBind )
		return NULL;

	// NOTE: This is memory inefficient. We create a copy of all vertices
	// for each face set, even if those vertices aren't used by the face set
	// Mostly chose to do this for code simplicity, although it also is faster to generate meshes
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	VertexFormat_t vertexFormat = ComputeHwMeshVertexFormat( );
	IMesh *pMesh = pRenderContext->CreateStaticMesh( vertexFormat, "dmemesh" );

	CMeshBuilder meshBuilder;

	// prepare vertices
	FieldIndex_t posField = pBind->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	FieldIndex_t normalField = pBind->FindFieldIndex( CDmeVertexData::FIELD_NORMAL );
	FieldIndex_t tangentField = pBind->FindFieldIndex( CDmeVertexData::FIELD_TANGENT );
	FieldIndex_t uvField = pBind->FindFieldIndex( CDmeVertexData::FIELD_TEXCOORD );
	FieldIndex_t colorField = pBind->FindFieldIndex( CDmeVertexData::FIELD_COLOR );

	Assert( posField >= 0 );
	bool bHasNormals = ( normalField >= 0 );
	bool bHasTangent = ( tangentField >= 0 );
	bool bHasTexCoords = ( uvField >= 0 );
	bool bHasColors = ( colorField >= 0 );

	// build the mesh
	int nIndices = pFaceSet->GetTriangulatedIndexCount();
	int nVertices = pBind->VertexCount();
	int nJointCount = pBind->JointCount();
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nVertices, nIndices );

	const CDmrArrayConst<int> pPositionIndices = pBind->GetIndexData( posField );
	const CDmrArrayConst<Vector> pPositionData = pBind->GetVertexData( posField );
	const CDmrArrayConst<int> pNormalIndices = bHasNormals ? pBind->GetIndexData( normalField ) : NULL;
	const CDmrArrayConst<Vector> pNormalData = bHasNormals ? pBind->GetVertexData( normalField ) : NULL;
	const CDmrArrayConst<int> pTangentIndices = bHasTangent ? pBind->GetIndexData( tangentField ) : NULL;
	const CDmrArrayConst<Vector4D> pTangentData = bHasTangent ? pBind->GetVertexData( tangentField ) : NULL;
	const CDmrArrayConst<int> pUVIndices = bHasTexCoords ? pBind->GetIndexData( uvField ) : NULL;
	const CDmrArrayConst<Vector2D> pUVData = bHasTexCoords ? pBind->GetVertexData( uvField ) : NULL;
	const CDmrArrayConst<int> pColorIndices = bHasColors ? pBind->GetIndexData( colorField ) : NULL;
	const CDmrArrayConst<Color> pColorData = bHasColors ? pBind->GetVertexData( colorField ) : NULL;

	Vector4D defaultTangentS( 1.0f, 0.0f, 0.0f, 1.0f );
	for ( int vi = 0; vi < nVertices; ++vi )
	{
		meshBuilder.Position3fv( pPositionData.Get( pPositionIndices.Get( vi ) ).Base() );
		if ( pNormalData.IsValid() )
		{
			meshBuilder.Normal3fv( pNormalData.Get( pNormalIndices.Get( vi ) ).Base() );
		}
		else
		{
			meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
		}
		if ( pTangentData.IsValid() )
		{
			meshBuilder.UserData( pTangentData.Get( pTangentIndices.Get( vi ) ).Base() );
		}
		else
		{
			meshBuilder.UserData( defaultTangentS.Base() );
		}
		if ( pUVData.IsValid() )
		{
			const Vector2D &uv = pUVData.Get( pUVIndices.Get( vi ) );
			if ( !pBind->IsVCoordinateFlipped() )
			{
				meshBuilder.TexCoord2fv( 0, uv.Base() );
			}
			else
			{
				meshBuilder.TexCoord2f( 0, uv.x, 1.0f - uv.y );
			}
		}
		else
		{
			meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
		}
		if ( pColorIndices.IsValid() )
		{
			int color = pColorData.Get( pColorIndices.Get( vi ) ).GetRawColor();
			meshBuilder.Color4ubv( (unsigned char*)&color );
		}
		else
		{
			meshBuilder.Color4ub( 255, 255, 255, 255 );
		}

		// FIXME: Note that this will break once we exceeed the max joint count
		// that the hardware can handle
		const float *pJointWeight = pBind->GetJointWeights( vi );
		const int *pJointIndices = pBind->GetJointIndices( vi );
		for ( int i = 0; i < nJointCount; ++i )
		{
			meshBuilder.BoneWeight( i, pJointWeight[i] );
			meshBuilder.BoneMatrix( i, pJointIndices[i] );
		}

		for ( int i = nJointCount; i < 4; ++i )
		{
			meshBuilder.BoneWeight( i, ( i == 0 ) ? 1.0f : 0.0f );
			meshBuilder.BoneMatrix( i, 0 );
		}

		meshBuilder.AdvanceVertex();
	}

	// prepare indices
	int nFirstIndex = 0;
	int nIndexCount = pFaceSet->NumIndices();
	while ( nFirstIndex < nIndexCount )
	{
		int nVertexCount = pFaceSet->GetNextPolygonVertexCount( nFirstIndex );
		if ( nVertexCount >= 3 )
		{
			int nOutCount = ( nVertexCount-2 ) * 3;
			int *pIndices = (int*)_alloca( nOutCount * sizeof(int)	);
			ComputeTriangulatedIndices( pBind, pFaceSet, nFirstIndex, pIndices, nOutCount );
			for ( int ii = 0; ii < nOutCount; ++ii )
			{
				meshBuilder.FastIndex( pIndices[ii] );
			}
		}
		nFirstIndex += nVertexCount + 1;
	}

	meshBuilder.End();

	return pMesh;
}


//-----------------------------------------------------------------------------
// Compute triangulated indices
//-----------------------------------------------------------------------------
void CDmeMesh::ComputeTriangulatedIndices( const CDmeVertexData *pBaseState, CDmeFaceSet *pFaceSet, int nFirstIndex, int *pIndices, int nOutCount )
{
	// FIXME: Come up with a more efficient way of computing this
	// This involves a bunch of recomputation of distances
	float flMinDistance = FLT_MAX;
	int nMinIndex = 0;
	int nVertexCount = pFaceSet->GetNextPolygonVertexCount( nFirstIndex );

	// Optimization for quads + triangles.. it's totally symmetric
	int nLoopCount = nVertexCount;
	if ( nVertexCount <= 3 )
	{
		nLoopCount = 0;
	}
	else if ( nVertexCount == 4 )
	{
		nLoopCount = 2;
	}

	for ( int i = 0; i < nLoopCount; ++i )
	{
		float flDistance = 0.0f;
		const Vector &vecCenter = pBaseState->GetPosition( pFaceSet->GetIndex( nFirstIndex+i ) );
		for ( int j = 2; j < nVertexCount-1; ++j )
		{
			int vi = ( i + j ) % nVertexCount;
			const Vector &vecEdge = pBaseState->GetPosition( pFaceSet->GetIndex( nFirstIndex+vi ) );
			flDistance += vecEdge.DistTo( vecCenter );
		}

		if ( flDistance < flMinDistance )
		{
			nMinIndex = i;
			flMinDistance = flDistance;
		}
	}
	 
	// Compute the triangulation indices
	Assert( nOutCount == ( nVertexCount - 2 ) * 3 );
	int nOutIndex = 0;
	for ( int i = 1; i < nVertexCount - 1; ++i )
	{
		pIndices[nOutIndex++] = pFaceSet->GetIndex( nFirstIndex + nMinIndex );
		pIndices[nOutIndex++] = pFaceSet->GetIndex( nFirstIndex + ((nMinIndex + i) % nVertexCount) );
		pIndices[nOutIndex++] = pFaceSet->GetIndex( nFirstIndex + ((nMinIndex + i + 1) % nVertexCount) );
	}
}


//-----------------------------------------------------------------------------
// Build a map from vertex index to a list of triangles that share the vert.
//-----------------------------------------------------------------------------
void CDmeMesh::BuildTriangleMap( const CDmeVertexData *pBaseState, CDmeFaceSet* pFaceSet, CUtlVector<Triangle_t>& triangles, CUtlVector< CUtlVector<int> >* pVertToTriMap )
{
	// prepare indices
	int nFirstIndex = 0;
	int nIndexCount = pFaceSet->NumIndices();
	while ( nFirstIndex < nIndexCount )
	{
		int nVertexCount = pFaceSet->GetNextPolygonVertexCount( nFirstIndex );
		if ( nVertexCount >= 3 )
		{
			int nOutCount = ( nVertexCount-2 ) * 3;
			int *pIndices = (int*)_alloca( nOutCount * sizeof(int) );
			ComputeTriangulatedIndices( pBaseState, pFaceSet, nFirstIndex, pIndices, nOutCount );
			for ( int ii = 0; ii < nOutCount; ii += 3 )
			{
				int t = triangles.AddToTail();
				Triangle_t& triangle = triangles[t];

				triangle.m_nIndex[0] = pIndices[ii];
				triangle.m_nIndex[1] = pIndices[ii+1];
				triangle.m_nIndex[2] = pIndices[ii+2];

				if ( pVertToTriMap )
				{
					(*pVertToTriMap)[ pIndices[ii] ].AddToTail( t );
					(*pVertToTriMap)[ pIndices[ii+1] ].AddToTail( t );
					(*pVertToTriMap)[ pIndices[ii+2] ].AddToTail( t );
				}
			}
		}
		nFirstIndex += nVertexCount + 1;
	}
}


//-----------------------------------------------------------------------------
// Computes tangent space data for triangles
//-----------------------------------------------------------------------------
void CDmeMesh::ComputeTriangleTangets( const CDmeVertexData *pVertexData, CUtlVector<Triangle_t>& triangles )
{
	// Calculate the tangent space for each triangle.
	int nTriangleCount = triangles.Count();
	for ( int triID = 0; triID < nTriangleCount; triID++ )
	{
		Triangle_t &triangle = triangles[triID];

		const Vector &p0 = pVertexData->GetPosition( triangle.m_nIndex[0] );
		const Vector &p1 = pVertexData->GetPosition( triangle.m_nIndex[1] );
		const Vector &p2 = pVertexData->GetPosition( triangle.m_nIndex[2] );
		const Vector2D &t0 = pVertexData->GetTexCoord( triangle.m_nIndex[0] );
		const Vector2D &t1 = pVertexData->GetTexCoord( triangle.m_nIndex[1] );
		const Vector2D &t2 = pVertexData->GetTexCoord( triangle.m_nIndex[2] );
		CalcTriangleTangentSpace( p0, p1, p2, t0, t1, t2, triangle.m_vecTangentS, triangle.m_vecTangentT );
	}	
}


//-----------------------------------------------------------------------------
// Build a map from vertex index to a list of triangles that share the vert.
//-----------------------------------------------------------------------------
void CDmeMesh::ComputeAverageTangent( CDmeVertexData *pVertexData, bool bSmoothTangents, CUtlVector< CUtlVector<int> >& vertToTriMap, CUtlVector<Triangle_t>& triangles )
{
	FieldIndex_t posField = pVertexData->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	FieldIndex_t normalField = pVertexData->FindFieldIndex( CDmeVertexData::FIELD_NORMAL );
	FieldIndex_t tangentField = pVertexData->FindFieldIndex( CDmeVertexData::FIELD_TANGENT );

	const CDmrArray<int> pPositionIndices = pVertexData->GetIndexData( posField );
	const CDmrArray<Vector> pPositionData = pVertexData->GetVertexData( posField );
	const CDmrArray<int> pNormalIndices = pVertexData->GetIndexData( normalField );
	const CDmrArray<Vector> pNormalData = pVertexData->GetVertexData( normalField );

	// calculate an average tangent space for each vertex.
	int nVertexCount = pVertexData->VertexCount();
	Vector4D finalSVect;
	for( int vertID = 0; vertID < nVertexCount; vertID++ )
	{
		CUtlVector<int> &triangleList = vertToTriMap[vertID];

		Vector sVect, tVect;
		sVect.Init( 0.0f, 0.0f, 0.0f );
		tVect.Init( 0.0f, 0.0f, 0.0f );
		int nTriangleCount = triangleList.Count();
		for ( int triID = 0; triID < nTriangleCount; triID++ )
		{
			Triangle_t &tri = triangles[ triangleList[triID] ];
			sVect += tri.m_vecTangentS;
			tVect += tri.m_vecTangentT;
		}

		// In the case of zbrush, everything needs to be treated as smooth.
		if ( bSmoothTangents )
		{
			const Vector &vertPos1 = pPositionData.Get( pPositionIndices.Get( vertID ) );
			for( int vertID2 = 0; vertID2 < nVertexCount; vertID2++ )
			{
				if ( vertID2 == vertID )
					continue;

				const Vector &vertPos2 = pPositionData.Get( pPositionIndices.Get( vertID2 ) );
				if ( vertPos1 != vertPos2 )
					continue;

				CUtlVector<int> &triangleList2 = vertToTriMap[vertID2];
				int nTriangleCount2 = triangleList2.Count();
				for ( int triID2 = 0; triID2 < nTriangleCount2; triID2++ )
				{
					Triangle_t &tri2 = triangles[ triangleList2[triID2] ];
					sVect += tri2.m_vecTangentS;
					tVect += tri2.m_vecTangentT;
				}
			}
		}

		// make an orthonormal system.
		// need to check if we are left or right handed.
		Vector tmpVect;
		CrossProduct( sVect, tVect, tmpVect );
		const Vector &normal = pNormalData.Get( pNormalIndices.Get( vertID ) );
		bool bLeftHanded = DotProduct( tmpVect, normal ) < 0.0f;
		if ( !bLeftHanded )
		{
			CrossProduct( normal, sVect, tVect );
			CrossProduct( tVect, normal, sVect );
			VectorNormalize( sVect );
			VectorNormalize( tVect );
			finalSVect[0] = sVect[0];
			finalSVect[1] = sVect[1];
			finalSVect[2] = sVect[2];
			finalSVect[3] = 1.0f;
		}
		else
		{
			CrossProduct( sVect, normal, tVect );
			CrossProduct( normal, tVect, sVect );
			VectorNormalize( sVect );
			VectorNormalize( tVect );
			finalSVect[0] = sVect[0];
			finalSVect[1] = sVect[1];
			finalSVect[2] = sVect[2];
			finalSVect[3] = -1.0f;
		}

		pVertexData->SetVertexData( tangentField, vertID, 1, AT_VECTOR4, &finalSVect );
		pVertexData->SetVertexIndices( tangentField, vertID, 1, &vertID );
	}
}


//-----------------------------------------------------------------------------
// Builds a map from vertex index to all triangles that use it
//-----------------------------------------------------------------------------
void CDmeMesh::BuildVertToTriMap( const CDmeVertexData *pVertexData, CUtlVector<Triangle_t> &triangles, CUtlVector< CUtlVector<int> > &vertToTriMap )
{
	vertToTriMap.AddMultipleToTail( pVertexData->VertexCount() );

	int nCount = FaceSetCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeFaceSet *pFaceSet = GetFaceSet( i );
		BuildTriangleMap( pVertexData, pFaceSet, triangles, &vertToTriMap );
	}
}


//-----------------------------------------------------------------------------
// Compute a default per-vertex tangent given normal data + uv data
//-----------------------------------------------------------------------------
void CDmeMesh::ComputeDefaultTangentData( CDmeVertexData *pVertexData, bool bSmoothTangents )
{
	if ( !pVertexData )
		return;

	// Need to have valid pos, uv, and normal to perform this operation
	FieldIndex_t posField = pVertexData->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	FieldIndex_t normalField = pVertexData->FindFieldIndex( CDmeVertexData::FIELD_NORMAL );
	FieldIndex_t uvField = pVertexData->FindFieldIndex( CDmeVertexData::FIELD_TEXCOORD );
	if ( posField < 0 || uvField < 0 || normalField < 0 )
		return;

	// FIXME: Need to do a pass to make sure no vertex is referenced by 
	// multiple facesets that have different materials in them.
	// In that case, we need to add extra copies of that vertex and modify
	// the face set data to refer to the new vertices

	// Build a map from vertex to a list of triangles that share the vert.
	CUtlVector<Triangle_t> triangles( 0, 1024 );
	CUtlVector< CUtlVector<int> > vertToTriMap;
	vertToTriMap.AddMultipleToTail( pVertexData->VertexCount() );

	int nCount = FaceSetCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeFaceSet *pFaceSet = GetFaceSet( i );
		BuildTriangleMap( pVertexData, pFaceSet, triangles, &vertToTriMap );
	}

	ComputeTriangleTangets( pVertexData, triangles );

	// FIXME: We could do a pass to determine the unique combinations of 
	// position + tangent indices in the vertex data. We only need to have
	// a unique tangent for each of these unique vertices. For simplicity
	// (and speed), I'll assume all tangents are unique per vertex.
	FieldIndex_t tangent = pVertexData->CreateField<Vector4D>( "tangents" );
	pVertexData->RemoveAllVertexData( tangent );
	pVertexData->AddVertexData( tangent, pVertexData->VertexCount() );

	ComputeAverageTangent( pVertexData, bSmoothTangents, vertToTriMap, triangles );
}


//-----------------------------------------------------------------------------
// Compute a default per-vertex tangent given normal data + uv data for all vertex data referenced by this mesh
//-----------------------------------------------------------------------------
void CDmeMesh::ComputeDefaultTangentData( bool bSmoothTangents )
{
	const int nBaseStateCount = m_BaseStates.Count();
	for ( int i = 0; i < nBaseStateCount; ++i )
	{
		if ( m_BaseStates[i] && m_BaseStates[i]->NeedsTangentData() )
		{
			ComputeDefaultTangentData( m_BaseStates[i], bSmoothTangents );
		}
	}
}


//-----------------------------------------------------------------------------
// Utility method to compute default tangent data on all meshes in the sub-dag hierarchy
//-----------------------------------------------------------------------------
void ComputeDefaultTangentData( CDmeDag *pDag, bool bSmoothTangents )
{
	if ( !pDag )
		return;

	CDmeMesh *pMesh = CastElement< CDmeMesh >( pDag->GetShape() );
	if ( pMesh )
	{
		pMesh->ComputeDefaultTangentData( bSmoothTangents );
	}

	int nChildCount = pDag->GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		ComputeDefaultTangentData( pDag->GetChild( i ), bSmoothTangents );
	}
}


//-----------------------------------------------------------------------------
// Compute the dimensionality of the delta state (how many inputs affect it)
//-----------------------------------------------------------------------------
int CDmeMesh::ComputeDeltaStateDimensionality( int nDeltaIndex )
{
	CDmeVertexDeltaData *pDeltaState = GetDeltaState( nDeltaIndex );
	const char *pDeltaStateName = pDeltaState->GetName();

	const char *pUnderBar = pDeltaStateName;
	int nDimensions = 0;
	while ( pUnderBar )
	{
		++nDimensions;
		pUnderBar = strchr( pUnderBar, '_' );
		if ( pUnderBar )
		{
			++pUnderBar;
		}
	}

	return nDimensions;
}


//-----------------------------------------------------------------------------
// Computes the aggregate position for all vertices after applying a set of delta states
//-----------------------------------------------------------------------------
void CDmeMesh::AddDelta( CDmeVertexData *pBaseState, Vector *pDeltaPosition, int nDeltaStateIndex, CDmeVertexData::StandardFields_t fieldId )
{
	CDmeVertexDeltaData *pDeltaState = GetDeltaState( nDeltaStateIndex );
	FieldIndex_t nFieldIndex = pDeltaState->FindFieldIndex( fieldId );
	if ( nFieldIndex < 0 )
		return;

	if ( pBaseState->FindFieldIndex( CDmeVertexData::FIELD_BALANCE ) != -1 )
	{
		AddStereoVertexDelta<Vector>( pBaseState, pDeltaPosition, sizeof(Vector), fieldId, nDeltaStateIndex, true );
	}
	else
	{
		AddVertexDelta<Vector>( pBaseState, pDeltaPosition, sizeof(Vector), fieldId, nDeltaStateIndex, true );
	}
}


//-----------------------------------------------------------------------------
// Computes correctly averaged vertex normals from position data
//-----------------------------------------------------------------------------
void CDmeMesh::ComputeNormalsFromPositions(
	CDmeVertexData *pBase,
	const Vector *pPosition,
	const CUtlVector<Triangle_t> &triangles,
	int nNormalCount,
	Vector *pNormals )
{
	Assert( nNormalCount == pBase->GetNormalData().Count() );
	int *pNormalsAdded = (int*)_alloca( nNormalCount * sizeof(int) );
	memset( pNormalsAdded, 0, nNormalCount * sizeof(int) );
	memset( pNormals, 0, nNormalCount * sizeof(Vector) );

	const CUtlVector<int> &positionIndices = pBase->GetVertexIndexData( CDmeVertexData::FIELD_POSITION );
	const CUtlVector<int> &normalIndices = pBase->GetVertexIndexData( CDmeVertexData::FIELD_NORMAL );

	int nTriangleCount = triangles.Count();
	for ( int i = 0; i < nTriangleCount; ++i )
	{
		const Triangle_t &tri = triangles[i];
		int p1 = positionIndices[ tri.m_nIndex[0] ];
		int p2 = positionIndices[ tri.m_nIndex[1] ];
		int p3 = positionIndices[ tri.m_nIndex[2] ];

		int n1 = normalIndices[ tri.m_nIndex[0] ];
		int n2 = normalIndices[ tri.m_nIndex[1] ];
		int n3 = normalIndices[ tri.m_nIndex[2] ];

		Vector vecDelta, vecDelta2, vecNormal;
		VectorSubtract( pPosition[p2], pPosition[p1], vecDelta );
		VectorSubtract( pPosition[p3], pPosition[p1], vecDelta2 );
		CrossProduct( vecDelta, vecDelta2, vecNormal );
		VectorNormalize( vecNormal );

		pNormals[n1] += vecNormal;
		pNormals[n2] += vecNormal;
		pNormals[n3] += vecNormal;

		++pNormalsAdded[n1]; ++pNormalsAdded[n2]; ++pNormalsAdded[n3];
	}

	for ( int i = 0; i < nNormalCount; ++i )
	{
		if ( pNormalsAdded[i] > 0 )
		{
			pNormals[i] /= pNormalsAdded[i];
			VectorNormalize( pNormals[i] );
		}
		else
		{
			pNormals[i].Init( 0, 1, 0 );
		}
	}
}


//-----------------------------------------------------------------------------
// Converts pose-space normals into deltas appropriate for correction delta states
//-----------------------------------------------------------------------------
void CDmeMesh::ComputeCorrectedNormalsFromActualNormals( const CUtlVector<int> &deltaStateList, int nNormalCount, Vector *pNormals )
{
	CDmeVertexData *pBind = GetBindBaseState();
	if ( !pBind )
		return;

	Assert( nNormalCount == pBind->GetNormalData().Count() );

	// Subtract out all other normal contributions
	Vector *pUncorrectedNormals = (Vector*)_alloca( nNormalCount * sizeof(Vector) );
	memcpy( pUncorrectedNormals, pBind->GetNormalData().Base(), nNormalCount * sizeof( Vector ) );
	int nDeltaStateCount = deltaStateList.Count();
	for ( int i = 0; i < nDeltaStateCount; ++i )
	{
		AddDelta( pBind, pUncorrectedNormals, deltaStateList[i], CDmeVertexData::FIELD_NORMAL );
	}

	for ( int i = 0; i < nNormalCount; ++i )
	{
		pNormals[i] -= pUncorrectedNormals[i];
	}
}


//-----------------------------------------------------------------------------
// Copies the corrected normal data into a delta state
//-----------------------------------------------------------------------------
void CDmeMesh::SetDeltaNormalData( int nDeltaIndex, int nNormalCount, Vector *pNormals )
{
	// pNormals represents the correct normal delta state for this combination
	// Copy it into the delta state for this combination. 
	// Use tolerance to deal with precision errors introduced by the various computations
	CDmeVertexDeltaData *pDeltaState = GetDeltaState( nDeltaIndex );
	FieldIndex_t nNormalField = pDeltaState->FindFieldIndex( CDmeVertexDeltaData::FIELD_NORMAL );
	if ( nNormalField >= 0 )
	{
		pDeltaState->RemoveAllVertexData( nNormalField );
	}
	else
	{
		nNormalField = pDeltaState->CreateField( CDmeVertexDeltaData::FIELD_NORMAL );
	}

	for ( int i = 0; i < nNormalCount; ++i )
	{
		if ( pNormals[i].LengthSqr() < 1e-4 )
			continue;

		int nNormalIndex = pDeltaState->AddVertexData( nNormalField, 1 );
		pDeltaState->SetVertexData( nNormalField, nNormalIndex, 1, AT_VECTOR3, &pNormals[i] );
		pDeltaState->SetVertexIndices( nNormalField, nNormalIndex, 1, &i );
	}
}


//-----------------------------------------------------------------------------
// Discovers the atomic controls used by the various delta states 
//-----------------------------------------------------------------------------
static int DeltaStateUsageLessFunc( const int * lhs, const int * rhs )
{
	return *lhs - *rhs;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMesh::BuildAtomicControlLists( int nCount, DeltaComputation_t *pInfo, CUtlVector< CUtlVector< int > > &deltaStateUsage )
{
	CUtlVector< CUtlString > atomicControls;
	deltaStateUsage.SetCount( nCount );

	// Build a list of atomic controls
	int nCurrentDelta;
	for ( nCurrentDelta = 0; nCurrentDelta < nCount; ++nCurrentDelta ) 
	{
		if ( pInfo[nCurrentDelta].m_nDimensionality != 1 )
			break;
		int j = atomicControls.AddToTail( GetDeltaState( pInfo[nCurrentDelta].m_nDeltaIndex )->GetName() );
		deltaStateUsage[ nCurrentDelta ].AddToTail( j );
	}

	for ( ; nCurrentDelta < nCount; ++nCurrentDelta )
	{
		CDmeVertexDeltaData *pDeltaState = GetDeltaState( pInfo[nCurrentDelta].m_nDeltaIndex );
		int nLen = Q_strlen( pDeltaState->GetName() );
		char *pTempBuf = (char*)_alloca( nLen + 1 );
		memcpy( pTempBuf, pDeltaState->GetName(), nLen+1 );
		char *pNext;
		for ( char *pUnderBar = pTempBuf; pUnderBar; pUnderBar = pNext )
		{
			pNext = strchr( pUnderBar, '_' );
			if ( pNext )
			{
				*pNext = 0;
				++pNext;
			}

			// Find this name in the list of strings
			int j;
			int nControlCount = atomicControls.Count();
			for ( j = 0; j < nControlCount; ++j )
			{
				if ( !Q_stricmp( pUnderBar, atomicControls[j] ) )
					break;
			}
			if ( j == nControlCount )
			{
				j = atomicControls.AddToTail( pUnderBar );
			}
			deltaStateUsage[ nCurrentDelta ].AddToTail( j );
		}
		deltaStateUsage[ nCurrentDelta ].Sort( DeltaStateUsageLessFunc );
	}
}


//-----------------------------------------------------------------------------
// Construct list of all n-1 -> 1 dimensional delta states 
// that will be active when this delta state is active
//-----------------------------------------------------------------------------
void CDmeMesh::ComputeDependentDeltaStateList( CUtlVector< DeltaComputation_t > &compList )
{
	if ( compList.Count() == 0 )
	{
		ComputeDeltaStateComputationList( compList );
	}

	CUtlVector< CUtlVector< int > > deltaStateUsage;
	const int nCount( compList.Count() );
	BuildAtomicControlLists( nCount, compList.Base(), deltaStateUsage );

	// Now build up a list of dependent delta states based on usage
	// NOTE: Usage is sorted in ascending order.
	for ( int i = 1; i < nCount; ++i )
	{
		int nUsageCount1 = deltaStateUsage[i].Count();
		for ( int j = 0; j < i; ++j )
		{
			// At the point they have the same dimensionality, no more need to check
			if ( compList[j].m_nDimensionality == compList[i].m_nDimensionality )
				break;

			int ii = 0;
			bool bSubsetFound = true;
			int nUsageCount2 = deltaStateUsage[j].Count();
			for ( int ji = 0; ji < nUsageCount2; ++ji )
			{
				for ( bSubsetFound = false; ii < nUsageCount1; ++ii )
				{
					if ( deltaStateUsage[j][ji] == deltaStateUsage[i][ii] )
					{
						++ii;
						bSubsetFound = true;
						break;
					}

					if ( deltaStateUsage[j][ji] < deltaStateUsage[i][ii] )
						break;
				}

				if ( !bSubsetFound )
					break;
			}

			if ( bSubsetFound )
			{
				compList[i].m_DependentDeltas.AddToTail( compList[j].m_nDeltaIndex );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Sorts DeltaComputation_t's by dimensionality
//-----------------------------------------------------------------------------
int CDmeMesh::DeltaStateLessFunc( const void * lhs, const void * rhs )
{
	DeltaComputation_t &info1 = *(DeltaComputation_t*)lhs;
	DeltaComputation_t &info2 = *(DeltaComputation_t*)rhs;
	return info1.m_nDimensionality - info2.m_nDimensionality;
}


//-----------------------------------------------------------------------------
// Generates a sorted list in order of dimensionality of the delta states
// NOTE: This assumes a naming scheme where delta state names have _ that separate control names
//-----------------------------------------------------------------------------
void CDmeMesh::ComputeDeltaStateComputationList( CUtlVector< DeltaComputation_t > &compList )
{
	// Do all combinations in order of dimensionality, lowest dimension first
	const int nCount = DeltaStateCount();
	compList.EnsureCount( nCount );	// Resets the CUtlVector
	for ( int i = 0; i < nCount; ++i )
	{
		compList[i].m_nDeltaIndex = i;
		compList[i].m_nDimensionality = ComputeDeltaStateDimensionality( i );
	}
	qsort( compList.Base(), nCount, sizeof(DeltaComputation_t), DeltaStateLessFunc );
}


//-----------------------------------------------------------------------------
// Computes normal deltas for all delta states based on position deltas
// NOTE: This assumes a naming scheme where delta state names have _ that separate control names
//-----------------------------------------------------------------------------
void CDmeMesh::ComputeDeltaStateNormals()
{
	CDmeVertexData *pBind = GetBindBaseState();
	if ( !pBind )
		return;

	const FieldIndex_t nBindNormalIndex = pBind->CreateField( CDmeVertexData::FIELD_NORMAL );

	const CUtlVector< Vector > &basePosData = pBind->GetPositionData();
	const int nPosCount = basePosData.Count();

	// Build a map from vertex to a list of triangles that share the vert.
	CUtlVector< Triangle_t > triangles( 0, 1024 );
	CUtlVector< CUtlVector<int> > vertToTriMap;
	vertToTriMap.AddMultipleToTail( pBind->VertexCount() );

	const int nFaceSetCount = FaceSetCount();
	for ( int i = 0; i < nFaceSetCount; ++i )
	{
		CDmeFaceSet *pFaceSet = GetFaceSet( i );
		BuildTriangleMap( pBind, pFaceSet, triangles, &vertToTriMap );
	}

	// Temporary storage for normals
	Vector *pNormals = reinterpret_cast< Vector * >( alloca( nPosCount * sizeof( Vector ) ) );

	// Make all of the normals in the bind pose smooth
	{
		const CUtlVector< int > &basePosIndices = pBind->GetVertexIndexData( CDmeVertexData::FIELD_POSITION );

		pBind->SetVertexIndices( nBindNormalIndex, 0, basePosIndices.Count(), basePosIndices.Base() );
		pBind->RemoveAllVertexData( nBindNormalIndex );
		pBind->AddVertexData( nBindNormalIndex, nPosCount );

		ComputeNormalsFromPositions( pBind, basePosData.Base(), triangles, nPosCount, pNormals );
		pBind->SetVertexData( nBindNormalIndex, 0, nPosCount, AT_VECTOR3, pNormals );

		// Fix up the current state to have smooth normals if current is not bind
		CDmeVertexData *pCurrent = GetCurrentBaseState();
		if ( pCurrent != pBind )
		{
			const FieldIndex_t nCurrentNormalIndex = pCurrent->CreateField( CDmeVertexData::FIELD_NORMAL );
			pCurrent->SetVertexIndices( nCurrentNormalIndex, 0, basePosIndices.Count(), basePosIndices.Base() );
			pCurrent->RemoveAllVertexData( nCurrentNormalIndex );
			pCurrent->AddVertexData( nCurrentNormalIndex, nPosCount );

			const CUtlVector< Vector > &currPosData = pCurrent->GetPositionData();
			ComputeNormalsFromPositions( pCurrent, currPosData.Base(), triangles, nPosCount, pNormals );
			pCurrent->SetVertexData( nCurrentNormalIndex, 0, nPosCount, AT_VECTOR3, pNormals );
		}
	}

	// Temporary storage for the positions
	Vector *pPosData = reinterpret_cast< Vector * >( alloca( nPosCount * sizeof( Vector ) ) );

	// Compute the dependent delta state list like thing
	CUtlVector< DeltaComputation_t > computationOrder;
	ComputeDependentDeltaStateList( computationOrder );

	const int nDeltaStateCount = computationOrder.Count();
	for ( int i = 0; i < nDeltaStateCount; ++i )
	{
		const DeltaComputation_t &deltaComputation = computationOrder[ i ];

		memcpy( pPosData, basePosData.Base(), nPosCount * sizeof( Vector ) );

		const CUtlVector< int > &depDeltas = deltaComputation.m_DependentDeltas;
		const int nDepStateCount = depDeltas.Count();
		for ( int j = 0; j < nDepStateCount; ++j )
		{
			AddDelta( GetDeltaState( depDeltas[ j ] ), pPosData, nPosCount, CDmeVertexData::FIELD_POSITION );
		}

		AddDelta( GetDeltaState( deltaComputation.m_nDeltaIndex ), pPosData, nPosCount, CDmeVertexData::FIELD_POSITION );

		ComputeNormalsFromPositions( pBind, pPosData, triangles, nPosCount, pNormals );

		SetDeltaNormalDataFromActualNormals( computationOrder[ i ].m_nDeltaIndex, depDeltas, nPosCount, pNormals );
	}
}


//-----------------------------------------------------------------------------
// Computes normal deltas for all delta states based on position deltas
// NOTE: This assumes a naming scheme where delta state names have _ that separate control names
//-----------------------------------------------------------------------------
void CDmeMesh::SetDeltaNormalDataFromActualNormals( int nDeltaIndex, const CUtlVector<int> &deltaStateList, int nNormalCount, Vector *pNormals )
{
	// Store off the current state values
	CUtlVector< Vector2D > deltaStateWeights[MESH_DELTA_WEIGHT_TYPE_COUNT];
	for ( int i = 0; i < MESH_DELTA_WEIGHT_TYPE_COUNT; ++i )
	{
		deltaStateWeights[i] = m_DeltaStateWeights[i].Get();

		// Turn on the current weights to all be 1 to get max effect of morphs
		int nCount = m_DeltaStateWeights[i].Count();
		for ( int j = 0; j < nCount; ++j )
		{
			m_DeltaStateWeights[i].Set( j, Vector2D( 1.0f, 1.0f ) );
		}
	}

	ComputeCorrectedNormalsFromActualNormals( deltaStateList, nNormalCount, pNormals );

	// Finally, store the corrected normals into the delta state
	SetDeltaNormalData( nDeltaIndex, nNormalCount, pNormals );

	// Restore weights to their current value
	for ( int i = 0; i < MESH_DELTA_WEIGHT_TYPE_COUNT; ++i )
	{
		m_DeltaStateWeights[i] = deltaStateWeights[i];
	}
}


//-----------------------------------------------------------------------------
// A recursive algorithm to compute nCk, i.e. the number of order independent
// Combinations without any repeats of k items taking n at a time 
// The size of the returned array is:
//
//       n!
// -------------
// k! ( n - r )!
//
// e.g. 4C4 = { 0 1 2 3 }
// e.g. 3C4 = { 0 1 2 }, { 0 1 3 }, { 0 2 3 }, { 1 2 3 }
// e.g. 2C4 = { 0 1 }, { 0 2 }, { 0 3 }, { 1 2 }, { 1 3 }, { 2 3 }
// e.g. 1C4 = { 0 }, { 1 }, { 2 }, { 3 }
//
// It's recursive and meant to be called by the user with just n, k and combos
// the other default arguments are for the recursive steps
//-----------------------------------------------------------------------------
void CDmeMesh::Combinations(
	int n,
	int k,
	CUtlVector< CUtlVector< int > > &combos,
	int *pTmpArray,
	int start,
	int currentK )
{
	if ( !pTmpArray )
	{
		pTmpArray = reinterpret_cast< int * >( alloca( k * sizeof( int ) ) );
		memset( pTmpArray, 0, k * sizeof( int ) );
	}

	if ( currentK >= k )
	{
		combos[ combos.AddToTail() ].CopyArray( pTmpArray, k );
		return;
	}

	for ( int i( start ); i < n; ++i )
	{
		pTmpArray[ currentK ] = i;

		Combinations( n, k, combos, pTmpArray, i + 1, currentK + 1 );
	}
}


//-----------------------------------------------------------------------------
// Takes an incoming Delta state, splits it's name '_' and then finds the
// control delta (a state without a '_' in its name) and adds the index
// of that control delta to the referenced array
//
// Returns true if all of the control states exist, false otherwise
//-----------------------------------------------------------------------------
bool CDmeMesh::GetControlDeltaIndices(
	CDmeVertexDeltaData *pDeltaState,
	CUtlVector< int > &controlDeltaIndices ) const
{
	Assert( pDeltaState );
	return GetControlDeltaIndices( pDeltaState->GetName(), controlDeltaIndices );
}


//-----------------------------------------------------------------------------
// Same as above but just uses the name of a delta
//-----------------------------------------------------------------------------
bool CDmeMesh::GetControlDeltaIndices(
	const char *pDeltaStateName,
	CUtlVector< int > &controlDeltaIndices ) const
{
	Assert( pDeltaStateName );
	controlDeltaIndices.RemoveAll();

	const int nDeltaStateName( Q_strlen( pDeltaStateName ) );
	char *pTmpBuf( reinterpret_cast< char * >( alloca( nDeltaStateName + 1 ) ) );
	Q_strncpy( pTmpBuf, pDeltaStateName, nDeltaStateName + 1 );
	char *pNext;
	for ( char *pCurr = pTmpBuf; pCurr; pCurr = pNext )
	{
		pNext = strchr( pCurr, '_' );
		if ( pNext )
		{
			*pNext = '\0';
			++pNext;
		}

		if ( Q_strlen( pCurr ) )
		{
			const int controlDeltaIndex( FindDeltaStateIndex( pCurr ) );
			if ( controlDeltaIndex >= 0 )
			{
				controlDeltaIndices.AddToTail( controlDeltaIndex );
			}
			else
			{
				controlDeltaIndices.RemoveAll();
				return false;
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Builds a list of all of the underlying control delta indices for each
// delta state in the mesh
//
// e.g. Say the delta states are (in this order): A, B, C, A_C, A_B_C
//
// Will build: {
//               { 0 },
//               { 1 },
//               { 2 },
//               { 0, 2 },
//               { 0, 1, 2 }
//             }
// 
// Returns true if all of the control states exist, false otherwise
//-----------------------------------------------------------------------------
bool CDmeMesh::BuildCompleteDeltaStateControlList(
	CUtlVector< CUtlVector< int > > &deltaStateControlList ) const
{
	deltaStateControlList.RemoveAll();

	CUtlVector< int > tmpControlDeltaIndices;

	const int nDeltas( m_DeltaStates.Count() );
	for ( int i = 0; i < nDeltas; ++i )
	{
		if ( !GetControlDeltaIndices( m_DeltaStates[ i ], tmpControlDeltaIndices ) )
			return false;

		deltaStateControlList[ deltaStateControlList.AddToTail() ].CopyArray( tmpControlDeltaIndices.Base(), tmpControlDeltaIndices.Count() );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Searches controlList for a sub array that has exactly the same indices as
// controlIndices.  The order of the indices do not have to match but all of
// them must be present and no extras can be present.
// It assumes that the controlList is in the same order as m_deltaStates
//-----------------------------------------------------------------------------
int CDmeMesh::FindDeltaIndexFromControlIndices(
	const CUtlVector< int > &controlIndices,
	const CUtlVector< CUtlVector< int > > &controlList ) const
{
	const int nControlIndices( controlIndices.Count() );
	const int nControlList( controlList.Count() );

	int nControlListIndices;
	int foundCount;

	for ( int i = 0; i < nControlList; ++i )
	{
		const CUtlVector< int > &controlListIndices( controlList[ i ] );
		nControlListIndices = controlListIndices.Count();
		if ( nControlListIndices == nControlIndices )
		{
			foundCount = 0;

			for ( int j( 0 ); j < nControlListIndices; ++j )
			{
				for ( int k( 0 ); k < nControlIndices; ++k )
				{
					if ( controlListIndices[ j ] == controlIndices[ k ] )
					{
						++foundCount;
						break;
					}
				}
			}

			if ( foundCount == nControlIndices )
				return i;
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Builds a list of all of the required underlying deltas that make up this
// state whether that do not exist.  All of the control deltas must exist
// though (Deltas without '_' in their name).
//
// e.g. Say only Delta states A, B, C, D, A_B_C_D exist and A_B_C_D is
//      passed in.  This function will return:
//
//		A_B_C, A_B_D, A_C_D, B_C_D, A_B, A_C, A_D, B_C, B_D, C_D
//
// Returns true if all of the control states exist, false otherwise
//-----------------------------------------------------------------------------
bool CDmeMesh::BuildMissingDependentDeltaList(
	CDmeVertexDeltaData *pDeltaState,
	CUtlVector< int > &controlIndices,
	CUtlVector< CUtlVector< int > > &dependentStates ) const
{
	dependentStates.RemoveAll();

	CUtlVector< CUtlVector< int > > deltaStateControlList;
	BuildCompleteDeltaStateControlList( deltaStateControlList );

	if ( !GetControlDeltaIndices( pDeltaState, controlIndices ) )
		return false;

	const int nControlIndices( controlIndices.Count() );

	CUtlVector< int > comboControls;

	for ( int i( nControlIndices - 1 ); i > 0; --i )
	{
		CUtlVector< CUtlVector< int > > combos;
		Combinations( nControlIndices, i, combos );
		const int nCombos( combos.Count() );
		for ( int j( 0 ); j < nCombos; ++j )
		{
			const CUtlVector< int > &comboIndices( combos[ j ] );
			const int nComboIndices( comboIndices.Count() );
			if ( comboIndices.Count() )
			{
				comboControls.RemoveAll();
				comboControls.EnsureCapacity( nComboIndices );

				for ( int k( 0 ); k < nComboIndices; ++k )
				{
					comboControls.AddToTail( controlIndices[ comboIndices[ k ] ] );
				}

				if ( FindDeltaIndexFromControlIndices( comboControls, deltaStateControlList) < 0 )
				{
					dependentStates[ dependentStates.AddToTail() ].CopyArray( comboControls.Base(), comboControls.Count() );
				}
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T_t >
int CDmeMesh::GenerateCompleteDataForDelta(
	const CDmeVertexDeltaData *pDelta,
	T_t *pFullData,
	int nFullData,
	CDmeVertexData::StandardFields_t standardField )
{
	memset( pFullData, 0, nFullData * sizeof( T_t ) );

	const FieldIndex_t fIndex( pDelta->FindFieldIndex( standardField ) );
	if ( fIndex >= 0 )
	{
		CDmrArrayConst< T_t > fDataArray( pDelta->GetVertexData( fIndex ) );
		const CUtlVector< T_t > &fData( fDataArray.Get() );
		const CUtlVector< int > &fIndexData( pDelta->GetVertexIndexData( fIndex ) );
		const int nIndexData( fIndexData.Count() );

		Assert( nIndexData <= nFullData );

		int index;

		int i( 0 );

		for ( int j( 0 ); j < nIndexData; ++j )
		{
			index = fIndexData[ j ];
			while ( index > i )
			{
				++i;
			}

			Assert( i < nFullData );
			pFullData[ i ] = fData[ j ];
		}

		return nIndexData;
	}

	return 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T_t >
void CDmeMesh::AddDelta(
	const CDmeVertexDeltaData *pDelta,
	T_t *pFullData,
	int nFullData,
	FieldIndex_t fieldIndex,
	float weight,
	const CDmeSingleIndexedComponent *pMask )
{
	if ( fieldIndex >= 0 )
	{
		CDmrArrayConst< T_t > fDataArray( pDelta->GetVertexData( fieldIndex ) );
		const CUtlVector< T_t > &fData( fDataArray.Get() );
		const CUtlVector< int > &fIndexData( pDelta->GetVertexIndexData( fieldIndex ) );
		const int nIndexData( fIndexData.Count() );

		T_t t;

		Assert( nIndexData <= nFullData );

		int index;

		int i( 0 );

		if ( pMask )
		{
			float cWeight;

			for ( int j( 0 ); j < nIndexData; ++j )
			{
				index = fIndexData[ j ];

				if ( !pMask->GetWeight( index, cWeight ) )
					continue;

				while ( index > i )
				{
					++i;
				}

				Assert( i < nFullData );

				t = fData[ j ];
				t *= ( weight * cWeight );
				pFullData[ i ] += t;
			}
		}
		else
		{
			for ( int j( 0 ); j < nIndexData; ++j )
			{
				index = fIndexData[ j ];
				while ( index > i )
				{
					++i;
				}

				Assert( i < nFullData );
				t = fData[ j ];
				t *= weight;
				pFullData[ i ] += t;
			}
		}
	}
}

template void CDmeMesh::AddDelta< float >( const CDmeVertexDeltaData *, float *, int, FieldIndex_t, float, const CDmeSingleIndexedComponent * );
template void CDmeMesh::AddDelta< Vector2D >( const CDmeVertexDeltaData *, Vector2D *, int, FieldIndex_t, float, const CDmeSingleIndexedComponent * );
template void CDmeMesh::AddDelta< Vector >( const CDmeVertexDeltaData *, Vector *, int, FieldIndex_t, float, const CDmeSingleIndexedComponent * );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T_t >
void CDmeMesh::AddDelta(
	const CDmeVertexDeltaData *pDelta,
	T_t *pFullData,
	int nFullData,
	CDmeVertexData::StandardFields_t standardField,
	float weight,
	const CDmeSingleIndexedComponent *pMask )
{
	const FieldIndex_t fIndex( pDelta->FindFieldIndex( standardField ) );
	AddDelta( pDelta, pFullData, nFullData, fIndex, weight, pMask );
}

template void CDmeMesh::AddDelta< float >( const CDmeVertexDeltaData *, float *, int, CDmeVertexData::StandardFields_t, float, const CDmeSingleIndexedComponent * );
template void CDmeMesh::AddDelta< Vector2D >( const CDmeVertexDeltaData *, Vector2D *, int, CDmeVertexData::StandardFields_t, float, const CDmeSingleIndexedComponent * );
template void CDmeMesh::AddDelta< Vector >( const CDmeVertexDeltaData *, Vector *, int, CDmeVertexData::StandardFields_t, float, const CDmeSingleIndexedComponent * );

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMesh::ComputeAllCorrectedPositionsFromActualPositions()
{
	const CDmeVertexData *pBase = GetBindBaseState();
	if ( !pBase )
		return;

	CUtlVector< DeltaComputation_t > deltaList;
	ComputeDependentDeltaStateList( deltaList );

	const int nDeltas( deltaList.Count() );

	const int nPositions( pBase->GetPositionData().Count() );

	Vector *pPositions( reinterpret_cast< Vector * >( alloca( nPositions * sizeof( Vector ) ) ) );
	int *pIndices( reinterpret_cast< int * >( alloca( nPositions * sizeof( int ) ) ) );

	int pCount;

	for ( int i = 0; i < nDeltas; ++i )
	{
		const DeltaComputation_t &deltaComputation( deltaList[ i ] );
		CDmeVertexDeltaData *pDelta( m_DeltaStates[ deltaComputation.m_nDeltaIndex ] );
		if ( !pDelta->GetValue< bool >( "corrected" ) )
		{
			const FieldIndex_t pIndex( pDelta->FindFieldIndex( CDmeVertexDeltaData::FIELD_POSITION ) );
			if ( pIndex < 0 )
				continue;

			GenerateCompleteDataForDelta( pDelta, pPositions, nPositions, CDmeVertexData::FIELD_POSITION );

			const CUtlVector< int > &dependentDeltas( deltaComputation.m_DependentDeltas );
			const int nDependentDeltas( dependentDeltas.Count() );
			for ( int j( 0 ); j < nDependentDeltas; ++j )
			{
				const CDmeVertexDeltaData *pDependentDelta( m_DeltaStates[ dependentDeltas[ j ] ] );
				const CUtlVector< Vector > &dPositions( pDependentDelta->GetPositionData() );
				const CUtlVector<int> &dIndices( pDependentDelta->GetVertexIndexData( CDmeVertexData::FIELD_POSITION ) );
				Assert( dPositions.Count() == dIndices.Count() );
				const int nIndices( dIndices.Count() );

				int index;

				int k( 0 );
				for ( int l( 0 ); l < nIndices; ++l )
				{
					index = dIndices[ l ];
					while ( index > k )
					{
						++k;
					}

					Assert( k < nPositions );
					pPositions[ k ] -= dPositions[ l ];
				}
			}

			pCount = 0;
			for ( int j( 0 ); j < nPositions; ++j )
			{
				const Vector &v( pPositions[ j ] );
				// Kind of a magic number but it's because of 16 bit compression of the delta values
				if ( fabs( v.x ) >= ( 1 / 4096.0f ) || fabs( v.y ) >= ( 1 / 4096.0f ) || fabs( v.z ) >= ( 1 / 4096.0f ) )
				{
					pPositions[ pCount ] = v;
					pIndices[ pCount ] = j;
					++pCount;
				}
			}

			pDelta->RemoveAllVertexData( pIndex );

			if ( pCount )
			{
				pDelta->AddVertexData( pIndex, pCount );
				pDelta->SetVertexData( pIndex, 0, pCount, AT_VECTOR3, pPositions );
				pDelta->SetVertexIndices( pIndex, 0, pCount, pIndices );
			}
			pDelta->SetValue( "corrected", true );
		}
	}
}


//-----------------------------------------------------------------------------
// There's no guarantee that fields are added in any order, nor that only
// standard fields exist...
//-----------------------------------------------------------------------------
template < class T_t >
void CDmeMesh::AddCorrectedDelta(
	CDmrArray< T_t > &baseDataArray,
	const CUtlVector< int > &baseIndices,
	const DeltaComputation_t &deltaComputation,
	const char *pFieldName,
	float weight,
	const CDmeSingleIndexedComponent *pMask )
{
	const CUtlVector< T_t > &baseData( baseDataArray.Get() );
	const int nData( baseData.Count() );
	T_t *pData( reinterpret_cast< T_t * >( alloca( nData * sizeof( T_t ) ) ) );
	Q_memcpy( pData, baseData.Base(), nData * sizeof( T_t ) );

	CDmeVertexDeltaData *pDelta( GetDeltaState( deltaComputation.m_nDeltaIndex ) );

	const int deltaFieldIndex( pDelta->FindFieldIndex( pFieldName ) );
	if ( deltaFieldIndex < 0 )
		return;

	AddDelta( pDelta, pData, nData, deltaFieldIndex, weight, pMask );

	const CUtlVector< int > &depDeltas( deltaComputation.m_DependentDeltas );
	const int nDepDeltas( depDeltas.Count() );
	for ( int j( 0 ); j < nDepDeltas; ++j )
	{
		pDelta = GetDeltaState( depDeltas[ j ] );

		int depFieldIndex = pDelta->FindFieldIndex( pFieldName );
		if ( depFieldIndex < 0 )
			continue;

		AddDelta( pDelta, pData, nData, depFieldIndex, weight, pMask );
	}

	baseDataArray.CopyArray( pData, nData );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T_t >
void CDmeMesh::AddCorrectedDelta(
	CUtlVector< T_t > &baseData,
	const CUtlVector< int > &baseIndices,
	const DeltaComputation_t &deltaComputation,
	const char *pFieldName,
	float weight,
	const CDmeSingleIndexedComponent *pMask )
{
	const int nData( baseData.Count() );

	CDmeVertexDeltaData *pDelta( GetDeltaState( deltaComputation.m_nDeltaIndex ) );

	const int deltaFieldIndex( pDelta->FindFieldIndex( pFieldName ) );
	if ( deltaFieldIndex < 0 )
		return;

	AddDelta( pDelta, baseData.Base(), nData, deltaFieldIndex, weight, pMask );

	const CUtlVector< int > &depDeltas( deltaComputation.m_DependentDeltas );
	const int nDepDeltas( depDeltas.Count() );
	for ( int j( 0 ); j < nDepDeltas; ++j )
	{
		pDelta = GetDeltaState( depDeltas[ j ] );

		int depFieldIndex = pDelta->FindFieldIndex( pFieldName );
		if ( depFieldIndex < 0 )
			continue;

		AddDelta( pDelta, baseData.Base(), nData, depFieldIndex, weight, pMask );
	}
}


//-----------------------------------------------------------------------------
// There's no guarantee that fields are added in any order, nor that only
// standard fields exist...
//-----------------------------------------------------------------------------
template < class T_t >
void CDmeMesh::AddRawDelta(
	CDmeVertexDeltaData *pDelta,
	CDmrArray< T_t > &baseDataArray,
	FieldIndex_t nDeltaFieldIndex,
	float weight,
	const CDmeSingleIndexedComponent *pMask )
{
	if ( !pDelta || nDeltaFieldIndex < 0 )
		return;

	const CUtlVector< T_t > &baseData( baseDataArray.Get() );
	const int nData( baseData.Count() );
	T_t *pData( reinterpret_cast< T_t * >( alloca( nData * sizeof( T_t ) ) ) );
	Q_memcpy( pData, baseData.Base(), nData * sizeof( T_t ) );

	AddDelta( pDelta, pData, nData, nDeltaFieldIndex, weight, pMask );

	baseDataArray.CopyArray( pData, nData );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T_t >
void CDmeMesh::AddRawDelta(
	CDmeVertexDeltaData *pDelta,
	CUtlVector< T_t > &baseData,
	FieldIndex_t nDeltaFieldIndex,
	float weight,
	const CDmeSingleIndexedComponent *pMask )
{
	if ( !pDelta || nDeltaFieldIndex < 0 )
		return;

	const int nData( baseData.Count() );

	AddDelta( pDelta, baseData.Base(), nData, nDeltaFieldIndex, weight, pMask );
}


//-----------------------------------------------------------------------------
// Sets the specified base state to the specified delta
// If no delta is specified then the current state is copied from the bind state
// If no base state is specified then the current base state is used
// The specified base state or the current base state cannot be the bind state
//-----------------------------------------------------------------------------
bool CDmeMesh::SetBaseStateToDelta( const CDmeVertexDeltaData *pDelta, CDmeVertexData *pPassedBase /* = NULL */ )
{
	CDmeVertexData *pBase = pPassedBase ? pPassedBase : GetCurrentBaseState();
	const CDmeVertexData *pBind = GetBindBaseState();

	if ( !pBase || !pBind || pBase == pBind )
		return false;

	pBind->CopyTo( pBase );

	if ( !pDelta )
		return true;

	// This should be cached and recomputed only when states are added
	CUtlVector< DeltaComputation_t > compList;
	ComputeDependentDeltaStateList( compList );

	const int nDeltas( compList.Count() );
	for ( int i = 0; i < nDeltas; ++i )
	{
		if ( pDelta != GetDeltaState( compList[ i ].m_nDeltaIndex ) )
			continue;

		const int nBaseField( pBase->FieldCount() );
		const int nDeltaField( pDelta->FieldCount() );

		for ( int j( 0 ); j < nBaseField; ++j )
		{
			const CUtlString &baseFieldName( pBase->FieldName( j ) );

			for ( int k( 0 ); k < nDeltaField; ++k )
			{
				const CUtlString &deltaFieldName( pDelta->FieldName( k ) );

				if ( baseFieldName != deltaFieldName )
					continue;

				const FieldIndex_t baseFieldIndex( pBase->FindFieldIndex( baseFieldName ) );
				const FieldIndex_t deltaFieldIndex( pDelta->FindFieldIndex( deltaFieldName ) );
				if ( baseFieldIndex < 0 || deltaFieldIndex < 0 )
					break;

				CDmAttribute *pBaseData( pBase->GetVertexData( baseFieldIndex ) );
				const CDmAttribute *pDeltaData( pDelta->GetVertexData( deltaFieldIndex ) );

				if ( pBaseData->GetType() != pDeltaData->GetType() )
					break;

				const CUtlVector< int > &baseIndices( pBase->GetVertexIndexData( baseFieldIndex ) );

				switch ( pBaseData->GetType() )
				{
				case AT_FLOAT_ARRAY:
					AddCorrectedDelta( CDmrArray< float >( pBaseData ), baseIndices, compList[ i ], baseFieldName );
					break;
				case AT_COLOR_ARRAY:
					AddCorrectedDelta( CDmrArray< Vector >( pBaseData ), baseIndices, compList[ i ], baseFieldName );
					break;
				case AT_VECTOR2_ARRAY:
					AddCorrectedDelta( CDmrArray< Vector2D >( pBaseData ), baseIndices, compList[ i ], baseFieldName );
					break;
				case AT_VECTOR3_ARRAY:
					AddCorrectedDelta( CDmrArray< Vector >( pBaseData ), baseIndices, compList[ i ], baseFieldName );
					break;
				default:
					break;
				}
				break;
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMesh::SelectVerticesFromDelta(
	CDmeVertexDeltaData *pDelta,
	CDmeSingleIndexedComponent *pSelection )
{
	if ( !pSelection )
		return;

	pSelection->Clear();

	if ( !pDelta )
		return;

	const FieldIndex_t pField( pDelta->FindFieldIndex( CDmeVertexData::FIELD_POSITION ) );
	if ( pField < 0 )
		return;

	const CUtlVector< int > &pIndicies( pDelta->GetVertexIndexData( CDmeVertexData::FIELD_POSITION ) );

	pSelection->AddComponents( pIndicies );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMesh::SelectAllVertices( CDmeSingleIndexedComponent *pSelection, CDmeVertexData *pPassedBase /* = NULL */ )
{
	const CDmeVertexData *pBase = pPassedBase ? pPassedBase : GetCurrentBaseState();

	if ( !pBase )
	{
		pBase = GetBindBaseState();
	}

	if ( !pBase )
		return;

	if ( !pSelection )
		return;

	pSelection->Clear();

	const FieldIndex_t pField( pBase->FindFieldIndex( CDmeVertexData::FIELD_POSITION ) );
	if ( pField < 0 )
		return;

	CUtlVector< int > indices;
	indices.EnsureCount( CDmrArrayConst< Vector >( pBase->GetVertexData( pField ) ).Count() );
	const int nIndices = indices.Count();
	for ( int i = 0; i < nIndices; ++i )
	{
		indices[ i ] = i;
	}

	pSelection->AddComponents( indices );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMesh::SelectHalfVertices( SelectHalfType_t selectHalfType, CDmeSingleIndexedComponent *pSelection, CDmeVertexData *pPassedBase /* = NULL */ )
{
	const CDmeVertexData *pBase = pPassedBase ? pPassedBase : GetCurrentBaseState();

	if ( !pBase )
	{
		pBase = GetBindBaseState();
	}

	if ( !pBase )
		return;

	if ( !pSelection )
		return;

	pSelection->Clear();

	const FieldIndex_t pField( pBase->FindFieldIndex( CDmeVertexData::FIELD_POSITION ) );
	if ( pField < 0 )
		return;

	const CDmrArrayConst< Vector > pos( pBase->GetVertexData( pField ) );
	const int nPosCount = pos.Count();

	CUtlVector< int > indices;
	indices.EnsureCapacity( nPosCount );

	if ( selectHalfType == kRight )
	{
		for ( int i = 0; i < nPosCount; ++i )
		{
			if ( pos[ i ].x <= 0.0f )
			{
				indices.AddToTail( i );
			}
		}
	}
	else
	{
		for ( int i = 0; i < nPosCount; ++i )
		{
			if ( pos[ i ].x >= 0.0f )
			{
				indices.AddToTail( i );
			}
		}
	}

	pSelection->AddComponents( indices );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeMesh::CreateDeltaFieldFromBaseField(
	CDmeVertexData::StandardFields_t nStandardFieldIndex,
	const CDmrArrayConst< float > &baseArray,
	const CDmrArrayConst< float > &bindArray,
	CDmeVertexDeltaData *pDelta )
{
	const int nData( baseArray.Count() );
	if ( nData != bindArray.Count() )
		return false;

	const float *pBaseData( baseArray.Get().Base() );
	const float *pBindData( bindArray.Get().Base() );

	float *pData( reinterpret_cast< float * >( nData * sizeof( float ) ) );
	Q_memcpy( pData, pBaseData, nData * sizeof( float ) );
	int *pIndices( reinterpret_cast< int * >( nData * sizeof( int ) ) );

	float v;

	int nDeltaCount( 0 );
	for ( int i = 0; i < nData; ++i )
	{
		v = pBaseData[ i ] - pBindData[ i ];

		// Kind of a magic number but it's because of 16 bit compression of the delta values
		if ( fabs( v ) >= ( 1 / 4096.0f ) )
		{
			pData[ nDeltaCount ] = v;
			pIndices[ nDeltaCount ] = i;
			++nDeltaCount;
		}
	}

	if ( nDeltaCount <= 0 )
		return true;

	FieldIndex_t fieldIndex( pDelta->CreateField( nStandardFieldIndex ) );
	if ( fieldIndex < 0 )
		return false;

	pDelta->AddVertexData( fieldIndex, nDeltaCount );
	pDelta->SetVertexData( fieldIndex, 0, nDeltaCount, AT_FLOAT, pData );
	pDelta->SetVertexIndices( fieldIndex, 0, nDeltaCount, pIndices );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeMesh::CreateDeltaFieldFromBaseField(
	CDmeVertexData::StandardFields_t nStandardFieldIndex,
	const CDmrArrayConst< Vector2D > &baseArray,
	const CDmrArrayConst< Vector2D > &bindArray,
	CDmeVertexDeltaData *pDelta )
{
	const int nData( baseArray.Count() );
	if ( nData != bindArray.Count() )
		return false;

	const Vector2D *pBaseData( baseArray.Get().Base() );
	const Vector2D *pBindData( bindArray.Get().Base() );

	Vector2D *pData( reinterpret_cast< Vector2D * >( nData * sizeof( Vector2D ) ) );
	Q_memcpy( pData, pBaseData, nData * sizeof( Vector2D ) );
	int *pIndices( reinterpret_cast< int * >( nData * sizeof( int ) ) );

	Vector2D v;

	int nDeltaCount( 0 );
	for ( int i = 0; i < nData; ++i )
	{
		v = pBaseData[ i ] - pBindData[ i ];

		// Kind of a magic number but it's because of 16 bit compression of the delta values
		if ( fabs( v.x ) >= ( 1 / 4096.0f ) || fabs( v.y ) >= ( 1 / 4096.0f ) )
		{
			pData[ nDeltaCount ] = v;
			pIndices[ nDeltaCount ] = i;
			++nDeltaCount;
		}
	}

	if ( nDeltaCount <= 0 )
		return true;

	FieldIndex_t fieldIndex( pDelta->CreateField( nStandardFieldIndex ) );
	if ( fieldIndex < 0 )
		return false;

	pDelta->AddVertexData( fieldIndex, nDeltaCount );
	pDelta->SetVertexData( fieldIndex, 0, nDeltaCount, AT_VECTOR2, pData );
	pDelta->SetVertexIndices( fieldIndex, 0, nDeltaCount, pIndices );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeMesh::CreateDeltaFieldFromBaseField(
	CDmeVertexData::StandardFields_t nStandardFieldIndex,
	const CDmrArrayConst< Vector > &baseArray,
	const CDmrArrayConst< Vector > &bindArray,
	CDmeVertexDeltaData *pDelta )
{
	const int nData( baseArray.Count() );
	if ( nData != bindArray.Count() )
		return false;

	const Vector *pBaseData( baseArray.Get().Base() );
	const Vector *pBindData( bindArray.Get().Base() );

	Vector *pData( reinterpret_cast< Vector * >( alloca( nData * sizeof( Vector ) ) ) );
	Q_memcpy( pData, pBaseData, nData * sizeof( Vector ) );
	int *pIndices( reinterpret_cast< int * >( alloca( nData * sizeof( int ) ) ) );

	Vector v;

	int nDeltaCount( 0 );
	for ( int i = 0; i < nData; ++i )
	{
		v = pBaseData[ i ] - pBindData[ i ];

		// Kind of a magic number but it's because of 16 bit compression of the delta values
		if ( fabs( v.x ) >= ( 1 / 4096.0f ) || fabs( v.y ) >= ( 1 / 4096.0f ) || fabs( v.z ) >= ( 1 / 4096.0f ) )
		{
			pData[ nDeltaCount ] = v;
			pIndices[ nDeltaCount ] = i;
			++nDeltaCount;
		}
	}

	if ( nDeltaCount <= 0 )
		return true;

	FieldIndex_t fieldIndex( pDelta->CreateField( nStandardFieldIndex ) );
	if ( fieldIndex < 0 )
		return false;

	pDelta->AddVertexData( fieldIndex, nDeltaCount );
	pDelta->SetVertexData( fieldIndex, 0, nDeltaCount, AT_VECTOR3, pData );
	pDelta->SetVertexIndices( fieldIndex, 0, nDeltaCount, pIndices );

	return true;
}

//-----------------------------------------------------------------------------
// Creates a delta from the difference between the bind base state and the
// specified base state.  If pBaseName is NULL the current base state is used
//-----------------------------------------------------------------------------
CDmeVertexDeltaData *CDmeMesh::ModifyOrCreateDeltaStateFromBaseState( const char *pDeltaName, CDmeVertexData *pPassedBase /* = NULL */, bool absolute /* = false */ )
{
	// Find All States Which Have This Guy 
	CDmeVertexData *pBase = pPassedBase ? pPassedBase : GetCurrentBaseState();
	if ( !pBase )
		return NULL;

	CDmeVertexData *pBind = GetBindBaseState();
	if ( !pBind )
		return NULL;

	// It's ok if pBase == pBind

	CUtlVector< int > superiorDeltaStates;
	ComputeSuperiorDeltaStateList( pDeltaName, superiorDeltaStates );
	const int nSuperior = superiorDeltaStates.Count();

	if ( nSuperior > 0 )
	{
		UniqueId_t id;
		char idBuf[ MAX_PATH ];

		CDmeVertexData *pTmpBaseState = NULL;
		do 
		{
			CreateUniqueId( &id );
			UniqueIdToString( id, idBuf, sizeof( idBuf ) );
			pTmpBaseState = FindBaseState( idBuf );
		} while( pTmpBaseState != NULL );

		pTmpBaseState = FindOrCreateBaseState( idBuf );
		if ( !pTmpBaseState )
			return NULL;

		for ( int i = 0; i < nSuperior; ++i )
		{
			Assert( superiorDeltaStates[ i ] < DeltaStateCount() );
			CDmeVertexDeltaData *pSuperiorDelta = GetDeltaState( superiorDeltaStates[ i ] );
			if ( pSuperiorDelta->GetValue< bool >( "corrected" ) )
			{
				// Only fiddle with states that are "corrected"
				if ( !SetBaseStateToDelta( pSuperiorDelta, pTmpBaseState ) )
					return NULL;

				if ( !ModifyOrCreateDeltaStateFromBaseState( CUtlString( pSuperiorDelta->GetName() ), pTmpBaseState, true ) )
					return NULL;
			}
		}

		DeleteBaseState( idBuf );
	}

	ResetDeltaState( pDeltaName );
	CDmeVertexDeltaData *pDelta = FindOrCreateDeltaState( pDeltaName );
	if ( !pDelta )
		return NULL;

	CDmeVertexData::StandardFields_t deltaFields[] =
	{
		CDmeVertexData::FIELD_POSITION,
		CDmeVertexData::FIELD_NORMAL,
		CDmeVertexData::FIELD_WRINKLE
	};

	for ( int i = 0; i < sizeof( deltaFields ) / sizeof( deltaFields[ 0 ] ); ++i )
	{
		CDmeVertexData::StandardFields_t standardFieldIndex( deltaFields[ i ] );
		const FieldIndex_t baseFieldIndex( pBase->FindFieldIndex( standardFieldIndex ) );
		const FieldIndex_t bindFieldIndex( pBind->FindFieldIndex( standardFieldIndex ) );

		if ( baseFieldIndex < 0 || bindFieldIndex < 0 )
			continue;

		CDmAttribute *pBaseData( pBase->GetVertexData( baseFieldIndex ) );
		CDmAttribute *pBindData( pBind->GetVertexData( bindFieldIndex ) );

		if ( pBaseData->GetType() != pBindData->GetType() )
			continue;

		switch ( pBaseData->GetType() )
		{
		case AT_FLOAT_ARRAY:
			CreateDeltaFieldFromBaseField( standardFieldIndex, CDmrArrayConst< float >( pBaseData ), CDmrArrayConst< float >( pBindData ), pDelta );
			break;
		case AT_COLOR_ARRAY:
			CreateDeltaFieldFromBaseField( standardFieldIndex, CDmrArrayConst< Vector >( pBaseData ), CDmrArrayConst< Vector >( pBindData ), pDelta );
			break;
		case AT_VECTOR2_ARRAY:
			CreateDeltaFieldFromBaseField( standardFieldIndex, CDmrArrayConst< Vector2D >( pBaseData ), CDmrArrayConst< Vector2D >( pBindData ), pDelta );
			break;
		case AT_VECTOR3_ARRAY:
			CreateDeltaFieldFromBaseField( standardFieldIndex, CDmrArrayConst< Vector >( pBaseData ), CDmrArrayConst< Vector >( pBindData ), pDelta );
			break;
		default:
			break;
		}
	}

	if ( !strchr( pDelta->GetName(), '_' ) )
	{
		const static UtlSymId_t symTargets = g_pDataModel->GetSymbol( "targets" );
		CDmeCombinationOperator *pCombo( FindReferringElement< CDmeCombinationOperator >( this, symTargets ) );
		if ( pCombo )
		{
			pCombo->FindOrCreateControl( pDelta->GetName(), false, true );
		}
	}

	if ( !absolute )
	{
		ComputeAllCorrectedPositionsFromActualPositions();
	}

	return pDelta;
}


//-----------------------------------------------------------------------------
// TODO: Uncorrect all superior states and then correct them afterwards
//-----------------------------------------------------------------------------
bool CDmeMesh::DeleteDeltaState( const char *pDeltaName )
{
	const int nDeltaIndex = FindDeltaStateIndex( pDeltaName );
	if ( nDeltaIndex < 0 )
		return false;

	Assert( m_DeltaStates.Count() == m_DeltaStateWeights[ MESH_DELTA_WEIGHT_NORMAL ].Count() );
	Assert( m_DeltaStates.Count() == m_DeltaStateWeights[ MESH_DELTA_WEIGHT_LAGGED ].Count() );
	CDmeVertexDeltaData *pDelta( m_DeltaStates[ nDeltaIndex ] );
	if ( !pDelta )
		return false;

	m_DeltaStates.Remove( nDeltaIndex );
	m_DeltaStateWeights[ MESH_DELTA_WEIGHT_NORMAL ].Remove( nDeltaIndex );
	m_DeltaStateWeights[ MESH_DELTA_WEIGHT_LAGGED ].Remove( nDeltaIndex );
	g_pDataModel->DestroyElement( pDelta->GetHandle() );

	const static UtlSymId_t symTargets = g_pDataModel->GetSymbol( "targets" );
	CDmeCombinationOperator *pCombo( FindReferringElement< CDmeCombinationOperator >( this, symTargets ) );
	if ( pCombo )
	{
		pCombo->Purge();
	}

	return true;
}


//-----------------------------------------------------------------------------
// TODO: Uncorrect all superior states and then correct them afterwards
//-----------------------------------------------------------------------------
bool CDmeMesh::ResetDeltaState( const char *pDeltaName )
{
	const int nDeltaIndex = FindDeltaStateIndex( pDeltaName );
	if ( nDeltaIndex < 0 )
		return false;

	CDmeVertexDeltaData *pOldDelta = m_DeltaStates[ nDeltaIndex ];
	CDmeVertexDeltaData *pNewDelta = CreateElement< CDmeVertexDeltaData >( pOldDelta->GetName(), GetFileId() );
	if ( !pNewDelta )
		return false;

	m_DeltaStates.Set( nDeltaIndex, pNewDelta );
	g_pDataModel->DestroyElement( pOldDelta->GetHandle() );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

class CSelectionHelper
{
public:
	class CVert
	{
	public:
		int m_index;
		int m_count;
		float m_weight;
	};

	void AddVert( int vIndex, float weight = 1.0f );

	int AddToSelection( CDmeSingleIndexedComponent *pSelection ) const;

	int RemoveFromSelection( CDmeSingleIndexedComponent *pSelection, bool bAllowEmpty ) const;

protected:
	CUtlVector< CVert > m_verts;

	int BinarySearch( int component ) const;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSelectionHelper::AddVert( int vIndex, float weight /* = 1.0f */ )
{
	// Find the vertex, add it if necessary
	const int index = BinarySearch( vIndex );

	if ( index == m_verts.Count() )
	{
		// New Add to end
		CVert &v( m_verts[ m_verts.AddToTail() ] );
		v.m_index = vIndex;
		v.m_count = 1;
		v.m_weight = weight;
	}
	else if ( vIndex == m_verts[ index ].m_index )
	{
		// Existing, increment
		CVert &v( m_verts[ index ] );
		Assert( v.m_index == vIndex );
		v.m_count += 1;
		v.m_weight += weight;
	}
	else
	{
		// New insert before index
		CVert &v( m_verts[ m_verts.InsertBefore( index ) ] );
		v.m_index = vIndex;
		v.m_count = 1;
		v.m_weight = weight;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CSelectionHelper::AddToSelection( CDmeSingleIndexedComponent *pSelection ) const
{
	const int nVerts = m_verts.Count();

	for ( int i = 0; i < nVerts; ++i )
	{
		const CVert &v( m_verts[ i ] );
		Assert( !pSelection->HasComponent( v.m_index ) );
		pSelection->AddComponent( v.m_index, v.m_weight / static_cast< float >( v.m_count ) );
	}

	return nVerts;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CSelectionHelper::RemoveFromSelection( CDmeSingleIndexedComponent *pSelection, bool bAllowEmpty ) const
{
	const int nVerts = m_verts.Count();
	int nVertsRemovedCount = 0;

	for ( int i = 0; i < nVerts; ++i )
	{
		const CVert &v( m_verts[ i ] );
		if ( bAllowEmpty || pSelection->Count() > 1 )
		{
			pSelection->RemoveComponent( v.m_index );
			++nVertsRemovedCount;
		}
	}

	return nVertsRemovedCount;
}


//-----------------------------------------------------------------------------
// Searches for the component in the sorted component list and returns the
// index if it's found or if it's not found, returns the index at which it
// should be inserted to maintain the sorted order of the component list
//-----------------------------------------------------------------------------
int CSelectionHelper::BinarySearch( int vIndex ) const
{
	const int nVerts( m_verts.Count() );

	int left( 0 );
	int right( nVerts - 1 );
	int mid;

	while ( left <= right )
	{
		mid = ( left + right ) >> 1;	// floor( ( left + right ) / 2.0 )
		if ( vIndex > m_verts[ mid ].m_index )
		{
			left = mid + 1;
		}
		else if ( vIndex < m_verts[ mid ].m_index )
		{
			right = mid - 1;
		}
		else
		{
			return mid;
		}
	}

	return left;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMesh::GrowSelection( int nSize, CDmeSingleIndexedComponent *pSelection, CDmMeshComp *pPassedMeshComp )
{
	if ( nSize <= 0 || !pSelection )
		return;

	CUtlVector< int > sIndices;
	CUtlVector< float > sWeights;
	pSelection->GetComponents( sIndices, sWeights );
	const int nVertices = sIndices.Count();

	CDmMeshComp *pMeshComp = pPassedMeshComp ? pPassedMeshComp : new CDmMeshComp( this );

	CUtlVector< CDmMeshComp::CVert * > neighbours;

	CSelectionHelper sHelper;

	for ( int i = 0; i < nVertices; ++i )
	{
		const int nNeighbours = pMeshComp->FindNeighbouringVerts( sIndices[ i ], neighbours );
		for ( int j = 0; j < nNeighbours; ++j )
		{
			CDmMeshComp::CVert *pNeighbour = neighbours[ j ];
			Assert( pNeighbour );
			if ( pNeighbour )
			{
				const int vIndex = pNeighbour->PositionIndex();
				if ( !pSelection->HasComponent( vIndex ) )
				{
					sHelper.AddVert( vIndex, sWeights[ i ] );
				}
			}
		}
	}

	if ( sHelper.AddToSelection( pSelection ) > 0 )
	{
		GrowSelection( nSize - 1, pSelection, pMeshComp );
	}

	if ( pMeshComp != pPassedMeshComp )
	{
		delete pMeshComp;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMesh::ShrinkSelection( int nSize, CDmeSingleIndexedComponent *pSelection, CDmMeshComp *pPassedMeshComp )
{
	if ( nSize <= 0 || !pSelection )
		return;

	CUtlVector< int > sIndices;
	CUtlVector< float > sWeights;
	pSelection->GetComponents( sIndices, sWeights );
	const int nVertices = sIndices.Count();

	CDmMeshComp *pMeshComp = pPassedMeshComp ? pPassedMeshComp : new CDmMeshComp( this );

	CUtlVector< CDmMeshComp::CVert * > neighbours;

	CSelectionHelper sHelper;

	for ( int i = 0; i < nVertices; ++i )
	{
		bool hasSelectedNeighbour = false;
		bool hasUnselectedNeighbour = false;

		const int vIndex = sIndices[ i ];
		const int nNeighbours = pMeshComp->FindNeighbouringVerts( vIndex, neighbours );
		for ( int j = 0; j < nNeighbours; ++j )
		{
			const int nvIndex = neighbours[ j ]->PositionIndex();
			if ( pSelection->HasComponent( nvIndex ) )
			{
				hasSelectedNeighbour = true;
				if ( hasUnselectedNeighbour )
				{
					sHelper.AddVert( vIndex );
					break;
				}
			}
			else
			{
				hasUnselectedNeighbour = true;
				if ( hasSelectedNeighbour )
				{
					sHelper.AddVert( vIndex );
					break;
				}
			}
		}
	}

	if ( sHelper.RemoveFromSelection( pSelection, false ) > 0 )
	{
		ShrinkSelection( nSize - 1, pSelection, pMeshComp );
	}

	if ( pMeshComp != pPassedMeshComp )
	{
		delete pMeshComp;
	}
}


CDmeSingleIndexedComponent *CDmeMesh::FeatherSelection(
	float falloffDistance,
	Falloff_t falloffType,
	Distance_t distanceType,
	CDmeSingleIndexedComponent *pSelection,
	CDmMeshComp *pPassedMeshComp )
{
	switch ( falloffType )
	{
	case SMOOTH:
		return FeatherSelection< SMOOTH >( falloffDistance, distanceType, pSelection, pPassedMeshComp );
	case SPIKE:
		return FeatherSelection< SPIKE >( falloffDistance, distanceType, pSelection, pPassedMeshComp );
	case DOME:
		return FeatherSelection< DOME >( falloffDistance, distanceType, pSelection, pPassedMeshComp );
	default:
		return FeatherSelection< LINEAR >( falloffDistance, distanceType, pSelection, pPassedMeshComp );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < int T >
CDmeSingleIndexedComponent *CDmeMesh::FeatherSelection(
	float fDistance, Distance_t distanceType,
	CDmeSingleIndexedComponent *pSelection, CDmMeshComp *pPassedMeshComp )
{
	// TODO: Support feathering inward instead of just outward
	if ( fDistance <= 0.0f || !pSelection )
		return NULL;

	// Make a new CDmeSingleIndexedComponent to do all of the dirty work
	CDmeSingleIndexedComponent *pNewSelection = CreateElement< CDmeSingleIndexedComponent >( "feather", pSelection->GetFileId() );
	pSelection->CopyAttributesTo( pNewSelection );

	CDmMeshComp *pMeshComp = pPassedMeshComp ? pPassedMeshComp : new CDmMeshComp( this );
	CDmeVertexData *pBase = pMeshComp->BaseState();

	if ( distanceType == DIST_RELATIVE )
	{
		Vector vCenter;
		float flRadius;
		GetBoundingSphere( vCenter, flRadius, pBase, pSelection );
		fDistance *= flRadius;
	}

	const CUtlVector< Vector > &positions( pBase->GetPositionData() );
	const int nPositions = positions.Count();

	if ( !pBase )
		return NULL;

	CUtlVector< int > sIndices;

	int insideCount = 0;

	CFalloff< T > falloff;
	
	do
	{
		insideCount = 0;
		CUtlVector< CDmMeshComp::CVert * > neighbours;
		CSelectionHelper sHelper;

		pNewSelection->GetComponents( sIndices );
		int nVertices = sIndices.Count();

		for ( int i = 0; i < nVertices; ++i )
		{
			const int nNeighbours = pMeshComp->FindNeighbouringVerts( sIndices[ i ], neighbours );

			for ( int j = 0; j < nNeighbours; ++j )
			{
				const int vIndex = neighbours[ j ]->PositionIndex();

				if ( pNewSelection->HasComponent( vIndex ) )
					continue;

				const int closestVert = ClosestSelectedVertex( vIndex, pSelection, pBase );
				if ( closestVert < 0 || closestVert >= nPositions )
					continue;

				const float vDistance = positions[ vIndex ].DistTo( positions[ closestVert ] );
				if ( vDistance <= fDistance )
				{
					sHelper.AddVert( vIndex, falloff( vDistance / fDistance ) );
					++insideCount;
				}
			}
		}

		sHelper.AddToSelection( pNewSelection );

	} while ( insideCount > 0 );

	return pNewSelection;
}


//-----------------------------------------------------------------------------
// Add the specified delta, scaled by the weight value to the DmeVertexData
// base state specified.  Optionally the add can be masked by a specified
// weight map.
//
// If a DmeVertexData is not explicitly specified, the current state of the
// mesh is modified unless it's the bind state.  The bind state will never
// be modified even if it is explicitly specified.
//
// Only the delta specified is added.  No dependent states are added.
//-----------------------------------------------------------------------------
bool CDmeMesh::AddMaskedDelta(
	CDmeVertexDeltaData *pDelta,
	CDmeVertexData *pDst /* = NULL */,
	float weight /* = 1.0f */,
	const CDmeSingleIndexedComponent *pMask /* = NULL */ )
{
	CDmeVertexData *pBase = pDst ? pDst : GetCurrentBaseState();

	if ( !pBase || pBase == GetBindBaseState() )
		return false;

	bool retVal = true;

	const int nBaseField( pBase->FieldCount() );
	const int nDeltaField( pDelta->FieldCount() );

	// Try to add every field of the base state
	for ( int j( 0 ); j < nBaseField; ++j )
	{
		const CUtlString &baseFieldName( pBase->FieldName( j ) );

		// Find the corresponding field in the delta
		for ( int k( 0 ); k < nDeltaField; ++k )
		{
			const CUtlString &deltaFieldName( pDelta->FieldName( k ) );

			if ( baseFieldName != deltaFieldName )
				continue;

			const FieldIndex_t baseFieldIndex( pBase->FindFieldIndex( baseFieldName ) );
			const FieldIndex_t deltaFieldIndex( pDelta->FindFieldIndex( deltaFieldName ) );
			if ( baseFieldIndex < 0 || deltaFieldIndex < 0 )
				break;

			CDmAttribute *pBaseData( pBase->GetVertexData( baseFieldIndex ) );
			CDmAttribute *pDeltaData( pDelta->GetVertexData( deltaFieldIndex ) );

			if ( pBaseData->GetType() != pDeltaData->GetType() )
				break;

			switch ( pBaseData->GetType() )
			{
			case AT_FLOAT_ARRAY:
				AddRawDelta( pDelta, CDmrArray< float >( pBaseData ), baseFieldIndex, weight, pMask );
				break;
			case AT_COLOR_ARRAY:
				// TODO: Color is missing some algebraic operators
//				AddRawDelta( pDelta, CDmrArray< Color >( pBaseData ), baseFieldIndex, weight, pMask );
				break;
			case AT_VECTOR2_ARRAY:
				AddRawDelta( pDelta, CDmrArray< Vector2D >( pBaseData ), baseFieldIndex, weight, pMask );
				break;
			case AT_VECTOR3_ARRAY:
				AddRawDelta( pDelta, CDmrArray< Vector >( pBaseData ), baseFieldIndex, weight, pMask );
				break;
			default:
				break;
			}
			break;
		}
	}

	return retVal;
}


//-----------------------------------------------------------------------------
// Add the specified delta, scaled by the weight value to the DmeVertexData
// base state specified.  Optionally the add can be masked by a specified
// weight map.
//
// If a DmeVertexData is not explicitly specified, the current state of the
// mesh is modified unless it's the bind state.  The bind state will never
// be modified even if it is explicitly specified.
//
// Only the delta specified is added.  No dependent states are added.
//-----------------------------------------------------------------------------
bool CDmeMesh::AddCorrectedMaskedDelta(
	CDmeVertexDeltaData *pDelta,
	CDmeVertexData *pDst /* = NULL */,
	float weight /* = 1.0f */,
	const CDmeSingleIndexedComponent *pMask /* = NULL */ )
{
	CDmeVertexData *pBase = pDst ? pDst : GetCurrentBaseState();

	if ( !pBase || pBase == GetBindBaseState() )
		return false;

	bool retVal = true;

	const int nBaseField( pBase->FieldCount() );
	const int nDeltaField( pDelta->FieldCount() );

	// This should be cached and recomputed only when states are added
	CUtlVector< DeltaComputation_t > compList;
	ComputeDependentDeltaStateList( compList );

	const int nDeltas( compList.Count() );
	for ( int i = 0; i < nDeltas; ++i )
	{
		if ( pDelta != GetDeltaState( compList[ i ].m_nDeltaIndex ) )
			continue;

		// Try to add every field of the base state
		for ( int j( 0 ); j < nBaseField; ++j )
		{
			const CUtlString &baseFieldName( pBase->FieldName( j ) );

			// Find the corresponding field in the delta
			for ( int k( 0 ); k < nDeltaField; ++k )
			{
				const CUtlString &deltaFieldName( pDelta->FieldName( k ) );

				if ( baseFieldName != deltaFieldName )
					continue;

				const FieldIndex_t baseFieldIndex( pBase->FindFieldIndex( baseFieldName ) );
				const FieldIndex_t deltaFieldIndex( pDelta->FindFieldIndex( deltaFieldName ) );
				if ( baseFieldIndex < 0 || deltaFieldIndex < 0 )
					break;

				CDmAttribute *pBaseData( pBase->GetVertexData( baseFieldIndex ) );
				CDmAttribute *pDeltaData( pDelta->GetVertexData( deltaFieldIndex ) );

				if ( pBaseData->GetType() != pDeltaData->GetType() )
					break;

				const CUtlVector< int > &baseIndices( pBase->GetVertexIndexData( baseFieldIndex ) );

				switch ( pBaseData->GetType() )
				{
				case AT_FLOAT_ARRAY:
					AddCorrectedDelta( CDmrArray< float >( pBaseData ), baseIndices, compList[ i ], baseFieldName, weight, pMask );
					break;
				case AT_COLOR_ARRAY:
					AddCorrectedDelta( CDmrArray< Vector >( pBaseData ), baseIndices, compList[ i ], baseFieldName, weight, pMask );
					break;
				case AT_VECTOR2_ARRAY:
					AddCorrectedDelta( CDmrArray< Vector2D >( pBaseData ), baseIndices, compList[ i ], baseFieldName, weight, pMask );
					break;
				case AT_VECTOR3_ARRAY:
					AddCorrectedDelta( CDmrArray< Vector >( pBaseData ), baseIndices, compList[ i ], baseFieldName, weight, pMask );
					break;
				default:
					break;
				}
				break;
			}
		}
	}

	return retVal;
}


//-----------------------------------------------------------------------------
// Interpolates between two arrays of values and stores the result in a
// CDmrArray.
//
// result = ( ( 1 - weight ) * a ) + ( weight * b )
//
//-----------------------------------------------------------------------------
template< class T_t >
bool CDmeMesh::InterpMaskedData(
	CDmrArray< T_t > &aData,
	const CUtlVector< T_t > &bData,
	float weight,
	const CDmeSingleIndexedComponent *pMask ) const
{
	const int nDst = aData.Count();

	if ( bData.Count() != nDst )
		return false;

	// The wacky way of writing these expression is because Vector4D is missing operators
	// And this probably works better because of fewer temporaries

	T_t a;
	T_t b;

	if ( pMask )
	{
		// With a weight mask
		float vWeight;
		for ( int i = 0; i < nDst; ++i )
		{
			if ( pMask->GetWeight( i, vWeight ) )
			{
				vWeight *= weight;	// Specifically not clamping
				a = aData.Get( i );
				a *= ( 1.0f - vWeight );
				b = bData[ i ];
				b *= vWeight;
				b += a;
				aData.Set( i, b );
			}
		}
	}
	else
	{
		// Without a weight mask
		const float oneMinusWeight( 1.0f - weight );
		for ( int i = 0; i < nDst; ++i )
		{
			a = aData.Get( i );
			a *= oneMinusWeight;
			b = bData[ i ];
			b *= weight;
			b += a;
			aData.Set( i, b );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Interpolates between two CDmeVertexData's
//
// paData = ( ( 1 - weight ) * a ) + ( weight * b )
//-----------------------------------------------------------------------------
bool CDmeMesh::InterpMaskedData(
	CDmeVertexData *paData,
	const CDmeVertexData *pbData,
	float weight,
	const CDmeSingleIndexedComponent *pMask ) const
{
	if ( !paData || !pbData || paData == pbData )
		return false;

	const int naField = paData->FieldCount();
	const int nbField = pbData->FieldCount();

	for ( int i = 0; i < naField; ++i )
	{
		const CUtlString &aFieldName( paData->FieldName( i ) );

		for ( int j = 0; j < nbField; ++j )
		{
			const CUtlString &bFieldName( pbData->FieldName( j ) );
			if ( aFieldName != bFieldName )
				continue;

			const FieldIndex_t aFieldIndex( paData->FindFieldIndex( aFieldName ) );
			const FieldIndex_t bFieldIndex( pbData->FindFieldIndex( bFieldName ) );

			if ( aFieldIndex < 0 || bFieldIndex < 0 )
				break;

			CDmAttribute *paAttr( paData->GetVertexData( aFieldIndex ) );
			const CDmAttribute *pbAttr( pbData->GetVertexData( bFieldIndex ) );

			if ( paAttr->GetType() != pbAttr->GetType() )
				break;

			if ( paData->GetVertexIndexData( aFieldIndex ).Count() != pbData->GetVertexIndexData( bFieldIndex ).Count() )
				break;

			switch ( paAttr->GetType() )
			{
			case AT_FLOAT_ARRAY:
				InterpMaskedData( CDmrArray< float >( paAttr ), CDmrArrayConst< float >( pbAttr ).Get(), weight, pMask );
				break;
			case AT_COLOR_ARRAY:
				InterpMaskedData( CDmrArray< Vector4D >( paAttr ), CDmrArrayConst< Vector4D >( pbAttr ).Get(), weight, pMask );
				break;
			case AT_VECTOR2_ARRAY:
				InterpMaskedData( CDmrArray< Vector2D >( paAttr ), CDmrArrayConst< Vector2D >( pbAttr ).Get(), weight, pMask );
				break;
			case AT_VECTOR3_ARRAY:
				InterpMaskedData( CDmrArray< Vector >( paAttr ), CDmrArrayConst< Vector >( pbAttr ).Get(), weight, pMask );
				break;
			default:
				break;
			}
			break;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Interpolates between the specified VertexData and the specified Delta
// If pBase is NULL it will become the current state
// If pDelta is NULL then the state to interpolate to will be the bind state
//-----------------------------------------------------------------------------
bool CDmeMesh::InterpMaskedDelta(
	CDmeVertexDeltaData *pDelta,
	CDmeVertexData *pDst /* = NULL */,
	float weight /*= 1.0f */,
	const CDmeSingleIndexedComponent *pMask /*= NULL */ )
{
	CDmeVertexData *pDstBase = pDst ? pDst : GetCurrentBaseState();
	CDmeVertexData *pBind = GetBindBaseState();

	if ( !pDstBase || !pBind || pDstBase == pBind )
		return false;

	if ( pDelta == NULL )
	{
		// Interpolate between specified state and bind state
		return InterpMaskedData( pDstBase, pBind, weight, pMask );
	}

	// This should be cached and recomputed only when states are added
	CUtlVector< DeltaComputation_t > compList;
	ComputeDependentDeltaStateList( compList );

	bool retVal = false;

	const int nDeltas( compList.Count() );
	for ( int i = 0; i < nDeltas; ++i )
	{
		if ( pDelta != GetDeltaState( compList[ i ].m_nDeltaIndex ) )
			continue;

		retVal = true;

		const int nBaseField( pDstBase->FieldCount() );
		const int nBindField( pBind->FieldCount() );
		const int nDeltaField( pDelta->FieldCount() );

		CUtlVector< float > floatData;
		CUtlVector< Vector2D > vector2DData;
		CUtlVector< Vector > vectorData;
		CUtlVector< Vector4D > vector4DData;

		for ( int j( 0 ); j < nBaseField; ++j )
		{
			const CUtlString &baseFieldName( pDstBase->FieldName( j ) );

			for ( int k = 0; k < nBindField; ++k )
			{
				const CUtlString &bindFieldName( pBind->FieldName( k ) );
				if ( baseFieldName != bindFieldName )
					continue;

				for ( int l = 0; l < nDeltaField; ++l )
				{
					const CUtlString &deltaFieldName( pDelta->FieldName( l ) );
					if ( bindFieldName != deltaFieldName )
						continue;

					const FieldIndex_t baseFieldIndex( pDstBase->FindFieldIndex( baseFieldName ) );
					const FieldIndex_t bindFieldIndex( pBind->FindFieldIndex( bindFieldName ) );
					const FieldIndex_t deltaFieldIndex( pDelta->FindFieldIndex( deltaFieldName ) );

					if ( baseFieldIndex < 0 || bindFieldIndex < 0 || deltaFieldIndex < 0 )
						break;

					CDmAttribute *pDstBaseData( pDstBase->GetVertexData( baseFieldIndex ) );
					CDmAttribute *pBindData( pBind->GetVertexData( bindFieldIndex ) );
					CDmAttribute *pDeltaData( pDelta->GetVertexData( deltaFieldIndex ) );

					if ( pDstBaseData->GetType() != pBindData->GetType() || pBindData->GetType() != pDeltaData->GetType() )
						break;

					const CUtlVector< int > &bindIndices( pBind->GetVertexIndexData( bindFieldIndex ) );

					switch ( pDstBaseData->GetType() )
					{
					case AT_FLOAT_ARRAY:
						floatData = CDmrArrayConst< float >( pBindData ).Get();
						AddCorrectedDelta( floatData, bindIndices, compList[ i ], baseFieldName );
						InterpMaskedData( CDmrArray< float >( pDstBaseData ), floatData, weight, pMask );
						break;
					case AT_COLOR_ARRAY:
						vector4DData = CDmrArrayConst< Vector4D >( pBindData ).Get();
						AddCorrectedDelta( vector4DData, bindIndices, compList[ i ], baseFieldName );
						InterpMaskedData( CDmrArray< Vector4D >( pDstBaseData ), vector4DData, weight, pMask );
						break;
					case AT_VECTOR2_ARRAY:
						vector2DData = CDmrArrayConst< Vector2D >( pBindData ).Get();
						AddCorrectedDelta( vector2DData, bindIndices, compList[ i ], baseFieldName );
						InterpMaskedData( CDmrArray< Vector2D >( pDstBaseData ), vector2DData, weight, pMask );
						break;
					case AT_VECTOR3_ARRAY:
						vectorData = CDmrArrayConst< Vector >( pBindData ).Get();
						AddCorrectedDelta( vectorData, bindIndices, compList[ i ], baseFieldName );
						InterpMaskedData( CDmrArray< Vector >( pDstBaseData ), vectorData, weight, pMask );
						break;
					default:
						break;
					}
					break;
				}
			}
		}
	}

	return retVal;
}


//-----------------------------------------------------------------------------
// Returns the index of the closest selected vertex in the mesh to vIndex
// -1 on failure
//-----------------------------------------------------------------------------
int CDmeMesh::ClosestSelectedVertex( int vIndex, CDmeSingleIndexedComponent *pSelection, const CDmeVertexData *pPassedBase /* = NULL */ ) const
{
	const CDmeVertexData *pBase = pPassedBase ? pPassedBase : GetCurrentBaseState();
	if ( !pBase )
		return -1;

	const CUtlVector< Vector > &positions( pBase->GetPositionData() );

	if ( vIndex >= positions.Count() )
		return -1;

	const Vector &p( positions[ vIndex ] );

	CUtlVector< int > verts;
	pSelection->GetComponents( verts );
	const int nVerts = verts.Count();

	if ( nVerts <= 0 )
		return -1;

	float minSqDist = p.DistToSqr( positions[ verts[ 0 ] ] );
	float tmpSqDist;

	int retVal = verts[ 0 ];
	for ( int i = 1; i < nVerts; ++i )
	{
		tmpSqDist = p.DistToSqr( positions[ verts[ i ] ] );
		if ( tmpSqDist < minSqDist )
		{
			minSqDist = tmpSqDist;
			retVal = verts[ i ];
		}
	}

	return retVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float CDmeMesh::DistanceBetween( int vIndex0, int vIndex1, const CDmeVertexData *pPassedBase /*= NULL */ ) const
{
	const CDmeVertexData *pBase = pPassedBase ? pPassedBase : GetCurrentBaseState();
	if ( !pBase )
		return 0.0f;

	const CUtlVector< Vector > &positions( pBase->GetPositionData() );
	const int nPositions = positions.Count();

	if ( vIndex0 >= nPositions || vIndex1 >= nPositions )
		return 0.0f;

	return positions[ vIndex0 ].DistTo( positions[ vIndex1 ] );
}


//-----------------------------------------------------------------------------
// Sorts DeltaComputation_t's by dimensionality
//-----------------------------------------------------------------------------
int ControlIndexLessFunc( const void *lhs, const void *rhs )
{
	const int &lVal = *reinterpret_cast< const int * >( lhs );
	const int &rVal = *reinterpret_cast< const int * >( rhs );
	return lVal - rVal;
}

//-----------------------------------------------------------------------------
// This will compute a list of delta states that are superior to the passed
// delta state name (which has the form of <NAME>[_<NAME>]..., i.e. controls
// separated by underscores.  The states will be returned in order from
// most superior to least superior.  Since the deltas need to be broken down
// by the control deltas, if any control delta doesn't exist it will return false.
// 
// A superior delta state is defined as a delta which has this delta as
// a dependent (or inferior) delta.
//
// Given the network of:
//
// A, B, C
// A_B, A_C, B_C
// A_B_C
//
// A_B_C is superior to A, B, A_B, A_C & B_C
// A_B is superior to A, B & C
// A_C is superior to A, B & C
// B_C is superior to A, B & C
//
// Input	Output
// -------  --------------------
// A		A_B_C, A_B, A_C, B_C
// B		A_B_C, A_B, A_C, B_C
// C		A_B_C, A_B, A_C, B_C
// A_B		A_B_C
// A_C		A_B_C
// B_C		A_B_C
// A_B_C
//-----------------------------------------------------------------------------
bool CDmeMesh::ComputeSuperiorDeltaStateList( const char *pInferiorDeltaName, CUtlVector< int > &superiorDeltaStates )
{
	// TODO: Compute this data only when the deltas are added, removed or renamed
	CUtlVector< DeltaComputation_t > compList;
	ComputeDeltaStateComputationList( compList );

	// Typically the passed delta won't be in the list yet, but it could be, that's ok
	// Treat it like it isn't to be sure.
	CUtlVector< int > inferiorIndices;
	if ( !GetControlDeltaIndices( pInferiorDeltaName, inferiorIndices ) )
		return false;

	const int nInferiorIndices = inferiorIndices.Count();
	qsort( inferiorIndices.Base(), nInferiorIndices, sizeof( int ), ControlIndexLessFunc );

	CUtlVector< int > superiorIndices;
	int nSuperiorIndices;
	CDmeVertexDeltaData *pSuperiorDelta;

	for ( int i = compList.Count() - 1; i >= 0; --i )
	{
		const DeltaComputation_t &deltaComp = compList[ i ];

		// For a delta to be superior, it has to have more control inputs than the specified delta
		// compList is sorted in order of dimensionality, so safe to abort
		if ( nInferiorIndices >= deltaComp.m_nDimensionality )
			break;

		pSuperiorDelta = GetDeltaState( deltaComp.m_nDeltaIndex );
		if ( !pSuperiorDelta )
			continue;

		if ( !GetControlDeltaIndices( pSuperiorDelta, superiorIndices ) )
			continue;

		nSuperiorIndices = superiorIndices.Count();

		qsort( superiorIndices.Base(), nSuperiorIndices, sizeof( int ), ControlIndexLessFunc );

		int nFound = 0;
		int si = 0;
		for ( int ii = 0; ii < nInferiorIndices; ++ii )
		{
			const int &iIndex = inferiorIndices[ ii ];
			while ( si < nSuperiorIndices && iIndex != superiorIndices[ si ] )
			{
				++si;
			}

			if ( si < nSuperiorIndices )
			{
				++nFound;
			}
		}

		if ( nFound == nInferiorIndices )
		{
			superiorDeltaStates.AddToTail( deltaComp.m_nDeltaIndex );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Removes the passed base state from the list of base states in the mesh
// if it exists in the list of base states in the mesh, but doesn't delete
// the element itself
//-----------------------------------------------------------------------------
bool CDmeMesh::RemoveBaseState( CDmeVertexData *pBase )
{
	const int nBaseStates = m_BaseStates.Count();
	for ( int i = 0; i < nBaseStates; ++i )
	{
		if ( m_BaseStates[ i ] == pBase )
		{
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Adds an existing element to the list of base states of the mesh if it
// isn't already one of the base states
//-----------------------------------------------------------------------------
CDmeVertexData *CDmeMesh::FindOrAddBaseState( CDmeVertexData *pBase )
{
	const int nBaseStates = m_BaseStates.Count();
	for ( int i = 0; i < nBaseStates; ++i )
	{
		if ( m_BaseStates[ i ] == pBase )
		{
			return pBase;
		}
	}

	return m_BaseStates[ m_BaseStates.AddToTail( pBase ) ];
}


//-----------------------------------------------------------------------------
// TODO: Current state is insufficient as long as the current state isn't
//       created from the current delta weights
//-----------------------------------------------------------------------------
void CDmeMesh::GetBoundingSphere(
	Vector &c, float &r,
	CDmeVertexData *pPassedBase /* = NULL */, CDmeSingleIndexedComponent *pPassedSelection /* = NULL */ ) const
{ 
	c.Zero();
	r = 0.0f;

	const CDmeVertexData *pBase = pPassedBase ? pPassedBase : GetCurrentBaseState();
	if ( !pBase )
		return;

	const FieldIndex_t pIndex = pBase->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	if ( pIndex < 0 )
		return;

	const CUtlVector< Vector > &pData( pBase->GetPositionData() );
	const int nPositions = pData.Count();

	if ( pPassedSelection )
	{
		const int nSelectionCount = pPassedSelection->Count();
		int nIndex;
		float fWeight;
		for ( int i = 0; i < nSelectionCount; ++i )
		{
			pPassedSelection->GetComponent( i, nIndex, fWeight );
			c += pData[ nIndex ];
		}

		c /= static_cast< float >( nSelectionCount );

		float sqDist;
		for ( int i = 0; i < nSelectionCount; ++i )
		{
			for ( int iPos = 0; iPos < nPositions; ++iPos )
			{
				sqDist = c.DistToSqr( pData[ iPos ] );
				if ( sqDist > r )
				{
					r = sqDist;
				}
			}
		}
	}
	else
	{
		for ( int i = 0; i < nPositions; ++i )
		{
			c += pData[ i ];
		}

		c /= static_cast< float >( nPositions );

		float sqDist;
		for ( int i = 0; i < nPositions; ++i )
		{
			for ( int iPos = 0; iPos < nPositions; ++iPos )
			{
				sqDist = c.DistToSqr( pData[iPos] );
				if ( sqDist > r )
				{
					r = sqDist;
				}
			}
		}
	}

	r = sqrt( r );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMesh::GetBoundingBox( Vector &min, Vector &max, CDmeVertexData *pPassedBase /* = NULL */, CDmeSingleIndexedComponent *pPassedSelection /* = NULL */ ) const
{
	min.Zero();
	max.Zero();

	const CDmeVertexData *pBase = pPassedBase ? pPassedBase : GetCurrentBaseState();
	if ( !pBase )
		return;

	const FieldIndex_t pIndex = pBase->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	if ( pIndex < 0 )
		return;

	const CUtlVector< Vector > &pData( pBase->GetPositionData() );
	const int nPositions = pData.Count();

	if ( pPassedSelection )
	{
		const int nSelectionCount = pPassedSelection->Count();

		if ( nSelectionCount > 0 )
		{
			int nIndex;
			float fWeight;

			pPassedSelection->GetComponent( 0, nIndex, fWeight );
			min = pData[ nIndex ];
			max = min;

			for ( int i = 1; i < nSelectionCount; ++i )
			{
				pPassedSelection->GetComponent( i, nIndex, fWeight );

				const Vector &p = pData[ nIndex ];
				if ( p.x < min.x )
				{
					min.x = p.x;
				}
				else if ( p.x > max.x )
				{
					max.x = p.x;
				}

				if ( p.y < min.y )
				{
					min.y = p.y;
				}
				else if ( p.y > max.y )
				{
					max.y = p.y;
				}

				if ( p.z < min.z )
				{
					min.z = p.z;
				}
				else if ( p.z > max.z )
				{
					max.z = p.z;
				}
			}
		}
	}
	else
	{
		if ( nPositions > 0 )
		{
			min = pData[ 0 ];
			max = min;

			for ( int i = 1; i < nPositions; ++i )
			{
				const Vector &p = pData[ i ];
				if ( p.x < min.x )
				{
					min.x = p.x;
				}
				else if ( p.x > max.x )
				{
					max.x = p.x;
				}

				if ( p.y < min.y )
				{
					min.y = p.y;
				}
				else if ( p.y > max.y )
				{
					max.y = p.y;
				}

				if ( p.z < min.z )
				{
					min.z = p.z;
				}
				else if ( p.z > max.z )
				{
					max.z = p.z;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T_t >
bool CDmeMesh::SetBaseDataToDeltas(
	CDmeVertexData *pBase,
	CDmeVertexData::StandardFields_t nStandardField, CDmrArrayConst< T_t > &srcData, CDmrArray< T_t > &dstData, bool bDoStereo, bool bDoLag )
{
	const int nDataCount = dstData.Count();
	if ( srcData.Count() != nDataCount )
		return false;

	// Create the temp buffer for the data
	T_t *pData = reinterpret_cast< T_t * >( alloca( nDataCount * sizeof( T_t ) ) );

	// Copy the data from the src base state
	memcpy( pData, srcData.Base(), nDataCount * sizeof( T_t ) );

	const int nCount = m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL].Count();
	Assert( m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL].Count() == m_DeltaStateWeights[MESH_DELTA_WEIGHT_LAGGED].Count() );

	if ( bDoStereo )
	{
		for ( int i = 0; i < nCount; ++i )
		{
			float flLeftWeight = m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL][i].x;
			float flRightWeight = m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL][i].y;
			float flLeftWeightLagged = m_DeltaStateWeights[MESH_DELTA_WEIGHT_LAGGED][i].x;
			float flRightWeightLagged = m_DeltaStateWeights[MESH_DELTA_WEIGHT_LAGGED][i].y;
			if ( flLeftWeight <= 0.0f && flRightWeight <= 0.0f && ( !bDoLag || ( flLeftWeightLagged <= 0.0f && flRightWeightLagged <= 0.0f ) ) )
				continue;

			AddStereoVertexDelta< T_t >( pBase, pData, sizeof( T_t ), nStandardField, i, bDoLag );
		}
	}
	else
	{
		for ( int i = 0; i < nCount; ++i )
		{
			float flWeight = m_DeltaStateWeights[MESH_DELTA_WEIGHT_NORMAL][i].x;
			float flWeightLagged = m_DeltaStateWeights[MESH_DELTA_WEIGHT_LAGGED][i].x;
			if ( flWeight < 0.0f && ( !bDoLag || flWeightLagged <= 0.0f ) )
				continue;

			AddVertexDelta< T_t >( pBase, pData, sizeof( T_t ), nStandardField, i, bDoLag );
		}
	}

	dstData.SetMultiple( 0, nDataCount, pData );

	return true;
}


//-----------------------------------------------------------------------------
// Sets the specified based state to the version of the mesh specified by the
// current weighted deltas
// It's ok to modify the bind state... if you know what you're doing
//-----------------------------------------------------------------------------
bool CDmeMesh::SetBaseStateToDeltas( CDmeVertexData *pPassedBase /*= NULL */ )
{
	CDmeVertexData *pBind = GetBindBaseState();
	CDmeVertexData *pBase = pPassedBase ? pPassedBase : GetCurrentBaseState();

	if ( !pBind || !pBase )
		return false;

	CDmeVertexData::StandardFields_t deltaFields[] =
	{
		CDmeVertexData::FIELD_POSITION,
		CDmeVertexData::FIELD_NORMAL,
		CDmeVertexData::FIELD_WRINKLE
	};

	const bool bDoStereo = ( pBind->FindFieldIndex( CDmeVertexDeltaData::FIELD_BALANCE ) >= 0 );

	for ( int i = 0; i < sizeof( deltaFields ) / sizeof( deltaFields[ 0 ] ); ++i )
	{
		const CDmeVertexDeltaData::StandardFields_t nStandardField = deltaFields[ i ];
		const int nSrcField = pBind->FindFieldIndex( nStandardField );
		const int nDstField = pBase->FindFieldIndex( nStandardField );
		if ( nSrcField < 0 || nDstField < 0 )
			continue;

		const CDmAttribute *pSrcAttr = pBind->GetVertexData( nSrcField );
		CDmAttribute *pDstAttr = pBase->GetVertexData( nDstField );
		if ( !pSrcAttr || !pDstAttr || pSrcAttr->GetType() != pDstAttr->GetType() )
			continue;

		switch ( pDstAttr->GetType() )
		{
		case AT_FLOAT_ARRAY:
			SetBaseDataToDeltas( pBind, nStandardField, CDmrArrayConst< float >( pSrcAttr ), CDmrArray< float >( pDstAttr ), bDoStereo, false );
			break;
		case AT_VECTOR3_ARRAY:
			SetBaseDataToDeltas( pBind, nStandardField, CDmrArrayConst< Vector >( pSrcAttr ), CDmrArray< Vector >( pDstAttr ), bDoStereo, false );
			break;
		default:
			Assert( 0 );
			break;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Replace all instances of a material with a different material
//-----------------------------------------------------------------------------
void CDmeMesh::ReplaceMaterial( const char *pOldMaterialName, const char *pNewMaterialName )
{
	char pOldFixedName[MAX_PATH];
	char pNewFixedName[MAX_PATH];
	char pFixedName[MAX_PATH];
	if ( pOldMaterialName )
	{
		V_FixupPathName( pOldFixedName, sizeof(pOldFixedName), pOldMaterialName );
	}
	V_FixupPathName( pNewFixedName, sizeof(pNewFixedName), pNewMaterialName );
	V_FixSlashes( pNewFixedName, '/' );

	CDmeMaterial *pReplacementMaterial = NULL;

	int nCount = m_FaceSets.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeFaceSet *pFaceSet = m_FaceSets[i];
		CDmeMaterial *pMaterial = pFaceSet->GetMaterial();
		if ( pOldMaterialName )
		{
			const char *pMaterialName = pMaterial->GetMaterialName();
			V_FixupPathName( pFixedName, sizeof(pFixedName), pMaterialName );
			if ( Q_stricmp( pFixedName, pOldFixedName ) )
				continue;
		}

		if ( !pReplacementMaterial )
		{
			pReplacementMaterial = CreateElement< CDmeMaterial >( pMaterial->GetName(), pMaterial->GetFileId() );
			pReplacementMaterial->SetMaterial( pNewFixedName );
		}
		pFaceSet->SetMaterial( pReplacementMaterial );
	}
}


//-----------------------------------------------------------------------------
// Cleans up delta data that is referring to normals which have been merged out
//-----------------------------------------------------------------------------
static void CollapseRedundantDeltaNormals( CDmeVertexDeltaData *pDmeDelta, const CUtlVector< int > &normalMap )
{
	if ( !pDmeDelta )
		return;

	FieldIndex_t nNormalFieldIndex = pDmeDelta->FindFieldIndex( CDmeVertexData::FIELD_NORMAL );
	if ( nNormalFieldIndex < 0 )
		return;	// No normal deltas

	const CUtlVector< Vector > &oldNormalData = pDmeDelta->GetNormalData();
	const CUtlVector< int > &oldNormalIndices = pDmeDelta->GetVertexIndexData( nNormalFieldIndex );

	Assert( oldNormalData.Count() == oldNormalIndices.Count() );

	CUtlVector< bool > done;
	done.SetCount( normalMap.Count() );
	Q_memset( done.Base(), 0, done.Count() * sizeof( bool ) );

	CUtlVector< Vector > newNormalData;
	CUtlVector< int > newNormalIndices;

	for ( int i = 0; i < oldNormalIndices.Count(); ++i )
	{
		const int nNewIndex = normalMap[ oldNormalIndices[i] ];
		if ( nNewIndex < 0 || done[ nNewIndex ] )
			continue;

		done[ nNewIndex ] = true;
		newNormalData.AddToTail( oldNormalData[i] );
		newNormalIndices.AddToTail( nNewIndex );
	}

	pDmeDelta->RemoveAllVertexData( nNormalFieldIndex );
	nNormalFieldIndex = pDmeDelta->CreateField( CDmeVertexDeltaData::FIELD_NORMAL );
	pDmeDelta->AddVertexData( nNormalFieldIndex, newNormalData.Count() );
	pDmeDelta->SetVertexData( nNormalFieldIndex, 0, newNormalData.Count(), AT_VECTOR3, newNormalData.Base() );
	pDmeDelta->SetVertexIndices( nNormalFieldIndex, 0, newNormalIndices.Count(), newNormalIndices.Base() );
}


//-----------------------------------------------------------------------------
// Remove redundant normals from a DMX Mesh
// Looks at all of the normals around each position vertex and merges normals
// which are numerically similar (within flNormalBlend which by default in
// studiomdl is within 2 degrees) around that vertex
//
// If this would result in more normals being created, then don't do anything
// return false.
//-----------------------------------------------------------------------------
static bool CollapseRedundantBaseNormals( CDmeVertexData *pDmeVertexData, CUtlVector< int > &normalMap, float flNormalBlend )
{
	if ( !pDmeVertexData )
		return false;

	FieldIndex_t nPositionFieldIndex = pDmeVertexData->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	FieldIndex_t nNormalFieldIndex = pDmeVertexData->FindFieldIndex( CDmeVertexData::FIELD_NORMAL );
	if ( nPositionFieldIndex < 0 || nNormalFieldIndex < 0 )
		return false;

	const CUtlVector< Vector > &oldNormalData = pDmeVertexData->GetNormalData();
	const CUtlVector< int > &oldNormalIndices = pDmeVertexData->GetVertexIndexData( nNormalFieldIndex );

	CUtlVector< Vector > newNormalData;
	CUtlVector< int > newNormalIndices;

	newNormalIndices.SetCount( oldNormalIndices.Count() );
	for ( int i = 0; i < newNormalIndices.Count(); ++i )
	{
		newNormalIndices[i] = -1;
	}

	const int nPositionDataCount = pDmeVertexData->GetPositionData().Count();
	for ( int i = 0; i < nPositionDataCount; ++i )
	{
		int nNewNormalDataIndex = newNormalData.Count();

		const CUtlVector< int > &vertexIndices = pDmeVertexData->FindVertexIndicesFromDataIndex( CDmeVertexData::FIELD_POSITION, i );
		for ( int j = 0; j < vertexIndices.Count(); ++j )
		{
			bool bUnique = true;
			const int nVertexIndex = vertexIndices[j];
			const Vector &vNormal = oldNormalData[ oldNormalIndices[ vertexIndices[j] ] ];

			for ( int k = nNewNormalDataIndex; k < newNormalData.Count(); ++k )
			{
				if ( DotProduct( vNormal, newNormalData[k] ) > flNormalBlend )
				{
					newNormalIndices[ nVertexIndex ] = k;
					bUnique = false;
					break;
				}
			}

			if ( !bUnique )
				continue;

			newNormalIndices[ nVertexIndex ] = newNormalData.AddToTail( vNormal );
		}
	}

	for ( int i = 0; i < newNormalIndices.Count(); ++i )
	{
		if ( newNormalIndices[i] == -1 )
		{
			newNormalIndices[i] = newNormalData.AddToTail( oldNormalData[ oldNormalIndices[i] ] );
		}
	}

	// If it's the same or more don't do anything
	if ( newNormalData.Count() >= oldNormalData.Count() )
		return false;

	normalMap.SetCount( oldNormalData.Count() );
	for ( int i = 0; i < normalMap.Count(); ++i )
	{
		normalMap[i] = -1;
	}

	Assert( newNormalIndices.Count() == oldNormalIndices.Count() );
	for ( int i = 0; i < oldNormalIndices.Count(); ++i )
	{
		if ( normalMap[ oldNormalIndices[i] ] == -1 )
		{
			normalMap[ oldNormalIndices[i] ] = newNormalIndices[i];
		}
		else
		{
			Assert( normalMap[ oldNormalIndices[i] ] == newNormalIndices[i] );
		}
	}

	pDmeVertexData->RemoveAllVertexData( nNormalFieldIndex );
	nNormalFieldIndex = pDmeVertexData->CreateField( CDmeVertexDeltaData::FIELD_NORMAL );
	pDmeVertexData->AddVertexData( nNormalFieldIndex, newNormalData.Count() );
	pDmeVertexData->SetVertexData( nNormalFieldIndex, 0, newNormalData.Count(), AT_VECTOR3, newNormalData.Base() );
	pDmeVertexData->SetVertexIndices( nNormalFieldIndex, 0, newNormalIndices.Count(), newNormalIndices.Base() );

	return true;
}


//-----------------------------------------------------------------------------
// Collapse all normals with the same numerical value into the same normal
//-----------------------------------------------------------------------------
static bool CollapseRedundantBaseNormalsAggressive( CDmeVertexData *pDmeVertexData, float flNormalBlend )
{
	if ( !pDmeVertexData )
		return false;

	FieldIndex_t nNormalFieldIndex = pDmeVertexData->FindFieldIndex( CDmeVertexData::FIELD_NORMAL );
	if ( nNormalFieldIndex < 0 )
		return false;

	const CUtlVector< Vector > &oldNormalData = pDmeVertexData->GetNormalData();
	const CUtlVector< int > &oldNormalIndices = pDmeVertexData->GetVertexIndexData( nNormalFieldIndex );

	CUtlVector< int > normalMap;
	normalMap.SetCount( oldNormalData.Count() );

	CUtlVector< Vector > newNormalData;

	for ( int i = 0; i < oldNormalData.Count(); ++i )
	{
		bool bUnique = true;
		const Vector &vNormal = oldNormalData[ i ];

		for ( int j = 0; j < newNormalData.Count(); ++j )
		{
			if ( DotProduct( vNormal, newNormalData[j] ) > flNormalBlend )
			{
				normalMap[ i ] = j;
				bUnique = false;
				break;
			}
		}

		if ( !bUnique )
			continue;

		normalMap[ i ] = newNormalData.AddToTail( vNormal );
	}

	// If it's the same then don't do anything.
	if ( newNormalData.Count() >= oldNormalData.Count() )
		return false;

	CUtlVector< int > newNormalIndices;
	newNormalIndices.SetCount( oldNormalIndices.Count() );

	for ( int i = 0; i < oldNormalIndices.Count(); ++i )
	{
		newNormalIndices[i] = normalMap[ oldNormalIndices[i] ];
	}

	pDmeVertexData->RemoveAllVertexData( nNormalFieldIndex );
	nNormalFieldIndex = pDmeVertexData->CreateField( CDmeVertexDeltaData::FIELD_NORMAL );
	pDmeVertexData->AddVertexData( nNormalFieldIndex, newNormalData.Count() );
	pDmeVertexData->SetVertexData( nNormalFieldIndex, 0, newNormalData.Count(), AT_VECTOR3, newNormalData.Base() );
	pDmeVertexData->SetVertexIndices( nNormalFieldIndex, 0, newNormalIndices.Count(), newNormalIndices.Base() );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMesh::NormalizeNormals()
{
	Vector vNormal;

	for ( int i = 0; i < this->BaseStateCount(); ++i )
	{
		CDmeVertexData *pDmeVertexData = GetBaseState( i );
		if ( !pDmeVertexData )
			continue;

		FieldIndex_t nNormalIndex = pDmeVertexData->FindFieldIndex( CDmeVertexData::FIELD_NORMAL );
		if ( nNormalIndex < 0 )
			continue;

		CDmAttribute *pDmNormalAttr = pDmeVertexData->GetVertexData( nNormalIndex );
		if ( !pDmNormalAttr )
			continue;

		CDmrArray< Vector > normalData( pDmNormalAttr );
		for ( int j = 0; j < normalData.Count(); ++j )
		{
			vNormal = normalData.Get( j );
			VectorNormalize( vNormal );
			normalData.Set( j, vNormal );
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMesh::CollapseRedundantNormals( float flNormalBlend )
{
	NormalizeNormals();

	CDmeVertexData *pDmeBind = GetBindBaseState();
	if ( !pDmeBind )
		return;

	CUtlVector< int > normalMap;

	const int nDeltaStateCount = DeltaStateCount();
	if ( nDeltaStateCount <= 0 )
	{
		// No deltas
		if ( CollapseRedundantBaseNormalsAggressive( pDmeBind, flNormalBlend ) )
		{
			// Collapse any other states
			for ( int i = 0; i < BaseStateCount(); ++i )
			{
				CDmeVertexData *pDmeVertexData = GetBaseState( i );
				if ( !pDmeVertexData || pDmeVertexData == pDmeBind )
					continue;

				CollapseRedundantBaseNormalsAggressive( pDmeVertexData, flNormalBlend );
			}
		}
	}
	else
	{
		// Collapse the base state
		if ( CollapseRedundantBaseNormals( pDmeBind, normalMap, flNormalBlend ) )
		{
			// Collapse any delta states using the baseState normal map
			for ( int i = 0; i < DeltaStateCount(); ++i )
			{
				CDmeVertexDeltaData *pDmeDeltaData = GetDeltaState( i );
				if ( !pDmeDeltaData )
					continue;

				CollapseRedundantDeltaNormals( pDmeDeltaData, normalMap );
			}

			// Collapse any other states
			for ( int i = 0; i < BaseStateCount(); ++i )
			{
				CDmeVertexData *pDmeVertexData = GetBaseState( i );
				if ( !pDmeVertexData || pDmeVertexData == pDmeBind )
					continue;

				CollapseRedundantBaseNormals( pDmeVertexData, normalMap, flNormalBlend );
			}
		}
	}
}
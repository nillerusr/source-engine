//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "studiorender.h"
#include "studiorendercontext.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "optimize.h"
#include "mathlib/vmatrix.h"
#include "tier0/vprof.h"
#include "tier1/strtools.h"
#include "tier1/KeyValues.h"
#include "tier0/memalloc.h"
#include "convar.h"
#include "materialsystem/itexture.h"
#include "tier2/tier2.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static ConVar	r_studio_stats( "r_studio_stats", "0", FCVAR_CHEAT );

inline float TriangleArea( const Vector &v0, const Vector &v1, const Vector &v2 )
{
	Vector vecEdge0, vecEdge1, vecCross;
	VectorSubtract( v1, v0, vecEdge0 );
	VectorSubtract( v2, v0, vecEdge1 );
	CrossProduct( vecEdge0, vecEdge1, vecCross );
	return ( VectorLength( vecCross ) * 0.5f );
}


void CStudioRender::R_GatherStats( studiomeshgroup_t *pGroup, CMeshBuilder &MeshBuilder, IMesh *pMesh, IMaterial *pMaterial )
{
	int nCount = 0;
	float flSurfaceArea = 0.0f;
	float flTriSurfaceArea = 0.0f;
	float flTextureSurfaceArea = 0.0f;
	int nFrontFacing = 0;
	CUtlVector< Vector >	Positions;
	CUtlVector< Vector2D >	TextureCoords;
	CUtlVector< short >		Indexes;
	CUtlVector< float >		TriAreas;
	CUtlVector< float >		TextureAreas;
	int nTextureWidth = 0;
	int nTextureHeight = 0;
	IMaterialVar	**pMaterialVar = pMaterial->GetShaderParams();

	for( int i = 0; i < pMaterial->ShaderParamCount(); i++ )
	{
		if ( pMaterialVar[ i ]->IsTexture() == false )
		{
			continue;
		}

		ITexture *pTexture = pMaterialVar[ i ]->GetTextureValue();
		if ( pTexture == NULL )
		{
			continue;
		}

		int nWidth = pTexture->GetActualWidth();
		if ( nWidth > nTextureWidth )
		{
			nTextureWidth = nWidth;
		}
		int nHeight = pTexture->GetActualHeight();
		if ( nHeight > nTextureHeight )
		{
			nTextureHeight = nHeight;
		}
	}

	Vector2D	vTextureSize( nTextureWidth, nTextureHeight );

	VMatrix		m_ViewMatrix, m_ProjectionMatrix, m_ViewProjectionMatrix;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->GetMatrix( MATERIAL_VIEW, &m_ViewMatrix );
	pRenderContext->GetMatrix( MATERIAL_PROJECTION, &m_ProjectionMatrix );
	MatrixMultiply( m_ProjectionMatrix, m_ViewMatrix, m_ViewProjectionMatrix );

	Positions.EnsureCapacity( MeshBuilder.VertexCount() );
	Positions.SetCount( MeshBuilder.VertexCount() );
	TextureCoords.EnsureCapacity( MeshBuilder.VertexCount() );
	TextureCoords.SetCount( MeshBuilder.VertexCount() );
	for( int i = 0; i < MeshBuilder.VertexCount(); i++ )
	{
		MeshBuilder.SelectVertex( i );
		Positions[ i ] = *( const Vector * )MeshBuilder.Position();
		TextureCoords[ i ] = ( *( const Vector2D * )MeshBuilder.TexCoord( 0 ) ) * vTextureSize;
	}

	int nNumIndexes = 0;
	for (int j = 0; j < pGroup->m_NumStrips; ++j)
	{
		OptimizedModel::StripHeader_t *pStrip = &pGroup->m_pStripData[ j ];
		nNumIndexes += pStrip->numIndices;
	}

	Indexes.EnsureCapacity( nNumIndexes );
	Indexes.SetCount( nNumIndexes );
	TriAreas.EnsureCapacity( nNumIndexes / 3 );
	TriAreas.SetCount( nNumIndexes / 3 );
	TextureAreas.EnsureCapacity( nNumIndexes / 3 );
	TextureAreas.SetCount( nNumIndexes / 3 );
	for (int j = 0; j < pGroup->m_NumStrips; ++j)
	{
		OptimizedModel::StripHeader_t *pStrip = &pGroup->m_pStripData[ j ];
		for( int i = 0; i < pStrip->numIndices; i += 3 )
		{
			Indexes[ pStrip->indexOffset + i ] = pGroup->m_pIndices[ pStrip->indexOffset + i ];
			Indexes[ pStrip->indexOffset + i + 1 ] = pGroup->m_pIndices[ pStrip->indexOffset + i + 1 ];
			Indexes[ pStrip->indexOffset + i + 2 ] = pGroup->m_pIndices[ pStrip->indexOffset + i + 2 ];
			TriAreas[ ( pStrip->indexOffset + i ) / 3 ] = 0.0f;
			TextureAreas[ ( pStrip->indexOffset + i ) / 3 ] = 0.0f;
		}
	}

	const float UNIFORM_SCREEN_WIDTH = 1600.0f;
	const float UNIFORM_SCREEN_HEIGHT = 1200.0f;
	
	for (int j = 0; j < pGroup->m_NumStrips; ++j)
	{
		OptimizedModel::StripHeader_t *pStrip = &pGroup->m_pStripData[ j ];

		for( int i = 0; i < pStrip->numIndices; i += 3 )
		{
			int nIndex1 = pGroup->m_pIndices[ pStrip->indexOffset + i ];
			int nIndex2 = pGroup->m_pIndices[ pStrip->indexOffset + i + 1 ];
			int nIndex3 = pGroup->m_pIndices[ pStrip->indexOffset + i + 2 ];

			MeshBuilder.SelectVertex( nIndex1 );
			const float *pPos1 = MeshBuilder.Position();

			MeshBuilder.SelectVertex( nIndex2 );
			const float *pPos2 = MeshBuilder.Position();

			MeshBuilder.SelectVertex( nIndex3 );
			const float *pPos3 = MeshBuilder.Position();

			float flTriArea = TriangleArea( *( const Vector * )( pPos1 ), *( const Vector * )( pPos2 ), *( const Vector * )( pPos3 ) );
			flSurfaceArea += flTriArea;

			Vector	V1View, V2View, V3View;

			m_ViewProjectionMatrix.V3Mul( *( const Vector * )( pPos1 ), V1View );
			m_ViewProjectionMatrix.V3Mul( *( const Vector * )( pPos2 ), V2View );
			m_ViewProjectionMatrix.V3Mul( *( const Vector * )( pPos3 ), V3View );

			Vector vNormal;
			float flIntercept;
			ComputeTrianglePlane( V1View, V2View, V3View, vNormal, flIntercept );

			V1View = ( V1View * 0.5f ) + Vector( 0.5f, 0.5f, 0.5f );
			V1View *= Vector( UNIFORM_SCREEN_WIDTH, UNIFORM_SCREEN_HEIGHT, 1.0f );
			V2View = ( V2View * 0.5f ) + Vector( 0.5f, 0.5f, 0.5f );
			V2View *= Vector( UNIFORM_SCREEN_WIDTH, UNIFORM_SCREEN_HEIGHT, 1.0f );
			V3View = ( V3View * 0.5f ) + Vector( 0.5f, 0.5f, 0.5f );
			V3View *= Vector( UNIFORM_SCREEN_WIDTH, UNIFORM_SCREEN_HEIGHT, 1.0f );

			flTriArea = -TriArea2D( V1View, V2View, V3View );
			if ( flTriArea > 0.0f )
			{
				nFrontFacing++;

				flTriSurfaceArea += flTriArea;
				TriAreas[ ( pStrip->indexOffset + i ) / 3 ] = flTriArea;

				Vector2D	TexV1 = TextureCoords[ nIndex1 ];
				Vector2D	TexV2 = TextureCoords[ nIndex2 ];
				Vector2D	TexV3 = TextureCoords[ nIndex3 ];

				flTriArea = fabs( TriArea2D( TexV1, TexV2, TexV3 ) );
				flTextureSurfaceArea += flTriArea;
				TextureAreas[ ( pStrip->indexOffset + i ) / 3 ] = flTriArea;
			}
		}

		nCount += pStrip->numIndices;
	}

//	Msg( "%d / %d / %g / %g ||| %d / %g\n", MeshBuilder.VertexCount(), nCount, flSurfaceArea, flTriSurfaceArea, nFrontFacing, flTriSurfaceArea / (float)nFrontFacing );

	for( int i = 0; i < MeshBuilder.VertexCount(); i++ )
	{
		MeshBuilder.SelectVertex( i );
		MeshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	}

	MeshBuilder.End();
	pMesh->MarkAsDrawn();

	pMaterial = materials->FindMaterial( "debug/modelstats", TEXTURE_GROUP_OTHER );
	pRenderContext->Bind( pMaterial );

	int nRenderCount = -1;

	for (int j = 0; j < pGroup->m_NumStrips; ++j)
	{
		OptimizedModel::StripHeader_t *pStrip = &pGroup->m_pStripData[ j ];

		for( int i = 0; i < pStrip->numIndices; i += 3 )
		{
			if ( nRenderCount >= 10000 || nRenderCount == -1 )
			{
				if ( nRenderCount >= 0 )
				{
					MeshBuilder.End( false, true );
				}

				pMesh = pRenderContext->GetDynamicMeshEx( false );
				nRenderCount = 0;

				if ( nFrontFacing > 10000 )
				{
					MeshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 10000 );
					nFrontFacing -= 10000;

				}
				else
				{
					MeshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nFrontFacing );
				}
			}

			int nIndex1 = Indexes[ pStrip->indexOffset + i ];
			int nIndex2 = Indexes[ pStrip->indexOffset + i + 1 ];
			int nIndex3 = Indexes[ pStrip->indexOffset + i + 2 ];

			float flArea = TriAreas[ ( pStrip->indexOffset + i ) / 3 ];
			if ( flArea > 0.0f )
			{
				Vector vColor;

				if ( r_studio_stats.GetInt() == 1 )
				{
					if ( flArea < 20.0f )
					{
						vColor.Init( 1.0f, 0.0f, 0.0f );
					}
					else if ( flArea < 50.0f )
					{
						vColor.Init( 1.0f, 0.565f, 0.0f );
					}
					else if ( flArea < 100.0f )
					{
						vColor.Init( 1.0f, 0.871f, 0.0f );
					}
					else if ( flArea < 200.0f )
					{
						vColor.Init( 0.701f, 1.0f, 0.0f );
					}
					else 
					{
						vColor.Init( 0.0f, 1.0f, 0.0f );
					}
				}
				else
				{
					float flArea = TextureAreas[ ( pStrip->indexOffset + i ) / 3  ] / TriAreas[ ( pStrip->indexOffset + i ) / 3 ];

					if ( flArea >= 16.0f )
					{
						vColor.Init( 1.0f, 0.0f, 0.0f );
					}
					else if ( flArea >= 8.0f )
					{
						vColor.Init( 1.0f, 0.565f, 0.0f );
					}
					else if ( flArea >= 4.0f )
					{
						vColor.Init( 1.0f, 0.871f, 0.0f );
					}
					else if ( flArea >= 2.0f )
					{
						vColor.Init( 0.701f, 1.0f, 0.0f );
					}
					else if ( flArea >= 1.0f )
					{
						vColor.Init( 0.0f, 1.0f, 0.0f );
					}
					else 
					{
						vColor.Init( 0.0f, 0.871f, 1.0f );
					}
				}

				MeshBuilder.Position3fv( Positions[ nIndex1 ].Base() );
				MeshBuilder.Color3fv( vColor.Base() );
				MeshBuilder.AdvanceVertex();

				MeshBuilder.Position3fv( Positions[ nIndex2 ].Base() );
				MeshBuilder.Color3fv( vColor.Base() );
				MeshBuilder.AdvanceVertex();

				MeshBuilder.Position3fv( Positions[ nIndex3 ].Base() );
				MeshBuilder.Color3fv( vColor.Base() );
				MeshBuilder.AdvanceVertex();
				nRenderCount++;
			}
		}
	}

	if ( nRenderCount >= 0 )
	{
		MeshBuilder.End( false, true );
	}
}



//-----------------------------------------------------------------------------
// Main model rendering entry point
//-----------------------------------------------------------------------------
void CStudioRender::ModelStats( const DrawModelInfo_t& info, const StudioRenderContext_t &rc, 
							    matrix3x4_t *pBoneToWorld, const FlexWeights_t &flex, int flags )
{
	StudioRenderContext_t	StatsRC = rc;

	StatsRC.m_Config.m_bStatsMode = true;

	m_pRC = const_cast< StudioRenderContext_t* >( &StatsRC );
	m_pFlexWeights = flex.m_pFlexWeights;
	m_pFlexDelayedWeights = flex.m_pFlexDelayedWeights;
	m_pBoneToWorld = pBoneToWorld;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	// Disable flex if we're told to...
	bool flexConfig = m_pRC->m_Config.bFlex;
	if (flags & STUDIORENDER_DRAW_NO_FLEXES)
	{
		m_pRC->m_Config.bFlex = false;
	}

	// Enable wireframe if we're told to...
	bool bWireframe = m_pRC->m_Config.bWireframe;
	if ( flags & STUDIORENDER_DRAW_WIREFRAME )
	{
		m_pRC->m_Config.bWireframe = true;
	}

	int boneMask = BONE_USED_BY_VERTEX_AT_LOD( info.m_Lod );

	// Preserve the matrices if we're skinning
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	m_VertexCache.StartModel();

	m_pStudioHdr = info.m_pStudioHdr;
	m_pStudioMeshes = info.m_pHardwareData->m_pLODs[info.m_Lod].m_pMeshData;

	// Bone to world must be set before calling drawmodel; it uses that here
	ComputePoseToWorld( m_PoseToWorld, m_pStudioHdr, boneMask, m_pRC->m_ViewOrigin, pBoneToWorld );

	R_StudioRenderModel( pRenderContext, info.m_Skin, info.m_Body, info.m_HitboxSet, info.m_pClientEntity,
		info.m_pHardwareData->m_pLODs[info.m_Lod].ppMaterials, 
		info.m_pHardwareData->m_pLODs[info.m_Lod].pMaterialFlags, flags, boneMask, info.m_Lod, info.m_pColorMeshes);

	// Restore the matrices if we're skinning
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	// Restore the configs
	m_pRC->m_Config.bFlex = flexConfig;
	m_pRC->m_Config.bWireframe = bWireframe;

#ifdef REPORT_FLEX_STATS
	GetFlexStats();
#endif

	StatsRC.m_Config.m_bStatsMode = false;

	pRenderContext->SetNumBoneWeights( 0 );
	m_pRC = NULL;
	m_pBoneToWorld = NULL;
	m_pFlexWeights = NULL;
	m_pFlexDelayedWeights = NULL;
}

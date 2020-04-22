//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "istudiorender.h"
#include "materialsystem/imesh.h"
#include "mathlib/mathlib.h"
#include "matsyswin.h"
#include "viewersettings.h"
#include "materialsystem/imaterialvar.h"

extern IMaterialSystem *g_pMaterialSystem;

#define NORMAL_LENGTH .5f
#define NORMAL_OFFSET_FROM_MESH 0.1f

int DebugDrawModel( IStudioRender *pStudioRender, DrawModelInfo_t& info, 
	matrix3x4_t *pBoneToWorld, const Vector &modelOrigin, int flags )
{
	// Make static so that we aren't reallocating everything all the time.
	// TODO: make sure that this actually keeps us from reallocating inside of GetTriangles.
	static GetTriangles_Output_t tris;

	pStudioRender->GetTriangles( info, pBoneToWorld, tris );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	CMeshBuilder meshBuilder;

	int batchID;
	for( batchID = 0; batchID < tris.m_MaterialBatches.Count(); batchID++ )
	{
		GetTriangles_MaterialBatch_t &materialBatch = tris.m_MaterialBatches[batchID];

		pRenderContext->Bind( materialBatch.m_pMaterial );
		IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false );
		meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, materialBatch.m_Verts.Count(), 
			materialBatch.m_TriListIndices.Count() );

		int vertID;
		// Send the vertices down to the hardware.
		for( vertID = 0; vertID < materialBatch.m_Verts.Count(); vertID++ )
		{
			GetTriangles_Vertex_t &vert = materialBatch.m_Verts[vertID];
			const Vector &pos = vert.m_Position;
			const Vector &normal = vert.m_Normal;
			const Vector4D &tangentS = vert.m_TangentS;
			Vector skinnedPos( 0.0f, 0.0f, 0.0f );
			Vector skinnedNormal( 0.0f, 0.0f, 0.0f );
			Vector4D skinnedTangentS( 0.0f, 0.0f, 0.0f, vert.m_TangentS[3] );
			int k;
			for( k = 0; k < vert.m_NumBones; k++ )
			{
				const matrix3x4_t &poseToWorld = tris.m_PoseToWorld[ vert.m_BoneIndex[k] ];
				Vector tmp;
				VectorTransform( pos, poseToWorld, tmp );
				skinnedPos += vert.m_BoneWeight[k] * tmp;
				VectorRotate( normal, poseToWorld, tmp );
				skinnedNormal += vert.m_BoneWeight[k] * tmp;
				VectorRotate( tangentS.AsVector3D(), poseToWorld, tmp );
				skinnedTangentS.AsVector3D() += vert.m_BoneWeight[k] * tmp;
			}

			meshBuilder.Position3fv( &skinnedPos.x );
			meshBuilder.Normal3fv( &skinnedNormal.x );
			meshBuilder.TexCoord2fv( 0, &vert.m_TexCoord.x );
			meshBuilder.UserData( &skinnedTangentS.x );
			meshBuilder.AdvanceVertex();
		}

		int i;
		// Set the indices down to the hardware.
		// Each triplet of indices is a triangle.
		for( i = 0; i < materialBatch.m_TriListIndices.Count(); i++ )
		{
			meshBuilder.FastIndex( materialBatch.m_TriListIndices[i] );
		}
		meshBuilder.End();
		pBuildMesh->Draw();
	}
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	return 0;
}

int DebugDrawModelNormals( IStudioRender *pStudioRender, DrawModelInfo_t& info, 
	matrix3x4_t *pBoneToWorld, const Vector &modelOrigin, int flags )
{
	// Make static so that we aren't reallocating everything all the time.
	// TODO: make sure that this actually keeps us from reallocating inside of GetTriangles.
	static GetTriangles_Output_t tris;

	pStudioRender->GetTriangles( info, pBoneToWorld, tris );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	int batchID;
	for( batchID = 0; batchID < tris.m_MaterialBatches.Count(); batchID++ )
	{
		GetTriangles_MaterialBatch_t &materialBatch = tris.m_MaterialBatches[batchID];

		CMeshBuilder meshBuilder;
		pRenderContext->Bind( g_materialVertexColor );
		IMesh *pBuildMesh = pRenderContext->GetDynamicMesh();
		meshBuilder.Begin( pBuildMesh, MATERIAL_LINES, materialBatch.m_Verts.Count() );

		int vertID;
		// Send the vertices down to the hardware.
		for( vertID = 0; vertID < materialBatch.m_Verts.Count(); vertID++ )
		{
			GetTriangles_Vertex_t &vert = materialBatch.m_Verts[vertID];
			const Vector &pos = vert.m_Position;
			const Vector &normal = vert.m_Normal;
			Vector skinnedPos( 0.0f, 0.0f, 0.0f );
			Vector skinnedNormal( 0.0f, 0.0f, 0.0f );
			int k;
			for( k = 0; k < vert.m_NumBones; k++ )
			{
				const matrix3x4_t &poseToWorld = tris.m_PoseToWorld[ vert.m_BoneIndex[k] ];
				Vector tmp;
				VectorTransform( pos, poseToWorld, tmp );
				skinnedPos += vert.m_BoneWeight[k] * tmp;
				VectorRotate( normal, poseToWorld, tmp );
				skinnedNormal += vert.m_BoneWeight[k] * tmp;
			}

//			skinnedPos += skinnedNormal * NORMAL_OFFSET_FROM_MESH;
			meshBuilder.Position3fv( &skinnedPos.x );
			meshBuilder.Color3f( 0.0f, 0.0f, 1.0f );
			meshBuilder.AdvanceVertex();

			skinnedPos += skinnedNormal * NORMAL_LENGTH;
			meshBuilder.Position3fv( &skinnedPos.x );
			meshBuilder.Color3f( 0.0f, 0.0f, 1.0f );
			meshBuilder.AdvanceVertex();
		}

		meshBuilder.End();
		pBuildMesh->Draw();
	}
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	return 0;
}

int DebugDrawModelTangentS( IStudioRender *pStudioRender, DrawModelInfo_t& info, 
	matrix3x4_t *pBoneToWorld, const Vector &modelOrigin, int flags )
{
	// Make static so that we aren't reallocating everything all the time.
	// TODO: make sure that this actually keeps us from reallocating inside of GetTriangles.
	static GetTriangles_Output_t tris;

	pStudioRender->GetTriangles( info, pBoneToWorld, tris );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	int batchID;
	for( batchID = 0; batchID < tris.m_MaterialBatches.Count(); batchID++ )
	{
		GetTriangles_MaterialBatch_t &materialBatch = tris.m_MaterialBatches[batchID];

		CMeshBuilder meshBuilder;
		pRenderContext->Bind( g_materialVertexColor );
		IMesh *pBuildMesh = pRenderContext->GetDynamicMesh();
		meshBuilder.Begin( pBuildMesh, MATERIAL_LINES, materialBatch.m_Verts.Count() );

		int vertID;
		// Send the vertices down to the hardware.
		for( vertID = 0; vertID < materialBatch.m_Verts.Count(); vertID++ )
		{
			GetTriangles_Vertex_t &vert = materialBatch.m_Verts[vertID];
			const Vector &pos = vert.m_Position;
			const Vector &normal = vert.m_Normal;
			const Vector4D &tangentS = vert.m_TangentS;
			Vector skinnedPos( 0.0f, 0.0f, 0.0f );
			Vector skinnedNormal( 0.0f, 0.0f, 0.0f );
			Vector4D skinnedTangentS( 0.0f, 0.0f, 0.0f, vert.m_TangentS[3] );
			int k;
			for( k = 0; k < vert.m_NumBones; k++ )
			{
				const matrix3x4_t &poseToWorld = tris.m_PoseToWorld[ vert.m_BoneIndex[k] ];
				Vector tmp;
				VectorTransform( pos, poseToWorld, tmp );
				skinnedPos += vert.m_BoneWeight[k] * tmp;
				VectorRotate( normal, poseToWorld, tmp );
				skinnedNormal += vert.m_BoneWeight[k] * tmp;
				VectorRotate( tangentS.AsVector3D(), poseToWorld, tmp );
				skinnedTangentS.AsVector3D() += vert.m_BoneWeight[k] * tmp;
			}

//			skinnedPos += skinnedNormal * NORMAL_OFFSET_FROM_MESH;
			meshBuilder.Position3fv( &skinnedPos.x );
			meshBuilder.Color3f( 1.0f, 0.0f, 0.0f );
			meshBuilder.AdvanceVertex();

			skinnedPos += skinnedTangentS.AsVector3D() * NORMAL_LENGTH;
			meshBuilder.Position3fv( &skinnedPos.x );
			meshBuilder.Color3f( 1.0f, 0.0f, 0.0f );
			meshBuilder.AdvanceVertex();
		}

		meshBuilder.End();
		pBuildMesh->Draw();
	}
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	return 0;
}

int DebugDrawModelTangentT( IStudioRender *pStudioRender, DrawModelInfo_t& info,
	matrix3x4_t *pBoneToWorld, const Vector &modelOrigin, int flags )
{
	// Make static so that we aren't reallocating everything all the time.
	// TODO: make sure that this actually keeps us from reallocating inside of GetTriangles.
	static GetTriangles_Output_t tris;

	pStudioRender->GetTriangles( info, pBoneToWorld, tris );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	int batchID;
	for( batchID = 0; batchID < tris.m_MaterialBatches.Count(); batchID++ )
	{
		GetTriangles_MaterialBatch_t &materialBatch = tris.m_MaterialBatches[batchID];

		CMeshBuilder meshBuilder;
		pRenderContext->Bind( g_materialVertexColor );
		IMesh *pBuildMesh = pRenderContext->GetDynamicMesh();
		meshBuilder.Begin( pBuildMesh, MATERIAL_LINES, materialBatch.m_Verts.Count() );

		int vertID;
		// Send the vertices down to the hardware.
		for( vertID = 0; vertID < materialBatch.m_Verts.Count(); vertID++ )
		{
			GetTriangles_Vertex_t &vert = materialBatch.m_Verts[vertID];
			const Vector &pos = vert.m_Position;
			const Vector &normal = vert.m_Normal;
			const Vector4D &tangentS = vert.m_TangentS;
			Vector skinnedPos( 0.0f, 0.0f, 0.0f );
			Vector skinnedNormal( 0.0f, 0.0f, 0.0f );
			Vector4D skinnedTangentS( 0.0f, 0.0f, 0.0f, vert.m_TangentS[3] );
			int k;
			for( k = 0; k < vert.m_NumBones; k++ )
			{
				const matrix3x4_t &poseToWorld = tris.m_PoseToWorld[ vert.m_BoneIndex[k] ];
				Vector tmp;
				VectorTransform( pos, poseToWorld, tmp );
				skinnedPos += vert.m_BoneWeight[k] * tmp;
				VectorRotate( normal, poseToWorld, tmp );
				skinnedNormal += vert.m_BoneWeight[k] * tmp;
				VectorRotate( tangentS.AsVector3D(), poseToWorld, tmp );
				skinnedTangentS.AsVector3D() += vert.m_BoneWeight[k] * tmp;
			}

			Vector skinnedTangentT = CrossProduct( skinnedNormal, skinnedTangentS.AsVector3D() ) * skinnedTangentS.w;
			
//			skinnedPos += skinnedNormal * NORMAL_OFFSET_FROM_MESH;
			meshBuilder.Position3fv( &skinnedPos.x );
			meshBuilder.Color3f( 0.0f, 1.0f, 0.0f );
			meshBuilder.AdvanceVertex();

			skinnedPos += skinnedTangentT * NORMAL_LENGTH;
			meshBuilder.Position3fv( &skinnedPos.x );
			meshBuilder.Color3f( 0.0f, 1.0f, 0.0f );
			meshBuilder.AdvanceVertex();
		}

		meshBuilder.End();
		pBuildMesh->Draw();
	}
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	return 0;
}

int DebugDrawModelBadVerts( IStudioRender *pStudioRender, DrawModelInfo_t& info, 
	matrix3x4_t *pBoneToWorld, const Vector &modelOrigin, int flags )
{
	// Make static so that we aren't reallocating everything all the time.
	// TODO: make sure that this actually keeps us from reallocating inside of GetTriangles.
	static GetTriangles_Output_t tris;

	pStudioRender->GetTriangles( info, pBoneToWorld, tris );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	CMeshBuilder meshBuilder;

	int batchID;
	for( batchID = 0; batchID < tris.m_MaterialBatches.Count(); batchID++ )
	{
		GetTriangles_MaterialBatch_t &materialBatch = tris.m_MaterialBatches[batchID];

		pRenderContext->Bind( g_materialVertexColor );
		IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false );
		meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, materialBatch.m_Verts.Count(), 
			materialBatch.m_TriListIndices.Count() );

		int vertID;
		// Send the vertices down to the hardware.
		for( vertID = 0; vertID < materialBatch.m_Verts.Count(); vertID++ )
		{
			GetTriangles_Vertex_t &vert = materialBatch.m_Verts[vertID];
			const Vector &pos = vert.m_Position;
			const Vector &normal = vert.m_Normal;
			const Vector4D &tangentS = vert.m_TangentS;
			Vector skinnedPos( 0.0f, 0.0f, 0.0f );
			Vector skinnedNormal( 0.0f, 0.0f, 0.0f );
			Vector4D skinnedTangentS( 0.0f, 0.0f, 0.0f, vert.m_TangentS[3] );
			int k;
			for( k = 0; k < vert.m_NumBones; k++ )
			{
				const matrix3x4_t &poseToWorld = tris.m_PoseToWorld[ vert.m_BoneIndex[k] ];
				Vector tmp;
				VectorTransform( pos, poseToWorld, tmp );
				skinnedPos += vert.m_BoneWeight[k] * tmp;
				VectorRotate( normal, poseToWorld, tmp );
				skinnedNormal += vert.m_BoneWeight[k] * tmp;
				VectorRotate( tangentS.AsVector3D(), poseToWorld, tmp );
				skinnedTangentS.AsVector3D() += vert.m_BoneWeight[k] * tmp;
			}

			meshBuilder.Position3fv( &skinnedPos.x );
			meshBuilder.Normal3fv( &skinnedNormal.x );
			meshBuilder.TexCoord2fv( 0, &vert.m_TexCoord.x );
			meshBuilder.UserData( &skinnedTangentS.x );

			Vector color( 0.0f, 0.0f, 0.0f );	
			float len;

			// check the length of the tangent S vector.
			len = tangentS.AsVector3D().Length();
			if( len < .9f || len > 1.1f )
			{
				color.Init( 1.0f, 0.0f, 0.0f );
			}

			// check the length of the normal.
			len = normal.Length();
			if( len < .9f || len > 1.1f )
			{
				color.Init( 1.0f, 0.0f, 0.0f );
			}

			// check the dot of tangent s and normal
			float dot = DotProduct( tangentS.AsVector3D(), normal );
			if( dot > .95 || dot < -.95 )
			{
				color.Init( 1.0f, 0.0f, 0.0f );
			}

			meshBuilder.Color3fv( color.Base() );

			meshBuilder.AdvanceVertex();
		}

		int i;
		// Set the indices down to the hardware.
		// Each triplet of indices is a triangle.
		for( i = 0; i < materialBatch.m_TriListIndices.Count(); i++ )
		{
			meshBuilder.FastIndex( materialBatch.m_TriListIndices[i] );
		}
		meshBuilder.End();
		pBuildMesh->Draw();
	}
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	return 0;
}

int DebugDrawModelWireframe( IStudioRender *pStudioRender, DrawModelInfo_t& info, 
	matrix3x4_t *pBoneToWorld, const Vector &modelOrigin, const Vector &color, int flags )
{
	// Make static so that we aren't reallocating everything all the time.
	// TODO: make sure that this actually keeps us from reallocating inside of GetTriangles.
	static GetTriangles_Output_t tris;

	pStudioRender->GetTriangles( info, pBoneToWorld, tris );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	CMeshBuilder meshBuilder;

	int batchID;
	for( batchID = 0; batchID < tris.m_MaterialBatches.Count(); batchID++ )
	{
		GetTriangles_MaterialBatch_t &materialBatch = tris.m_MaterialBatches[batchID];

		pRenderContext->Bind( g_materialWireframeVertexColor );
		IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false );
		meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, materialBatch.m_Verts.Count(), 
			materialBatch.m_TriListIndices.Count() );

		int vertID;
		// Send the vertices down to the hardware.
		for( vertID = 0; vertID < materialBatch.m_Verts.Count(); vertID++ )
		{
			GetTriangles_Vertex_t &vert = materialBatch.m_Verts[vertID];
			const Vector &pos = vert.m_Position;
			const Vector &normal = vert.m_Normal;
			const Vector4D &tangentS = vert.m_TangentS;
			Vector skinnedPos( 0.0f, 0.0f, 0.0f );
			Vector skinnedNormal( 0.0f, 0.0f, 0.0f );
			Vector4D skinnedTangentS( 0.0f, 0.0f, 0.0f, vert.m_TangentS[3] );
			int k;
			for( k = 0; k < vert.m_NumBones; k++ )
			{
				const matrix3x4_t &poseToWorld = tris.m_PoseToWorld[ vert.m_BoneIndex[k] ];
				Vector tmp;
				VectorTransform( pos, poseToWorld, tmp );
				skinnedPos += vert.m_BoneWeight[k] * tmp;
				VectorRotate( normal, poseToWorld, tmp );
				skinnedNormal += vert.m_BoneWeight[k] * tmp;
				VectorRotate( tangentS.AsVector3D(), poseToWorld, tmp );
				skinnedTangentS.AsVector3D() += vert.m_BoneWeight[k] * tmp;
			}

			meshBuilder.Position3fv( &skinnedPos.x );
			meshBuilder.Normal3fv( &skinnedNormal.x );
			meshBuilder.TexCoord2fv( 0, &vert.m_TexCoord.x );
			meshBuilder.UserData( &skinnedTangentS.x );
			meshBuilder.Color3fv( color.Base() );
			meshBuilder.AdvanceVertex();
		}

		int i;
		// Set the indices down to the hardware.
		// Each triplet of indices is a triangle.
		for( i = 0; i < materialBatch.m_TriListIndices.Count(); i++ )
		{
			meshBuilder.FastIndex( materialBatch.m_TriListIndices[i] );
		}
		meshBuilder.End();
		pBuildMesh->Draw();
	}
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	return 0;
}

int DebugDrawModelBoneWeights( IStudioRender *pStudioRender, DrawModelInfo_t& info, 
	matrix3x4_t *pBoneToWorld, const Vector &modelOrigin, int flags )
{
	// Make static so that we aren't reallocating everything all the time.
	// TODO: make sure that this actually keeps us from reallocating inside of GetTriangles.
	static GetTriangles_Output_t tris;

	pStudioRender->GetTriangles( info, pBoneToWorld, tris );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	CMeshBuilder meshBuilder;

	int batchID;
	for( batchID = 0; batchID < tris.m_MaterialBatches.Count(); batchID++ )
	{
		GetTriangles_MaterialBatch_t &materialBatch = tris.m_MaterialBatches[batchID];

		pRenderContext->Bind( g_materialVertexColor );
		IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false );
		meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, materialBatch.m_Verts.Count(), 
			materialBatch.m_TriListIndices.Count() );

		int vertID;
		// Send the vertices down to the hardware.
		for( vertID = 0; vertID < materialBatch.m_Verts.Count(); vertID++ )
		{
			GetTriangles_Vertex_t &vert = materialBatch.m_Verts[vertID];
			const Vector &pos = vert.m_Position;
			const Vector &normal = vert.m_Normal;
			const Vector4D &tangentS = vert.m_TangentS;
			Vector skinnedPos( 0.0f, 0.0f, 0.0f );
			Vector skinnedNormal( 0.0f, 0.0f, 0.0f );
			Vector4D skinnedTangentS( 0.0f, 0.0f, 0.0f, vert.m_TangentS[3] );
			int k;
			for( k = 0; k < vert.m_NumBones; k++ )
			{
				const matrix3x4_t &poseToWorld = tris.m_PoseToWorld[ vert.m_BoneIndex[k] ];
				Vector tmp;
				VectorTransform( pos, poseToWorld, tmp );
				skinnedPos += vert.m_BoneWeight[k] * tmp;
				VectorRotate( normal, poseToWorld, tmp );
				skinnedNormal += vert.m_BoneWeight[k] * tmp;
				VectorRotate( tangentS.AsVector3D(), poseToWorld, tmp );
				skinnedTangentS.AsVector3D() += vert.m_BoneWeight[k] * tmp;
			}

			meshBuilder.Position3fv( &skinnedPos.x );
			meshBuilder.Normal3fv( &skinnedNormal.x );
			meshBuilder.TexCoord2fv( 0, &vert.m_TexCoord.x );
			meshBuilder.UserData( &skinnedTangentS.x );

			if (g_viewerSettings.highlightBone >= 0)
			{
				float v = 0.0;
				for( k = 0; k < vert.m_NumBones; k++ )
				{
					if (vert.m_BoneIndex[k] == g_viewerSettings.highlightBone)
					{
						v = vert.m_BoneWeight[k];
					}
				}
				v = clamp( v, 0.0f, 1.0f );
				meshBuilder.Color4f( 1.0f - v, 1.0f, 1.0f - v, 0.5 );
			}
			else
			{
				switch( vert.m_NumBones )
				{
				case 0:
					meshBuilder.Color3f( 0.0f, 0.0f, 0.0f );
					break;
				case 1:
					meshBuilder.Color3f( 0.0f, 1.0f, 0.0f );
					break;
				case 2:
					meshBuilder.Color3f( 1.0f, 1.0f, 0.0f );
					break;
				case 3:
					meshBuilder.Color3f( 1.0f, 0.0f, 0.0f );
					break;
				default:
					meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
					break;
				}
			}
			meshBuilder.AdvanceVertex();
		}

		int i;
		// Set the indices down to the hardware.
		// Each triplet of indices is a triangle.
		for( i = 0; i < materialBatch.m_TriListIndices.Count(); i++ )
		{
			meshBuilder.FastIndex( materialBatch.m_TriListIndices[i] );
		}
		meshBuilder.End();
		pBuildMesh->Draw();
	}
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	return 0;
}

int DebugDrawModelTexCoord( IStudioRender *pStudioRender, const char *pMaterialName, const DrawModelInfo_t& info, matrix3x4_t *pBoneToWorld, float w, float h )
{
	// Make static so that we aren't reallocating everything all the time.
	// TODO: make sure that this actually keeps us from reallocating inside of GetTriangles.
	static GetTriangles_Output_t tris;

	pStudioRender->GetTriangles( info, pBoneToWorld, tris );

	CUtlVector<int> batchList;
	for( int batchID = 0; batchID < tris.m_MaterialBatches.Count(); batchID++ )
	{
		GetTriangles_MaterialBatch_t &materialBatch = tris.m_MaterialBatches[batchID];
		if ( !materialBatch.m_Verts.Count() || V_stricmp(materialBatch.m_pMaterial->GetName(), pMaterialName) )
			continue;
		batchList.AddToTail(batchID);
	}
	if ( !batchList.Count() )
		return 0;

	bool bFound = false;
	IMaterialVar *pBaseVar = g_materialDebugCopyBaseTexture->FindVar( "$basetexture", &bFound, true );
	if ( !bFound )
		return 0;

	GetTriangles_MaterialBatch_t &materialBatch = tris.m_MaterialBatches[batchList[0]];
	IMaterialVar *pVar = materialBatch.m_pMaterial->FindVar( "$basetexture", &bFound, true );
	if ( !bFound )
		return 0;
	pBaseVar->SetTextureValue( pVar->GetTextureValue() );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->OverrideDepthEnable( false, false );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->Ortho( 0, h, w, 0, -1, 1 );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	CMeshBuilder meshBuilder;

	// now render a single quad with the base texture on it
	{
		GetTriangles_MaterialBatch_t &materialBatch = tris.m_MaterialBatches[batchList[0]];
		//pRenderContext->Bind( materialBatch.m_pMaterial );
		pRenderContext->Bind( g_materialDebugCopyBaseTexture );
		IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false );
		meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, 4, 6 );
		GetTriangles_Vertex_t &vert = materialBatch.m_Verts[0];
		//const Vector &pos = vert.m_Position;
		const Vector &normal = vert.m_Normal;
		const Vector4D &tangentS = vert.m_TangentS;

		Vector uv0(0,0,0), uv1(1,0,0), uv2(1, 1, 0), uv3(0,1,0);
		Vector *pUV[] = {&uv0, &uv1, &uv2, &uv3};

		for ( int i = 0;i < 4; i++ )
		{
			Vector p = *pUV[i];
			p.x *= w;
			p.y *= h;
			meshBuilder.Position3fv( &p.x );
			meshBuilder.Normal3fv( &normal.x );
			meshBuilder.TexCoord2fv( 0, pUV[i]->Base() );
			meshBuilder.UserData( &tangentS.x );

			meshBuilder.Color3f( 1.0f, 1.0f, 1.0f );
			meshBuilder.AdvanceVertex();
		}
		meshBuilder.FastIndex( 0 );
		meshBuilder.FastIndex( 1 );
		meshBuilder.FastIndex( 2 );

		meshBuilder.FastIndex( 0 );
		meshBuilder.FastIndex( 2 );
		meshBuilder.FastIndex( 3 );

		meshBuilder.End();
		pBuildMesh->Draw();
	}

	// now draw coverage - show which UV space is used more than once
#if 0 
	for( int i = 0; i < batchList.Count(); i++ )
	{
		GetTriangles_MaterialBatch_t &materialBatch = tris.m_MaterialBatches[batchList[i]];
		//pRenderContext->Bind( g_materialWireframeVertexColorNoCull );
		pRenderContext->Bind( g_materialVertexColorAdditive );
		IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false );
		meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, materialBatch.m_Verts.Count(), 
			materialBatch.m_TriListIndices.Count() );

		int vertID;
		// Send the vertices down to the hardware.
		for( vertID = 0; vertID < materialBatch.m_Verts.Count(); vertID++ )
		{
			GetTriangles_Vertex_t &vert = materialBatch.m_Verts[vertID];
			const Vector &normal = vert.m_Normal;
			const Vector4D &tangentS = vert.m_TangentS;

			Vector p;
			p.x = vert.m_TexCoord.x * w;
			p.y = vert.m_TexCoord.y * h;
			p.z = 0;

			meshBuilder.Position3fv( &p.x );
			meshBuilder.Normal3fv( &normal.x );
			meshBuilder.UserData( &tangentS.x );


			meshBuilder.Color3f( 0.25f, 0.0f, 0.0f );
			meshBuilder.AdvanceVertex();
		}

		int i;
		// Set the indices down to the hardware.
		// Each triplet of indices is a triangle.
		for( i = 0; i < materialBatch.m_TriListIndices.Count(); i++ )
		{
			meshBuilder.FastIndex( materialBatch.m_TriListIndices[i] );
		}
		meshBuilder.End();
		pBuildMesh->Draw();
	}
#endif

	const color32 batchColor = {0,255,255,0};
	// now draw all batches with the matching material in wireframe over the render of the base texture
	for( int i = 0; i < batchList.Count(); i++ )
	{
		GetTriangles_MaterialBatch_t &materialBatch = tris.m_MaterialBatches[batchList[i]];

		pRenderContext->Bind( g_materialWireframeVertexColorNoCull );
		IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false );
		meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, materialBatch.m_Verts.Count(), 
			materialBatch.m_TriListIndices.Count() );

		// Send the vertices down to the hardware.
		for( int vertID = 0; vertID < materialBatch.m_Verts.Count(); vertID++ )
		{
			GetTriangles_Vertex_t &vert = materialBatch.m_Verts[vertID];
			const Vector &normal = vert.m_Normal;
			const Vector4D &tangentS = vert.m_TangentS;

			Vector p;
			p.x = vert.m_TexCoord.x * w;
			p.y = vert.m_TexCoord.y * h;
			p.z = 0;

			meshBuilder.Position3fv( &p.x );
			meshBuilder.Normal3fv( &normal.x );
			meshBuilder.UserData( &tangentS.x );


			meshBuilder.Color4ubv( &batchColor.r );
			meshBuilder.AdvanceVertex();
		}

		// Set the indices down to the hardware.
		// Each triplet of indices is a triangle.
		for( int j = 0; j < materialBatch.m_TriListIndices.Count(); j++ )
		{
			meshBuilder.FastIndex( materialBatch.m_TriListIndices[j] );
		}
		meshBuilder.End();
		pBuildMesh->Draw();
	}
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	return 0;
}


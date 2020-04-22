//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DEBUGDRAWMODEL_H
#define DEBUGDRAWMODEL_H
#ifdef _WIN32
#pragma once
#endif

int DebugDrawModel( IStudioRender *pStudioRender, DrawModelInfo_t& info, matrix3x4_t *pBoneToWorld, const Vector &modelOrigin,
			        int flags = STUDIORENDER_DRAW_ENTIRE_MODEL );
int DebugDrawModelNormals( IStudioRender *pStudioRender, DrawModelInfo_t& info, matrix3x4_t *pBoneToWorld, const Vector &modelOrigin,
			        int flags = STUDIORENDER_DRAW_ENTIRE_MODEL );
int DebugDrawModelTangentS( IStudioRender *pStudioRender, DrawModelInfo_t& info, matrix3x4_t *pBoneToWorld, const Vector &modelOrigin,
			        int flags = STUDIORENDER_DRAW_ENTIRE_MODEL );
int DebugDrawModelTangentT( IStudioRender *pStudioRender, DrawModelInfo_t& info, matrix3x4_t *pBoneToWorld, const Vector &modelOrigin,
			        int flags = STUDIORENDER_DRAW_ENTIRE_MODEL );
int DebugDrawModelBoneWeights( IStudioRender *pStudioRender, DrawModelInfo_t& info, matrix3x4_t *pBoneToWorld, const Vector &modelOrigin,
			        int flags = STUDIORENDER_DRAW_ENTIRE_MODEL  );
int DebugDrawModelBadVerts( IStudioRender *pStudioRender, DrawModelInfo_t& info, matrix3x4_t *pBoneToWorld, const Vector &modelOrigin,
							  int flags = STUDIORENDER_DRAW_ENTIRE_MODEL  );
int DebugDrawModelWireframe( IStudioRender *pStudioRender, DrawModelInfo_t& info, matrix3x4_t *pBoneToWorld, const Vector &modelOrigin,
						   const Vector &color, int flags = STUDIORENDER_DRAW_ENTIRE_MODEL );
int DebugDrawModelTexCoord( IStudioRender *pStudioRender, const char *pMaterialName, const DrawModelInfo_t& info, matrix3x4_t *pBoneToWorld, float w, float h );


#endif // DEBUGDRAWMODEL_H
